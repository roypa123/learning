# Chapter 41 — Project: A Polyphonic Synthesizer

> Part III's components, assembled into one instrument. This chapter is mostly architecture —
> how the pieces fit, what order things happen in, and the decisions that separate a synth that
> works from one that clicks, drifts and eats CPU.

---

## 41.1 The architecture

```
   ┌─────────────────────── SYNTH ───────────────────────────┐
   │                                                          │
   │  MIDI in ──► VOICE MANAGER ──► [ VOICE ] × N ──► SUM ──► │──► MASTER ──► out
   │                    │                 ▲                   │      │
   │                    │                 │                   │   (Part IV:
   │              note assignment    global mod               │    effects)
   │              voice stealing     (LFOs, aftertouch)       │
   └──────────────────────────────────────────────────────────┘

   ┌──────────────────── ONE VOICE ──────────────────────────┐
   │                                                          │
   │  OSC1 ─┐                                                 │
   │  OSC2 ─┼─► MIX ─► FILTER ─► AMP ─► pan ─► out            │
   │  SUB  ─┤            ▲        ▲                           │
   │  NOISE─┘            │        │                           │
   │                  FLT ENV  AMP ENV                        │
   │                     ▲        ▲                           │
   │                     └── MOD MATRIX ◄── LFOs, velocity,   │
   │                                        key, mod wheel    │
   └──────────────────────────────────────────────────────────┘
```

---

## 41.2 The order of operations per sample

This ordering is not arbitrary, and getting it wrong causes subtle bugs.

```
   1. Update global modulation sources (LFOs, tempo)
   2. For each active voice:
        a. Update per-voice modulation sources (envelopes)
        b. Evaluate the mod matrix -> destination values
        c. Apply smoothing to destinations
        d. Update oscillator frequencies (base * pitch mod)
        e. Generate oscillator samples
        f. Mix sources
        g. Update filter coefficients (every N samples)
        h. Filter
        i. Apply amplitude envelope
        j. Pan into the output buses
   3. Sum voices
   4. Master gain, DC block, limiter
```

**Why this order.**

**Global before per-voice** — a global LFO must have the same value for every voice in a given
sample, or voices drift out of phase with each other and a unison patch smears.

**Envelopes before the matrix** — the matrix reads envelope values, so they must be current.

**Smoothing after the matrix** — smoothing the *result* covers every source at once. Smoothing
each source separately is more work and misses the accumulation.

**Frequencies before generation** — obvious, and forgotten often enough to cause a one-sample
pitch lag in vibrato.

**Filter coefficients on a divider** — Chapter 33: every 16 samples, not every sample.

**Amplitude envelope last in the voice** — so the filter sees the full-level signal. Filtering a
signal that has already been faded gives a different (and usually worse) result at high
resonance, because the filter's self-oscillation is not scaled the same way.

---

## 41.3 Voice management

```cpp
class VoiceManager
{
public:
    void noteOn(int note, float velocity, uint64_t timestamp)
    {
        // 1. Retrigger a voice already playing this note.
        if (Voice* v = findPlaying(note)) { v->noteOn(note, velocity); return; }

        // 2. Take a genuinely free voice.
        if (Voice* v = findFree())        { v->start(note, velocity, timestamp); return; }

        // 3. Steal. Priority, worst first:
        //    - voices in RELEASE (the listener has let go of them)
        //    - then the quietest
        Voice* victim = findQuietestReleasing();
        if (!victim) victim = findQuietest();
        victim->steal(note, velocity, timestamp);
    }

    void noteOff(int note)
    {
        for (auto& v : voices_)
            if (v.isPlaying() && v.note() == note && !v.isHeldBySustain())
                v.noteOff();
    }

    void sustainPedal(bool down)
    {
        sustain_ = down;
        if (!down)
            for (auto& v : voices_)
                if (v.isHeldBySustain()) v.noteOff();
    }

private:
    std::vector<Voice> voices_;
    bool sustain_ = false;
};
```

**Stealing priority matters more than it sounds.** Releasing voices first is the important rule:
if the player has let go of a note, they have already stopped attending to it. Taking a sustained
note they are still holding is immediately noticeable.

**The sustain pedal is a common source of bugs.** A note released while the pedal is down must
*not* stop — it must be flagged as held-by-sustain and released when the pedal lifts. Getting
this wrong gives a synth where the pedal appears to do nothing, or where notes hang forever.

---

## 41.4 Unison

Unison is multiple detuned voices per note, and it is how you get the enormous modern lead and
pad sounds.

```cpp
struct UnisonSettings
{
    int    count       = 1;       // voices per note, 1..8
    double detuneCents = 12.0;    // total spread
    double stereoSpread = 0.8;    // 0 = mono, 1 = fully spread
    bool   randomPhase = true;
};

void startUnison(int note, float velocity, const UnisonSettings& u)
{
    for (int i = 0; i < u.count; ++i)
    {
        // Spread symmetrically about the centre: -1 .. +1
        const double pos = (u.count == 1) ? 0.0
                         : (2.0 * i / (u.count - 1) - 1.0);

        Voice* v = allocate();
        v->setDetuneCents(pos * u.detuneCents * 0.5);
        v->setPan(0.5 + pos * u.stereoSpread * 0.5);

        if (u.randomPhase)
            v->randomisePhase();          // see Chapter 30's supersaw note

        v->start(note, velocity);
    }
}
```

**Detune and pan must correlate.** Voices detuned sharp panned right, flat panned left, produces
a wide, moving image. Detuning without panning gives thickness but no width; panning without
detuning gives width but no movement. The combination is what makes a supersaw feel enormous.

**Unison multiplies your voice count.** 8-note polyphony with 7-voice unison is 56 voices. Budget
accordingly, and note that the voice-stealing logic must now steal whole *unison groups*, not
individual voices — stealing one voice out of seven leaves a detuned hole.

---

## 41.5 The master section

```cpp
float Synth::nextSample(float& left, float& right)
{
    double l = 0.0, r = 0.0;

    for (auto& v : voices_)
    {
        if (!v.isActive()) continue;
        float vl, vr;
        v.nextSample(vl, vr);
        l += vl;
        r += vr;
    }

    // Voice-count normalisation: uncorrelated summing is ~sqrt(N).
    const double norm = 1.0 / std::sqrt(std::max(1.0,
                            static_cast<double>(activeVoiceCount_)));

    l *= norm * masterGain_;
    r *= norm * masterGain_;

    // DC block, then a soft limiter as a safety net.
    l = dcL_.process(static_cast<float>(l));
    r = dcR_.process(static_cast<float>(r));

    left  = std::tanh(static_cast<float>(l));
    right = std::tanh(static_cast<float>(r));

    return 0.0f;
}
```

**Voice-count normalisation is a design decision, not an obvious win.** Dividing by `√N` keeps
the level roughly constant as you add notes — but it means a 10-note chord is not actually louder
than a single note, which some players find wrong. The alternatives:

- **No normalisation** plus a limiter: chords get louder, and the limiter catches the peaks.
  This is what most hardware does and arguably what players expect.
- **Fixed normalisation** by the maximum voice count: consistent, quiet.
- **√N** as above: a compromise.

**The `tanh` on the output** is Chapter 14's soft clipper used as a safety net. It cannot produce
a value outside ±1, so the synth can never clip the converter, and at normal levels it is
essentially transparent. Three characters, and it removes a whole class of complaint.

---

## 41.6 Presets

A preset is a struct. Serialising it is worth doing properly, because it defines what your synth
*is*.

```cpp
struct Preset
{
    std::string name;
    std::string author;
    std::string category;      // Bass, Lead, Pad, Keys, FX, Cinematic

    VoiceParams     voice;
    UnisonSettings  unison;
    std::array<LFOSettings, 3> lfos;
    std::vector<ModRouting>    modMatrix;

    double masterGain = 0.7;
    int    polyphony  = 16;
};
```

**Two rules learned the hard way:**

**Version your format.** Write a version number first. When you add a parameter in six months,
old presets must still load — with the new parameter at a sensible default, not at zero.

**Default every field in the struct.** A preset file missing a field must produce a working
sound, not silence or a scream. C++11 default member initialisers (Chapter 7) do this for free.

---

## 41.7 A cinematic patch

Since Part VIII is coming, here is the same architecture configured for scale rather than
musicality.

```cpp
Preset cinematicDrone()
{
    Preset p;
    p.name = "Tectonic";
    p.category = "Cinematic";

    // Very low, very wide
    p.voice.osc1Wave = Waveform::Saw;
    p.voice.osc2Wave = Waveform::Saw;
    p.voice.osc2DetuneCents = 22.0;       // wide detune -> slow, huge beating
    p.voice.osc2Semitones   = -12;        // an octave down
    p.voice.subLevel        = 0.6f;       // sine sub for weight
    p.voice.noiseLevel      = 0.04f;      // a little air

    // Filter: low and slow
    p.voice.cutoffHz        = 260.0;
    p.voice.resonance       = 1.1;
    p.voice.keyTracking     = 0.3;
    p.voice.filterEnvAmount = 300.0;

    // Geological time constants
    p.voice.ampA = 3.5;  p.voice.ampD = 2.0; p.voice.ampS = 1.0; p.voice.ampR = 6.0;
    p.voice.fltA = 5.0;  p.voice.fltD = 3.0; p.voice.fltS = 0.8; p.voice.fltR = 5.0;

    // Unison: 6 voices, wide
    p.unison.count        = 6;
    p.unison.detuneCents  = 30.0;
    p.unison.stereoSpread = 1.0;
    p.unison.randomPhase  = true;

    // Very slow modulation for life
    p.lfos[0] = { LFO::Shape::SmoothRandom, 0.037, false };
    p.lfos[1] = { LFO::Shape::Sine,         0.011, false };

    p.modMatrix = {
        { ModSource::LFO1, ModDest::FilterCutoff, 0.25f, ModSource::None },
        { ModSource::LFO2, ModDest::OscDetune,    0.15f, ModSource::None },
        { ModSource::LFO1, ModDest::Pan,          0.20f, ModSource::None },
    };

    p.polyphony = 8;
    return p;
}
```

**Three things make this cinematic rather than musical:**

1. **Time constants ×50.** A 3.5-second attack and a 6-second release. Nothing happens quickly.
2. **Wide detune (30 cents) with full stereo spread.** The beating is slow enough to read as
   *movement in space* rather than as chorus.
3. **Very slow random modulation.** At 0.037 Hz, a cycle is 27 seconds. The listener never
   consciously notices it changing, but it never sits still either.

Feed this through Chapter 21's cathedral convolution and Chapter 52's saturation and you have the
bed layer of a trailer cue. Chapter 84 builds the rest.

---

## 41.8 Performance

Budget for a 16-voice, 4-unison synth — 64 simultaneous voices:

| Component | Ops/sample/voice | × 64 voices |
|---|---|---|
| 2 wavetable oscillators | 16 | 1,024 |
| Sub oscillator | 8 | 512 |
| Noise | 6 | 384 |
| Mixing | 4 | 256 |
| Filter (biquad or SVF) | 12 | 768 |
| 2 envelopes | 8 | 512 |
| Mod matrix | ~20 | 1,280 |
| Pan and sum | 4 | 256 |
| **Total** | ~78 | **~5,000** |

At 44,100 samples per second: **220 million operations per second** — roughly 5–10% of one modern
core. Comfortable.

**The three optimisations that matter most, in order:**

1. **Skip inactive voices entirely.** Most of the time most voices are idle. A `continue` at the
   top of the loop is the single biggest win available.
2. **Divide the control rate.** Mod matrix and filter coefficients every 16–32 samples rather
   than every sample. This alone roughly halves the total.
3. **Process in blocks, not per sample.** Better cache behaviour and it lets the compiler
   vectorise. Chapter 63.

**And the thing not to do:** do not optimise the oscillator's inner arithmetic before doing those
three. The wins are an order of magnitude apart.

---

## 41.9 Exercises

**41.1** Assemble the full synth. Render a four-bar chord progression with the "pad" preset from
Chapter 33.

**41.2** Implement voice stealing with the priority in §41.3. Play 20 simultaneous notes on an
8-voice synth and listen for artefacts.

**41.3** *Deliberate breakage.* Steal the oldest voice instead of the quietest releasing one.
Play a sustained chord and then a fast run over it.

**41.4** Implement the sustain pedal correctly. Test: hold the pedal, play and release ten notes,
lift the pedal. All ten should release together.

**41.5** Implement unison with correlated detune and pan. Render the same patch with unison 1, 3,
5 and 7. Measure the stereo correlation of each (Chapter 75 previews the measurement; a simple
L·R correlation coefficient is enough).

**41.6** *Deliberate breakage.* Give every unison voice the same start phase. Measure the peak in
the first 50 samples against the random-phase version.

**41.7** Compare the three voice-count normalisation strategies in §41.5 by playing 1, 3, 6 and
10 note chords. Which feels right?

**41.8** Build the cinematic drone preset. Render 60 seconds of a slow chord progression, then add
convolution reverb.

**41.9** Profile the synth. Find where the time actually goes. Then implement the three
optimisations from §41.8 in order and measure each one's effect.

**41.10** Implement preset save/load with a version number. Add a new parameter, bump the version,
and verify old presets still load correctly.

---

### Chapter summary

- The architecture is: **MIDI → voice manager → N voices → sum → master**, with each voice being
  oscillators → mix → filter → amp, driven by envelopes and a mod matrix.
- **Order matters**: global modulation before per-voice; envelopes before the matrix; smoothing
  after it; the **amplitude envelope last**, so the filter sees full level.
- **Voice stealing priority**: retrigger the same note, then a free voice, then the **quietest
  releasing** voice, then the quietest. Always ramp down before reuse.
- **The sustain pedal** needs a held-by-sustain flag, or notes either hang or ignore the pedal.
- **Unison** needs **correlated detune and pan** — sharp voices right, flat left — and **random
  start phases**. Steal whole unison groups, not individual voices.
- Master: **voice-count normalisation** is a design choice (no normalisation + limiter is what
  most hardware does), a **DC blocker**, and a **`tanh` safety limiter** that costs three
  characters and prevents all converter clipping.
- **Presets: version the format and default every field**, or future changes break old sounds.
- A cinematic patch is the same architecture with **time constants ×50, wide detune with full
  stereo spread, and very slow random modulation**.
- 64 voices is about 220 M ops/sec — comfortable. Optimise in this order: **skip inactive
  voices**, **divide the control rate**, **process in blocks**. Do not micro-optimise before
  those three.

---

## Part III is complete

You can now synthesise: band-limited classic waveforms, wavetables with morphing, subtractive
voices with full modulation, FM including inharmonic metallic timbres, additive with
analysis/resynthesis, granular clouds and time-stretching, physically modelled strings and tubes,
modal resonators for any struck object, and procedural wind, rain, fire and engines — plus a
complete polyphonic instrument that ties it together.

**Part IV processes it.** Delay, modulation effects, four kinds of reverb, dynamics, distortion,
EQ, pitch shifting and vocoding.

**Next:** [Chapter 42 — Delay Lines and Circular Buffers](42-delay-lines.md)
