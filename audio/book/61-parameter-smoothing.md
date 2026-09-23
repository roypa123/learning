# Chapter 61 — Parameter Smoothing and Click-Free Automation

> Chapter 13 established that any instantaneous change produces broadband energy. A parameter
> that jumps is such a change. This short chapter is about making every control in your software
> move smoothly, which is the difference between software that feels professional and software
> that clicks.

---

## 61.1 The problem

```cpp
// GUI thread moves a fader.
gain.store(0.8f);

// Audio thread, next block:
for (int i = 0; i < n; ++i)
    out[i] = in[i] * gain.load();      // JUMPS from 0.2 to 0.8 at the block boundary
```

A gain change from 0.2 to 0.8 at one sample boundary is a step of 0.6 × the signal value. That is
a discontinuity, which is a click.

**And it happens at the block rate**, so continuous automation produces clicking at
`sampleRate/blockSize` Hz — 86 Hz at 512 frames, which is an audible buzz. This is
**zipper noise**, and it is one of the most recognisable signs of amateur audio software.

---

## 61.2 The one-pole smoother

The same filter you have used since Chapter 13.

```cpp
class SmoothedValue
{
public:
    void setSampleRate(double sr, double smoothingTimeMs = 20.0)
    {
        sr_ = sr;
        setSmoothingTime(smoothingTimeMs);
    }

    void setSmoothingTime(double ms)
    {
        coeff_ = (ms <= 0.0) ? 0.0f
               : static_cast<float>(std::exp(-1.0 / (ms * 0.001 * sr_)));
    }

    void setTarget(float t)  { target_ = t; }

    // Jump immediately -- for initialisation and preset loads.
    void setImmediate(float v) { target_ = v; current_ = v; }

    float next()
    {
        current_ = target_ + (current_ - target_) * coeff_;

        // Snap when close, both to finish cleanly and to avoid denormals
        // (Chapter 59).
        if (std::fabs(current_ - target_) < 1e-6f) current_ = target_;

        return current_;
    }

    float current() const   { return current_; }
    bool  isSmoothing() const { return current_ != target_; }

private:
    float  current_ = 0.0f, target_ = 0.0f, coeff_ = 0.0f;
    double sr_ = kDefaultRate;
};
```

Used per sample:

```cpp
for (int i = 0; i < n; ++i)
    out[i] = in[i] * gain_.next();
```

**One multiply and one add per sample**, and the click is gone.

### Choosing the time

| Parameter | Smoothing time | Why |
|---|---|---|
| **Gain / volume** | 10–50 ms | Fast enough to feel responsive |
| **Pan** | 20–50 ms | |
| **Filter cutoff** | 20–100 ms | Longer, because coefficient changes are more audible |
| **Delay time** | 100–500 ms | Or you get pitch artefacts (Chapter 42) |
| **Dry/wet mix** | 20–50 ms | |
| **Anything in a feedback loop** | 100 ms+ | Fast changes can destabilise |

**The exponential smoother never quite arrives**, which is why the snap threshold exists. It also
means the *perceived* time is shorter than the time constant: after one time constant you are 63%
of the way, after three you are 95%.

---

## 61.3 Linear ramping

Sometimes you want to arrive exactly, in a known time:

```cpp
class LinearRamp
{
public:
    void rampTo(float target, int numSamples)
    {
        target_ = target;
        remaining_ = numSamples;
        step_ = (numSamples > 0) ? (target - current_) / numSamples : 0.0f;
    }

    float next()
    {
        if (remaining_ <= 0) return target_;
        current_ += step_;
        --remaining_;
        return current_;
    }

private:
    float current_ = 0.0f, target_ = 0.0f, step_ = 0.0f;
    int   remaining_ = 0;
};
```

**Exponential versus linear:**

| | Exponential | Linear |
|---|---|---|
| Arrives exactly | No (asymptotic) | **Yes** |
| Natural for level | **Yes** (matches perception) | No |
| Cost | 2 ops | 2 ops |
| Overshoot | Never | Never |
| Best for | Gain, filter cutoff, most things | Crossfades, fixed-length fades, block-boundary ramps |

**Use linear for crossfades** where the two halves must sum correctly, and exponential for
everything else.

---

## 61.4 Block-rate ramping

Smoothing per sample costs two operations per parameter per sample. With fifty parameters that is
a real cost.

**The alternative: compute the parameter once per block, and linearly ramp between the previous
block's value and this one's.**

```cpp
void processBlock(float* out, const float* in, int numFrames)
{
    const float targetGain = gainParam_.load(std::memory_order_relaxed);

    if (targetGain == lastGain_)
    {
        // No change: a simple loop, no ramping cost at all.
        for (int i = 0; i < numFrames; ++i)
            out[i] = in[i] * targetGain;
    }
    else
    {
        // Ramp across the block.
        const float step = (targetGain - lastGain_) / numFrames;
        float g = lastGain_;

        for (int i = 0; i < numFrames; ++i)
        {
            out[i] = in[i] * g;
            g += step;
        }

        lastGain_ = targetGain;
    }
}
```

**The fast path when nothing changed is the point.** Most parameters are static most of the time,
so the branch is almost always taken and the cost is zero.

**The limitation:** a block-rate ramp reaches its target in one block, which at 512 frames is
11.6 ms. For a fader that is fine. For a parameter that needs 100 ms of smoothing, chain the two
approaches: smooth the target at block rate, then ramp to the smoothed value within the block.

---

## 61.5 Filter coefficients

Filter parameters are the hard case, and Chapter 23 flagged it.

**You cannot simply smooth the coefficients.** Interpolating between two stable coefficient sets
can pass through an unstable region (Chapter 24's stability triangle), and the filter explodes.

**Three correct approaches:**

**1. Smooth the *parameters*, recompute the coefficients.**

```cpp
// The smoothed frequency and Q are always in the valid range, so the
// computed coefficients are always stable.
const double f = cutoffSmoothed_.next();
const double q = resonanceSmoothed_.next();

if (++counter_ >= 16)
{
    counter_ = 0;
    coeffs_ = BiquadCoeffs::design(type_, f, sr_, q);
}
```

**Safe, and the standard approach.** Recomputing every 16 samples is the Chapter 33 compromise.

**2. Use a topology that modulates well.** The TPT state-variable filter (Chapter 33) has
coefficients that map directly to frequency and stay stable under any modulation rate. This is
why synth filters are SVFs rather than biquads.

**3. Crossfade between two filter instances.** Run the old and new coefficient sets in parallel
and crossfade over a few milliseconds. Twice the cost, and completely safe for arbitrary
coefficient jumps — useful when loading presets.

---

## 61.6 Sample-accurate automation

A DAW's automation curve is not a sequence of block-rate values; it has a value at every sample.
Reproducing that faithfully requires events *within* the block.

```cpp
struct ParameterEvent
{
    int   sampleOffset;      // position within this block
    int   parameterId;
    float value;
};

void processBlock(AudioBuffer& buffer, const std::vector<ParameterEvent>& events)
{
    int position = 0;
    size_t eventIndex = 0;

    while (position < buffer.numFrames())
    {
        // How far to the next event?
        int nextEvent = buffer.numFrames();
        if (eventIndex < events.size())
            nextEvent = events[eventIndex].sampleOffset;

        const int chunk = nextEvent - position;

        // Process up to the event with the current parameters.
        if (chunk > 0)
        {
            processChunk(buffer, position, chunk);
            position += chunk;
        }

        // Apply every event at this position.
        while (eventIndex < events.size()
               && events[eventIndex].sampleOffset == position)
        {
            applyParameter(events[eventIndex]);
            ++eventIndex;
        }
    }
}
```

**This "split the block at each event" pattern** is how plugins handle sample-accurate automation
and sample-accurate MIDI (Chapter 62). It is the same structure in both cases.

**The cost** is that very dense automation produces many tiny chunks, and per-chunk overhead
dominates. Most hosts and plugins therefore quantise automation to a minimum chunk size — 32 or
64 samples — which is inaudible and bounds the worst case.

---

## 61.7 Where smoothing is not enough

Some parameter changes cannot be smoothed and need a different strategy.

**Changing a delay time** produces either a pitch bend (if smoothed — Chapter 42's tape
behaviour) or a discontinuity (if jumped). Neither is "no artefact". The clean solution is a
**crossfade between two read positions** over 10–30 ms.

**Changing the FFT size** in a spectral processor cannot be smoothed at all. Crossfade between
two instances, or accept a brief gap.

**Changing an impulse response** in a convolution reverb: crossfade between two convolvers, which
doubles the cost during the transition.

**Switching a wavetable frame** past the mipmap boundary: Chapter 32's crossfade.

**Loading a preset** changes everything at once. The professional approach is to fade the output
to silence over ~10 ms, apply the changes, and fade back in. The brief gap is far less
objectionable than the simultaneous discontinuity in every parameter.

**The general principle:** when a parameter cannot change continuously, **crossfade between two
instances of the thing**. It costs double for the duration of the transition and it always works.

---

## 61.8 Exercises

**61.1** Build a gain control with no smoothing. Automate it with a 5 Hz square wave and listen to
the clicks.

**61.2** Add the one-pole smoother. Find the shortest smoothing time at which the clicks become
inaudible.

**61.3** Measure the zipper noise frequency with no smoothing at block sizes 128, 512 and 2048.
Does it match `sampleRate/blockSize`?

**61.4** Compare exponential and linear smoothing on a gain automated from 0 to 1 over 100 ms.
Plot both curves.

**61.5** Implement block-rate ramping with the no-change fast path. Measure the cost difference
between a static and a moving parameter.

**61.6** *Deliberate breakage.* Interpolate biquad coefficients directly between a 50 Hz Q=10
low-pass and a 15 kHz Q=10 low-pass. Check `isStable()` at each step of the interpolation.

**61.7** Implement all three filter-smoothing approaches from §61.5 and compare them on a fast
cutoff sweep.

**61.8** Build the sample-accurate automation splitter. Test it with 10 events at random offsets
within a 512-sample block.

**61.9** Add a minimum chunk size of 32 samples and measure the cost difference on dense
automation.

**61.10** Implement preset loading with a fade-out/fade-in. Compare with instant switching on a
patch where every parameter differs.

---

### Chapter summary

- A parameter that jumps is a discontinuity, which is a click. At the block rate this becomes
  **zipper noise** at `sampleRate/blockSize` Hz — one of the clearest signs of amateur audio
  software.
- The **one-pole smoother** fixes it for two operations per sample. **Snap when close**, both to
  finish cleanly and to avoid denormals.
- Smoothing times: 10–50 ms for gain and pan, 20–100 ms for filter cutoff, 100 ms+ for delay time
  and anything in a feedback loop.
- **Linear ramps arrive exactly** and are right for crossfades; **exponential is right for
  everything else** because it matches perception.
- **Block-rate ramping** with a **fast path when nothing changed** costs nothing for static
  parameters, which is most of them most of the time.
- **Never interpolate filter coefficients directly** — the path between two stable sets can be
  unstable. Smooth the *parameters* and recompute, use a **TPT SVF** that modulates safely, or
  crossfade two filter instances.
- **Sample-accurate automation** splits the block at each event. Quantise to a minimum chunk size
  (32–64 samples) to bound the worst case.
- When a parameter **cannot** change continuously — delay time, FFT size, impulse response,
  preset load — **crossfade between two instances**. Double cost for the duration, and it always
  works.

**Next:** [Chapter 62 — MIDI Input](62-midi-input.md)
