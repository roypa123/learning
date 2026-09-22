# Chapter 12 — Square, Saw, Triangle: The Classic Waveforms

> Four waveforms have dominated electronic music for sixty years. This chapter builds all of
> them the obvious way, and then shows you why the obvious way is wrong — which is how you will
> meet aliasing for the first time, with your ears rather than your calculator.

---

## 12.1 The four shapes

```
   SINE                       TRIANGLE
      .-'''-.                     /\
    .'       '.                  /  \
   -----------'--------      ---/----\------
                '.   .'            \  /
                  '-'               \/

   SAWTOOTH                   SQUARE / PULSE
       /|   /|   /|            ,----,    ,----,
      / |  / |  / |            |    |    |    |
   --/--|-/--|-/--|---      ---'    '----'    '---
    /   |/   |/   |
```

All four are **periodic**: they repeat exactly once per cycle. All four are therefore describable
as a sum of harmonics (Chapter 2's Fourier claim). What distinguishes them is *which* harmonics
are present and how strong each one is — and that determines entirely how they sound.

Here is the summary table. It is worth committing to memory, because it explains most of what
you will do in subtractive synthesis.

| Waveform | Harmonics present | Amplitude of harmonic *h* | Roll-off | Character |
|---|---|---|---|---|
| Sine | 1st only | — | — | Pure, hollow, dull |
| Triangle | Odd only (1, 3, 5, 7…) | 1/h² | −12 dB/octave | Soft, flute-like, slightly hollow |
| Square | Odd only (1, 3, 5, 7…) | 1/h | −6 dB/octave | Hollow, woody, clarinet-like, buzzy |
| Sawtooth | **All** (1, 2, 3, 4…) | 1/h | −6 dB/octave | Bright, brassy, rich, aggressive |

Two questions fall out of this table immediately, and answering them tells you most of what you
need to know about timbre.

**Why do square and triangle sound so different when both contain only odd harmonics?**
Because of the roll-off. The square's harmonics fall as `1/h` and the triangle's as `1/h²`. At
the 9th harmonic, the square still has 1/9 (−19 dB) while the triangle has 1/81 (−38 dB). The
triangle's upper harmonics are essentially gone, so it sounds much closer to a sine — soft and
rounded — while the square keeps enough high content to sound buzzy.

**Why does the sawtooth sound brighter than the square, when both roll off at `1/h`?**
Because the saw has *twice as many* harmonics — all integers, not just odd ones. Between 1 kHz
and 2 kHz a 100 Hz saw has ten harmonics; a 100 Hz square has five. Twice the density in every
band means noticeably more energy up top.

That second point also explains the **hollowness** of odd-only waveforms. The 2nd harmonic is an
octave above the fundamental, the 4th two octaves, the 6th an octave-and-a-fifth. Removing all of
them removes the octave reinforcement that makes a sound feel "solid". Clarinets are
odd-harmonic-dominant (a cylindrical tube closed at one end), which is exactly why a clarinet has
that distinctive woody hollowness while a saxophone (conical bore, all harmonics) sounds full and
brassy. You are hearing the same physics as the square-versus-saw distinction.

---

## 12.2 Generating them the obvious way

Every one of these is a simple function of phase. We already have a phase accumulator from
Chapter 10; we only need to change what we do with the phase.

It is convenient to work with **normalised phase** running 0 … 1 instead of 0 … 2π, because three
of the four formulas become simpler. We will keep both forms in mind.

```cpp
// phase is 0.0 .. 1.0 across one cycle

float naiveSine(double phase)
{
    return static_cast<float>(std::sin(2.0 * kPi * phase));
}

float naiveSaw(double phase)
{
    return static_cast<float>(2.0 * phase - 1.0);        // 0..1  ->  -1..+1
}

float naiveSquare(double phase)
{
    return (phase < 0.5) ? 1.0f : -1.0f;
}

float naivePulse(double phase, double width)             // width 0..1
{
    return (phase < width) ? 1.0f : -1.0f;
}

float naiveTriangle(double phase)
{
    // Ramp up 0->1 over the first half, down 1->0 over the second,
    // then map to -1..+1.
    const double t = (phase < 0.5) ? (phase * 2.0) : (2.0 - phase * 2.0);
    return static_cast<float>(2.0 * t - 1.0);
}
```

**Walkthrough**

`naiveSaw` — as phase goes 0→1, `2·phase − 1` goes −1→+1 linearly, then snaps back when the phase
wraps. That snap is the vertical edge of the sawtooth. Note this produces a *rising* saw; some
synthesisers use a falling saw (`1 − 2·phase`). They sound identical — the ear cannot hear the
difference between a waveform and its time-reverse at this level — but the distinction matters
when you sum two of them, because they can cancel.

`naiveSquare` — the first half of the cycle is +1, the second is −1. Two instantaneous jumps per
cycle.

`naivePulse` — the same, but the switchover point is adjustable. `width = 0.5` is a square;
`width = 0.1` is a narrow spike. The width is called the **duty cycle**, and modulating it is
**pulse-width modulation (PWM)**, one of the most recognisable sounds in electronic music.

`naiveTriangle` — a ramp up then a ramp down. No instantaneous jumps, only changes of slope, and
that difference is the whole reason it is so much gentler than the square.

### Why "naive"?

Every one of these is mathematically correct as a description of the continuous waveform. But we
are *sampling* it, and Chapter 4 warned what happens when you sample something containing
frequencies above Nyquist.

A sawtooth has harmonics at every integer multiple of the fundamental, extending to infinity.
Infinity is a great deal more than 22,050 Hz. So the moment you evaluate `2·phase − 1` at sample
instants, you are sampling a signal with unlimited bandwidth, with no anti-alias filter in front
of it — the exact situation Chapter 4 said must never happen.

Hold that thought. First, let us hear them.

---

## 12.3 The program

**Code — `code/ch12/waveforms.cpp`** (abridged here; full listing in `code/`)

```cpp
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>
#include <functional>

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- shapes
// All take normalised phase in 0..1 and return -1..+1.

float shapeSine(double p)     { return static_cast<float>(std::sin(2.0 * kPi * p)); }
float shapeSaw(double p)      { return static_cast<float>(2.0 * p - 1.0); }
float shapeSquare(double p)   { return (p < 0.5) ? 1.0f : -1.0f; }
float shapeTriangle(double p)
{
    const double t = (p < 0.5) ? (p * 2.0) : (2.0 - p * 2.0);
    return static_cast<float>(2.0 * t - 1.0);
}

// ---------------------------------------------------------------- render

// Generate `seconds` of a waveform using a normalised phase accumulator.
std::vector<float> render(const std::function<float(double)>& shape,
                          double freq,
                          double amplitude,
                          double seconds,
                          double sampleRate)
{
    const size_t n = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> out(n);

    double phase = 0.0;
    const double inc = freq / sampleRate;       // normalised: 1.0 = one full cycle

    for (size_t i = 0; i < n; ++i)
    {
        out[i] = static_cast<float>(amplitude * shape(phase));

        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    return out;
}

// Band-limited versions, built additively: sum harmonics, stop at Nyquist.
// These are CORRECT -- no aliasing is possible because nothing above
// Nyquist is ever generated.

std::vector<float> bandlimitedSaw(double freq, double amp, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; ++h)
    {
        const double f = freq * h;
        if (f >= sr / 2.0)
            break;

        // Sawtooth series: sum (-1)^(h+1) / h * sin(h w t), scaled by 2/pi.
        const double a = (2.0 / kPi) * ((h % 2 == 1) ? 1.0 : -1.0) / h;

        double phase = 0.0;
        const double inc = 2.0 * kPi * f / sr;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
    }
    return out;
}

std::vector<float> bandlimitedSquare(double freq, double amp, double seconds, double sr)
{
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> out(n, 0.0f);

    for (int h = 1; ; h += 2)                   // odd harmonics only
    {
        const double f = freq * h;
        if (f >= sr / 2.0)
            break;

        const double a = (4.0 / kPi) / h;

        double phase = 0.0;
        const double inc = 2.0 * kPi * f / sr;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(amp * a * std::sin(phase));
            phase += inc;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
    }
    return out;
}
```

And the `main` that renders the comparisons:

```cpp
int main()
{
    const double sr = 44100.0;

    // ---- 1. The four shapes at a low frequency ---------------------
    // At 110 Hz most harmonics fit below Nyquist, so aliasing is mild
    // and you hear the characteristic timbres cleanly.
    {
        std::vector<float> out;
        const std::pair<const char*, std::function<float(double)>> shapes[] = {
            { "sine",     shapeSine     },
            { "triangle", shapeTriangle },
            { "square",   shapeSquare   },
            { "saw",      shapeSaw      },
        };

        for (const auto& s : shapes)
        {
            std::cout << "  " << s.first << "\n";
            auto part = render(s.second, 110.0, 0.3, 1.0, sr);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("shapes_110.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 2. The aliasing demonstration ------------------------------
    // A naive saw swept upward. Listen for the "wrong" descending tones.
    {
        const size_t n = static_cast<size_t>(8.0 * sr);
        std::vector<float> out(n);

        double phase = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(n);
            const double f = 100.0 * std::pow(40.0, t);     // 100 Hz -> 4 kHz

            out[i] = static_cast<float>(0.3 * shapeSaw(phase));

            phase += f / sr;
            if (phase >= 1.0) phase -= 1.0;
        }
        WavWriter::write("alias_naive_saw.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 3. The same sweep, band-limited ----------------------------
    {
        const size_t n = static_cast<size_t>(8.0 * sr);
        std::vector<float> out(n, 0.0f);

        // Recompute the harmonic set per sample: expensive but exact.
        std::vector<double> phases(200, 0.0);

        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(n);
            const double f0 = 100.0 * std::pow(40.0, t);

            double sum = 0.0;
            for (int h = 1; h < 200; ++h)
            {
                const double f = f0 * h;
                if (f >= sr / 2.0) break;

                const double a = (2.0 / kPi) * ((h % 2 == 1) ? 1.0 : -1.0) / h;
                sum += a * std::sin(phases[static_cast<size_t>(h)]);

                phases[static_cast<size_t>(h)] += 2.0 * kPi * f / sr;
                if (phases[static_cast<size_t>(h)] >= 2.0 * kPi)
                    phases[static_cast<size_t>(h)] -= 2.0 * kPi;
            }
            out[i] = static_cast<float>(0.3 * sum);
        }
        WavWriter::write("alias_bandlimited_saw.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 4. High-frequency comparison: where it really hurts --------
    {
        std::vector<float> out;
        for (double f : { 1000.0, 2000.0, 4000.0 })
        {
            auto naive = render(shapeSaw, f, 0.3, 0.8, sr);
            auto bl    = bandlimitedSaw(f, 0.3, 0.8, sr);
            out.insert(out.end(), naive.begin(), naive.end());
            out.insert(out.end(), bl.begin(), bl.end());
        }
        WavWriter::write("naive_vs_bandlimited.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 5. Pulse-width modulation ----------------------------------
    {
        const size_t n = static_cast<size_t>(6.0 * sr);
        std::vector<float> out(n);

        double phase = 0.0, lfoPhase = 0.0;
        const double inc    = 110.0 / sr;
        const double lfoInc = 0.5   / sr;         // 0.5 Hz sweep of the width

        for (size_t i = 0; i < n; ++i)
        {
            const double width = 0.5 + 0.45 * std::sin(2.0 * kPi * lfoPhase);
            out[i] = static_cast<float>(0.3 * (phase < width ? 1.0 : -1.0));

            phase += inc;       if (phase    >= 1.0) phase    -= 1.0;
            lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        }
        WavWriter::write("pwm.wav", out, static_cast<int>(sr), 1);
    }

    return 0;
}
```

---

## 12.4 Aliasing, heard

This is the centre of the chapter. Play the two sweep files back to back.

### `alias_naive_saw.wav`

A sawtooth sweeping from 100 Hz to 4 kHz over eight seconds.

What you *expect*: a tone rising smoothly, getting thinner as it goes up.

What you *hear*: the fundamental rises, yes. But underneath and around it you hear additional
tones **moving downwards**, and metallic, gritty shimmer that has no business being there. As the
fundamental climbs, a whole ghost orchestra of wrong notes descends through it. Near the end it
sounds more like a broken radio than a synthesiser.

### `alias_bandlimited_saw.wav`

The same sweep, built by summing only harmonics below Nyquist.

Smooth. A clean tone rising, becoming gradually thinner and purer as fewer harmonics fit under
Nyquist. No descending ghosts. No grit.

**You have just heard the difference between a cheap synthesiser and a good one.**

### Why the ghosts descend

Work through the arithmetic for one moment in the sweep. Suppose the fundamental is at 3,000 Hz.
The saw's harmonics are at:

```
   3,000   6,000   9,000   12,000   15,000   18,000   21,000   24,000   27,000  ...
```

Nyquist is 22,050 Hz. Everything at and above that folds back, by Chapter 4's rule
`f_alias = fs − f`:

| Harmonic | True frequency | Above Nyquist? | Appears at |
|---|---|---|---|
| 7th | 21,000 | no | 21,000 |
| 8th | 24,000 | yes | 44,100 − 24,000 = **20,100** |
| 9th | 27,000 | yes | 44,100 − 27,000 = **17,100** |
| 10th | 30,000 | yes | 44,100 − 30,000 = **14,100** |
| 11th | 33,000 | yes | 44,100 − 33,000 = **11,100** |
| 12th | 36,000 | yes | 44,100 − 36,000 = **8,100** |

Now raise the fundamental slightly, to 3,100 Hz. The 12th harmonic moves to 37,200, and its alias
moves to 44,100 − 37,200 = **6,900 Hz**. The fundamental went **up** by 100 Hz; this alias went
**down** by 1,200 Hz.

That is the descending ghost. Every aliased harmonic moves in the opposite direction to the note,
at a multiple of its speed. And critically:

**The aliases are not harmonically related to the fundamental.** 8,100 Hz is not an integer
multiple of 3,000 Hz. It is not a musical interval. It is a foreign tone sitting inside your note,
and it changes its relationship to the note as you play different pitches. This is why aliasing
sounds *dissonant and unstable* rather than merely bright.

### `naive_vs_bandlimited.wav`

Six segments, alternating: naive saw then band-limited saw, at 1 kHz, 2 kHz and 4 kHz.

- **At 1 kHz** the difference is audible but not dramatic. 22 harmonics fit below Nyquist, and
  the aliased ones are comparatively weak (they are high-numbered, so their `1/h` amplitude is
  small).
- **At 2 kHz** the naive version is clearly grittier and slightly "wrong" in pitch character.
- **At 4 kHz** they barely sound like the same instrument. The band-limited version is a clean,
  thin tone (only 5 harmonics fit). The naive version is a harsh, metallic, clangorous noise.

**This is the practical rule:** aliasing is a **high-note problem**. A naive oscillator is
tolerable in the bass and unusable in the treble. Which is exactly backwards from what you want,
because it means your synth degrades as you play up the keyboard — and it is why amateur
synthesisers are so often described as "sounding cheap in the top octave".

### Experiments

**Experiment 12.1.** Render a naive square at 5,000 Hz and look at it in Audacity's spectrum
analyser (Analyze → Plot Spectrum). Count the peaks. Are they at 5k, 15k, 25k...? Where did the
peaks between them come from?

**Experiment 12.2.** Render naive saws at 100, 200, 400, 800, 1600, 3200 Hz, one second each.
Listen for the exact point at which you first notice the grit. Then compute how many harmonics
fit below Nyquist at that frequency.

**Experiment 12.3.** Change the sample rate to 96,000 in the naive sweep and re-render. Is the
aliasing gone? (It is not — it is just moved higher and become weaker. Oversampling helps and
does not cure. Chapter 29 explains the difference between "helps" and "cures".)

**Experiment 12.4.** Render a naive saw at exactly 11,025 Hz (a quarter of the sample rate). Look
at the samples. You get four samples per cycle, and the "sawtooth" is four values. Does it sound
like a sawtooth at all?

---

## 12.5 How aliasing is actually fixed

We will do this properly in Chapter 31, but you should know the landscape now, because the
options differ enormously in cost and quality.

**1. Additive synthesis (what we just did).** Sum sines, stop at Nyquist. Perfect result, and
crushingly expensive: a 100 Hz saw needs 220 sine oscillators. Unusable in real time for
polyphony, but excellent as a *reference* — when you want to know what an oscillator should
sound like, build it additively and compare.

**2. Wavetables.** Pre-compute, offline, one band-limited waveform per octave (or per few
semitones). At run time, look up the table for the pitch you are playing and interpolate. This is
what most modern soft-synths do. Cheap at run time, needs memory and a careful crossfade between
tables. Chapter 32.

**3. PolyBLEP / BLIT / minBLEP.** Generate the naive waveform, then *correct* the region around
each discontinuity with a small polynomial that approximates a band-limited step. Very cheap —
a handful of operations per discontinuity — and good enough for most purposes. This is the
standard technique in modern virtual-analogue synths. Chapter 31.

**4. Oversampling.** Run the oscillator at 4× or 8× the sample rate, then low-pass filter and
decimate. This pushes the aliases up where the filter can remove them. Expensive and imperfect
for oscillators (there is always *some* content above the new Nyquist), but it is the standard
and correct solution for **nonlinear** processes like distortion, which generate new harmonics
from whatever you feed them. Chapter 29.

**5. Just do not use naive waveforms above the bass.** A legitimate engineering choice in some
contexts, and worth knowing as an option.

> **Why teach the naive version at all, then?** Three reasons. It is how you learn what aliasing
> sounds like, which is a permanent, valuable skill. The naive waveform is the starting point for
> PolyBLEP — the correction is applied *to* it. And in the bass, where most of these waveforms
> actually live in a mix, it is genuinely acceptable.

---

## 12.6 Pulse width and the DC problem

Listen to `pwm.wav`. A 110 Hz pulse whose duty cycle sweeps from 5% to 95% and back, twice.

The timbre changes dramatically — thin and nasal at the extremes, full and hollow at 50%. The
*pitch* never changes. This is a pure timbral modulation, and it is one of the most useful in
synthesis: it is the classic string-ensemble and "fat bass" sound, and it works because it is
changing the harmonic recipe without touching the fundamental.

**The harmonic explanation.** A pulse of duty cycle `d` has harmonic amplitudes proportional to
`sin(π·h·d) / h`. When `d = 0.5`, `sin(π·h·0.5)` is zero for every even `h` — which is precisely
why a square wave has only odd harmonics. Move `d` away from 0.5 and the even harmonics appear,
growing as the pulse narrows. So PWM is literally a control that fades the even harmonics in and
out, which is why it sounds like the waveform is "opening up".

### The DC offset trap

Here is a real bug this exposes. A pulse with duty cycle `d` spends fraction `d` of its time at
+1 and `(1−d)` at −1. Its mean value is:

```
   mean = d·(+1) + (1-d)·(-1) = 2d - 1
```

For `d = 0.5` the mean is 0. For `d = 0.1` the mean is **−0.8**.

So a narrow pulse carries an enormous DC offset. Chapter 11 explained why that is bad: it eats
headroom, it makes meters lie, and sustained it can heat a speaker's voice coil. And if the duty
cycle is being *modulated*, as in PWM, the DC offset moves around — which means there is a
low-frequency thump in your signal that you did not ask for.

Measure it:

```cpp
std::cout << "PWM DC offset: " << db::dcOffset(pwmBuffer) << "\n";
```

Chapter 14 builds the DC-blocking filter that removes it, and it is the first infinite-impulse-
response filter in the book. For now, note that a real analogue synth has this problem too — and
solves it with a capacitor, which is the same filter in hardware.

---

## 12.7 A word on what these waveforms are *for*

It is easy to treat saw/square/triangle as a menu of four flavours. They are better understood as
**four different amounts of raw material**.

Subtractive synthesis (Chapter 33) works by starting with something harmonically rich and
*removing* what you do not want with a filter. From that viewpoint:

- **Sawtooth** is the richest starting point. Every harmonic present. This is why it is the
  default oscillator in almost every synthesiser ever made, and why brass, strings and aggressive
  basses are nearly always saw-based. If in doubt, start with a saw.
- **Square/pulse** has half the harmonics, so a filter sweep over it reveals a sparser, hollower
  sequence. Good for woodwind-ish sounds, reedy leads, and — with PWM — thick ensemble textures.
- **Triangle** is nearly a sine. There is very little to subtract. Use it when you want a soft
  body, a sub-bass with a little edge, or a starting point for FM.
- **Sine** has nothing to subtract at all. In subtractive synthesis it is only useful as a
  sub-oscillator or for reinforcing a fundamental. Its real home is FM (Chapter 35) and additive
  synthesis (Chapter 36), where you *add* rather than remove.

Cinematic context, since it is coming: the enormous low brass "braam" of Chapter 84 is
fundamentally a stack of detuned **sawtooths** through a low-pass filter into distortion. It has
to be saws, because you need the harmonic density to survive both the filtering and the
compression and still sound massive. A stack of triangles would sound polite, and polite is the
opposite of what a braam is for.

---

## 12.8 Exercises

**12.1** Add `shapePulse(double phase, double width)` to the program and render duty cycles of
0.5, 0.25, 0.1 and 0.05 at 110 Hz. Measure the DC offset of each and check it against the formula
`2d − 1`.

**12.2** Build a band-limited **triangle** additively. The series is: odd harmonics only, with
amplitude `1/h²` and alternating sign — `(8/π²) · Σ (−1)^((h−1)/2) · sin(hωt) / h²` for odd `h`.
Compare it to the naive triangle at 4 kHz. Is the difference as dramatic as it was for the saw?
Explain why, using the roll-off rates in §12.1.

**12.3** Measure peak, RMS and crest factor for each of the four naive waveforms at amplitude
1.0, using Chapter 11's tools. Sort them by RMS. Which sounds loudest? Does the ordering match?

**12.4** Render a naive saw at 440 Hz and a band-limited saw at 440 Hz, then subtract one from
the other sample by sample and write the difference to a file. What you have isolated is *the
aliasing itself*. Listen to it. Measure its RMS relative to the signal.

**12.5** *Deliberate breakage.* Remove the `if (f >= sr/2.0) break;` from `bandlimitedSaw` and cap
the harmonic count at 500 instead. Render at 440 Hz. What happens, and why is it the same problem
you were trying to avoid?

**12.6** At what fundamental frequency does a sawtooth at 44,100 Hz have exactly 10 harmonics
below Nyquist? At what frequency does it have exactly 2? What does a 2-harmonic saw sound like —
is it distinguishable from a sine?

**12.7** Write a function that reports, for a given waveform and fundamental, the total RMS energy
that *would* land above Nyquist. Use it to predict, before listening, which of a set of
frequencies will alias audibly.

**12.8** Implement a **hard-synced** sawtooth: a second oscillator whose phase is forcibly reset
to zero every time a first (lower-frequency) oscillator wraps. Sweep the second oscillator's
frequency while keeping the first fixed. This is "oscillator sync", one of the most aggressive
sounds in synthesis. It aliases horribly in naive form — note where and why.

---

### Chapter summary

- Four classic shapes. Their harmonic content determines everything: **triangle** = odd harmonics
  at `1/h²` (soft), **square** = odd at `1/h` (hollow, buzzy), **saw** = all at `1/h` (bright,
  rich), **sine** = fundamental only.
- Odd-only spectra sound "hollow" because the octave-reinforcing even harmonics are missing —
  the same physics that makes a clarinet sound different from a saxophone.
- Generating them directly from phase is easy and **aliases badly**, because these waveforms have
  unlimited bandwidth and we are sampling without an anti-alias filter.
- Aliased harmonics fold down to `fs − f`, are **not harmonically related** to the note, and move
  **downward** as the note moves up. That is the descending metallic shimmer you heard.
- Aliasing is a **high-note problem**: negligible in the bass, ruinous in the top two octaves.
- Fixes, in order of cost: PolyBLEP (cheap, standard), wavetables (cheap at runtime, memory
  hungry), oversampling (essential for nonlinearities), additive (exact but expensive; useful as
  a reference).
- Pulse width controls the even harmonics, giving PWM. Narrow pulses carry a large **DC offset**
  (`2d − 1`), which needs removing — Chapter 14.
- Think of these waveforms as *amounts of raw material* for a filter to subtract from. The
  sawtooth is the default for a reason.

**Next:** [Chapter 13 — Envelopes: Making a Note Begin and End](13-envelopes.md)
