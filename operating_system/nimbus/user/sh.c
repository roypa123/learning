/* ===========================================================================
 *  nimbus/user/sh.c  --  the shell
 * ===========================================================================
 *
 *  A shell is a loop: read a line, split it into words, and run the result. It
 *  feels like part of the operating system and it is not -- it is an ordinary
 *  program, with no privileges, using only the system calls in libc.h. You
 *  could delete it and the kernel would not notice.
 *
 *  What makes it interesting is redirection and pipes. Both are implemented
 *  entirely with fork(), dup2() and close(), in the window between forking and
 *  exec'ing -- a window that exists only because Unix split process creation
 *  into two calls. Every other design has to pass redirection as a parameter
 *  to the spawn call, and then has to enumerate in advance everything anyone
 *  might want to do to a child.
 *
 *  Explained in: docs/46-shell.md
 *  Line by line: docs/line-by-line/nimbus-sh.md
 * =========================================================================== */

#include "libc.h"

#define MAX_ARGS 16
#define MAX_LINE 256

/* ---------------------------------------------------------------------------
 *  Tokenising
 *
 *  Split on whitespace, in place, by replacing the separators with NULs and
 *  keeping pointers to the starts. No allocation, and the resulting array is
 *  exactly the char*[] that execv wants.
 *
 *  What this does not do: quotes, escapes, globbing, variables. Each of those
 *  is a subsystem of its own in a real shell, and Chapter 46's exercises add
 *  quoting, which is the one that stops being optional as soon as a filename
 *  has a space in it.
 * ------------------------------------------------------------------------- */
static int tokenise(char *line, char *argv[], int max)
{
    int argc = 0;

    while (*line && argc < max - 1) {
        while (*line == ' ' || *line == '\t') *line++ = '\0';
        if (!*line) break;

        argv[argc++] = line;

        while (*line && *line != ' ' && *line != '\t') line++;
    }

    argv[argc] = NULL;
    return argc;
}

/* ---------------------------------------------------------------------------
 *  Built-ins
 *
 *  `cd` must be a built-in and this is worth understanding properly. If cd
 *  were an external program, it would run in a forked child, call chdir(), and
 *  exit -- changing the *child's* directory and then destroying the child. The
 *  parent shell would be exactly where it started.
 *
 *  Anything that must change the shell's own state has the same problem:
 *  `exit`, `export`, `cd`. That is the whole rule for what has to be built in.
 * ------------------------------------------------------------------------- */
static int builtin(int argc, char *argv[])
{
    if (strcmp(argv[0], "exit") == 0) {
        exit(argc > 1 ? atoi(argv[1]) : 0);
    }

    if (strcmp(argv[0], "cd") == 0) {
        const char *target = (argc > 1) ? argv[1] : "/";
        if (chdir(target) < 0)
            printf("cd: %s: no such directory\n", target);
        return 1;
    }

    if (strcmp(argv[0], "pwd") == 0) {
        printf("/\n");     /* we do not track a path string; see Chapter 46 */
        return 1;
    }

    if (strcmp(argv[0], "ps") == 0)     { ps();     return 1; }
    if (strcmp(argv[0], "reboot") == 0) { reboot(); return 1; }

    if (strcmp(argv[0], "uptime") == 0) {
        unsigned ms = uptime_ms();
        printf("up %u.%03u seconds\n", ms / 1000, ms % 1000);
        return 1;
    }

    if (strcmp(argv[0], "help") == 0) {
        printf("built in : cd exit pwd ps uptime reboot help\n");
        printf("in /bin  : ls cat echo hexdump sleep true false\n");
        printf("syntax   : cmd > file    cmd < file    cmd1 | cmd2\n");
        return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 *  Redirection
 *
 *  Scan the argument list for `<` and `>`, remove them and their filenames,
 *  and remember what to open. This runs in the *parent*, before forking,
 *  because the parent is the one holding the parsed command -- but the open()
 *  and dup2() happen in the child, because they must not disturb the shell's
 *  own descriptors.
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *in_file;
    const char *out_file;
} redirect_t;

static int extract_redirects(int argc, char *argv[], redirect_t *r)
{
    r->in_file = r->out_file = NULL;

    int out = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            r->out_file = argv[++i];
        } else if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
            r->in_file = argv[++i];
        } else {
            argv[out++] = argv[i];
        }
    }
    argv[out] = NULL;
    return out;
}

static void apply_redirects(const redirect_t *r)
{
    if (r->in_file) {
        int fd = open(r->in_file, O_RDONLY);
        if (fd < 0) { fprintf(STDERR_FILENO, "cannot open %s\n", r->in_file); exit(1); }

        /*  dup2 makes descriptor 0 a second name for the same open file, and
         *  closes whatever 0 was. The program that execs next reads its stdin
         *  and has no idea it is a file -- which is the point.               */
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (r->out_file) {
        int fd = open(r->out_file, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { fprintf(STDERR_FILENO, "cannot create %s\n", r->out_file); exit(1); }

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

/* ---------------------------------------------------------------------------
 *  A pipeline of exactly two commands
 *
 *  The shape is always the same:
 *
 *      pipe(fds)
 *      fork -> child A: stdout = fds[1], close both, exec cmd1
 *      fork -> child B: stdin  = fds[0], close both, exec cmd2
 *      parent: close BOTH ends, then wait twice
 *
 *  The parent closing both ends is the step everyone forgets, and the symptom
 *  is a pipeline that never finishes. While the parent still holds the write
 *  end open, the pipe has a writer, so the reader's read() never returns 0,
 *  so cmd2 waits forever for input that will never come -- and the shell waits
 *  forever for cmd2.
 * ------------------------------------------------------------------------- */
static void run_pipeline(char *left[], char *right[])
{
    int fds[2];
    if (pipe(fds) < 0) { printf("sh: cannot create pipe\n"); return; }

    pid_t a = fork();
    if (a == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(left[0], left);
        fprintf(STDERR_FILENO, "%s: not found\n", left[0]);
        exit(127);
    }

    pid_t b = fork();
    if (b == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(right[0], right);
        fprintf(STDERR_FILENO, "%s: not found\n", right[0]);
        exit(127);
    }

    close(fds[0]);
    close(fds[1]);

    int status;
    wait(&status);
    wait(&status);
}

/* ---------------------------------------------------------------------------
 *  Resolving a command name
 *
 *  No PATH variable -- there are no environment variables. A name without a
 *  slash is looked for in /bin, which is what PATH would have said anyway.
 * ------------------------------------------------------------------------- */
static const char *resolve(const char *name, char *buf, size_t size)
{
    if (strchr(name, '/')) return name;

    snprintf(buf, size, "/bin/%s", name);
    return buf;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("\n");
    printf("Nimbus shell. Type `help`.\n");

    char line[MAX_LINE];
    char resolved[MAX_LINE];

    for (;;) {
        printf("nimbus> ");

        if (getline(line, sizeof(line)) < 0) {
            /*  End of file on stdin. A shell reading a script has reached the
             *  end; an interactive shell has had its console closed. Either
             *  way, stop.                                                     */
            printf("\n");
            exit(0);
        }

        if (line[0] == '\0') continue;

        /* ---- a pipeline? ---------------------------------------------------- */
        char *bar = strchr(line, '|');
        if (bar) {
            *bar = '\0';

            char *left_argv[MAX_ARGS], *right_argv[MAX_ARGS];
            char  left_path[MAX_LINE], right_path[MAX_LINE];

            int ln = tokenise(line, left_argv, MAX_ARGS);
            int rn = tokenise(bar + 1, right_argv, MAX_ARGS);

            if (ln == 0 || rn == 0) { printf("sh: syntax error near |\n"); continue; }

            /*  resolve() may point into the buffer, so copy the result back
             *  into argv[0] -- execv takes the path and the argument list
             *  separately, and argv[0] is conventionally the name as typed.   */
            left_argv[0]  = (char *)resolve(left_argv[0], left_path, sizeof(left_path));
            right_argv[0] = (char *)resolve(right_argv[0], right_path, sizeof(right_path));

            run_pipeline(left_argv, right_argv);
            continue;
        }

        /* ---- a single command ----------------------------------------------- */
        char *args[MAX_ARGS];
        int   n = tokenise(line, args, MAX_ARGS);
        if (n == 0) continue;

        if (builtin(n, args)) continue;

        redirect_t redir;
        n = extract_redirects(n, args, &redir);
        if (n == 0) { printf("sh: no command\n"); continue; }

        const char *path = resolve(args[0], resolved, sizeof(resolved));

        pid_t pid = fork();
        if (pid < 0) { printf("sh: cannot fork\n"); continue; }

        if (pid == 0) {
            apply_redirects(&redir);
            execv(path, args);
            fprintf(STDERR_FILENO, "%s: command not found\n", args[0]);
            exit(127);
        }

        int status = 0;
        wait(&status);

        /*  A non-zero status is reported, because a shell that swallows
         *  failures is a shell you cannot debug. Real shells put it in `$?`
         *  instead of printing it; we have no variables.                      */
        if (status != 0 && status != 127)
            printf("[exit %d]\n", status);
    }
}
