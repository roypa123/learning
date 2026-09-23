# Chapter 52 — Distortion, Saturation, and Waveshaping

> Distortion means non-linearity, and Chapter 20 established what that implies: new frequencies
> that were not in the input. This chapter is about generating them deliberately, controlling
> which ones appear, and — critically — keeping them below Nyquist.

---

## 52.1 What distortion actually does

A **waveshaper** applies a function to each sample:

```cpp
out = f(in);
```

If `f` is a straight line through the origin, the system is linear and nothing happens except a
gain change. Any curvature creates harmonics.

**Which harmonics depends on the symmetry of the function:**

| Function symmetry | Harmonics generated | Character |
|---|---|---|
| **Odd** (`f(−x) = −f(x)`) | 3rd, 5th, 7th... | Hollow, aggressive, "transistor" |
| **Even** (`f(−x) = f(x)`) | 2nd, 4th, 6th... | Warm, musical, "tube" |
| **Asymmetric** | Both | Rich, complex |

**Why even harmonics sound "musical":** the 2nd harmonic is an octave above the fundamental, the
4th is two octaves. Those are consonant intervals, so the added content reinforces the note. Odd
harmonics — the 3rd is an octave-plus-a-fifth, the 5th is two octaves-plus-a-third — are more
dissonant, which reads as aggression.

**This is the real technical difference between "tube warmth" and "transistor bite."** Tube
circuits are typically asymmetric (producing 2nd-harmonic-dominant distortion); solid-state
clipping is typically symmetric (3rd-harmonic-dominant). It is not mysticism; it is symmetry.

```cpp
// Symmetric -> odd harmonics only
float symmetric(float x) { return std::tanh(x); }

// Asymmetric -> even harmonics appear
float asymmetric(float x)
{
    return (x > 0.0f) ? std::tanh(x)          // gentler on the positive side
                      : std::tanh(x * 1.5f);   // harder on the negative
}

// A DC offset makes ANY symmetric shaper asymmetric
float offsetShaper(float x, float bias)
{
    return std::tanh(x + bias) - std::tanh(bias);   // subtract to remove DC
}
```

**The `- std::tanh(bias)` is essential.** Adding a bias shifts the output's DC level, and Chapter
14 explained why that is harmful. Subtracting the shaper's output at zero input restores it.
Alternatively, a DC blocker afterwards.

---

## 52.2 The shaper catalogue

```cpp
// --- soft, smooth ----------------------------------------------------

float softTanh(float x, float drive)
{
    return std::tanh(x * drive) / std::tanh(drive);   // normalised
}

float softAtan(float x, float drive)
{
    return static_cast<float>(std::atan(x * drive) / std::atan(drive));
}

// Exactly linear below 1/3, so transparent until driven.
float cubic(float x)
{
    if (x <= -1.0f) return -2.0f / 3.0f;
    if (x >=  1.0f) return  2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

// --- hard ------------------------------------------------------------

float hardClip(float x) { return std::clamp(x, -1.0f, 1.0f); }

// Diode-like: soft in one direction, hard in the other.
float diode(float x)
{
    return (x > 0.0f) ? 1.0f - std::exp(-x) : -1.0f + std::exp(x * 0.5f);
}

// --- aggressive ------------------------------------------------------

// Wave folding: instead of clipping, REFLECT back. Generates a huge
// number of harmonics and a distinctive metallic character.
float foldback(float x, float threshold)
{
    while (std::fabs(x) > threshold)
        x = std::copysign(2.0f * threshold - std::fabs(x), x);
    return x;
}

// Bit crushing: quantise to fewer bits (Chapter 4's quantization, used
// deliberately).
float bitCrush(float x, int bits)
{
    const float levels = static_cast<float>(1 << bits);
    return std::round(x * levels) / levels;
}

// Sample-rate reduction: hold each sample for N samples.
float sampleReduce(float x, int factor)
{
    if (++counter_ >= factor) { counter_ = 0; held_ = x; }
    return held_;
}

// --- rectification ---------------------------------------------------

float halfWave(float x) { return std::max(x, 0.0f); }         // octave up, + DC
float fullWave(float x) { return std::fabs(x); }              // octave up, + DC
```

**Rectification produces an octave up**, because `|sin|` repeats twice as often as `sin`. This is
how octave-up guitar pedals work, and it is also how Chapter 83 gets harmonic reinforcement for
sub-bass on small speakers. Both forms produce a large DC offset that must be removed.

**Wave folding** is worth trying: instead of flattening peaks, it reflects them back. The result
has *more* harmonics than clipping, arranged in a distinctive pattern, and the character changes
dramatically with drive rather than just getting louder. It is a staple of experimental and
cinematic sound design.

---

## 52.3 Oversampling: not optional

Chapter 29 covered this. Restating the essential points in the context where they matter most:

**Distortion is the archetypal aliasing generator.** A hard clipper on a 5 kHz sine produces
aliases at 900, 9,100, 10,900, 19,100 and 20,900 Hz — none harmonically related to the input.

```cpp
class Saturator : public Processor
{
public:
    void prepare(double sr, int) override
    {
        os_.prepare(oversampleFactor_, sr, 64);
        dc_.setSampleRate(sr);
    }

    void process(AudioBuffer& buffer) override
    {
        for (int c = 0; c < buffer.numChannels(); ++c)
            for (float& s : buffer.channel(c))
            {
                // The nonlinearity runs at the HIGH rate.
                s = os_.processSample(s * driveGain_, [this](float x) {
                    return shape(x);
                });

                s *= outputGain_;
                s = dc_.process(s);        // asymmetric shapers produce DC
            }
    }

    int latencySamples() const { return os_.latencySamples(); }

private:
    Oversampler os_;
    DCBlocker   dc_;
    int   oversampleFactor_ = 4;
    float driveGain_ = 1.0f, outputGain_ = 1.0f;
};
```

**How much oversampling, by shaper:**

| Shaper | Minimum factor | Reason |
|---|---|---|
| `tanh` at low drive | 2× | Harmonics decay fast |
| `tanh` at high drive | 4× | More harmonics reach further |
| Cubic | 2× | Only 3rd-order, so bounded |
| Hard clip | **8×** | Harmonics fall only as `1/h` |
| Foldback | **8–16×** | Enormous harmonic generation |
| Bit crush | **16×** or none | Very high order; sometimes the aliasing *is* the sound |

**Bit crushing is the interesting exception.** Its aliasing is part of the lo-fi aesthetic — a
correctly oversampled bit crusher sounds *wrong* to most ears, because it removes the grit people
associate with the effect. This is a legitimate case for deliberately not oversampling, and it is
worth being explicit that it is a choice.

---

## 52.4 Drive, and gain staging around a nonlinearity

**The level going *into* a nonlinearity determines everything.** Chapter 14 flagged this: once
anything nonlinear is in the chain, gain staging stops being cosmetic.

```cpp
out = outputGain * shape(input * driveGain);
```

**Compensated drive** keeps the output level roughly constant as drive increases, so you hear the
*character* change rather than just the level:

```cpp
// Normalise so that full-scale input still gives full-scale output.
outputGain_ = 1.0f / shape(1.0f * driveGain_);
```

Without this, turning up the drive just makes it louder, and Chapter 11's "louder sounds better"
makes honest comparison impossible.

**Pre- and post-EQ around the shaper** is where much of the character of real distortion units
lives:

```
   in → [ pre-EQ ] → [ drive ] → [ shaper ] → [ post-EQ ] → [ output gain ] → out
```

**Pre-EQ decides which frequencies get distorted.** A high-pass before the shaper means the bass
stays clean while the mids and highs distort — which is how you get a distorted guitar that still
has a defined low end. Almost every guitar amplifier does this.

**Post-EQ tames what the shaper produced.** Distortion generates a lot of high-frequency content;
a low-pass afterwards (the "cabinet" in a guitar amp) removes the harshest of it.

**This pre/post pair is the single most effective way to make distortion musical**, and it is
more important than which shaping function you choose.

---

## 52.5 Analogue emulation

Three families, and what actually characterises each.

**Tube (valve):**
- **Asymmetric** transfer curve → 2nd harmonic dominant
- **Soft knee** — gradual onset of distortion
- **Bias shift** under sustained drive: the operating point moves, so the distortion character
  changes over time
- Often followed by an output transformer, which adds its own low-frequency saturation

**Tape:**
- **Soft symmetric** compression → 3rd harmonic
- **Frequency-dependent** — high frequencies saturate sooner (tape cannot record them at the same
  level)
- **Hysteresis** — the transfer curve depends on the recent history, not just the current sample
- **Wow and flutter** (Chapter 43)
- **Head bump** — a low-frequency resonance
- **Self-erasure** at high frequencies and high levels

**Transformer:**
- **Hysteresis** with a loop
- **Frequency-dependent saturation** — low frequencies saturate first, because the core saturates
  on flux which integrates with frequency
- Adds low-frequency harmonics specifically, which is why transformers "thicken" bass

**The common element is hysteresis** — a memory effect where the output depends on the path taken
rather than just the current input. A memoryless waveshaper cannot reproduce it:

```cpp
// A crude hysteresis model: the transfer curve shifts based on history.
float hysteresisShape(float x)
{
    const float shifted = x - state_ * hysteresisAmount_;
    const float y = std::tanh(shifted);

    state_ = state_ * 0.999f + y * 0.001f;     // slow memory

    return y;
}
```

Proper hysteresis modelling (the Jiles-Atherton model for magnetic materials) is considerably
more involved, and it is what distinguishes serious tape emulations from `tanh` with an EQ curve.

---

## 52.6 Distortion in cinematic sound

Distortion is used heavily in cinematic work, and for reasons quite different from music
production.

**1. Making things bigger without more level.** Chapter 11: +10 dB for twice as loud, and you do
not have +10 dB. Saturation adds harmonics in the 2–5 kHz region where hearing is most sensitive
(Chapter 3), so the sound gets *perceptually* louder without a peak increase.

**2. Implying low end that is not there.** Chapter 3's missing fundamental: distorting a sub-bass
generates harmonics, and the listener's brain infers the fundamental even on a phone speaker.
Chapter 83 develops this.

**3. Density.** Harmonics fill in spectral gaps, making a sound harder to hear through and
therefore more dominant in a mix.

**4. Texture and character.** Heavy distortion on an organic source (a human voice, an animal
sound) produces something that is recognisably *not* synthetic but also not natural — the
standard route to creature and machine sounds.

**5. Cohesion.** Running several layers through the same saturator creates intermodulation between
them, which glues them into one object. Chapter 82 uses this: separate layers sound like separate
sounds; the same layers through one saturator sound like one thing.

**That last point is worth emphasising** because it is not obvious. Intermodulation is usually
considered a defect. Here it is the mechanism: the sum and difference products between layers are
shared content that belongs to none of them individually, and shared content is what Chapter 3's
auditory scene analysis uses to fuse things into a single object.

---

## 52.7 Exercises

**52.1** Apply `tanh`, cubic, hard clip and foldback to a 220 Hz sine at drive 1, 2, 4 and 8. FFT
each and list the harmonics.

**52.2** Verify the symmetry rule: build a symmetric and an asymmetric shaper with similar gain,
and confirm one produces only odd harmonics and the other produces even ones too.

**52.3** Implement the biased shaper with and without the `− tanh(bias)` correction. Measure the
DC offset of each.

**52.4** Measure aliasing (Chapter 29's `aliasingEnergyDb`) for each shaper at 1×, 2×, 4× and 8×
oversampling on a 5 kHz input. Build the table.

**52.5** Implement compensated drive. Sweep drive from 1 to 20 and confirm the output level stays
roughly constant.

**52.6** Build a guitar-amp chain: high-pass at 100 Hz → drive → `tanh` → low-pass at 5 kHz.
Compare with the same shaper with no pre/post EQ.

**52.7** Apply full-wave rectification to a 100 Hz sine. Verify the output's fundamental is
200 Hz. Measure the DC offset, then remove it.

**52.8** Build a foldback distortion and sweep the threshold from 1.0 down to 0.1 on a sine. Note
how the character changes rather than just the level.

**52.9** *The cohesion experiment.* Render three separate layers (a low sine, a mid saw, a noise
burst) mixed cleanly. Then run the same mix through a saturator. Which sounds more like a single
object?

**52.10** Implement the crude hysteresis model. Compare its transfer curve when the input is
rising versus falling.

---

### Chapter summary

- Distortion is a **waveshaper**: `out = f(in)`. Any curvature creates harmonics.
- **Symmetry determines which harmonics**: odd symmetry → 3rd, 5th (hollow, aggressive); even →
  2nd, 4th (warm, musical, consonant intervals). **This is the real difference between "tube" and
  "transistor".**
- Biasing makes any symmetric shaper asymmetric — **subtract `f(bias)`** or add a DC blocker.
- Shapers: `tanh` and `atan` (smooth), cubic (linear below 1/3), hard clip, diode (asymmetric),
  **foldback** (reflects rather than clips — many harmonics, character changes with drive),
  bit crush, sample-rate reduction, and **rectification** (which produces an octave up plus DC).
- **Oversample**: 2× for gentle `tanh`, **8× for hard clipping**, 8–16× for foldback. Bit
  crushing is the exception where aliasing *is* the aesthetic — but make that a deliberate
  choice.
- **Compensated drive** keeps the output level constant so you hear character rather than
  loudness.
- **Pre- and post-EQ around the shaper matters more than the shaping function.** Pre-EQ chooses
  what gets distorted; post-EQ tames what came out.
- Analogue emulation's common element is **hysteresis** — the output depends on history, which a
  memoryless shaper cannot reproduce.
- In cinematic work distortion provides: perceived loudness without peak level, **implied low
  end** via the missing fundamental, spectral density, texture — and **cohesion**, because
  intermodulation between layers creates shared content that fuses them into one object.

**Next:** [Chapter 53 — Equalisation: Shelves, Bells, and Linear Phase](53-equalisation.md)
