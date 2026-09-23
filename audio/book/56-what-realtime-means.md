# Chapter 56 — What "Real-Time" Actually Means

> Everything so far rendered to a file, where taking ten seconds to compute one second of audio
> was merely slow. Part V removes that luxury. Real-time audio has a **hard deadline**, and
> missing it once is audible.

---

## 56.1 The callback

The operating system's audio driver calls your function whenever it needs more samples.

```cpp
void audioCallback(float* output, const float* input, int numFrames)
{
    // You have numFrames/sampleRate seconds to fill this buffer.
    // Not "usually". Every single time.
    for (int i = 0; i < numFrames; ++i)
        output[i] = computeSample();
}
```

**This function runs on a high-priority thread that you do not own.** It is called by the driver,
on the driver's schedule, and if it does not return in time the driver plays whatever happens to
be in the buffer — usually the previous block again, or silence. Either way the user hears a
click.

```
   Time available per callback = numFrames / sampleRate
```

| Buffer | At 44.1 kHz | At 48 kHz |
|---|---|---|
| 64 | 1.45 ms | 1.33 ms |
| 128 | 2.90 ms | 2.67 ms |
| 256 | 5.80 ms | 5.33 ms |
| 512 | 11.61 ms | 10.67 ms |
| 1024 | 23.22 ms | 21.33 ms |

**That is the entire budget**, and it must cover everything: your DSP, the driver's own work, and
any other audio applications on the system.

---

## 56.2 Soft real-time versus hard real-time

Audio is **soft real-time**: missing a deadline degrades quality rather than causing a
catastrophe. Nobody dies if your reverb glitches.

But the tolerance is very low, for a specific perceptual reason: Chapter 13 established that a
discontinuity is broadband energy. A dropout is a discontinuity at both ends, so a single missed
buffer produces two clicks — and Chapter 3 established that the ear's temporal resolution is
about 2 ms, so a 5.8 ms gap is not subtle.

**The practical standard: zero dropouts, ever.** A glitch once an hour is a bug report. A glitch
once a minute makes the software unusable.

### What this means for how you think about cost

> **The worst case matters. The average does not.**

This is the single most important shift in mindset for Part V, and it inverts normal optimisation
intuition.

Consider two algorithms:

| | Algorithm A | Algorithm B |
|---|---|---|
| Average cost | 2.0 ms | 3.5 ms |
| Worst case | 9.0 ms | 4.0 ms |

With a 5.8 ms budget, **A glitches and B does not**, despite A being 43% cheaper on average. A is
unusable and B ships.

Chapter 49 met this concretely: non-uniform partitioned convolution's large FFTs all landing in
one callback. The fix was not to make it faster on average but to **flatten the profile**.

---

## 56.3 Latency

**Round-trip latency** is the total time from a sound entering the system to the processed
version leaving it.

```
   input → ADC → driver buffer → YOUR CALLBACK → driver buffer → DAC → output
```

Contributions:

| Source | Typical |
|---|---|
| ADC conversion | 0.5–1.5 ms |
| Input buffer | 1 × buffer size |
| Your processing | (within the buffer period) |
| Output buffer | 1 × buffer size |
| DAC conversion | 0.5–1.5 ms |
| **Plus** any plugin lookahead | 0–50 ms |

So a 256-frame buffer at 44.1 kHz gives roughly `5.8 + 5.8 + 2 = 13.6 ms` round trip.

**What latency is acceptable:**

| Task | Tolerable |
|---|---|
| Playing a virtual instrument | **< 10 ms** (the threshold where it stops feeling connected) |
| Singing while monitoring yourself | **< 6 ms** (you also hear yourself through bone conduction, so the comb filtering is the issue) |
| Playing guitar through an amp sim | < 10 ms |
| Mixing and playback only | 20–50 ms is fine |
| Live sound reinforcement | < 5 ms (or the PA and the stage sound comb) |

**The musician-latency figure has a physical anchor:** sound travels 343 m/s, so 10 ms is 3.4
metres. Standing 3.4 m from your amplifier is normal; 10 m (30 ms) feels wrong. That is roughly
the threshold.

**Note the singing case.** You hear your own voice through bone conduction with zero delay, and
through the monitoring path with some delay. The two combine, and Chapter 42 tells you what
happens when you sum a signal with a delayed copy: a comb filter. At 6 ms the first notch is at
83 Hz; at 15 ms it is at 33 Hz and there are notches throughout the vocal range. Singers describe
this as "my voice sounds weird", and they are hearing comb filtering, not delay.

---

## 56.4 Choosing a buffer size

The fundamental trade-off:

**Smaller buffers:**
- Lower latency
- More callbacks per second, each with fixed overhead
- **Less slack** to absorb a momentarily expensive computation
- More sensitive to system interference

**Larger buffers:**
- Higher latency
- Fewer callbacks, lower overhead
- More slack

**The overhead is real.** Each callback costs a thread wake-up, cache warming, and driver
bookkeeping — roughly 10–50 µs regardless of size. At 64 frames (1.45 ms) that overhead is 1–3%
of the budget; at 1024 frames it is 0.05%.

**Practical guidance:**

| Situation | Buffer |
|---|---|
| Recording, playing instruments | 64–128 |
| General production | 256 |
| Mixing with many plugins | 512–1024 |
| Playback only | 1024–2048 |

**Aim to use no more than 50–70% of the budget.** The rest is headroom for the system doing other
things — and it will.

---

## 56.5 Why the deadline gets missed

Six causes, roughly in order of how often they are the culprit.

**1. Your code is too slow.** The obvious one, and the least common in practice.

**2. Memory allocation.** `new`, `malloc`, `std::vector::push_back`, `std::string` concatenation.
Usually fast, occasionally *much* slower when the allocator must ask the OS for more memory —
and the allocator may take a lock, which can block for milliseconds. Chapter 59.

**3. Locks.** A mutex in the audio callback means the audio thread can be blocked by a lower-
priority thread that holds it. **Priority inversion**: your high-priority audio thread waits for
a low-priority GUI thread, which is itself waiting to be scheduled. Chapter 60.

**4. Page faults.** Memory that has been swapped out, or never touched. The first access takes a
trip to the OS and possibly to disk. Fix by pre-touching all buffers during `prepare()`.

**5. Denormals.** Chapter 6. A reverb tail decaying into denormal range can slow arithmetic by
10–100×. The characteristic symptom is CPU load *rising during silence*. Chapter 59.

**6. The system.** Another application, a driver, an antivirus scan, a power-management state
change. You cannot prevent these; you can only leave enough headroom to survive them.

---

## 56.6 Measuring

You cannot manage what you do not measure. The essential instrumentation:

```cpp
class CallbackProfiler
{
public:
    void begin() { start_ = std::chrono::steady_clock::now(); }

    void end(int numFrames, double sampleRate)
    {
        const auto elapsed = std::chrono::steady_clock::now() - start_;
        const double us = std::chrono::duration<double, std::micro>(elapsed).count();
        const double budgetUs = numFrames / sampleRate * 1e6;

        const double load = us / budgetUs;

        // Store into a lock-free ring buffer for the GUI to read.
        // Do NOT print from the audio thread (Chapter 59).
        peakLoad_ = std::max(peakLoad_.load(), load);
        recent_.push(load);

        if (load > 1.0) ++overruns_;
    }

    double peakLoad() const { return peakLoad_.load(); }
    int    overruns() const { return overruns_.load(); }

private:
    std::chrono::steady_clock::time_point start_;
    std::atomic<double> peakLoad_{ 0.0 };
    std::atomic<int>    overruns_{ 0 };
    LockFreeRingBuffer<double> recent_;
};
```

**Report the peak, not the average.** A CPU meter showing 30% average while the peak is 105% is
lying to you about the only number that matters.

**Plot a histogram of callback times** over a few minutes. What you want to see is a tight
distribution. What reveals problems is a long tail — a few callbacks taking many times the
median. Those are your allocations, locks and page faults.

**Never print from the audio callback.** `std::cout` allocates, locks, and does I/O — three
violations in one statement, and it will itself cause the glitch you were trying to diagnose.
Push data to a lock-free queue and print from another thread (Chapter 60).

---

## 56.7 Thread priority

The audio thread must run at high priority, and the OS provides a mechanism.

**Windows:** the driver typically handles this, or you use the **Multimedia Class Scheduler
Service** (MMCSS):

```cpp
DWORD taskIndex = 0;
HANDLE task = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
// ... audio work ...
AvRevertMmThreadCharacteristics(task);
```

**macOS:** time-constraint threads via `thread_policy_set`, specifying period, computation and
constraint.

**Linux:** `SCHED_FIFO` with a real-time priority, which requires appropriate permissions
(`rtprio` limits in `/etc/security/limits.conf`).

**What you must not do:** raise the priority and then block. A high-priority thread that holds a
lock and gets descheduled can hang the whole system. This is why Chapter 60's lock-free
techniques exist — they are not an optimisation, they are what makes high priority safe.

---

## 56.8 The two-thread model

The architecture that every audio application uses:

```
   ┌──────────────────┐         ┌──────────────────┐
   │   AUDIO THREAD   │         │   MAIN/GUI THREAD │
   │  high priority   │         │  normal priority  │
   │                  │         │                   │
   │  - process audio │◄────────│  - user input     │
   │  - NO allocation │ lock-   │  - allocate       │
   │  - NO locks      │ free    │  - load files     │
   │  - NO I/O        │ queues  │  - draw           │
   │  - NO exceptions │────────►│  - log            │
   └──────────────────┘         └──────────────────┘
```

**Everything expensive happens on the main thread.** Loading a sample, allocating a delay line,
computing filter coefficients from scratch, reading a preset — all of it.

**The audio thread only processes.** It reads parameters that were already prepared, writes
samples, and communicates through lock-free queues.

**The queues go both ways:**
- Main → audio: parameter changes, note events, new buffers to use
- Audio → main: metering data, "I have finished with this buffer, please free it", profiling

That last one is a genuinely important pattern: the audio thread cannot call `delete`, so when it
is finished with an object it posts a message and the main thread does the freeing. Chapter 60
builds it.

---

## 56.9 Exercises

**56.1** Compute the callback budget for buffer sizes 32 through 2048 at 44.1, 48 and 96 kHz.
Build the table.

**56.2** Write a callback that deliberately takes a variable amount of time (e.g. a loop whose
length depends on a random value). Instrument it and plot the distribution of callback times.

**56.3** Measure the round-trip latency of your system empirically: output an impulse, loop the
output back to the input physically, and measure the delay.

**56.4** Compute the comb filter notches for monitoring latencies of 3, 6, 12 and 25 ms. At which
latency is the first notch inside the vocal range?

**56.5** Build the callback profiler. Run a synth and report peak load, average load and overrun
count.

**56.6** *Deliberate breakage.* Put a `std::cout` in the audio callback. Measure the callback time
distribution before and after.

**56.7** Allocate a large `std::vector` inside the callback once every 1000 callbacks. Find it in
the timing histogram.

**56.8** Measure the fixed per-callback overhead: run an empty callback at buffer sizes 32 and
2048 and compare the CPU usage.

**56.9** Calculate how much of your budget a 64-voice synth (Chapter 41's estimate of 220 M
ops/sec) uses at 256 frames and 44.1 kHz.

---

### Chapter summary

- The driver calls your callback on **its** schedule. You have `numFrames / sampleRate` seconds,
  **every time**, not on average.
- Audio is **soft real-time** but with very low tolerance: a missed buffer is two discontinuities,
  which is two clicks. The standard is **zero dropouts**.
- **The worst case matters; the average does not.** An algorithm with a lower average but a
  spikier profile can be unusable where a more expensive, flatter one ships.
- **Latency** is roughly two buffer periods plus converter time. Under 10 ms for playing
  instruments; under 6 ms for singing — where the real problem is **comb filtering** against bone
  conduction, not delay.
- Smaller buffers cost per-callback overhead (10–50 µs) and leave less slack. Aim to use
  **50–70% of the budget**, no more.
- Deadlines are missed by: slow code, **allocation**, **locks** (priority inversion), page faults,
  **denormals** (CPU load rising during silence), and the rest of the system.
- **Measure the peak, not the average**, and plot a histogram — the long tail is where the
  allocations and locks are. **Never print from the callback.**
- Use the OS's real-time priority mechanism, and never block on a high-priority thread.
- **The two-thread model**: everything expensive on the main thread, only processing on the audio
  thread, communicating through lock-free queues in both directions.

**Next:** [Chapter 57 — Audio APIs on Windows: WASAPI, ASIO, and Friends](57-audio-apis.md)
