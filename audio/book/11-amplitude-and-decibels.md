# Chapter 11 — Amplitude, Decibels, and Loudness

> In Chapter 10 you heard amplitude 0.8 and amplitude 0.25 and thought "a bit quieter", even
> though one is more than three times the other. This chapter explains why, and gives you the
> unit that every audio professional actually works in.

---

## 11.1 The problem with linear amplitude

Our samples run from −1.0 to +1.0. Amplitude 0.5 is half of 1.0. Simple, and completely useless
as a description of how loud something sounds.

Three concrete demonstrations of the mismatch:

**1. A 10 million-to-one range.** Chapter 2's table: the threshold of hearing is 0.00002 Pa, the
threshold of pain is 63 Pa. In our −1.0 … +1.0 world, if full scale is "painfully loud", then
the quietest audible sound is around 0.0000003. A volume fader running 0.0 to 1.0 linearly would
put every interesting value in the bottom hundredth of its travel. The top 90% of the fader would
be "loud" to "slightly louder".

**2. Perception is ratio-based.** Going from 0.1 to 0.2 sounds like the same increase as going
from 0.4 to 0.8. Both are a doubling. In linear terms one adds 0.1 and the other adds 0.4, which
tells you nothing useful.

**3. The numbers become unreadable.** "Turn it up by 0.0032" is meaningless. "Turn it up 3 dB" is
an instruction any engineer can follow.

What we need is a unit where **equal steps feel equal**. That means a logarithmic unit, because
logarithms turn ratios into differences — which is exactly the transformation that matches the
cochlea (Chapter 3).

---

## 11.2 Logarithms, from scratch

If you are comfortable with logs, skip to §11.3. If they are a vague memory from school, read
this section carefully — it is short, and the whole chapter rests on it.

**A logarithm answers the question: "what power do I raise this base to, in order to get that
number?"**

```
   log₁₀(1000) = 3        because 10³ = 1000
   log₁₀(100)  = 2        because 10² = 100
   log₁₀(10)   = 1        because 10¹ = 10
   log₁₀(1)    = 0        because 10⁰ = 1
   log₁₀(0.1)  = -1       because 10⁻¹ = 0.1
   log₁₀(0.01) = -2
```

That is the whole definition. In C++, `std::log10(x)`.

### The property that makes logs useful

```
   log(a × b) = log(a) + log(b)
```

**Multiplication becomes addition.** That is the magic, and it is exactly what we want, because:

- Perception works in ratios (multiplication).
- Humans and meters work best with differences (addition).

So taking the log of an amplitude ratio converts "three times louder" into a number you can add
and subtract.

Two more properties you will use:

```
   log(a / b)  = log(a) - log(b)
   log(aⁿ)     = n · log(a)
```

### Some values to know on sight

| x | log₁₀(x) |
|---|---|
| 1 | 0 |
| 1.259 | 0.1 |
| 1.414 (√2) | 0.1505 |
| 2 | 0.301 |
| 3.162 (√10) | 0.5 |
| 10 | 1 |
| 100 | 2 |
| 1000 | 3 |

`log₁₀(2) = 0.301` is the one to memorise. It is the reason 6 dB is a doubling, as we are about
to see.

### A crucial caveat

```
   log₁₀(0) = -infinity
```

There is no power of 10 that gives zero. You can get arbitrarily close by going to very negative
exponents, but never there. In code this means **taking the log of a zero sample gives you
`-inf`**, which then propagates through your arithmetic and can turn into NaN. Every dB
conversion function must guard against it. §11.6 does.

---

## 11.3 The bel and the decibel

The unit was invented at Bell Labs to describe signal loss along telephone lines. One **bel** is
a factor of 10 in **power**:

```
   bels = log₁₀(P₁ / P₀)
```

A bel turned out to be an inconveniently large step, so the working unit is a tenth of one — the
**decibel**:

```
   dB = 10 · log₁₀(P₁ / P₀)
```

### Two formulas, and why

Here is the part that confuses people for years. There are two decibel formulas:

```
   For POWER:      dB = 10 · log₁₀(P₁ / P₀)
   For AMPLITUDE:  dB = 20 · log₁₀(A₁ / A₀)
```

They are not two different conventions. They are the *same* formula, because **power is
proportional to amplitude squared**:

```
   P ∝ A²
```

(Double the pressure swing and you have four times the energy — the same relationship as
kinetic energy and velocity, or electrical power and voltage.)

So:

```
   10 · log₁₀(P₁/P₀) = 10 · log₁₀((A₁/A₀)²) = 10 · 2 · log₁₀(A₁/A₀) = 20 · log₁₀(A₁/A₀)
```

The 20 is just the 10 with the square pulled out by the `log(aⁿ) = n·log(a)` rule.

**Which one do you use?** In digital audio, your samples are amplitude values. **Use 20.**

```cpp
double gainToDb(double gain) { return 20.0 * std::log10(gain); }
double dbToGain(double db)   { return std::pow(10.0, db / 20.0); }
```

You will see the 10-version in acoustics papers dealing with intensity and in some measurement
contexts. If a number seems to be off by a factor of two, this is almost always why.

### The decibel is always a ratio

**A decibel value on its own means nothing.** It is always relative to something. "The signal is
−12 dB" is an incomplete sentence until you know −12 dB relative to *what*.

This is why the suffixes exist:

| Unit | Reference (0 dB is...) | Used for |
|---|---|---|
| **dBFS** | Digital full scale, amplitude 1.0 | Digital audio. **Our default.** |
| dBSPL | 20 µPa, the threshold of hearing | Acoustic level in air |
| dBu | 0.775 V RMS | Professional analogue gear |
| dBV | 1.0 V RMS | Consumer analogue gear |
| dBm | 1 milliwatt into 600 Ω | Old telephony |
| **LUFS / LKFS** | Full scale, K-weighted and time-integrated | Broadcast/streaming loudness |
| dBTP | Full scale, measured with oversampling | True peak, inter-sample peaks |

In **dBFS**, amplitude 1.0 is 0 dBFS, and every real signal is negative:

```
   amplitude 1.0    ->     0 dBFS   (full scale)
   amplitude 0.5    ->    -6 dBFS
   amplitude 0.25   ->   -12 dBFS
   amplitude 0.1    ->   -20 dBFS
   amplitude 0.01   ->   -40 dBFS
   amplitude 0.001  ->   -60 dBFS
```

When a mix engineer says "bring the snare down 2 dB", they mean 2 dB relative to where it is —
a *relative* change, which is the most common usage of all and needs no reference at all.

---

## 11.4 The table you must memorise

These come up constantly. Learn the first six by heart; you will use them daily.

| dB change | Amplitude factor | What it means |
|---|---|---|
| **+6 dB** | **× 2** | Double the amplitude. One more bit of resolution. |
| **−6 dB** | **× 0.5** | Half the amplitude. |
| **+3 dB** | **× 1.414** (√2) | Double the *power*. Just-noticeably louder. |
| **−3 dB** | **× 0.707** | Half the power. The "half-power point" of a filter. |
| **+10 dB** | **× 3.16** | Roughly **twice as loud** perceptually. |
| **+20 dB** | **× 10** | Ten times the amplitude. |
| **+1 dB** | × 1.122 | About the smallest change most people notice in a mix. |
| +12 dB | × 4 | |
| +40 dB | × 100 | |
| −40 dB | × 0.01 | |
| −60 dB | × 0.001 | Usually considered "silence" in a decay measurement |
| −96 dB | × 0.0000158 | The 16-bit noise floor |

Three of these deserve comment.

**+6 dB = double amplitude.** Because `20 · log₁₀(2) = 20 × 0.301 = 6.02`. This is why bit depth
gives "6 dB per bit" (Chapter 4) — each extra bit doubles the number of levels.

**+3 dB = double power.** Because `10 · log₁₀(2) = 3.01`. The **−3 dB point** is the standard
definition of a filter's cutoff frequency: the frequency at which half the power gets through,
i.e. amplitude 0.707. Chapter 23 uses this constantly. You met 0.707 in Chapter 5's
`mathcheck.cpp` and now you know why it was there.

**+10 dB ≈ twice as loud.** This is perceptual (Chapter 3), not physical, and it is why the
decibel scale feels right: a 10 dB step is a *musically meaningful* change, and mixers work in
1–3 dB increments for fine adjustments.

---

## 11.5 Peak, RMS, and crest factor

Chapter 2 introduced these. Now we can be precise, because they are all measured in dB.

### Peak

```
   peak = max over all samples of |x[n]|
   peak_dBFS = 20 · log₁₀(peak)
```

Peak tells you exactly one thing: **how close you are to clipping**. It tells you nothing
useful about loudness. A single-sample click can peak at 0 dBFS and be inaudible.

### RMS — root mean square

The name is the recipe, read backwards:

1. **Square** every sample. (This makes everything positive, and weights large values more.)
2. Take the **mean** of those squares.
3. Take the square **root**.

```
   RMS = sqrt( (1/N) · Σ x[n]² )
```

Why not just average the absolute values? Because squaring corresponds to **power**, and power
is what corresponds to perceived loudness and to what a meter or a speaker actually has to
deliver. RMS is the "equivalent steady level" of a varying signal — the DC value that would
deliver the same power.

For a sine wave of amplitude A, the RMS works out to:

```
   RMS = A / √2 ≈ 0.7071 · A
```

so a full-scale sine measures **−3.01 dBFS RMS** while peaking at 0 dBFS. (This is a genuine
source of confusion in calibration: a "0 dBFS sine" and a "0 dBFS square" have different RMS
levels, and some meters are calibrated so that a full-scale sine reads 0.)

For a square wave of amplitude A, RMS = A — the signal is always at full magnitude, so peak and
RMS coincide. This is why square waves sound so much louder than sines at the same peak: they
have 3 dB more RMS, plus a spectrum full of harmonics in the ear's sensitive region.

### Crest factor

```
   crest factor (dB) = peak_dBFS - RMS_dBFS
```

The gap between the loudest moment and the average. It is one of the most informative single
numbers you can compute about a piece of audio.

| Signal | Typical crest factor |
|---|---|
| Square wave | 0 dB |
| Sine wave | 3 dB |
| Heavily limited modern pop master | 6–9 dB |
| Typical rock mix | 10–14 dB |
| Acoustic jazz, classical | 15–20 dB |
| Solo percussion, uncompressed drums | 20–25 dB |
| **Film soundtrack (cinema mix)** | **25–35 dB** |

That last row is the whole story of cinematic sound in one number. A film mix keeps enormous
crest factor so that when the explosion arrives, it has somewhere to go. A pop master throws the
crest factor away to maximise average loudness. Neither is wrong; they are made for different
playback situations. Chapters 50 and 89 develop this at length, and Chapter 83 uses it to
explain why "make it louder" is the wrong tool for "make it bigger".

### And LUFS, briefly

RMS is better than peak for loudness, but it is still not how the ear works, because it weights
all frequencies equally and Chapter 3 showed that the ear does not.

**LUFS** (Loudness Units relative to Full Scale, standardised as ITU-R BS.1770) fixes this:
apply a "K-weighting" filter that approximates the ear's frequency response, then take a
gated, time-integrated mean square. It is the unit Spotify, YouTube, Netflix and every
broadcaster now use to normalise everything they play.

We build a compliant LUFS meter in Chapter 89. For now, know that it exists and that it is
what "loudness" means professionally.

---

## 11.6 The dB utilities

**Code — `code/ch11/decibels.h`**

```cpp
#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace db
{
    // Below this amplitude we report -infinity as a large negative number
    // instead. -200 dB is around 1e-10, far below any audible or
    // representable signal, so this is a safe floor.
    constexpr double kMinusInfinityDb = -200.0;
    constexpr double kTinyGain        = 1e-10;

    // Amplitude ratio -> decibels. Uses 20*log10 (see section 11.3).
    inline double gainToDb(double gain)
    {
        const double a = std::fabs(gain);
        return (a < kTinyGain) ? kMinusInfinityDb : 20.0 * std::log10(a);
    }

    // Decibels -> amplitude ratio.
    inline double dbToGain(double decibels)
    {
        return (decibels <= kMinusInfinityDb) ? 0.0
                                              : std::pow(10.0, decibels / 20.0);
    }

    // Power ratio -> decibels. Uses 10*log10.
    inline double powerToDb(double power)
    {
        return (power < kTinyGain * kTinyGain) ? kMinusInfinityDb
                                               : 10.0 * std::log10(power);
    }

    // --- measurements over a buffer ---------------------------------

    inline double peak(const std::vector<float>& buf)
    {
        double p = 0.0;
        for (float s : buf)
            p = std::max(p, static_cast<double>(std::fabs(s)));
        return p;
    }

    inline double rms(const std::vector<float>& buf)
    {
        if (buf.empty())
            return 0.0;

        double sumSquares = 0.0;            // accumulate in double -- Chapter 7
        for (float s : buf)
            sumSquares += static_cast<double>(s) * static_cast<double>(s);

        return std::sqrt(sumSquares / static_cast<double>(buf.size()));
    }

    inline double peakDb(const std::vector<float>& buf)  { return gainToDb(peak(buf)); }
    inline double rmsDb (const std::vector<float>& buf)  { return gainToDb(rms(buf));  }

    inline double crestFactorDb(const std::vector<float>& buf)
    {
        return peakDb(buf) - rmsDb(buf);
    }

    // --- DC offset ---------------------------------------------------
    // The mean sample value. Should be ~0 for any healthy audio signal.

    inline double dcOffset(const std::vector<float>& buf)
    {
        if (buf.empty())
            return 0.0;

        double sum = 0.0;
        for (float s : buf)
            sum += static_cast<double>(s);

        return sum / static_cast<double>(buf.size());
    }
}
```

**Walkthrough**

`namespace db { ... }` groups these without polluting the global namespace, so you call
`db::gainToDb(0.5)`. Since all the functions are short, they are `inline` and live entirely in
the header — no `.cpp` needed.

`inline` here means "it is fine for this definition to appear in multiple translation units".
Without it, including this header from two `.cpp` files would cause a duplicate-definition link
error.

**The `kTinyGain` guard is the important part.** `std::log10(0.0)` returns `-inf`. Printed, that
is `-inf` in your meter display; used in arithmetic, `-inf - -inf` is NaN, and Chapter 6 warned
what NaN does. Clamping to −200 dB is the standard approach: it is far below anything
representable, so nothing is lost, and everything downstream stays finite.

`std::fabs(gain)` — a negative gain is a *polarity inversion*, not a negative loudness. A gain of
−0.5 is the same magnitude as +0.5, just phase-inverted. The dB value should be −6.02 for both.

---

## 11.7 A program that measures

**Code — `code/ch11/levels.cpp`**

```cpp
#include "decibels.h"
#include "../ch09/wavwriter.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

std::vector<float> makeSine(double freq, double amp, double seconds, double sr)
{
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;
    for (auto& s : out)
    {
        s = static_cast<float>(amp * std::sin(phase));
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
    return out;
}

std::vector<float> makeSquare(double freq, double amp, double seconds, double sr)
{
    std::vector<float> out(static_cast<size_t>(seconds * sr));
    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;
    for (auto& s : out)
    {
        s = static_cast<float>(phase < kPi ? amp : -amp);
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
    return out;
}

void report(const std::string& name, const std::vector<float>& buf)
{
    std::cout << std::fixed << std::setprecision(2)
              << std::setw(22) << std::left  << name << std::right
              << "  peak " << std::setw(7) << db::peakDb(buf) << " dBFS"
              << "   RMS " << std::setw(7) << db::rmsDb(buf)  << " dBFS"
              << "   crest " << std::setw(6) << db::crestFactorDb(buf) << " dB"
              << "   DC " << std::setw(8) << std::setprecision(5)
              << db::dcOffset(buf) << "\n"
              << std::setprecision(2);
}

int main()
{
    const double sr = 44100.0;

    std::cout << "--- dB conversion table ---\n";
    std::cout << std::fixed << std::setprecision(4);
    for (double g : { 1.0, 0.707, 0.5, 0.25, 0.1, 0.01, 0.001 })
        std::cout << "  gain " << std::setw(7) << g
                  << "  ->  " << std::setw(8) << std::setprecision(2)
                  << db::gainToDb(g) << " dB\n" << std::setprecision(4);

    std::cout << "\n";
    for (double d : { 0.0, -1.0, -3.0, -6.0, -10.0, -20.0, -40.0, -60.0 })
        std::cout << "  " << std::setw(6) << std::setprecision(1) << d
                  << " dB  ->  gain " << std::setprecision(6)
                  << db::dbToGain(d) << "\n";

    std::cout << "\n--- signal measurements ---\n";

    report("sine 1.0",        makeSine  (440.0, 1.0,  1.0, sr));
    report("sine 0.5",        makeSine  (440.0, 0.5,  1.0, sr));
    report("sine 0.25",       makeSine  (440.0, 0.25, 1.0, sr));
    report("square 1.0",      makeSquare(440.0, 1.0,  1.0, sr));
    report("square 0.5",      makeSquare(440.0, 0.5,  1.0, sr));

    // An impulse: one sample at full scale, the rest silent.
    {
        std::vector<float> impulse(static_cast<size_t>(sr), 0.0f);
        impulse[22050] = 1.0f;
        report("impulse", impulse);
    }

    // A signal with a deliberate DC offset.
    {
        auto buf = makeSine(440.0, 0.4, 1.0, sr);
        for (auto& s : buf) s += 0.2f;
        report("sine 0.4 + DC 0.2", buf);
    }

    std::cout << "\n--- the -6 dB ladder (listen) ---\n";
    {
        std::vector<float> out;
        for (int step = 0; step < 8; ++step)
        {
            const double gain = db::dbToGain(-6.0 * step);
            std::cout << "  step " << step << ": " << std::setprecision(1)
                      << -6.0 * step << " dB  gain " << std::setprecision(5)
                      << gain << "\n";

            auto part = makeSine(440.0, gain, 0.5, sr);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("db_ladder.wav", out, static_cast<int>(sr), 1);
        std::cout << "  -> wrote db_ladder.wav\n";
    }

    return 0;
}
```

**Expected output (abridged)**

```
--- dB conversion table ---
  gain  1.0000  ->      0.00 dB
  gain  0.7070  ->     -3.02 dB
  gain  0.5000  ->     -6.02 dB
  gain  0.2500  ->    -12.04 dB
  gain  0.1000  ->    -20.00 dB
  gain  0.0100  ->    -40.00 dB
  gain  0.0010  ->    -60.00 dB

     0.0 dB  ->  gain 1.000000
    -1.0 dB  ->  gain 0.891251
    -3.0 dB  ->  gain 0.707946
    -6.0 dB  ->  gain 0.501187
   -10.0 dB  ->  gain 0.316228
   -20.0 dB  ->  gain 0.100000
   -40.0 dB  ->  gain 0.010000
   -60.0 dB  ->  gain 0.001000

--- signal measurements ---
sine 1.0                peak    0.00 dBFS   RMS   -3.01 dBFS   crest   3.01 dB   DC  0.00000
sine 0.5                peak   -6.02 dBFS   RMS   -9.03 dBFS   crest   3.01 dB   DC  0.00000
sine 0.25               peak  -12.04 dBFS   RMS  -15.05 dBFS   crest   3.01 dB   DC  0.00000
square 1.0              peak    0.00 dBFS   RMS   -0.00 dBFS   crest   0.00 dB   DC  0.00000
square 0.5              peak   -6.02 dBFS   RMS   -6.02 dBFS   crest   0.00 dB   DC  0.00000
impulse                 peak    0.00 dBFS   RMS  -46.44 dBFS   crest  46.44 dB   DC  0.00002
sine 0.4 + DC 0.2       peak   -3.52 dBFS   RMS   -9.05 dBFS   crest   5.53 dB   DC  0.20000
```

### Reading the results

**Every sine has a crest factor of exactly 3.01 dB**, regardless of amplitude. That is `20 ·
log₁₀(√2)`. Crest factor is a property of the *shape*, not the level — which is exactly why it is
useful: it survives any gain change.

**Every square has a crest factor of 0 dB.** Peak equals RMS. This confirms §11.5's claim, and it
is why a square at −6 dBFS peak is 3 dB louder than a sine at −6 dBFS peak.

**The impulse has a crest factor of 46 dB.** One sample at full scale, 44,099 at zero. Peak says
"as loud as it gets"; RMS says "essentially silence". Play it and you hear a faint tick. **This is
the clearest possible proof that peak level is not loudness**, and it is exactly the situation a
peak-limiter has to handle: enormous peaks carrying almost no energy.

**The DC offset example.** Peak went up (the whole waveform shifted toward +1.0, eating 2 dB of
headroom) and the crest factor got worse, all for a component you cannot hear. DC offset is pure
waste: it costs headroom, it can heat speaker coils, and it makes every downstream meter lie.
Chapter 14 adds a DC-blocking filter.

### Listen to `db_ladder.wav`

Eight 440 Hz tones, each 6 dB below the last: 0, −6, −12, −18, −24, −30, −36, −42 dBFS.

**Each step is the same perceptual size.** That is the whole point of the unit: equal dB steps
feel equal, even though in linear amplitude the first step subtracts 0.5 and the last subtracts
0.004.

Count how many steps you can still hear before it vanishes into your system's noise floor. On
good headphones in a quiet room you should hear all eight. On a laptop in a noisy room, maybe
five. That difference is your listening environment's dynamic range, and it is worth knowing.

**Experiment 11.1.** Change the ladder to 1 dB steps over 20 steps. Can you reliably hear each
step? Most people can detect about 1 dB in a direct A/B comparison, and about 3 dB in a mix.
Test yourself honestly — it calibrates how much precision your mixing decisions actually need.

**Experiment 11.2.** Make a ladder of *linear* steps instead: amplitudes 1.0, 0.875, 0.75, 0.625,
... 0.125. Listen. The early steps sound tiny and the last ones sound enormous. This is precisely
why fader laws are logarithmic.

---

## 11.8 Gain staging and headroom

Some practical discipline that follows directly from the numbers above.

**Headroom** is the gap between your loudest peak and 0 dBFS. It is insurance.

In a **float** pipeline (which yours is), exceeding 0 dBFS internally costs nothing — Chapter 4
explained why. A signal at +12 dBFS inside your program is undamaged; multiply by `dbToGain(-12)`
and it is back, bit for bit.

But at the **output** — when you write a 16-bit WAV, or hand samples to the sound card — the
ceiling is real. Above 1.0, the conversion clamps, which is clipping, which is audible harsh
distortion.

Working practice:

- **Aim for peaks around −6 to −3 dBFS** in a finished render. This leaves room for inter-sample
  peaks (Chapter 4) and for any lossy encoding, which can overshoot by a decibel or two.
- **Do not normalise to 0.0 dBFS.** It gains you nothing audible and guarantees true-peak
  overshoots. Chapter 7's `normalise()` defaults to 0.99 for this reason.
- **Watch gain accumulation.** Every stage that adds energy — summing, resonant filters,
  saturation, reverb — can push you up. Measure between stages; do not assume.

### What happens when you add signals

This one is not intuitive and it matters for mixing.

**Correlated signals** (identical, or nearly so) add **linearly**. Two copies of the same sine
give exactly double the amplitude: **+6 dB**.

**Uncorrelated signals** (different sources, independent noise) add in **power**, because their
peaks do not line up. Two independent signals of equal RMS give `√2` times the RMS: **+3 dB**.

| Number of sources | Correlated | Uncorrelated |
|---|---|---|
| 2 | +6.0 dB | +3.0 dB |
| 4 | +12.0 dB | +6.0 dB |
| 8 | +18.0 dB | +9.0 dB |
| 16 | +24.0 dB | +12.0 dB |
| 64 | +36.0 dB | +18.0 dB |

Practical consequences:

- **A 24-track mix does not need 24× the headroom.** Mostly-uncorrelated tracks sum at roughly
  +3 dB per doubling, so 24 tracks is around +14 dB over one track, not +28 dB.
- **But the bass and kick *are* correlated** in the low frequencies, so they add closer to +6 dB.
  This is why low end is where mixes run out of headroom first.
- **A stack of detuned oscillators** (Chapter 84's braam) is partially correlated: it adds
  somewhere between +3 and +6 dB per doubling, drifting between the two as the phases slide. This
  is precisely why a braam *breathes* — the level genuinely fluctuates as correlation changes.
- **When layering identical samples** to make something bigger, you gain +6 dB of level and
  nothing else. To gain *size*, the layers must differ — different pitches, different timing,
  different spectra. Chapter 82 is built on this observation.

**Experiment 11.3.** Generate 8 sines at 440 Hz, all identical, and sum them. Measure the RMS.
Then generate 8 sines at 440, 441, 442... 447 Hz and sum those. Measure again. Predict the
difference from the table before you run it.

---

## 11.9 Fader laws

A short but practical aside, since you will build interfaces eventually.

A volume fader that maps its position linearly to gain is unusable — §11.1 explained why. Real
faders map position to **decibels** and then convert.

A simple and decent law:

```cpp
// position: 0.0 (silent) .. 1.0 (unity)
double faderToGain(double position)
{
    if (position <= 0.0)
        return 0.0;
    const double db = -60.0 * (1.0 - position);   // 0.0 -> -60 dB, 1.0 -> 0 dB
    return db::dbToGain(db);
}
```

This gives a fader with 60 dB of range, linear in dB. Real consoles use a **piecewise** law with
finer resolution near unity — typically 0 dB at about 75% of travel, with the top quarter giving
up to +10 dB of boost and the range expanding rapidly toward the bottom, so the last few percent
covers −60 dB to −∞. That shape exists because mixers spend their time making 1 dB adjustments
near unity and rarely care about the difference between −52 and −55 dB.

The lesson generalises: **any parameter that is perceived logarithmically should have a
logarithmic control.** Frequency, time (delay, decay), and level are all in this category.
Chapter 34 applies it to modulation ranges, and it is one of the most common ways amateur audio
interfaces feel wrong.

---

## 11.10 Exercises

**11.1** Without a calculator: what amplitude is −18 dBFS? What is the dB value of amplitude
0.125? (Hint: both are multiples of 6.)

**11.2** Prove algebraically that `20·log₁₀(A)` and `10·log₁₀(A²)` are equal. Then prove that
doubling amplitude is +6.02 dB.

**11.3** Write `double dbDifference(const std::vector<float>& a, const std::vector<float>& b)`
returning the RMS difference between two signals in dB. Use it to check that applying
`dbToGain(-6.0)` to a signal really reduces it by 6 dB.

**11.4** Compute the crest factor of: a sine, a square, a triangle, white noise, and a single
impulse. Predict each before measuring. (Triangle is `√3 ≈ 4.77` dB; white noise is around 11–12
dB, though it varies with the length of the measurement — explain why.)

**11.5** Generate a signal that peaks at exactly 0 dBFS but has an RMS of −40 dBFS. What does it
sound like? What would a peak meter and a loudness meter each say about it?

**11.6** Implement `powerSum(int n)` returning the dB increase from summing `n` uncorrelated
sources, and verify the table in §11.8 by actually generating and summing noise. (You will need
Chapter 15's noise generator, or use `std::mt19937` directly.)

**11.7** *Deliberate breakage.* Remove the `kTinyGain` guard from `gainToDb` and call it on a
buffer of silence. What is printed? Now use the result in arithmetic. Then put the guard back.

**11.8** A film mix has dialogue at −27 LUFS and an explosion peaking at −1 dBTP. Estimate the
crest factor of the whole programme. Now the same content is normalised for streaming at −14
LUFS integrated. What must happen to the explosion, and what does that do to the emotional
effect? (This is the central tension of Chapter 89; a paragraph of reasoning is enough.)

**11.9** Implement a simple peak meter with **ballistics**: instant attack, and a release that
falls at 20 dB per second. Run it over `db_ladder.wav` and print the meter reading every 100 ms.
Why do real meters need a slow release?

---

### Chapter summary

- Linear amplitude does not match perception. Decibels do, because logarithms turn ratios into
  differences.
- **Amplitude: `dB = 20·log₁₀(A₁/A₀)`. Power: `dB = 10·log₁₀(P₁/P₀)`.** They are the same formula
  because `P ∝ A²`. In digital audio, use 20.
- A dB figure is always relative to something. **dBFS** (full scale = 1.0) is our working unit.
- Memorise: **+6 dB = ×2**, **−6 dB = ×0.5**, **±3 dB = double/half power**, **+10 dB ≈ twice as
  loud**, **+20 dB = ×10**.
- **Peak** tells you about clipping. **RMS** tells you about loudness. **Crest factor** (peak −
  RMS) tells you about dynamics, and is the single number that distinguishes a film mix (25–35
  dB) from a loudness-war pop master (6–9 dB).
- Always guard dB conversions against zero; `log10(0)` is `-inf` and leads to NaN.
- Leave headroom: render peaks at −6 to −3 dBFS. Internal float overs are harmless; output overs
  are not.
- Summing: correlated signals add **+6 dB** per doubling, uncorrelated **+3 dB**. Layering
  identical sounds buys level, not size.
- Any logarithmically-perceived parameter — level, frequency, time — needs a logarithmic control.

**Next:** [Chapter 12 — Square, Saw, Triangle: The Classic Waveforms](12-classic-waveforms.md)
