# Chapter 22 — FIR Filters and Windowed-Sinc Design

> A filter is a system that treats different frequencies differently. Chapter 20 told you a
> filter *is* its impulse response. This chapter turns that around: **decide what you want the
> frequency response to be, and derive the impulse response that produces it.** That is filter
> design, and FIR is where it is easiest to see.

---

## 22.1 What an FIR filter is

**FIR** = Finite Impulse Response. The impulse response has a finite number of non-zero samples,
which means there is no feedback:

```
   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] + ... + bM·x[n-M]
```

or compactly:

```
           M
   y[n] =  Σ  b[k]·x[n-k]
          k=0
```

Compare that with Chapter 21's convolution sum. **They are the same equation.** An FIR filter is
convolution with a short impulse response, and:

> **The coefficients `b[k]` ARE the impulse response `h[k]`.**

Feed in an impulse: `x[0] = 1`, everything else zero. Then `y[0] = b0`, `y[1] = b1`, and so on.
The coefficients come straight out. Nothing else in DSP is this direct, and it is why FIR is the
right place to start.

The number of coefficients is the **filter length** or **number of taps**. "Taps" comes from
analogue delay lines, where you physically tapped the line at various points.

### Properties

| Property | FIR | Why |
|---|---|---|
| **Always stable** | ✓ | No feedback, so nothing can run away. Bounded input gives bounded output, always. |
| **Can have exactly linear phase** | ✓ | If the coefficients are symmetric. See §22.6. |
| **Needs many taps to be sharp** | ✗ | A steep filter can need hundreds or thousands of taps. |
| **Costs one MAC per tap** | ✗ | 512 taps = 512 multiply-accumulates per sample per channel. |
| **Latency** | ✗ | Linear-phase FIR delays the signal by half its length. |

That trade is the whole FIR-versus-IIR decision, and Chapter 23 gives you the other side.

---

## 22.2 The simplest filter: a moving average

```cpp
y[n] = (x[n] + x[n-1]) / 2;
```

Coefficients `{0.5, 0.5}`. What does it do?

**A constant input** (0 Hz): `(1 + 1)/2 = 1`. Passes unchanged. Gain 1.

**An alternating input** `+1, -1, +1, -1` (that is Nyquist, `ω = π`): `(1 + (-1))/2 = 0`.
Completely removed. Gain 0.

So it passes low frequencies and blocks the highest. **It is a low-pass filter.** The crudest
one there is, but a real one.

Extending to `N` taps of `1/N` each gives a longer moving average, which cuts more high
frequency. But it is a *bad* low-pass: its frequency response has large ripples and a very
gradual roll-off, because — as we will see — a rectangular shape in one domain is a disaster in
the other.

### Computing the frequency response

From Chapter 20, `H(ω) = Σ h[n]·e^(-jωn)`. For our two-tap filter:

```
   H(ω) = 0.5 + 0.5·e^(-jω)
```

At `ω = 0`: `0.5 + 0.5 = 1`. ✓
At `ω = π`: `0.5 + 0.5·e^(-jπ) = 0.5 - 0.5 = 0`. ✓

In code, evaluating this for any impulse response is a direct transcription:

```cpp
// Frequency response of an FIR filter at normalised frequency w (radians/sample).
std::complex<double> frequencyResponse(const std::vector<float>& h, double w)
{
    std::complex<double> H(0.0, 0.0);
    for (size_t n = 0; n < h.size(); ++n)
        H += static_cast<double>(h[n]) * std::exp(std::complex<double>(0.0, -w * n));
    return H;
}
```

Twelve lines including braces, and it tells you exactly what any FIR filter does at any
frequency. Run it across a range of `ω` and you have plotted the filter.

---

## 22.3 The ideal low-pass, and the sinc

Now design properly. What frequency response do we *want*?

**A brick wall.** Gain 1 below the cutoff, gain 0 above:

```
   |H(w)|
     1 |________
       |        |
       |        |
     0 +--------+-----------> w
       0       wc          pi
```

What impulse response produces it? The inverse Fourier transform of that rectangle. The answer —
derived in Appendix A, or take it on trust — is:

```
   h[n] = (2·fc/fs) · sinc( 2·fc·n / fs )
```

where

```
   sinc(x) = sin(πx) / (πx),     and sinc(0) = 1
```

```
         1 |        *
           |       * *
           |      *   *
         0 |*--*-*-----*-*--*----*----
           |  *           *
           |
        -4 -3 -2 -1  0  1  2  3  4
```

The **sinc function**. It is 1 at the centre, exactly zero at every other integer, and it decays
slowly with oscillating sign. You met it in Chapter 4 as the reconstruction function, and that is
not a coincidence: reconstruction is exactly low-pass filtering at Nyquist.

### Two problems, one of which is fatal

**Problem 1: it is infinitely long.** `sinc` never reaches zero. A perfect brick wall needs
infinitely many taps.

**Problem 2: it is non-causal.** It extends to *negative* `n` — it needs the future.

Problem 2 is fixable: **shift it**. Take the section from `-M/2` to `+M/2`, and slide it right so
it runs from 0 to M. The filter now works; it simply delays the signal by `M/2` samples. That
delay is the **latency** of a linear-phase FIR, and it is unavoidable.

Problem 1 is where the interesting engineering lives.

---

## 22.4 Truncation, and why it fails

The obvious fix: just cut the sinc off after `M` taps.

The obvious fix is wrong, and the way it fails is instructive.

Cutting a signal off abruptly is **multiplying it by a rectangular window** — which is exactly
what caused the click in Chapter 13. By the convolution theorem (Chapter 21), multiplying in
time *convolves* in frequency. So the ideal brick wall gets convolved with the rectangle's
spectrum, and the rectangle's spectrum is... a sinc, with large oscillating sidelobes.

The result:

```
   Ideal:                      Truncated (rectangular window):
   |H|                         |H|
    1 |________                 1 |_/\_/\__
      |        |                  |        \
      |        |                  |         \_/\_/\_/\__ 
    0 +--------+---              0 +--------------------
```

Two specific defects:

**Ripple in the passband and stopband.** The response oscillates instead of being flat.

**The Gibbs phenomenon.** The overshoot at the transition is about **9% of the step height**, and
— this is the important part — **it does not get smaller as you add taps.** More taps make the
ripples narrower and more numerous, but the peak overshoot stays at 9% forever.

You met Gibbs in Chapter 10's additive square wave: those ripples at the corners that never went
away. Same phenomenon, same cause.

In audio terms, 9% overshoot is about **−21 dB of stopband rejection**. That is poor: frequencies
you asked to be removed are only 21 dB down, which is clearly audible. A usable audio filter
wants 60–100 dB.

---

## 22.5 Windowing: the fix

Instead of chopping abruptly, **taper the sinc gently to zero** at both ends.

```
   h[n] = sinc_part[n] × window[n]
```

```
   Rectangular (bad):          Hamming (good):
   ___________                      .-''''''-.
   |         |                   .-'          '-.
   |         |                 .'                '.
   ---------------            '--------------------'
```

The gentler taper has a much more compact spectrum — narrower sidelobes, far lower amplitude —
so convolving the brick wall with it produces much less ripple.

**The trade-off is unavoidable and it is the same one as Chapter 13's fade length:**

> **A gentler window gives lower ripple (better stopband rejection) but a wider transition band
> (less sharp cutoff).** You cannot have both without more taps.

### The standard windows

| Window | Formula (n from 0 to M) | Sidelobe | Transition width | Use |
|---|---|---|---|---|
| **Rectangular** | `1` | −13 dB | narrowest | Almost never |
| **Hann** | `0.5 - 0.5·cos(2πn/M)` | −31 dB | 2× | General purpose, FFT |
| **Hamming** | `0.54 - 0.46·cos(2πn/M)` | −41 dB | 2× | Good default for FIR |
| **Blackman** | `0.42 - 0.5·cos(2πn/M) + 0.08·cos(4πn/M)` | −57 dB | 3× | When you need rejection |
| **Blackman–Harris** | 4-term | −92 dB | 4× | Measurement |
| **Kaiser** | Bessel-based, parameter β | **adjustable** | adjustable | Best general choice |

The **Kaiser window** deserves its reputation: a single parameter `β` trades sidelobe level
against transition width continuously, so you can dial in exactly the specification you need
rather than picking from a menu.

```cpp
// Kaiser window. beta = 0 gives rectangular; larger beta = gentler taper.
double kaiser(size_t n, size_t M, double beta)
{
    const double r = 2.0 * static_cast<double>(n) / static_cast<double>(M) - 1.0;
    return besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
}

// Zeroth-order modified Bessel function of the first kind, by series.
// Converges quickly for the arguments we use.
double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum  += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}
```

And the design formulas, which are the genuinely useful part — they let you go from a
specification to a filter with no guesswork:

```
   Given a required stopband attenuation A (in dB) and transition width Δf (in Hz):

   beta = 0.1102·(A - 8.7)                    if A > 50
        = 0.5842·(A - 21)^0.4 + 0.07886·(A-21) if 21 <= A <= 50
        = 0                                     if A < 21

   numTaps = ceil( (A - 8) / (2.285 · 2π·Δf/fs) ) + 1
```

So "I need 80 dB of rejection with a 500 Hz transition at 44.1 kHz" gives `β = 7.85` and
**about 250 taps**. No trial and error.

---

## 22.6 Linear phase — the FIR superpower

If the coefficients are **symmetric** — `h[n] = h[M-n]` — the filter has **exactly linear
phase**, which means **every frequency is delayed by exactly the same amount**: `M/2` samples.

Why that matters: a filter with non-linear phase delays different frequencies by different
amounts, which smears transients. A kick drum's fundamental arrives at a slightly different time
from its click. Usually this is inaudible; sometimes it is not.

**Where linear phase genuinely matters:**

- **Mastering EQ.** Preserving transient shape on a full mix.
- **Crossovers** in multi-band processing and loudspeakers, where the bands must recombine
  exactly.
- **Anything summed with its unprocessed self**, where phase differences cause comb filtering.
- **Measurement and analysis**, where you need to know exactly what happened when.

**Where it is a liability:**

- **Latency.** `M/2` samples. A 1,024-tap filter at 44.1 kHz delays by 11.6 ms — unusable for a
  performer monitoring themselves.
- **Pre-ringing.** A symmetric impulse response rings *before* the transient as well as after.
  This is physically impossible in nature, and on sharp transients it is audible as a soft
  "smear" ahead of the hit. Many engineers prefer minimum-phase EQ on drums for exactly this
  reason.
- **CPU cost** at low frequencies, where the transition must be narrow and the tap count
  explodes.

> **The honest summary:** linear phase is a real and useful property, and it is oversold. Use it
> where the maths requires it (crossovers, parallel paths) and where transient preservation on a
> whole mix matters. Do not assume it is "more accurate" in general — pre-ringing is its own kind
> of inaccuracy.

---

## 22.7 The implementation

**Code — `lib/include/audio/fir.h`** (excerpt)

```cpp
namespace audio {

enum class WindowType { Rectangular, Hann, Hamming, Blackman, BlackmanHarris, Kaiser };

double windowValue(WindowType type, size_t n, size_t M, double kaiserBeta = 8.6);

// --- design functions: each returns an impulse response ---------------

std::vector<float> designLowPass (double cutoffHz, double sampleRate, size_t numTaps,
                                  WindowType w = WindowType::Hamming, double beta = 8.6);
std::vector<float> designHighPass(double cutoffHz, double sampleRate, size_t numTaps,
                                  WindowType w = WindowType::Hamming, double beta = 8.6);
std::vector<float> designBandPass(double lowHz, double highHz, double sampleRate,
                                  size_t numTaps,
                                  WindowType w = WindowType::Hamming, double beta = 8.6);
std::vector<float> designBandStop(double lowHz, double highHz, double sampleRate,
                                  size_t numTaps,
                                  WindowType w = WindowType::Hamming, double beta = 8.6);

// Kaiser design from a specification rather than a tap count.
size_t kaiserTapCount(double attenuationDb, double transitionHz, double sampleRate);
double kaiserBeta(double attenuationDb);

// --- the filter itself -------------------------------------------------

class FIRFilter : public Processor
{
public:
    void setCoefficients(std::vector<float> h);
    const std::vector<float>& coefficients() const { return h_; }

    void prepare(double sampleRate, int maxBlockSize) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* name() const override { return "FIRFilter"; }

    float processSample(float x, int channel = 0);

    // Latency in samples: (numTaps - 1) / 2 for a symmetric filter.
    int latencySamples() const { return static_cast<int>(h_.size() - 1) / 2; }

private:
    std::vector<float>              h_;
    std::vector<std::vector<float>> delay_;   // circular buffer per channel
    std::vector<size_t>             writeIndex_;
};

}   // namespace audio
```

And the design function, which is the heart of it:

```cpp
std::vector<float> designLowPass(double cutoffHz, double sampleRate, size_t numTaps,
                                 WindowType w, double beta)
{
    // Odd tap count gives a true centre sample and exact linear phase.
    if (numTaps % 2 == 0) ++numTaps;

    std::vector<float> h(numTaps);

    const double fc     = cutoffHz / sampleRate;     // normalised, 0..0.5
    const size_t M      = numTaps - 1;
    const double centre = static_cast<double>(M) / 2.0;

    double sum = 0.0;

    for (size_t n = 0; n < numTaps; ++n)
    {
        const double t = static_cast<double>(n) - centre;   // distance from centre

        // The ideal (infinite) low-pass impulse response.
        double ideal;
        if (std::fabs(t) < 1e-9)
            ideal = 2.0 * fc;                               // sinc(0) limit
        else
            ideal = std::sin(2.0 * kPi * fc * t) / (kPi * t);

        const double win = windowValue(w, n, M, beta);
        h[n] = static_cast<float>(ideal * win);
        sum += h[n];
    }

    // Normalise so the DC gain is exactly 1.0. Windowing perturbs it
    // slightly, and a filter that is 0.3 dB off is a filter with a bug.
    if (std::fabs(sum) > 1e-12)
        for (auto& v : h) v = static_cast<float>(v / sum);

    return h;
}
```

**Walkthrough of the three subtleties**

**Odd tap count.** With an odd number there is a genuine centre sample, and symmetry about it is
exact. Even-length symmetric filters have a half-sample delay, which is legal but complicates
latency compensation. Forcing odd is the conventional choice.

**The `sinc(0)` special case.** At the centre, `t = 0`, and `sin(0)/(π·0)` is `0/0` — NaN.
Mathematically the limit is `2·fc`; in code you must special-case it. Skip this and your filter
produces NaN, which Chapter 6 warned propagates everywhere. **This is the single most common bug
in hand-written FIR design.**

**DC normalisation.** Windowing changes the total gain slightly. Dividing by the coefficient sum
forces the DC gain to exactly 1.0. For a high-pass you normalise at Nyquist instead (sum with
alternating signs); for a band-pass, at the centre frequency.

### High-pass by spectral inversion

A neat trick that avoids deriving a second formula:

```cpp
std::vector<float> designHighPass(double cutoffHz, double sampleRate, size_t numTaps,
                                  WindowType w, double beta)
{
    auto h = designLowPass(cutoffHz, sampleRate, numTaps, w, beta);

    // Spectral inversion: negate everything, then add 1 to the centre tap.
    for (auto& v : h) v = -v;
    h[h.size() / 2] += 1.0f;

    return h;
}
```

**Why it works:** an all-pass filter is `δ[n]` — a single 1 at the centre. Subtracting the
low-pass from all-pass leaves everything the low-pass rejected, which is the high-pass.
`highpass = δ - lowpass`. Clean, and it guarantees the two are complementary, which matters for
crossovers.

Band-pass is the difference of two low-passes; band-stop is `δ` minus the band-pass. All four
filter types from one design function.

### Running the filter

```cpp
float FIRFilter::processSample(float x, int channel)
{
    auto&  line = delay_[static_cast<size_t>(channel)];
    size_t& wi  = writeIndex_[static_cast<size_t>(channel)];

    line[wi] = x;

    double acc = 0.0;
    size_t idx = wi;

    for (size_t k = 0; k < h_.size(); ++k)
    {
        acc += static_cast<double>(h_[k]) * static_cast<double>(line[idx]);
        idx = (idx == 0) ? line.size() - 1 : idx - 1;    // walk backwards
    }

    wi = (wi + 1) % line.size();
    return static_cast<float>(acc);
}
```

The circular buffer holds the last `M` input samples. We walk backwards from the newest, pairing
`h[0]` with `x[n]`, `h[1]` with `x[n-1]`, and so on — the gather form of convolution from
Chapter 21.

> **Performance note for Chapter 63:** the modulo and the backwards walk with wraparound both
> cost more than they should. Production FIR code keeps a **doubled buffer** (write each sample
> twice, at `wi` and `wi + M`) so the inner loop is a straight contiguous pass with no wrapping
> — which the compiler can then vectorise into 4 or 8 taps per instruction. That is typically a
> 3–5× speedup and it is why real FIR implementations look stranger than this one.

---

## 22.8 Hearing it

**Code — `examples/ch22_fir.cpp`** renders a series of comparisons.

**1. Window comparison.** The same 1 kHz low-pass at 101 taps, designed with each window, applied
to white noise. Rectangular has audible "leakage" — content above the cutoff that should not be
there. Blackman is clean but noticeably duller, because its transition band is wider so it starts
cutting earlier.

**2. Tap count.** A 1 kHz low-pass at 11, 31, 101, 501 and 2001 taps.

| Taps | Transition width | Latency | Character |
|---|---|---|---|
| 11 | ~4 kHz | 0.11 ms | Barely a filter; a gentle tilt |
| 31 | ~1.4 kHz | 0.34 ms | Soft |
| 101 | ~430 Hz | 1.1 ms | Usable |
| 501 | ~87 Hz | 5.7 ms | Sharp |
| 2001 | ~22 Hz | 22.7 ms | Surgical, and unusable live |

**The relationship is worth internalising: transition width is inversely proportional to tap
count.** Halving the transition width doubles the taps, doubles the CPU, and doubles the latency.
This is the time-frequency trade-off from Chapter 13, appearing for the third time.

**3. A low-pass sweep.** A 500-tap low-pass whose cutoff sweeps from 20 kHz down to 200 Hz over
eight seconds, applied to a full-spectrum source. This is the classic filter sweep, and it shows
that a filter is not a volume control: the sound gets *darker*, not quieter, until very low
cutoffs.

Note that sweeping the cutoff means **redesigning the filter** each time, which is expensive
(hundreds of `sin` calls) and technically makes the system time-varying (Chapter 20). For real
sweeps you use an IIR filter whose coefficients are cheap to recompute — which is Chapter 23,
and which is exactly why IIR dominates in synthesisers.

**4. Measured frequency response.** The program prints the measured `|H(ω)|` in dB at a set of
frequencies, alongside the design target, so you can verify the filter does what it claims:

```
  1 kHz low-pass, 101 taps, Hamming:
        50 Hz:   -0.00 dB
       200 Hz:   -0.00 dB
       500 Hz:   -0.01 dB
       900 Hz:   -0.31 dB
      1000 Hz:   -6.02 dB      <-- the -6 dB point is AT the cutoff
      1100 Hz:  -14.21 dB
      1500 Hz:  -44.80 dB
      2000 Hz:  -52.11 dB
      5000 Hz:  -51.30 dB
     10000 Hz:  -53.02 dB
```

**Note the −6 dB at the cutoff, not −3 dB.** The windowed-sinc design places the *half-amplitude*
point at the nominal cutoff. IIR filters conventionally use the −3 dB (half-power) point. The two
families use different conventions, and comparing a "1 kHz" FIR against a "1 kHz" biquad without
knowing this leads to confusion. Chapter 23 uses −3 dB.

Also note the stopband sits around −50 dB, which is Hamming's −41 dB sidelobe plus the roll-off.
Switch to Blackman and it drops to about −74 dB, at the cost of a wider transition.

---

## 22.9 When to use FIR

| Use FIR when | Use IIR when |
|---|---|
| You need exactly linear phase | Latency must be minimal |
| Building a crossover that must sum flat | CPU is tight |
| Applying a measured response (cab IR, room correction) | The cutoff needs to sweep in real time |
| The filter must be guaranteed stable | You need a steep filter cheaply |
| Offline processing where latency is free | Emulating analogue circuits |
| Very long responses (reverb) | Standard EQ, synth filters |

In practice: **synthesisers and mixing EQs are almost always IIR; mastering EQs, crossovers,
resamplers and convolution processors are FIR.** Chapter 23 builds the IIR side.

---

## 22.10 Exercises

**22.1** Design a 1 kHz low-pass at 44.1 kHz with 51 taps and a Hamming window. Print the
coefficients. Verify they are symmetric and that they sum to 1.0.

**22.2** Implement `frequencyResponse` from §22.2 and plot `|H(ω)|` in dB at 200 points from 0 to
Nyquist for that filter. At what frequency is it −6 dB? −40 dB?

**22.3** *Deliberate breakage.* Remove the `sinc(0)` special case. What appears in the
coefficients? Convolve with it and listen (volume at zero). This is the most common FIR bug.

**22.4** Design the same filter with all six window types and compare stopband rejection and
transition width numerically. Build a table. Does it match §22.5?

**22.5** Verify spectral inversion: design a low-pass and a high-pass at the same cutoff, add
their outputs, and confirm the sum reproduces the input (delayed by `M/2`). What does this prove
about their crossover?

**22.6** Use the Kaiser design formulas to build a filter with 80 dB rejection and a 200 Hz
transition at 48 kHz. How many taps does it need? Verify the measured rejection matches.

**22.7** Measure the latency of a 501-tap filter empirically: send an impulse through and find
which output sample is largest. Does it equal `(M-1)/2`?

**22.8** Demonstrate pre-ringing: filter a sharp impulse with a 2001-tap linear-phase low-pass
and look at the output in Audacity. How much ringing appears *before* the impulse? Now do the
same with Chapter 23's biquad and compare.

**22.9** Implement the doubled-buffer optimisation from §22.7's performance note. Time both
versions on a 501-tap filter over 10 seconds of audio. What speedup do you get?

**22.10** Build a 3-band crossover using linear-phase FIR filters (low-pass at 200 Hz, band-pass
200–2000 Hz, high-pass at 2000 Hz). Sum the three bands and measure the difference from the
original. It should be essentially zero — if it is not, why not?

---

### Chapter summary

- An **FIR filter is convolution with a short impulse response**, and **its coefficients *are*
  its impulse response**.
- FIR is **always stable**, can have **exactly linear phase** (if symmetric), but needs many taps
  to be sharp and costs one MAC per tap.
- The ideal brick-wall low-pass has a **sinc** impulse response: infinitely long and non-causal.
  Shifting makes it causal (at the cost of `M/2` samples of latency); truncating makes it finite.
- **Truncating abruptly fails**: the Gibbs phenomenon gives about 9% overshoot (−21 dB stopband)
  that **never improves with more taps**.
- **Windowing** — tapering the sinc smoothly to zero — fixes it. Gentler window = lower ripple
  but wider transition. Kaiser lets you dial the trade-off, and its design formulas take you from
  a specification straight to a tap count.
- **Watch for `sinc(0)`** — it is `0/0` and must be special-cased, or you get NaN.
- **Normalise the coefficients** so the DC gain is exactly 1.
- `highpass = δ − lowpass` (spectral inversion) gives you all four filter types from one design
  function.
- **Transition width is inversely proportional to tap count.** Halving it doubles CPU *and*
  latency — the time-frequency trade-off again.
- FIR uses the **−6 dB** convention at cutoff; IIR uses **−3 dB**. Do not compare them naively.
- Linear phase is genuinely useful for crossovers, parallel paths and mastering — and it costs
  latency and **pre-ringing**, which is its own kind of inaccuracy.

**Next:** [Chapter 23 — IIR Filters and the Biquad](23-iir-filters-and-biquads.md)
