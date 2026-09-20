# Chapter 46 — The shell and the utilities

[← A C library](45-user-libc.md) · [Contents](README.md) · [Next: Debugging →](47-debugging.md)

> 📖 **Line by line:** [sh.c](line-by-line/nimbus-sh.md)

---

## Goal

Write the program that makes everything visible. A read-parse-run loop, redirection, pipelines, and
six utilities — all of it ordinary userland code with no privileges.

This finishes Part VI.

---

## 1. A shell is not part of the operating system

> A shell is a loop: read a line, split it into words, and run the result. It feels like part of the
> operating system and it is not — it is an ordinary program, with no privileges, using only the
> system calls in `libc.h`. You could delete it and the kernel would not notice.

That is worth taking seriously. Everything in this chapter runs in ring 3, in its own address space,
and can do nothing the kernel has not published a syscall for.

What makes it interesting:

> redirection and pipes. Both are implemented entirely with `fork()`, `dup2()` and `close()`, in the
> window between forking and exec'ing — a window that exists only because Unix split process creation
> into two calls. Every other design has to pass redirection as a parameter to the spawn call, and
> then has to enumerate in advance everything anyone might want to do to a child.

Chapter 34, §1. The shell is where that design decision pays off.

---

## 2. The loop

```c
    for (;;) {
        printf("nimbus> ");

        if (getline(line, sizeof(line)) < 0) {
            printf("\n");
            exit(0);
        }

        if (line[0] == '\0') continue;
        ...
    }
```

### 2.1 EOF ends the shell

```c
        if (getline(line, sizeof(line)) < 0) {
            printf("\n");
            exit(0);
        }
```

> End of file on stdin. A shell reading a script has reached the end; an interactive shell has had
> its console closed. Either way, stop.

That is why Ctrl-D exits a shell: it sends EOF, `read` returns 0, and the loop ends. Not a signal, not
a special case — just the same end-of-input handling a script gets.

---

## 3. Tokenising

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

Split on whitespace, **in place**, by replacing separators with NULs and keeping pointers to the
starts.

No allocation, and the resulting array is exactly the `char *[]` that `execv` wants.

```
    "ls  -l /bin"

    'l' 's' \0 \0 '-' 'l' \0 '/' 'b' 'i' 'n' \0
     ^          ^          ^
     argv[0]    argv[1]    argv[2]         argv[3] = NULL
```

### 3.1 What it does not do

> quotes, escapes, globbing, variables. Each of those is a subsystem of its own in a real shell, and
> Chapter 46's exercises add quoting, which is the one that stops being optional as soon as a
> filename has a space in it.

Quoting alone means: tracking a quote state, handling `'` (literal) and `"` (with escapes)
differently, and — because the removal of quotes changes the string's length — no longer being able
to tokenise in place.

Globbing means reading directories and matching patterns. Variables mean a symbol table, `$`
expansion, and `export`. Command substitution means a recursive shell invocation and a pipe.

A real shell's parser is a few thousand lines and is one of the more subtle pieces of software people
write.

---

## 4. Built-ins, and the rule for what has to be one

```c
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
    ...
}
```

> `cd` must be a built-in and this is worth understanding properly. If `cd` were an external program,
> it would run in a forked child, call `chdir()`, and exit — changing the *child's* directory and then
> destroying the child. The parent shell would be exactly where it started.
>
> Anything that must change the shell's own state has the same problem: `exit`, `export`, `cd`. That
> is the whole rule for what has to be built in.

It is a complete and precise rule, and it explains every built-in on every shell. `echo` is a
built-in in bash for *performance*, not necessity — and `/bin/echo` exists and works.

Ours are: `exit`, `cd`, `pwd`, `ps`, `uptime`, `reboot`, `help`.

`ps`, `uptime` and `reboot` are built-ins for convenience — they are thin wrappers over syscalls and
making them separate programs would cost a fork per invocation for no benefit.

### 4.1 `pwd` lies

```c
    if (strcmp(argv[0], "pwd") == 0) {
        printf("/\n");     /* we do not track a path string; see Chapter 46 */
        return 1;
    }
```

The kernel stores `current_task->cwd` as a **node pointer**, not a path. There is no way to walk back
up (Chapter 39, §5.3 — no parent pointers), so there is no way to reconstruct the string.

Two fixes:

**Track it in the shell.** `cd` appends or pops components. Simple, and wrong the moment a program
other than the shell changes the directory.

**Track it in the kernel.** A `char cwd_path[VFS_PATH_MAX]` in the task struct, updated by `chdir`,
returned by `getcwd`. That is what `SYS_GETCWD` in [`syscall.h`](../nimbus/include/nimbus/syscall.h)
is reserved for, and it is what Linux did before the dentry cache made reconstruction possible.

Exercise 46.4.

---

## 5. Redirection

```c
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
```

Scan for `<` and `>`, remove them and their filenames, compact the array.

> This runs in the *parent*, before forking, because the parent is the one holding the parsed command
> — but the `open()` and `dup2()` happen in the child, because they must not disturb the shell's own
> descriptors.

That split is the important part. If the shell opened the file and dup'd it over its own stdout, the
shell would be redirected too, permanently.

### 5.1 Applying them

```c
static void apply_redirects(const redirect_t *r)
{
    if (r->in_file) {
        int fd = open(r->in_file, O_RDONLY);
        if (fd < 0) { fprintf(STDERR_FILENO, "cannot open %s\n", r->in_file); exit(1); }

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
```

Three calls each: `open`, `dup2`, `close`.

> `dup2` makes descriptor 0 a second name for the same open file, and closes whatever 0 was. The
> program that execs next reads its stdin and has no idea it is a file — which is the point.

The `close(fd)` is not optional. After `dup2` there are two descriptors on the file; the program
should inherit one, not two. Leaving it open means the program has a stray descriptor it does not
know about, which matters for pipes (§6.1) and wastes a slot in a 16-entry table.

And `exit(1)` on failure, not `return` — we are in the child, and continuing would `exec` the program
with the wrong stdin.

### 5.2 What is missing

`>>` (append), `2>` (redirect stderr), `2>&1` (merge), `<<` (heredoc), `&>` (both).

`>>` is one flag: `O_APPEND` instead of `O_TRUNC`. `2>` is one number. `2>&1` is a bare `dup2(1, 2)`
with no `open` at all — and the reason the syntax looks like that is that `&1` means "descriptor 1"
rather than "the file named 1".

Exercise 46.5.

---

## 6. Pipelines

```c
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
        ...
    }

    close(fds[0]);
    close(fds[1]);

    int status;
    wait(&status);
    wait(&status);
}
```

Chapter 43, §6 covered the reference counting. The shape:

```
    pipe(fds)
    fork -> child A: stdout = fds[1], close both, exec cmd1
    fork -> child B: stdin  = fds[0], close both, exec cmd2
    parent: close BOTH ends, then wait twice
```

> The parent closing both ends is the step everyone forgets, and the symptom is a pipeline that never
> finishes.

### 6.1 Two `wait`s, unordered

```c
    wait(&status);
    wait(&status);
```

Two children, two reaps. `wait` returns whichever finishes first, and we do not care which.

A real shell tracks pids to report the *last* command's status, because `ls | grep x` should exit
with `grep`'s status. Ours overwrites `status` and uses neither.

### 6.2 Only two commands

`ls | grep x | wc -l` needs two pipes and three children, and generalising means an array of commands
and *n*−1 pipes — plus getting the closes right, which is where the rule "close every descriptor you
do not need" stops being optional.

Exercise 43.8.

---

## 7. Resolving a command name

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

The `strchr` test is the same rule every shell uses: a name containing a slash is a path, and is used
as-is. That is why `./prog` runs a program in the current directory and `prog` does not — and why
`PATH` not containing `.` is a security measure rather than an oversight.

---

## 8. The utilities

Six programs in one file, selected at compile time:

```c
#ifdef BUILD_LS
int main(int argc, char *argv[]) { ... }
#endif
```

```make
	$$(CC) $$(CFLAGS) -DBUILD_$(shell echo $(1) | tr a-z A-Z) -c $$< -o bin/user/$(1).o
```

> Each is compiled separately, with `-D` to select which `main()` is built. That keeps six
> near-identical build rules from cluttering the tree while still producing six independent ELF
> binaries, which is the point: every one of them is a real process with its own address space,
> started by fork and exec like any other.

### 8.1 `cat`, and the two-line lesson

```c
static int cat_fd(int fd)
{
    char buf[512];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);

    return n < 0 ? 1 : 0;
}
```

> Read until `read()` returns 0, which means end of file. Not until it returns less than we asked for
> — a short read is normal and says nothing about the end. Getting that wrong truncates output from a
> pipe every time, because a pipe returns whatever is buffered.

That is the single most common I/O bug in C, and a pipe is what exposes it: a file read almost always
returns the full request, so the bug hides until the program is used in a pipeline.

```c
    if (argc < 2) return cat_fd(STDIN_FILENO);
```

> No arguments: read standard input. That one line is what makes `cat` usable in a pipeline, and it
> is the convention every Unix filter follows.

### 8.2 `ls`

```c
    while (readdir_fd(fd, &entry, index++) == 1) {
        if (entry.type & VFS_DIRECTORY)
            printf("%s/  ", entry.name);
        else
            printf("%s  ", entry.name);

        if (++shown % 5 == 0) printf("\n");
    }
```

> readdir returns 1 for an entry, 0 at the end. The index-based interface is ours; POSIX uses an
> opaque `DIR*` with internal state, which is nicer to use and harder to implement across a syscall
> boundary.

It does not sort, which is honest — `ls` sorting is a userland decision and ours has no `qsort`.

### 8.3 `hexdump`

```c
        printf("%08x  ", offset);

        for (int i = 0; i < 16; i++) {
            if (i < n) printf("%02x ", buf[i]);
            else       printf("   ");
            if (i == 7) printf(" ");
        }

        printf(" |");
        for (int i = 0; i < n; i++)
            putchar(isprint(buf[i]) ? buf[i] : '.');
        printf("|\n");
```

The classic layout: offset, sixteen bytes with a gap at eight, then the printable rendering.

The `else printf("   ")` pads a short final line so the `|...|` column stays aligned. Small, and the
difference between a tool you can scan and one you cannot.

### 8.4 `sleep` proves preemption

```c
    unsigned before = uptime_ms();
    sleep_ms(ms);
    unsigned after = uptime_ms();

    printf("slept %u ms (asked for %u)\n", after - before, ms);
```

> Printing the actual elapsed time makes the point that sleep is not a busy wait: during those
> milliseconds this process was not on the CPU at all, and anything else that was runnable ran
> instead.

Run it and check `ps` in another... except there is no job control, so run `forktest` and watch the
interleaving instead.

### 8.5 `true` and `false`

```c
#ifdef BUILD_TRUE
int main(void) { return 0; }
#endif
```

The two smallest useful programs, and they exist because shell conditionals need them — `while true;
do ...` predates shells having a built-in.

And at 4688 bytes each (Chapter 45, §9.2), they are a good demonstration of linker granularity.

---

## 9. Running it

```
Nimbus shell. Type `help`.
nimbus> help
built in : cd exit pwd ps uptime reboot help
in /bin  : ls cat echo hexdump sleep true false
syntax   : cmd > file    cmd < file    cmd1 | cmd2
nimbus> ls /
bin/  dev/  mnt/
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running   12043  idle
   1    0  running      31  /bin/sh
nimbus> echo hello > /mnt/a.txt
nimbus> cat /mnt/a.txt
hello
nimbus> cat < /mnt/a.txt
hello
nimbus> ls /bin | cat
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
nimbus> hexdump /mnt/a.txt
00000000  68 65 6c 6c 6f                                    |hello|
nimbus> uptime
up 47.230 seconds
nimbus> nosuchthing
nosuchthing: command not found
nimbus> false
[exit 1]
```

Every line of that exercises something from Parts II through VI.

### 9.1 What just happened, for one line

`echo hello > /mnt/a.txt`:

1. The keyboard IRQ delivered 26 scancodes (Ch. 19).
2. The console echoed them and buffered the line (Ch. 20).
3. The shell's `read` returned from `console_read_line` (Ch. 20, 33).
4. `tokenise` split it; `extract_redirects` removed `>` and the filename (§3, §5).
5. `fork` cloned the address space (Ch. 28, 34).
6. The child `open`ed the file — `vfs_lookup`, `fat_create`, a directory sector write (Ch. 39, 42).
7. `dup2` made it descriptor 1 (Ch. 43).
8. `execv` read `/bin/echo` from the initrd, built a new address space, loaded two ELF segments
   (Ch. 40, 44, 34).
9. `crt0` set up argv and called `main` (Ch. 45).
10. `write(1, "hello", 5)` went through the syscall gate, the fd table, the VFS, FAT16, and the ATA
    driver (Ch. 33, 43, 39, 42, 37).
11. `exit(0)` made a zombie; the shell's `wait` reaped it (Ch. 34).

Eleven steps across eleven chapters, and the user typed nineteen characters.

---

## 10. What a real shell adds

| | What it needs |
|---|---|
| Quoting and escapes | A real tokeniser; cannot split in place |
| Variables, `export` | A symbol table, `$` expansion, envp |
| Globbing (`*.txt`) | Directory reads and pattern matching in the shell |
| Command substitution | A recursive invocation and a pipe |
| Job control (`&`, `fg`, `bg`) | Process groups, a controlling terminal, `SIGTSTP` |
| `$?`, `$!`, `$$` | Tracking the last status, last pid, own pid |
| Conditionals, loops, functions | A grammar and an interpreter |
| Line editing, history | Raw mode, and a line editor (Ch. 20, §7) |
| Scripts | `#!` handling in the kernel's `exec` |

### 10.1 Signals, which everything above wants

Ctrl-C currently abandons a line (Chapter 20, §3.3). Proper handling needs:

- **A pending-signal mask** per process.
- **Delivery** on the way back to userland, from the syscall/interrupt return path.
- **A signal frame** built on the user stack, holding the interrupted context.
- **`sigreturn`**, a syscall that unwinds it — which is why the frame includes a tiny piece of code
  or a pre-arranged return address.
- **Default actions**: terminate, ignore, stop.
- **Process groups**, so Ctrl-C reaches the foreground job and not the shell.

That last one is what job control is built on, and it is why a terminal has a *controlling process
group* at all.

About 300 lines, and it is the single largest missing feature in Nimbus. Exercise 46.8.

### 10.2 `#!`

```
#!/bin/sh
echo hello
```

The kernel's `exec` reads the first two bytes; if they are `#!`, it reads the rest of the line,
and execs *that* with the original path appended as an argument.

Fifteen lines in `task_exec_regs`, and it is what makes every scripting language work. Exercise 46.6.

---

## 11. Exercises

🟢 **46.1** Add a `clear` built-in. (It needs an escape sequence or a syscall — which tells you
something about §10.)

🟢 **46.2** Make `cat` stop reading on a short read instead of on 0, then run `ls | cat`.

🟢 **46.3** Add `wc` that counts lines, words and bytes from stdin.

🟡 **46.4** Implement `SYS_GETCWD`: store a path string in the task struct, update it in `chdir`, and
make `pwd` tell the truth. Handle `..` and absolute paths.

🟡 **46.5** Add `>>`, `2>` and `2>&1`. Note that `2>&1` needs no `open` at all.

🟡 **46.6** Implement `#!` in `task_exec_regs`, then write a shell script.

🟡 **46.7** Add quoting: `'` and `"`, with escapes inside the latter. You will have to stop
tokenising in place.

🔴 **46.8** Implement signals: the mask, delivery on the return path, a signal frame, `sigreturn`,
and default actions. Then make Ctrl-C send SIGINT to the foreground process.

🔴 **46.9** Add job control: process groups, `&` to background, `fg`/`bg`/`jobs`, and a controlling
terminal that knows its foreground group. This needs 46.8 first.

---

## What we covered

- A shell as an ordinary unprivileged program, and the two-call design that makes redirection
  ordinary code.
- EOF ending the loop, which is why Ctrl-D exits a shell.
- Tokenising in place, and the four things that stop it being possible.
- The complete rule for what must be a built-in, and why `pwd` lies.
- Redirection parsed in the parent and applied in the child, with `close` after `dup2` not being
  optional.
- A pipeline's reference counting, and the two `wait`s that ignore which is which.
- Command resolution, and why `PATH` without `.` is a security measure.
- Six utilities, including the short-read bug that only a pipe exposes and the no-arguments rule that
  makes a filter.
- Nine things a real shell has, and the 300 lines of signal handling that most of them need.
- Eleven chapters exercised by nineteen typed characters.

**Part VI is finished.** [Chapter 47](47-debugging.md) is the chapter you should have read first.

---

[← A C library](45-user-libc.md) · [Contents](README.md) · [Next: Debugging →](47-debugging.md)
