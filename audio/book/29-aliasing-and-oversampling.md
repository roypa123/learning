# Chapter 29 — Aliasing In Depth, and Oversampling

> Chapter 4 explained aliasing. Chapter 12 let you hear it. This chapter closes Part II by
> dealing with the case that matters most in practice and is least often handled correctly:
> **aliasing that your own nonlinear processing creates**, and the standard cure for it.

---

## 29.1 Two sources of aliasing

**Source 1: sampling something that was not band-limited.** Chapter 4's case. Solved by the
anti-alias filter in every converter. Not your problem — unless you are generating the signal
yourself, in which case Chapter 12's naive oscillators are exactly this failure.

**Source 2: nonlinear processing creating new frequencies above Nyquist.** This is entirely your
problem, there is no converter to protect you, and it is where most real audio-software aliasing
comes from.

Chapter 20 established the rule:

> **A linear system cannot create new frequencies, so it cannot alias. A nonlinear system always
> creates new frequencies, so it always can.**

That is the whole map. Every processor in your chain is on one side of it:

| Process | Linear? | Can alias? |
|---|---|---|
| Gain, mixing, panning | ✓ | No |
| EQ, filters (fixed coefficients) | ✓ | No |
| Delay, reverb, convolution | ✓ | No |
| **Distortion, saturation, clipping** | ✗ | **Yes** |
| **Waveshaping, bit-crushing** | ✗ | **Yes** |
| **Compression, limiting** | ✗ | **Yes** (at the gain-change transients) |
| **Rectification, ring modulation** | ✗ | **Yes** |
| **Naive oscillators** | — | **Yes** (they generate unlimited bandwidth) |
| Modulated filters, chorus, tremolo | time-varying | Mildly — sidebands can exceed Nyquist |

---

## 29.2 How much aliasing does a nonlinearity make?

Work it out for a concrete case, because the numbers are worse than people expect.

A polynomial waveshaper of order `k` generates harmonics up to the `k`th:

| Shaper | Harmonics generated |
|---|---|
| `y = x²` | 2nd |
| `y = x³` | 3rd (and 1st) |
| `y = tanh(x)` | odd harmonics, decaying — effectively unlimited |
| Hard clip | odd harmonics at `1/h` — **unlimited** |
| Bit crush | very high order — **unlimited** |

Feed a 5 kHz sine into a hard clipper at 44.1 kHz:

| Harmonic | Frequency | Above Nyquist (22,050)? | Appears at |
|---|---|---|---|
| 1st | 5,000 | no | 5,000 |
| 3rd | 15,000 | no | 15,000 |
| 5th | 25,000 | **yes** | 44,100 − 25,000 = **19,100** |
| 7th | 35,000 | **yes** | 44,100 − 35,000 = **9,100** |
| 9th | 45,000 | **yes** | 45,000 − 44,100 = **900** |
| 11th | 55,000 | **yes** | 55,000 − 44,100 = **10,900** |
| 13th | 65,000 | **yes** | 65,000 − 44,100 = **20,900** |

Five aliased components, at 900, 9,100, 10,900, 19,100 and 20,900 Hz. **None of them is a
harmonic of 5 kHz.** The 900 Hz one in particular is more than two octaves below the input and
utterly unrelated to it musically.

This is why a distortion plugin can sound fine on a bass guitar and horrible on a bright synth
lead: the bass's harmonics mostly fit under Nyquist; the lead's do not.

**And it gets worse with chords.** Two input frequencies produce not just harmonics but
**intermodulation products** at every sum and difference — `2f1 − f2`, `f1 + 2f2`, and so on.
Those alias too, and there are far more of them.

---

## 29.3 Oversampling: the standard cure

The idea, in one line: **move Nyquist further away while the nonlinearity is doing its damage.**

```
   1. UPSAMPLE by L (typically 2, 4, 8 or 16)
        - insert L-1 zeros between samples
        - low-pass filter at the ORIGINAL Nyquist to remove images
        - multiply by L

   2. APPLY THE NONLINEARITY at the high rate
        - harmonics now have L times as much room before they fold

   3. DOWNSAMPLE by L
        - low-pass filter at the ORIGINAL Nyquist  <- THIS removes the
          harmonics that would have aliased
        - keep every Lth sample
```

```
   Before (no oversampling):          After (4x oversampling):

   |<-- audio -->|                    |<-- audio -->|
   0          22.05k                  0          22.05k            88.2k
   ###############|xxxxx              ###############|~~~~~~~~~~~~~~~|
                  ^                                  ^
              harmonics fold                    harmonics live here,
              back into here                    then get filtered off
```

The harmonics that would have folded down are now legitimately representable at the higher rate,
and the downsampling filter **removes** them instead of letting them alias.

### How much oversampling?

| Factor | New Nyquist (from 44.1k) | Harmonics captured | Typical use |
|---|---|---|---|
| 2× | 44.1 kHz | up to the 8th of a 5 kHz tone | Gentle saturation |
| 4× | 88.2 kHz | up to the 17th | Standard for distortion |
| 8× | 176.4 kHz | up to the 35th | Hard clipping, aggressive shapers |
| 16× | 352.8 kHz | up to the 70th | Bit-crushers, extreme waveshaping |

**Oversampling never eliminates aliasing entirely** — the nonlinearity still generates content
above the *new* Nyquist. It reduces it, and the reduction is roughly proportional to how fast the
harmonics decay. For `tanh` (harmonics falling quickly) 2× is often enough. For hard clipping
(harmonics falling as `1/h`) even 16× leaves measurable aliasing.

**The rule of thumb:** the harsher the nonlinearity, the more oversampling it needs, and hard
clipping is the worst case. This is one genuine reason soft clipping is preferred — it is not
only gentler-sounding, it is *cheaper to do correctly*.

---

## 29.4 Implementation

**Code — `lib/include/audio/oversample.h`** (core)

```cpp
namespace audio {

// Upsample by L, run a per-sample function at the high rate, downsample by L.
class Oversampler
{
public:
    void prepare(int factor, double sampleRate, int quality = 64)
    {
        factor_ = std::max(1, factor);
        if (factor_ == 1) return;

        // One filter design serves both directions. Cutoff slightly below
        // the ORIGINAL Nyquist, with a little transition room.
        const double cutoff = sampleRate * 0.5 * 0.90;
        const double highRate = sampleRate * factor_;

        coeffs_ = designLowPass(cutoff, highRate,
                                static_cast<size_t>(quality * factor_),
                                WindowType::Kaiser, kaiserBeta(90.0));

        up_.setCoefficients(coeffs_);
        down_.setCoefficients(coeffs_);
        reset();
    }

    void reset() { up_.reset(); down_.reset(); }

    // fn is applied at the high rate.
    template <typename Fn>
    float processSample(float x, Fn&& fn)
    {
        if (factor_ == 1)
            return fn(x);

        float result = 0.0f;

        for (int i = 0; i < factor_; ++i)
        {
            // Zero-stuff: the real sample first, then L-1 zeros.
            // Multiply by L to restore the energy the zeros removed.
            const float in = (i == 0) ? x * static_cast<float>(factor_) : 0.0f;

            const float upsampled = up_.processSample(in, 0);

            const float shaped = fn(upsampled);

            const float filtered = down_.processSample(shaped, 0);

            // Keep only the first of every L samples.
            if (i == 0) result = filtered;
        }

        return result;
    }

    int latencySamples() const
    {
        return (factor_ == 1) ? 0
             : static_cast<int>(coeffs_.size() - 1) / factor_;   // both passes
    }

private:
    int                factor_ = 1;
    std::vector<float> coeffs_;
    FIRFilter          up_, down_;
};

}   // namespace audio
```

**Walkthrough of the two details that matter.**

**`x * factor_`.** Zero-stuffing puts `L-1` zeros between real samples, which divides the average
energy by `L`. Multiplying the real sample by `L` restores it. Omit this and your oversampled
path is `20·log10(L)` dB quieter — 12 dB at 4×, which looks like a mysterious gain bug.

**The filter is used twice** — once for upsampling (image rejection) and once for downsampling
(anti-aliasing). They need the same cutoff, so one design serves both. This halves the design
cost and guarantees they match.

**Latency.** Two FIR passes at the high rate, so `(taps−1)` samples at rate `L·fs`, which is
`(taps−1)/L` at the base rate. This must be reported and compensated, or an oversampled plugin in
parallel with a dry path will comb-filter (Chapter 2).

### The polyphase optimisation

The naive implementation computes `L` filter outputs per input sample, of which `L−1` are
discarded — and most of the multiplies are against zeros.

A **polyphase** implementation splits the filter into `L` sub-filters ("phases") and computes
only the outputs actually needed, skipping every multiply-by-zero. It is `L` times faster for the
same result. Chapter 63 covers the implementation; the idea is just "do not multiply by zeros you
inserted yourself".

---

## 29.5 Measuring it

The right test is a **swept sine through the nonlinearity, viewed as a spectrogram** (Chapter
27).

`examples/ch29_oversample.cpp` sweeps 100 Hz → 8 kHz through a hard clipper at 1×, 2×, 4× and 8×
oversampling.

**1× (none):** the fundamental climbs. Above about 2.5 kHz, a forest of lines appears, most of
them descending. By 6 kHz the picture is chaos and the sound is a metallic scream.

**2×:** noticeably cleaner. Aliasing begins around 5 kHz.

**4×:** clean to about 9 kHz. This is where most commercial distortion plugins sit.

**8×:** essentially clean across the sweep; a faint haze at the very top.

And the numerical measurement — feed a single 5 kHz sine, then sum all energy at frequencies that
are **not** harmonics of 5 kHz:

```
  oversampling    aliasing energy    relative to signal
  ----------------------------------------------------
      1x             -22.1 dB              -22.1 dB
      2x             -38.6 dB              -38.6 dB
      4x             -52.3 dB              -52.3 dB
      8x             -64.8 dB              -64.8 dB
     16x             -76.1 dB              -76.1 dB
```

Roughly **12–14 dB of improvement per doubling**. Note that even 16× does not reach zero, because
hard clipping's harmonics decay only as `1/h`.

**Repeat the measurement with `tanh` instead of hard clipping** and the numbers improve
dramatically at every factor, because `tanh`'s harmonics decay much faster. That comparison is
the strongest argument for soft clipping there is.

---

## 29.6 Alternatives to oversampling

Oversampling is the general solution. Three alternatives are worth knowing, because each is
better in a specific case.

**1. Antiderivative anti-aliasing (ADAA).** Instead of evaluating the nonlinearity at each
sample, evaluate the *antiderivative* of the shaping function at two consecutive samples and take
the difference. This effectively integrates the nonlinearity over the sample period, which
band-limits it. First-order ADAA gives roughly the alias reduction of 4× oversampling at a
fraction of the cost. It is the modern technique for waveshaping, and the catch is that you need
a closed-form antiderivative, plus careful handling when consecutive samples are nearly equal
(the difference becomes `0/0`).

**2. Band-limited synthesis.** For oscillators, do not generate the aliasing in the first place.
PolyBLEP, BLIT and wavetables (Chapters 31–32) generate band-limited waveforms directly, which is
far cheaper than oversampling a naive one.

**3. Choose gentler nonlinearities.** A `tanh` needs less oversampling than a hard clip. A cubic
soft-clip that is exactly linear below a threshold generates nothing at all until it is driven.
This is a design choice available to you before you reach for CPU.

---

## 29.7 Where oversampling belongs in a chain

A practical note that saves real CPU.

**Oversample only the nonlinear stages**, and keep the linear ones at the base rate:

```
   IN -> EQ -> [ up 4x -> saturate -> down 4x ] -> compressor -> reverb -> OUT
                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                only this part runs at 4x
```

Running the whole chain at 4× costs four times as much for no benefit — filters and reverbs are
linear and cannot alias.

**But do not oversample each nonlinearity separately if they are adjacent.** Two back-to-back
saturators should share one up/down pair; otherwise you pay for four filter passes instead of two,
and you double the latency.

And note that **a compressor is nonlinear too**. Its gain changes create sidebands, and a fast
attack on bright material can alias. Most compressors do not oversample; the ones that do tend to
be described as "cleaner", and this is why.

---

## 29.8 Exercises

**29.1** Feed a 5 kHz sine through a hard clipper with no oversampling. FFT the output and list
every peak. Mark which are harmonics of 5 kHz and which are aliases. Check against §29.2's table.

**29.2** Implement the `Oversampler` and reproduce §29.5's measurement table for hard clipping.

**29.3** Repeat 29.2 for `tanh`, cubic soft clip, and `x²`. Rank them by how much oversampling
they need for −60 dB of alias rejection.

**29.4** *Deliberate breakage.* Remove the `x * factor_` scaling. How many dB quiet is the output
at 2×, 4× and 8×? Confirm it is `20·log10(L)`.

**29.5** Make spectrograms of a swept sine through a clipper at 1×, 2×, 4× and 8×. Put the four
images side by side.

**29.6** Measure the latency of the oversampler by sending an impulse through it. Does it match
`latencySamples()`?

**29.7** Build a chain with a dry path and an oversampled saturated path in parallel, summed.
First without latency compensation, then with. Listen to both — the uncompensated version should
comb-filter audibly.

**29.8** Implement first-order ADAA for `tanh`. Its antiderivative is `log(cosh(x))`. Compare its
aliasing against 1×, 2× and 4× oversampling, and compare CPU cost. Handle the near-equal-samples
case.

**29.9** Take Chapter 12's naive sawtooth and oversample it 8×. How much does the aliasing
improve? Compare against the band-limited additive version. Which approach is cheaper for the
same quality? (This is the argument for Chapter 31.)

---

### Chapter summary

- Two sources of aliasing: sampling something not band-limited (handled by converters, except
  when *you* are the generator), and **nonlinear processing creating content above Nyquist**
  (entirely yours to handle).
- **Linear processes cannot alias; nonlinear ones always can.** That single rule tells you which
  stages of your chain need protection.
- A hard clipper on a 5 kHz sine produces aliases at 900, 9,100, 10,900, 19,100 and 20,900 Hz —
  **none harmonically related to the input**. Chords are worse, because intermodulation products
  multiply.
- **Oversampling**: upsample by `L` (zero-stuff, filter, **scale by `L`**), apply the
  nonlinearity, then downsample (filter, decimate). The downsampling filter removes what would
  have aliased.
- It never eliminates aliasing — it reduces it by roughly **12–14 dB per doubling**. The harsher
  the nonlinearity, the more you need; hard clipping is the worst case.
- One filter design serves both directions. **Polyphase** implementation skips the
  multiply-by-zeros and is `L` times faster.
- **Report and compensate the latency**, or parallel dry paths comb-filter.
- Alternatives: **ADAA** (≈4× quality at a fraction of the cost, needs a closed-form
  antiderivative), **band-limited synthesis** for oscillators (Chapters 31–32), and simply
  choosing gentler nonlinearities.
- **Oversample only the nonlinear stages**, and share one up/down pair across adjacent ones.

---

## Part II is complete

You have derived and implemented, by hand: signals and systems notation, complex numbers and
phasors, linearity and time-invariance, the impulse response, convolution, FIR design by windowed
sinc, IIR biquads from the RBJ cookbook, poles and zeros and the stability test, the DFT, the
FFT, fast convolution, windowing, the STFT and spectrograms, interpolation and resampling, and
oversampling.

That is the complete working toolkit of a DSP engineer. Every effect in Part IV and every
synthesis technique in Part III is assembled from these pieces — and you can now read the
literature, because you know what the symbols mean.

**Part III is the fun part.** Instruments.

**Next:** [Chapter 30 — Oscillator Architecture](30-oscillator-architecture.md)
