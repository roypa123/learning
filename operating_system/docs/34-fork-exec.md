# Chapter 34 — fork, exec, exit, wait

[← System calls](33-syscalls.md) · [Contents](README.md) · [Next: Blocking →](35-blocking.md)

---

## Goal

Make one process into two, replace a process with a program, and clean up afterwards. Four calls,
and the first of them returns twice.

---

## 1. Why two calls and not one

Most systems have a single "create a process running this program" call: `CreateProcess` on Windows,
`posix_spawn`, `vfork`+`exec` variants. Unix splits it into `fork` (duplicate me) and `exec` (become
a different program).

The reason is in [`task.c`](../nimbus/kernel/task.c):

> Everything a shell does between fork and exec — redirecting stdout, closing descriptors, changing
> directory — is ordinary code running in the child, needing no support from exec at all. Two simple
> calls compose into every case that a single complicated call has to enumerate as parameters.

Compare. With `fork`+`exec`, redirection is:

```c
    if (fork() == 0) {
        int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        execv(path, args);
    }
```

Ordinary code. With a single spawn call, the API needs a parameter for it — and then another for
stdin, and another for closing arbitrary descriptors, and another for the working directory, and
another for the environment. `CreateProcess` takes ten parameters and a struct with eighteen fields.

The cost of the Unix design is that `fork` copies an address space you are about to throw away
(Chapter 28, §4), which is why `vfork` and `posix_spawn` exist as optimisations.

---

## 2. fork: returning twice

```c
pid_t task_fork_regs(registers_t *regs)
{
    task_t *parent = current_task;
    task_t *child  = task_alloc();
    if (!child) return -EAGAIN;

    strlcpy(child->name, parent->name, TASK_NAME_LEN);
    child->ppid     = parent->pid;
    child->priority = parent->priority;
    child->brk      = parent->brk;
    child->user_stack_bottom = parent->user_stack_bottom;

    child->directory = paging_clone_directory(parent->directory);
    ...
```

The child is a copy of the parent that differs in **exactly one observable way**: `fork()` returns 0
in it.

Everything else — registers, open files, memory contents, the instruction it resumes at — is
identical.

### 2.1 The trick

```c
    registers_t *child_frame =
        (registers_t *)(child->kernel_stack - sizeof(registers_t));
    memcpy(child_frame, regs, sizeof(registers_t));
    child_frame->eax = 0;                       /* <- fork() returns 0 here */

    uint32_t *sp = (uint32_t *)child_frame;
    *--sp = (uint32_t)isr_return;               /* popped by switch_context's ret */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    child->context = (context_t *)sp;
```

Four lines, and they are the answer to "how can one call return twice".

**The parent** returns through the normal syscall path. `syscall_dispatch` writes
`regs->eax = child->pid`, the stub pops the frame, `iret` resumes it after the `int 0x80`.

**The child** has no syscall in progress at all. So we manufacture one:

1. Copy the parent's trap frame onto the child's fresh kernel stack.
2. Set `EAX` to 0 in the copy.
3. Point the child's saved context at `isr_return` — the label in the middle of `isr_common_stub`
   (Chapter 16, §6.4).

> When the scheduler first picks the child, it unwinds a trap frame it never pushed and `iret`s to
> the instruction after the parent's `int 0x80`.

Same machinery as starting pid 1 (Chapter 32, §5.1) and as a preempted task resuming
(Chapter 31, §4.3). Three uses, one path.

### 2.2 Why the frame goes at the top

```c
    registers_t *child_frame =
        (registers_t *)(child->kernel_stack - sizeof(registers_t));
```

> It must be at the top, because `tss.esp0` will point there and the next interrupt the child takes
> will push below it.

If the frame were lower, the space above it would be unused, and the first interrupt after the child
starts would push its frame *over* the space the `iret` is still reading. The child would resume with
a corrupted `EIP`.

### 2.3 File descriptors are shared, not copied

```c
    for (int i = 0; i < MAX_FDS; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i]) file_ref(child->fds[i]);
    }
    child->cwd = parent->cwd;
```

The **descriptor table** is copied; the **`file_t` objects** are shared, with a refcount.

Chapter 43 covers the three-level structure. The consequence here:

> That is what makes `(echo a; echo b) > f` work — and it is why a naive implementation that copies
> the offset produces a file containing only "b".

Both processes share the `file_t`, so they share the read/write offset. `echo a` writes at offset 0
and advances it to 2; `echo b` writes at offset 2.

Copy the `file_t` instead and both write at offset 0.

---

## 3. exec: keep the process, replace the program

```c
int task_exec_regs(registers_t *regs, const char *path, char *const argv[])
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -ENOENT;
    if (node->flags & VFS_DIRECTORY) return -EISDIR;

    uint8_t *image = (uint8_t *)kmalloc(node->length);
    if (!image) return -ENOMEM;

    ssize_t got = vfs_read(node, 0, node->length, image);
    if (got < 0 || (size_t)got != node->length) { kfree(image); return -EIO; }

    if (!elf_validate(image, node->length)) { kfree(image); return -ENOEXEC; }
```

The pid, the parent, the open descriptors and the current directory all survive. The address space
does not.

### 3.1 Read the whole file first

> Demand paging an executable straight off the disk is what a real kernel does and it needs the page
> fault handler to know how to find the file; Chapter 44 describes the upgrade. Reading it up front
> means the old address space is not destroyed until we know the new program is loadable.

That second clause is the important one. `exec` has a point of no return: once the old address space
is gone, a failure cannot be reported to a program that no longer exists.

So everything that can fail happens first: the lookup, the read, the ELF validation. Only then do we
build the new address space, and only after *that* do we destroy the old one.

### 3.2 Copy argv before destroying the address space

```c
    char  *argv_copy[32];
    int    argc = 0;

    while (argv && argv[argc] && argc < 31) {
        size_t len = strlen(argv[argc]) + 1;
        argv_copy[argc] = (char *)kmalloc(len);
        if (!argv_copy[argc]) break;
        memcpy(argv_copy[argc], argv[argc], len);
        argc++;
    }
```

> argv points into the *old* address space, which is about to cease existing.

The strings were already copied into kernel memory once, by `sys_exec` (Chapter 33, §6.1). This is a
second copy, because `task_exec_regs` is also callable from the kernel with kernel strings, and it
should not care which.

### 3.3 Switch, then write the stack

```c
    paging_switch_directory(new_dir);
    current_task->directory = new_dir;

    uint8_t *sp = (uint8_t *)USER_STACK_TOP;
    char    *user_argv[32];

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv_copy[i]) + 1;
        sp -= len;
        memcpy(sp, argv_copy[i], len);
        user_argv[i] = (char *)sp;
        kfree(argv_copy[i]);
    }
```

Here we *do* switch address spaces, unlike `task_spawn_user` (Chapter 32, §5.3).

The reason: we need the strings' **addresses**, not just their bytes, because `argv[]` has to contain
pointers that will be valid in the new address space. Writing through the direct map would give us
kernel addresses.

> Everything from here to `enter_usermode` runs in the new address space with the kernel still mapped
> — which it is, because the kernel half is shared by every directory.

Chapter 28, §2.2 again. Switching `CR3` mid-function is safe only because of that.

### 3.4 The stack layout crt0 expects

```c
    sp = (uint8_t *)ALIGN_DOWN((uintptr_t)sp, 16);

    sp -= sizeof(char *);
    *(char **)sp = NULL;
    for (int i = argc - 1; i >= 0; i--) {
        sp -= sizeof(char *);
        *(char **)sp = user_argv[i];
    }

    char **argv_base = (char **)sp;

    sp -= sizeof(char **);
    *(char ***)sp = argv_base;
    sp -= sizeof(int);
    *(int *)sp = argc;
```

Building downwards:

```
    high   "ls\0"                    the strings
           "-l\0"
           (16-byte alignment pad)
           NULL                      argv[2]
           -> "-l"                   argv[1]
           -> "ls"                   argv[0]   <- argv_base
           argv_base                 the argv pointer
    low    argc                      <- ESP
```

And `crt0.asm` reads it:

```nasm
    mov eax, [esp]                  ; argc
    mov ebx, [esp + 4]              ; argv

    push ebx
    push eax
    call main
```

> That layout is a choice our kernel made, not a law. Linux puts argc, then argv, then a NULL, then
> envp, then a NULL, then the auxiliary vector — and a real crt0 has to walk past all of it to find
> envp.

### 3.5 Rewriting the trap frame

```c
    regs->eip     = entry;
    regs->useresp = (uint32_t)sp;
    regs->eax     = 0;
    regs->ecx = regs->edx = regs->ebx = 0;
    regs->esi = regs->edi = regs->ebp = 0;
    regs->cs      = SEL_UCODE;
    regs->ss      = SEL_UDATA;
    regs->ds      = SEL_UDATA;
    regs->eflags |= 0x200;
```

> Rather than call `enter_usermode`, we rewrite the saved registers that the interrupt stub is about
> to restore. The `iret` at the end of the syscall path then lands in the new program instead of
> after the old `int 0x80`. Same mechanism, no second code path.

This is the second of the three features that depend on `registers_t` being a pointer
(Chapter 16, §6.3).

Note the registers are **zeroed**. Leaving the old program's register values would leak information
across an `exec` — which matters on a real system where `exec` can cross a privilege boundary
(setuid), and is good hygiene regardless.

`|= 0x200` rather than `= 0x202`, preserving whatever else was in `EFLAGS`. Setting `IF` is the part
that matters (Chapter 32, §3.3).

### 3.6 The old address space, last

```c
    if (old_dir != kernel_directory)
        paging_free_directory(old_dir);

    tss_set_kernel_stack(current_task->kernel_stack);
    return 0;
```

> Only now is the old address space unreachable and safe to destroy.

We already switched `CR3` to `new_dir`, so nothing in `old_dir` is in use. Freeing it before the
switch would unmap the stack we are standing on.

---

## 4. exit: what you cannot free yourself

```c
void task_exit(int status)
{
    task_t *t = current_task;

    for (int i = 0; i < MAX_FDS; i++) {
        if (t->fds[i]) { file_unref(t->fds[i]); t->fds[i] = NULL; }
    }

    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == t->pid)
            tasks[i].ppid = 1;

    t->exit_status = status;
    t->state       = TASK_ZOMBIE;

    task_t *parent = task_find(t->ppid);
    if (parent) sched_wake(parent);

    sched_remove(t);
    schedule();

    panic("task_exit: the scheduler returned to a dead task (pid %d)", t->pid);
}
```

### 4.1 Descriptors first

Closing them may have side effects that matter: the last reference to a pipe's write end is what
tells the reader there will be no more data (Chapter 43, §3).

A process that exits without closing its pipes leaves the other end blocked forever.

### 4.2 Re-parenting orphans

```c
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == t->pid)
            tasks[i].ppid = 1;
```

> Hand any children to init (pid 1), so that they still have someone to reap them. Skipping this
> leaks a task slot per orphan.

A zombie whose parent is gone is never reaped, because only the parent calls `wait` for it. Its task
slot, kernel stack and address space stay allocated forever.

This is what init is *for*. Its entire job is to call `wait()` in a loop and reap whatever arrives.

### 4.3 Becoming a zombie

```c
    t->exit_status = status;
    t->state       = TASK_ZOMBIE;
```

Chapter 28, §5.2: a process cannot free its own address space or kernel stack while standing on them.

And the status has to survive until someone asks.

### 4.4 Never returning

```c
    sched_remove(t);
    schedule();

    panic("task_exit: the scheduler returned to a dead task (pid %d)", t->pid);
```

`sched_remove` takes it off the run queue; `schedule()` switches away and never comes back, because
nothing will ever pick a `ZOMBIE`.

The `panic` is unreachable, and it is there because "unreachable" is a claim worth checking. If it
ever fires, `pick_next` has a bug that would otherwise present as a dead process running again.

---

## 5. wait: the parent does the rest

```c
pid_t task_wait(int *status)
{
    task_t *self = current_task;

    for (;;) {
        bool have_children = false;

        for (int i = 0; i < MAX_TASKS; i++) {
            task_t *t = &tasks[i];
            if (t->state == TASK_UNUSED || t->ppid != self->pid) continue;

            have_children = true;

            if (t->state == TASK_ZOMBIE) {
                pid_t pid = t->pid;
                if (status) *status = t->exit_status;

                if (t->directory && t->directory != kernel_directory)
                    paging_free_directory(t->directory);

                if (t->kernel_stack)
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));

                t->state = TASK_UNUSED;
                return pid;
            }
        }

        if (!have_children) return -ECHILD;

        sched_block(self);
    }
}
```

The other half of exit. Now, from a different stack and a different address space, it is safe to
release the rest.

**`-ECHILD` when there are no children at all**, which is how a shell knows not to wait forever.

**`sched_block(self)`** — the parent's own task struct as the wait channel, woken by
`sched_wake(parent)` in `task_exit`. Both sides name the same object (Chapter 31, §6.1).

**The `for(;;)`** because `sched_wake` may have woken us for a different child, or another `wait`
may have taken the zombie. `while`, never `if` (Chapter 31, §6).

### 5.1 The kernel stack subtraction

```c
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));
```

`kernel_stack` is the **top** (Chapter 29, §4.3), so recovering the allocation's base needs the
subtraction.

`kfree(t->kernel_stack)` would be a pointer into the middle of a heap block, and the magic check
would catch it (Chapter 27, §4) — which is exactly the kind of thing those magic numbers are for.

---

## 6. Running it

### 6.1 forktest

```c
int main(void)
{
    printf("parent pid is %d\n", getpid());

    pid_t pid = fork();

    if (pid == 0) {
        for (int i = 0; i < 5; i++) {
            printf("  child  %d: %d\n", getpid(), i);
            sleep_ms(120);
        }
        exit(7);
    }

    for (int i = 0; i < 5; i++) {
        printf("parent %d: %d\n", getpid(), i);
        sleep_ms(100);
    }

    int status = 0;
    pid_t reaped = wait(&status);
    printf("child %d exited with %d\n", reaped, status);
    return 0;
}
```

```
nimbus> forktest
parent pid is 4
parent 4: 0
  child  5: 0
parent 4: 1
  child  5: 1
parent 4: 2
  child  5: 2
parent 4: 3
  child  5: 3
parent 4: 4
  child  5: 4
child 5 exited with 7
```

> Both processes execute this same line of the same program, at the same address, with different page
> tables behind it.

The interleaving is not cooperative — neither yields to the other. The different sleep intervals (100
and 120 ms) make them drift apart, which is how you can tell they are genuinely independent.

### 6.2 The shell

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

Note the error path after `execv`:

> `execv` only returns if it failed, because on success there is no longer any code here to return to
> — the address space has been replaced. That is why every exec is followed by an error path and
> never by an `if`.

And 127 is the shell convention for "command not found", which is why `echo $?` after a typo gives
127 on any Unix.

### 6.3 Memory accounting

```
nimbus> mem
pmm: 1348/32512 frames used
nimbus> forktest
...
nimbus> mem
pmm: 1348/32512 frames used
```

Fork allocated an address space, exit made a zombie, wait freed it, and the count returned exactly.

If it does not, something leaked — usually a page table freed without its pages, or a kernel stack
freed with the wrong pointer.

---

## 7. What could go wrong

| Symptom | Cause |
|---|---|
| `fork` returns the same value in both | `child_frame->eax` not zeroed |
| Child starts at a random address | Trap frame not at the top of the kernel stack |
| `(cmd1; cmd2) > f` loses output | `file_t` copied instead of refcounted |
| `exec` succeeds but the program crashes | argv layout does not match crt0 |
| Fault in `exec` after the switch | Old address space freed too early |
| Task slots leak | Orphans not re-parented, or `wait` never called |
| Panic "scheduler returned to a dead task" | `pick_next` selecting a zombie |
| Heap magic panic in `wait` | `kfree` without the `- KERNEL_STACK_SIZE` |
| `exec` of a bad file destroys the process | Validation done after the address space was replaced |

---

## 8. Exercises

🟢 **34.1** Print the return value of `fork` in both processes and confirm one is 0 and the other is
a pid.

🟢 **34.2** Remove `child_frame->eax = 0` and run `forktest`. What does the child do?

🟢 **34.3** Write a program that forks and exits without the parent calling `wait`. Run it ten times
and watch `ps`.

🟡 **34.4** Make `exec` validate the ELF *after* building the new address space, then exec a
non-executable file. Describe the failure.

🟡 **34.5** Implement `waitpid(pid, ...)` that waits for a specific child, and make the shell use it.

🟡 **34.6** Pass an environment: add `envp` after argv on the stack, a `getenv` to libc, and an
`export` builtin to the shell.

🔴 **34.7** Implement copy-on-write (Exercise 28.7) and measure `fork` before and after with `rdtsc`.
A shell forking for every command should get dramatically faster.

🔴 **34.8** Implement `vfork`: the child shares the parent's address space and the parent is
suspended until the child execs or exits. It is faster than COW fork and it is genuinely dangerous —
document exactly which operations are legal in the child.

---

## What we covered

- Why Unix splits process creation in two, and what a single spawn call has to enumerate instead.
- `fork` returning twice, by manufacturing a trap frame the child never pushed and pointing its
  context at `isr_return`.
- Why that frame sits at the top of the kernel stack.
- Descriptor tables copied, `file_t` objects shared — and the shell idiom that depends on it.
- `exec`'s point of no return, and doing everything that can fail before crossing it.
- Switching `CR3` mid-function so that argv pointers are user addresses, which is safe only because
  the kernel half is shared.
- The argv layout, and how much more a real one carries.
- Rewriting the trap frame instead of a second entry path, and zeroing the registers on the way.
- `exit` closing descriptors first, re-parenting orphans to init, and the unreachable panic that is
  worth having.
- `wait` doing what `exit` could not, the `-ECHILD` that stops a shell hanging, and the subtraction
  that keeps `kfree` honest.
- Eleven lines in the shell, and the `exit(127)` after every `execv`.

[Chapter 35](35-blocking.md) is about what a task does when it has nothing to do, and why a spinning
kernel is a broken one.

---

[← System calls](33-syscalls.md) · [Contents](README.md) · [Next: Blocking →](35-blocking.md)
