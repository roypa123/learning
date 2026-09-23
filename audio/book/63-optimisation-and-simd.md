# Chapter 63 — Optimisation: Profiling, SIMD, and Cache

> Chapter 56 established that the worst case is what matters. This chapter is about making it
> smaller — in the right order, which is almost never the order people guess.

---

## 63.1 Measure first

The two rules that save the most time:

> **1. Never optimise without measuring.**
> **2. The bottleneck is never where you think it is.**

Audio code is unusually prone to misdirected optimisation because the DSP *looks* like the
expensive part. Frequently it is not: memory access, cache misses, denormals and control-rate
recomputation dominate more often than arithmetic does.

### Timing a block

```cpp
class ScopedTimer
{
public:
    explicit ScopedTimer(std::atomic<double>& accumulator)
        : acc_(accumulator), start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer()
    {
        const auto elapsed = std::chrono::steady_clock::now() - start_;
        const double us = std::chrono::duration<double, std::micro>(elapsed).count();

        // Accumulate the MAXIMUM, not the average.
        double prev = acc_.load(std::memory_order_relaxed);
        while (us > prev && !acc_.compare_exchange_weak(prev, us)) {}
    }

private:
    std::atomic<double>& acc_;
    std::chrono::steady_clock::time_point start_;
};
```

**Accumulating the maximum** with a compare-exchange loop is the right measurement for real-time
code, and it is why this is not just `acc_ += us`.

### Profilers

| Tool | Platform | Notes |
|---|---|---|
| **Visual Studio Profiler** | Windows | Built in; sampling and instrumentation |
| **Intel VTune** | Windows/Linux | Best for cache and vectorisation analysis |
| **perf** | Linux | `perf record` / `perf report`; excellent and free |
| **Instruments** | macOS | Time Profiler, plus Counters for cache |
| **Tracy** | All | **Real-time frame profiler — ideal for audio** |

**Tracy deserves the recommendation** for audio work specifically: it is designed for per-frame
profiling in real-time systems, has very low overhead, and shows you a timeline of every callback
rather than an aggregate.

**A caution about sampling profilers:** they show where time is spent on *average*. For audio you
also need the *distribution*, because one callback in a thousand taking 10× as long is invisible
in an average but is the bug. Instrument the callback yourself and record a histogram.

---

## 63.2 The optimisation hierarchy

In order of typical payoff. **Do not skip levels.**

**1. Algorithm.** `O(N log N)` beats `O(N²)`. Chapter 25's FFT convolution was 75× faster than
direct convolution — no amount of SIMD would have closed that gap.

**2. Do less work.**
   - Skip inactive voices (Chapter 41: the single biggest win in a synth)
   - Skip silent partitions (Chapter 49)
   - Skip disabled EQ bands (Chapter 53)
   - Control-rate instead of audio-rate (Chapter 33: every 16 samples, not every sample)
   - Bypass entirely when a processor is at unity

**3. Memory layout.** Cache misses cost 100–300 cycles. An L1 hit costs 4. Getting the layout
right is often worth more than vectorising the arithmetic.

**4. Vectorisation (SIMD).** 4–8× on the arithmetic, *if* memory is not the bottleneck.

**5. Micro-optimisation.** Reordering instructions, avoiding branches. Usually the compiler is
better at this than you are.

**The common mistake is starting at 4 or 5.** Vectorising a loop that is waiting on memory gains
nothing.

---

## 63.3 Cache

The numbers that govern everything:

| Level | Size | Latency |
|---|---|---|
| L1 data | 32–48 KB | ~4 cycles |
| L2 | 256 KB–2 MB | ~12 cycles |
| L3 | 8–32 MB | ~40 cycles |
| RAM | — | **~200–300 cycles** |

**An L3 miss costs the same as roughly 100 multiplications.** That is the fact that makes memory
layout matter more than arithmetic.

### Structure of Arrays

```cpp
// Array of Structures -- BAD for bulk processing
struct Voice { float phase, freq, amp, filterState1, filterState2; };
std::vector<Voice> voices;

for (auto& v : voices) v.phase += v.freq;
// Each iteration touches 20 bytes but uses 8. The cache line carries
// filter state you do not need.

// Structure of Arrays -- GOOD
struct VoiceBank
{
    std::vector<float> phase, freq, amp, filterState1, filterState2;
};

for (size_t i = 0; i < n; ++i) phase[i] += freq[i];
// Contiguous, uses every byte fetched, and VECTORISES automatically.
```

**SoA is the single most effective structural change** for bulk audio processing, and it is a
prerequisite for useful SIMD — you cannot vectorise a loop over scattered fields.

**The trade-off** is that SoA is less pleasant to write and reason about. The pragmatic approach:
AoS for objects that are processed individually (a filter, a delay line), SoA for banks of
identical things processed together (voices, partials, grains).

### Block processing

```cpp
// Sample-at-a-time through a chain: each sample makes the full trip
// through every processor's state, evicting the previous one's data.
for (int i = 0; i < n; ++i)
{
    float s = osc.next();
    s = filter.process(s);
    s = delay.process(s);
    out[i] = s;
}

// Block-at-a-time: each processor's state stays hot for its whole pass.
osc.process(buffer, n);
filter.process(buffer, n);
delay.process(buffer, n);
```

**Block processing is typically 2–4× faster** for a chain of processors, and it is what makes
vectorisation possible. The cost is one buffer of intermediate storage per stage — cheap.

### Alignment and prefetching

```cpp
// 32-byte alignment for AVX. Misaligned SIMD loads are slower, and on
// some older hardware they fault.
alignas(32) std::array<float, 512> buffer;
```

**Prefetching** is rarely worth it for audio, because the access patterns are already sequential
and the hardware prefetcher handles them well. It is worth trying only for genuinely irregular
access — a large wavetable indexed unpredictably, for instance.

---

## 63.4 SIMD

**Single Instruction, Multiple Data**: one instruction operating on 4, 8 or 16 values at once.

| Instruction set | Width (float) | Availability |
|---|---|---|
| SSE2 | 4 | Every x86-64 CPU |
| AVX | 8 | 2011+ |
| AVX-512 | 16 | Server/high-end; **can downclock the core** |
| NEON | 4 | All ARM |

### Let the compiler do it first

```cpp
// This vectorises automatically with -O3 on any modern compiler.
void applyGain(float* buffer, int n, float gain)
{
    for (int i = 0; i < n; ++i)
        buffer[i] *= gain;
}
```

**Check whether it did:**

```bash
g++ -O3 -march=native -fopt-info-vec-optimized -c file.cpp
```

Modern compilers auto-vectorise simple loops very well. **Write clear code and check the report
before writing intrinsics.**

**What prevents auto-vectorisation:**

- Possible pointer aliasing (fix with `__restrict`)
- Loop-carried dependencies (a filter's feedback — genuinely unvectorisable across samples)
- Branches inside the loop
- Function calls that are not inlined
- Non-contiguous access

### Explicit intrinsics

When auto-vectorisation fails and the loop matters:

```cpp
#include <immintrin.h>

void applyGainAVX(float* __restrict buffer, int n, float gain)
{
    const __m256 g = _mm256_set1_ps(gain);

    int i = 0;
    for (; i + 8 <= n; i += 8)
    {
        __m256 v = _mm256_loadu_ps(buffer + i);
        v = _mm256_mul_ps(v, g);
        _mm256_storeu_ps(buffer + i, v);
    }

    // The tail: handle the remaining 0-7 samples scalar.
    for (; i < n; ++i)
        buffer[i] *= gain;
}
```

**The tail loop is mandatory.** Audio buffer sizes are usually multiples of 8, but not always —
Chapter 57 established that `frameCount` can be anything.

**`__restrict` promises the pointers do not alias**, which lets the compiler avoid defensive
reloads. It is a promise you must keep; violating it is undefined behaviour.

### What vectorises and what does not

| Operation | Vectorises? |
|---|---|
| Gain, mixing, panning | **Yes, trivially** |
| FIR filters | **Yes** (with care over the delay line) |
| Oscillator banks (SoA) | **Yes** |
| FFT butterflies | **Yes** |
| Complex multiply-accumulate (convolution) | **Yes — the biggest win in Chapter 49** |
| **IIR filters across samples** | **No** — each output depends on the previous |
| IIR across **channels or voices** | **Yes** — process 4 voices' filters in parallel |

**The IIR case is the important one.** You cannot vectorise a biquad along the time axis, because
`y[n]` needs `y[n-1]`. But you *can* process four independent biquads — four voices, or four
channels — simultaneously. That is how SIMD helps synthesisers.

### Runtime dispatch

```cpp
// Compiling with -march=native produces a binary that crashes on older
// CPUs. For shipped software, detect at runtime.
using ProcessFn = void(*)(float*, int, float);

ProcessFn selectImplementation()
{
    if (cpuSupportsAVX2()) return applyGainAVX2;
    if (cpuSupportsSSE2()) return applyGainSSE2;
    return applyGainScalar;
}
```

**Select once at startup**, not per call — a function-pointer call per sample would cost more
than it saves.

---

## 63.5 Lookup tables

Replacing computation with memory, which is only a win when the computation is expensive and the
table is small enough to stay cached.

```cpp
// A sine table -- Chapter 32's wavetable, in effect.
class SineTable
{
public:
    SineTable()
    {
        for (size_t i = 0; i < kSize + 1; ++i)      // +1 for interpolation
            table_[i] = std::sin(kTwoPi * i / kSize);
    }

    float lookup(double phase) const                // phase 0..1
    {
        const double idx = phase * kSize;
        const size_t i0  = static_cast<size_t>(idx);
        const double frac = idx - i0;
        return static_cast<float>(table_[i0] * (1.0 - frac) + table_[i0 + 1] * frac);
    }

private:
    static constexpr size_t kSize = 4096;
    std::array<float, kSize + 1> table_;            // 16 KB -- fits in L1
};
```

**When a table wins:**

| Function | Direct cost | Table worth it? |
|---|---|---|
| `sin`, `cos` | ~20–40 cycles | **Yes** |
| `exp`, `log`, `pow` | ~30–50 cycles | **Yes** |
| `tanh` | ~40–60 cycles | **Yes** |
| `sqrt` | ~15 cycles (hardware) | No |
| Multiply, add | 1–4 cycles | Never |

**The size limit is the cache.** A 4,096-entry float table is 16 KB, which fits comfortably in
L1. A 65,536-entry table is 256 KB, which does not, and every lookup may then be an L2 miss —
at which point computing `sin` directly is faster.

**With many voices reading different tables**, the aggregate working set matters more than any
individual table's size. This is a real effect in large synths, and it is why sharing tables
between voices is worth doing.

---

## 63.6 Realistic expectations

What the levels actually buy, on typical audio code:

| Optimisation | Typical speedup |
|---|---|
| Better algorithm | **10–1000×** |
| Skipping unnecessary work | **2–10×** |
| Block instead of per-sample | 2–4× |
| SoA instead of AoS | 1.5–3× |
| SIMD (when memory-bound) | 1.1–1.5× |
| SIMD (when compute-bound) | **3–7×** |
| Lookup tables for transcendentals | 2–5× |
| Micro-optimisation | 1.0–1.2× |

**The spread between the top and bottom rows is three orders of magnitude**, and it is why the
hierarchy in §63.2 is ordered as it is.

**Compiler flags matter too**, and they are free:

| Flag | Effect | Caution |
|---|---|---|
| `-O2` | The baseline. **Never ship without it** | — |
| `-O3` | More aggressive; enables more vectorisation | Occasionally slower |
| `-march=native` | Use this CPU's instructions | **Crashes on older CPUs** |
| `-ffast-math` | Relax IEEE rules | **Breaks NaN handling and can change filter behaviour** |
| `-funroll-loops` | Unroll | Larger code, may hurt cache |
| LTO (`-flto`) | Cross-file inlining | Slower builds, usually worth it |

**`-ffast-math` deserves a specific warning.** It allows the compiler to assume no NaNs and no
infinities, and to reassociate floating-point operations. In audio that can change a filter's
numerical behaviour and can break the `isfinite` guards from Chapter 24. Use it only if you have
measured a benefit and tested thoroughly.

---

## 63.7 Exercises

**63.1** Instrument your synth's callback and record a histogram of durations over 10,000
callbacks. Where is the long tail?

**63.2** Profile a full effects chain. Which processor actually dominates? Was it the one you
expected?

**63.3** Convert a voice bank from AoS to SoA. Measure the speedup at 8, 32 and 128 voices.

**63.4** Convert a processing chain from per-sample to per-block. Measure.

**63.5** Compile a gain loop with `-O3 -fopt-info-vec-optimized`. Did it vectorise? Now add a
branch inside the loop and check again.

**63.6** Write the AVX gain function with a scalar tail. Verify it matches the scalar version
bit-for-bit and measure the speedup.

**63.7** Measure `sin` against a 4,096-entry interpolated table. Then try a 1,048,576-entry table
and explain the result.

**63.8** Vectorise four independent biquads processing four voices simultaneously. Compare with
four scalar biquads.

**63.9** Try `-ffast-math` on a resonant filter at high Q. Compare the output bit-for-bit with
and without.

**63.10** Implement runtime SIMD dispatch and verify the correct path is chosen on your machine.

---

### Chapter summary

- **Measure first, and the bottleneck is never where you think.** Record the **maximum** and the
  **distribution**, not just the average — one callback in a thousand taking 10× is invisible in
  an average and is the bug.
- **The hierarchy, in order:** algorithm → do less work → memory layout → SIMD →
  micro-optimisation. The spread between top and bottom is three orders of magnitude.
- **Cache misses cost 100–300 cycles** — about 100 multiplications. Layout beats arithmetic.
- **Structure of Arrays** for banks of identical things; it is also a prerequisite for useful
  SIMD. **Block processing** rather than per-sample is worth 2–4× for a chain.
- Let the **compiler auto-vectorise** and check with `-fopt-info-vec-optimized` before writing
  intrinsics. Aliasing, branches and loop-carried dependencies prevent it.
- **IIR filters cannot vectorise across time** (each output needs the previous), but they
  **can vectorise across voices or channels** — which is how SIMD helps a synthesiser.
- **Always write the scalar tail loop**; buffer sizes are not guaranteed to be multiples of 8.
- **Lookup tables** win for `sin`, `exp`, `tanh` — provided the table stays in L1. A table too
  large to cache is slower than computing directly.
- `-O2` always; `-march=native` crashes on older CPUs; **`-ffast-math` can break NaN guards and
  change filter behaviour**.

**Next:** [Chapter 64 — Graph-Based Audio Engines](64-audio-graphs.md)
