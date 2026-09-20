# Chapter 35 — Blocking and wait queues

[← fork and exec](34-fork-exec.md) · [Contents](README.md) · [Next: Synchronisation →](36-synchronisation.md)

---

## Goal

Make waiting free. A task that has nothing to do should consume no CPU, and the CPU it gives up
should go to someone who does.

The mechanism is four lines old enough to have been written in 1975, and the subtleties around it are
what this chapter is actually about.

---

## 1. Three ways to wait, two of them wrong

### 1.1 Spinning

```c
    while (kbd_head == kbd_tail)
        ;
    return kbd_buffer[kbd_tail];
```

Burns 100% of a CPU doing nothing. On a laptop the fan spins up; on a battery it halves the runtime;
on a server it costs a core.

Worse, on a **uniprocessor** it can deadlock: if the thing you are waiting for is produced by another
*task* rather than an interrupt, spinning means that task never gets the CPU to produce it.

### 1.2 Polling with a sleep

```c
    while (kbd_head == kbd_tail)
        sleep_ms(10);
```

Better — the CPU is free between checks — and it has two problems:

**Latency.** Up to 10 ms between the event and noticing it, on average 5 ms.

**Wasted wakeups.** At 100 polls per second with nothing happening, that is 100 context switches a
second per waiter, for nothing.

Halving the interval halves the latency and doubles the waste. There is no setting that is good.

### 1.3 Blocking

```c
    for (;;) {
        int key = keyboard_getkey_nonblock();
        if (key >= 0) return key;

        sched_block((void *)&kbd_buffer);
    }
```

Zero CPU while waiting. Zero latency — the waker runs `sched_wake` in the same interrupt that
produced the data. Zero wasted wakeups.

The cost is that somebody has to remember to call `sched_wake`, and a forgotten wakeup is a task that
sleeps forever.

---

## 2. The interface

```c
void  sched_block(void *channel);
void  sched_wake(void *channel);
void  sched_wake_one(void *channel);
void  sleep_ms(uint32_t ms);
```

Four functions. The sleep/wakeup interface from Unix Sixth Edition:

> It is fifty years old, it is four lines, and nothing has improved on it for a kernel this size.

### 2.1 A channel is an address

```c
void sched_block(void *channel)
{
    ...
    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;
    ...
}

void sched_wake(void *channel)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == channel) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }
}
```

Nothing is stored at the address. It is compared for equality and that is all.

> Using the address of the thing you are waiting for — a semaphore, a device's buffer, a parent's
> task struct — makes the pairing between sleeper and waker impossible to get wrong, because both
> sides name the same object.

The six channels in Nimbus:

| Channel | Waiter | Waker |
|---|---|---|
| `&kbd_buffer` | `keyboard_getkey` | the keyboard IRQ |
| `&console_wait_channel` | `console_read_line` | `console_input`, on Enter |
| `sem` | `sem_wait` | `sem_post` |
| `p` (a `pipe_t *`) | `pipe_read`, `pipe_write` | the other end |
| `self` (a `task_t *`) | `task_wait` | a child's `task_exit` |

### 2.2 Why not a queue per object?

A real kernel gives each waitable object its own list:

```c
struct wait_queue {
    task_t *head;
};
```

`block` appends; `wake` walks that list only.

Ours scans all 64 task slots. That is O(MAX_TASKS) per wake against O(waiters), and with 64 slots it
is a few hundred cycles — against a context switch that already costs thousands.

The payoff is that there is no list to corrupt, no `next` pointer to get wrong, and no object
lifetime problem when a waited-on object is freed while someone is on its queue. Exercise 35.6.

---

## 3. The three rules

Everything that goes wrong with blocking is a violation of one of these.

### 3.1 Always loop

```c
    for (;;) {
        int key = keyboard_getkey_nonblock();
        if (key >= 0) return key;

        sched_block((void *)&kbd_buffer);
    }
```

**`while`, never `if`.**

`sched_wake` makes every blocked task on the channel runnable. Between being made runnable and
actually running, another task may have taken the thing you were woken for.

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
```

The source is blunt about it:

> When sem_post wakes us, the count is positive — but by the time we are scheduled, another task may
> have taken it. So the woken task must re-check rather than assume, which is why this is
> `while (count == 0)` and never `if (count == 0)`. That distinction is the single most common
> concurrency bug in kernel code, and it is the reason condition variables are always documented with
> "always wait in a loop".

It is also why `sched_block` returns rather than looping internally: the *condition* is the caller's
business, and only the caller can re-check it.

### 3.2 Close the lost-wakeup window

```c
    for (;;) {
        uint32_t flags = irq_save();
        if (line_ready) { irq_restore(flags); break; }
        irq_restore(flags);

        sched_block(&console_wait_channel);
    }
```

The race:

```
    task:       test line_ready        -> false
    interrupt:                            set line_ready, call sched_wake
    task:       sched_block(...)       -> sleeps forever
```

The wakeup arrived *between* the test and the sleep. Nobody will deliver it again.

Disabling interrupts across the test-and-sleep closes it — **as long as the act of sleeping is what
re-enables them.**

That is the precise formulation and it is worth being careful about it. `sched_block` does:

```c
    uint32_t flags = irq_save();
    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;
    irq_restore(flags);
    schedule();
```

and `schedule()` runs `irq_save()` again immediately, switches, and the *next* task restores *its*
flags (Chapter 30, §6.3).

So interrupts are off from before the state change until after the switch. The window between "I have
decided to sleep" and "I am no longer running" contains no point at which an interrupt can find us
awake-but-about-to-sleep.

> the window really is closed rather than merely narrowed.

### 3.3 Wake exactly the right number

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

**`wake_one`** when exactly one waiter can proceed: a semaphore, a mutex.

**`wake`** when the condition is now true for everyone: a pipe has data, a line is ready, a child
exited.

Getting it backwards is not a correctness bug — rule 3.1 handles it — but it is a performance bug
that scales with the number of waiters.

---

## 4. Sleeping on a deadline

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

A different state, woken by a different mechanism:

```c
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }
```

Chapter 29, §3: `BLOCKED` waits for an event, `SLEEPING` waits for a deadline. Merging them would
mean the tick checking every blocked task's deadline, most of which do not have one.

### 4.1 The scan cost

`sched_tick` walks all 64 slots every tick — 100 times a second.

A real kernel keeps sleepers in a **sorted list** or a **timer wheel**, so the tick checks only the
head. With 64 slots at a few cycles each, ours costs perhaps 300 cycles per tick, which is 0.003% of
a 100 MHz CPU. Exercise 18.6 builds the sorted version.

### 4.2 Granularity

The `+ 1` is the same fix as `timer_spin_ms` (Chapter 18, §7): without it, `sleep_ms(10)` called
9.9 ms into a tick returns in 0.1 ms.

With it, a 10 ms sleep takes 10–20 ms. Never too short, sometimes too long, which is the right
direction to err.

Sub-tick sleeps are impossible without a higher-resolution timer. Exercise 18.8.

---

## 5. What this buys, measured

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

Nine ticks used across forty-eight seconds of uptime — 0.2% of the CPU.

The other 4,821 ticks went to the idle task, which means `hlt`, which means the processor drew almost
no power.

With `timer_spin_ms` it would be 4,800 ticks, a hot laptop, and no CPU for anything else.

### 5.1 Blocking on I/O

```
nimbus> ps
 PID PPID  STATE     TICKS  NAME
   0    0  running   12043  idle
   1    0  blocked      31  /bin/sh
```

The shell is blocked in `console_read_line`, waiting for a keypress. It has used 31 ticks since boot,
all of them running commands.

A polling shell would show thousands.

---

## 6. Deadlock, and the two shapes it takes

### 6.1 The missing wakeup

Task A blocks on channel X. Nothing ever calls `sched_wake(X)`. A sleeps forever.

The symptom is a process stuck in `BLOCKED` in `ps`, and it is usually a path through the waker that
returns early:

```c
static int pipe_close(vfs_node_t *node)
{
    ...
    sched_wake(p);

    if (dead) kfree(p);
    return 0;
}
```

> Wake the other end so it can notice that we are gone. Without this, a reader blocked on an empty
> pipe sleeps forever after its writer exits: nothing would ever run `sched_wake`, because the
> writer's last act was to close.

That `sched_wake` is unconditional, deliberately — even when the pipe is not dead, because a close of
one of several writers may still change what a reader should do.

### 6.2 The cycle

A holds resource 1 and waits for resource 2. B holds 2 and waits for 1.

Neither is spinning; both are correctly blocked; the machine is idle; nothing progresses.

The standard prevention is **lock ordering**: number every lock, and require that they be acquired in
increasing order. A cycle then cannot form, because it would require someone to acquire a lower
number while holding a higher one.

Nimbus has few enough locks that this has not come up. It would the moment two filesystems could be
mounted on each other.

### 6.3 Finding them

`ps` showing `BLOCKED` is the first signal. Adding the channel to the output makes it diagnostic:

```c
    kprintf("%4d %4d  %-9s %5u  %p  %s\n",
            t->pid, t->ppid, state_name(t->state),
            (uint32_t)t->ticks_used, t->wait_channel, t->name);
```

```
 PID PPID  STATE     TICKS  CHANNEL     NAME
   4    1  blocked      12  0xd0004018  cat
   5    1  blocked       9  0xd0004018  grep
```

Two tasks on the same channel, which is a pipe, and neither is progressing — so the question becomes
"who should have called `sched_wake` on `0xd0004018`?"

Exercise 35.5.

---

## 7. Interrupt context, once more

```c
    kbd_push((int)(unsigned char)c);
    console_input((int)(unsigned char)c);
    sched_wake((void *)&kbd_buffer);
```

All three called from `keyboard_callback`, which is an interrupt handler.

**Waking is safe from an interrupt.** It changes a state field and returns.

**Switching is not.** `schedule()` from inside a handler would switch away with a half-processed
interrupt, and the EOI would not be sent until the task was scheduled again (Chapter 17, §5.1).

> The distinction is worth internalising: **waking is safe from an interrupt, switching is not.**

And the corollary: `sched_block` must never be called from interrupt context, because there is no
task to put to sleep. Nothing in Nimbus does, and a `ASSERT(!in_interrupt)` would be a reasonable
addition (Exercise 35.7).

---

## 8. Exercises

🟢 **35.1** Change `keyboard_getkey`'s `for(;;)` to an `if`. Type quickly with two readers and watch
it return garbage.

🟢 **35.2** Add the wait channel to `ps` as in §6.3.

🟢 **35.3** Replace `sleep_ms` in `counter_thread` with `timer_spin_ms` and compare the tick counts
in `ps`.

🟡 **35.4** Remove the `irq_save` around the test in `console_read_line`, and widen the window with a
`timer_spin_ms(1)` between the test and the `sched_block`. Confirm the hang.

🟡 **35.5** Write a deliberate deadlock: two kernel threads, two semaphores, acquired in opposite
orders. Find it with `ps`, then fix it with lock ordering.

🟡 **35.6** Give each waitable object its own wait queue: a `task_t *` head in the object, and
`next`-linked waiters. Then work out what happens if the object is freed while someone is queued.

🟡 **35.7** Add an `in_interrupt` counter, incremented in `isr_common_stub`'s dispatch, and assert on
it in `sched_block`. Confirm nothing trips it.

🔴 **35.8** Add timeouts to blocking: `sched_block_timeout(channel, ms)` that wakes either on the
channel or on a deadline, and returns which. Then use it to give the ATA driver a real timeout.

---

## What we covered

- Three ways to wait, and why two of them are wrong in ways that do not show up in testing.
- The four-function Unix Sixth Edition interface, and the six channels it is used on.
- Channels as addresses compared for equality, and why that makes the pairing hard to get wrong.
- A scan instead of per-object queues, and exactly what that trades.
- Rule one: always loop, because a wakeup is a hint and not a guarantee.
- Rule two: the lost-wakeup window, and the precise reason disabling interrupts across test-and-sleep
  closes it rather than narrowing it.
- Rule three: `wake` versus `wake_one`, and the thundering herd.
- Sleeping as a deadline in a different state, the scan cost, and the `+ 1` that makes sleeps never
  too short.
- Nine ticks in forty-eight seconds, which is what blocking buys.
- Two shapes of deadlock, lock ordering as the standard prevention, and the `ps` column that turns a
  hang into a question.
- Waking is interrupt-safe; switching and blocking are not.

[Chapter 36](36-synchronisation.md) is about the races that appeared the moment Chapter 33 ran
`sti()` inside a system call.

---

[← fork and exec](34-fork-exec.md) · [Contents](README.md) · [Next: Synchronisation →](36-synchronisation.md)
