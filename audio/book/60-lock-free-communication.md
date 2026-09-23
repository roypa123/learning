# Chapter 60 — Lock-Free Ring Buffers and Thread Communication

> Chapter 59 forbade locks. This chapter supplies the alternative: a handful of structures that
> let two threads exchange data without either ever waiting for the other.

---

## 60.1 The single-producer, single-consumer ring buffer

The workhorse. One thread writes, one thread reads, neither ever blocks.

```cpp
template <typename T, size_t Capacity>
class SPSCRingBuffer
{
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    // Called by the PRODUCER thread only.
    bool push(const T& item)
    {
        const size_t w = writeIndex_.load(std::memory_order_relaxed);
        const size_t next = (w + 1) & kMask;

        // Full? The reader has not caught up.
        if (next == readIndex_.load(std::memory_order_acquire))
            return false;

        buffer_[w] = item;

        // RELEASE: everything written above is visible before the index
        // update becomes visible to the consumer.
        writeIndex_.store(next, std::memory_order_release);
        return true;
    }

    // Called by the CONSUMER thread only.
    bool pop(T& item)
    {
        const size_t r = readIndex_.load(std::memory_order_relaxed);

        // Empty?
        if (r == writeIndex_.load(std::memory_order_acquire))
            return false;

        item = buffer_[r];

        readIndex_.store((r + 1) & kMask, std::memory_order_release);
        return true;
    }

    size_t sizeApprox() const
    {
        const size_t w = writeIndex_.load(std::memory_order_relaxed);
        const size_t r = readIndex_.load(std::memory_order_relaxed);
        return (w - r) & kMask;
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    std::array<T, Capacity> buffer_;

    // Separate cache lines: otherwise the two threads' writes to adjacent
    // memory cause FALSE SHARING and the cache line ping-pongs between
    // cores. This alignment is worth 2-5x in throughput.
    alignas(64) std::atomic<size_t> writeIndex_{ 0 };
    alignas(64) std::atomic<size_t> readIndex_{ 0 };
};
```

### Why this is correct without a lock

**Only one thread writes each index.** The producer owns `writeIndex_`; the consumer owns
`readIndex_`. Neither ever modifies the other's. That is what makes it safe without
synchronisation — there is no contention to resolve.

**Power-of-two capacity** lets the wrap be a mask (`& kMask`) instead of a modulo, which is
Chapter 8's trick and matters because these operations are on the hot path.

**Acquire/release ordering** is the subtle part, and it is worth understanding rather than
copying.

---

## 60.2 Memory ordering, explained

Modern CPUs and compilers **reorder memory operations** for speed. Without constraints, the
consumer could see an updated `writeIndex_` *before* it sees the data that index refers to — and
read garbage.

```cpp
// Producer:
buffer_[w] = item;                                  // (A) write the data
writeIndex_.store(next, std::memory_order_release); // (B) publish the index

// release means: (A) is guaranteed visible to any thread that sees (B)

// Consumer:
if (r == writeIndex_.load(std::memory_order_acquire))  // (C) read the index
    return false;
item = buffer_[r];                                     // (D) read the data

// acquire means: anything the producer did before its release is visible
// to us after this load
```

**Release-acquire creates a one-way barrier**: writes before the release cannot move after it,
and reads after the acquire cannot move before it. Together they guarantee the data is visible.

| Ordering | Meaning | Cost |
|---|---|---|
| `relaxed` | Atomic, but no ordering guarantees | Free on x86 |
| `acquire` | No later reads move before this | Free on x86, a fence on ARM |
| `release` | No earlier writes move after this | Free on x86, a fence on ARM |
| `acq_rel` | Both | |
| `seq_cst` | Total global order | Most expensive; the default |

**`seq_cst` is the default** for `std::atomic` operations, and it is the safe choice when you are
unsure. It is also the slowest. Use `relaxed` for independent values (Chapter 58's parameters),
and `acquire`/`release` for the publish/consume pattern above.

**On x86 this mostly does not matter** because the hardware has a strong memory model. On ARM —
Apple Silicon, phones, Raspberry Pi — it matters a great deal, and code that "works" on x86 with
relaxed ordering everywhere can genuinely fail on ARM.

---

## 60.3 False sharing

The `alignas(64)` on the two indices is not decoration.

CPU caches work in **cache lines**, typically 64 bytes. If the producer's `writeIndex_` and the
consumer's `readIndex_` share a cache line, then every write by either thread **invalidates the
line in the other core's cache**, forcing it to be re-fetched.

The threads are not sharing data — they are sharing a cache line — hence **false sharing**. The
cost is real: 2–5× throughput on a contended queue.

```cpp
alignas(64) std::atomic<size_t> writeIndex_{ 0 };
alignas(64) std::atomic<size_t> readIndex_{ 0 };
```

C++17 provides `std::hardware_destructive_interference_size` for the correct value, though 64 is
right on essentially all current hardware.

---

## 60.4 Sending parameter changes

For a handful of parameters, plain atomics are simpler than a queue:

```cpp
struct Parameters
{
    std::atomic<float> cutoff  { 1000.0f };
    std::atomic<float> resonance{ 0.707f };
    std::atomic<float> mix     { 0.5f };

    // Version counter: the audio thread only recomputes coefficients
    // when something has actually changed.
    std::atomic<uint32_t> version{ 0 };
};

// GUI thread:
params.cutoff.store(newValue, std::memory_order_relaxed);
params.version.fetch_add(1, std::memory_order_release);

// Audio thread, once per block:
const uint32_t v = params.version.load(std::memory_order_acquire);
if (v != lastVersion_)
{
    lastVersion_ = v;
    recomputeCoefficients();          // expensive; only when needed
}
```

**The version counter is the useful pattern.** Recomputing biquad coefficients costs `sin`, `cos`
and `pow`. Doing it every block when nothing changed wastes most of a filter's budget; doing it
only on change costs one atomic load.

**The caveat:** individual atomic stores are not a transaction. If the GUI sets cutoff and
resonance separately, the audio thread can see the new cutoff with the old resonance. For a
filter that is harmless. For parameters that must be consistent, use §60.5.

---

## 60.5 Publishing a whole object

When a set of values must change together — a full preset, a new impulse response, a wavetable —
publish a pointer atomically.

```cpp
template <typename T>
class AtomicPublisher
{
public:
    // GUI thread: prepare the new object completely, then publish.
    void publish(std::unique_ptr<T> newObject)
    {
        T* old = current_.exchange(newObject.release(), std::memory_order_acq_rel);

        // The audio thread may STILL be using `old`. We cannot delete it
        // yet. Park it and free it later, once we are certain.
        if (old) retired_.push_back(std::unique_ptr<T>(old));

        // Free anything retired long enough ago.
        collectGarbage();
    }

    // Audio thread: read the current pointer. Never deletes anything.
    T* get() const { return current_.load(std::memory_order_acquire); }

private:
    void collectGarbage()
    {
        // In practice: keep retired objects for a few seconds, or use
        // a generation counter the audio thread increments each block.
        if (retired_.size() > 4) retired_.erase(retired_.begin());
    }

    std::atomic<T*> current_{ nullptr };
    std::vector<std::unique_ptr<T>> retired_;   // GUI thread only
};
```

**The lifetime problem is the hard part**, and it is the one people get wrong.

When you swap in a new object, the audio thread may be **midway through using the old one**. You
cannot delete it immediately. Three standard solutions:

**1. Deferred deletion by time.** Keep retired objects for a few seconds. Crude, and it works —
the audio thread cannot possibly still be in a callback that started seconds ago.

**2. Generation counters.** The audio thread increments a counter at the start and end of each
callback. The GUI thread frees an object only once the counter has advanced past the point where
the object could still be in use. Precise, and not difficult.

**3. Reference counting with atomics.** `std::shared_ptr` with atomic operations. Correct, but
the atomic refcount increments are contended and `std::atomic_load(shared_ptr)` is not
guaranteed lock-free before C++20's `atomic<shared_ptr<T>>`.

**Never** free on the audio thread. That is allocation (Chapter 59), and it is also usually wrong
because the GUI thread may still hold a reference.

---

## 60.6 Sending messages the other way

The audio thread needs to send things back: meter levels, profiling data, "I have finished with
this object", "a voice was stolen".

**For metering, a plain atomic is enough**, because the reader only wants the latest value:

```cpp
// Audio thread:
peakLevel_.store(std::max(peakLevel_.load(std::memory_order_relaxed), blockPeak),
                 std::memory_order_relaxed);

// GUI thread, at frame rate:
const float peak = peakLevel_.exchange(0.0f, std::memory_order_relaxed);
```

**`exchange` reads and resets in one operation**, which gives you the peak since the last read
without missing anything.

**For events that must not be lost**, use the ring buffer in the other direction:

```cpp
struct AudioMessage
{
    enum Type { VoiceStolen, BufferFinished, Overrun, PeakLevel } type;
    union { int voiceIndex; void* pointer; float value; };
};

SPSCRingBuffer<AudioMessage, 256> toMainThread;
```

**The "please free this" pattern** is the most important use:

```cpp
// Audio thread: finished with an old buffer.
toMainThread.push({ AudioMessage::BufferFinished, .pointer = oldBuffer });

// Main thread, polling:
AudioMessage msg;
while (toMainThread.pop(msg))
    if (msg.type == AudioMessage::BufferFinished)
        delete static_cast<AudioBuffer*>(msg.pointer);   // safe HERE
```

**This is how the audio thread "frees" memory without allocating**: it hands ownership back.

---

## 60.7 Streaming from disk

A concrete application that uses everything above: playing a file too large to fit in memory
(Chapter 69 develops it).

```
   DISK THREAD                 RING BUFFER              AUDIO THREAD
   ───────────                 ───────────              ────────────
   read a chunk      ──push──►  [====....]  ──pop──►    play samples
   (blocking I/O,               several                 (never blocks)
    normal priority)            seconds
```

```cpp
class StreamingPlayer
{
public:
    // Disk thread
    void fillLoop()
    {
        while (running_)
        {
            // Keep the buffer at least half full.
            while (ring_.sizeApprox() < ring_.capacity() / 2)
            {
                float sample;
                if (!file_.readSample(sample)) { atEnd_ = true; break; }
                ring_.push(sample);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Audio thread
    void process(float* out, int numFrames)
    {
        for (int i = 0; i < numFrames; ++i)
            if (!ring_.pop(out[i]))
            {
                out[i] = 0.0f;                  // underrun: output silence
                underruns_.fetch_add(1, std::memory_order_relaxed);
            }
    }

private:
    SPSCRingBuffer<float, 262144> ring_;        // ~6 s at 44.1 kHz
    std::atomic<int> underruns_{ 0 };
};
```

**Buffer size is the key design decision.** It must cover the worst-case disk latency, which on a
spinning disk under load can be 100 ms or more. Several seconds is the usual choice — memory is
cheap and an underrun is not.

**The audio thread outputs silence on underrun** rather than blocking. Blocking would be a
dropout for every voice, not just this one; silence is a local failure.

**Count the underruns.** A streaming player that silently drops samples is impossible to debug;
one that reports a count tells you immediately whether the disk is keeping up.

---

## 60.8 Testing lock-free code

Lock-free bugs are **rare, timing-dependent, and do not reproduce**. Testing needs deliberate
effort.

**1. Thread sanitizer.** `-fsanitize=thread` detects data races at runtime and is remarkably
effective. Run every test under it.

**2. Stress testing.** Run the producer and consumer flat out for minutes, with a checksum or a
sequence number to detect lost or duplicated items:

```cpp
// Producer pushes 0, 1, 2, 3...  Consumer asserts it receives them in order.
static uint64_t expected = 0;
while (queue.pop(value))
{
    assert(value == expected++);
}
```

**3. Test on ARM.** x86's strong memory model hides ordering bugs. Code that is subtly wrong runs
correctly on Intel and fails on Apple Silicon or a Raspberry Pi. If you cannot test on ARM,
assume your ordering is wrong until proven otherwise.

**4. Vary the thread priorities and add random delays** to shake out timing assumptions.

**5. Prefer proven implementations.** `moodycamel::ConcurrentQueue`, `boost::lockfree`, and
JUCE's `AbstractFifo` are all well-tested. Writing your own SPSC queue is a good exercise and a
reasonable production choice; writing your own multi-producer queue is not.

---

## 60.9 Exercises

**60.1** Implement the SPSC ring buffer. Stress test it with a sequence-number check for 60
seconds.

**60.2** Remove the `alignas(64)` and benchmark throughput. How large is the false-sharing
penalty on your machine?

**60.3** *Deliberate breakage.* Change all orderings to `relaxed`. Does it still pass on x86? If
you have ARM hardware, test there.

**60.4** Build the version-counter parameter system. Measure the cost of recomputing biquad
coefficients every block versus only on change.

**60.5** Implement the atomic publisher with generation-counter garbage collection. Verify no
object is freed while in use.

**60.6** Build the metering path with `exchange`. Confirm no peak is missed between GUI frames.

**60.7** Implement the "please free this" message pattern. Verify with a sanitizer that nothing
is freed on the audio thread.

**60.8** Build the streaming player. Reduce the ring buffer until you get underruns, and find the
minimum safe size on your system.

**60.9** Run everything under `-fsanitize=thread`. Fix whatever it reports.

**60.10** Write a test that pushes from one thread and pops from another with randomised sleeps,
and assert that the total pushed equals the total popped plus whatever remains.

---

### Chapter summary

- The **SPSC ring buffer** is the core structure: one producer, one consumer, **each owning one
  index**, so there is no contention and no lock.
- **Power-of-two capacity** makes the wrap a mask. **`alignas(64)` on the two indices** prevents
  **false sharing**, which is worth 2–5× throughput.
- **Acquire/release ordering** guarantees the data is visible before the index that refers to it.
  `relaxed` for independent values; `seq_cst` is the safe default and the slowest. **On x86 this
  mostly does not matter; on ARM it does.**
- For a few parameters, plain atomics plus a **version counter** avoid recomputing expensive
  coefficients when nothing changed.
- To publish a whole object, **swap a pointer atomically** — and solve the **lifetime problem**
  with deferred deletion, generation counters, or reference counting. **Never free on the audio
  thread**; hand ownership back through a queue.
- Audio → main: **atomics with `exchange`** for metering, a ring buffer for events that must not
  be lost. The **"please free this"** message is how the audio thread releases memory.
- **Disk streaming** uses a multi-second ring buffer filled by a normal-priority thread. On
  underrun, **output silence and count it** — never block.
- Test with **thread sanitizer**, **stress tests with sequence numbers**, and **on ARM**. Prefer
  proven implementations for anything beyond SPSC.

**Next:** [Chapter 61 — Parameter Smoothing and Click-Free Automation](61-parameter-smoothing.md)
