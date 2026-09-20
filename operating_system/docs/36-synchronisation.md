# Chapter 36 — Synchronisation

[← Blocking](35-blocking.md) · [Contents](README.md) · [Next: The ATA driver →](37-ata-driver.md)

---

## Goal

Find the races we already have, and build the three tools that fix them. This finishes Part IV.

Nimbus runs on one CPU, which removes half of the concurrency problem and leaves the other half
firmly in place.

---

## 1. What concurrency we actually have

> Two tasks never execute at the same instant — but a task can be preempted between any two
> instructions, and an interrupt handler can run between any two instructions, including in the
> middle of a read-modify-write on a variable the handler also touches.

Three sources, and they are not the same:

**Interrupt handlers.** An IRQ can arrive between any two instructions whenever `IF` is set. The
handler runs on the current task's kernel stack and may touch anything.

**Preemption.** Since Chapter 31, the timer can switch tasks at the end of any interrupt.

**Voluntary blocking.** Any call to `sched_block`, `sleep_ms`, or anything that calls them, is a
point where another task runs. `kmalloc` does not block; `vfs_read` on a pipe does.

The first two arrived at different moments. The third arrived when Chapter 33 ran:

```c
    sti();

    int32_t result = syscall_table[number](regs);
```

> This is also the moment the kernel becomes re-entrant: from here on another task can be scheduled
> in the middle of this call, and every data structure we touch needs to survive that.

Before that line, the kernel was effectively single-threaded.

---

## 2. Races we already have

Worth finding them explicitly, because reading about races in the abstract teaches much less than
finding one in code you wrote.

### 2.1 The PIC mask

```c
void irq_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(1 << (irq & 7));

    outb(port, (uint8_t)(inb(port) | bit));
}
```

Read, modify, write. If an interrupt lands between the read and the write, and its handler also
touches the mask, one update is lost.

```
    task:       read mask -> 0xFB
    interrupt:              read 0xFB, write 0xFA (unmask line 0)
    task:       write 0xFB | 0x04 = 0xFF        <- line 0 is masked again
```

The source flags it:

> On a uniprocessor with interrupts already disabled inside a handler this is safe. Called from task
> context it is not, which is why callers are expected to be in early boot or holding interrupts off.
> Chapter 36 revisits it.

**The fix** is `irq_save`/`irq_restore` around the read-modify-write. Exercise 17.6.

### 2.2 The 64-bit tick counter

```c
uint64_t timer_ticks(void)
{
    uint64_t t;
    do {
        t = ticks;
    } while (t != ticks);
    return t;
}
```

Already fixed, and worth revisiting as an example of a *lock-free* solution.

Chapter 18, §6: a 64-bit read on a 32-bit machine is two loads, and the timer interrupt can land
between them. Reading twice and retrying is the sequence-lock pattern in miniature.

No lock, no interrupt disabling, two extra instructions. When a lock-free answer exists it is usually
better than a lock — but they are rare, and the ones that exist are subtle.

### 2.3 The heap

```c
void *kmalloc(size_t size)
{
    ...
    uint32_t flags = irq_save();
    ...
    irq_restore(flags);
```

Already protected, because `kmalloc` is called from interrupt context — the page fault handler
allocates.

Without it, an interrupt in the middle of splitting a block leaves the free list in a state where
`b->next` has been set but `rest->next` has not, and the handler's own `kmalloc` walks into a
half-built node.

### 2.4 The file descriptor table

```c
int fd_alloc(file_t *f)
{
    uint32_t flags = irq_save();

    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (!current_task->fds[fd]) {
            current_task->fds[fd] = f;
            irq_restore(flags);
            return fd;
        }
    }
    ...
}
```

Protected, and it is worth asking *why*, because `current_task->fds` is per-process and a process is
single-threaded in Nimbus.

The answer: preemption between finding the free slot and claiming it. Another task cannot touch *this*
table — but if threads are ever added (Exercise 29.8), two threads sharing a table would both find
slot 3 free.

Protecting it now costs nothing and is correct in advance.

### 2.5 The one that is not protected

```c
static task_t *task_alloc(void)
{
    uint32_t flags = irq_save();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            memset(&tasks[i], 0, sizeof(task_t));
            tasks[i].state = TASK_EMBRYO;
            tasks[i].pid   = next_pid++;
```

This *is* protected. But look at the pattern it demonstrates — the `TASK_EMBRYO` state exists
precisely because the protection cannot extend across the whole creation:

> The slot is claimed but not yet runnable, so a concurrent `task_alloc` cannot take it while the
> caller is still filling it in.

The lock covers the claim; the state covers the gap between claim and completion. That two-part
pattern is everywhere in kernels.

---

## 3. Tool one: disabling interrupts

```c
static ALWAYS_INLINE uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static ALWAYS_INLINE void irq_restore(uint32_t flags)
{
    __asm__ volatile ("pushl %0; popfl" :: "r"(flags) : "memory", "cc");
}
```

**The only thing that works against an interrupt handler.** A lock cannot help: the handler runs on
this CPU, in the middle of this code, and if it tries to take a lock we hold it spins forever on a
CPU that will never release it.

### 3.1 Save and restore, never `cli`/`sti`

```c
 *    irq_save/restore   the only thing that works against an interrupt
 *                       handler. Cheap, and it stops *everything*, so it must
 *                       be held for a handful of instructions at most.
```

The reason for save/restore rather than blunt `cli`/`sti`:

```c
void spin_unlock(spinlock_t *lock)
{
    ...
    irq_restore(flags);
}
```

> Restore, do not blindly `sti`. If the caller already had interrupts disabled for its own reasons,
> turning them on here would break an invariant it is relying on — and the resulting bug would appear
> in the caller's caller, with nothing to connect it to this line.

Nesting works automatically: the inner section restores "disabled" because that is what it found.

A depth counter would also work and is what some kernels use. Saving the flags is simpler and cannot
get out of step.

### 3.2 Keep it short

Interrupts off means: no timer, no keyboard, no disk completion, no preemption. The whole machine is
frozen.

A section held for a millisecond loses ten timer ticks and drops keystrokes. The rule is a handful of
instructions — a pointer update, a counter increment, a small `memcpy`:

```c
    uint32_t flags = irq_save();

    size_t n = line_len;
    if (n > max - 1) n = max - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';

    line_len   = 0;
    line_ready = false;

    irq_restore(flags);
```

At most 256 bytes. That is the right shape.

**Never across anything that can block.** Blocking with interrupts disabled means the *next* task
runs with them disabled — except it does not, because `schedule()` restores the new task's flags
(Chapter 30, §6.3). The real problem is that the blocked task's saved flags say "disabled", so when
it resumes it has interrupts off in a context where the caller expects them on.

---

## 4. Tool two: spinlocks

```c
typedef struct spinlock {
    volatile uint32_t locked;
    uint32_t          saved_flags;
    const char       *name;
    void             *holder;
} spinlock_t;
```

On one CPU, **a spinlock that actually spins is a deadlock**:

> the holder cannot release it while we are spinning, because we are the only CPU and we are not
> running the holder.

So `spin_lock` does not spin. It disables interrupts and records that it holds the lock:

```c
void spin_lock(spinlock_t *lock)
{
    uint32_t flags = irq_save();

    if (lock->locked)
        panic("spin_lock(%s): already held by %p -- recursive acquisition",
              lock->name ? lock->name : "?", lock->holder);

    lock->locked      = 1;
    lock->holder      = current_task;
    lock->saved_flags = flags;
}
```

### 4.1 Why keep the API at all

> Keeping the spinlock API anyway is not ceremony. It marks the places in the kernel that would need
> a real lock the day a second CPU appears, it gives those places a name that shows up in a panic,
> and it means the change to support SMP is confined to this file.

That third point is the practical one. The SMP version is:

```c
void spin_lock(spinlock_t *lock)
{
    uint32_t flags = irq_save();
    while (atomic_xchg(&lock->locked, 1) != 0)
        __asm__ volatile ("pause");
    lock->holder      = current_task;
    lock->saved_flags = flags;
}
```

Three lines different, in one file, and the rest of the kernel is unchanged.

### 4.2 The deadlock detector

```c
    if (lock->locked)
        panic("spin_lock(%s): already held by %p -- recursive acquisition", ...);
```

> With interrupts off on a uniprocessor, finding the lock already held means *this* CPU took it and
> did not release it — a genuine deadlock that would spin forever. Detecting it is free and the
> diagnostic is the difference between "the machine froze" and a file and line.

On SMP this check would be wrong — another CPU legitimately holds it — and would become "am I the
holder?" instead.

### 4.3 The atomics we have anyway

```c
static ALWAYS_INLINE uint32_t atomic_xchg(volatile uint32_t *ptr, uint32_t value)
{
    __asm__ volatile ("lock xchgl %0, %1"
                      : "+r"(value), "+m"(*ptr)
                      :: "memory");
    return value;
}
```

> `lock xchg` is atomic against interrupts and against other cores. On a uniprocessor the LOCK prefix
> is redundant, but it costs one cycle and it makes the intent explicit, so we keep it.

`xchg` is special: it is the one instruction that asserts the bus lock **without** the `lock` prefix.
Writing it anyway documents what is happening.

`atomic_inc` and `atomic_dec_and_test` are there for reference counts, which is the other place
atomicity matters:

```c
static ALWAYS_INLINE bool atomic_dec_and_test(volatile uint32_t *ptr)
{
    bool zero;
    __asm__ volatile ("lock decl %0; sete %1"
                      : "+m"(*ptr), "=q"(zero) :: "memory", "cc");
    return zero;
}
```

Decrement and report whether it hit zero, atomically. Without it, two concurrent `file_unref` calls
can both see 1, both decrement to 0, and both free.

---

## 5. Tool three: sleeping locks

For code that **can** sleep. A task that cannot get the lock blocks, and the CPU goes to someone
else.

```c
void sem_wait(semaphore_t *sem)
{
    for (;;) {
        uint32_t flags = irq_save();

        if (sem->count > 0) {
            sem->count--;
            irq_restore(flags);
            return;
        }

        irq_restore(flags);
        sched_block(sem);
    }
}

void sem_post(semaphore_t *sem)
{
    uint32_t flags = irq_save();
    sem->count++;
    irq_restore(flags);

    sched_wake_one(sem);
}
```

Chapter 35, §3.1 covered the loop. Note the structure: a short interrupt-disabled section for the
counter, and the *blocking* happens outside it.

### 5.1 Mutexes, and the owner check

```c
void mutex_lock(mutex_t *m)
{
    if (m->owner && m->owner == (void *)current_task)
        panic("mutex_lock(%s): already held by this task (pid %d)", ...);

    sem_wait(&m->sem);
    m->owner = (void *)current_task;
}

void mutex_unlock(mutex_t *m)
{
    if (m->owner != (void *)current_task)
        panic("mutex_unlock(%s): released by a task that does not hold it", ...);

    m->owner = NULL;
    sem_post(&m->sem);
}
```

A semaphore with a count of 1, plus two checks.

> The owner check is what turns "the system froze" into "task 4 tried to take a mutex it already
> holds, at fat16.c:212".

**Not recursive**, deliberately:

> A mutex is not recursive: taking it twice from the same task blocks forever waiting for yourself.
> Some systems offer a recursive mutex; they are almost always a sign that the locking design is
> unclear, and they hide exactly this bug rather than fixing it.

The release check catches the other classic error: unlocking a mutex you do not hold, which happens
when an error path unlocks unconditionally.

### 5.2 Which to use

| | Interrupt context | Can block | Held for |
|---|---|---|---|
| `irq_save`/`irq_restore` | yes | no | a handful of instructions |
| `spin_lock` | yes | no | microseconds |
| `mutex_lock` | **no** | yes | anything |

**A mutex can never be taken from an interrupt handler**, because there is no task to put to sleep.
That is the hard rule, and violating it means `sched_block` with `current_task` pointing at whoever
happened to be running.

---

## 6. The disciplines that matter more than the tools

### 6.1 Name what each lock protects

Not "the FAT16 lock" — "the lock protecting `fs->fat` and `fs->fat_dirty`".

A lock without a stated invariant becomes a lock that is taken in some paths and not others, and
nobody can tell which is the bug.

### 6.2 Lock ordering

If two locks are ever held at once, fix a global order and always acquire in it. Chapter 35, §6.2: a
cycle cannot form if every acquisition increases the number.

The usual encoding is a comment at the top of the file listing the order, and — in kernels that care
— a debug mode that asserts it.

### 6.3 Do not call out while holding a lock

Calling an unknown function with a lock held means you do not know what it will take. That is how
ordering violations appear in code that looks locally correct.

The pattern is: take the lock, extract what you need, release, then call.

### 6.4 Prefer immutability and ownership

The cheapest lock is the one you do not need.

Nimbus has several examples: the keymaps are `const`, the syscall table is written once at build
time, a `task_t`'s `pid` never changes after creation, and a page directory belongs to exactly one
task.

Each of those is data that needs no protection because nothing writes it concurrently.

---

## 7. What SMP would change

Worth knowing what is uniprocessor-specific in this chapter, because almost all of it is.

| | Uniprocessor | SMP |
|---|---|---|
| `irq_save` | stops all concurrency on this CPU | stops only *this* CPU |
| `spin_lock` | records a flag | genuinely spins on `xchg` |
| `lock` prefix | redundant | mandatory |
| A read-modify-write on a global | safe with interrupts off | needs a lock or an atomic |
| Memory ordering | irrelevant | `mfence`, `lfence`, acquire/release semantics |
| TLB invalidation | `invlpg` | plus a shootdown IPI to every CPU |
| Per-CPU data | does not exist | `GS`-relative, which is why `isr.asm` saving only `DS` would break |

That last row connects back to Chapter 16, §6.2:

> It would not be safe in a kernel that used FS or GS for per-CPU data, which is exactly what an SMP
> kernel does.

The honest summary: **making this kernel SMP-safe is not a matter of adding locks.** It is a matter
of re-examining every global, every read-modify-write, and every assumption that "interrupts off"
means "alone". Chapter 48.

---

## 8. Part IV, in retrospect

Eight chapters, and the kernel can now:

| | |
|---|---|
| Represent a process | Four things, and everything else is emergent (29) |
| Switch between them | Fourteen instructions (30) |
| Decide who runs | Round robin, priorities, preemption from IRQ 0 (31) |
| Enforce a boundary | Ring 3, the TSS, `iret` into userland (32) |
| Accept requests across it | One gate, and never trusting a pointer (33) |
| Create and replace programs | `fork` returning twice, `exec` rewriting a trap frame (34) |
| Wait for free | Channels, and three rules about wakeups (35) |
| Not corrupt itself | Three tools, and the disciplines that matter more (36) |

There is a shell running in ring 3, in its own address space, that can start other programs.

What it cannot do is read a file from a disk — everything it runs comes from a tar archive in RAM.
Part V fixes that.

---

## 9. Exercises

🟢 **36.1** Fix the `irq_mask` race from §2.1 with `irq_save`/`irq_restore`.

🟢 **36.2** Take a mutex twice from the same task and read the panic.

🟢 **36.3** Unlock a mutex from a task that does not hold it and read that panic.

🟡 **36.4** Write a race: two kernel threads incrementing a shared counter a million times each
without protection. Run it, report the final value, then add a spinlock and re-run.

🟡 **36.5** Find one more unprotected read-modify-write in the kernel that this chapter did not
name, and argue about whether it matters.

🟡 **36.6** Add lock ordering: give every `spinlock_t` and `mutex_t` a level, track the highest held
per task, and assert on any acquisition of a lower one.

🔴 **36.7** Implement a reader-writer lock: many concurrent readers or one writer. Use it for the
VFS's mount table. Then work out what happens when a writer waits behind an endless stream of
readers, and implement the fix.

🔴 **36.8** Convert `spin_lock` to the real SMP version from §4.1, and audit five globals in the
kernel for whether they would still be safe. Write down what each would need.

---

## What we covered

- Three sources of concurrency, and the exact line in Chapter 33 at which the third appeared.
- Five real races in code already written, one of which is deliberately left unfixed with a comment.
- The `TASK_EMBRYO` pattern: a lock for the claim, a state for the gap.
- `irq_save`/`irq_restore` as the only thing that works against a handler, why save-and-restore
  rather than `cli`/`sti`, and why sections must be short.
- Spinlocks that do not spin, why the API is worth keeping anyway, and the three-line diff to SMP.
- A deadlock detector that is free on a uniprocessor and wrong on a multiprocessor.
- `lock xchg`, `atomic_dec_and_test`, and the double-free two refcount decrements can cause.
- Sleeping locks, the non-recursive mutex with an owner check, and the hard rule about interrupt
  context.
- Four disciplines that matter more than the tools, ending with the cheapest lock being the one you
  do not need.
- What SMP would change, and why it is not a matter of adding locks.

**Part IV is finished.** [Chapter 37](37-ata-driver.md) starts Part V by reading a sector off a real
disk.

---

[← Blocking](35-blocking.md) · [Contents](README.md) · [Next: The ATA driver →](37-ata-driver.md)
