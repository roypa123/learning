# Chapter 45 — Phasers and Allpass Filters

> A flanger sweeps hundreds of evenly-spaced notches. A phaser sweeps four or six unevenly-spaced
> ones. That difference sounds small and is not — and the component that makes it possible, the
> all-pass filter, turns out to be the single most important building block in reverb design.

---

## 45.1 The all-pass filter

An **all-pass** filter passes every frequency at **exactly** the same amplitude, while changing
each one's **phase**.

Chapter 24 explained how: a pole at radius `r` and a zero at radius `1/r`, at the same angle.
The distance ratios cancel at every frequency, so the magnitude is flat; the angles do not
cancel, so the phase is not.

**First-order all-pass:**

```
   y[n] = -a·x[n] + x[n-1] + a·y[n-1]
```

```cpp
class AllpassFirstOrder
{
public:
    // Set the frequency at which the phase shift passes through 90 degrees.
    void setFrequency(double hz, double sampleRate)
    {
        const double t = std::tan(kPi * hz / sampleRate);
        a_ = (t - 1.0) / (t + 1.0);
    }

    float process(float x)
    {
        const double y = -a_ * x + x1_ + a_ * y1_;
        x1_ = x;
        y1_ = y;
        return static_cast<float>(y);
    }

    void reset() { x1_ = 0.0; y1_ = 0.0; }

private:
    double a_ = 0.0, x1_ = 0.0, y1_ = 0.0;
};
```

**Its phase response** runs from 0° at DC to −180° at Nyquist, passing through −90° at the
frequency you set. Magnitude is 1.0 everywhere — verify this; it is a good test of a filter
implementation.

**On its own it does nothing audible.** Chapter 2 said phase alone is inaudible without a
reference. An all-pass filter applied to a signal in isolation sounds identical to the input.

**But add the original signal back** and the phase shift becomes amplitude:

```
   out = x + allpass(x)
```

Where the all-pass has shifted phase by 180°, the sum cancels → a **notch**. Where the shift is
0° or 360°, the sum reinforces → a **peak**.

That is a phaser.

---

## 45.2 The phaser

```cpp
class Phaser
{
public:
    void prepare(double sr, int stages = 4)
    {
        sr_ = sr;
        stages_.resize(static_cast<size_t>(stages));
        lfo_.setSampleRate(sr);
        lfo_.setRateHz(0.5);
    }

    float process(float in)
    {
        const double mod = lfo_.next();

        // Exponential sweep, as with the flanger (Chapter 44) --
        // frequency perception is logarithmic.
        const double centre = minHz_ * std::pow(maxHz_ / minHz_,
                                                (mod + 1.0) * 0.5);

        if (++counter_ >= 16)
        {
            counter_ = 0;
            for (size_t i = 0; i < stages_.size(); ++i)
            {
                // Spread the stages across a frequency range so the notches
                // are not all in the same place.
                const double f = centre * std::pow(spread_,
                                     static_cast<double>(i) - stages_.size() * 0.5);
                stages_[i].setFrequency(std::clamp(f, 20.0, sr_ * 0.45), sr_);
            }
        }

        float v = in + feedbackSample_ * feedback_;

        for (auto& s : stages_) v = s.process(v);

        feedbackSample_ = v;

        return in * (1.0f - mix_) + v * mix_;
    }

private:
    std::vector<AllpassFirstOrder> stages_;
    LFO    lfo_;
    double sr_ = kDefaultRate, minHz_ = 200.0, maxHz_ = 4000.0, spread_ = 1.6;
    float  feedback_ = 0.5f, feedbackSample_ = 0.0f, mix_ = 0.5f;
    int    counter_ = 0;
};
```

**Number of stages determines the number of notches:**

```
   notches = stages / 2
```

| Stages | Notches | Character |
|---|---|---|
| 2 | 1 | Very subtle |
| **4** | **2** | The classic (MXR Phase 90) |
| 6 | 3 | Richer (Small Stone) |
| 8 | 4 | Thick |
| 12 | 6 | Very dense, approaching flanger territory |

Each all-pass contributes up to 180° of phase shift, so two stages are needed per 360° — per
notch.

---

## 45.3 Phaser versus flanger

The comparison is the point of both chapters.

| | Flanger | Phaser |
|---|---|---|
| Mechanism | Delay + sum | All-pass + sum |
| Notches | **Hundreds**, evenly spaced (harmonic) | **2–6**, unevenly spaced |
| Notch spacing | `fs/(2D)` — constant in Hz | Set by the all-pass frequencies |
| Character | Metallic, jet-plane, dramatic | Smooth, swirling, subtle |
| CPU | One delay line | 4–12 biquad-ish filters |
| Classic use | Guitar, drums, whooshes | Electric piano, guitar, pads |

**The decisive difference is *harmonic* versus *inharmonic* notch spacing.**

A flanger's notches are at `fs/(2D)`, `3fs/(2D)`, `5fs/(2D)`... — odd multiples of a fundamental.
That is a harmonic relationship, and the ear perceives harmonically-related resonances as
**pitched**. Hence a flanger's characteristic metallic, tuned sound.

A phaser's notches are wherever you put the all-pass stages, which is typically *not* in a
harmonic relationship. Inharmonic resonances do not fuse into a pitch (Chapter 3's auditory scene
analysis), so a phaser sounds smooth and unpitched.

**This is why you would choose one over the other**, and it is a genuinely useful thing to be
able to explain. If you want motion without drawing attention, use a phaser. If you want a
dramatic, obviously-processed sweep, use a flanger.

---

## 45.4 All-pass as a diffuser — the reverb connection

Here is the reason this chapter exists in a book about cinematic sound.

Put a **delay inside** the all-pass instead of a single sample:

```
   y[n] = -g·x[n] + x[n-D] + g·y[n-D]
```

```cpp
class AllpassDelay
{
public:
    void prepare(double sr, double maxSeconds) { delay_.prepare(sr, maxSeconds); }
    void setDelaySamples(double d) { delaySamples_ = d; }
    void setGain(float g)          { g_ = g; }

    float process(float x)
    {
        const float delayed = delay_.read(delaySamples_);
        const float v       = x + g_ * delayed;

        delay_.write(v);

        return -g_ * x + delayed;       // still all-pass: flat magnitude
    }

private:
    DelayLine delay_;
    double delaySamples_ = 100.0;
    float  g_ = 0.6f;
};
```

**This is the single most important component in algorithmic reverb.**

What it does: it takes an impulse and turns it into a burst of many impulses, **without changing
the frequency response at all**. It scatters energy in *time* while leaving the *spectrum*
untouched.

```
   in:   |

   out:  |  .  .   .    .     .      .       .        .
         (many echoes, decaying, but the spectrum is unchanged)
```

Compare with a feedback comb (Chapter 42), which produces regularly-spaced echoes *and* a
strongly coloured comb frequency response. The comb adds ringing; the all-pass does not.

**This property is exactly what reverb needs.** A reverb must be dense in time (thousands of
overlapping reflections) but spectrally neutral (a room should not sound like a resonant tube).
Chaining several all-pass delays with different, mutually-prime delay lengths produces
exponentially increasing echo density with a flat response — which is Schroeder's insight from
1962 and the basis of Chapter 46.

```cpp
// A diffuser: several all-pass delays in series with prime-ish lengths.
// Each stage multiplies the echo density.
AllpassDelay diffusion[4];
const double lengths[4] = { 142, 107, 379, 277 };   // samples, mutually prime
const float  gains[4]   = { 0.75f, 0.75f, 0.625f, 0.625f };
```

**Why mutually prime lengths?** Because two all-passes whose delays share a common factor
produce echoes that coincide, reinforcing at those times and leaving gaps elsewhere — which
reintroduces the periodicity you were trying to destroy. Prime-ish lengths keep the echo pattern
maximally irregular, and Chapter 15's rule applies: **nature is irregular**.

---

## 45.5 Nested and Schroeder all-passes

Two elaborations that appear in real reverbs.

**Nested all-pass**: replace the delay inside an all-pass with *another* all-pass. Each level
multiplies the density.

```cpp
float NestedAllpass::process(float x)
{
    const float delayed = inner_.process(delay_.read(delaySamples_));
    const float v = x + g_ * delayed;
    delay_.write(v);
    return -g_ * x + delayed;
}
```

This is what Jon Dattorro's plate reverb uses, and it is how you get very high echo density from
few components.

**Modulated all-pass**: modulate the internal delay slightly with a slow LFO. This breaks up any
remaining periodicity in the echo pattern and prevents the metallic ringing that static all-pass
chains can develop.

The modulation must be **very slight** — a fraction of a millisecond — or it becomes audible as
pitch wobble on sustained material. This is a real tuning problem in reverb design: too little and
the tail rings, too much and pianos sound seasick.

---

## 45.6 Other uses of all-pass filters

**Phase correction / crossover alignment.** Chapter 22 noted that IIR crossovers do not sum flat
because of phase differences. An all-pass in the complementary path can realign them.

**Frequency-dependent delay (dispersion).** A chain of all-passes delays high frequencies
differently from low ones. This models:
- **String stiffness** (Chapter 38) — higher partials travel faster, making them sharp
- **Spring reverb** — the characteristic "boing" is dispersion in a metal spring, where high
  frequencies arrive before low ones
- **Long-distance propagation** in some media

A spring reverb emulation is essentially a chain of many all-pass filters plus some delay and
feedback, and the dispersion *is* the sound.

**Hilbert transform / 90° phase splitting.** Two all-pass chains designed so their outputs are
always 90° apart give you the analytic signal (Chapter 19), which enables frequency shifting,
single-sideband modulation, and accurate envelope following. Chapter 54 uses it.

**Phase rotation for peak reduction.** Broadcast processors use all-pass filters to make
asymmetric waveforms (like speech) more symmetric, reducing peak level without changing loudness —
a free 1–3 dB of headroom.

---

## 45.7 Exercises

**45.1** Build the first-order all-pass. Verify its magnitude response is within 0.001 dB of 0 at
twenty frequencies across the spectrum, while its phase varies from 0 to −180°.

**45.2** Listen to an all-pass filter on its own applied to music. Can you hear any difference?
Now sum it with the dry signal.

**45.3** Build the 4-stage phaser. Measure the frequency response at several LFO positions and
count the notches.

**45.4** Compare a 4-stage and a 12-stage phaser at the same settings. At what point does it start
to sound like a flanger?

**45.5** Build a flanger and a phaser with similar sweep rates. Apply both to the same drum loop.
Describe the difference in terms of harmonic versus inharmonic notch spacing.

**45.6** Build the all-pass **delay** and measure its impulse response. Confirm it produces
multiple echoes. Then measure its frequency response and confirm it is flat.

**45.7** Chain four all-pass delays with the lengths in §45.4. Feed an impulse and count the
echoes in the output. Now use lengths that share a common factor (e.g. 100, 200, 300, 400) and
compare.

**45.8** Add slight modulation to the all-pass delays. Find the modulation depth at which the
ringing disappears, and the depth at which pitch wobble becomes audible on a sustained piano
note.

**45.9** Build a spring reverb emulation: a long chain of all-passes (20+) with a delay and
feedback. Listen for the dispersion "boing".

**45.10** Implement phaser feedback. Sweep it from 0 to 0.9 and note where the resonances become
prominent.

---

### Chapter summary

- An **all-pass filter** passes all frequencies at equal amplitude while changing their phase —
  a pole at radius `r` and a zero at `1/r`, same angle (Chapter 24).
- On its own it is **inaudible**. Summed with the dry signal, its phase shifts become **peaks and
  notches**. That is a **phaser**.
- **Notches = stages / 2.** Four stages (two notches) is the classic.
- **Phaser versus flanger:** a flanger's hundreds of notches are **harmonically spaced**, so the
  ear hears them as pitched and metallic. A phaser's few notches are **inharmonic**, so it sounds
  smooth and unpitched. That is the whole difference, and it tells you which to choose.
- Put a **delay inside** an all-pass and you get the **single most important component in
  algorithmic reverb**: it scatters energy in **time** (many echoes) while leaving the
  **spectrum flat**. A feedback comb cannot do this — it always colours.
- Chain several with **mutually prime delay lengths** to multiply echo density without
  reintroducing periodicity. **Nesting** multiplies it further.
- **Slight modulation** of the internal delays breaks up residual ringing — too little and the
  tail rings, too much and sustained notes wobble.
- All-passes also give: crossover phase alignment, **dispersion** (string stiffness, spring
  reverb's "boing"), 90° phase splitting for the analytic signal, and peak reduction by phase
  rotation.

**Next:** [Chapter 46 — Reverb I: Schroeder, Comb Filters, and Freeverb](46-reverb-schroeder.md)
