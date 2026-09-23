# Chapter 35 — FM and Phase-Modulation Synthesis

> Two sine waves, one modulating the other's phase. That is the whole mechanism, and it produces
> bells, electric pianos, basses, brass and metallic chaos that subtractive synthesis cannot
> reach at any price. It also sold more synthesisers than any other method in history.

---

## 35.1 The idea

Chapter 34 showed that modulating a carrier above ~20 Hz creates sidebands. FM synthesis takes
that to its conclusion: **modulate a sine's phase with another sine at audio rate.**

```
   MODULATOR ──► (phase offset) ──► CARRIER ──► out
   (sine, fm)                       (sine, fc)
```

```cpp
float fmSample()
{
    const double mod = std::sin(modPhase) * index;     // the modulator
    const double out = std::sin(carPhase + mod);       // carrier, phase-shifted

    carPhase += kTwoPi * carrierFreq / sr;
    modPhase += kTwoPi * modFreq / sr;

    return static_cast<float>(out);
}
```

**Four lines.** From them you can make a Rhodes piano, a tubular bell, a slap bass and a gong.

### Why it is called FM when it is PM

Chapter 30 explained: true frequency modulation integrates the modulator, so any DC in it makes
the pitch drift, and the effective modulation index changes with the modulator's frequency. Phase
modulation has neither problem and produces the same spectrum.

Yamaha's DX7 — the instrument that made this famous — performs phase modulation and calls it FM.
Everyone has followed. **In this book, "FM" means PM**, and the implementation adds an offset to
the read phase rather than to the increment.

---

## 35.2 The spectrum

This is where FM becomes controllable rather than magical.

FM produces sidebands at:

```
   f = fc ± k·fm         for k = 0, 1, 2, 3, ...
```

So a 200 Hz carrier modulated at 100 Hz produces energy at 200, 100 and 300, 0 and 400, −100 and
500... spreading outward in steps of `fm`.

The **amplitude** of each sideband is given by a **Bessel function of the first kind**, `Jk(I)`,
where `I` is the modulation index. You do not need to compute Bessel functions to use FM, but two
consequences of them matter enormously:

**1. The modulation index controls brightness.**

| Index `I` | Significant sidebands | Sound |
|---|---|---|
| 0 | none | A pure sine |
| 1 | ±1, ±2 | Slightly bright |
| 3 | ±1..±4 | Clearly harmonic-rich |
| 5 | ±1..±7 | Bright, brassy |
| 10 | ±1..±12 | Very bright, edging toward noise |
| 20+ | ±1..±22 | Metallic, chaotic |

A useful rule of thumb: **significant sidebands extend to about `I + 1`**. So `I = 10` spreads
energy across roughly 22 sidebands.

> **This is why an envelope on the index is essential.** A bright attack settling to a mellow
> sustain is what every real instrument does, and in FM it costs one envelope. FM with a static
> index sounds sterile; FM with an index envelope sounds like an instrument.

**2. Negative frequencies fold back with inverted phase.** When `fc − k·fm` goes below zero, it
reflects to `|fc − k·fm|` with a sign flip. This is not aliasing — it is legitimate and it is part
of the sound. It also means FM spectra get complicated quickly, which is both its power and its
reputation for being hard to program.

### The ratio controls harmonicity

This is the single most important parameter.

```
   ratio = fm / fc
```

| Ratio | Result | Character |
|---|---|---|
| **1:1** | Sidebands at `fc, 2fc, 3fc...` | Sawtooth-like, harmonic |
| **2:1** | Odd harmonics only | Square/clarinet-like, hollow |
| **3:1** | Sparse harmonics | Nasal, reedy |
| **1:2** | Sidebands at half-integer multiples | Still harmonic, an octave lower fundamental |
| **1.414:1** (√2) | **Inharmonic** | **Bell, gong, metal** |
| **3.5:1** | Inharmonic | Metallic, clangorous |
| 1.01:1 | Nearly harmonic, slowly beating | Detuned, chorusing |

> **Integer ratios give harmonic (pitched) sounds. Non-integer ratios give inharmonic (metallic)
> sounds.** That one sentence is most of what you need to program FM.

And it explains why FM owns certain timbres: bells, chimes, gongs and metallic percussion are all
inharmonic, and subtractive synthesis — which starts from harmonic oscillators and only removes
things — cannot produce inharmonic partials at all. FM makes them trivially.

**For cinematic sound this matters a great deal.** Chapter 3 noted that the auditory system
cannot resolve inharmonic content into a comfortable single pitch, which is why it reads as
unsettling. Non-integer FM ratios are therefore a direct route to dread, and Chapters 84–85 use
them.

---

## 35.3 Operators and algorithms

The DX7 generalised this to **six operators** — six sine oscillators, each with its own envelope
— connected in configurable topologies called **algorithms**.

```
   Algorithm A (simple FM):        Algorithm B (stacked):

        [6]                             [6]
         │                               │
        [5]                             [5]
         │                               │
        [4]                             [4]
         │                               ├──[3]
        [3]                              │   │
         │                              [2]  │
        [2]                              │   │
         │                              [1]──┘
        [1]                              │
         │                               ▼
         ▼                              out
        out
```

An operator that feeds another is a **modulator**. One that goes to the output is a **carrier**.
The same operator can be both.

```cpp
struct Operator
{
    Osc  osc;
    ADSR env;
    double ratio      = 1.0;      // frequency = noteFreq * ratio
    double fixedFreq  = 0.0;      // or a fixed Hz, ignoring the note
    double level      = 1.0;      // output level / modulation index
    double feedback   = 0.0;      // self-modulation, 0..1
    double lastOutput = 0.0;
};

class FMVoice
{
public:
    float nextSample()
    {
        std::array<double, 6> out{};

        // Process from the highest operator down, so modulators are ready.
        for (int i = 5; i >= 0; --i)
        {
            auto& op = ops_[static_cast<size_t>(i)];

            double mod = 0.0;

            // Sum the modulation arriving from every operator that feeds this one.
            for (int j = 0; j < 6; ++j)
                if (matrix_[static_cast<size_t>(j)][static_cast<size_t>(i)] > 0.0f)
                    mod += out[static_cast<size_t>(j)]
                         * matrix_[static_cast<size_t>(j)][static_cast<size_t>(i)];

            // Self-feedback: average the last two outputs to tame the chaos.
            if (op.feedback > 0.0)
                mod += op.lastOutput * op.feedback;

            // PM: the offset is in CYCLES, so divide by 2*pi.
            const double sample = op.osc.nextSample(mod / kTwoPi)
                                * op.env.nextSample() * op.level;

            op.lastOutput = (op.lastOutput + sample) * 0.5;   // the averaging
            out[static_cast<size_t>(i)] = sample;
        }

        // Sum the carriers.
        double total = 0.0;
        for (int i = 0; i < 6; ++i)
            total += out[static_cast<size_t>(i)] * carrierGain_[static_cast<size_t>(i)];

        return static_cast<float>(total);
    }

private:
    std::array<Operator, 6>                     ops_;
    std::array<std::array<float, 6>, 6>         matrix_{};     // who modulates whom
    std::array<float, 6>                        carrierGain_{};
};
```

**The modulation matrix here is a 6×6 grid** — which is the same idea as Chapter 34's mod matrix,
applied at audio rate. Using a matrix rather than fixed algorithms is how modern FM synths
(FM8, Dexed, Operator) generalise the DX7's 32 presets into arbitrary topologies.

**Feedback needs the averaging.** `op.lastOutput = (op.lastOutput + sample) * 0.5` is a one-pole
low-pass on the feedback path. Without it, an operator modulating itself goes chaotic almost
immediately and produces noise rather than a controllable bright tone. The DX7 did exactly this,
and the averaging is why its feedback operator sounds like a sawtooth rather than a crash.

---

## 35.4 Classic FM patches

These are the sounds that sold the DX7, and each is a handful of numbers.

**Electric piano (the Rhodes/DX7 sound):**
```
   Op2 → Op1        ratio 1:1 and 14:1 (two carriers)
   Op1 (carrier):   ratio 1.0,  env A 0.001 D 2.0 S 0.0 R 0.5
   Op2 (modulator): ratio 1.0,  index 3.0, env A 0.001 D 0.4 S 0.0 R 0.2
   Plus a second pair at ratio 14:1 with a very fast decay -> the "tine" attack
```
The fast-decaying high-ratio pair is the metallic tine strike; the slow 1:1 pair is the body.
**Two operators with different decay times is the whole trick.**

**Bell:**
```
   Op2 → Op1
   ratio 1 : 1.414   (irrational -> inharmonic)
   index 5, decaying slowly
   amp env: A 0.001, D 4.0, S 0.0, R 4.0
```

**Bass:**
```
   Op2 → Op1
   ratio 1:1, index 2 with a FAST index decay (0.05 s)
   Bright click on the attack, pure sine body. Extremely punchy.
```

**Brass:**
```
   Op2 → Op1
   ratio 1:1, index rising from 0 to 4 over 80 ms then settling to 2.5
   The rising index is what makes brass "blare" as it speaks.
```

**Metallic cinematic hit:**
```
   Op3 → Op2 → Op1
   ratios 1 : 3.7 : 11.3     (all irrational relative to each other)
   index 8 and 5, both decaying over 2-6 s
   Add distortion and reverb -> Chapter 85
```

---

## 35.5 FM and aliasing

FM generates sidebands extending to roughly `fc + (I+1)·fm`. With `fc = 1000`, `fm = 1000` and
`I = 10`, that is 12 kHz — fine. With `I = 30` it is 32 kHz, which is above Nyquist and **will
alias**.

FM is therefore a nonlinear process by Chapter 29's definition, and it needs the same protections:

- **Limit the index at high pitches.** A common technique: scale the maximum index down as the
  note rises, so the highest sideband stays under Nyquist. Most hardware FM synths did this.
- **Oversample.** 2× is usually enough for moderate indices; extreme FM needs more.
- **Use band-limited operators.** Sine operators are already band-limited, which is one reason
  classic FM uses sines exclusively. Using a sawtooth as a modulator sounds enormous and aliases
  catastrophically.

**The practical rule:** keep `fc + (I+1)·fm < fs/2`, or oversample. The DX7's characteristic
gritty high end is partly aliasing, and some people like it — which is a legitimate aesthetic
position as long as you know it is what you are hearing.

---

## 35.6 Why FM is hard to program, and how to cope

FM has a genuine reputation for being unintuitive, and the reason is worth stating.

In subtractive synthesis, parameters map onto perception: *cutoff* → brightness, *resonance* →
emphasis, *detune* → thickness. Turn a knob, get the expected change.

In FM, the index and the ratio interact through Bessel functions. Increasing the index does not
smoothly increase brightness — sidebands rise *and fall*, so a patch can get brighter, then
duller, then brighter again as you sweep the index. There is no monotonic knob.

**Three strategies that make it tractable:**

1. **Fix the ratio first, then explore the index.** The ratio decides *what kind of sound* it is
   (harmonic vs metallic); the index decides *how much*. Choose the category before the amount.
2. **Always envelope the index.** A static index is the sterile case. An index that decays is an
   instrument.
3. **Start from two operators.** Six-operator patches are combinatorially enormous. Almost every
   classic FM sound is two or three operators doing something simple, possibly layered.

---

## 35.7 Exercises

**35.1** Build a two-operator FM voice. Render a sweep of the index from 0 to 20 at a 1:1 ratio,
and FFT snapshots along the way. Count the sidebands and compare with `I + 1`.

**35.2** Render ratios 1:1, 2:1, 3:1, 1:2, 1.414:1, 3.5:1 at index 5. Which sound pitched and
which metallic?

**35.3** Implement the electric piano patch. Then change only the high-ratio operator's decay
time and listen to the "tine" appear and disappear.

**35.4** *Deliberate breakage.* Remove the index envelope from the brass patch. How much less like
brass is it?

**35.5** Set `fc = 2000`, `fm = 2000`, index 25 at 44.1 kHz. FFT the output and find the aliased
components. Then oversample 4× and compare.

**35.6** Implement operator feedback with and without the two-sample averaging. Sweep the feedback
from 0 to 1 and listen to the difference.

**35.7** Build the metallic cinematic hit from §35.4. Add Chapter 21's cathedral convolution and
a `tanh` saturator. This is a Chapter 85 impact layer.

**35.8** Compare true FM (modulating the increment) with PM (offsetting the read phase) using a
modulator that has a small DC offset. Show that FM drifts in pitch and PM does not.

**35.9** Implement the 6×6 matrix and reproduce three DX7 algorithms from published diagrams.

---

### Chapter summary

- FM synthesis is **phase modulation**: `sin(carPhase + index·sin(modPhase))`. Four lines.
- Sidebands appear at **`fc ± k·fm`**, with amplitudes given by Bessel functions. Significant
  sidebands extend to about **`I + 1`**.
- **The index controls brightness; the ratio controls harmonicity.** Integer ratios are
  harmonic; non-integer ratios are **inharmonic** — bells, gongs, metal, and dread.
- Subtractive synthesis **cannot** make inharmonic partials. This is FM's permanent territory,
  and it is why cinematic metallic sounds are FM-based.
- **Always envelope the index.** Static index = sterile; decaying index = an instrument. A bright
  attack settling to a mellow sustain is what real instruments do.
- Operators are sine + envelope + ratio + level. A **6×6 matrix** generalises the DX7's fixed
  algorithms. Feedback needs **two-sample averaging** or it goes chaotic.
- FM is nonlinear and **will alias** when `fc + (I+1)·fm` exceeds Nyquist. Limit the index at
  high pitches, or oversample.
- To program it: **fix the ratio first, envelope the index, start with two operators.**

**Next:** [Chapter 36 — Additive Synthesis](36-additive-synthesis.md)
