# Chapter 18 — The PIT: time and preemption

[← The PIC](17-pic-irqs.md) · [Contents](README.md) · [Next: The keyboard →](19-keyboard.md)

---

## Goal

Claim IRQ 0 and give the kernel a heartbeat. Three port writes, a handler that increments a counter,
and the machine starts doing something on its own for the first time.

Everything in Part IV — preemption, sleeping, timeouts, the idea that a program can be interrupted
against its will — is built on this one interrupt arriving a hundred times a second.

---

## 1. Reactive to active

Until now the kernel has been entirely reactive. It runs when something calls it, and between calls
it does not exist.

The timer changes that. After this chapter, control returns to the kernel every 10 milliseconds no
matter what the running code is doing, whether or not it cooperates, and whether or not it wants to.

That involuntariness is the whole point. A program cannot monopolise the CPU, cannot avoid being
descheduled, and cannot tell that it was. The illusion of many programs running at once is built
entirely on top of it.

---

## 2. The 8253/8254

Three independent 16-bit counters, each fed from the same clock, each counting down and doing
something when it reaches zero.

| Channel | Port | Wired to |
|---|---|---|
| 0 | `0x40` | IRQ 0 — the system timer |
| 1 | `0x41` | DRAM refresh on the original PC; unused since |
| 2 | `0x42` | The PC speaker |

We use channel 0. Channel 2 is how you make the speaker beep, and it is Exercise 18.7.

### 2.1 The strangest constant in the PC

```c
#define PIT_FREQUENCY 1193182u
```

1.193182 MHz. It is one third of 3.579545 MHz, the NTSC colour burst frequency.

In 1981 the cheapest way to get a stable clock was to divide the crystal already on the board for the
composite video output. Every PC since has kept it for compatibility, and every operating system on
earth divides by it.

So the number encodes a decision about American analogue television, made for cost reasons, in a
computer that has not had a composite video output for thirty years.

### 2.2 The divisor

The counter is loaded with a divisor and counts down at 1193182 Hz. When it reaches zero it fires and
— in the mode we use — reloads itself.

```
    interrupt frequency = 1193182 / divisor
```

| Divisor | Frequency | Period |
|---|---|---|
| 1 | 1193182 Hz | 0.8 µs |
| 11932 | 100 Hz | 10 ms |
| 59659 | 20 Hz | 50 ms |
| 65536 (as 0) | 18.2 Hz | 55 ms |

That last row is the default the BIOS leaves, and 18.2 Hz is the number every DOS programmer knew.

---

## 3. Choosing 100 Hz

```c
#define TIMER_HZ      100u
```

The tradeoff, in both directions:

**Too slow** and time slices are coarse. At 18.2 Hz a task that yields immediately still holds the
CPU for up to 55 ms, and interactive response suffers visibly.

**Too fast** and the interrupt overhead dominates. At 10,000 Hz the handler runs every 100 µs; the
entry, dispatch and exit cost maybe 300 cycles, so a few percent of the CPU goes to timekeeping.
Worse, every interrupt wakes a sleeping CPU, which on a laptop is a measurable battery cost.

100 Hz is the number Linux used for years, for exactly these reasons. Modern Linux defaults to 250 or
1000 Hz and has a "tickless" mode that stops the timer entirely when nothing needs it — which is a
genuinely clever piece of engineering and a chapter of its own.

For us, 10 ms is fast enough that a time slice feels instant and slow enough that the overhead is
invisible.

---

## 4. Programming it

```c
void timer_init(uint32_t frequency)
{
    hz = frequency;

    uint32_t divisor = PIT_FREQUENCY / frequency;

    if (divisor == 0)      divisor = 1;
    if (divisor > 0xFFFF)  divisor = 0xFFFF;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    ...
    irq_register(IRQ_TIMER, timer_callback);
}
```

### 4.1 The command byte

`0x36` = `00 11 011 0`:

| Bits | Value | Meaning |
|---|---|---|
| 7–6 | `00` | Channel 0 |
| 5–4 | `11` | Access mode: write the low byte, then the high byte |
| 3–1 | `011` | Mode 3: square wave generator |
| 0 | `0` | Binary, not BCD |

**Mode 3** is the right choice for a periodic tick: the counter reloads itself automatically, so the
interrupt repeats forever with no help from us.

Mode 0 fires once and stops — that is what you would use for a one-shot timeout, and it is how a
tickless kernel works: program a one-shot for the next thing that needs to happen, and sleep until
then.

**Nobody uses BCD.** Bit 0 exists because the 8253 could count in binary-coded decimal, which was
useful for driving a numeric display in 1978.

### 4.2 Two writes for one value

```c
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
```

A 16-bit value through an 8-bit port, low byte first. Access mode `11` in the command byte is what
tells the chip to expect two writes.

Between the two writes the counter holds a half-updated value. The chip handles this correctly —
it does not reload until both bytes arrive — but it is another instance of the index/data and
split-value patterns that this era of hardware is full of.

### 4.3 The bounds check

```c
    if (divisor == 0)      divisor = 1;
    if (divisor > 0xFFFF)  divisor = 0xFFFF;
```

The divisor is 16 bits. Ask for more than 1,193,182 Hz and the division truncates to zero — which the
chip interprets as 65536, the *slowest* possible rate. Ask for less than 19 Hz and it overflows.

Both failures present as "the timer runs at completely the wrong speed", which is a confusing symptom
when you asked for something reasonable and got something absurd.

### 4.4 The drift nobody corrects

```c
    uint32_t actual_mhz = PIT_FREQUENCY * 1000 / divisor;
    LOG_INFO("pit: divisor %u -> %u.%03u Hz (asked for %u)",
             divisor, actual_mhz / 1000, actual_mhz % 1000, frequency);
```

```
[    0.010] inf  pit: divisor 11931 -> 100.006 Hz (asked for 100)
```

1193182 / 100 = 11931.82, which truncates to 11931, giving 100.0069 Hz. Over a day that is about six
seconds of drift.

Real kernels correct for it against the RTC or the TSC, which is a whole subsystem (NTP, clock
disciplining, and the reason `adjtimex` exists). We log it and move on — six seconds a day is not
going to confuse anything in this book, and pretending the number is exactly 100 would be worse than
printing the truth.

---

## 5. The handler

```c
static void timer_callback(registers_t *regs UNUSED)
{
    ticks++;
    sched_tick();
}
```

Two lines, and both are load-bearing.

### 5.1 It must be fast

The handler runs with interrupts disabled — the gate cleared `IF` — and it must finish well within
10 ms. If it takes longer, the next tick arrives before this one is acknowledged, the PIC drops it,
and **time runs slow, silently**.

That is a nasty failure: nothing errors, nothing logs, and `uptime` simply becomes wrong. Anything
that measures a timeout in ticks measures it against a clock that has lost time.

So the rule for the timer handler, more than any other: do the bookkeeping and get out.

### 5.2 It does not switch tasks

```c
void sched_tick(void)
{
    ...
    if (current_task) {
        current_task->ticks_used++;
        if (current_task->time_slice > 0) current_task->time_slice--;
        if (current_task->time_slice == 0) timer_need_resched = true;
    }
}
```

`sched_tick` decrements the slice and sets a flag. The actual switch happens in
`interrupt_dispatch`, after the EOI:

```c
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));

        if (timer_need_resched && sched_enabled) {
            timer_need_resched = false;
            schedule();
        }
```

Chapter 17, §5.1 covered the ordering. Chapter 31 covers why switching from here is safe while
switching from inside `timer_callback` would not be.

The short version: at the point of the EOI we are on this task's kernel stack holding a complete
trap frame. `schedule()` saves that stack pointer in the task's context; when the task is picked
again it returns from `schedule()` right there, walks back out through `isr_common_stub`, and `iret`s
to exactly the instruction that was interrupted.

---

## 6. Reading the clock without tearing

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

This looks pointless and is not.

`ticks` is 64 bits on a 32-bit machine, so reading it is **two loads**. The timer interrupt can land
between them, and if the low half has just wrapped, you get the new low half with the old high half —
a value off by four billion ticks, or about 497 days.

Reading twice and retrying if the value changed costs two instructions and removes the bug. This is
the "sequence lock" pattern in miniature, and the same idea appears in Linux's `ktime` and in every
lock-free reader of a multi-word value.

At 100 Hz the low half wraps every 497 days, so the window is vanishingly rare — and catastrophic
when it hits, which is exactly the profile of a bug that ships.

### 6.1 Why 64 bits at all

A 32-bit tick counter at 100 Hz wraps after 497 days. That sounds like plenty until you consider what
depends on it: `wake_tick` comparisons in the scheduler, timeouts in drivers, and any arithmetic of
the form `now + delay`.

Every one of those is wrong across a wrap. Linux handles 32-bit `jiffies` wrapping with the
`time_after()` macro family, which does signed subtraction so that comparisons stay correct — and
getting that right is a small art.

64 bits puts the wrap in the year seven billion and removes the whole topic. The cost is the
two-load read above.

---

## 7. Spinning, and why it is temporary

```c
void timer_spin_ms(uint32_t ms)
{
    uint64_t target = timer_ticks() + ((uint64_t)ms * hz) / 1000 + 1;
    while (timer_ticks() < target)
        __asm__ volatile ("pause");
}
```

A busy wait. Legal in exactly two places: before the scheduler exists, and inside drivers that must
not sleep.

Note the `+ 1`. Without it, `timer_spin_ms(10)` at 100 Hz waits for one tick — but we might be called
9.9 ms into the current tick, so the actual delay is anywhere from 0.1 ms to 10 ms. Adding one tick
makes it 10–20 ms, which is at least never *too short*. Every sleep implementation has this
granularity problem and the honest fix is a higher-resolution timer.

**`pause`** is the spin-loop hint. On a hyperthreaded CPU it yields the pipeline to the other thread;
on older CPUs it is a `nop` with a different encoding. It costs nothing and it is what you should
write in any spin loop.

### 7.1 The trap

> Note it reads `timer_ticks()` rather than caching — and note that if interrupts are disabled,
> `ticks` never advances and this hangs forever. That has caught everyone at least once.

Calling `timer_spin_ms` from inside an interrupt handler, or inside a `cli`/`sti` critical section,
is an infinite loop. The variable it is waiting on can only be changed by the interrupt it has
disabled.

Chapter 35 replaces this with `sleep_ms`, which blocks the task and lets someone else run.

---

## 8. Running it

```c
    timer_init(TIMER_HZ);
    ...
    sti();

    uint64_t start = timer_ticks();
    while (timer_ticks() - start < 300) {
        if (timer_ticks() % 100 == 0)
            kprintf("\r%u seconds", (uint32_t)(timer_ticks() / 100));
    }
```

```
[    0.010] inf  pit: divisor 11931 -> 100.006 Hz (asked for 100)
[    0.010] inf  irq: line 0 claimed
1 seconds
2 seconds
3 seconds
```

The machine is doing something without being asked. That is new.

### 8.1 Checking the rate

The most useful test is against the host's clock. Print `timer_ms()` once a second for ten seconds
and compare with a stopwatch, or:

```c
    uint64_t t0 = timer_ms();
    timer_spin_ms(5000);
    kprintf("asked 5000, got %u\n", (uint32_t)(timer_ms() - t0));
```

QEMU's timing is not exact — it is emulated, and it catches up in bursts if the host is loaded — so a
few percent off is expected. An order of magnitude off means the divisor is wrong.

### 8.2 Seeing it in the QEMU log

```bash
qemu-system-i386 ... -d int -D bin/qemu.log
$ grep -c "v=20" bin/qemu.log
```

`v=20` is vector 32, IRQ 0. Count them over a known interval and you have the real rate.

The log is enormous at 100 Hz — that is a hundred lines per second — which is itself a useful lesson
about what the timer costs.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| No ticks at all | IRQ 0 masked, or `sti` never executed |
| Ticks once, then stops | Missing EOI (Chapter 17, §5) |
| Time runs slow | Handler too long, or ticks being dropped |
| Time runs absurdly slow | Divisor truncated to 0, giving 18.2 Hz |
| `timer_spin_ms` hangs | Called with interrupts disabled |
| Uptime jumps by 497 days | The 64-bit read race; check `timer_ticks` |
| Boot hangs after `timer_init` | Registered the handler before `sti` and something faults in it |

---

## 10. Exercises

🟢 **18.1** Change `TIMER_HZ` to 1000 and to 18. Measure the drift against a stopwatch in each case.

🟢 **18.2** Remove the `+ 1` from `timer_spin_ms` and measure `timer_spin_ms(10)` a hundred times.
What is the spread?

🟢 **18.3** Print the divisor and the actual frequency for 50, 100, 250 and 1000 Hz. Which is the
most accurate, and why?

🟡 **18.4** Implement `timer_uptime_string()` that formats ticks as `1d 04:23:07.15`, and add it to
the `uptime` shell builtin.

🟡 **18.5** Make the timer handler deliberately slow — a loop of 10 million iterations — and measure
how much time is lost. Confirm that ticks are being dropped rather than delayed.

🟡 **18.6** Implement one-shot timers: a small sorted list of `(deadline, callback)` pairs, checked
in `sched_tick`. Use it to give the ATA driver a real timeout instead of a spin count.

🔴 **18.7** Make the PC speaker beep. Program channel 2 with mode 3 and a divisor for the frequency,
then set bits 0 and 1 of port `0x61` to connect it. Turn it off after 200 ms. Then play a scale.

🔴 **18.8** Read the TSC (`rdtsc`), calibrate it against the PIT over 100 ms, and use it for
microsecond-resolution timing. Then replace the ATA driver's spin counts with real microsecond
timeouts.

---

## What we covered

- The timer as the thing that makes the kernel active rather than reactive, and why the
  involuntariness is the point.
- Three channels, one of which drives the speaker, and a base frequency that encodes a 1981 decision
  about American television.
- Choosing 100 Hz: what too slow and too fast each cost, and what tickless kernels do instead.
- Command byte `0x36` field by field, mode 3 versus mode 0, and the two-write divisor.
- The bounds check, because both overflow directions produce "completely the wrong speed".
- Drift, and why logging the truth beats pretending the number is round.
- A handler that must be fast, and that sets a flag rather than switching tasks.
- The two-load race on a 64-bit counter, and the sequence-lock read that removes it.
- Why 64 bits at all, and what Linux does instead with 32.
- `timer_spin_ms`, the `+ 1`, `pause`, and the deadlock you get by calling it with interrupts off.

[Chapter 19](19-keyboard.md) claims IRQ 1 and turns switch closures into characters — which is
harder, and stranger, than it sounds.

---

[← The PIC](17-pic-irqs.md) · [Contents](README.md) · [Next: The keyboard →](19-keyboard.md)
