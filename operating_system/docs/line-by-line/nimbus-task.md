# Line by line: `nimbus/kernel/task.c`

[Index](README.md) · [Chapter 29](../29-what-is-a-process.md) · [Chapter 32](../32-usermode.md) · [Chapter 34](../34-fork-exec.md)

---

## Storage

```c
static task_t  tasks[MAX_TASKS];
static pid_t   next_pid = 0;

extern void isr_return(void);
```
⚠️ 64 slots in `.bss`. The structure that tracks processes is needed before the allocator that would
allocate it.

`isr_return` is the label inside `isr_common_stub`. A forked child's first instruction.

---

## `task_alloc`

```c
    uint32_t flags = irq_save();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            memset(&tasks[i], 0, sizeof(task_t));
            tasks[i].state = TASK_EMBRYO;
            tasks[i].pid   = next_pid++;
```
⚠️ **`memset` first.** A recycled slot holds the previous occupant's file descriptors, wait channel
and directory pointer. Reusing any of them is a use-after-free with a very confusing symptom.

⚠️ **`TASK_EMBRYO` before returning.** The slot is claimed but not runnable, so a concurrent
`task_alloc` cannot take it while the caller is still filling it in.

Without an intermediate state the only options are "unused" (someone else takes it) and "ready" (the
scheduler runs a half-built task). The lock covers the claim; the state covers the gap.

---

## `task_init`

```c
    task_t *t = task_alloc();
    ASSERT(t != NULL && t->pid == 0);

    strlcpy(t->name, "idle", TASK_NAME_LEN);
    t->state        = TASK_RUNNING;
    t->directory    = kernel_directory;
    t->priority     = PRIORITY_IDLE;
```
We are already executing on the boot stack with no task struct.

Rather than create a task and switch to it — which needs a context to switch *from* — we declare that
**what is already running is task 0**.

⚠️ `t->context` is left NULL, and that is correct: `switch_context(&prev->context, ...)` *writes*
`prev->context`. Nothing reads task 0's context until it has been saved once.

```c
    extern char stack_top[];
    t->kernel_stack = (uint32_t)stack_top;
```
The boot stack from `boot.asm`, claimed so that `esp0` has something valid before the first task
exists.

```c
    tss_set_kernel_stack(t->kernel_stack);
```
Even though nothing runs in ring 3 yet, so there is never a window where `esp0` is zero.

---

## `task_spawn_kernel`

```c
    t->kernel_stack = (uint32_t)(stack + KERNEL_STACK_SIZE);
```
⚠️ The **top**, because stacks grow down. Which is why the free later needs a subtraction.

```c
    uint32_t *sp = (uint32_t *)t->kernel_stack;
    *--sp = (uint32_t)kernel_thread_exit;
    *--sp = (uint32_t)entry;
    *--sp = 0;                                  /* ebp */
    *--sp = 0;                                  /* ebx */
    *--sp = 0;                                  /* esi */
    *--sp = 0;                                  /* edi */

    t->context = (context_t *)sp;
```
Manufacturing a stack for something that has never run.

`switch_context` will pop four registers and `ret`, so we lay out exactly what those instructions
expect. The first switch pops four zeros and returns to `entry`, on a stack that looks as though
`entry` had been called normally.

```c
static void kernel_thread_exit(void)
{
    task_exit(0);
}
```
A kernel thread whose function returns would otherwise `ret` into whatever four bytes are above the
stack. Same principle as `crt0`'s `.hang`.

---

## `task_fork_regs`

```c
    child->directory = paging_clone_directory(parent->directory);
```
Every user page copied. Chapter 28, §4 — COW is what this should become.

```c
    registers_t *child_frame =
        (registers_t *)(child->kernel_stack - sizeof(registers_t));
    memcpy(child_frame, regs, sizeof(registers_t));
    child_frame->eax = 0;
```
⚠️ **Four lines, and they are the answer to "how can one call return twice".**

The parent returns through the normal syscall path with the child's pid in `EAX`. The child has no
syscall in progress, so we manufacture one: copy the parent's trap frame, set `EAX` to 0.

⚠️ **At the top of the stack**, because `tss.esp0` will point there and the next interrupt the child
takes will push below it. Lower down, the first interrupt would push over the frame the `iret` is
still reading.

```c
    uint32_t *sp = (uint32_t *)child_frame;
    *--sp = (uint32_t)isr_return;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    child->context = (context_t *)sp;
```
Point the context at `isr_return`. When the scheduler first picks the child, it unwinds a trap frame
it never pushed and `iret`s to the instruction after the parent's `int 0x80`.

```c
    for (int i = 0; i < MAX_FDS; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i]) file_ref(child->fds[i]);
    }
```
⚠️ The **table** is copied; the `file_t` objects are **shared**, with a refcount.

> That is what makes `(echo a; echo b) > f` work — and it is why a naive implementation that copies
> the offset produces a file containing only "b".

---

## `task_spawn_user`

```c
    uint8_t *image = (uint8_t *)kmalloc(node->length);
    if (vfs_read(node, 0, node->length, image) != (ssize_t)node->length) { ... }
```
Read the whole file. No demand paging.

```c
    paddr_t  stack_phys = paging_virt_to_phys(t->directory, stack_bottom);
    uint8_t *stack_kv   = (uint8_t *)P2V(stack_phys & PTE_FRAME_MASK);

    uint32_t *top = (uint32_t *)(stack_kv + PAGE_SIZE);
    *--top = 0;                                   /* argv = NULL */
    *--top = 0;                                   /* argc = 0    */
```
⚠️ We are **not** running in the new address space, so `USER_STACK_TOP` is not a usable pointer.

Translate to physical, reach it through the direct map. Cheaper than switching `CR3`, and it leaves
the running task undisturbed.

(`task_exec_regs` does the opposite — it switches first, because it needs the argv *addresses* to be
user-space addresses.)

```c
    registers_t *frame = (registers_t *)(t->kernel_stack - sizeof(registers_t));
    memset(frame, 0, sizeof(registers_t));

    frame->eip     = entry;
    frame->cs      = SEL_UCODE;
    frame->eflags  = 0x202;
    frame->useresp = user_esp;
    frame->ss      = SEL_UDATA;
    frame->ds      = SEL_UDATA;
```
`0x202` is `IF` (bit 9) plus bit 1, which is reserved and **always 1** on every x86. Including it
makes the value match what a register dump shows.

```c
    vfs_node_t *con = console_device_node();
    for (int i = 0; i < 3; i++)
        t->fds[i] = file_open_node(con, i == 0 ? O_RDONLY : O_WRONLY);
```
stdin, stdout, stderr.

> They are not magic — they are simply the first three descriptors, and they are inherited by every
> child, which is the entire mechanism behind `>` and `|`.

---

## `task_exec_regs`

```c
    if (!elf_validate(image, node->length)) { kfree(image); return -ENOEXEC; }
```
⚠️ **Everything that can fail happens before the point of no return.** Once the old address space is
gone, a failure cannot be reported to a program that no longer exists.

Lookup, read, validate — then build, then destroy.

```c
    while (argv && argv[argc] && argc < 31) {
        size_t len = strlen(argv[argc]) + 1;
        argv_copy[argc] = (char *)kmalloc(len);
        memcpy(argv_copy[argc], argv[argc], len);
```
⚠️ argv points into the **old** address space, which is about to cease existing.

(The strings were already copied once by `sys_exec`; this second copy is because `task_exec_regs` is
also callable with kernel strings and should not care which.)

```c
    paging_switch_directory(new_dir);
    current_task->directory = new_dir;

    uint8_t *sp = (uint8_t *)USER_STACK_TOP;
    for (int i = argc - 1; i >= 0; i--) {
        ...
        user_argv[i] = (char *)sp;
```
⚠️ Here we **do** switch, because we need the strings' *addresses* for `argv[]` — writing through the
direct map would give kernel addresses.

Safe only because the kernel half is shared.

```c
    sp -= sizeof(char **);
    *(char ***)sp = argv_base;
    sp -= sizeof(int);
    *(int *)sp = argc;
```
The layout `crt0.asm` reads: `[esp]` argc, `[esp+4]` argv.

```c
    regs->eip     = entry;
    regs->useresp = (uint32_t)sp;
    regs->eax     = 0;
    regs->ecx = regs->edx = regs->ebx = 0;
    regs->esi = regs->edi = regs->ebp = 0;
```
⚠️ Rewriting the saved registers the stub is about to restore, rather than a second entry path. The
`iret` at the end of the syscall lands in the new program.

Registers **zeroed** — leaving the old program's values would leak information across an `exec`.

```c
    regs->eflags |= 0x200;
```
`|=` rather than `=`, preserving whatever else was there. Setting `IF` is the part that matters.

```c
    if (old_dir != kernel_directory)
        paging_free_directory(old_dir);
```
⚠️ **Last.** We already switched `CR3`, so nothing in `old_dir` is in use. Freeing before the switch
would unmap the stack we are standing on.

---

## `task_exit`

```c
    for (int i = 0; i < MAX_FDS; i++) {
        if (t->fds[i]) { file_unref(t->fds[i]); t->fds[i] = NULL; }
    }
```
⚠️ Descriptors **first**, because closing may have side effects that matter — the last reference to a
pipe's write end is what tells the reader there will be no more data.

```c
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == t->pid)
            tasks[i].ppid = 1;
```
⚠️ Re-parent orphans to init. A zombie whose parent is gone is never reaped, and its slot, stack and
address space stay allocated forever.

That is what init is *for*: call `wait()` in a loop and reap whatever arrives.

```c
    t->state       = TASK_ZOMBIE;

    task_t *parent = task_find(t->ppid);
    if (parent) sched_wake(parent);

    sched_remove(t);
    schedule();

    panic("task_exit: the scheduler returned to a dead task (pid %d)", t->pid);
```
Cannot free its own address space or kernel stack while standing on them — hence the zombie.

The `panic` is unreachable. It is there because "unreachable" is a claim worth checking: if it fires,
`pick_next` is selecting a zombie.

---

## `task_wait`

```c
            if (t->state == TASK_ZOMBIE) {
                pid_t pid = t->pid;
                if (status) *status = t->exit_status;

                if (t->directory && t->directory != kernel_directory)
                    paging_free_directory(t->directory);

                if (t->kernel_stack)
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));
```
⚠️ **`- KERNEL_STACK_SIZE`**, because `kernel_stack` is the top. `kfree(t->kernel_stack)` is a pointer
into the middle of a heap block — caught by the magic check, which is exactly what it is for.

```c
        if (!have_children) return -ECHILD;

        sched_block(self);
```
`-ECHILD` is how a shell knows not to wait forever.

The channel is the parent's own task struct, woken by `sched_wake(parent)` in `task_exit`. Both sides
name the same object.

⚠️ The enclosing `for(;;)` because `sched_wake` may have woken us for a different child, or another
`wait` may have taken the zombie. `while`, never `if`.

---

[Index](README.md) · [Chapter 29](../29-what-is-a-process.md) · [Chapter 34](../34-fork-exec.md)
