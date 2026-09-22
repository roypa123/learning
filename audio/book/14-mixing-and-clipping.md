# Chapter 14 — Mixing, Gain Staging, and Clipping

> Mixing is addition. That is the entire operation. Everything difficult about mixing is
> bookkeeping around the fact that the sum can get too big — plus the small matter of taste,
> which we will come to in Part VIII.

---

## 14.1 Summing

Chapter 2 established superposition: sounds arriving at the same point add. So mixing two
signals is:

```cpp
for (size_t i = 0; i < n; ++i)
    out[i] = a[i] + b[i];
```

And mixing many, each with its own level:

```cpp
for (size_t i = 0; i < n; ++i)
{
    float sum = 0.0f;
    for (size_t t = 0; t < tracks.size(); ++t)
        sum += tracks[t][i] * gains[t];
    out[i] = sum;
}
```

That is a mixing console. A real console adds EQ, dynamics, routing, and a great deal of
ergonomics, but the summing bus is those three lines.

> **A note on "summing quality".** You will encounter claims that some DAWs have better "summing
> engines" than others. In a modern 32-bit-float DAW this is essentially folklore: addition is
> addition, and the results are bit-identical or differ in the 24th significant bit. Where real
> differences arise, they are in pan laws, in whether faders are smoothed, in internal
> oversampling, and in dither — all of which are design choices, not qualities of the addition.
> You now know enough to test such claims yourself, which is the point of Chapter 1's remark
> about folklore.

---

## 14.2 The headroom problem

Two signals each peaking at 0.8 can sum to 1.6. That is over full scale, and at the output it
will clip.

Chapter 11 gave the statistics: correlated signals add at +6 dB per doubling, uncorrelated at
+3 dB. But statistics describe the *average*; the *peak* can be as bad as the simple sum of all
peaks, which is what matters for clipping.

There are three strategies, and professionals use all three in combination.

### Strategy 1: attenuate on the way in

Divide each source before summing:

```cpp
out[i] = (a[i] + b[i]) * 0.5f;              // average instead of sum
```

**Advantage:** cannot clip. **Disadvantage:** everything gets quieter as you add tracks, and with
mostly-uncorrelated material the result is far too quiet — dividing 24 tracks by 24 loses about
14 dB more than necessary.

A better version divides by `√N` rather than `N`, matching the uncorrelated power sum:

```cpp
const float busGain = 1.0f / std::sqrt(static_cast<float>(numTracks));
```

This is a reasonable default for automatic mixing, though it is a heuristic, not a guarantee.

### Strategy 2: work in float and fix it at the end

Sum freely, let the bus go to +12 dBFS if it wants, measure the peak, then apply one gain at the
very end.

**This is what we will do, and what modern DAWs do.** Chapter 4 explained why it is safe:
floating point has enormous range, and a signal at +12 dBFS internally is undamaged. Nothing
clips until a conversion to integer or to hardware.

```cpp
// Sum with no regard for level
for (...) out[i] = sum;

// Then, once, at the end:
const float p = peak(out);
if (p > 0.0f)
    applyGain(out, 0.891f / p);         // land at -1 dBFS
```

**The one rule:** this works only if *every* stage in between is linear. The moment you have a
nonlinear stage — a distortion, a compressor, a clipper — the level arriving at that stage
changes what it does. Then gain staging matters again, and it matters at every stage, not just
at the end. Chapter 52 returns to this.

### Strategy 3: manage levels deliberately

The professional approach. Every source arrives at a sensible level (peaks around −18 to −12
dBFS is a common convention, inherited from analogue gear where that corresponded to the sweet
spot on the meter), and the mix sums to something around −6 dBFS with room to spare.

The reason this convention persists in a float world is not headroom — it is **plugin
calibration**. Many plugins, especially analogue emulations, are designed around a specific
input level and behave differently when hit harder. Feeding one a signal at −3 dBFS when it
expects −18 dBFS gives you the saturated version of that plugin, whether you wanted it or not.

---

## 14.3 Clipping

When a sample exceeds ±1.0 and gets forced back inside, that is **clipping**.

```
   Input (too loud)           Output (hard-clipped)
        .-''''-.                  ,------,
      .'        '.                |      |
   --'------------'--         ----'      '----
```

The peaks are flattened. The waveform acquires corners.

### Why it sounds bad

Corners are discontinuities in the waveform's *slope*. Chapter 13 explained that discontinuities
produce broadband energy; the same logic applies here, and the result is a large amount of new
harmonic content that was not in the original.

Specifically, symmetric clipping — flattening the top and bottom equally — generates **odd
harmonics**: 3rd, 5th, 7th, and so on. Two consequences:

1. **Odd harmonics of a complex signal are not consonant with it.** Clipping a chord produces
   harmonics of each note, plus **intermodulation products** at the sums and differences of every
   pair of frequencies, most of which are not in the key. This is why clipped music sounds not
   just harsh but *muddy* and *dissonant*.
2. **The new harmonics extend well above Nyquist and alias** (Chapter 4). So digital clipping
   produces both harsh harmonics and inharmonic aliased garbage. This is why digital clipping is
   widely considered uglier than analogue clipping — analogue circuits clip smoothly and have no
   Nyquist.

### Hard clipping in code

```cpp
float hardClip(float x)
{
    return std::clamp(x, -1.0f, 1.0f);
}
```

**And the disaster case, from Chapter 9:** if you do *not* clamp before converting to `int16_t`,
the value wraps around instead. A sample at 1.2 becomes a large negative number — a full-scale
inverted spike. That is not clipping; that is a violent click on every over. It is worth
rendering once, deliberately, so you know the sound:

```cpp
// NEVER do this. Render it once to hear why.
int16_t broken = static_cast<int16_t>(x * 32767.0f);     // no clamp -> wraparound
```

---

## 14.4 Soft clipping and saturation

Hard clipping is abrupt. **Soft clipping** compresses the peaks gradually, so the transition into
distortion is smooth. The result has the same odd-harmonic character but far less high-order
content, and it sounds warm rather than harsh.

Three standard curves:

```cpp
// 1. Hyperbolic tangent. The classic. Smooth, symmetric, asymptotes at +/-1.
float softClipTanh(float x, float drive = 1.0f)
{
    return std::tanh(x * drive);
}

// 2. Cubic soft clip. Cheaper than tanh; linear below 1/3, curving above.
float softClipCubic(float x)
{
    if (x <= -1.0f) return -2.0f / 3.0f;
    if (x >=  1.0f) return  2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

// 3. Arctangent. Softer knee than tanh, slower approach to the ceiling.
float softClipAtan(float x, float drive = 1.0f)
{
    return static_cast<float>((2.0 / kPi) * std::atan(x * drive));
}
```

### Comparing them

```
   x:    -3  -2  -1   0   1   2   3

   hard:  -1  -1  -1   0   1   1   1        |‾‾‾‾  sharp corners
   tanh: -.995 -.96 -.76  0  .76 .96 .995   ~~~~  smooth everywhere
   cubic: -.67 -.67 -.67  0  .67 .67 .67    (clamps at 2/3)
   atan: -.795 -.70 -.50  0  .50 .70 .795   ~~~~  gentlest
```

Notice that `tanh` never quite reaches ±1 — it approaches asymptotically. That is a feature: the
output is always inside the legal range, so tanh is a **safety net** as well as an effect. Some
engines run a `tanh` on the master bus permanently for exactly this reason.

Notice also that `tanh` is **not transparent below 1.0**. `tanh(0.5) = 0.462`, not 0.5. So it is
always adding a little harmonic content, even on quiet material. Whether that is warmth or
distortion depends on how much you like it; if you want true transparency until the threshold,
use the cubic, which is exactly linear below `x = 1/3`... or a proper limiter (Chapter 51).

### What this has to do with cinematic sound

Saturation is one of the main tools for making something feel **big and physical** rather than
merely loud, and the reason is directly perceptual.

Chapter 11 explained that you cannot get "twice as loud" without +10 dB, and you do not have
+10 dB. But saturation adds harmonics, and harmonics:

- Add energy in the 2–5 kHz region where the ear is most sensitive (Chapter 3), so the sound
  gets *perceptually* louder without a peak increase.
- Fill in the spectrum, making the sound denser and harder to hear through.
- Imply, through the missing-fundamental effect (Chapter 3), more low end than is actually
  present — which is how a sub-bass drop survives on a phone speaker.

This is why every cinematic impact, braam and sub-drop goes through saturation at some stage.
Chapter 52 covers it properly, with oversampling to keep the aliasing away, and Chapter 83
applies it to the low end specifically.

---

## 14.5 DC offset and your first IIR filter

Chapter 12's pulse waveform carried a DC offset. Chapter 11 explained why that is harmful. Now
we remove it — and in doing so, build the first feedback filter in the book.

### The idea

DC is "zero hertz" — a constant. We want a filter that passes everything *except* 0 Hz: a
**high-pass filter** with a very low cutoff, typically 5–20 Hz.

The standard, minimal implementation is the **DC blocker**:

```cpp
class DCBlocker
{
public:
    void setSampleRate(double sr, double cutoffHz = 20.0)
    {
        // R controls the cutoff. Closer to 1.0 = lower cutoff.
        R = 1.0 - (2.0 * kPi * cutoffHz / sr);
        if (R < 0.0) R = 0.0;
        if (R > 0.9999999) R = 0.9999999;
    }

    float process(float x)
    {
        const double y = static_cast<double>(x) - x1 + R * y1;
        x1 = x;
        y1 = y;
        return static_cast<float>(y);
    }

    void reset() { x1 = 0.0; y1 = 0.0; }

private:
    double R  = 0.9995;
    double x1 = 0.0;      // previous input
    double y1 = 0.0;      // previous output
};
```

### Walkthrough — read this slowly, it is the template for all of Part II

```
   y[n] = x[n] - x[n-1] + R · y[n-1]
```

Three terms, each doing a distinct job.

**`x[n] - x[n-1]`** — the **difference** between this input sample and the last. This is a
discrete derivative.

Why does it remove DC? If the input is constant — say every sample is 0.3 — then
`x[n] - x[n-1] = 0.3 - 0.3 = 0`. A constant produces no output. Any *change*, however, produces
output. So this alone is already a crude DC remover.

But it is a bad one on its own: it is a differentiator, which boosts high frequencies by 6 dB per
octave. Applied to music it would sound painfully bright.

**`+ R · y[n-1]`** — feedback. A fraction `R` of the *previous output* is added back in. This
reintroduces the low frequencies that the difference stripped away, in a controlled fashion.

The balance between these two gives a high-pass response with a gentle 6 dB/octave slope and
a cutoff determined by `R`:

| R | Cutoff at 44.1 kHz | Character |
|---|---|---|
| 0.99 | ~70 Hz | Audibly thins the bass |
| 0.995 | ~35 Hz | Noticeable on sub-heavy material |
| 0.9985 | ~10 Hz | Safe: removes DC, keeps all music |
| 0.9999 | ~0.7 Hz | Very slow; may take seconds to settle |

**`x1` and `y1` are the filter's state.** They must persist between calls, which is why this is a
class rather than a function. This is the pattern Chapter 7 described: state as members,
`process()` to run.

**The state is `double`, the interface is `float`.** Chapter 6's rule. Feedback accumulates, and
`R` is very close to 1.0, so `float` precision produces measurable drift and, at very low
cutoffs, instability.

**`reset()` exists** because a filter with stale state produces a transient when you start feeding
it new material. Always provide a reset; always call it when a voice is reassigned.

### This is an IIR filter

`y[n]` depends on `y[n-1]`, which depended on `y[n-2]`, and so on back to the beginning. A single
input sample therefore influences the output **forever** (asymptotically). Hence **Infinite
Impulse Response**.

Compare with `y[n] = x[n] - x[n-1]` alone, where a single input affects exactly two outputs — a
**Finite** Impulse Response.

That distinction structures the whole of Part II:

- **FIR** (Chapter 22): no feedback. Always stable, can have perfectly linear phase, needs many
  coefficients to be sharp.
- **IIR** (Chapter 23): feedback. Very efficient — the DC blocker achieves a useful filter with
  one multiply — but can become unstable if the coefficients are wrong, and has non-linear phase.

You have now written an IIR filter, and everything in Chapter 23 is a more sophisticated version
of these same three terms.

**Experiment 14.1.** Set `R = 1.001` (greater than 1) and feed the filter any signal. The output
grows without limit — within a second it is at 10^30 and then `inf`. **This is instability**, and
now you know what it sounds and looks like. Chapter 24 explains exactly why `R > 1` diverges and
`R < 1` does not. Do this experiment with your volume at zero.

---

## 14.6 A small mixer

**Code — `code/ch14/mixer.h`** (abridged; full listing in `code/`)

```cpp
#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

struct Track
{
    std::string       name;
    std::vector<float> samples;      // mono source
    float gain  = 1.0f;              // linear gain
    float pan   = 0.0f;              // -1 = hard left, 0 = centre, +1 = hard right
    bool  mute  = false;
    bool  solo  = false;
};

class Mixer
{
public:
    void addTrack(Track t) { tracks.push_back(std::move(t)); }

    // Mix down to stereo. Returns {left, right}.
    std::vector<std::vector<float>> render() const
    {
        size_t longest = 0;
        for (const auto& t : tracks)
            longest = std::max(longest, t.samples.size());

        std::vector<std::vector<float>> out(2, std::vector<float>(longest, 0.0f));

        const bool anySolo = std::any_of(tracks.begin(), tracks.end(),
                                         [](const Track& t) { return t.solo; });

        for (const auto& t : tracks)
        {
            if (t.mute)            continue;
            if (anySolo && !t.solo) continue;

            // Constant-power pan: gains follow a quarter-circle so that
            // L^2 + R^2 stays constant. See Chapter 74 for the derivation.
            const double angle = (t.pan + 1.0) * 0.25 * kPi;   // 0 .. pi/2
            const float gl = static_cast<float>(std::cos(angle)) * t.gain;
            const float gr = static_cast<float>(std::sin(angle)) * t.gain;

            for (size_t i = 0; i < t.samples.size(); ++i)
            {
                out[0][i] += t.samples[i] * gl;
                out[1][i] += t.samples[i] * gr;
            }
        }

        return out;
    }

private:
    std::vector<Track> tracks;
    static constexpr double kPi = 3.14159265358979323846;
};
```

### Why constant-power panning

The obvious pan law is linear: `gl = (1 - pan) / 2`, `gr = (1 + pan) / 2`. At centre both are
0.5, and the two channels sum to 1.0.

The problem: over speakers, two channels at 0.5 do **not** sound as loud as one channel at 1.0.
They are uncorrelated at the listening position, so they sum in power (Chapter 11): `√(0.5² +
0.5²) = 0.707`. That is −3 dB. So a linear-panned sound **dips by 3 dB as it crosses the
centre** — audible as a hole in the middle of a pan sweep.

Constant-power panning uses `cos`/`sin` so that `gl² + gr² = 1` at every position. At centre both
gains are 0.707, the power sums to 1.0, and the loudness stays even all the way across.

```
   Linear pan:           Constant power:
   L: 1.0 ... 0.5 ... 0   L: 1.0 ... 0.707 ... 0
   R: 0   ... 0.5 ... 1.0 R: 0   ... 0.707 ... 1.0
   Power at centre: 0.5   Power at centre: 1.0
   -> 3 dB dip            -> even
```

Chapter 74 derives this properly, covers the −4.5 dB compromise law used in many consoles, and
explains why headphones want a different law from speakers.

---

## 14.7 The render program

**Code — `code/ch14/mix.cpp`** (abridged)

```cpp
int main(int argc, char** argv)
{
    const double sr = 44100.0;

    // Optional command-line control: mix.exe [outputGainDb]
    double outputGainDb = -1.0;
    if (argc > 1)
        outputGainDb = std::atof(argv[1]);

    Mixer mixer;

    // Four sources, each built from earlier chapters' code.
    mixer.addTrack({ "bass",   makeSaw(55.0,  0.6, 4.0, sr), 1.0f,  0.0f, false, false });
    mixer.addTrack({ "chord",  makeChord(220.0, 0.35, 4.0, sr), 0.8f, -0.3f, false, false });
    mixer.addTrack({ "lead",   makeSaw(440.0, 0.4, 4.0, sr), 0.6f,  0.4f, false, false });
    mixer.addTrack({ "hats",   makeHats(4.0, sr),            0.5f,  0.7f, false, false });

    auto stereo = mixer.render();

    // Report the pre-gain-staging state.
    std::cout << "Raw bus peak: " << db::peakDb(stereo[0]) << " / "
                                  << db::peakDb(stereo[1]) << " dBFS\n";

    // Remove any DC, per channel.
    for (auto& ch : stereo)
    {
        DCBlocker dc;
        dc.setSampleRate(sr, 20.0);
        for (auto& s : ch)
            s = dc.process(s);
    }

    // Three different output treatments, for comparison.
    writeVariant("mix_clipped.wav",   stereo, sr, Variant::HardClip);
    writeVariant("mix_saturated.wav", stereo, sr, Variant::Tanh);
    writeVariant("mix_normalised.wav",stereo, sr, Variant::Normalise, outputGainDb);

    return 0;
}
```

**Listen to the three variants.**

- `mix_normalised.wav` — the reference. Clean, with the full dynamic range preserved.
- `mix_saturated.wav` — louder-sounding at the same peak, denser, slightly thicker in the low
  mids. Most people prefer it on first listen, which is exactly why the loudness war happened.
- `mix_clipped.wav` — harsh. The transients have a gritty edge, and the bass sounds less defined
  because clipping generates harmonics that mask it.

Then measure all three with Chapter 11's tools. The clipped and saturated versions have
**higher RMS at the same peak**, i.e. lower crest factor. That is the trade being made: density
bought with dynamics. Chapter 89 is about deciding when that trade is worth it, and the answer
for cinematic work is usually "much less often than for pop".

---

## 14.8 Command-line arguments

Since several programs from here on take parameters, a brief explanation of `main`'s arguments:

```cpp
int main(int argc, char** argv)
```

- `argc` — the **arg**ument **c**ount, including the program name itself. So it is always ≥ 1.
- `argv` — the **arg**ument **v**ector: an array of C strings. `argv[0]` is the program name,
  `argv[1]` is the first real argument.

```bash
./mix.exe -3.0 output.wav
```

gives `argc = 3`, `argv[0] = "./mix.exe"`, `argv[1] = "-3.0"`, `argv[2] = "output.wav"`.

Convert with `std::atof` (to double), `std::atoi` (to int), or better, `std::stod`/`std::stoi`,
which throw on bad input rather than silently returning 0:

```cpp
double gain = -1.0;
if (argc > 1)
{
    try             { gain = std::stod(argv[1]); }
    catch (...)     { std::cerr << "Bad gain value, using default\n"; }
}
```

**Always check `argc` before indexing `argv`.** Reading `argv[1]` when only `argv[0]` exists is
an out-of-bounds access, and Chapter 7 explained what those do.

---

## 14.9 Exercises

**14.1** Mix two 440 Hz sines, each at amplitude 0.6, and measure the peak. Now mix a 440 Hz and
a 553 Hz sine, each at 0.6, and measure. Explain the difference using Chapter 11's correlated vs
uncorrelated rule.

**14.2** Render the same signal hard-clipped and tanh-saturated at several drive levels (1×, 2×,
4×, 8×). Measure RMS, peak and crest factor for each, and plot the numbers. At what drive does
the crest factor stop decreasing, and why?

**14.3** Take a 1 kHz sine at amplitude 2.0 (i.e. deliberately over) and hard-clip it. Look at the
spectrum in Audacity. Which harmonics appear? Are they odd, even, or both? Now clip it
asymmetrically (clamp to +1.0 and −0.5) and look again. What changed, and why?

**14.4** Implement `float asymmetricClip(float x)` that clips positive and negative peaks at
different thresholds. Asymmetric clipping produces **even** harmonics, which sound very different
from odd ones (warmer, more "tube-like"). Compare by ear.

**14.5** Feed a signal with a 0.3 DC offset through the `DCBlocker` and measure the offset before
and after. How many samples does it take to settle to within 0.001? Compute that from `R` and
check.

**14.6** *Deliberate breakage.* Set `R = 1.0` exactly in the DC blocker. What happens? (It becomes
a pure integrator, and any DC in the input accumulates without bound.) Then try `R = 1.0001`.

**14.7** Implement a `Track::gainDb` setter that stores dB and converts to linear internally, and
a fader law from Chapter 11. Verify that a fader at 75% gives unity gain.

**14.8** Extend the mixer with a **bus** concept: tracks route to a named bus, buses have their
own gain and can route to other buses. This is the architecture of every real console and of
Chapter 64's audio graph.

**14.9** Write a function that reports, for a mix, the **true peak** by upsampling 4× with linear
interpolation and taking the maximum. Compare it to the sample peak on the clipped mix. This is a
crude version of Chapter 51's true-peak meter, and it will show you inter-sample overs.

---

### Chapter summary

- Mixing is addition. `out[i] += track[i] * gain` is the whole operation.
- Summing raises level: **+6 dB per doubling for correlated, +3 dB for uncorrelated** sources.
  Peaks can be worse than either.
- Three gain-staging strategies: attenuate on input (`1/√N` is a reasonable default), sum freely
  in float and fix once at the end (our approach), or manage levels deliberately at every stage
  (necessary as soon as anything nonlinear is involved).
- **Clipping** flattens peaks, creating corners, which create odd harmonics plus intermodulation
  products plus aliasing. Digital hard clipping is the harshest form.
- **Never convert float to integer without clamping first** — wraparound turns an over into a
  full-scale inverted spike.
- **Soft clipping** (`tanh`, cubic, `atan`) distorts gradually and sounds warm rather than harsh.
  It adds perceived loudness and weight without raising the peak, which is why it is central to
  cinematic sound design.
- **DC offset** wastes headroom and can damage speakers. The **DC blocker**
  `y[n] = x[n] - x[n-1] + R·y[n-1]` removes it, and is your first IIR filter: feedback makes it
  efficient and makes instability possible.
- **Constant-power panning** (`cos`/`sin`) keeps loudness even across the stereo field; linear
  panning dips 3 dB at the centre.

**Next:** [Chapter 15 — Noise: White, Pink, and Brown](15-noise.md)
