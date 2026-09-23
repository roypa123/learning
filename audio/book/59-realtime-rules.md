# Chapter 59 — The Real-Time Rules: No Malloc, No Locks, No Excuses

> A short list of things you must never do in an audio callback, and the reasons — because
> knowing *why* is what lets you recognise the hundredth case that is not on the list.

---

## 59.1 The list

**Never, in the audio callback:**

1. **Allocate or free memory** — `new`, `delete`, `malloc`, `free`, and anything that calls them
2. **Take a lock** — `std::mutex`, `std::lock_guard`, any blocking synchronisation
3. **Do file or network I/O** — including `std::cout`
4. **Throw an exception**
5. **Call an unknown function** — a library, a callback, anything you cannot inspect
6. **Wait on anything** — `sleep`, `join`, `wait`, condition variables
7. **Use anything with unbounded runtime** — `std::map` lookups on a growing map, unbounded loops

**The underlying principle unifies all seven:**

> **Every operation in the audio callback must have a bounded worst-case execution time, and must
> not depend on any other thread making progress.**

That is the whole rule. The list is just its common violations.

---

## 59.2 Why allocation is forbidden

`malloc` is usually fast — tens of nanoseconds from a thread-local cache. **Usually.**

Sometimes it must:

- Take a lock on the global heap (another thread is also allocating)
- Request more memory from the OS (`mmap` or `VirtualAlloc`) — a syscall
- Trigger a page fault when the new memory is first touched
- Wait while the OS zeroes a page

**Worst case: milliseconds.** At a 5.8 ms budget, one such event is a dropout.

**The hidden allocations** are the dangerous ones, because they do not look like allocation:

```cpp
void callback(float* out, int n)
{
    std::vector<float> temp(n);              // ALLOCATES
    buffer.push_back(x);                     // MAY allocate
    buffer.resize(n);                        // MAY allocate
    std::string msg = "voice " + id;         // ALLOCATES
    auto p = std::make_unique<Voice>();      // ALLOCATES
    std::function<void()> f = [x](){...};    // MAY allocate (large captures)
    map[key] = value;                        // MAY allocate
    voices.emplace_back(...);                // MAY allocate
    throw std::runtime_error("...");         // ALLOCATES
    std::shared_ptr<T> sp = other;           // control block may allocate
}
```

**`std::function` is a particularly common trap**, because it looks like a plain callable. If the
captured state exceeds the small-buffer optimisation size (typically 16 bytes), it heap-allocates
on construction. Assigning one inside a callback allocates.

### The fix

**Allocate everything in `prepare()`, sized for the maximum:**

```cpp
void prepare(double sampleRate, int maxBlockSize, int maxVoices)
{
    scratch_.resize(static_cast<size_t>(maxBlockSize));
    voices_.resize(static_cast<size_t>(maxVoices));       // constructed, ready
    events_.reserve(1024);

    // Pre-TOUCH the memory so the pages are resident. Allocating does not
    // necessarily commit physical pages; the first write does.
    std::fill(scratch_.begin(), scratch_.end(), 0.0f);
}
```

**The pre-touch matters.** On most systems, allocated-but-never-written memory is not backed by
physical pages. The first write triggers a page fault, which is a trip into the kernel. Writing
zeros during `prepare()` forces the pages to be committed while you are not on a deadline.

### Detecting violations

```cpp
// A debug-build allocation detector. Replace the global operators and
// assert if the audio thread allocates.
thread_local bool g_inAudioCallback = false;

void* operator new(std::size_t size)
{
    assert(!g_inAudioCallback && "ALLOCATION IN AUDIO CALLBACK");
    return std::malloc(size);
}

// In the callback:
struct AudioThreadGuard
{
    AudioThreadGuard()  { g_inAudioCallback = true;  }
    ~AudioThreadGuard() { g_inAudioCallback = false; }
};
```

**This finds violations you did not know you had**, particularly inside library code. Run it in
every debug build; it costs nothing in release.

---

## 59.3 Why locks are forbidden

A lock does not just cost time — it costs **an unbounded amount of time**, because it depends on
another thread.

### Priority inversion

```
   1. Low-priority GUI thread takes the lock.
   2. The OS deschedules it (its time slice ends).
   3. High-priority audio thread tries to take the lock and BLOCKS.
   4. The audio thread now waits for the GUI thread to be rescheduled.
   5. That could be 10 ms. Or 50 ms. It depends on system load.
```

**Your highest-priority thread is now waiting for your lowest-priority thread.** The priority
system has inverted, and the audio deadline is gone.

Some operating systems implement **priority inheritance** (temporarily boosting the lock holder),
which helps — but it is not universally available and it does not make the wait bounded.

### Things that take locks without looking like it

```cpp
std::cout << x;                     // the stream takes a lock
printf("...");                      // takes a lock
malloc / new                        // may take the heap lock
std::shared_ptr copy                // atomic refcount -- not a lock, but contended
std::mt19937                        // fine, but some RNGs use locks
file operations                     // lock plus I/O
some logging libraries              // lock plus allocation plus I/O
```

### The alternative

**Lock-free communication** — Chapter 60. The audio thread never waits; it reads what is
available and continues.

For simple values, `std::atomic` is enough:

```cpp
std::atomic<float> cutoff{ 1000.0f };

// GUI thread:
cutoff.store(newValue, std::memory_order_relaxed);

// Audio thread:
const float c = cutoff.load(std::memory_order_relaxed);
```

**`std::atomic<T>::is_lock_free()`** tells you whether it compiles to a hardware instruction or a
hidden mutex. For `float`, `double`, `int` and pointers on any modern platform it is lock-free.
For larger structs it may not be — check, do not assume:

```cpp
static_assert(std::atomic<float>::is_always_lock_free);
```

---

## 59.4 Denormals

A floating-point number smaller than about `1.18e-38` (for `float`) is **denormal** — represented
with reduced precision to allow values near zero.

**On many CPUs, arithmetic on denormals is 10–100× slower**, because it falls back to a microcode
path rather than the hardware pipeline.

**Where they appear in audio:** anything that decays exponentially toward zero.

- Reverb tails after the input stops
- Filter state after silence
- Envelope releases
- Delay feedback

**The characteristic symptom: CPU load *rises* when the signal goes quiet.** That is diagnostic;
nothing else behaves that way.

### The fixes

**1. Flush-to-zero mode (best).** Tell the CPU to treat denormals as zero:

```cpp
#include <immintrin.h>

void enableFlushToZero()
{
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}
```

**Set this at the top of every audio callback**, not once at startup — the mode is per-thread and
some hosts reset it. A tiny RAII guard is the safe pattern:

```cpp
struct ScopedNoDenormals
{
    ScopedNoDenormals()  { saved_ = _MM_GET_FLUSH_ZERO_MODE();
                           _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); }
    ~ScopedNoDenormals() { _MM_SET_FLUSH_ZERO_MODE(saved_); }
    unsigned saved_;
};
```

Restoring on exit is politeness: a plugin should not change the host's FPU state.

**2. Add tiny DC** to keep values above the denormal threshold:

```cpp
constexpr float kAntiDenormal = 1e-20f;
state = state * coeff + input + kAntiDenormal;
```

Simple and portable, but it adds a small DC offset to everything. An alternating sign version
avoids that:

```cpp
antiDenormal_ = -antiDenormal_;
state += antiDenormal_;
```

**3. Snap to zero** below a threshold:

```cpp
if (std::fabs(state) < 1e-15) state = 0.0;
```

One comparison per sample, no side effects. Good for envelopes and filter states where you
control the code.

**Use flush-to-zero if you can, and snapping as a backup in specific places.** ARM processors
generally flush denormals by default, so this is largely an x86 concern — but plugins must
assume x86.

---

## 59.5 Other bounded-time requirements

**Bounded loops.** Every loop must have a compile-time or parameter-bounded iteration count.

```cpp
// BAD: could iterate indefinitely
while (queue.pop(event)) { handle(event); }

// GOOD: bounded, and drops the excess rather than blowing the deadline
int processed = 0;
while (processed < kMaxEventsPerBlock && queue.pop(event))
{
    handle(event);
    ++processed;
}
```

**Dropping events is better than missing the deadline.** A dropped MIDI note is one wrong note; a
dropout is a click that everyone hears.

**No exceptions.** Throwing allocates (for the exception object), unwinds through arbitrary
destructors, and takes an unpredictable amount of time. Audio code should use return codes and
be written so that failure is impossible in the callback.

**No virtual dispatch in the innermost loop** — not because it is forbidden, but because it
prevents inlining and vectorisation. Virtual calls at the *block* level (Chapter 17's `Processor`
interface) are entirely fine; the cost is amortised over hundreds of samples. Virtual calls
per-sample are not.

**Bounded data structures.** `std::map`, `std::unordered_map` and `std::list` all allocate on
insert. Fixed-size arrays and pre-allocated pools do not.

---

## 59.6 The pool pattern

When you genuinely need dynamic objects — voices, grains, events — pre-allocate a pool and manage
availability with flags rather than allocation.

```cpp
template <typename T, size_t N>
class Pool
{
public:
    T* acquire()
    {
        for (size_t i = 0; i < N; ++i)
        {
            const size_t idx = (next_ + i) % N;
            if (!inUse_[idx])
            {
                inUse_[idx] = true;
                next_ = (idx + 1) % N;
                return &items_[idx];
            }
        }
        return nullptr;          // pool exhausted -- caller decides what to do
    }

    void release(T* p)
    {
        const size_t idx = static_cast<size_t>(p - items_.data());
        if (idx < N) inUse_[idx] = false;
    }

private:
    std::array<T, N>    items_;
    std::array<bool, N> inUse_{};
    size_t next_ = 0;
};
```

**The linear search is `O(N)`** but `N` is small and fixed, so the worst case is bounded — which
is the actual requirement. A free-list makes it `O(1)` if `N` is large.

**Returning `nullptr` when exhausted is correct.** The caller then decides: steal (Chapter 41),
drop (Chapter 37's grains), or ignore. **Never** fall back to allocating.

---

## 59.7 The realistic position

Some honesty, because the rules are sometimes stated more absolutely than practice warrants.

**These rules are not arbitrary purity.** Every one corresponds to a real, observed failure mode,
and violations produce bugs that are intermittent, load-dependent and hard to reproduce — the
worst kind.

**But they are guidelines with a purpose, not commandments.** The purpose is bounded worst-case
time. If you can genuinely guarantee that — for example, a lock that is only ever taken by the
audio thread, or an allocator with a hard real-time guarantee — the rule does not apply.

**What you must not do is assume.** "It's probably fine" is how you get a plugin that works on
your machine and glitches on the user's. The discipline exists because the failure mode is
invisible during development: everything works until the system is under load, and then it does
not.

**A practical standard for this book:** follow all seven rules while learning. Break one only
when you can state precisely why the worst case is bounded, and measure to confirm it.

---

## 59.8 Exercises

**59.1** Implement the allocation detector from §59.2. Run it on a synth and see whether anything
allocates.

**59.2** *Deliberate breakage.* Allocate a 1 MB vector in the callback once per second. Plot the
callback time distribution and find the spikes.

**59.3** Put a `std::mutex` in the callback, taken also by a busy main thread. Measure the
callback times.

**59.4** Demonstrate priority inversion: a low-priority thread that holds a lock for 20 ms while
the audio thread waits.

**59.5** Build a reverb and let it decay into silence with flush-to-zero **off**. Measure the CPU
usage during the decay and after. Then turn it on.

**59.6** Compare the three denormal fixes for CPU cost and audible side effects.

**59.7** Implement `ScopedNoDenormals` and verify it restores the previous mode.

**59.8** Build the pool and use it for granular synthesis voices. What happens when the pool is
exhausted? Is that better or worse than allocating?

**59.9** Audit an existing audio callback of yours against all seven rules. Document every
violation and whether it is safe.

**59.10** Measure the cost of a virtual call per sample versus per block over a 512-sample buffer.

---

### Chapter summary

- **The single rule:** every operation in the callback must have a **bounded worst-case time**
  and must not depend on another thread making progress.
- **No allocation.** `malloc` is usually fast and occasionally milliseconds. Watch for hidden
  allocations in `std::vector` growth, `std::string`, `std::function`, `std::map` and `throw`.
  Allocate in `prepare()` for the maximum, and **pre-touch the memory** so the pages are
  committed.
- **No locks.** **Priority inversion** makes your highest-priority thread wait for your lowest.
  Use `std::atomic` for values and lock-free queues for messages (Chapter 60). Check
  `is_always_lock_free`.
- **No I/O, no exceptions, no waiting, no unknown functions, no unbounded loops.** Drop events
  rather than missing the deadline.
- **Denormals** slow arithmetic 10–100× in decaying tails. The symptom is **CPU load rising
  during silence**. Fix with **flush-to-zero set per callback** via an RAII guard, plus snapping
  to zero where you control the code.
- Use a **pre-allocated pool** for dynamic objects, returning `nullptr` when exhausted so the
  caller can steal or drop — never fall back to allocating.
- The rules are guidelines with a purpose. Break one only when you can **state and measure** the
  bounded worst case.

**Next:** [Chapter 60 — Lock-Free Ring Buffers and Thread Communication](60-lock-free-communication.md)
