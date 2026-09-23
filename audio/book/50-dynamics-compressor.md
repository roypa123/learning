# Chapter 50 — Dynamics I: Envelope Followers and Compressors

> A compressor turns a signal's own level into a control signal and uses it to change the gain.
> That is the whole mechanism. Everything difficult about compression is in the details of how
> you measure the level and how fast you react — and those details are the entire reason
> compressors have personalities.

---

## 50.1 The structure

```
   in ──┬──────────────────────────────► [ × ] ──► out
        │                                  ▲
        └──► DETECTOR ──► GAIN COMPUTER ──►│
             (level)      (how much to      gain
                           reduce)
```

Three stages:

1. **Detector** — measure the current level. Rectify, then smooth.
2. **Gain computer** — decide how much reduction that level calls for.
3. **Apply** — multiply.

The path from input to the detector is the **sidechain**. Everything interesting happens there.

---

## 50.2 The envelope follower

### Rectification

You cannot follow a signal that swings negative, so first take its magnitude.

| Method | Formula | Character |
|---|---|---|
| **Peak** | `\|x\|` | Catches every transient. Fast, aggressive |
| **RMS** | `√(mean(x²))` | Corresponds to loudness (Ch 11). Slower, smoother |
| **Hybrid** | Peak for attack, RMS for release | What many hardware units do |

**Peak vs RMS is the single biggest character difference between compressors.** A peak detector
responds to a drum transient; an RMS detector barely notices it. Neither is correct — they are
for different jobs.

### Smoothing: the attack/release filter

Raw rectified audio is a jagged mess at the signal's own frequency. Smooth it with the one-pole
filter you have been using since Chapter 13:

```cpp
class EnvelopeFollower
{
public:
    void setSampleRate(double sr) { sr_ = sr; update(); }
    void setAttackMs(double ms)   { attackMs_ = ms;  update(); }
    void setReleaseMs(double ms)  { releaseMs_ = ms; update(); }

    float process(float x)
    {
        const double rectified = std::fabs(static_cast<double>(x));

        // ASYMMETRIC: different coefficients depending on direction.
        // This is what makes it an envelope follower rather than a low-pass.
        const double coeff = (rectified > env_) ? attackCoeff_ : releaseCoeff_;

        env_ = rectified + coeff * (env_ - rectified);

        return static_cast<float>(env_);
    }

private:
    void update()
    {
        attackCoeff_  = std::exp(-1.0 / (attackMs_  * 0.001 * sr_));
        releaseCoeff_ = std::exp(-1.0 / (releaseMs_ * 0.001 * sr_));
    }

    double sr_ = kDefaultRate, env_ = 0.0;
    double attackMs_ = 10.0, releaseMs_ = 100.0;
    double attackCoeff_ = 0.0, releaseCoeff_ = 0.0;
};
```

**The asymmetry is the whole idea.** Rising signal → use the fast attack coefficient. Falling
signal → use the slow release coefficient. A symmetric filter is just a low-pass and would follow
the signal's own waveform rather than its envelope.

### Detecting in the level domain or the gain domain

Two architectures, and they sound different:

**Feed-forward** (modern, most digital compressors): detect the *input*, compute the gain, apply
it. Predictable, precise, easy to analyse.

**Feed-back** (most classic hardware — 1176, LA-2A, Fairchild): detect the *output*, feed the
gain back. The reduction affects what is being measured, so the behaviour is self-regulating and
harder to characterise. It has a softer, more "musical" knee that emerges from the loop rather
than being designed.

```cpp
// Feed-forward
gain = computeGain(detector.process(input));
output = input * gain;

// Feed-back
gain = computeGain(detector.process(lastOutput));
output = input * gain;
lastOutput = output;
```

**Two lines' difference, and it is a large part of why vintage compressors sound the way they
do.** Feed-back compressors cannot have a true hard knee, cannot achieve infinite ratio, and
overshoot on fast transients — all of which are "flaws" that people specifically seek out.

---

## 50.3 The gain computer

Work in **decibels**, because the controls are in dB and because Chapter 11 established that
perception is logarithmic.

```cpp
float computeGainDb(float levelDb)
{
    const float over = levelDb - thresholdDb_;

    if (over <= -kneeDb_ * 0.5f)
        return 0.0f;                                   // below the knee: no reduction

    if (over >= kneeDb_ * 0.5f)
        return over * (1.0f / ratio_ - 1.0f);          // above: full ratio

    // Inside the knee: interpolate quadratically for a smooth transition.
    const float k = over + kneeDb_ * 0.5f;
    return (1.0f / ratio_ - 1.0f) * k * k / (2.0f * kneeDb_);
}
```

### The parameters

| Parameter | Range | What it does |
|---|---|---|
| **Threshold** | −60 to 0 dB | Level above which reduction begins |
| **Ratio** | 1:1 to ∞:1 | Input dB per output dB above threshold |
| **Knee** | 0 to 24 dB | Width of the transition into compression |
| **Attack** | 0.1 to 300 ms | How fast reduction engages |
| **Release** | 10 to 3000 ms | How fast it lets go |
| **Makeup gain** | 0 to +30 dB | Compensates for the level lost |

**Ratio, concretely.** With a threshold of −20 dB and a 4:1 ratio, an input at −8 dB (12 dB over)
produces an output at −17 dB (3 dB over). Twelve in, three out.

| Ratio | Name | Use |
|---|---|---|
| 1.5:1 – 2:1 | Gentle | Mix bus, mastering, "glue" |
| 3:1 – 4:1 | Moderate | Vocals, general levelling |
| 6:1 – 10:1 | Heavy | Drums, aggressive control |
| 20:1+ | **Limiting** | Peak control (Chapter 51) |

**Knee.** A hard knee (0 dB) switches abruptly at the threshold — precise, audible on material
that hovers around it. A soft knee (6–12 dB) eases in — much more transparent, and what you want
on vocals and mix buses.

---

## 50.4 Attack and release: where the sound is

These two parameters determine more of a compressor's character than anything else.

### Attack

**Fast attack (< 1 ms):** catches the transient. Drums lose their click; the compressor grabs the
initial hit.

**Slow attack (10–50 ms):** the transient passes through *before* reduction engages. The result
is a drum with a *more* prominent attack relative to its body — the compressor reduces the
sustain but not the hit.

> **This is the single most counter-intuitive fact in compression: a slow attack makes drums
> sound punchier, not softer.** Because the peak escapes and the body is reduced, the crest
> factor within each hit actually increases.

### Release

**Fast release (< 50 ms):** the gain recovers quickly, so quiet parts come back up fast. Dense,
loud, and at extremes produces audible "pumping" and — with bass content — distortion, because
the gain is changing within a single cycle of a low-frequency wave.

**Slow release (200 ms+):** smooth, transparent, but can hold the gain down after a transient and
duck the material behind it.

**Auto-release** adapts: fast for short transients, slow for sustained material. Usually
implemented as two release envelopes in parallel, taking whichever gives less reduction:

```cpp
// Two time constants. The fast one handles transients, the slow one
// handles programme material, and taking the max avoids pumping.
env1_ = std::max(rectified, env1_ * fastRelease_);
env2_ = std::max(rectified, env2_ * slowRelease_);
const double env = std::max(env1_ * 0.5, env2_);
```

This is what "programme-dependent release" means on classic units, and it is why they are so
forgiving.

### The low-frequency problem

A 40 Hz wave has a 25 ms period. A compressor with a 10 ms release will change its gain
*within* each cycle, which modulates the waveform's shape — that is distortion, and it sounds
like a gritty buzz on bass.

**Three fixes:**

1. **Slow release** on bass-heavy material — at least one period of the lowest frequency you care
   about.
2. **High-pass the sidechain** so the detector does not see the bass at all. Very common: a
   100 Hz high-pass in the sidechain lets a kick drum through without the compressor reacting to
   it.
3. **Multiband** (Chapter 51), so the bass gets its own time constants.

---

## 50.5 The complete compressor

```cpp
class Compressor : public Processor
{
public:
    void prepare(double sr, int) override
    {
        sr_ = sr;
        detector_.setSampleRate(sr);
        sidechainHP_.setSampleRate(sr);
        sidechainHP_.setFilter(FilterType::HighPass, sidechainHpHz_, 0.707);
        update();
    }

    void process(AudioBuffer& buffer) override
    {
        const int channels = buffer.numChannels();
        const int frames   = buffer.numFrames();

        for (int i = 0; i < frames; ++i)
        {
            // --- detect: LINKED across channels -----------------------
            // Using the max of all channels keeps the stereo image stable.
            // Detecting per-channel makes the image wander under compression.
            float detect = 0.0f;
            for (int c = 0; c < channels; ++c)
                detect = std::max(detect, std::fabs(buffer.channel(c)[i]));

            if (sidechainHpHz_ > 20.0)
                detect = std::fabs(sidechainHP_.processSample(detect, 0));

            const float env = detector_.process(detect);

            // --- gain computer, in dB ---------------------------------
            const float levelDb   = static_cast<float>(gainToDb(env));
            const float reduceDb  = computeGainDb(levelDb);
            const float gain      = static_cast<float>(dbToGain(reduceDb + makeupDb_));

            gainReductionDb_ = reduceDb;      // for metering

            // --- apply ------------------------------------------------
            for (int c = 0; c < channels; ++c)
                buffer.channel(c)[i] *= gain;
        }
    }

    void reset() override { detector_ = EnvelopeFollower{}; }
    const char* name() const override { return "Compressor"; }

    float gainReductionDb() const { return gainReductionDb_; }

private:
    EnvelopeFollower detector_;
    Biquad sidechainHP_;
    double sr_ = kDefaultRate;
    float  thresholdDb_ = -20.0f, ratio_ = 4.0f, kneeDb_ = 6.0f, makeupDb_ = 0.0f;
    double sidechainHpHz_ = 0.0;
    float  gainReductionDb_ = 0.0f;
};
```

**Stereo linking is not optional.** Detecting each channel independently means a loud sound on
the left reduces only the left channel, which pulls the whole stereo image to the right for as
long as the reduction lasts. Using the maximum across channels (or the sum, or the mid channel)
keeps the image stable.

**The exception:** occasionally you *want* unlinked behaviour, for wide creative effects. It
should be a switch, defaulting to linked.

---

## 50.6 Using a compressor

Practical guidance, since the parameters interact confusingly.

**Setting order that works:**

1. Ratio to something moderate (4:1).
2. Attack and release to medium (10 ms / 100 ms).
3. **Lower the threshold until you see 3–6 dB of reduction on the loudest parts.**
4. Now adjust attack: faster to control transients, slower to let them through.
5. Adjust release: listen for pumping, back off if you hear it.
6. Makeup gain to match the bypassed level — **this is essential for honest comparison**, because
   louder always sounds better (Chapter 11).

**How much reduction:**

| Reduction | Effect |
|---|---|
| 1–3 dB | Transparent levelling |
| 3–6 dB | Audible control, still natural |
| 6–12 dB | Obvious compression, a "sound" |
| 12–20 dB | Heavy, effect-like |
| 20 dB+ | Extreme; usually parallel |

**Parallel compression** (New York compression): mix a heavily compressed copy *under* the
uncompressed original. The quiet details come up without the peaks being squashed, so you get
density without losing dynamics. Extremely useful on drums and on anything that needs to feel
both controlled and alive — which in cinematic work is most things.

```cpp
out = dry + compressed * parallelAmount;
```

**Note the phase requirement:** the compressor must not introduce latency, or the parallel path
comb-filters against the dry (Chapter 2). Lookahead limiters (Chapter 51) definitely do introduce
latency and must be compensated.

---

## 50.7 Compression in cinematic sound

A brief but important note, because film practice differs sharply from music practice.

**Film mixes use far less compression than music mixes.** Chapter 11's crest-factor table: a film
mix runs 25–35 dB crest factor against a pop master's 6–9 dB. The dynamic range *is* the
storytelling — a whisper must be quiet so that the explosion can be enormous.

**Where compression is used in film:**

- **Dialogue** — heavily, because intelligibility is non-negotiable and speech has a wide natural
  range. 4:1 to 8:1 with a soft knee is common.
- **Ducking** — music and effects under dialogue, usually via a sidechain (Chapter 51).
- **Individual elements** to control them before they reach the mix, not on the mix bus.
- **Loudness compliance** at the final stage (Chapter 89), which is about meeting a spec rather
  than making it sound bigger.

**Where it is avoided:** the mix bus. A film mix bus compressor would flatten exactly the contrast
the mix is built on.

> **The cinematic principle:** make things big by **contrast**, not by **level**. Compressing
> everything removes the contrast and makes the loud things smaller.

---

## 50.8 Exercises

**50.1** Build the envelope follower. Feed it a 100 Hz sine and plot the envelope with attack and
release both at 1 ms, then both at 100 ms. Which follows the waveform rather than the envelope?

**50.2** Verify the gain computer: with a threshold of −20 dB and 4:1, confirm that an input at
−8 dB gives an output at −17 dB.

**50.3** Compare hard knee (0 dB) and soft knee (12 dB) on material that hovers around the
threshold. Which is more audible?

**50.4** *The punch experiment.* Compress a drum loop at 6:1 with attack 0.1 ms, then 30 ms.
Measure the crest factor of each. Which is punchier, and does the measurement agree with your
ears?

**50.5** Compress a bass line with a 10 ms release. Look at the waveform. Can you see the gain
changing within each cycle? Now use a 200 ms release.

**50.6** Add a sidechain high-pass at 100 Hz and compress a full mix. Compare with and without.

**50.7** *Deliberate breakage.* Detect each channel independently on a stereo mix with a loud
left-panned element. Watch the image move.

**50.8** Implement feed-back detection and compare with feed-forward at identical settings on
drums.

**50.9** Implement auto-release with two time constants. Compare with fixed release on material
that alternates between transients and sustain.

**50.10** Build parallel compression: 20:1 with a fast attack, mixed under the dry at various
amounts. Compare with serial compression achieving the same peak reduction.

---

### Chapter summary

- A compressor is **detector → gain computer → multiply**. The detector path is the **sidechain**,
  and that is where the character lives.
- **Peak vs RMS detection** is the biggest single difference between compressors: peak catches
  transients, RMS follows loudness.
- The envelope follower is the one-pole filter with **asymmetric coefficients** — fast rising,
  slow falling. Symmetric would just be a low-pass.
- **Feed-forward** (detect the input) is predictable; **feed-back** (detect the output) is
  self-regulating with a naturally soft knee — a two-line difference that explains much of why
  vintage compressors sound as they do.
- Work the gain computer **in dB**. Soft knee interpolates quadratically through the threshold.
- **A slow attack makes drums punchier**, because the transient escapes and the body is reduced.
- **Fast release on bass causes distortion** — the gain changes within a cycle. Fix with a slower
  release, a **sidechain high-pass**, or multiband.
- **Link the detection across channels**, or compression pulls the stereo image around.
- Always **match makeup gain** before judging, because louder sounds better.
- **Parallel compression** gives density without losing peaks — and requires a latency-free
  compressor or the paths comb-filter.
- **Film mixes use far less compression than music mixes.** Dialogue yes, ducking yes, mix bus
  no. Make things big by **contrast**, not by level.

**Next:** [Chapter 51 — Dynamics II: Limiters, Gates, Sidechains, Multiband](51-dynamics-advanced.md)
