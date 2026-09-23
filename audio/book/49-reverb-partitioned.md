# Chapter 49 — Reverb IV: Partitioned Low-Latency Convolution

> Chapter 48 left one problem: a 9-second impulse response needs a 524,288-point FFT, which means
> 11.9 seconds of latency. This chapter fixes it, and the fix is one of the more elegant pieces
> of engineering in audio — it turns an impossible operation into a few percent of a CPU core
> with zero added latency.

---

## 49.1 Uniform partitioning

Chop the impulse response into blocks of the same size as the audio callback.

```
   IR:  [==============================================================]
         │ h0 │ h1 │ h2 │ h3 │ h4 │ ... │ h_{P-1} │      P partitions
```

For each incoming audio block, convolve it with **every** partition, and add each result into the
output at the right time offset.

```
   block n arrives:
      out[n]   += block_n * h0
      out[n+1] += block_n * h1
      out[n+2] += block_n * h2
      ...
```

Equivalently, and this is how it is implemented: keep a **history of the last `P` input
spectra**, and for the current output, sum the products of each history entry with the
corresponding partition.

```cpp
class UniformPartitionedConvolver
{
public:
    void prepare(const std::vector<float>& ir, size_t blockSize)
    {
        blockSize_ = blockSize;
        fftSize_   = blockSize * 2;               // 50% overlap-save

        numPartitions_ = (ir.size() + blockSize - 1) / blockSize;

        // Pre-transform each IR partition. Done once, at load time.
        irSpectra_.resize(numPartitions_);
        for (size_t p = 0; p < numPartitions_; ++p)
        {
            std::vector<float> chunk(fftSize_, 0.0f);
            const size_t start = p * blockSize;
            for (size_t i = 0; i < blockSize && start + i < ir.size(); ++i)
                chunk[i] = ir[start + i];

            irSpectra_[p] = fftReal(chunk, fftSize_);
        }

        // Circular history of input spectra.
        history_.assign(numPartitions_, std::vector<Complex>(fftSize_,
                                                             Complex(0.0, 0.0)));
        historyIndex_ = 0;

        inputBuffer_.assign(fftSize_, 0.0f);
        accumulator_.assign(fftSize_, Complex(0.0, 0.0));
        outputTail_.assign(blockSize, 0.0f);
    }

    void process(const float* in, float* out, size_t numSamples)
    {
        // --- overlap-save: keep the previous block, append the new one ---
        std::copy(inputBuffer_.begin() + blockSize_, inputBuffer_.end(),
                  inputBuffer_.begin());
        std::copy(in, in + numSamples, inputBuffer_.begin() + blockSize_);

        // --- transform and store in the history ---
        std::vector<Complex> spectrum(fftSize_);
        for (size_t i = 0; i < fftSize_; ++i)
            spectrum[i] = Complex(inputBuffer_[i], 0.0);
        fft(spectrum, false);

        history_[historyIndex_] = std::move(spectrum);

        // --- multiply-accumulate across all partitions ---
        std::fill(accumulator_.begin(), accumulator_.end(), Complex(0.0, 0.0));

        for (size_t p = 0; p < numPartitions_; ++p)
        {
            // Walk BACKWARDS through the history: partition p pairs with
            // the input from p blocks ago.
            const size_t h = (historyIndex_ + numPartitions_ - p) % numPartitions_;

            for (size_t k = 0; k < fftSize_; ++k)
                accumulator_[k] += history_[h][k] * irSpectra_[p][k];
        }

        fft(accumulator_, true);

        // --- overlap-save: the SECOND half is the valid output ---
        for (size_t i = 0; i < numSamples; ++i)
            out[i] = static_cast<float>(accumulator_[blockSize_ + i].real());

        historyIndex_ = (historyIndex_ + 1) % numPartitions_;
    }

private:
    std::vector<std::vector<Complex>> irSpectra_, history_;
    std::vector<Complex> accumulator_;
    std::vector<float>   inputBuffer_, outputTail_;
    size_t blockSize_ = 512, fftSize_ = 1024;
    size_t numPartitions_ = 1, historyIndex_ = 0;
};
```

**Overlap-save rather than overlap-add.** With overlap-save you keep the *previous* input block
alongside the current one, transform both together, and discard the first half of the result
(which is the circular-convolution wraparound). It avoids the separate tail buffer of overlap-add
and is what most production convolvers use.

**The history is circular and walked backwards.** Partition `p` of the IR must be paired with the
input block from `p` blocks ago. The modular index arithmetic does that.

### The cost

Per block, the work is:

- 1 forward FFT: `O(N log N)`
- `P` complex multiply-accumulates over `N` bins: `O(P·N)`
- 1 inverse FFT: `O(N log N)`

**The `P·N` term dominates for long IRs.** For a 9-second cathedral at 512-sample blocks,
`P = 775` partitions. That is 775 × 1024 complex multiply-accumulates per block — about 3.2
million operations per 512 samples, or **273 million per second per channel.**

Better than direct convolution's 17.5 billion by a factor of 64, but still heavy. And the latency
is now only `blockSize` — 11.6 ms at 512 samples.

**11.6 ms is still too much** for a musician monitoring themselves through the reverb.

---

## 49.2 Non-uniform partitioning

The insight that solves it:

> **The beginning of the impulse response needs low latency. The end does not.**

Nobody can tell whether the reverb tail from four seconds ago arrives 50 ms late. But the direct
sound and early reflections must be immediate.

So use **small partitions at the start and progressively larger ones later**:

```
   IR:  [=][=][==][==][====][====][========][========][================]
         64 64 128 128  256  256      512       512          1024
         ▲                                                      ▲
      zero latency                                    large, efficient blocks
```

```
   partition sizes:   64, 64, 128, 128, 256, 256, 512, 512, 1024, 1024, 2048...
```

Each size class is a separate uniform convolver running at its own block rate, with its own
delay to place it at the right time in the IR.

**The cost falls dramatically** because the `P·N` term shrinks: large blocks mean fewer
partitions, and each large FFT is more efficient per sample than many small ones.

| Scheme | Partitions | Ops/sec (9 s IR) | Latency |
|---|---|---|---|
| Direct convolution | — | 17.5 billion | 0 |
| Uniform, 512 | 775 | 273 million | 11.6 ms |
| Uniform, 4096 | 97 | 48 million | 92.9 ms |
| **Non-uniform (64→4096)** | ~30 blocks | **~35 million** | **1.45 ms** |

**Lower cost *and* lower latency than either uniform scheme.** That is why every commercial
convolution reverb uses non-uniform partitioning (the Gardner scheme, 1995).

### The implementation shape

```cpp
class NonUniformConvolver
{
public:
    void prepare(const std::vector<float>& ir, size_t hostBlockSize)
    {
        // A typical schedule: several small blocks, then doubling.
        struct Stage { size_t blockSize; size_t count; };
        const Stage schedule[] = {
            {   64, 4 }, {  128, 4 }, {  256, 4 },
            {  512, 4 }, { 1024, 8 }, { 2048, 8 }, { 4096, 32 }
        };

        size_t offset = 0;
        for (const auto& s : schedule)
        {
            for (size_t i = 0; i < s.count && offset < ir.size(); ++i)
            {
                const size_t len = std::min(s.blockSize, ir.size() - offset);

                Segment seg;
                seg.delaySamples = offset;
                seg.convolver.prepare(
                    std::vector<float>(ir.begin() + offset,
                                       ir.begin() + offset + len),
                    s.blockSize);
                segments_.push_back(std::move(seg));

                offset += len;
            }
            if (offset >= ir.size()) break;
        }
    }

private:
    struct Segment
    {
        UniformPartitionedConvolver convolver;
        size_t delaySamples = 0;
        std::vector<float> inputFifo, outputFifo;
    };

    std::vector<Segment> segments_;
};
```

**Each segment needs its own FIFO** to accumulate enough input for its block size, and its own
output FIFO delayed by its position in the IR. That buffering is most of the implementation
complexity.

---

## 49.3 The real-time scheduling problem

Non-uniform partitioning creates a problem that is not obvious until you measure it.

**A 4096-sample FFT happens once every 4096 samples — but when it happens, it takes a long
time.** If it lands in the same audio callback as several other large FFTs, that callback
overruns its deadline and the user hears a glitch.

```
   callback:  1    2    3    4    5    6    7    8
   cost:      ▁    ▁    ▁    ▁    ▁    ▁    ▁    █████   <- spike!
                                                  ^
                                        all the big FFTs land here
```

**Two solutions, both used in practice:**

**1. Stagger the schedules.** Offset each large partition's processing so they never coincide.
Requires careful bookkeeping but adds no latency.

**2. Process large partitions on a background thread.** They have plenty of slack — a 4096-sample
partition's result is not needed for 4096 samples — so a lower-priority worker thread can compute
them while the audio thread handles the small, urgent ones. This is what most commercial
implementations do.

**The second approach needs lock-free communication** between the audio thread and the worker
(Chapter 60) and is genuinely fiddly to get right. But it is the difference between a convolution
reverb that works and one that clicks under load.

> **This is a good example of a general real-time principle** from Chapter 59: it is not the
> *average* cost that matters, it is the **worst case per callback**. An algorithm with a lower
> average but a spikier profile can be unusable where a more expensive but flatter one is fine.

---

## 49.4 Optimisations

**Real-input FFT.** Audio is real, so the spectrum is conjugate-symmetric (Chapter 25). A real
FFT halves both the FFT cost and the multiply-accumulate cost. **This is the single biggest win
available** and it is roughly a 2× speedup for a modest amount of code.

**SIMD in the multiply-accumulate.** The `P·N` loop is the hot spot and it is perfectly
vectorisable — independent complex multiplies over contiguous data. 4-wide SSE gives close to 4×;
AVX gives 8×. Chapter 63.

**Skip silent partitions.** Reverb tails decay. Partitions whose IR content is below, say,
−90 dBFS contribute nothing audible. Checking at load time and skipping them can eliminate a
large fraction of the work for a long, quiet tail:

```cpp
if (peak(partition) < dbToGain(-90.0)) { skip = true; }
```

For a 9-second IR where the last three seconds are near the noise floor, this can cut the cost by
a third.

**Frequency-domain storage.** Keep the input history as spectra rather than re-transforming.
Already done above, and it is essential — transforming once per block rather than once per
partition is the whole point.

**Shared input spectra across channels.** In a true-stereo convolver (Chapter 48), the same input
block is convolved with four IRs. Transform each input channel once and reuse its spectrum for
both of its IRs. Saves two FFTs out of six.

---

## 49.5 Velvet noise: a cheaper alternative

Chapter 15 mentioned velvet noise. Here is where it pays off.

**Velvet noise** is mostly zeros with occasional ±1 impulses at randomised positions. Perceptually
it sounds like smooth noise at surprisingly low densities (around 2,000 impulses per second).

**Convolving with a sparse signal is cheap**, because you skip all the zeros:

```cpp
// Convolution with velvet noise: only the non-zero taps cost anything.
float process(float x)
{
    delay_.write(x);

    double sum = 0.0;
    for (const auto& tap : taps_)              // maybe 2000 taps for 1 second
        sum += delay_.read(tap.delaySamples) * tap.sign * tap.gain;

    return static_cast<float>(sum);
}
```

A 1-second velvet reverb at 2,000 impulses per second is **2,000 multiply-accumulates per
sample** — compared with 44,100 for dense convolution. A **22× saving**, with no FFT, no latency
and no scheduling spikes.

**The trade-off:** you cannot reproduce a *specific* measured room, because velvet noise is
synthetic. You can, however, shape its density and decay envelope to match a target, and the
result is a smooth, natural-sounding late tail.

**Where this is used:** as the **late tail** in a hybrid reverb (Chapter 48's §48.6), replacing
the FDN. Convolve the measured early reflections accurately, then hand off to velvet-noise
convolution for the tail. You get real room identity, a smooth tail, low CPU and no scheduling
problems.

---

## 49.6 Choosing a reverb architecture

Four chapters, four methods. The decision table:

| Need | Use | Chapter |
|---|---|---|
| Lowest CPU, adjustable | Schroeder / Freeverb | 46 |
| Best algorithmic quality, full parameter control | **FDN** | 47 |
| A specific real space, offline | Direct/FFT convolution | 48 |
| A specific real space, real time | **Non-uniform partitioned** | 49 |
| Real room identity + adjustable tail + low CPU | **Hybrid: convolved ER + FDN or velvet tail** | 48/49 |
| Guitar cabinet, short IR | Single-block FFT convolution | 48 |
| Games, many simultaneous spaces | FDN with shared tails | 47 |
| Cinematic mixing | **Hybrid**, usually true-stereo | 48/49 |

**For Chapter 91's cinematic engine the answer is the hybrid**, and this table is why: it is the
only option that gives real spatial identity, adjustable decay, modulation for smoothness, and a
CPU cost that allows several simultaneous spaces.

---

## 49.7 Exercises

**49.1** Implement uniform partitioned convolution. Verify it matches offline `fastConvolve`
exactly.

**49.2** Measure the latency of your uniform convolver by sending an impulse and finding when it
appears.

**49.3** Count the operations per second for a 4-second IR at block sizes 128, 512 and 2048.
Confirm the trade-off between cost and latency.

**49.4** Implement non-uniform partitioning with the schedule in §49.2. Measure both latency and
CPU cost, and compare with uniform.

**49.5** Instrument your convolver to record per-callback processing time over 10,000 callbacks.
Plot the distribution. Do you see the spikes from §49.3?

**49.6** Implement partition skipping for silent partitions. Measure the saving on a long IR with
a quiet tail.

**49.7** Implement a real-input FFT and measure the speedup in the convolver.

**49.8** Build a velvet-noise reverb at 1,000, 2,000 and 4,000 impulses per second. At what
density does it stop sounding sparse?

**49.9** Compare velvet-noise convolution with dense convolution of the same decay envelope.
Measure both quality (by ear) and CPU.

**49.10** Build the full hybrid: convolve the first 120 ms of a real IR with a single-block FFT
convolver, and generate the tail with velvet noise. Crossfade between them. Compare with full
convolution for both quality and cost.

---

### Chapter summary

- **Uniform partitioning** chops the IR into callback-sized blocks and keeps a history of input
  spectra. Latency drops to one block, but the `P·N` multiply-accumulate term still dominates.
- Use **overlap-save** rather than overlap-add: keep the previous input block, transform both,
  discard the first half of the result.
- **Non-uniform partitioning** uses small blocks at the start (low latency) and progressively
  larger ones later (efficiency). It achieves **both lower cost and lower latency** than any
  uniform scheme — ~1.45 ms and ~35 M ops/sec for a 9-second IR.
- Large partitions create **per-callback cost spikes**. Either stagger their schedules or process
  them on a **background thread**, which needs lock-free communication (Chapter 60). **Worst-case
  cost per callback matters more than average cost.**
- Optimisations in order of value: **real-input FFT** (~2×), **SIMD** on the MAC loop (4–8×),
  **skip silent partitions**, and share input spectra across channels.
- **Velvet noise** — sparse ±1 impulses — convolves ~22× cheaper than dense audio because the
  zeros cost nothing. It cannot reproduce a specific room, but it makes an excellent **late
  tail**.
- **The hybrid** — accurately convolved early reflections plus an FDN or velvet-noise tail — is
  the right architecture for cinematic work: real room identity, adjustable decay, smooth
  modulated tail, and affordable CPU.

**Next:** [Chapter 50 — Dynamics I: Envelope Followers and Compressors](50-dynamics-compressor.md)
