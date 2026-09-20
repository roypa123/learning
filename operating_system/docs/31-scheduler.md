# Chapter 31 — The scheduler

[← The context switch](30-context-switch.md) · [Contents](README.md) · [Next: Ring 3 →](32-usermode.md)

> 📖 **Line by line:** [sched.c](line-by-line/nimbus-sched.md)

---

## Goal

Decide who runs next, and make the decision happen without anyone's cooperation. Round robin with
four priority levels, a time slice, an idle task, and preemption from the timer interrupt.

---

## 1. Cooperative versus preemptive

There are two ways to share a CPU.

**Cooperative.** Each task calls `yield()` when it feels like it. Simple, and one bad task hangs the
machine forever. Windows 3.1 and classic Mac OS worked this way, and everyone remembers what that
was like.

**Preemptive.** A timer interrupt takes the CPU away whether the task likes it or not. Requires a
timer (Chapter 18) and a context switch that works from interrupt context (Chapter 30).

Nimbus is preemptive. The mechanism is three lines spread across two files, and §4 is about getting
them in the right order.

---

## 2. The policy

```c
#define SCHED_TIME_SLICE  5     /* ticks == 50 ms at 100 Hz */
#define PRIORITY_HIGH     0
#define PRIORITY_NORMAL   1
#define PRIORITY_LOW      2
#define PRIORITY_IDLE     3
```

Pick the highest-priority runnable task. Among equals, take the one *after* the current one, so
nobody starves within a level.

```c
static task_t *pick_next(void)
{
    task_t  *tasks = task_table();
    int      start = (int)(current_task - tasks);
    task_t  *best  = NULL;

    for (int i = 1; i <= MAX_TASKS; i++) {
        task_t *t = &tasks[(start + i) % MAX_TASKS];

        if (t->state != TASK_READY && t->state != TASK_RUNNING) continue;
        if (t->state == TASK_RUNNING && t != current_task)      continue;

        if (!best || t->priority < best->priority)
            best = t;
    }

    if (!best) panic("sched: nothing is runnable, not even idle");
    return best;
}
```

### 2.1 Starting after the current task

```c
    for (int i = 1; i <= MAX_TASKS; i++) {
        task_t *t = &tasks[(start + i) % MAX_TASKS];
```

`i` starts at 1, not 0 — so the scan begins at the slot *after* `current_task` and wraps around.

> Starting at zero every time would mean the lowest-numbered runnable task runs forever — a bug that
> looks like "the scheduler works" right up until you have two CPU-bound processes.

With three equal-priority tasks at slots 2, 3 and 4, starting from zero always finds task 2 first.
Task 2 runs, uses its slice, and is picked again. Tasks 3 and 4 never run.

The symptom is subtle because it only appears when more than one task is *continuously* runnable —
anything that sleeps or blocks masks it completely.

### 2.2 A 5-tick slice

50 ms at 100 Hz.

Too short and you pay switch costs for nothing (Chapter 30, §9: a process switch costs thousands of
cycles in TLB and cache misses). Too long and interactivity suffers — a keystroke waits behind a
CPU-bound task.

50 ms is at the edge of perceptible. Linux's default was 100 ms for years and is now dynamic.

### 2.3 The flaw, named up front

```c
 *  This is about as simple as a scheduler can be while still being a
 *  scheduler, and it has one honest flaw worth naming up front: a CPU-bound
 *  task at PRIORITY_NORMAL will starve a CPU-bound task at PRIORITY_LOW
 *  forever.
```

Strict priority means a lower-priority task runs only when nothing above it is runnable. A busy loop
at `PRIORITY_NORMAL` means `PRIORITY_LOW` never gets the CPU.

Two standard fixes:

**Ageing.** A task that has waited too long is temporarily promoted. About fifteen lines:

```c
    if (now - t->last_ran > STARVATION_TICKS && t->priority > 0)
        t->effective_priority = t->priority - 1;
```

**Virtual time.** Instead of a fixed slice, track how much CPU each task has *had*, scaled by its
weight, and always run whoever is furthest behind. That is Linux's CFS, and it makes priority a
matter of *rate* rather than *precedence* — which removes starvation entirely, by construction.

Exercise 31.6 implements ageing.

### 2.4 The scan is O(MAX_TASKS)

64 slots checked on every switch, at maybe 5 cycles each — a few hundred cycles, against a switch
that already costs thousands.

A real kernel keeps a list per priority level so picking is O(1):

> With 64 slots the scan costs a few hundred cycles and buys a data structure you can read without a
> diagram.

That is the trade, and it is the right one at this size. At 10,000 tasks it would not be.

---

## 3. The idle task

```c
    if (!best) panic("sched: nothing is runnable, not even idle");
```

This can only fire if something marked task 0 as not runnable, because the idle task is **never
blocked**.

Its whole purpose is to always be available:

```c
    for (;;) {
        sti();
        hlt();
    }
```

`PRIORITY_IDLE` is the lowest level, so it is chosen only when nothing else can run
(Chapter 29, §7.1).

Without an idle task, `pick_next` would have to return NULL and the scheduler would need a
"there is nothing to run" path — busy-waiting, or halting without a task context. Having a task that
is always runnable removes the special case entirely.

That pattern — **add a participant so the code needs no special case** — is the same idea as
`ISR_NOERR` pushing a fake error code (Chapter 16, §5.1) and as the manufactured stack for a new task
(Chapter 30, §5).

---

## 4. Preemption, and the ordering that matters

Three pieces, in three files.

### 4.1 The timer handler sets a flag

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

    if (current_task) {
        current_task->ticks_used++;

        if (current_task->time_slice > 0)
            current_task->time_slice--;

        if (current_task->time_slice == 0)
            timer_need_resched = true;
    }
}
```

Bookkeeping only. Wake expired sleepers, charge the running task a tick, and set a flag if its slice
is up.

### 4.2 The dispatcher acts on it

```c
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));

        if (timer_need_resched && sched_enabled) {
            timer_need_resched = false;
            schedule();
        }
    }
```

**After the EOI.** Chapter 17, §5.1:

> Switching first and acknowledging afterwards would mean the acknowledgement does not happen until
> this task is scheduled again, and in the meantime no device in the system can interrupt.

### 4.3 Why here and not in `timer_callback`

This is the design decision of the chapter, and the comment in `isr.c` states it:

> Switching here rather than inside `timer_callback()` is also what keeps the stack honest. We are on
> this task's kernel stack, holding a complete trap frame. `schedule()` saves that stack pointer in
> the task's context; when the task is picked again it returns from `schedule()` right here, walks
> back out through `isr_common_stub`, and irets to exactly the instruction that was interrupted.

Trace it through:

```
    task A running in userland
        timer interrupt
        -> isr_common_stub pushes a trap frame on A's KERNEL stack
        -> interrupt_dispatch
           -> timer_callback -> sched_tick -> sets the flag
           -> pic_send_eoi
           -> schedule()
              -> switch_context saves A's ESP, which points into that frame
              -> ... task B runs for a while ...
              -> eventually switch_context returns here, in A
        <- interrupt_dispatch returns
    <- isr_common_stub pops the frame, iret
    task A resumes in userland, at the exact instruction
```

The trap frame sits untouched on A's kernel stack for the entire time B runs. When A resumes, the
unwind proceeds normally.

Doing the switch inside `timer_callback` would work identically — it is the same stack — but doing it
at the *top* of the interrupt path, after the EOI and after all handlers have run, means there is
exactly one place in the kernel where preemption happens.

### 4.4 The flag, rather than switching directly

`sched_tick` could call `schedule()` itself. Setting a flag instead has two benefits:

**The EOI happens first** (§4.2).

**It composes.** Any handler can set `timer_need_resched` — a driver waking a high-priority task, for
instance — and the switch happens at the one defined point. Linux calls this `need_resched` and uses
it the same way.

---

## 5. `schedule()`

```c
void schedule(void)
{
    if (!sched_enabled) return;

    uint32_t flags = irq_save();

    task_t *prev = current_task;
    task_t *next = pick_next();

    if (next == prev) {
        prev->time_slice = SCHED_TIME_SLICE;
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        irq_restore(flags);
        return;
    }

    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    next->state      = TASK_RUNNING;
    next->time_slice = SCHED_TIME_SLICE;

    current_task = next;
    switches++;

    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);

    tss_set_kernel_stack(next->kernel_stack);

    switch_context(&prev->context, next->context);

    irq_restore(flags);
}
```

### 5.1 The self-switch case

```c
    if (next == prev) {
        prev->time_slice = SCHED_TIME_SLICE;
        ...
        return;
    }
```

If nothing better is runnable, refresh the slice and return.

Without the refresh, `time_slice` stays at 0 and `sched_tick` sets `need_resched` on the very next
tick — so a machine with one runnable task would call `schedule()` a hundred times a second to
decide, each time, to keep running the same task.

Harmless but wasteful, and the symptom is a mysteriously high switch count.

### 5.2 The state changes

```c
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
```

The `if` matters. `schedule()` is called from `sched_block()`, which has *already* set the state to
`BLOCKED`:

```c
void sched_block(void *channel)
{
    ...
    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;

    irq_restore(flags);
    schedule();
}
```

An unconditional `prev->state = TASK_READY` would undo that, and the task would be scheduled again
immediately despite waiting for something. That bug presents as "blocking does not work" and takes a
while to find, because everything else looks right.

### 5.3 Interrupts across the switch

Chapter 30, §6.3. The key sentence:

> Note that the restore happens *after* the switch returns — and the switch returns in a different
> task, whose saved flags are its own.

`flags` is a local on the task's own kernel stack. Each task restores its own.

---

## 6. Blocking and waking

```c
void sched_block(void *channel)
{
    if (!sched_enabled) {
        __asm__ volatile ("sti; hlt");
        return;
    }

    uint32_t flags = irq_save();

    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;

    irq_restore(flags);
    schedule();
}

void sched_wake(void *channel)
{
    uint32_t flags = irq_save();
    task_t  *tasks = task_table();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == channel) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }

    irq_restore(flags);
}
```

The sleep/wakeup interface from Unix Sixth Edition. Fifty years old, four lines, and nothing has
improved on it for a kernel this size.

### 6.1 Wait channels

> A "wait channel" is any address used as a token. Nothing is stored at it; it is compared for
> equality and that is all. Using the address of the thing you are waiting for — a semaphore, a
> device's buffer, a parent's task struct — makes the pairing between sleeper and waker impossible
> to get wrong, because both sides name the same object.

```c
    sched_block((void *)&kbd_buffer);       /* keyboard.c  */
    sched_wake((void *)&kbd_buffer);

    sched_block(&console_wait_channel);     /* console.c   */
    sched_block(sem);                       /* sync.c      */
    sched_block(p);                         /* pipe.c      */
    sched_block(self);                      /* task_wait   */
```

Six different channels, all obviously paired.

### 6.2 The fallback before the scheduler exists

```c
    if (!sched_enabled) {
        __asm__ volatile ("sti; hlt");
        return;
    }
```

Before `sched_init`, there is nobody to switch to. The honest thing is to wait for an interrupt.

`sti` then `hlt`, in that order, and the order is the whole point: `hlt` with interrupts disabled is
a permanent stop.

And it `return`s rather than looping, because the caller is always in a `while` loop re-checking its
condition (Chapter 19, §6.1).

### 6.3 `sched_wake` versus `sched_wake_one`

`sched_wake` wakes everyone on the channel. `sched_wake_one` wakes the first.

```c
void sem_post(semaphore_t *sem)
{
    ...
    sched_wake_one(sem);
}
```

> Waking one, not all. Waking everyone for a resource only one of them can have is the "thundering
> herd": n tasks are scheduled, n−1 find the count zero again and go back to sleep, and the wakeup
> cost is n times what it should be.

Use `wake_one` when exactly one waiter can proceed (a semaphore, a mutex). Use `wake` when the
condition is now true for everyone (a pipe has data, a line is ready).

Getting it backwards is not a correctness bug — the loop handles it — but it is a performance bug
that scales badly.

---

## 7. Sleeping

```c
void sleep_ms(uint32_t ms)
{
    if (!sched_enabled) { timer_spin_ms(ms); return; }

    uint32_t flags = irq_save();

    current_task->wake_tick = timer_ticks() + ((uint64_t)ms * TIMER_HZ) / 1000 + 1;
    current_task->state     = TASK_SLEEPING;

    irq_restore(flags);
    schedule();
}
```

A deadline in ticks, and `sched_tick` wakes it.

The `+ 1` is the same granularity fix as `timer_spin_ms` (Chapter 18, §7): without it,
`sleep_ms(10)` at 100 Hz might return in 0.1 ms if called 9.9 ms into a tick.

The difference from spinning is the whole point: **the CPU goes to someone else.**

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
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running    4821  idle
   1    0  sleeping      9  counter
```

Nine ticks used in forty-eight seconds of uptime. The rest went to idle, which means `hlt`, which
means the CPU drew almost no power.

With `timer_spin_ms` it would be 4,800 ticks and a hot laptop.

---

## 8. Running it

```c
    task_spawn_kernel("a", thread_a);
    task_spawn_kernel("b", thread_b);
    sched_init();
    sti();
```

with two CPU-bound threads that never yield:

```c
static void thread_a(void)
{
    for (uint32_t i = 0; ; i++)
        if (i % 20000000 == 0) kprintf("A");
}
```

```
ABABABABABABABABAB
```

Neither calls `yield`, neither sleeps, and they alternate. That is preemption — and it is the first
time in this book that something has happened against the running code's will.

### 8.1 Checking the slice

```c
    kprintf("%s got %u ticks\n", t->name, (uint32_t)t->ticks_used);
```

Over a second, at a 5-tick slice, two equal tasks should each get about 50 ticks. If one gets 100 and
the other 0, §2.1's scan-from-zero bug is present.

### 8.2 Checking priorities

Set one thread to `PRIORITY_HIGH` and the other to `PRIORITY_NORMAL`:

```
AAAAAAAAAAAAAAAAAAAAAAAA
```

Only A runs. That is strict priority working exactly as designed, and it is also §2.3's starvation
flaw made visible.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| One task runs forever | Scan starts at 0 (§2.1), or a priority inversion |
| Machine hangs at `sti` | No runnable task, or `sched_enabled` set too early |
| Panic "nothing is runnable" | The idle task got blocked |
| Blocking does not block | `prev->state` overwritten unconditionally (§5.2) |
| Very high switch count | Missing slice refresh in the self-switch case |
| Random deadlocks | Reschedule before EOI |
| Woken tasks proceed when they should not | Caller used `if` instead of `while` |
| Everything stops when one task sleeps | `sleep_ms` spinning instead of blocking |

---

## 10. Exercises

🟢 **31.1** Change the scan to start at 0 and run two CPU-bound threads. Confirm the starvation.

🟢 **31.2** Set `SCHED_TIME_SLICE` to 1 and to 100. Watch the interleaving change, and measure the
switch count in each case.

🟢 **31.3** Remove the slice refresh from the self-switch case and watch the switch counter.

🟡 **31.4** Add `last_ran` to the task struct and print, in `ps`, how long each task has been waiting.

🟡 **31.5** Make `sched_wake` into `sched_wake_one` for the console and observe what happens with two
readers.

🟡 **31.6** Implement priority ageing: promote any `READY` task that has waited more than 100 ticks,
and restore its priority when it runs. Confirm §8.2's starvation disappears.

🔴 **31.7** Replace the array scan with per-priority linked lists and make `pick_next` O(1). Keep
`task_dump_all` working.

🔴 **31.8** Implement a simplified CFS: give each task a `vruntime` incremented by
`slice / weight` each tick, and always run the task with the smallest. Compare fairness against round
robin with two tasks of different priorities.

---

## What we covered

- Cooperative versus preemptive, and the three lines across two files that make ours the latter.
- Round robin within priority levels, and the scan-from-after-current that prevents starvation within
  a level.
- A 50 ms slice, and what each direction costs.
- The strict-priority starvation flaw, named up front, with ageing and virtual time as the two
  standard fixes.
- The idle task as a participant that removes a special case, and the pattern that recurs.
- Preemption in three pieces: a flag in the tick, action after the EOI, and why the switch happens
  at the top of the interrupt path.
- The trap frame sitting untouched on a preempted task's kernel stack while another task runs.
- The self-switch refresh, and the conditional state change that makes blocking work.
- Wait channels as tokens compared for equality, and the six places they are used.
- `wake` versus `wake_one`, and the thundering herd.
- Sleeping as a deadline, and nine ticks of CPU across forty-eight seconds.

[Chapter 32](32-usermode.md) builds the wall: ring 3, the TSS in anger, and an `iret` into a program
that cannot touch the kernel.

---

[← The context switch](30-context-switch.md) · [Contents](README.md) · [Next: Ring 3 →](32-usermode.md)
