# Chapter 30 — Oscillator Architecture

> Part III builds instruments. It starts here, with the component every synthesiser has more of
> than anything else: the oscillator. You have been writing them since Chapter 10. This chapter
> makes one that is precise, modulatable, and fast enough to run sixty-four of at once.

---

## 30.1 The three jobs of an oscillator

```
   frequency ──►  PHASE ACCUMULATOR  ──► phase (0..1) ──►  SHAPER  ──► sample
                         ▲
                   modulation (vibrato, FM, drift, glide)
```

**Accumulate phase.** Advance by `freq/fs` each sample, wrapping at 1.0.
**Shape it.** Map phase to amplitude: `sin`, a wavetable lookup, a polynomial.
**Accept modulation.** Frequency and phase must both be modulatable per sample, cheaply.

The first is where precision lives, the second is where timbre lives, the third is where
musicality lives. Chapter 31 rebuilds the shaper to remove aliasing; Chapter 32 replaces it with
a table. This chapter gets the accumulator right.

---

## 30.2 Normalised phase

Chapter 10 used radians (0 to 2π). From here on we use **normalised phase**, 0.0 to 1.0:

```cpp
phase += frequency / sampleRate;
if (phase >= 1.0) phase -= 1.0;
```

Four reasons this is better:

- **The shapers are simpler.** A sawtooth is `2·p − 1`; a square is `p < 0.5`. No `2π` anywhere.
- **Wavetable indexing is direct**: `index = phase · tableSize`.
- **PolyBLEP** (Chapter 31) needs the distance to a discontinuity as a fraction of the phase
  increment, which is natural in normalised units.
- **One multiply is saved** per sample in the increment calculation.

For a sine you pay `sin(2π·p)`, which is one extra multiply — and Chapter 32 removes even that
with a table.

---

## 30.3 Phase precision

Chapter 6 said phase accumulators must be `double`. Here is the measurement.

At 44,100 Hz, one hour is 158.76 million samples. Each is one addition, and each addition rounds.

| Type | Mantissa bits | Error after 1 hour at 440 Hz |
|---|---|---|
| `float` | 24 | **~4 cents sharp — clearly audible** |
| `double` | 53 | ~10⁻⁹ cents — irrelevant |
| `uint32_t` fixed point | 32 | **exactly zero** |

The `float` figure is not a rounding curiosity; four cents is enough to make a sustained pad beat
against a reference. Two `float` oscillators started together drift apart audibly within minutes.

### Fixed-point phase

A `uint32_t` accumulator is an appealing alternative: **integer overflow wraps automatically**,
so there is no `if` and no rounding at all.

```cpp
uint32_t phase     = 0;
uint32_t increment = static_cast<uint32_t>(freq / sampleRate * 4294967296.0);

phase += increment;                    // wraps at 2^32 for free -- no branch

// Table index from the top bits:
const uint32_t index = phase >> (32 - tableBits);

// Fractional part for interpolation:
const float frac = static_cast<float>(phase << tableBits) / 4294967296.0f;
```

**Advantages:** exact (the only error is in the initial increment calculation), branchless, and
the table index falls out of a shift.

**Disadvantages:** frequency resolution is limited to `fs/2³² = 0.00001 Hz` (fine), it is awkward
for audio-rate phase modulation, and the code is less readable.

Hardware synthesisers and game audio engines use fixed point almost universally. Plugins mostly
use `double`. We use `double` for clarity and mention fixed point where it matters — Chapter 32's
wavetable oscillator is the natural place for it.

---

## 30.4 Wrapping correctly

```cpp
// WRONG at high frequencies:
if (phase >= 1.0) phase -= 1.0;

// CORRECT:
while (phase >= 1.0) phase -= 1.0;
```

If the increment exceeds 1.0 — which happens when the frequency exceeds the sample rate, and
audio-rate FM can produce exactly that — a single subtraction leaves the phase above 1.0 and
everything downstream breaks.

`std::fmod(phase, 1.0)` also works and handles any magnitude, but it is roughly ten times slower
than a subtraction. The `while` loop is the right compromise: one comparison in the normal case,
correct in the abnormal one.

**And negative phase** must wrap too, because phase modulation and reverse playback produce it:

```cpp
while (phase >= 1.0) phase -= 1.0;
while (phase <  0.0) phase += 1.0;
```

---

## 30.5 Modulation: two different things

Beginners conflate these, and they sound different.

### Frequency modulation (FM)

Change the *increment*:

```cpp
const double instantFreq = baseFreq * pitchRatio;   // from LFO, envelope, glide
phase += instantFreq / sampleRate;
```

The phase is continuous; only its rate changes. This is vibrato, pitch bend, portamento, sirens,
and Chapter 77's Doppler.

### Phase modulation (PM)

Add an offset to the phase *at read time*, without disturbing the accumulator:

```cpp
const double readPhase = wrap(phase + modulationAmount);
out = shape(readPhase);
phase += increment;          // the accumulator itself is untouched
```

**PM is what "FM synthesis" actually is.** The DX7 and every "FM" synth since perform phase
modulation, because it has two decisive advantages:

- **No DC drift.** True FM integrates the modulator, so any DC offset in it causes the pitch to
  wander. PM does not integrate, so it does not drift.
- **The modulation index is directly controllable**, which makes the timbre predictable.

Chapter 35 develops this. The naming confusion is historical and universal; when someone says FM
synthesis, they mean PM.

### Hard sync

Reset one oscillator's phase whenever another wraps:

```cpp
if (masterWrapped)
    slavePhase = 0.0;
```

The forced reset creates a discontinuity — which is a click, which is broadband, which is the
aggressive tearing sound of oscillator sync. Sweeping the slave's frequency over a fixed master
gives the classic sync lead.

It aliases horrendously in naive form, for exactly the reason Chapter 13 gave. Chapter 31's
PolyBLEP handles it, and doing sync *correctly* is one of the harder problems in synthesis
because the discontinuity does not land on a sample boundary.

---

## 30.6 The oscillator class

**Code — `lib/include/audio/oscillator.h`** (core)

```cpp
namespace audio {

class Osc
{
public:
    void setSampleRate(double sr)
    {
        sampleRate_ = sr;
        invSampleRate_ = 1.0 / sr;
        updateIncrement();
    }

    void setFrequency(double hz)
    {
        frequency_ = hz;
        updateIncrement();
    }

    // Detune in cents. 1200 cents = one octave.
    void setDetuneCents(double cents)
    {
        detuneRatio_ = std::pow(2.0, cents / 1200.0);
        updateIncrement();
    }

    void setWaveform(Waveform w) { waveform_ = w; }
    void setPulseWidth(double w) { pulseWidth_ = std::clamp(w, 0.001, 0.999); }

    // Free-running or reset-on-note. See section 30.7.
    void setPhase(double p)  { phase_ = wrap01(p); }
    void reset()             { phase_ = 0.0; wrapped_ = false; }

    double phase() const     { return phase_; }
    bool   justWrapped() const { return wrapped_; }   // for hard sync

    // One sample. `pmAmount` is phase modulation in cycles (Chapter 35).
    float nextSample(double pmAmount = 0.0)
    {
        const double readPhase = wrap01(phase_ + pmAmount);
        const float  out = shape(readPhase);

        phase_ += increment_;
        wrapped_ = false;
        while (phase_ >= 1.0) { phase_ -= 1.0; wrapped_ = true; }
        while (phase_ <  0.0) { phase_ += 1.0; wrapped_ = true; }

        return out;
    }

    // Per-sample frequency modulation: vibrato, glide, Doppler.
    float nextSample(double pmAmount, double freqRatio)
    {
        const double readPhase = wrap01(phase_ + pmAmount);
        const float  out = shape(readPhase);

        phase_ += increment_ * freqRatio;
        wrapped_ = false;
        while (phase_ >= 1.0) { phase_ -= 1.0; wrapped_ = true; }
        while (phase_ <  0.0) { phase_ += 1.0; wrapped_ = true; }

        return out;
    }

private:
    static double wrap01(double p)
    {
        p -= std::floor(p);
        return p;
    }

    void updateIncrement()
    {
        increment_ = frequency_ * detuneRatio_ * invSampleRate_;
    }

    float shape(double p) const;    // Chapter 31 replaces this

    double sampleRate_    = kDefaultRate;
    double invSampleRate_ = 1.0 / kDefaultRate;
    double frequency_     = 440.0;
    double detuneRatio_   = 1.0;
    double increment_     = 440.0 / kDefaultRate;
    double phase_         = 0.0;
    double pulseWidth_    = 0.5;
    bool   wrapped_       = false;
    Waveform waveform_    = Waveform::Sine;
};

}   // namespace audio
```

**Two design notes.**

**`invSampleRate_` is cached** because division is 10–20× slower than multiplication on most
CPUs, and this runs per sample per oscillator. With 64 voices × 3 oscillators × 44,100 samples,
that is 8.5 million divisions per second saved.

**`wrap01` uses `std::floor`** rather than a loop, because phase modulation can push the value
arbitrarily far. The loop form is used for the accumulator, where the excursion is bounded by one
increment.

---

## 30.7 Free-running versus reset

When a note starts, does the oscillator reset its phase to zero?

**Reset (phase = 0 on note-on):**
- Every note sounds identical. Reproducible, which matters for percussive sounds where the
  attack transient *is* the sound.
- Stacked oscillators start in phase, so the initial transient is strong and consistent.
- Essential for kick drums and any sound whose character depends on its first cycle.

**Free-running (phase continues):**
- Each note starts at a different point, so repeated notes vary slightly — more organic.
- **Detuned stacks do not all start in phase**, which avoids an unnaturally loud attack.
- This is what analogue oscillators do, since they never stop.

**The practical answer:** reset for percussion and bass; free-run for pads, strings and anything
detuned. Most synths offer the choice, and it is usually labelled "retrigger" or "phase reset".

> **A useful middle ground:** reset to a *randomised* phase on each note. You get the
> free-running variation without the phase relationship between stacked oscillators being
> arbitrary. Many modern synths default to this.

---

## 30.8 Detuning and the supersaw

Detuning is the single cheapest way to make a synth sound large, and it is worth understanding
why it works.

```
   cents = 1200 · log2(ratio)         ratio = 2^(cents/1200)
```

| Detune | Effect |
|---|---|
| 0 cents | Perfectly in tune. Static, thin. |
| ±3–8 cents | Subtle thickening. Slow beating (Chapter 2). |
| ±10–20 cents | Classic "fat" detune. Obvious movement. |
| ±30–50 cents | Out of tune, but deliberately — chorus-like |
| ±100 cents | A semitone. A different note. |

The mechanism is Chapter 2's **beating**. Two oscillators 10 cents apart at 220 Hz differ by
1.27 Hz, so they beat 1.27 times a second — slow amplitude movement that reads as "alive".

**The supersaw** (Roland JP-8000, and the foundation of an entire genre) stacks seven sawtooths
with a specific detune spread, random phases, and a level curve where the centre oscillator is
loudest:

```cpp
// Seven saws: one centre, three below, three above.
const double spread[7] = { -1.0, -0.66, -0.33, 0.0, 0.33, 0.66, 1.0 };

for (int i = 0; i < 7; ++i)
{
    osc[i].setDetuneCents(spread[i] * detuneAmount);
    osc[i].setPhase(rng.nextFloat() * 0.5 + 0.5);   // random start phase
    gain[i] = (i == 3) ? 1.0f : 0.6f;               // centre dominant
}
```

**Random phases matter.** With all seven starting at zero, the first cycle is seven times the
amplitude of one — a huge transient spike, and then it settles. Random phases spread the energy
and it sounds even from the start. Chapter 11's correlated-vs-uncorrelated summing explains why:
in phase, seven saws add at +17 dB; out of phase, +8 dB.

This matters for Chapter 84's braam, which is a supersaw at low pitch through a filter and
distortion.

---

## 30.9 Cost

Per sample, per oscillator, roughly:

| Waveform | Operations | Notes |
|---|---|---|
| Naive saw | 3 | `2p − 1` plus the accumulator |
| Naive square | 3 | A comparison |
| `std::sin` | ~20–40 | Transcendental; the expensive one |
| Table lookup, no interpolation | 4 | Plus a cache miss risk |
| Table lookup, linear interpolation | 8 | Chapter 32 |
| PolyBLEP saw | ~12 | Chapter 31 |
| Additive (N harmonics) | 40N | Reference only |

**A 64-voice synth with 3 oscillators per voice is 192 oscillators.** At 44,100 samples per
second that is 8.5 million oscillator-samples per second. At 40 operations each (using `sin`),
340 million operations per second — a meaningful fraction of a core just for oscillators.

This is why wavetables dominate: 8 operations instead of 40 is a 5× saving on the single
largest cost in a polyphonic synth.

---

## 30.10 Exercises

**30.1** Measure phase drift: run `float` and `double` accumulators at 440 Hz for 10 million
samples and compare the final phase. Convert the difference to cents.

**30.2** Implement a `uint32_t` fixed-point accumulator. Verify it is bit-exact over 100 million
samples. What is its frequency resolution?

**30.3** *Deliberate breakage.* Use `if` instead of `while` for wrapping and set the frequency to
`1.5 × fs`. What does the output look like?

**30.4** Implement vibrato two ways: by modulating the increment (FM) and by adding a modulated
offset to the read phase (PM). Render both at 6 Hz, ±50 cents. Do they sound identical? Now add
a DC offset to the modulator and compare again.

**30.5** Build a supersaw with 7 oscillators. Render it with all phases at zero and with random
phases. Measure the peak of the first 100 samples in each case.

**30.6** Implement hard sync: a master at 110 Hz and a slave sweeping 110 → 1500 Hz. Render it.
Then make a spectrogram and find the aliasing.

**30.7** Write `double centsToRatio(double)` and its inverse, and verify 1200 cents → 2.0 and
100 cents → 1.05946.

**30.8** Benchmark: 192 oscillators for 10 seconds, using `std::sin` versus a 2048-entry table
with linear interpolation. What is the speedup?

**30.9** Implement **portamento** (glide): when the frequency changes, ramp toward it
exponentially over a settable time rather than jumping. Verify the pitch curve is linear in
*cents*, not in hertz — gliding linearly in hertz sounds wrong, and hearing why is the point.

---

### Chapter summary

- An oscillator does three things: **accumulate phase**, **shape it**, and **accept modulation**.
- Use **normalised phase (0..1)**, not radians: simpler shapers, direct table indexing, natural
  PolyBLEP, one less multiply.
- **Phase must be `double`** (or fixed-point `uint32_t`). A `float` accumulator drifts ~4 cents
  per hour — audible. Fixed point wraps for free on integer overflow and is exact.
- **Wrap with `while`, not `if`**, and handle negative phase; audio-rate modulation produces both.
- **Cache `1/sampleRate`** — division is 10–20× slower than multiplication and this runs millions
  of times per second.
- **FM changes the increment; PM adds an offset at read time.** "FM synthesis" is actually PM,
  because PM does not integrate the modulator and so does not drift.
- **Reset phase** for percussion and bass, **free-run** for pads and detuned stacks; randomised
  start phase is a good default.
- Detuning works through **beating**. A supersaw needs **random start phases**, or the first cycle
  is a +17 dB spike.
- Oscillators are the largest single cost in a polyphonic synth — 192 of them is typical — which
  is the economic argument for wavetables (Chapter 32).

**Next:** [Chapter 31 — Band-Limited Oscillators: PolyBLEP and BLIT](31-band-limited-oscillators.md)
