# Chapter 44 — Chorus, Flanger, and Fractional Delay

> Chorus, flanger and vibrato are the same circuit with three different delay times. That is not
> a simplification — it is literally true, and understanding the boundaries between them tells
> you more than studying each separately.

---

## 44.1 One circuit, three effects

```
   in ──┬────────────────────────────┬──► out
        │                            │
        └──► [ modulated delay ] ────┘
                    ▲
                   LFO
```

The delay time is modulated by an LFO. Everything depends on **how long the delay is** and
**whether the dry signal is mixed back in**.

| Effect | Delay range | Modulation | Dry mixed? | Feedback |
|---|---|---|---|---|
| **Vibrato** | 5–10 ms | ±2–5 ms, 4–7 Hz | **No** | No |
| **Flanger** | 0.1–10 ms | ±0.5–5 ms, 0.1–2 Hz | **Yes** | Often, high |
| **Chorus** | 15–40 ms | ±2–8 ms, 0.1–2 Hz | **Yes** | Usually none |
| **Doubler** | 20–50 ms | ±1–3 ms, slow random | **Yes** | No |

**Vibrato has no dry signal**, so you hear only the delayed, pitch-modulated copy — pure pitch
wobble.

**Flanger and chorus both mix dry with wet**, so you hear a comb filter (Chapter 42) whose notches
sweep. The difference is purely the delay length, which sets the comb spacing:

- **Flanger at 1 ms**: notches every 500 Hz. Few, wide teeth, sweeping through the audible range
  — the dramatic jet-plane swoosh.
- **Chorus at 25 ms**: notches every 20 Hz. Hundreds of narrow teeth, too dense to hear
  individually; you hear thickness and movement instead.

**That is the entire distinction.** Take a flanger, turn the delay up to 25 ms, and it becomes a
chorus. This is worth doing once as an experiment, because the transition is continuous and
surprisingly illuminating: somewhere around 10–15 ms the character changes from "sweeping filter"
to "multiple voices".

---

## 44.2 Why the pitch modulates

A modulated delay does not just move sound in time — it **changes its pitch** while moving.

The reason: if the read position moves toward the write position, you are reading samples faster
than they were written, so the pitch goes up. Moving away, pitch goes down.

```
   pitch ratio = 1 - d(delay)/dt
```

For sinusoidal modulation of depth `D` samples at rate `f`:

```
   maximum pitch deviation = 2π · f · D / fs
```

At 44,100 Hz with a 1 Hz LFO and ±5 ms depth (220 samples):

```
   2π × 1 × 220 / 44100 = 0.0313  ->  about 54 cents
```

Over half a semitone of wobble. **This is why chorus sounds like multiple players**: real
ensembles are slightly out of tune with each other, and modulated delay reproduces that
automatically.

**And it is why depth and rate interact.** Doubling the LFO rate doubles the pitch deviation at
the same depth. A "subtle" chorus at 0.3 Hz becomes seasick at 3 Hz with the same depth control.
Well-designed chorus units scale depth inversely with rate to compensate; ones that do not are
frustrating to use.

> **This is also Doppler** (Chapter 77). A source moving toward you is a delay that is shortening,
> and the pitch rises. The physics is identical, and Chapter 77 implements Doppler as exactly this
> — a varying delay line, not a frequency multiplication.

---

## 44.3 The chorus

```cpp
class Chorus
{
public:
    void prepare(double sr)
    {
        sr_ = sr;
        for (auto& d : delays_) d.prepare(sr, 0.1);       // 100 ms is plenty
        for (size_t i = 0; i < lfos_.size(); ++i)
        {
            lfos_[i].setSampleRate(sr);
            lfos_[i].setShape(LFO::Shape::Sine);
            // Spread the LFO phases evenly so the voices never align.
            lfos_[i].setPhaseOffset(static_cast<double>(i) / lfos_.size());
        }
    }

    void setRate(double hz)     { for (auto& l : lfos_) l.setRateHz(hz); }
    void setDepthMs(double ms)  { depthMs_ = ms; }
    void setCentreMs(double ms) { centreMs_ = ms; }
    void setMix(float m)        { mix_ = m; }

    void process(float in, float& outL, float& outR)
    {
        double l = 0.0, r = 0.0;

        for (size_t i = 0; i < delays_.size(); ++i)
        {
            const double mod = lfos_[i].next() * depthMs_;
            const double t   = (centreMs_ + mod) * 0.001 * sr_;

            // Hermite: the delay moves, and linear's brightness wobble
            // would modulate the tone (Chapter 42).
            const float s = delays_[i].readHermite(t);
            delays_[i].write(in);

            // Alternate voices across the stereo field.
            const double pan = (i % 2 == 0) ? 0.25 : 0.75;
            l += s * (1.0 - pan);
            r += s * pan;
        }

        const double norm = 1.0 / std::sqrt(static_cast<double>(delays_.size()));

        outL = in * (1.0f - mix_) + static_cast<float>(l * norm) * mix_;
        outR = in * (1.0f - mix_) + static_cast<float>(r * norm) * mix_;
    }

private:
    std::array<DelayLine, 4> delays_;
    std::array<LFO, 4>       lfos_;
    double sr_ = kDefaultRate, depthMs_ = 4.0, centreMs_ = 22.0;
    float  mix_ = 0.5f;
};
```

**Multiple voices with spread LFO phases** is what separates a lush chorus from a thin one. One
voice gives a single comb filter sweeping. Four voices with phases at 0°, 90°, 180° and 270°
give four combs sweeping independently, which is much closer to an actual ensemble — and
critically, they never all align, so there is no moment where the effect collapses.

**Stereo spread** by alternating pan positions makes the voices occupy different places. A mono
chorus is much less impressive than a stereo one for this reason alone.

**The `1/√N` normalisation** is Chapter 11's uncorrelated summing. Without it, adding voices
makes the wet signal louder and the mix control stops meaning anything.

### The Juno chorus

Worth a specific mention because it is so recognisable. The Roland Juno-60/106 chorus is two BBD
delay lines with LFOs in **anti-phase** (180° apart), panned hard left and right:

```
   Left  = dry + delay1(centre + depth·sin(2πft))
   Right = dry + delay2(centre − depth·sin(2πft))
```

When one side's delay lengthens the other shortens, so the two sides are always pitch-modulated
in *opposite directions*. This produces an extremely wide image — much wider than random phase
spreading — and it is why Juno pads sound enormous. Two delays and one LFO.

---

## 44.4 The flanger

```cpp
class Flanger
{
public:
    float process(float in)
    {
        const double mod = lfo_.next();

        // Modulate the delay EXPONENTIALLY, so the sweep sounds even.
        // Linear modulation spends too long at the top of the range.
        const double t = minMs_ * std::pow(maxMs_ / minMs_, (mod + 1.0) * 0.5)
                       * 0.001 * sr_;

        const float delayed = delay_.readHermite(t);

        // Feedback is what makes a flanger resonant rather than gentle.
        delay_.write(in + delayed * feedback_);

        // Invert the wet signal for "negative" (through-zero-ish) flanging:
        // the notches and peaks swap, giving a thinner, hollower sweep.
        const float wet = invert_ ? -delayed : delayed;

        return in * (1.0f - mix_) + wet * mix_;
    }

private:
    DelayLine delay_;
    LFO lfo_;
    double sr_ = kDefaultRate, minMs_ = 0.2, maxMs_ = 8.0;
    float feedback_ = 0.6f, mix_ = 0.5f;
    bool  invert_ = false;
};
```

**Three things distinguish a flanger from a chorus:**

**1. Exponential sweep.** The comb notch positions are `fs/(2D)`, which is inversely proportional
to delay. A linear delay sweep therefore moves the notches non-uniformly — fast at short delays,
slow at long ones. Exponential modulation of the delay gives a perceptually even sweep. This is
Chapter 11's logarithmic perception applied to time.

**2. Feedback.** Chorus rarely uses it; flanger depends on it. Feedback sharpens the comb peaks
(Chapter 42's feedback comb) into resonances, which is what produces the intense, metallic,
"jet-plane" character. At feedback above ~0.9 it rings enough to be almost pitched.

**3. Polarity inversion.** Inverting the wet path swaps the peaks and notches, putting a notch at
DC instead of a peak. The result is thinner and hollower — the "negative flanging" sound. Many
classic units offer this as a switch.

### Through-zero flanging

The original effect was made by playing two copies of a tape and pressing a finger on one reel's
*flange* to slow it down — hence the name. As one tape passes the other, the delay goes through
**zero** and then **negative**.

Digitally, you cannot have a negative delay. You simulate it by delaying *both* paths:

```cpp
// Both paths delayed; the modulated one sweeps THROUGH the fixed one.
const float fixed = delayA_.read(fixedDelay_);
const float swept = delayB_.readHermite(fixedDelay_ + sweep);
out = fixed + swept;
```

At the crossing point the two signals are identical and **completely cancel** (Chapter 10's
cancellation demo), producing a dramatic momentary silence that ordinary flanging cannot
achieve. This "zero crossing" whoosh is the defining feature of the original tape effect, and it
costs one extra delay line.

---

## 44.5 Practical notes

**Interpolation quality is more audible here than anywhere else.** The delay is moving constantly,
so Chapter 42's brightness-wobble artefact modulates at the LFO rate — which is precisely the
rate at which you are listening for modulation. Linear interpolation on a bright source gives a
chorus with an audible "dullness sweep" that nobody asked for.

**Use Hermite as a minimum.** All-pass interpolation is another option with flat magnitude
response (Chapter 38), though it introduces transient artefacts when the delay moves quickly.

**Feedback stability.** Same as Chapter 43: clamp below 1.0, `tanh` in the loop, NaN guard. A
flanger at high feedback with a short delay is a very high-Q resonator and can build up quickly.

**DC and very short delays.** At delays below about 0.1 ms the comb's first notch is above
20 kHz and you are just adding a slightly-delayed copy — a gentle high-frequency roll-off. Some
flangers clamp the minimum to 0.1–0.2 ms for this reason.

**LFO shape matters.** Sine is smooth and classic. Triangle gives a more linear sweep that some
prefer. **Random/smooth-random** gives a "drift" character that sounds more like an ensemble and
less like an effect — worth trying on chorus.

---

## 44.6 Where these sit in cinematic work

Modulation effects are used sparingly in film sound, and usually for specific reasons:

**Chorus for unreality.** A subtle chorus on a voice or an instrument makes it slightly *wrong*
in a way listeners cannot identify — useful for dreams, memories, and the supernatural.

**Flanger for movement and energy.** A flanger sweep on a whoosh or a riser adds motion.
Chapter 84 uses it.

**Doubler for width without obvious effect.** A 20–40 ms delay with slow random modulation, panned
opposite the dry, widens a source convincingly. This is how a single recorded element becomes a
wide bed.

**Through-zero flanging for transitions.** The cancellation at the crossing point is a dramatic
momentary "hole" in the sound, which works well as a transition marker.

**What to avoid:** obvious, fast chorus on dialogue or on anything meant to feel real. The effect
draws attention to itself, which is usually the opposite of what film sound wants.

---

## 44.7 Exercises

**44.1** Build the single-voice chorus/flanger circuit. Sweep the centre delay from 0.5 ms to
40 ms over 20 seconds on pink noise. Note where the character changes.

**44.2** Verify the pitch-modulation formula: set a 1 Hz LFO with ±5 ms depth and measure the
pitch deviation of a sine passed through the wet path only (vibrato mode).

**44.3** Build the 4-voice chorus with spread LFO phases. Compare with a single voice at the same
depth.

**44.4** *Deliberate breakage.* Give all four chorus voices the same LFO phase. What happens, and
why is it worse than one voice?

**44.5** Build the Juno chorus (two delays, anti-phase LFO, hard-panned). Compare its stereo
width with the 4-voice version.

**44.6** Compare linear and Hermite interpolation on a chorus applied to a bright source. Can you
hear the brightness wobble?

**44.7** Build the flanger with exponential sweep. Then change to linear sweep and compare. Which
sounds more even?

**44.8** Sweep flanger feedback from 0 to 0.95. At what point does it start to sound pitched?

**44.9** Implement polarity inversion and compare positive and negative flanging on drums.

**44.10** Build through-zero flanging with two delay lines. Verify the cancellation at the
crossing point by measuring the output level there.

---

### Chapter summary

- **Chorus, flanger and vibrato are one circuit** — a modulated delay — distinguished by delay
  length and whether the dry signal is mixed back.
- **Vibrato: no dry signal** (pure pitch wobble). **Flanger: 0.1–10 ms** (few wide comb teeth,
  dramatic sweep). **Chorus: 15–40 ms** (hundreds of narrow teeth, heard as thickness).
- A moving delay **changes pitch**: deviation `= 2π·f·D/fs`. This is why chorus sounds like
  multiple players, why depth and rate interact, and — identically — how Doppler works
  (Chapter 77).
- **Multiple chorus voices with spread LFO phases** and alternating pan is what makes it lush.
  Normalise by `1/√N`. The **Juno chorus** — two delays with anti-phase LFOs, hard-panned — is
  exceptionally wide for two lines of code.
- Flanger specifics: **exponential delay sweep** (notch positions go as `1/D`, so linear
  modulation sweeps unevenly), **feedback** to sharpen the resonances, and **polarity inversion**
  for the hollow "negative" sound.
- **Through-zero flanging** needs two delay lines so the swept one can cross the fixed one,
  producing complete cancellation at the crossing — the original tape effect.
- **Interpolation quality is most audible here**, because the artefact modulates at exactly the
  rate you are listening to. Hermite minimum.
- In cinematic work, use these **sparingly and for reasons**: chorus for unreality, flanger for
  motion on whooshes, doubler for width, through-zero for transitions. Avoid obvious modulation
  on anything meant to feel real.

**Next:** [Chapter 45 — Phasers and Allpass Filters](45-phaser-and-allpass.md)
