# Chapter 34 — LFOs and Modulation Routing

> A sound that does not change is a sound nobody believes. Chapter 13 said so about envelopes;
> this chapter generalises it. Modulation is the difference between a synthesiser and a tone
> generator, and a good modulation architecture is what separates a usable instrument from a
> collection of knobs.

---

## 34.1 The LFO

A **Low Frequency Oscillator** is an oscillator you do not listen to. Typically 0.01 to 20 Hz —
below hearing — used to control something else.

It is the same phase accumulator as Chapter 30, with three differences:

- **No band-limiting needed.** At 2 Hz a sawtooth's harmonics are all far below Nyquist. Use the
  naive shapes; PolyBLEP would be wasted work.
- **It usually needs a bipolar/unipolar switch.** Some destinations want −1…+1 (pitch), others
  0…1 (amplitude, filter amount).
- **It needs tempo sync.** Musical modulation happens in bars and beats, not hertz.

```cpp
class LFO
{
public:
    enum class Shape { Sine, Triangle, Saw, RampUp, Square, SampleHold, SmoothRandom };

    void setSampleRate(double sr) { sr_ = sr; update(); }
    void setRateHz(double hz)     { rate_ = hz; tempoSync_ = false; update(); }

    // Tempo sync: divisionInBeats of 1.0 = one beat, 4.0 = a bar, 0.25 = 16ths.
    void setTempoSync(double bpm, double divisionInBeats)
    {
        rate_ = bpm / 60.0 / divisionInBeats;
        tempoSync_ = true;
        update();
    }

    void setShape(Shape s)        { shape_ = s; }
    void setPhaseOffset(double p) { phaseOffset_ = p; }
    void setUnipolar(bool u)      { unipolar_ = u; }

    void reset() { phase_ = phaseOffset_; held_ = rng_.nextFloat(); }

    float next()
    {
        const double p = wrap01(phase_ + phaseOffset_);
        float v = 0.0f;

        switch (shape_)
        {
            case Shape::Sine:     v = static_cast<float>(std::sin(kTwoPi * p)); break;
            case Shape::Triangle: v = static_cast<float>(4.0 * std::fabs(p - 0.5) - 1.0); break;
            case Shape::Saw:      v = static_cast<float>(1.0 - 2.0 * p); break;
            case Shape::RampUp:   v = static_cast<float>(2.0 * p - 1.0); break;
            case Shape::Square:   v = (p < 0.5) ? 1.0f : -1.0f; break;

            case Shape::SampleHold:
                v = held_;
                break;

            case Shape::SmoothRandom:
                // Interpolate between random values: a wandering, organic LFO.
                v = static_cast<float>(prev_ + (target_ - prev_) * smoothstep(p));
                break;
        }

        phase_ += increment_;
        if (phase_ >= 1.0)
        {
            phase_ -= 1.0;
            prev_   = target_;
            target_ = rng_.nextFloat();
            held_   = target_;
        }

        return unipolar_ ? (v * 0.5f + 0.5f) : v;
    }

private:
    static double smoothstep(double t) { return t * t * (3.0 - 2.0 * t); }
    static double wrap01(double p)     { return p - std::floor(p); }
    void update() { increment_ = rate_ / sr_; }

    double sr_ = kDefaultRate, rate_ = 1.0, increment_ = 1.0 / kDefaultRate;
    double phase_ = 0.0, phaseOffset_ = 0.0;
    float  held_ = 0.0f, prev_ = 0.0f, target_ = 0.0f;
    bool   unipolar_ = false, tempoSync_ = false;
    Shape  shape_ = Shape::Sine;
    FastRandom rng_{ 77 };
};
```

**Sample & Hold** is worth noting: a new random value at each cycle, held flat in between. It
produces the stepped, burbling modulation of every sci-fi computer sound ever made, and it is
four lines.

**Smooth random** interpolates between those values with a smoothstep curve, giving organic
drift. This is the shape to use for anything meant to sound natural — analogue drift, wind
intensity, a creature's breathing.

---

## 34.2 Where LFOs go, and what they are called

The same LFO at the same rate has completely different names depending on its destination:

| Destination | Effect | Called | Typical rate/depth |
|---|---|---|---|
| Pitch | Wavering pitch | **Vibrato** | 4–7 Hz, ±10–50 cents |
| Amplitude | Pulsing volume | **Tremolo** | 3–8 Hz, 10–50% |
| Filter cutoff | Wah, movement | **Auto-wah / filter LFO** | 0.1–8 Hz |
| Pulse width | Thickening | **PWM** | 0.2–2 Hz |
| Pan position | Movement across the field | **Auto-pan** | 0.1–2 Hz |
| Delay time | Pitch wobble | **Chorus / vibrato** | 0.1–5 Hz (Ch 44) |
| Another LFO's rate | Chaos | Cross-modulation | any |

**The rate is the parameter that defines musicality.** Some numbers worth knowing:

- **4–7 Hz** is natural vibrato. Singers and string players land in this range because it is the
  speed of a relaxed muscle tremor. Faster sounds nervous; slower sounds seasick.
- **Below 0.5 Hz** stops reading as "modulation" and starts reading as "the sound is evolving."
  This is the range for pads, drones and cinematic beds.
- **Above 20 Hz** crosses into audio rate, and the LFO stops being an LFO. See §34.5.

---

## 34.3 The modulation matrix

A fixed routing — "LFO 1 goes to pitch" — is limiting. A **modulation matrix** lets any source
reach any destination with any depth.

```cpp
enum class ModSource { None, LFO1, LFO2, LFO3, AmpEnv, FilterEnv, ModEnv,
                       Velocity, KeyTrack, ModWheel, Aftertouch, Random };

enum class ModDest   { None, Osc1Pitch, Osc2Pitch, OscDetune, PulseWidth,
                       WavetablePosition, FilterCutoff, FilterResonance,
                       Amplitude, Pan, FMAmount, LFO1Rate, LFO2Depth };

struct ModRouting
{
    ModSource source = ModSource::None;
    ModDest   dest   = ModDest::None;
    float     amount = 0.0f;           // bipolar: -1 .. +1
    ModSource via    = ModSource::None; // optional: scales `amount`
};

class ModMatrix
{
public:
    void addRouting(ModRouting r) { routings_.push_back(r); }

    // Called once per sample (or per control block) after updating sources.
    void compute()
    {
        std::fill(dest_.begin(), dest_.end(), 0.0f);

        for (const auto& r : routings_)
        {
            if (r.dest == ModDest::None || r.source == ModSource::None) continue;

            float v = source_[static_cast<size_t>(r.source)] * r.amount;

            // A "via" source scales the routing -- this is how a mod wheel
            // controls the DEPTH of vibrato rather than the pitch directly.
            if (r.via != ModSource::None)
                v *= source_[static_cast<size_t>(r.via)];

            dest_[static_cast<size_t>(r.dest)] += v;
        }
    }

    void  setSource(ModSource s, float v) { source_[static_cast<size_t>(s)] = v; }
    float get(ModDest d) const            { return dest_[static_cast<size_t>(d)]; }

private:
    std::vector<ModRouting> routings_;
    std::array<float, 16> source_{};
    std::array<float, 16> dest_{};
};
```

**The `via` field is the feature that makes a matrix musical.** Without it, a mod wheel routed to
pitch bends the pitch. With it, the mod wheel scales the LFO-to-pitch routing, so the wheel
controls *how much vibrato there is* — which is what a player actually wants.

**Destinations accumulate.** Three sources routed to cutoff all add. That is correct and
expected, and it means you must clamp at the destination.

---

## 34.4 Applying modulation correctly

This is where most of the bugs live.

### Pitch must be modulated in cents, not hertz

```cpp
// WRONG: the vibrato depth changes with the note
freq = baseFreq + modValue * 20.0;          // +/- 20 Hz

// RIGHT: constant perceived depth at every pitch
freq = baseFreq * centsToRatio(modValue * 50.0);   // +/- 50 cents
```

±20 Hz on a 50 Hz bass note is a huge interval; on a 2 kHz lead it is inaudible. Cents are
perceptually uniform (Chapter 11), so modulating in cents gives the same *musical* depth at every
pitch. This is the single most common modulation bug.

### Filter cutoff must be modulated in octaves

Same argument:

```cpp
cutoff = baseCutoff * std::pow(2.0, modValue * octaveRange);
```

### Every modulated parameter must be clamped

```cpp
cutoff = std::clamp(cutoff, 20.0, sampleRate * 0.45);
```

Chapter 24: a cutoff at or above Nyquist produces coefficients outside the stability triangle,
and the filter screams. **Modulation is exactly how a parameter ends up somewhere you did not
intend**, so the clamp belongs at the destination, not at the source.

### Smooth everything that can jump

If the matrix is evaluated once per block rather than per sample, the destination values step.
Chapter 13: steps are clicks. Chapter 61 covers this properly; the short version is a one-pole
smoother on each destination:

```cpp
smoothed += (target - smoothed) * coeff;    // the one-pole filter, again
```

---

## 34.5 When modulation becomes audio

Take an LFO and raise its rate from 1 Hz to 1 kHz. Something changes at about 20 Hz.

Below 20 Hz you hear **modulation**: the sound wobbling. Above 20 Hz you stop hearing wobble and
start hearing **new frequencies** — sidebands.

For amplitude modulation by a sine at `fm` on a carrier at `fc`, the result contains:

```
   fc - fm,   fc,   fc + fm
```

Two sidebands appear, and the original wobble has become timbre. At `fc = 440`, `fm = 100`, you
hear 340, 440 and 540 Hz — a chord, not a tremolo.

This is:

- **Ring modulation** when the carrier is fully multiplied (no DC in the modulator), which
  removes the carrier and leaves only the sidebands. The classic Dalek voice, and a staple of
  horror and sci-fi sound design.
- **Amplitude modulation** when the modulator has a DC offset, so the carrier survives.
- **FM/PM synthesis** when the modulation is applied to phase rather than amplitude — Chapter 35.

**The boundary at ~20 Hz is not in the mathematics.** The physics is identical at 5 Hz and
500 Hz; only your auditory system changes its interpretation. This is the same perceptual
boundary as Chapter 2's beating-becomes-roughness-becomes-two-tones, and it is worth
experiencing as a continuous sweep rather than reading about.

---

## 34.6 Modulating with envelopes

An envelope is a one-shot modulation source, and a **mod envelope** — a third ADSR routed
anywhere — is one of the most useful additions to a synth.

| Routing | Result |
|---|---|
| Mod env → pitch, fast decay | A "blip" attack. Kick drums, synth toms (Chapter 13's kick) |
| Mod env → FM amount | Bright attack settling to a mellow sustain — what real instruments do |
| Mod env → wavetable position | Timbre morphs through the note |
| Mod env → LFO depth, slow attack | **Delayed vibrato** — the single most human modulation |

That last one deserves emphasis. Real players do not start a note with vibrato; they let the note
speak and then add vibrato. Routing `Velocity → LFO1 depth` via a slow-attack envelope reproduces
that, and it is the difference between a string patch that sounds like a machine and one that
sounds like a player.

---

## 34.7 Exercises

**34.1** Implement the LFO with all seven shapes. Render each modulating the pitch of a sawtooth
at 2 Hz, ±100 cents.

**34.2** *Deliberate breakage.* Modulate pitch in hertz instead of cents. Play the same vibrato
depth across four octaves and listen.

**34.3** Implement tempo sync and verify that at 120 bpm with division 1.0, the LFO completes one
cycle every 0.5 s.

**34.4** Build the mod matrix and set up: LFO1 → pitch via ModWheel (delayed vibrato), FilterEnv
→ cutoff, Velocity → cutoff, LFO2 → pulse width. Render a musical phrase.

**34.5** Sweep an LFO's rate from 0.5 Hz to 500 Hz over 20 seconds while it amplitude-modulates a
440 Hz sine. Note the point at which you stop hearing tremolo and start hearing sidebands.

**34.6** Implement ring modulation and verify the output contains only `fc ± fm` with no carrier.
FFT it to confirm.

**34.7** Build delayed vibrato: a mod envelope with a 0.8 s attack routed to LFO depth. Compare
with immediate vibrato on a sustained note.

**34.8** Add smoothing to every mod destination and measure the CPU cost. Then remove it and
evaluate the matrix once per 512-sample block — can you hear the stepping?

**34.9** Implement `SmoothRandom` and use it to add slow, subtle drift to oscillator tuning
(±3 cents) and filter cutoff (±5%). Compare a pad with and without. This is analogue-drift
emulation and it is remarkably effective.

---

### Chapter summary

- An **LFO** is a phase accumulator you do not listen to. No band-limiting needed; add
  **unipolar/bipolar** and **tempo sync**.
- **Sample & Hold** gives stepped random modulation; **smooth random** gives organic drift — the
  shape for anything meant to sound natural.
- The same LFO is called vibrato, tremolo, auto-wah, PWM, auto-pan or chorus depending only on
  its destination. **4–7 Hz is natural vibrato; below 0.5 Hz reads as evolution rather than
  modulation.**
- A **modulation matrix** routes any source to any destination with a depth. The **`via`** field
  — one source scaling another routing — is what makes it musical: it is how a mod wheel controls
  vibrato *depth*.
- **Modulate pitch in cents and cutoff in octaves**, never in hertz. This is the most common
  modulation bug.
- **Clamp at the destination.** Modulation is precisely how a parameter reaches an illegal value.
- **Smooth every destination**, or block-rate modulation steps and clicks.
- Above ~20 Hz, modulation stops being wobble and becomes **sidebands** at `fc ± fm` — ring
  modulation, AM, and (with phase) FM. The boundary is perceptual, not mathematical.
- **Delayed vibrato** — a slow envelope on LFO depth — is the single most human-sounding
  modulation you can add.

**Next:** [Chapter 35 — FM and Phase-Modulation Synthesis](35-fm-synthesis.md)
