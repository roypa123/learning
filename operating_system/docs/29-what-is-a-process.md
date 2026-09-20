# Chapter 29 — What a process is

[← Address spaces](28-address-spaces.md) · [Contents](README.md) · [Next: The context switch →](30-context-switch.md)

> 📖 **Line by line:** [task.c](line-by-line/nimbus-task.md)

---

## Goal

Define a process, build the structure that represents one, and create the first task — which is the
one we are already running in.

The definition is shorter than you would expect, and that is the point of the chapter.

---

## 1. A process is four things

```
    an address space        which page directory CR3 points at
    a kernel stack          where its state lives while it is in the kernel
    a saved context         five registers, so it can be resumed
    some bookkeeping        pid, parent, open files, exit status
```

That is all. Everything else people say about processes — isolation, concurrency, the illusion of
having the machine to yourself — is an **emergent property** of those four plus a timer interrupt.

Isolation comes from the address space (Chapter 28). Concurrency comes from the saved context plus
something that switches between them (Chapter 30). The illusion comes from doing it a hundred times
a second (Chapter 31).

---

## 2. The task struct

```c
typedef struct task {
    /* --- identity ------------------------------------------------------- */
    pid_t             pid;
    pid_t             ppid;
    char              name[TASK_NAME_LEN];
    task_state_t      state;

    /* --- what the scheduler needs ---------------------------------------- */
    context_t        *context;
    uint32_t          kernel_stack;
    uint32_t          priority;
    uint32_t          time_slice;
    uint64_t          wake_tick;
    void             *wait_channel;
    int               exit_status;

    /* --- address space ---------------------------------------------------- */
    page_directory_t *directory;
    vaddr_t           brk;
    vaddr_t           user_stack_bottom;

    /* --- open files -------------------------------------------------------- */
    struct file      *fds[MAX_FDS];
    struct vfs_node  *cwd;

    /* --- bookkeeping ------------------------------------------------------- */
    uint64_t          ticks_used;
    struct task      *next;
} task_t;
```

About 200 bytes. Linux's `task_struct` is roughly ten kilobytes and has several hundred fields, but
the first four groups are recognisably the same four groups.

### 2.1 A fixed table

```c
#define MAX_TASKS        64

static task_t  tasks[MAX_TASKS];
```

Sixty-four processes, in `.bss`, allocated at build time.

The reasons are the same as the PMM's fixed bitmap (Chapter 22, §3.1): the structure that tracks
processes is needed before the allocator that would allocate it, and a linear array is something you
can dump and read.

The cost is a hard limit and 12 KiB of `.bss` whether or not you use it. Real kernels allocate task
structs from a slab and chain them into hash tables by pid; that is Exercise 29.7.

```c
static task_t *task_alloc(void)
{
    uint32_t flags = irq_save();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            memset(&tasks[i], 0, sizeof(task_t));
            tasks[i].state = TASK_EMBRYO;
            tasks[i].pid   = next_pid++;
            irq_restore(flags);
            return &tasks[i];
        }
    }

    irq_restore(flags);
    return NULL;
}
```

**`memset` first.** A recycled slot holds the previous occupant's file descriptors, `wait_channel`,
and `directory` pointer. Reusing any of them is a use-after-free with a very confusing symptom.

**`TASK_EMBRYO` before returning.** The slot is claimed but not yet runnable, so a concurrent
`task_alloc` cannot take it while the caller is still filling it in. Without an intermediate state,
the only options are "unused" (someone else can take it) and "ready" (the scheduler might run a
half-built task).

---

## 3. The states

```c
typedef enum task_state {
    TASK_UNUSED = 0,   /* this slot is free                                   */
    TASK_EMBRYO,       /* being created; not yet runnable                     */
    TASK_READY,        /* runnable, waiting for the CPU                       */
    TASK_RUNNING,      /* on the CPU right now                                */
    TASK_BLOCKED,      /* waiting for something: I/O, a child, a lock         */
    TASK_SLEEPING,     /* waiting for a deadline in ticks                     */
    TASK_ZOMBIE        /* exited, but the parent has not collected the status */
} task_state_t;
```

```
                task_alloc
                    |
                    v
              [ EMBRYO ]
                    | sched_add
                    v
    +--------> [ READY ] <---------------+
    |               | schedule()         |
    |               v                    |
    |         [ RUNNING ]                |
    |          /    |    \               |
    |  slice  /     |     \ sched_block  |
    |  used  /      |      \             |
    +-------+       |       +--> [ BLOCKED ] --+ sched_wake
                    |       |                  |
                    |       +--> [ SLEEPING ] -+ deadline
                    | task_exit
                    v
              [ ZOMBIE ]
                    | task_wait
                    v
              [ UNUSED ]
```

Two states that beginners tend to merge, and should not:

**`BLOCKED` versus `SLEEPING`.** Blocked is waiting for an *event* — a keypress, a child exiting, a
semaphore. Sleeping is waiting for a *deadline*. They are woken by completely different mechanisms:
`sched_wake(channel)` for one, the timer tick for the other.

```c
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }
```

Merging them means the tick has to check every blocked task's deadline, most of which do not have
one.

**`ZOMBIE` is not a bug.** It is a deliberate state, and Chapter 28, §5.2 gave the reason: a process
cannot free its own address space or kernel stack while standing on them. The parent does it, later.
Plus the exit status has to survive until somebody asks for it.

---

## 4. The kernel stack

```c
#define KERNEL_STACK_SIZE 8192      /* 2 pages per task */
```

**Every task has its own**, separate from its user stack.

### 4.1 Why not use the user stack?

Three reasons, each sufficient on its own:

**It might not be mapped.** A process that is in the middle of growing its stack (Chapter 26, §3)
takes an interrupt, and the CPU has to push a frame — onto a page that faults. That is a double
fault.

**It might be malicious.** A user program can set `ESP` to anything. If the kernel pushed onto it,
a program could point it at a kernel address and have the CPU write there.

**It might be tiny.** A program with a 64-byte stack left is perfectly legal; an interrupt handler
needs a few hundred bytes.

So the CPU switches stacks automatically on a privilege change, reading `ss0:esp0` from the TSS
(Chapter 15, §4.2) — and that is the address this field holds.

### 4.2 8 KiB, and what it has to hold

Two pages. What lives on it:

```
    top      +--------------------+  <- tss.esp0 points here
             | registers_t        |  76 bytes: the trap frame
             +--------------------+
             | syscall dispatch   |
             | vfs_read           |
             | fat16_read         |
             | ata_read_sectors   |  a sector buffer: 512 bytes
             +--------------------+
             |  ...               |
    bottom   +--------------------+
```

The deepest path in Nimbus is a `read()` through the VFS into FAT16 into ATA, and `fat16_read` has a
512-byte `uint8_t sector[512]` local. 8 KiB is comfortable for that.

It would not be for a kernel with recursion, deep filesystem stacking, or large stack buffers. Linux
uses 8 KiB or 16 KiB and is careful; a function with a 1 KiB local in kernel code is a bug.

### 4.3 No guard page

```c
    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (!stack) { t->state = TASK_UNUSED; return NULL; }

    t->kernel_stack = (uint32_t)(stack + KERNEL_STACK_SIZE);
```

From `kmalloc`, so the memory below it is another heap block.

An overflow therefore corrupts a neighbouring allocation silently. Chapter 26, §3.3 flagged this, and
Exercise 26.6 fixes it by allocating stacks page-aligned with an unmapped page beneath.

Note `kernel_stack` is the **top**, because stacks grow down. Storing the bottom and adding the size
everywhere is a mistake waiting to be made once.

And note the free:

```c
                if (t->kernel_stack)
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));
```

Subtracting to recover the allocation's base. This is exactly why the "store the top" decision needs
a comment — `kfree(t->kernel_stack)` would be a wild pointer into the middle of a block, caught by
the magic check (Chapter 27, §4) but only because that check exists.

---

## 5. The saved context

```c
typedef struct context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;      /* where to resume: the return address of the switch   */
} context_t;
```

**Five registers.** Not sixteen.

The source explains it:

> The System V calling convention divides registers into caller-saved (EAX, ECX, EDX — a function may
> destroy them) and callee-saved (EBX, ESI, EDI, EBP — a function must preserve them). Whoever called
> switch_context() is a C function, so it has already spilled anything it cared about in the
> caller-saved set. We only need the other four, plus ESP and EIP, which are the switch itself.

`ESP` is not in the struct because the struct *lives on the stack* — saving `ESP` saves the address
of the struct, which is the pointer in `task->context`.

`EIP` is the return address, already on the stack because we were called.

Chapter 30 goes through the fourteen instructions. What matters here is the field order:

> This is why a context switch is twenty instructions and not a hundred, and why the field order here
> must match `switch.s` exactly.

Reorder these four without changing `cpu.asm` and the scheduler restores `EBP` into `EDI`.

---

## 6. Where a user program's pieces go

```c
#define USER_CODE_BASE   0x08048000u
#define USER_STACK_TOP   0xBFFFF000u
#define USER_STACK_SIZE  (64 * KiB)
#define USER_HEAP_BASE   0x40000000u
```

```
    0xBFFFF000  +------------------+  user stack top
                | stack            |  grows DOWN, one page initially
                |   |              |
                |   v              |
                |                  |
                |   (unmapped)     |  <- the guard: a wild pointer faults here
                |                  |
                |   ^              |
                |   |              |
                | heap             |  grows UP via sbrk
    ~0x0804A000 +------------------+  brk, just past .bss
                | .bss .data       |
                | .rodata .text    |
    0x08048000  +------------------+
                |                  |
                |   (unmapped)     |  128 MiB
    0x00000000  +------------------+
```

These are **choices**, not hardware constraints, and the source gives the reasons:

> Code at 0x08048000 rather than 0: leaving the first 128 MiB unmapped means dereferencing a null
> pointer, or a small offset from one (`p->field` where p is NULL), faults instead of reading real
> memory.
>
> Stack at the top of the user half, growing down, so that the heap growing up and the stack growing
> down have the maximum distance between them.

`0x08048000` is Linux's traditional base for 32-bit ELF executables, and copying it means a
disassembly of a Nimbus binary looks familiar.

The gap between heap and stack is the guard. It is enormous — nearly 2 GiB — and Chapter 26's
distance check is what stops the stack consuming it.

---

## 7. Bootstrapping: task 0 is us

```c
void task_init(void)
{
    memset(tasks, 0, sizeof(tasks));

    task_t *t = task_alloc();
    ASSERT(t != NULL && t->pid == 0);

    strlcpy(t->name, "idle", TASK_NAME_LEN);
    t->state        = TASK_RUNNING;
    t->directory    = kernel_directory;
    t->priority     = PRIORITY_IDLE;
    t->time_slice   = SCHED_TIME_SLICE;
    t->ppid         = 0;

    extern char stack_top[];
    t->kernel_stack = (uint32_t)stack_top;

    current_task = t;
    tss_set_kernel_stack(t->kernel_stack);
}
```

We are executing on the boot stack from `boot.asm`, in the kernel's address space, with no task
struct.

Rather than create a task and switch to it — which needs a context to switch *from* — we declare that
**what is already running is task 0**.

> Its saved context will be written the first time it is switched away from, which is all a saved
> context is for.

`t->context` is left NULL, and that is correct: `switch_context(&prev->context, next->context)`
*writes* `prev->context`. Nothing reads task 0's context until it has been saved once.

### 7.1 Task 0 becomes idle

```c
    for (;;) {
        sti();
        hlt();
    }
```

`kmain` never returns; it turns into pid 0's main loop. When nothing else is runnable the scheduler
picks it, and it halts until the next interrupt.

`PRIORITY_IDLE` is the lowest level, so it is chosen only when nothing else can run
(Chapter 31, §3).

`sti` before `hlt`, every iteration, because `hlt` with interrupts disabled is a machine that never
wakes up (Chapter 3, §3.6).

### 7.2 `tss_set_kernel_stack` before anything else

Even though nothing runs in ring 3 yet, `esp0` must be valid before the first interrupt that *could*
come from ring 3. Setting it here, once, means there is never a window where it is zero.

---

## 8. Kernel threads

```c
task_t *task_spawn_kernel(const char *name, void (*entry)(void))
{
    task_t *t = task_alloc();
    if (!t) return NULL;

    strlcpy(t->name, name, TASK_NAME_LEN);

    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (!stack) { t->state = TASK_UNUSED; return NULL; }

    t->kernel_stack = (uint32_t)(stack + KERNEL_STACK_SIZE);

    uint32_t *sp = (uint32_t *)t->kernel_stack;
    *--sp = (uint32_t)kernel_thread_exit;
    *--sp = (uint32_t)entry;
    *--sp = 0;                                  /* ebp */
    *--sp = 0;                                  /* ebx */
    *--sp = 0;                                  /* esi */
    *--sp = 0;                                  /* edi */

    t->context    = (context_t *)sp;
    ...
}
```

A task that runs a C function in ring 0 and shares the kernel address space. Used for the idle task
and for anything that wants to *block* — a driver waiting on a device — without a userland process
behind it.

### 8.1 Manufacturing a stack for something that has never run

This is the interesting part, and it is the same trick `fork` and `exec` use in different forms.

`switch_context` will restore four registers and then `ret`. So we lay out exactly what those
instructions expect to find:

```
    high address   ... top of the freshly allocated stack
                   kernel_thread_exit   <- where `entry` returns to
                   entry                <- popped by switch_context's `ret`
                   0                       ebp
                   0                       ebx
                   0                       esi
    low address    0                       edi   <- context points here
```

The first switch to this task pops four zeros and returns to `entry`, on a stack that looks exactly
as though `entry` had been called normally.

> There is no special case anywhere in the scheduler for a task that has not started yet, because
> there does not need to be.

That is the design goal. A scheduler with an "is this task new?" branch has two code paths to get
right; this one has one.

### 8.2 The return address that catches a mistake

```c
static void kernel_thread_exit(void)
{
    task_exit(0);
}
```

A kernel thread whose function returns would otherwise `ret` into whatever four bytes are above the
stack.

Putting `kernel_thread_exit` there gives it the same fate as a user process that fell off the end of
`main` — which is what `crt0.asm` does in userland (Chapter 45, §2), for exactly the same reason.

---

## 9. Running it

```c
    task_init();
    ...
    task_spawn_kernel("counter", counter_thread);
    sched_init();
    sti();
```

with

```c
static void counter_thread(void)
{
    for (int i = 0; ; i++) {
        kprintf("[counter %d]\n", i);
        sleep_ms(500);
    }
}
```

```
[    0.020] inf  task: pid 0 (idle) adopted, kernel stack at c010cfe0
[    0.020] inf  task: pid 1 (counter) is a kernel thread
[counter 0]
[counter 1]
[counter 2]
```

A second thread of control, running concurrently with the idle loop, on its own stack. Chapter 30
explains the fourteen instructions that make the switch happen; this chapter built everything they
operate on.

### 9.1 `ps`

```c
void task_dump_all(void)
{
    kprintf(" PID PPID  STATE     TICKS  NAME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = &tasks[i];
        if (t->state == TASK_UNUSED) continue;
        kprintf("%4d %4d  %-9s %5u  %s\n",
                t->pid, t->ppid, state_name(t->state),
                (uint32_t)t->ticks_used, t->name);
    }
}
```

```
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running    1523  idle
   1    0  sleeping     12  counter
```

Thirteen lines, and it is the single most useful diagnostic in Part IV. `ticks_used` tells you which
task is consuming the machine; `state` tells you what everything is waiting for.

---

## 10. Exercises

🟢 **29.1** Add a `ticks_used` percentage to `ps`, computed against total uptime.

🟢 **29.2** Spawn three counter threads with different sleep intervals and watch them interleave in
`ps`.

🟢 **29.3** Write a kernel thread whose function returns immediately. Confirm `kernel_thread_exit`
catches it and the slot is reaped.

🟡 **29.4** Reorder two fields in `context_t` without changing `cpu.asm`. Predict what happens to the
first switch, then run it.

🟡 **29.5** Make `KERNEL_STACK_SIZE` 1024 and run something that goes through the FAT16 read path.
Where does the corruption show up?

🟡 **29.6** Add a `magic` field at the bottom of every kernel stack, set at creation and checked on
every schedule. This is a stack overflow canary and it is about six lines.

🔴 **29.7** Replace the fixed task table with slab-allocated task structs and a hash table keyed by
pid. Keep `task_dump_all` working, which means keeping a list as well.

🔴 **29.8** Add threads: tasks that share an address space and a file descriptor table but have their
own stack and context. Work out which fields of `task_t` move into a shared `struct process` and
which stay per-thread.

---

## What we covered

- A process is four things, and everything else about processes is emergent.
- A 200-byte task struct in a fixed table, `memset` on reuse, and the `EMBRYO` state that closes a
  window.
- Seven states, why `BLOCKED` and `SLEEPING` are different, and why `ZOMBIE` is deliberate.
- A separate kernel stack per task, and the three independent reasons the user stack cannot be used.
- 8 KiB, what has to fit on it, and the guard page it does not have.
- Storing the top rather than the bottom, and the `kfree` that has to subtract.
- Five registers in a context, because the calling convention already saved the others.
- The user address space layout, and the two deliberate gaps — 128 MiB at the bottom and 2 GiB in the
  middle.
- Adopting the running context as task 0, leaving its context NULL, and turning `kmain` into idle.
- Manufacturing a stack for a task that has never run, so the scheduler needs no special case.

[Chapter 30](30-context-switch.md) is the fourteen instructions. One at a time.

---

[← Address spaces](28-address-spaces.md) · [Contents](README.md) · [Next: The context switch →](30-context-switch.md)
