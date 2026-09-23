# Chapter 55 — Vocoders, Formants, and Voice Processing

> The voice is the sound humans are most sensitive to, and the one where every artefact is
> noticed. It is also the most exploited source in cinematic sound design. This chapter closes
> Part IV with the tools for analysing, resynthesising and transforming it.

---

## 55.1 The source-filter model

Speech production splits cleanly into two independent parts:

```
   SOURCE                    FILTER                 OUTPUT
   (vocal folds or           (vocal tract:
    turbulent air)            throat, mouth, nose)

   buzz at pitch f0  ───►   resonances   ───►   vowel sound
   or
   noise             ───►   resonances   ───►   consonant
```

**The source** carries **pitch** and **voiced/unvoiced** character:
- **Voiced** (vowels, `m`, `n`, `z`, `v`): the vocal folds vibrate, producing a buzzy harmonic
  series at the fundamental.
- **Unvoiced** (`s`, `f`, `sh`, `t`, `k`): turbulent airflow, producing noise.

**The filter** is the vocal tract's resonances — the **formants** — and it carries **which sound
it is**. Formants are determined by the shape of the throat, mouth and tongue, not by pitch.

**The separation is nearly perfect**, and it is why:

- You can sing the same vowel at any pitch (source changes, filter does not).
- You can whisper intelligibly (source is noise, filter unchanged).
- Chapter 54's chipmunk effect happens (scaling the whole spectrum moves the filter, which should
  have stayed put).
- A vocoder works at all.

### The formants

| Vowel | F1 (Hz) | F2 (Hz) | F3 (Hz) |
|---|---|---|---|
| "ee" (beet) | 270 | 2290 | 3010 |
| "ih" (bit) | 390 | 1990 | 2550 |
| "eh" (bet) | 530 | 1840 | 2480 |
| "ae" (bat) | 660 | 1720 | 2410 |
| "ah" (father) | 730 | 1090 | 2440 |
| "aw" (bought) | 570 | 840 | 2410 |
| "oo" (boot) | 300 | 870 | 2240 |
| "uh" (but) | 640 | 1190 | 2390 |

**F1 and F2 alone identify the vowel.** Plot them against each other and the vowels occupy
distinct regions — the "vowel space", which is essentially the same shape for every language.
F1 correlates with how open the mouth is; F2 with how far forward the tongue is.

This means **two band-pass filters on a buzz produce a recognisable vowel**, which is the cheapest
possible speech synthesis and a very effective sound-design tool.

---

## 55.2 The channel vocoder

The classic design (Dudley, 1939 — originally for telephone bandwidth compression, later
borrowed by musicians).

```
   MODULATOR (voice) ──► [ filter bank ] ──► [ envelope followers ]
                                                      │
                                                      ▼ (gains)
   CARRIER (synth) ────► [ filter bank ] ──► [ × ] ──► SUM ──► out
```

**Split both signals into the same frequency bands. Measure the modulator's level in each band.
Apply those levels to the carrier's corresponding bands.**

The result: the carrier acquires the modulator's spectral shape — the synth "speaks".

```cpp
class Vocoder : public Processor
{
public:
    void prepare(double sr, int) override
    {
        sr_ = sr;

        // Logarithmically spaced bands, matching the ear (Chapter 3).
        const double lowHz = 80.0, highHz = 8000.0;

        for (size_t i = 0; i < kNumBands; ++i)
        {
            const double t = static_cast<double>(i) / (kNumBands - 1);
            const double centre = lowHz * std::pow(highHz / lowHz, t);

            // Q set so bands overlap slightly: adjacent centres about
            // one bandwidth apart.
            const double q = 4.0;

            modBands_[i].setSampleRate(sr);
            modBands_[i].setFilter(FilterType::BandPass, centre, q);

            carBands_[i].setSampleRate(sr);
            carBands_[i].setFilter(FilterType::BandPass, centre, q);

            followers_[i].setSampleRate(sr);
            followers_[i].setAttackMs(3.0);
            followers_[i].setReleaseMs(15.0);
        }

        sibilanceHP_.setSampleRate(sr);
        sibilanceHP_.setFilter(FilterType::HighPass, 4000.0, 0.707);
    }

    float process(float modulator, float carrier)
    {
        double out = 0.0;

        for (size_t i = 0; i < kNumBands; ++i)
        {
            const float m = modBands_[i].processSample(modulator, 0);
            const float env = followers_[i].process(m);

            const float c = carBands_[i].processSample(carrier, 1);

            out += c * env * bandGain_;
        }

        // --- the sibilance bypass -----------------------------------
        // Consonants like s, f, sh are NOISE. A harmonic carrier cannot
        // reproduce them, so intelligibility collapses without this.
        const float sibilance = sibilanceHP_.processSample(modulator, 2);
        const float sibEnv    = sibilanceFollower_.process(sibilance);

        out += noise_.nextFloat() * sibEnv * sibilanceAmount_;

        return static_cast<float>(out);
    }

private:
    static constexpr size_t kNumBands = 20;

    std::array<Biquad, kNumBands>           modBands_, carBands_;
    std::array<EnvelopeFollower, kNumBands> followers_;
    Biquad sibilanceHP_;
    EnvelopeFollower sibilanceFollower_;
    FastRandom noise_{ 1234 };
    double sr_ = kDefaultRate;
    float  bandGain_ = 3.0f, sibilanceAmount_ = 0.5f;
};
```

### The three things that make a vocoder intelligible

**1. Enough bands.** 8 bands is a recognisable robot voice; 16–24 is intelligible; 32+ is
detailed. Below 8, the formants are too coarsely sampled to identify vowels.

**2. The sibilance bypass.** This is the detail that separates a working vocoder from an
unintelligible one. Consonants like `s`, `sh` and `f` are broadband noise. A harmonic carrier —
a saw wave — has energy only at its harmonics, so it simply cannot produce noise. Detecting
high-frequency energy in the modulator and adding noise directly restores the consonants.

**Without it, "sister" becomes "i-e" and nothing is intelligible.**

**3. A harmonically rich carrier.** A sine has one harmonic; the vocoder can only shape what is
there. A sawtooth or a wide supersaw has energy everywhere and shapes beautifully. Noise also
works and gives a whispered character.

### Envelope follower timing

The attack and release times are a genuine trade-off:

- **Too fast** (< 1 ms): the envelope follows the modulator's waveform, not its envelope, and you
  get buzzing and distortion.
- **Too slow** (> 50 ms): consonants smear together and words become unintelligible.
- **3–15 ms** is the working range, and faster attack than release.

---

## 55.3 LPC: the analytical approach

**Linear Predictive Coding** models the vocal tract directly as an all-pole filter, and fits its
coefficients to the signal.

**The premise:** each sample can be predicted from the previous `p` samples:

```
   x[n] ≈ a1·x[n-1] + a2·x[n-2] + ... + ap·x[n-p]
```

Find the `a` coefficients that minimise the prediction error. Those coefficients **are** the
vocal tract filter; the leftover error — the **residual** — is the source.

```cpp
// Levinson-Durbin: solves for the LPC coefficients from the
// autocorrelation, in O(p^2) rather than O(p^3).
std::vector<double> levinsonDurbin(const std::vector<double>& autocorr, int order)
{
    std::vector<double> a(order + 1, 0.0);
    std::vector<double> temp(order + 1, 0.0);

    double error = autocorr[0];
    if (error <= 0.0) return a;

    for (int i = 1; i <= order; ++i)
    {
        double acc = autocorr[i];
        for (int j = 1; j < i; ++j)
            acc -= a[j] * autocorr[i - j];

        const double reflection = acc / error;

        temp[i] = reflection;
        for (int j = 1; j < i; ++j)
            temp[j] = a[j] - reflection * a[i - j];

        for (int j = 1; j <= i; ++j)
            a[j] = temp[j];

        error *= (1.0 - reflection * reflection);
        if (error <= 0.0) break;
    }

    return a;
}
```

**What LPC gives you:**

- **The filter coefficients** — the formant structure, compactly (order 12–20 for speech).
- **The residual** — the excitation. Inspect it: periodic means voiced, noisy means unvoiced.
- **The pitch**, from autocorrelating the residual.

**And then you can recombine them arbitrarily:**

| Combination | Result |
|---|---|
| Original filter + original residual | Perfect reconstruction |
| Original filter + **synthetic buzz** | Classic robot voice (LPC-10 telephony) |
| Original filter + **noise** | Whisper |
| Original filter + **a different sound** | Cross-synthesis — a guitar that talks |
| **Scaled** filter + original residual | Formant shift without pitch change |
| Original filter + **pitch-shifted** residual | Pitch shift with formants preserved |

**Cross-synthesis via LPC is the technique** behind talking instruments, and it is more precise
than a channel vocoder because the filter is fitted rather than sampled by fixed bands.

**It is also the basis of speech codecs.** Transmitting 12 filter coefficients plus a pitch and a
voicing flag, updated every 20 ms, is about 2.4 kbit/s — which is how early digital voice
communication worked.

---

## 55.4 The talkbox and formant filters

**A talkbox** is not a vocoder. A speaker drives a tube into the performer's mouth; their actual
vocal tract filters it, and a microphone picks up the result. The filtering is genuine, which is
why it sounds more organic than a vocoder.

**Digital approximation:** a bank of resonant band-pass filters at the formant frequencies:

```cpp
class FormantFilter
{
public:
    void setVowel(double f1, double f2, double f3, double sr)
    {
        formant_[0].setFilter(FilterType::BandPass, f1, 8.0);
        formant_[1].setFilter(FilterType::BandPass, f2, 10.0);
        formant_[2].setFilter(FilterType::BandPass, f3, 12.0);

        // Higher formants are progressively quieter.
        gains_ = { 1.0f, 0.6f, 0.3f };
    }

    // Interpolate between two vowels -- this is how you get a
    // continuously morphing "aaa-eee-ooo".
    void morphVowels(const Vowel& a, const Vowel& b, double t, double sr)
    {
        // Interpolate in LOG frequency, because formant perception is
        // logarithmic like everything else (Chapter 11).
        setVowel(std::exp(std::log(a.f1) * (1-t) + std::log(b.f1) * t),
                 std::exp(std::log(a.f2) * (1-t) + std::log(b.f2) * t),
                 std::exp(std::log(a.f3) * (1-t) + std::log(b.f3) * t), sr);
    }

    float process(float x)
    {
        double sum = 0.0;
        for (size_t i = 0; i < 3; ++i)
            sum += formant_[i].processSample(x, static_cast<int>(i)) * gains_[i];
        return static_cast<float>(sum);
    }

private:
    std::array<Biquad, 3> formant_;
    std::array<float, 3>  gains_{};
};
```

**Interpolating in log frequency** is the detail that makes vowel morphing sound smooth. Linear
interpolation between 270 Hz and 730 Hz passes through 500 Hz at the midpoint; log interpolation
passes through 444 Hz, which is where the ear expects it.

**This is a genuinely useful sound-design tool** beyond speech: applying formant filters to a
synth pad makes it sound like a choir; applying them to noise makes it sound like wind with a
voice in it; sweeping them slowly over a drone is one of the more effective ways to make
something sound alive and vaguely intelligent.

---

## 55.5 Voice processing for cinematic work

The voice is the most manipulated source in cinematic sound. The standard transformations:

| Effect | Method |
|---|---|
| **Monster / demon** | Pitch down 5–12 semitones, formants down further, add a sub layer, distortion |
| **Child / creature** | Pitch up, formants up more, add breathiness |
| **Robot** | Vocoder with a synth carrier, or LPC with a fixed-pitch buzz |
| **Radio / walkie** | Band-pass 400–3000 Hz, distortion, noise, occasional dropout |
| **Telephone** | Band-pass 300–3400 Hz, steep, slight codec artefacts |
| **Whisper** | LPC filter + noise excitation |
| **Ghost / ethereal** | Pitch shift with heavy phase-vocoder smear, long reverb, granular |
| **Crowd from one voice** | Many copies with random pitch (±30 cents), timing (±80 ms) and pan |
| **Alien / unintelligible** | Formant shift in the opposite direction to pitch |
| **Possessed / two voices** | Layer the original with a heavily processed version, timing-aligned |

**The most effective single technique** is the last row of the "Monster" entry combined with the
"Alien" principle: **shift pitch down while shifting formants in a way that does not match.**

The reason is perceptual. Chapter 3: the listener's auditory system computes a *body size* from
the formant positions and a *pitch* from the fundamental. In every real creature these are
consistent — big things have low formants and low pitch. Making them inconsistent produces
something the auditory system cannot assign a size to, and that failure is experienced as
*wrongness*.

**That is a more reliable route to unease than any amount of dissonance or reverb**, and it costs
two parameters.

### Layering voices

For crowds, mobs and choirs from a single recording:

```cpp
for (int i = 0; i < numCopies; ++i)
{
    const double pitchCents = rng.nextFloat() * 30.0;      // +/- 30 cents
    const double timingMs   = rng.nextFloat() * 80.0;      // +/- 80 ms
    const double pan        = rng.nextFloat();
    const double formantShift = rng.nextFloat() * 5.0;     // +/- 5%, for variety

    // The formant variation is what stops it sounding like one person
    // multi-tracked. Different people have different vocal tract sizes.
    addLayer(process(source, pitchCents, formantShift), timingMs, pan);
}
```

**The formant randomisation is the important part.** Randomising pitch and timing alone gives you
one person multitracked — which is a recognisable sound and not a crowd. Randomising formants
gives you *different people*, because formants are the acoustic signature of body size.

---

## 55.6 Exercises

**55.1** Build the formant filter and render all eight vowels from §55.1 on a 120 Hz sawtooth.
Can you identify them?

**55.2** Morph between "ah" and "ee" over 3 seconds, interpolating linearly and then
logarithmically. Which sounds smoother?

**55.3** Build the 20-band channel vocoder. Use a saw carrier and a speech modulator.

**55.4** *Deliberate breakage.* Remove the sibilance bypass. Try to understand a sentence
containing several `s` sounds.

**55.5** Compare 4, 8, 16 and 32 bands. At what count does speech become intelligible?

**55.6** Vary the envelope follower times from 1 ms to 100 ms. Find the range where speech is
clearest.

**55.7** Implement LPC analysis with Levinson-Durbin at order 16. Verify that filtering the
residual through the LPC filter reconstructs the original.

**55.8** Use LPC to whisper: replace the residual with noise. Then use it for a robot voice with
a fixed-pitch buzz.

**55.9** Cross-synthesise: use a voice's LPC filter with a guitar recording as the excitation.

**55.10** Build the monster voice: pitch down 8 semitones, formants down 12 semitones, add a sub
layer, saturate. Then build the "alien" version with formants shifted *up* instead. Which is more
unsettling, and why?

**55.11** Build a crowd from one voice with 20 layers. Render it once with pitch and timing
randomisation only, then with formant randomisation added. Compare.

---

### Chapter summary

- The **source-filter model**: a source (buzz at pitch `f0`, or noise) through the vocal tract's
  resonances (**formants**). The source carries pitch and voicing; **the filter carries which
  sound it is**, and does not move when pitch does.
- **F1 and F2 alone identify a vowel.** Two band-pass filters on a buzz produce recognisable
  speech.
- A **channel vocoder** splits both signals into the same bands, measures the modulator's
  envelope per band, and applies it to the carrier. Needs **16+ bands**, a **harmonically rich
  carrier**, and **3–15 ms** envelope times.
- **The sibilance bypass is essential.** A harmonic carrier cannot produce the noise of `s`, `f`
  and `sh`, so consonants must be added directly from a high-passed modulator. Without it,
  nothing is intelligible.
- **LPC** fits an all-pole vocal-tract model (Levinson-Durbin), giving the **filter** and the
  **residual** separately — which can then be recombined arbitrarily for whispers, robots,
  cross-synthesis, and formant-independent pitch shifting.
- **Interpolate formants in log frequency**, matching perception.
- The most reliable cinematic voice technique: **shift pitch and formants inconsistently**. The
  auditory system computes body size from formants and pitch from the fundamental; making them
  disagree produces something it cannot assign a size to, and that failure is experienced as
  wrongness.
- For crowds from one voice, **randomise the formants** as well as pitch and timing — otherwise
  you get one person multitracked rather than different people.

---

## Part IV is complete

You can now process sound: delay lines and every modulation effect built on them, four kinds of
reverb from cheap to state-of-the-art, compressors and limiters with true-peak safety, gates,
sidechaining, multiband and transient shaping, distortion with correct oversampling, equalisation
including dynamic and match EQ, pitch shifting and time stretching by four methods, and the full
voice-processing toolkit.

**Part V makes it run in real time.**

**Next:** [Chapter 56 — What "Real-Time" Actually Means](56-what-realtime-means.md)
