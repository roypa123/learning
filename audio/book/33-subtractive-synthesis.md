# Chapter 33 — Subtractive Synthesis and the Voice

> Start with something harmonically rich, then carve away what you do not want. That is
> subtractive synthesis, it is what almost every analogue synthesiser ever made does, and you now
> have every component it needs.

---

## 33.1 The architecture

```
   OSC 1 ──┐
   OSC 2 ──┼──► MIXER ──► FILTER ──► AMP ──► out
   NOISE ──┘                 ▲         ▲
                             │         │
                        FILTER ENV   AMP ENV
                             ▲         ▲
                            LFO ───────┘
```

Five blocks, and you have built all of them:

| Block | Chapter | Job |
|---|---|---|
| Oscillators | 30–32 | Raw harmonic material |
| Mixer | 14 | Blend sources, set levels |
| Filter | 23 | Remove harmonics — *the* creative stage |
| Amplifier + envelope | 13 | Shape the note's loudness over time |
| LFO | 34 | Slow modulation for movement |

**Two envelopes is the key insight.** The amplitude envelope says *how loud*; the filter envelope
says *how bright*. Brightness changing independently of loudness is what makes a synth sound
alive rather than like an organ with a volume pedal.

---

## 33.2 The voice

A **voice** is one note's worth of everything: oscillators, filter, envelopes, and its own state.
A polyphonic synth has many voices and assigns one per note.

**Code — `lib/include/audio/voice.h`** (core)

```cpp
namespace audio {

struct VoiceParams
{
    // Oscillators
    Waveform osc1Wave      = Waveform::Saw;
    Waveform osc2Wave      = Waveform::Saw;
    double   osc2DetuneCents = 7.0;
    int      osc2Semitones = 0;
    float    osc1Level     = 0.5f;
    float    osc2Level     = 0.5f;
    float    noiseLevel    = 0.0f;
    float    subLevel      = 0.0f;          // sine one octave down

    // Filter
    FilterType filterType  = FilterType::LowPass;
    double   cutoffHz      = 2000.0;
    double   resonance     = 0.9;
    double   keyTracking   = 0.5;           // 0 = fixed, 1 = follows pitch
    double   filterEnvAmount = 3000.0;      // in Hz, can be negative

    // Envelopes
    double ampA = 0.005, ampD = 0.2,  ampS = 0.7, ampR = 0.4;
    double fltA = 0.001, fltD = 0.25, fltS = 0.2, fltR = 0.3;

    // Per-voice
    double glideTime       = 0.0;           // portamento, seconds
    bool   resetPhaseOnNote = false;
};

class Voice
{
public:
    void prepare(double sampleRate)
    {
        sr_ = sampleRate;
        osc1_.setSampleRate(sr_);
        osc2_.setSampleRate(sr_);
        sub_.setSampleRate(sr_);
        filter_.setSampleRate(sr_);
        ampEnv_.setSampleRate(sr_);
        fltEnv_.setSampleRate(sr_);
        dc_.setSampleRate(sr_);
    }

    void setParams(const VoiceParams& p)
    {
        params_ = p;
        osc1_.setWaveform(p.osc1Wave);
        osc2_.setWaveform(p.osc2Wave);
        osc2_.setDetuneCents(p.osc2DetuneCents);
        sub_.setWaveform(Waveform::Sine);
        ampEnv_.setParameters(p.ampA, p.ampD, p.ampS, p.ampR);
        fltEnv_.setParameters(p.fltA, p.fltD, p.fltS, p.fltR);
    }

    void noteOn(int midiNote, float velocity)
    {
        note_     = midiNote;
        velocity_ = velocity;
        active_   = true;

        const double target = midiToFrequency(midiNote);

        if (params_.glideTime > 0.0 && currentFreq_ > 0.0)
            glideCoeff_ = std::exp(-1.0 / (params_.glideTime * sr_));
        else
        {
            currentFreq_ = target;
            glideCoeff_  = 0.0;
        }
        targetFreq_ = target;

        if (params_.resetPhaseOnNote)
        {
            osc1_.reset();
            osc2_.reset();
            sub_.reset();
        }

        ampEnv_.noteOn();
        fltEnv_.noteOn();
    }

    void noteOff()
    {
        ampEnv_.noteOff();
        fltEnv_.noteOff();
    }

    bool isActive() const { return active_ && ampEnv_.isActive(); }
    int  note()     const { return note_; }

    float nextSample()
    {
        if (!isActive()) { active_ = false; return 0.0f; }

        // --- glide: exponential in FREQUENCY RATIO, so it is linear in cents
        if (glideCoeff_ > 0.0)
            currentFreq_ = targetFreq_ + (currentFreq_ - targetFreq_) * glideCoeff_;

        osc1_.setFrequency(currentFreq_);
        osc2_.setFrequency(currentFreq_
                           * std::pow(2.0, params_.osc2Semitones / 12.0));
        sub_.setFrequency(currentFreq_ * 0.5);

        // --- sources -------------------------------------------------
        double s = 0.0;
        s += osc1_.nextSample() * params_.osc1Level;
        s += osc2_.nextSample() * params_.osc2Level;
        if (params_.subLevel   > 0.0f) s += sub_.nextSample()      * params_.subLevel;
        if (params_.noiseLevel > 0.0f) s += rng_.nextFloat()       * params_.noiseLevel;

        // --- filter --------------------------------------------------
        const double env = fltEnv_.nextSample();

        // Key tracking: higher notes get a higher cutoff, so the timbre
        // stays consistent across the keyboard.
        const double keyOffset = std::pow(currentFreq_ / 261.6256,
                                          params_.keyTracking);

        double cutoff = params_.cutoffHz * keyOffset
                      + params_.filterEnvAmount * env * velocity_;

        cutoff = std::clamp(cutoff, 20.0, sr_ * 0.45);

        // Recompute coefficients every 16 samples: cheap, and smooth enough
        // that zipper noise stays inaudible (Chapter 23 s23.8).
        if (++coeffCounter_ >= 16)
        {
            coeffCounter_ = 0;
            filter_.setFilter(params_.filterType, cutoff, params_.resonance);
        }

        s = filter_.processSample(static_cast<float>(s), 0);

        // --- amplifier -----------------------------------------------
        s *= ampEnv_.nextSample() * velocity_;

        return dc_.process(static_cast<float>(s));
    }

private:
    VoiceParams params_;
    Osc         osc1_, osc2_, sub_;
    Biquad      filter_;
    ADSR        ampEnv_, fltEnv_;
    DCBlocker   dc_;
    FastRandom  rng_{ 4321 };

    double sr_          = kDefaultRate;
    double currentFreq_ = 0.0;
    double targetFreq_  = 440.0;
    double glideCoeff_  = 0.0;
    int    note_        = -1;
    float  velocity_    = 1.0f;
    bool   active_      = false;
    int    coeffCounter_ = 0;
};

}   // namespace audio
```

---

## 33.3 Why each detail is there

**Two oscillators, slightly detuned.** Chapter 30: beating makes it sound alive. 5–10 cents is
the classic amount. With one oscillator the sound is thin and static; two is the minimum for
"synth".

**A sub-oscillator.** A sine one octave below the fundamental. It adds weight without adding
harmonic clutter, because a sine has nothing to clutter with. This is how you get a bass that is
huge on a club system and still audible on a phone (Chapter 3's missing fundamental supplies the
phone half). Chapter 83 uses it heavily.

**Noise.** Even a few percent adds "breath" and makes the sound less obviously synthetic. Real
instruments all have a noise component (Chapter 15).

**Key tracking.** Without it, a filter at 2 kHz makes a bass note bright and a high note dull —
because the high note's harmonics are all above the cutoff. Key tracking scales the cutoff with
pitch so the *timbre* stays consistent. `keyTracking = 1.0` means the cutoff tracks pitch
exactly; `0.5` is the usual musical compromise.

**Velocity to filter, not just to amplitude.** Hitting a real instrument harder makes it *louder
and brighter*. Routing velocity only to amplitude gives you a synth that sounds like a volume
knob. Routing it to the filter envelope amount is what makes it feel responsive.

**Coefficient recomputation every 16 samples.** Recomputing every sample costs `sin`, `cos` and
`pow` 44,100 times per second per voice. Every 16 samples is 2,756 times per second — inaudible
stepping, 16× cheaper. This is a standard optimisation and it is why modulation rate limits
exist in synths.

**Glide computed as an exponential approach to the target frequency.** Because the approach is
multiplicative, the perceived pitch moves linearly in cents — which is what sounds correct
(Chapter 11's logarithmic perception, again).

**A DC blocker on the output.** Pulse waves with non-50% duty carry DC (Chapter 12), resonant
filters can produce offsets, and asymmetric waveforms accumulate. One filter, no downside.

---

## 33.4 Filter topologies: why not a biquad?

Chapter 23 built the biquad, and our voice uses it. Real analogue-modelling synths generally do
not. Three reasons, worth knowing:

**1. Biquads do not self-oscillate gracefully.** Push Q high enough and a biquad goes unstable and
produces noise rather than a clean sine. Analogue filters self-oscillate into a pure tone, which
musicians use as an extra oscillator.

**2. Biquads do not modulate well.** Fast cutoff modulation makes the coefficients jump, and the
transient can be audible. Chapter 23's coefficient interpolation helps but does not fully solve
it.

**3. The nonlinearity is the sound.** A real Moog ladder filter saturates as the resonance
increases; a real MS-20 filter distorts aggressively. That nonlinearity is a large part of the
character, and a linear biquad has none of it.

The two standard alternatives:

**State Variable Filter (SVF).** Simulates an analogue integrator topology. It gives low-pass,
high-pass, band-pass and notch outputs *simultaneously* from one structure, modulates beautifully,
and its coefficients map almost directly to frequency and Q. The modern form — Andrew Simper's
**topology-preserving transform (TPT)** SVF — is stable under any modulation rate and is what most
current plugins use.

```cpp
// TPT state-variable filter -- the modern standard.
class SVF
{
public:
    void setSampleRate(double sr) { sr_ = sr; update(); }
    void setFrequency(double hz)  { fc_ = hz; update(); }
    void setResonance(double r)   { res_ = std::clamp(r, 0.0, 0.99); update(); }

    float processLowPass(float x)
    {
        const double v0 = static_cast<double>(x);
        const double v3 = v0 - ic2_;
        const double v1 = a1_ * ic1_ + a2_ * v3;
        const double v2 = ic2_ + a2_ * ic1_ + a3_ * v3;

        ic1_ = 2.0 * v1 - ic1_;
        ic2_ = 2.0 * v2 - ic2_;

        return static_cast<float>(v2);            // low-pass output
    }

private:
    void update()
    {
        const double g = std::tan(kPi * fc_ / sr_);
        const double k = 2.0 - 2.0 * res_;
        a1_ = 1.0 / (1.0 + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    double sr_ = kDefaultRate, fc_ = 1000.0, res_ = 0.0;
    double a1_ = 0, a2_ = 0, a3_ = 0, ic1_ = 0, ic2_ = 0;
};
```

Note `std::tan(π·fc/sr)` — the **bilinear transform's frequency warping**, which maps the
analogue frequency onto the digital one correctly right up to Nyquist. That is why this filter
behaves properly at high cutoffs where a naive digital filter would misbehave.

**Ladder filter.** Four one-pole filters in series with global feedback — the Moog topology.
24 dB/octave, and adding a `tanh` in the feedback path gives the saturation that defines the
sound. Needs oversampling (Chapter 29) because that `tanh` is a nonlinearity.

---

## 33.5 The classic sounds

Presets are the fastest way to learn what the parameters do. Each of these is a few numbers.

| Sound | Oscillators | Filter | Amp env | Filter env |
|---|---|---|---|---|
| **Bass** | Saw + sub, detune 5c | LP 400 Hz, Q 2 | A .002 D .3 S .6 R .1 | amount +2000, D .15 |
| **Acid** | Saw only | LP 300 Hz, **Q 12** | A .001 D .2 S 0 R .05 | amount +4000, D .25 |
| **Pad** | 2 saws, detune 12c | LP 1200 Hz, Q 0.7 | A **.8** D .3 S .8 R **1.5** | amount +800, A .6 |
| **Brass** | Saw + saw, detune 4c | LP 900 Hz, Q 1.5 | A .06 D .1 S .85 R .15 | amount +2500, A .05 D .3 |
| **Pluck** | Saw | LP 2000 Hz, Q 3 | A .001 D .25 S 0 R .1 | amount +3000, D .12 |
| **Strings** | 2 saws, detune 16c | LP 2500 Hz, Q 0.7 | A .35 D .2 S .9 R .8 | amount +500 |
| **Lead** | Saw + square, detune 8c | LP 1800 Hz, Q 4 | A .01 D .1 S .8 R .2 | amount +2000 |
| **Cinematic drone** | 2 saws, detune 25c, sub | LP 300 Hz, Q 1 | A **3.0** D 1 S 1 R **4.0** | amount +200, A 2.5 |

**The acid bass is worth studying**: Q = 12 is a very resonant filter, and a fast filter envelope
sweeping through a sawtooth's harmonic series picks out each harmonic in turn. That is the entire
TB-303 sound, and it is three parameters.

**The cinematic drone** is the same architecture with every time constant multiplied by fifty. A
three-second attack and a four-second release turn a synth patch into something that feels
geological. Chapter 84 builds on this.

---

## 33.6 Polyphony and voice stealing

A polyphonic synth holds an array of voices and assigns them.

```cpp
class Synth
{
public:
    void prepare(double sr, int numVoices = 16)
    {
        voices_.resize(static_cast<size_t>(numVoices));
        for (auto& v : voices_) v.prepare(sr);
    }

    void noteOn(int note, float velocity)
    {
        // 1. Reuse a voice already playing this note (retrigger).
        for (auto& v : voices_)
            if (v.isActive() && v.note() == note) { v.noteOn(note, velocity); return; }

        // 2. Any free voice.
        for (auto& v : voices_)
            if (!v.isActive()) { v.noteOn(note, velocity); return; }

        // 3. STEAL: take the quietest voice, not the oldest.
        Voice* quietest = &voices_[0];
        float  lowest   = 1e9f;
        for (auto& v : voices_)
            if (v.currentLevel() < lowest) { lowest = v.currentLevel(); quietest = &v; }

        quietest->steal(note, velocity);     // ramps down over ~2 ms first
    }

    void noteOff(int note)
    {
        for (auto& v : voices_)
            if (v.isActive() && v.note() == note) v.noteOff();
    }

    float nextSample()
    {
        double sum = 0.0;
        for (auto& v : voices_) sum += v.nextSample();
        return static_cast<float>(sum * outputGain_);
    }

private:
    std::vector<Voice> voices_;
    float outputGain_ = 0.25f;
};
```

**Steal the quietest, not the oldest.** The oldest voice might be a sustained pad note that the
listener is actively hearing; the quietest is by definition the one they will miss least. This
single choice is the difference between a synth that sounds like it has plenty of voices and one
that audibly drops notes.

**Stealing must ramp down first.** Cutting a voice mid-note is a discontinuity, which is a click
(Chapter 13). A 2 ms fade before reassignment is inaudible and eliminates it. Chapter 13's §13.7
gave the same advice for envelope retriggering; it is the same problem.

**Output gain of 0.25.** Sixteen voices summing can reach 16× one voice. In practice partial
correlation means around `√16 = 4×` for uncorrelated notes and worse for a unison chord.
Dividing by 4 is a reasonable default; a limiter (Chapter 51) on the master handles the rest.

---

## 33.7 Exercises

**33.1** Build the `Voice` class and render each preset from §33.5. Listen to all eight.

**33.2** Render the same patch with `keyTracking` at 0, 0.5 and 1.0, playing notes across four
octaves. Which sounds most consistent?

**33.3** *Deliberate breakage.* Route velocity only to amplitude, not to the filter. Play a
passage with varied velocity and compare.

**33.4** Change the coefficient recomputation interval from 16 to 1, 64 and 512 samples. At what
point does a fast filter sweep become audibly stepped? Measure the CPU difference.

**33.5** Implement the TPT SVF and compare it with the biquad on a fast filter sweep. Which
handles modulation better?

**33.6** Add a `tanh` to the SVF's feedback path to create saturation. Oversample it 4× (Chapter
29) and compare with and without.

**33.7** Build the polyphonic synth with 8 voices. Play a 12-note chord. Which notes get stolen?
Now change to oldest-first stealing and compare.

**33.8** *Deliberate breakage.* Remove the ramp-down from voice stealing. Play rapid overlapping
notes and listen for the clicks.

**33.9** Build the cinematic drone preset and render 30 seconds of a slow chord progression.
Add Chapter 21's convolution reverb. This is the raw material for Chapter 84.

---

### Chapter summary

- Subtractive synthesis: **rich source → filter → amplifier**, with **two envelopes** — one for
  loudness, one for brightness. Independent brightness is what makes it sound alive.
- A **voice** owns one note's oscillators, filter, envelopes and state.
- Details that matter: **two detuned oscillators** (beating), a **sine sub-oscillator** (weight
  without clutter), a little **noise** (breath), **key tracking** (consistent timbre across the
  keyboard), and **velocity routed to the filter** as well as the amplitude.
- **Recompute filter coefficients every ~16 samples**, not every sample — 16× cheaper and
  inaudible.
- **Glide exponentially in frequency**, which is linear in cents.
- Biquads are fine, but real synths use **TPT state-variable** or **ladder** filters: they
  self-oscillate cleanly, modulate without artefacts, and can carry the nonlinearity that defines
  the character. `tan(π·fc/sr)` is the bilinear frequency warping that makes them behave at high
  cutoffs.
- The classic patches are a handful of numbers. **Acid bass is just Q = 12 plus a fast filter
  envelope.** A cinematic drone is the same patch with every time constant ×50.
- **Steal the quietest voice, not the oldest**, and **ramp it down over ~2 ms** before reusing it.

**Next:** [Chapter 34 — LFOs and Modulation Routing](34-lfos-and-modulation.md)
