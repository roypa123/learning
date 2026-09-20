# Line by line: `nimbus/kernel/sched.c`

[Index](README.md) · [Chapter 31](../31-scheduler.md) · [Chapter 35](../35-blocking.md)

---

## `sched_add` / `sched_remove`

```c
void sched_add(task_t *t)
{
    if (t->state == TASK_READY || t->state == TASK_RUNNING) return;
    t->state = TASK_READY;
}
```
⚠️ Idempotent. Adding a task that is already ready is a no-op rather than a corrupted list — because
every wakeup path in the kernel would otherwise need to check first.

Our "run queue" is the process table scanned in order. A real kernel keeps a list per priority so
picking is O(1); with 64 slots the scan costs a few hundred cycles and buys a data structure you can
read without a diagram.

---

## `pick_next`

```c
    int      start = (int)(current_task - tasks);
    task_t  *best  = NULL;

    for (int i = 1; i <= MAX_TASKS; i++) {
        task_t *t = &tasks[(start + i) % MAX_TASKS];
```
⚠️ **`i` starts at 1**, so the scan begins at the slot *after* `current_task`.

> Starting at zero every time would mean the lowest-numbered runnable task runs forever — a bug that
> looks like "the scheduler works" right up until you have two CPU-bound processes.

The symptom is subtle because it only appears when more than one task is *continuously* runnable.
Anything that sleeps or blocks masks it completely.

```c
        if (t->state != TASK_READY && t->state != TASK_RUNNING) continue;
        if (t->state == TASK_RUNNING && t != current_task)      continue;
```
The second line is defensive: only one task should be `RUNNING`, and if two somehow are, prefer not
to switch to the other.

```c
        if (!best || t->priority < best->priority)
            best = t;
```
Lower number = higher priority. Strict priority.

⚠️ **The honest flaw**, named in the source:

> a CPU-bound task at PRIORITY_NORMAL will starve a CPU-bound task at PRIORITY_LOW forever.

Fixes: ageing (promote a task that has waited too long — fifteen lines) or virtual time (CFS: always
run whoever is furthest behind, which removes starvation by construction).

```c
    if (!best) panic("sched: nothing is runnable, not even idle");
```
Can only fire if something marked task 0 as not runnable. The idle task is never blocked — its whole
purpose is to always be available, which removes the "nothing to run" special case entirely.

---

## `schedule`

```c
    uint32_t flags = irq_save();
```
⚠️ The window between "choose the next task" and "switch to it" must be atomic. An interrupt in the
middle could block the task we just chose.

```c
    if (next == prev) {
        prev->time_slice = SCHED_TIME_SLICE;
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        irq_restore(flags);
        return;
    }
```
⚠️ **The refresh matters.** Without it `time_slice` stays at 0 and `sched_tick` sets `need_resched`
on the very next tick — so a machine with one runnable task calls `schedule()` a hundred times a
second to decide, each time, to keep running the same task.

Harmless but wasteful, and the symptom is a mysteriously high switch count.

```c
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
```
⚠️ **The `if` is load-bearing.**

`schedule()` is called from `sched_block()`, which has *already* set the state to `BLOCKED`. An
unconditional assignment would undo that, and the task would be scheduled again immediately despite
waiting for something.

That bug presents as "blocking does not work" and takes a while to find, because everything else
looks right.

```c
    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);
```
⚠️ **`CR3` first**, and safe only because the kernel half of every directory is identical — so the
code we are executing, the stack we are on and the task structs we are touching are all still mapped.

The `!=` check avoids a full TLB flush when the address space is unchanged, which is the case for two
kernel threads or a task resuming after an interrupt that did not switch spaces.

```c
    tss_set_kernel_stack(next->kernel_stack);
```
⚠️ Or the next ring 3 trap from this task lands on the previous task's kernel stack, over its saved
registers while it is blocked mid-syscall.

```c
    switch_context(&prev->context, next->context);

    irq_restore(flags);
```
⚠️ Execution resumes here when *prev* is scheduled again — possibly seconds later, possibly in a
different address space.

`flags` is a local on *this* task's stack. Each task restores its own interrupt state, which is how
`schedule()` can be called both from ordinary code (interrupts on) and from the tail of an interrupt
handler (off).

---

## `sched_tick`

```c
void sched_tick(void)
{
    if (!sched_enabled) return;

    uint64_t now = timer_ticks();
    task_t  *tasks = task_table();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }
```
⚠️ **`SLEEPING` is a different state from `BLOCKED`**, woken by a different mechanism: a deadline
rather than an event.

Merging them would mean the tick checking every blocked task's deadline, most of which do not have
one.

The scan is 64 slots, 100 times a second — a few hundred cycles per tick. A real kernel uses a sorted
list or a timer wheel so only the head is checked.

```c
    if (current_task) {
        current_task->ticks_used++;

        if (current_task->time_slice > 0)
            current_task->time_slice--;

        if (current_task->time_slice == 0)
            timer_need_resched = true;
    }
```
⚠️ **Bookkeeping only. It does not switch.**

The switch happens in `interrupt_dispatch`, after the EOI, so that the PIC is free to deliver the
next interrupt no matter which task we switch to.

Setting a flag also *composes*: any handler can set `timer_need_resched` — a driver waking a
high-priority task, say — and the switch happens at the one defined point. Linux calls this
`need_resched`.

---

## `sched_block`

```c
    if (!sched_enabled) {
        __asm__ volatile ("sti; hlt");
        return;
    }
```
Before `sched_init` there is nobody to switch to, so wait for an interrupt.

⚠️ `sti` then `hlt`, in that order. `hlt` with interrupts disabled is a permanent stop.

And it `return`s rather than looping, because the caller is always in a `while` re-checking its
condition.

```c
    uint32_t flags = irq_save();

    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;

    irq_restore(flags);
    schedule();
```
⚠️ The lost-wakeup window is closed not because interrupts are off during the sleep, but because
**the act of sleeping is what re-enables them**: `schedule()` runs `irq_save()` immediately, switches,
and the *next* task restores *its* flags.

So from "I have decided to sleep" to "I am no longer running" there is no point at which an interrupt
can find us awake-but-about-to-sleep.

---

## `sched_wake` / `sched_wake_one`

```c
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == channel) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }
```
A channel is an **address used as a token**. Nothing is stored at it; it is compared for equality.

> Using the address of the thing you are waiting for — a semaphore, a device's buffer, a parent's
> task struct — makes the pairing between sleeper and waker impossible to get wrong, because both
> sides name the same object.

The six channels in Nimbus: `&kbd_buffer`, `&console_wait_channel`, a `semaphore_t *`, a `pipe_t *`,
and a `task_t *` for `wait`.

⚠️ **`wake` versus `wake_one`.** Use `wake_one` when exactly one waiter can proceed (a semaphore);
`wake` when the condition is true for everyone (a pipe has data).

Getting it backwards is a performance bug that scales with the number of waiters — the thundering
herd — not a correctness bug, because every waiter re-checks in a loop.

⚠️ Safe from interrupt context. It only moves tasks onto the run queue. *Switching* from there would
not be.

---

## `sleep_ms`

```c
    if (!sched_enabled) { timer_spin_ms(ms); return; }
```

```c
    current_task->wake_tick = timer_ticks() + ((uint64_t)ms * TIMER_HZ) / 1000 + 1;
    current_task->state     = TASK_SLEEPING;
```
⚠️ The `+ 1`. Without it, `sleep_ms(10)` called 9.9 ms into a tick returns in 0.1 ms.

With it, a 10 ms sleep takes 10–20 ms. Never too short, sometimes too long — the right direction to
err.

```c
    irq_restore(flags);
    schedule();
```
The difference from spinning is the whole point: the CPU goes to someone else.

```
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running    4821  idle
   1    0  sleeping      9  counter
```

Nine ticks used across forty-eight seconds of uptime. With `timer_spin_ms` it would be 4,800 and a
hot laptop.

---

[Index](README.md) · [Chapter 31](../31-scheduler.md) · [Chapter 35](../35-blocking.md)
