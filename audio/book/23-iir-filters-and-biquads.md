# Chapter 23 — IIR Filters and the Biquad

> The biquad is the single most useful object in audio DSP. Eleven lines of code, five
> coefficients, and it gives you every filter shape in every equaliser you have ever used. This
> chapter derives it, explains where the coefficients come from, and builds it properly.

---

## 23.1 Feedback buys efficiency

Chapter 22's FIR filter needed hundreds of taps to be sharp, because each tap could only look at
one past *input*.

An IIR filter also looks at past **outputs**:

```
   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] + ...
        - a1·y[n-1] - a2·y[n-2] - ...
```

That feedback is enormously powerful. An output sample already contains a weighted blend of the
entire history; feeding it back lets a handful of coefficients express a response that would take
an FIR hundreds of taps to match.

**The rough exchange rate:** an IIR filter of order 2–4 achieves roughly what an FIR filter of
order 50–500 achieves, for the same steepness. That is a 20–100× saving in CPU.

**What you give up:**

| | FIR | IIR |
|---|---|---|
| Stability | guaranteed | **must be checked** |
| Phase | can be exactly linear | never linear (except trivially) |
| Latency | `M/2` samples | essentially none |
| Sharpness per operation | poor | excellent |
| Coefficient sensitivity | low | **high** — small errors can destabilise |
| Sweeping in real time | expensive (redesign) | cheap |

The last row is why synthesisers use IIR filters. A resonant low-pass sweeping across the
spectrum is the defining sound of subtractive synthesis, and recomputing an IIR's five
coefficients costs a handful of operations, while redesigning a 500-tap FIR costs 500 `sin`
calls.

---

## 23.2 The one-pole filter

The simplest useful IIR, and you have already written it three times without naming it (the ADSR
in Chapter 13, the DC blocker's feedback term in Chapter 14, the noise damping in Chapter 15).

```
   y[n] = (1-a)·x[n] + a·y[n-1]
```

Each output is a blend: a fraction `(1-a)` of the new input, and a fraction `a` of the previous
output.

- `a = 0` — output equals input. No filtering.
- `a = 0.5` — moderate smoothing.
- `a = 0.99` — heavy smoothing; only very slow changes get through.
- `a = 1` — output never changes. Frozen.
- `a > 1` — **unstable**, grows without bound.

It is a **low-pass filter** with a 6 dB/octave slope. Its impulse response is `(1-a)·a^n` — the
decaying exponential of Chapter 18.

### Setting the cutoff

```cpp
a = std::exp(-kTwoPi * cutoffHz / sampleRate);
```

This places the −3 dB point at `cutoffHz`. Derivation is in Appendix A; the formula is worth
memorising because one-poles appear everywhere:

| Use | Chapter |
|---|---|
| Envelope generator segments | 13 |
| DC blocker feedback | 14 |
| Pink and brown noise shaping | 15 |
| Parameter smoothing | 61 |
| Envelope follower in a compressor | 50 |
| Damping inside a reverb | 46 |
| Tone control on a delay's feedback path | 43 |

**Seven different jobs, one line of code.** When you see `y = a*y + (1-a)*x` anywhere in audio
code, it is this filter.

### Why one pole is not enough

6 dB/octave is gentle. At one octave above the cutoff you have removed only half the amplitude.
Real filters want 12, 24 or 48 dB/octave.

You could cascade one-poles — two gives 12 dB/octave — but cascaded one-poles cannot produce
**resonance**, and resonance is the most musically useful thing a filter does. For that you need
two poles, which means a second-order filter, which means the biquad.

---

## 23.3 The biquad

**Biquad** = "bi-quadratic": second order in both the feedforward and feedback paths.

```
   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] - a1·y[n-1] - a2·y[n-2]
```

Five coefficients. That is the entire filter, and it can be:

- Low-pass, high-pass, band-pass (12 dB/octave)
- Notch (band-reject)
- Peaking EQ (boost or cut a band)
- Low shelf, high shelf
- All-pass (changes phase only)

Every band of every parametric EQ you have used is one of these. A 10-band EQ is ten biquads.

> **The name "biquad" comes from the transfer function**, which is a ratio of two quadratics:
> `H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²)`. Chapter 24 explains `z`; for now it is
> just where the name comes from.

### Direct Form I

The literal transcription:

```cpp
float process(float x)
{
    const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

    x2 = x1;  x1 = x;
    y2 = y1;  y1 = y;

    return static_cast<float>(y);
}
```

Four state variables. Simple, and **the most numerically robust form** when coefficients change
often — which is why many synthesisers use it despite the extra memory.

### Transposed Direct Form II

The form most libraries use:

```cpp
float process(float x)
{
    const double y = b0*x + s1;
    s1 = b1*x - a1*y + s2;
    s2 = b2*x - a2*y;
    return static_cast<float>(y);
}
```

Only **two** state variables, and better numerical behaviour in floating point than plain Direct
Form II (which has an internal node that can overflow at high Q).

**Which to use?** Transposed DF-II for fixed coefficients; DF-I when coefficients are modulated
rapidly, because TDF-II can produce a small transient when coefficients jump. Our implementation
offers both so you can hear the difference.

---

## 23.4 Where the coefficients come from

You do not derive biquad coefficients from scratch. You use the **Audio EQ Cookbook** — a set of
formulas published by Robert Bristow-Johnson in 1994 that has become the universal standard. Open
almost any audio codebase and you will find these formulas.

Everything starts from three intermediate values:

```
   w0    = 2π · f0 / fs                 (normalised centre frequency)
   cos_w = cos(w0)
   sin_w = sin(w0)
   alpha = sin_w / (2·Q)                (bandwidth parameter)
```

And for the gain-based filters (peaking and shelving):

```
   A = 10^(dBgain / 40)                 (note: 40, not 20 -- see below)
```

> **Why 40?** These filters apply `A` to amplitude at the peak but the formulas use `A` in places
> where it gets squared. Dividing by 40 rather than 20 accounts for that, so a `dBgain` of +6
> really produces +6 dB. Using 20 gives you +12 dB and a confused afternoon.

### The formulas

**Low-pass:**
```
   b0 = (1 - cos_w) / 2
   b1 =  1 - cos_w
   b2 = (1 - cos_w) / 2
   a0 =  1 + alpha
   a1 = -2·cos_w
   a2 =  1 - alpha
```

**High-pass:**
```
   b0 =  (1 + cos_w) / 2
   b1 = -(1 + cos_w)
   b2 =  (1 + cos_w) / 2
   a0 =  1 + alpha
   a1 = -2·cos_w
   a2 =  1 - alpha
```

**Band-pass (constant peak gain of 1):**
```
   b0 =  alpha
   b1 =  0
   b2 = -alpha
   a0 =  1 + alpha
   a1 = -2·cos_w
   a2 =  1 - alpha
```

**Notch:**
```
   b0 =  1
   b1 = -2·cos_w
   b2 =  1
   a0 =  1 + alpha
   a1 = -2·cos_w
   a2 =  1 - alpha
```

**All-pass:**
```
   b0 =  1 - alpha
   b1 = -2·cos_w
   b2 =  1 + alpha
   a0 =  1 + alpha
   a1 = -2·cos_w
   a2 =  1 - alpha
```

**Peaking EQ:**
```
   b0 =  1 + alpha·A
   b1 = -2·cos_w
   b2 =  1 - alpha·A
   a0 =  1 + alpha/A
   a1 = -2·cos_w
   a2 =  1 - alpha/A
```

**Low shelf:**
```
   b0 =    A·((A+1) - (A-1)·cos_w + 2·sqrt(A)·alpha)
   b1 =  2·A·((A-1) - (A+1)·cos_w)
   b2 =    A·((A+1) - (A-1)·cos_w - 2·sqrt(A)·alpha)
   a0 =       (A+1) + (A-1)·cos_w + 2·sqrt(A)·alpha
   a1 =   -2·((A-1) + (A+1)·cos_w)
   a2 =       (A+1) + (A-1)·cos_w - 2·sqrt(A)·alpha
```

**High shelf:**
```
   b0 =    A·((A+1) + (A-1)·cos_w + 2·sqrt(A)·alpha)
   b1 = -2·A·((A-1) + (A+1)·cos_w)
   b2 =    A·((A+1) + (A-1)·cos_w - 2·sqrt(A)·alpha)
   a0 =       (A+1) - (A-1)·cos_w + 2·sqrt(A)·alpha
   a1 =    2·((A-1) - (A+1)·cos_w)
   a2 =       (A+1) - (A-1)·cos_w - 2·sqrt(A)·alpha
```

### The normalisation step everyone forgets

The formulas produce **six** numbers, including `a0`. The difference equation assumes `a0 = 1`.
So divide everything by `a0`:

```cpp
b0 /= a0;  b1 /= a0;  b2 /= a0;
a1 /= a0;  a2 /= a0;
a0 = 1.0;
```

**Omitting this is the most common biquad bug.** The symptom is a filter with the right *shape*
but the wrong *level* — often wildly wrong, and level-dependent on Q, which makes it look like a
mysterious gain problem rather than a coefficient problem.

---

## 23.5 What Q actually means

`Q` is the **quality factor**. It controls how sharply the filter acts around its centre
frequency.

For band-pass and notch, it is literally the ratio of centre frequency to bandwidth:

```
   Q = f0 / bandwidth
```

A 1 kHz band-pass with `Q = 10` has a bandwidth of 100 Hz. With `Q = 0.5`, 2,000 Hz.

For low-pass and high-pass, Q controls the **resonance** — how much the response peaks right at
the cutoff before falling away:

| Q | Behaviour at the cutoff |
|---|---|
| 0.5 | Maximally damped; no peak, gentlest knee |
| **0.7071 (1/√2)** | **Butterworth: maximally flat passband, no peak. The default.** |
| 1.0 | Slight peak (+1.2 dB) |
| 2.0 | Clear peak (+6 dB) |
| 5.0 | Strong resonance (+14 dB), audible "whistle" at the cutoff |
| 10.0 | Very resonant (+20 dB) |
| 20+ | Nearly self-oscillating; a ringing tone |

**`Q = 0.7071` is the value to remember.** It gives the Butterworth response — the flattest
possible passband — and it is what "no resonance" means on a filter control.

### Resonance is the sound of synthesis

High Q on a swept low-pass is the classic analogue synth sound. The resonant peak emphasises
whatever harmonic it is passing over, so as the cutoff sweeps down through a sawtooth's harmonic
series, you hear each harmonic get picked out in turn — that vocal, "wah", liquid quality.

At very high Q the filter is close to **self-oscillating**: it rings at its cutoff frequency even
with no input, because the feedback almost exactly sustains itself. Many analogue filters
deliberately allow full self-oscillation, turning the filter into a sine oscillator. Our digital
biquad becomes unstable rather than gracefully oscillating, which is one of several reasons real
synth filters are not biquads — Chapter 33 covers the ladder and state-variable designs that are.

> **Q and stability.** High Q pushes the filter's poles close to the unit circle (Chapter 24).
> Close to the circle means: sensitive to coefficient precision, slow to settle, and prone to
> instability if anything is slightly off. This is precisely why filter state must be `double` —
> at `Q = 20` and a 50 Hz cutoff, a `float` biquad can genuinely blow up. Try it in Exercise 23.8,
> with the volume at zero.

---

## 23.6 Cascading for steeper slopes

One biquad gives 12 dB/octave. For more, cascade them:

| Biquads | Order | Slope |
|---|---|---|
| 1 | 2 | 12 dB/octave |
| 2 | 4 | 24 dB/octave |
| 3 | 6 | 36 dB/octave |
| 4 | 8 | 48 dB/octave |

**But you cannot simply use the same Q in each stage.** Cascading two `Q = 0.7071` filters does
not give a 4th-order Butterworth; it gives a droopy response that is −6 dB at the nominal cutoff
instead of −3 dB.

The correct Q values for a Butterworth cascade come from evenly spacing poles around a semicircle:

```
   Q[k] = 1 / (2·cos( π·(2k+1) / (2·N) ))     for k = 0 .. N/2-1, order N
```

| Order | Stage Q values |
|---|---|
| 2 | 0.7071 |
| 4 | 0.5412, 1.3066 |
| 6 | 0.5176, 0.7071, 1.9319 |
| 8 | 0.5098, 0.6013, 0.8999, 2.5629 |

Note the pattern: one stage is heavily damped, another is quite resonant, and they combine into a
flat passband with a steep roll-off. This is genuinely non-obvious and it is why a "24 dB/octave
low-pass" in a plugin is not just two 12 dB filters stacked.

Other classic responses use different pole placements: **Chebyshev** trades passband ripple for a
steeper transition, **elliptic** ripples in both bands for the steepest transition of all, and
**Bessel** sacrifices steepness for the flattest group delay. Appendix D tabulates them.

---

## 23.7 The implementation

**Code — `lib/include/audio/biquad.h`** (excerpt)

```cpp
namespace audio {

enum class FilterType {
    LowPass, HighPass, BandPass, Notch, AllPass,
    Peaking, LowShelf, HighShelf
};

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs design(FilterType type, double freqHz, double sampleRate,
                               double Q = 0.70710678, double gainDb = 0.0);

    // |H(w)| in dB at a given frequency.
    double magnitudeDb(double freqHz, double sampleRate) const;

    // Stability test: poles inside the unit circle. See Chapter 24.
    bool isStable() const
    {
        return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
    }
};

class Biquad : public Processor
{
public:
    void setCoefficients(const BiquadCoeffs& c) { coeffs_ = c; }
    void setFilter(FilterType type, double freqHz, double Q = 0.70710678,
                   double gainDb = 0.0);

    void prepare(double sampleRate, int maxBlockSize) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* name() const override { return "Biquad"; }

    // Transposed Direct Form II: two state variables, good numerics.
    float processSample(float x, int channel = 0)
    {
        auto& s = state_[static_cast<size_t>(channel)];
        const double xd = static_cast<double>(x);

        const double y = coeffs_.b0 * xd + s.s1;
        s.s1 = coeffs_.b1 * xd - coeffs_.a1 * y + s.s2;
        s.s2 = coeffs_.b2 * xd - coeffs_.a2 * y;

        // Denormal guard (Chapter 6, and properly in Chapter 59).
        if (!std::isfinite(y)) { reset(); return 0.0f; }

        return static_cast<float>(y);
    }

private:
    struct State { double s1 = 0.0, s2 = 0.0; };

    BiquadCoeffs       coeffs_;
    std::vector<State> state_;
    double             sampleRate_ = kDefaultRate;
};

// A cascade of biquads, for higher-order filters.
class BiquadCascade : public Processor
{
public:
    // Butterworth of the given order (rounded up to even).
    void setButterworth(FilterType type, double freqHz, int order);
    // ... Processor interface
private:
    std::vector<Biquad> stages_;
};

}   // namespace audio
```

The `design` function is a direct transcription of §23.4's formulas, including the `a0`
normalisation:

```cpp
BiquadCoeffs BiquadCoeffs::design(FilterType type, double freqHz, double sampleRate,
                                  double Q, double gainDb)
{
    BiquadCoeffs c;

    // Clamp the frequency well inside the valid range. A cutoff at or above
    // Nyquist produces garbage coefficients; at zero it divides by zero.
    freqHz = std::clamp(freqHz, 1.0, sampleRate * 0.495);
    Q      = std::max(Q, 0.001);

    const double w0    = kTwoPi * freqHz / sampleRate;
    const double cos_w = std::cos(w0);
    const double sin_w = std::sin(w0);
    const double alpha = sin_w / (2.0 * Q);
    const double A     = std::pow(10.0, gainDb / 40.0);    // 40, not 20

    double b0, b1, b2, a0, a1, a2;

    switch (type)
    {
        case FilterType::LowPass:
            b0 = (1.0 - cos_w) * 0.5;  b1 = 1.0 - cos_w;  b2 = b0;
            a0 = 1.0 + alpha;  a1 = -2.0 * cos_w;  a2 = 1.0 - alpha;
            break;
        // ... the other seven types
    }

    // THE STEP EVERYONE FORGETS
    c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;
    c.a1 = a1 / a0;  c.a2 = a2 / a0;

    return c;
}
```

And the magnitude response, which lets you verify the filter numerically:

```cpp
double BiquadCoeffs::magnitudeDb(double freqHz, double sampleRate) const
{
    const double w = kTwoPi * freqHz / sampleRate;
    const std::complex<double> z1 = std::exp(std::complex<double>(0.0, -w));
    const std::complex<double> z2 = z1 * z1;

    const std::complex<double> num = b0 + b1 * z1 + b2 * z2;
    const std::complex<double> den = 1.0 + a1 * z1 + a2 * z2;

    return gainToDb(std::abs(num / den));
}
```

**Why this works:** the transfer function is `H(z) = (b0 + b1·z⁻¹ + b2·z⁻²)/(1 + a1·z⁻¹ +
a2·z⁻²)`, and evaluating it at `z = e^(jω)` — a point on the unit circle — gives the frequency
response. Chapter 24 explains why the unit circle is where the frequency response lives; for now,
note that this eight-line function tells you exactly what your filter does, and you should use it
to verify every filter you build.

---

## 23.8 Sweeping without zipper noise

Recomputing coefficients every sample during a sweep is correct but expensive (`sin`, `cos`,
`pow` per sample). Recomputing every block is cheap but makes the coefficients *jump*, and
Chapter 13 told you what jumps are: clicks. A sweep done this way produces **zipper noise** — a
stepped, grainy quality.

Three standard solutions:

**1. Recompute every N samples and interpolate the coefficients.** Cheap and usually good enough.
Note that interpolating coefficients can briefly produce an unstable intermediate state, so check
`isStable()` or interpolate a parameterisation that cannot go unstable.

**2. Smooth the *parameter*, not the coefficients.** Run the cutoff frequency through a one-pole
smoother, then recompute coefficients from the smoothed value every block. This is safer and it
is what most plugins do.

```cpp
smoothedCutoff = smoothedCutoff * a + targetCutoff * (1 - a);   // one-pole again
```

**3. Use a filter topology designed for modulation.** State-variable and ladder filters
(Chapter 33) have coefficients that map more directly to frequency and remain well-behaved under
fast modulation. This is the real reason synthesisers rarely use biquads for their main filter.

Chapter 61 covers parameter smoothing properly.

---

## 23.9 Hearing it

**`examples/ch23_biquad.cpp`** renders:

**1. The eight filter types** at 1 kHz on pink noise, so you can hear each shape.

**2. Q comparison.** A 1 kHz low-pass on a sawtooth at Q = 0.5, 0.707, 2, 5, 10, 20. Listen to
the resonant peak emerge and then dominate. At Q = 20 the filter nearly rings on its own.

**3. The classic sweep.** A sawtooth through a resonant low-pass sweeping from 8 kHz to 100 Hz
over six seconds at Q = 6. This single file is the sound of subtractive synthesis, and it is four
lines of setup.

**4. Slope comparison.** A low-pass at 1 kHz in 12, 24 and 48 dB/octave versions, using the
Butterworth cascade Q values from §23.6. The measured response confirms the slopes:

```
  order 2  (12 dB/oct):  1 kHz -3.0 dB   2 kHz -12.3 dB   4 kHz -24.1 dB
  order 4  (24 dB/oct):  1 kHz -3.0 dB   2 kHz -24.1 dB   4 kHz -48.2 dB
  order 8  (48 dB/oct):  1 kHz -3.0 dB   2 kHz -48.2 dB   4 kHz -96.3 dB
```

All three are −3 dB at the cutoff (the Butterworth Q values did their job) and each doubling of
order doubles the slope.

**5. Shelf and peak EQ.** ±12 dB at 100 Hz (low shelf), 1 kHz (peaking, Q = 1), 8 kHz (high
shelf) — the three controls of a basic mixing channel.

---

## 23.10 Exercises

**23.1** Implement the one-pole low-pass. Verify with `magnitudeDb` that its −3 dB point is where
`a = exp(-2π·fc/fs)` says it should be.

**23.2** Design a 1 kHz low-pass biquad at Q = 0.7071 and print its five coefficients. Verify
they produce −3.01 dB at 1 kHz and −12.3 dB at 2 kHz.

**23.3** *Deliberate breakage.* Skip the `a0` normalisation. Measure the DC gain of the resulting
filter at several Q values. How wrong is it, and how does the error depend on Q?

**23.4** Design a peaking EQ with +6 dB at 1 kHz, Q = 1. Verify the measured boost is 6 dB, not
12. Now change the `/40.0` to `/20.0` in the `A` calculation and re-measure.

**23.5** Cascade two Q = 0.7071 low-passes at 1 kHz and measure the response at 1 kHz. Then use
the correct 4th-order Butterworth Q values (0.5412, 1.3066) and measure again. What is the
difference at the cutoff?

**23.6** Build a 3-band EQ (low shelf 200 Hz, peaking 1.5 kHz, high shelf 6 kHz) as a `Chain` of
biquads. Apply it to a full mix and verify by measurement that each band does what you asked.

**23.7** Implement `isStable()` and test it by designing filters at frequencies approaching
Nyquist (0.4·fs, 0.49·fs, 0.499·fs) and at very high Q. Where does it first report instability?

**23.8** *Volume at zero.* Change the biquad's state variables from `double` to `float`. Design a
50 Hz low-pass at Q = 20 and run 10 seconds of audio through it. Compare the output with the
`double` version. This is why Chapter 6's rule exists.

**23.9** Implement the zipper-noise demonstration: sweep a filter's cutoff by recomputing
coefficients once per 512-sample block with no smoothing, then with one-pole parameter smoothing.
Render both and compare.

**23.10** Build a band-pass at 1 kHz and sweep its Q from 0.5 to 50 over ten seconds while
feeding it white noise. At what Q does it start to sound like a pitched tone rather than filtered
noise? Relate that to Chapter 3's critical bands.

---

### Chapter summary

- IIR filters feed back past **outputs**, which buys enormous efficiency: order 2–4 does the work
  of a 50–500-tap FIR. The costs are possible instability, non-linear phase, and sensitivity to
  coefficient precision.
- The **one-pole** `y = (1-a)·x + a·y[n-1]` with `a = exp(-2π·fc/fs)` is a 6 dB/octave low-pass
  and appears in at least seven different roles across this book.
- The **biquad** is `y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] − a1·y[n-1] − a2·y[n-2]`. Five
  coefficients give every standard filter shape.
- Use **Transposed Direct Form II** (two state variables, good numerics) for fixed coefficients;
  **Direct Form I** when coefficients are modulated fast.
- Coefficients come from the **RBJ Audio EQ Cookbook**, built from `w0`, `cos_w`, `sin_w`,
  `alpha = sin_w/(2Q)` and `A = 10^(dB/40)` — note the **40**.
- **Always divide all five coefficients by `a0`.** Forgetting this is the most common biquad bug,
  and it produces a right-shaped, wrong-level filter.
- **Q = 0.7071 is Butterworth**: maximally flat, no peak. Higher Q gives resonance — the sound of
  subtractive synthesis — and pushes poles toward instability, which is why state must be
  `double`.
- Cascade biquads for steeper slopes, but **use the correct per-stage Q values**; identical
  Q stages do not give a Butterworth response.
- Sweeping cutoff needs **parameter smoothing** or you get zipper noise. Synth filters use
  state-variable/ladder topologies (Chapter 33) that modulate more gracefully than biquads.
- Verify every filter you build with `magnitudeDb` — eight lines, and it catches almost every
  coefficient error.

**Next:** [Chapter 24 — Poles, Zeros, and Filter Stability](24-poles-zeros-stability.md)
