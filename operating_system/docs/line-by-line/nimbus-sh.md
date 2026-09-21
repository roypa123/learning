# Line by line: `nimbus/user/sh.c`

[Index](README.md) · [Chapter 46](../46-shell.md)

An ordinary unprivileged program. You could delete it and the kernel would not notice.

---

## `tokenise`

```c
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
```
Split **in place**, replacing separators with NULs and keeping pointers to the starts.

```
    "ls  -l /bin"

    'l' 's' \0 \0 '-' 'l' \0 '/' 'b' 'i' 'n' \0
     ^          ^          ^
     argv[0]    argv[1]    argv[2]         argv[3] = NULL
```

No allocation, and the result is exactly the `char *[]` `execv` wants.

`argc < max - 1` leaves room for the NULL terminator.

⚠️ What it does not do: quotes, escapes, globbing, variables.

> Each of those is a subsystem of its own in a real shell, and Chapter 46's exercises add quoting,
> which is the one that stops being optional as soon as a filename has a space in it.

Quoting alone means tracking a quote state, handling `'` and `"` differently, and — because removing
quotes changes the string's length — **no longer being able to tokenise in place**.

---

## `builtin`

```c
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = (argc > 1) ? argv[1] : "/";
        if (chdir(target) < 0)
            printf("cd: %s: no such directory\n", target);
        return 1;
    }
```
⚠️ **The complete rule for what must be a built-in:**

> If `cd` were an external program, it would run in a forked child, call `chdir()`, and exit —
> changing the *child's* directory and then destroying the child. The parent shell would be exactly
> where it started.
>
> Anything that must change the shell's own state has the same problem: `exit`, `export`, `cd`. That
> is the whole rule.

`echo` is a built-in in bash for *performance*, not necessity — and `/bin/echo` exists and works.

```c
    if (strcmp(argv[0], "pwd") == 0) {
        printf("/\n");     /* we do not track a path string; see Chapter 46 */
        return 1;
    }
```
⚠️ **`pwd` lies.**

The kernel stores `current_task->cwd` as a **node pointer**, not a path, and there is no way to walk
back up (no parent pointers) — so the string cannot be reconstructed.

`SYS_GETCWD` is reserved for the fix: a path string in the task struct, updated by `chdir`.

```c
    if (strcmp(argv[0], "ps") == 0)     { ps();     return 1; }
    if (strcmp(argv[0], "reboot") == 0) { reboot(); return 1; }
```
Built in for convenience — thin wrappers over syscalls, and making them separate programs would cost
a fork per invocation for no benefit.

```c
    return 0;
```
Zero means "not a built-in", so the caller goes on to fork and exec.

---

## `extract_redirects`

```c
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
```
Scan, remove the operator and its filename, compact the array in place.

The `i + 1 < argc` guard means a trailing `>` with no filename is treated as an ordinary argument
rather than reading past the end.

⚠️ Runs in the **parent**, before forking, because the parent holds the parsed command. The `open` and
`dup2` happen in the child, because they must not disturb the shell's own descriptors.

If the shell opened the file and dup'd it over its own stdout, the shell would be redirected too,
permanently.

⚠️ Only `>` and `<` as separate tokens. `>file` without a space is not recognised, which is a real
limitation and an honest one.

---

## `apply_redirects`

```c
    if (r->in_file) {
        int fd = open(r->in_file, O_RDONLY);
        if (fd < 0) { fprintf(STDERR_FILENO, "cannot open %s\n", r->in_file); exit(1); }

        dup2(fd, STDIN_FILENO);
        close(fd);
    }
```
Three calls: `open`, `dup2`, `close`.

> `dup2` makes descriptor 0 a second name for the same open file, and closes whatever 0 was. The
> program that execs next reads its stdin and has no idea it is a file — which is the point.

⚠️ **`close(fd)` is not optional.** After `dup2` there are two descriptors on the file; the program
should inherit one. A stray descriptor matters for pipes and wastes a slot in a 16-entry table.

⚠️ **`exit(1)`, not `return`.** We are in the child, and continuing would `exec` the program with the
wrong stdin.

---

## `run_pipeline`

```c
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
```
Child A writes into the pipe. It closes **both** original descriptors after duping — it needs neither
now.

```c
    pid_t b = fork();
    if (b == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(right[0], right);
        ...
    }
```
Child B reads from it.

```c
    close(fds[0]);
    close(fds[1]);
```
⚠️ **The step everyone forgets.**

> While the parent still holds the write end open, the pipe has a writer, so the reader's `read()`
> never returns 0, so cmd2 waits forever for input that will never come — and the shell waits forever
> for cmd2.

Count the references after both forks:

```
    write end:  parent, child A            = 2
    read end:   parent, child B            = 2
```

The parent's two closes bring both to 1. When A exits, the write count reaches 0 and B's next read
returns 0.

```c
    int status;
    wait(&status);
    wait(&status);
```
Two children, two reaps, in whatever order they finish.

⚠️ A real shell tracks pids to report the *last* command's status — `ls | grep x` should exit with
`grep`'s. Ours overwrites `status` and uses neither.

⚠️ Only two commands. Three needs two pipes and three children, and getting the closes right is where
"close every descriptor you do not need" stops being optional.

---

## `resolve`

```c
static const char *resolve(const char *name, char *buf, size_t size)
{
    if (strchr(name, '/')) return name;

    snprintf(buf, size, "/bin/%s", name);
    return buf;
}
```
> No PATH variable — there are no environment variables. A name without a slash is looked for in
> `/bin`, which is what PATH would have said anyway.

⚠️ The `strchr` test is the rule every shell uses: a name containing a slash is a path, used as-is.

That is why `./prog` runs a program in the current directory and `prog` does not — and why `PATH` not
containing `.` is a security measure rather than an oversight.

---

## `main`

```c
        if (getline(line, sizeof(line)) < 0) {
            printf("\n");
            exit(0);
        }
```
⚠️ **EOF ends the shell.**

> A shell reading a script has reached the end; an interactive shell has had its console closed.
> Either way, stop.

That is why Ctrl-D exits a shell: it sends EOF, `read` returns 0, and the loop ends. Not a signal,
not a special case.

```c
        char *bar = strchr(line, '|');
        if (bar) {
            *bar = '\0';
            ...
```
Split the line at the pipe, tokenise each half.

⚠️ Done before tokenising, so a `|` inside what would be a quoted string is still a pipe — which is
one of the things quoting would have to fix.

```c
            left_argv[0]  = (char *)resolve(left_argv[0], left_path, sizeof(left_path));
```
⚠️ `resolve` may return a pointer into `left_path`, so the result is written back into `argv[0]`.

`execv` takes the path and the argument list separately, and `argv[0]` is conventionally the name as
typed — we use the resolved path for both, which is a small simplification.

```c
        if (builtin(n, args)) continue;

        redirect_t redir;
        n = extract_redirects(n, args, &redir);
        if (n == 0) { printf("sh: no command\n"); continue; }
```
Built-ins checked **before** redirection extraction, so `cd > file` is a `cd` with a stray argument
rather than a redirected `cd`. Defensible either way.

```c
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
```
Eleven lines, and every command you type goes through them.

⚠️ The error path after `execv` is mandatory, not defensive — `execv` returns only on failure.

```c
        if (status != 0 && status != 127)
            printf("[exit %d]\n", status);
```
> a shell that swallows failures is a shell you cannot debug. Real shells put it in `$?` instead of
> printing it; we have no variables.

127 suppressed because the "command not found" message already said it.

---

## What a real shell adds

| | Needs |
|---|---|
| Quoting, escapes | A real tokeniser; cannot split in place |
| Variables, `export` | A symbol table, `$` expansion, envp |
| Globbing | Directory reads and pattern matching |
| Command substitution | A recursive invocation and a pipe |
| Job control (`&`, `fg`, `bg`) | Process groups, a controlling terminal, `SIGTSTP` |
| `$?`, `$!`, `$$` | Tracking the last status, last pid, own pid |
| Conditionals, loops, functions | A grammar and an interpreter |
| Line editing, history | Raw mode and a line editor |
| Scripts | `#!` handling in the kernel's `exec` |

⚠️ Most of them want **signals**, which Nimbus does not have. Ctrl-C currently abandons a line; proper
handling needs a pending mask, delivery on the return path, a signal frame on the user stack,
`sigreturn`, default actions, and process groups.

About 300 lines, and the single largest missing feature.

---

[Index](README.md) · [Chapter 46](../46-shell.md)
