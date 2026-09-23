# Chapter 39 — Modal Synthesis and Resonators

> Chapter 38 modelled one-dimensional objects by simulating wave travel. For bells, plates,
> cymbals and everything else you can strike, that approach becomes impractical. Modal synthesis
> takes a different route: model the object as a **bank of resonators**, one per vibration mode.
> It is the most direct route to "this sounds like a specific material".

---

## 39.1 Modes

Any physical object has **natural modes of vibration** — patterns in which it prefers to move,
each at its own frequency, each decaying at its own rate.

Chapter 2 described a string's modes: the whole string, halves, thirds. For a string the modes
land at integer multiples, which is why strings are pitched.

For a **two-dimensional plate** or a **three-dimensional bell**, the geometry is far more
complicated and the modes are **not** integer multiples. That is why struck metal sounds
metallic rather than pitched — Chapter 36 made the same point about bells, and here we build it
from the object's side.

Each mode is described by three numbers:

| Parameter | Meaning |
|---|---|
| **Frequency** | Where the mode sits |
| **Amplitude** | How strongly this mode is excited (depends on *where* you strike) |
| **Decay time** | How long it rings (higher modes usually decay faster) |

**Modal synthesis:** implement each mode as a resonant filter, excite them all with an impulse,
and sum.

---

## 39.2 The resonator

Chapter 24 built one directly in the z-plane: a conjugate pole pair at radius `r` and angle `θ`.

```cpp
class ModalResonator
{
public:
    // frequency: the mode's frequency in Hz
    // decayTime: seconds to fall 60 dB
    void set(double frequency, double decayTime, double sampleRate, double amplitude)
    {
        const double theta = kTwoPi * frequency / sampleRate;

        // Chapter 24: n60 = -6.907755 / ln(r), inverted.
        const double r = std::exp(-6.907755 / (decayTime * sampleRate));

        a1_ = -2.0 * r * std::cos(theta);
        a2_ = r * r;

        // Scale so the peak gain is roughly `amplitude` regardless of r.
        b0_ = amplitude * (1.0 - r) * std::sin(theta);

        amplitude_ = amplitude;
    }

    float process(float x)
    {
        const double y = b0_ * static_cast<double>(x) - a1_ * y1_ - a2_ * y2_;
        y2_ = y1_;
        y1_ = y;

        if (!std::isfinite(y)) { reset(); return 0.0f; }
        return static_cast<float>(y);
    }

    void reset() { y1_ = 0.0; y2_ = 0.0; }

private:
    double b0_ = 0.0, a1_ = 0.0, a2_ = 0.0;
    double y1_ = 0.0, y2_ = 0.0;
    double amplitude_ = 1.0;
};
```

**Eight operations per mode per sample.** A 32-mode bell costs 256 operations — comparable with
an FM patch and far cheaper than 32-partial additive synthesis, because the resonator *is* the
partial including its decay, with no per-partial envelope to evaluate.

**The `(1-r)·sin(θ)` normalisation** matters. Without it, a long-decaying mode (`r` near 1) has
enormous gain and a short one is nearly silent, so the amplitude parameter would not mean
anything. With it, `amplitude` is a usable control.

---

## 39.3 The bank

```cpp
struct Mode
{
    double frequencyRatio = 1.0;      // relative to the fundamental
    double amplitude      = 1.0;
    double decayTime      = 1.0;      // seconds
};

class ModalBank
{
public:
    void setModes(const std::vector<Mode>& modes) { modes_ = modes; }

    void prepare(double fundamental, double sampleRate, double decayScale = 1.0)
    {
        resonators_.resize(modes_.size());
        for (size_t i = 0; i < modes_.size(); ++i)
        {
            const double f = fundamental * modes_[i].frequencyRatio;
            if (f >= sampleRate * 0.49) { resonators_[i].set(0,0,sampleRate,0); continue; }

            resonators_[i].set(f,
                               modes_[i].decayTime * decayScale,
                               sampleRate,
                               modes_[i].amplitude);
        }
    }

    void strike() { for (auto& r : resonators_) r.reset(); pending_ = 1.0f; }

    float nextSample(float excitation = 0.0f)
    {
        const float in = excitation + pending_;
        pending_ = 0.0f;

        double sum = 0.0;
        for (auto& r : resonators_) sum += r.process(in);

        return static_cast<float>(sum);
    }

private:
    std::vector<Mode>           modes_;
    std::vector<ModalResonator> resonators_;
    float pending_ = 0.0f;
};
```

**Skipping modes above Nyquist** is essential. A bell's 40th mode at a high pitch can exceed
Nyquist, and a resonator tuned above Nyquist produces coefficients that alias the resonance to a
wrong frequency — an audible, wrong-sounding partial.

---

## 39.4 Modal data for real objects

The interesting part is the numbers. Here are usable mode sets.

**Tubular bell / chime** (the nominal is the perceived pitch, not the lowest mode):
```
   ratio:  0.50  1.00  1.19  1.56  2.00  2.66  3.01  4.07  5.43  6.79
   amp:    0.40  1.00  0.65  0.45  0.90  0.30  0.25  0.20  0.15  0.10
   decay:  8.0   6.0   5.0   4.0   4.5   2.5   2.0   1.5   1.0   0.8
```

**Note the 1.19 ratio** — Chapter 36's minor third, the interval that makes bells melancholy.

**Marimba bar** (tuned so mode 2 is exactly 4× mode 1 — bar makers undercut the bar to achieve
this):
```
   ratio:  1.00  4.00  10.00  19.00
   amp:    1.00  0.25   0.10   0.04
   decay:  1.2   0.6    0.3    0.15
```

**Glass** (a wine glass or a window pane — very sparse, very long decay):
```
   ratio:  1.00  2.32  4.25  6.63  9.38
   amp:    1.00  0.55  0.30  0.15  0.08
   decay:  4.0   3.0   2.0   1.2   0.8
```

**Wood block** (short decay, mid-range, inharmonic):
```
   ratio:  1.00  1.72  2.61  3.65  4.81
   amp:    1.00  0.70  0.50  0.30  0.20
   decay:  0.12  0.09  0.07  0.05  0.04
```

**Metal plate / sheet** (dense and chaotic — many modes, short-to-medium decay):
```
   Generate 60+ modes at pseudo-random ratios between 1 and 25,
   with amplitudes falling as 1/ratio and decays falling as 2.0/sqrt(ratio).
```

**Drum membrane** (the Bessel-function zeros of a circular membrane — genuinely the physics):
```
   ratio:  1.00  1.59  2.14  2.30  2.65  2.92  3.16  3.50  3.60  3.65
```

**These ratio sets are the whole design.** Change the ratios and the material changes. And notice
what it takes to make something sound **wooden** rather than **metallic**: wood has few modes
with short decays; metal has many modes with long decays. That is the entire distinction, and you
can hear it as soon as you change the numbers.

---

## 39.5 Striking: where and how

Two controls that make a modal instrument playable.

**Strike position** determines which modes are excited. A mode with a node at the strike point
receives no energy.

```cpp
// Excitation amplitude for mode k when struck at position p (0..1).
double strikeAmplitude(int k, double p)
{
    return std::sin(kPi * k * p);    // 1D approximation
}
```

Strike a bar at its centre and the even modes (which have a node there) vanish, leaving a purer,
more pitched tone. Strike near the end and everything rings, giving a clangy, complex sound.

**This is exactly Chapter 38's pluck position**, arrived at from the modal side — the same
physics in a different representation.

**Strike hardness** determines the excitation's spectrum. A hard mallet is a short, bright impulse
that excites high modes; a soft mallet is a longer, duller one that excites mainly low modes.

```cpp
// A simple mallet model: a short filtered noise burst.
std::vector<float> mallet(double hardness, double sampleRate)
{
    const size_t n = static_cast<size_t>((0.02 - 0.018 * hardness) * sampleRate);
    std::vector<float> out(n);

    FastRandom rng(12);
    OnePoleLP  lp;
    lp.setCutoff(200.0 + hardness * 15000.0, sampleRate);

    for (size_t i = 0; i < n; ++i)
    {
        const double env = 1.0 - static_cast<double>(i) / static_cast<double>(n);
        out[i] = lp.process(rng.nextFloat()) * static_cast<float>(env * env);
    }
    return out;
}
```

**Hardness is the single most expressive control in a modal instrument**, and it maps onto a
real physical thing. Sweeping it from soft to hard takes a marimba from mellow to sharp, exactly
as changing mallets does.

---

## 39.6 Resonators as effects

A modal bank does not have to be an instrument. Feed it *any* signal and it imposes the resonant
character of an object onto that signal.

| Input | Through modal bank | Result |
|---|---|---|
| Drum loop | Metal plate modes | The loop sounds like it is being played on metal |
| Speech | Tube modes | Robotic, resonant, "talking through a pipe" |
| Noise | Bell modes | The bell appears to be excited by wind |
| Footsteps | Wooden modes | Footsteps on a wooden floor |
| An impact | Long metallic modes | A cinematic "hit" with a ringing tail |

This is **resonator/exciter** synthesis, and it is one of the highest-value techniques in
cinematic sound design because it decouples *what happened* (the exciter) from *what it happened
to* (the resonator). Chapter 85 builds impacts exactly this way: a transient layer through a
resonant body.

**Keeping it stable.** Feeding continuous audio into a bank of high-Q resonators can build up
enormous gain at the mode frequencies. Two protections:

- **Scale the input** by `1/√numModes`.
- **Put a limiter** (Chapter 51) after the bank.

Neither is optional if the modes have long decays.

---

## 39.7 Getting modal data from real recordings

You can measure an object's modes rather than inventing them:

1. **Record** the object being struck, in a dead space.
2. **FFT** a window just after the attack (Chapter 25) with a **Blackman-Harris** window — you
   need low sidelobes to see quiet modes (Chapter 26).
3. **Peak-pick** and refine with parabolic interpolation (Chapter 36) to get accurate
   frequencies.
4. **Measure each mode's decay** by band-passing around it and fitting an exponential to the
   envelope — the slope in dB per second gives the decay time directly.
5. **Normalise** the frequencies by the lowest strong mode to get ratios.

The result is a mode table for that specific object, at any pitch, with any excitation. **A
30-second recording becomes a playable instrument in a few kilobytes.**

This is how commercial modal instruments (Applied Acoustics, AAS Chromaphone, Ableton's
Collision) are built, and it is a genuinely practical thing to do in an afternoon.

---

## 39.8 Modal versus the alternatives

| | Modal | Waveguide (Ch 38) | Additive (Ch 36) | Sampling |
|---|---|---|---|---|
| Struck objects | **Excellent** | Poor | Good | Good |
| Strings, tubes | Adequate | **Excellent** | Good | Good |
| Cost per voice | ~8 ops × modes | ~25 ops | ~30 ops × partials | 1 read |
| Pitch range | **Any** | Any | Any | Limited by stretch |
| Excitation control | **Excellent** | **Excellent** | Poor | None |
| Realism ceiling | High | High | High | **Perfect** (for what was recorded) |
| Memory | Tiny | Tiny | Small | **Large** |

**Modal wins for anything struck**, because the modes *are* the sound of a struck object. It also
wins on memory by an enormous margin — a mode table is a few hundred bytes against megabytes of
samples, which matters in games.

Its weakness is sustained excitation: a bowed or blown modal model is possible but a waveguide
does it more naturally, because the wave's round trip is physically meaningful for a 1D medium.

---

## 39.9 Exercises

**39.1** Build the modal resonator and verify that a single mode at 440 Hz with a 2-second decay
actually decays 60 dB in 2 seconds (Chapter 20's RT60 function).

**39.2** Build the modal bank and render each object from §39.4. Which sound most convincing?

**39.3** *Deliberate breakage.* Remove the Nyquist check and play a bell at a high pitch. Find the
aliased mode in a spectrogram.

**39.4** Implement strike position. Render a marimba bar struck at 0.5, 0.25 and 0.05. Which
modes vanish at the centre strike, and why?

**39.5** Implement the mallet model and sweep hardness from 0 to 1 on the glass modes. Describe
the change.

**39.6** Take the wood block ratios and multiply every decay time by 20. What material does it
sound like now? Then halve all the ratios' spacing — closer to harmonic. What changed?

**39.7** Feed a drum loop through the metal-plate bank. Scale the input by `1/√numModes` and add a
limiter. Compare with and without those protections.

**39.8** Analyse a real recording of something being struck (a mug, a radiator, a door). Extract
8 modes and their decay times. Rebuild it as a modal instrument and compare.

**39.9** Build a cinematic impact: a short noise burst through a 60-mode metallic plate bank with
4-second decays, then saturation and a cathedral reverb. This is a Chapter 85 layer.

---

### Chapter summary

- Every object has **natural modes** — frequency, amplitude and decay. Integer ratios give pitched
  sounds; non-integer ratios give metallic ones.
- Each mode is a **conjugate pole pair** (Chapter 24) — a two-pole resonator, eight operations per
  sample. Normalise by `(1-r)·sin(θ)` or the amplitude control means nothing.
- **Skip modes above Nyquist**, or they alias to wrong frequencies.
- The **ratio table is the design**. Wood = few modes, short decay. Metal = many modes, long
  decay. Bells have the characteristic **1.19 minor third**. Drum membranes use Bessel zeros.
- **Strike position** determines which modes are excited (`sin(π·k·p)`) — the same physics as
  Chapter 38's pluck position. **Strike hardness** shapes the excitation spectrum and is the most
  expressive control.
- A modal bank is also an **effect**: feed it any signal and it imposes an object's resonance.
  This **exciter/resonator** split — what happened, versus what it happened to — is a core
  cinematic technique.
- Scale the input by `1/√numModes` and limit afterwards, or high-Q banks build up dangerous gain.
- **Modal data can be measured** from a recording: FFT with Blackman-Harris, peak-pick, refine
  parabolically, fit decay envelopes. A 30-second recording becomes a few-kilobyte instrument.
- Modal wins for struck objects and for memory; waveguides win for bowed and blown.

**Next:** [Chapter 40 — Procedural Sound Design: Wind, Rain, Fire, Engines](40-procedural-sound-design.md)
