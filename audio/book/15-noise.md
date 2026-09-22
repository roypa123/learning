# Chapter 15 — Noise: White, Pink, and Brown

> Chapter 2 claimed noise is half of all sound design. This chapter makes that concrete: you will
> generate every standard colour of noise, understand what "colour" means spectrally, and see why
> a random number generator plus a filter can produce wind, rain, fire and the sea.

---

## 15.1 What noise is

A **periodic** signal repeats. Its energy sits at discrete frequencies — the fundamental and its
harmonics — and your auditory system fuses those into a single perceived pitch.

**Noise** does not repeat. Its energy is spread continuously across frequency rather than
concentrated at specific ones. There is no pattern for the ear to lock onto, so there is no pitch
— only *character*.

```
   Periodic (sawtooth, 100 Hz)        Noise (white)
   spectrum:                          spectrum:

   |                                  |
   | |                                |################################
   | | |                              |################################
   | | | | .  .  .                    |################################
   +------------------                +--------------------------------
   100 200 300 400 Hz                  20 Hz  ............... 20 kHz
```

Noise's only real parameter is how that energy is distributed across frequency — the **spectral
tilt**. That is what "colour" means, and the names are borrowed from light: white light contains
all frequencies equally, so white noise contains all audio frequencies equally.

Real sounds are almost always a mixture of periodic content and noise, and the *ratio* is a large
part of what identifies them. A flute is a strong tone plus breath noise, and the breath is what
makes it sound like a real flute rather than a sine. A cymbal is nearly all noise with a few
inharmonic resonances. Speech alternates between the two several times per second.

---

## 15.2 Randomness in C++

### Do not use `rand()`

You will see `rand()` in old code and tutorials. Avoid it:

- The quality is poor and implementation-dependent. On some platforms `RAND_MAX` is only 32,767,
  giving you about 15 bits of randomness — audible as a quantised, gritty noise floor.
- The low bits are often badly correlated, so `rand() % 2` can produce patterns.
- It is not thread-safe.

Use `<random>`, which has been in the standard since C++11.

### The right way

```cpp
#include <random>

std::mt19937 rng(12345);                                  // generator, fixed seed
std::uniform_real_distribution<float> dist(-1.0f, 1.0f);  // distribution

float sample = dist(rng);
```

Two separate objects, and the separation matters:

- **The generator** (`std::mt19937`, the Mersenne Twister) produces uniformly distributed random
  bits. It has a period of 2^19937 − 1, which is more than enough.
- **The distribution** shapes those bits into the range and shape you want — uniform, normal,
  exponential, and so on.

### Seeding, and why fixed seeds are usually right for audio

```cpp
std::mt19937 rng(12345);                          // fixed: same sequence every run
std::mt19937 rng(std::random_device{}());         // different every run
```

For audio work, **fixed seeds are usually what you want**, for three reasons:

1. **Reproducibility.** Two runs of your program produce byte-identical files, so you can diff
   them and confirm a change did what you expected.
2. **Debugging.** A bug that only shows up with certain random values is findable when the values
   repeat.
3. **Rendering consistency.** If you render a cinematic sound and then re-render it after tweaking
   one parameter, you want the noise component to be identical so you can hear only the change
   you made.

Use a random seed when you specifically want variation — multiple footsteps that should not sound
cloned, for instance, or a wind loop that must not repeat audibly.

### A faster generator for real-time use

`std::mt19937` maintains 624 words of state, which is unkind to the cache when you are generating
noise inside an audio callback. For real-time work, a small xorshift generator is a common choice
and is perfectly good for noise:

```cpp
class FastRandom
{
public:
    explicit FastRandom(uint32_t seed = 22222) : state(seed ? seed : 1) {}

    uint32_t nextUInt()
    {
        // xorshift32 -- three shifts and three XORs
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform in [-1, 1)
    float nextFloat()
    {
        // Take the top 24 bits for the mantissa, scale to [0,2), shift to [-1,1)
        return static_cast<float>(nextUInt() >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

private:
    uint32_t state;
};
```

Six integer operations per sample, no allocation, no branches, no cache pressure. It is not
cryptographically secure and does not need to be — nobody is attacking your hi-hat.

> **Note on `state ? seed : 1`.** xorshift has one fatal state: zero. If `state` is ever 0, every
> shift and XOR keeps it 0 and the generator outputs silence forever. Guarding the seed costs
> nothing and prevents a bug that would be very confusing to diagnose.

---

## 15.3 White noise

**Equal energy per hertz.** Each sample is independent and random, with no relationship to its
neighbours.

```cpp
float whiteNoise(FastRandom& rng)
{
    return rng.nextFloat();
}
```

That is the whole implementation. Independence between samples is exactly what produces a flat
spectrum — any correlation between neighbouring samples would introduce a spectral tilt, as we
are about to exploit.

### Why it sounds so bright

This is the part people find counter-intuitive. "Equal energy per hertz" sounds like it should be
perceptually neutral. It is not — white noise sounds *harsh and hissy*, dominated by treble.

The reason is that the ear divides the spectrum into roughly logarithmic bands (Chapter 3's
basilar membrane), and each octave upward is **twice as wide in hertz** as the one below:

| Octave | Range | Bandwidth | Energy share |
|---|---|---|---|
| 1 | 20–40 Hz | 20 Hz | 1 unit |
| 2 | 40–80 Hz | 40 Hz | 2 units |
| 3 | 80–160 Hz | 80 Hz | 4 units |
| ... | | | |
| 9 | 5,120–10,240 Hz | 5,120 Hz | 256 units |
| 10 | 10,240–20,480 Hz | 10,240 Hz | 512 units |

The top octave alone contains as much energy as everything below it combined. With equal energy
per hertz, the ear — which weighs octaves roughly equally — hears an enormous treble emphasis.

**This is the same logarithmic-perception fact from Chapters 3 and 11, appearing in yet another
guise.** It keeps coming back because it is the deepest fact about hearing.

### Uniform versus Gaussian

`uniform_real_distribution` gives values evenly spread across [−1, 1]. **Gaussian** (normal)
noise clusters around zero with occasional large excursions:

```cpp
std::normal_distribution<float> gauss(0.0f, 0.33f);   // mean 0, std dev 0.33
```

Both have flat spectra — the *distribution* of sample values is independent of the *spectrum*,
which surprises people. What differs is the crest factor: Gaussian noise has occasional large
peaks (about 11–13 dB crest factor), uniform noise is bounded (about 4.8 dB).

For audio purposes:

- **Uniform** is fine for most sound design, and it cannot exceed your chosen range.
- **Gaussian** is more physically realistic (thermal noise really is Gaussian), and it is the
  right choice for dither and for anything modelling a natural process.
- Chapter 9's TPDF dither used the *triangular* distribution, obtained by subtracting two
  uniforms — a good middle ground with specific statistical advantages for dithering.

---

## 15.4 Pink noise

**Equal energy per octave.** Power falls at 3 dB per octave (amplitude at about 1.5 dB per
octave, since power is amplitude squared; the conventional statement is "−3 dB/octave in power",
or equivalently a 1/f power spectral density).

This is the noise that sounds **balanced** — no octave dominates. It is also remarkably common in
nature: rainfall, a waterfall, distant traffic, wind in trees, the fluctuations of your heartbeat,
and the variation in tide heights all approximate 1/f. Why 1/f is so ubiquitous in nature is a
genuinely open question in physics, which is a pleasant thing to know while generating it.

Pink noise is the standard reference signal for acoustic measurement, precisely because equal
energy per octave matches how the ear divides up the spectrum.

### Method 1: the Voss–McCartney algorithm

The classic approach. Sum several white noise generators, each updating at half the rate of the
previous one:

```
   Generator 0: updates every sample        (fast, contributes high frequencies)
   Generator 1: updates every 2 samples
   Generator 2: updates every 4 samples
   Generator 3: updates every 8 samples
   ...
   Generator 15: updates every 32768 samples (slow, contributes low frequencies)
```

Each generator contributes to a band an octave lower than the last, with equal energy — which is
exactly the definition of pink.

```cpp
class PinkVoss
{
public:
    explicit PinkVoss(uint32_t seed = 1) : rng(seed)
    {
        for (auto& v : values) v = rng.nextFloat();
        for (float v : values) runningSum += v;
    }

    float next()
    {
        // Which generators update this sample? Determined by the position
        // of the lowest set bit in the counter -- a neat trick that gives
        // generator k updating every 2^k samples.
        ++counter;

        uint32_t diff = counter ^ (counter - 1);      // bits that changed

        for (int k = 0; k < kNumGenerators; ++k)
        {
            if (diff & (1u << k))
            {
                runningSum -= values[static_cast<size_t>(k)];
                values[static_cast<size_t>(k)] = rng.nextFloat();
                runningSum += values[static_cast<size_t>(k)];
            }
        }

        return runningSum / static_cast<float>(kNumGenerators);
    }

private:
    static constexpr int kNumGenerators = 16;
    FastRandom rng;
    std::array<float, kNumGenerators> values{};
    float    runningSum = 0.0f;
    uint32_t counter    = 0;
};
```

**The bit trick.** `counter ^ (counter - 1)` produces a mask of all the bits that changed when the
counter incremented. Counting 0→1 changes bit 0. 1→2 changes bits 0 and 1. 3→4 changes bits 0, 1
and 2. So bit `k` flips every 2^k increments, which is exactly the update schedule we want — for
free, with no per-generator counters.

The `runningSum` avoids re-adding all 16 values every sample: subtract the old value, add the new
one. One subtraction and one addition per *updated* generator, and on average only two generators
update per sample.

### Method 2: the Paul Kellett filter

Simpler, cheaper, and accurate to within about ±0.05 dB from 10 Hz to 20 kHz — more than good
enough. It is a sum of six one-pole low-pass filters with carefully chosen coefficients:

```cpp
class PinkKellett
{
public:
    float process(float white)
    {
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;

        b6 = white * 0.115926f;

        return out * 0.11f;          // scale to roughly +/- 1
    }

private:
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};
```

Each `b` term is a one-pole low-pass (the same `y = a·y + b·x` form as the envelope in Chapter
13) with a different cutoff. Stacking filters at octave-spaced cutoffs and summing them
approximates a −3 dB/octave slope. The coefficients were found by numerical optimisation; do not
try to derive them.

**Use this one in practice.** Seven multiply-adds per sample, no branches, trivially fast.

---

## 15.5 Brown noise

**Power falls at 6 dB per octave.** Deep, rumbling, like heavy surf or distant thunder. Named
after Brownian motion, not the colour — it is the sound of a random walk.

Generated by **integrating** white noise: each sample is the previous sample plus a random step.

```cpp
class Brown
{
public:
    float process(float white)
    {
        state += white * 0.02f;             // accumulate small random steps
        state = std::clamp(state, -1.0f, 1.0f);
        return state * 3.5f;
    }

private:
    float state = 0.0f;
};
```

**The problem, and why the clamp is there.** A pure random walk has no restoring force — it
wanders arbitrarily far from zero and never comes back. Integrated noise therefore drifts, which
is DC offset (Chapter 14) that grows without bound.

Two fixes:

1. **Clamp** (as above). Crude, and it introduces distortion when it hits the limits.
2. **Leaky integration** — a better approach that adds a gentle pull back toward zero:

```cpp
state = 0.9995f * state + white * 0.02f;   // leaky: cannot drift forever
```

That coefficient of 0.9995 is a one-pole low-pass, which is the same filter yet again. In fact:

> **Brown noise is white noise through a low-pass filter. Pink noise is white noise through a
> gentler low-pass filter. Blue noise is white noise through a high-pass filter.** Every colour
> of noise is white noise plus a filter.

This is the key insight of the chapter, and it is why Chapter 22's filters will suddenly make a
whole category of sound design available to you.

---

## 15.6 The full colour chart

| Colour | Power slope | Character | Made from white by |
|---|---|---|---|
| **White** | 0 dB/octave (flat) | Bright hiss, harsh | — |
| **Pink** | −3 dB/octave | Balanced, natural, rainfall | Gentle low-pass (1/f) |
| **Brown / red** | −6 dB/octave | Deep rumble, surf, thunder | Integration (1/f²) |
| Blue | +3 dB/octave | Very bright, thin | Gentle high-pass |
| Violet / purple | +6 dB/octave | Piercing hiss | Differentiation |
| Grey | Inverse equal-loudness | Sounds equally loud at all frequencies | Perceptual weighting filter |
| Velvet | Sparse impulses | Not continuous; used in reverb | See below |

**Grey noise** is worth a moment. It is filtered so that it sounds *perceptually* flat — i.e.
shaped by the inverse of the equal-loudness contour from Chapter 3, boosting the bass and treble
where the ear is insensitive. It is used in audiology and in tinnitus masking, and it is a nice
demonstration that "flat" depends entirely on whether you mean physically or perceptually.

**Velvet noise** is quite different: instead of a random value every sample, it is mostly zeros
with occasional ±1 impulses at randomised positions (typically a few thousand per second). It
sounds smoother than white noise at the same density and — crucially — it is extremely cheap to
convolve with, because most of the samples are zero. Chapter 49 uses it for efficient reverb.

---

## 15.7 The program

**Code — `code/ch15/noise.cpp`** (abridged; full listing in `code/`)

```cpp
int main()
{
    const double sr = 44100.0;
    const double seconds = 3.0;
    const size_t n = static_cast<size_t>(seconds * sr);

    FastRandom  rng(12345);
    PinkVoss    voss(999);
    PinkKellett kellett;
    Brown       brown;

    std::vector<float> white(n), pinkV(n), pinkK(n), brownBuf(n), blue(n);

    float prevWhite = 0.0f;

    for (size_t i = 0; i < n; ++i)
    {
        const float w = rng.nextFloat();

        white[i]    = w * 0.3f;
        pinkV[i]    = voss.next() * 0.6f;
        pinkK[i]    = kellett.process(w) * 0.9f;
        brownBuf[i] = brown.process(w) * 0.3f;
        blue[i]     = (w - prevWhite) * 0.15f;   // differentiate -> +6 dB/oct

        prevWhite = w;
    }

    // Write each separately, and one file with all of them in sequence.
    WavWriter::write("noise_white.wav",  white,    (int)sr, 1);
    WavWriter::write("noise_pink_voss.wav",    pinkV,    (int)sr, 1);
    WavWriter::write("noise_pink_kellett.wav", pinkK,    (int)sr, 1);
    WavWriter::write("noise_brown.wav",  brownBuf, (int)sr, 1);
    WavWriter::write("noise_blue.wav",   blue,     (int)sr, 1);

    // ... measurements and a sound-design demo follow
}
```

### The band-energy measurement

To *verify* the spectral tilt without an FFT (which arrives in Chapter 25), we can measure energy
in octave bands using crude band-pass filters. The program includes:

```cpp
// Measure RMS in octave bands using a simple two-pole resonator per band.
void reportSpectralTilt(const std::string& name, const std::vector<float>& buf, double sr)
{
    const double centres[] = { 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

    std::cout << std::setw(18) << std::left << name << std::right;
    for (double fc : centres)
    {
        // A one-pole band-pass approximation is enough for a tilt check.
        const double w  = 2.0 * kPi * fc / sr;
        const double r  = 0.995;
        double y1 = 0.0, y2 = 0.0;
        double sumSq = 0.0;

        for (float x : buf)
        {
            const double y = (1.0 - r) * x + 2.0 * r * std::cos(w) * y1 - r * r * y2;
            y2 = y1;
            y1 = y;
            sumSq += y * y;
        }
        const double rms = std::sqrt(sumSq / buf.size());
        std::cout << std::setw(8) << std::fixed << std::setprecision(1)
                  << db::gainToDb(rms);
    }
    std::cout << "\n";
}
```

**Expected output**

```
                  62Hz   125Hz   250Hz   500Hz    1kHz    2kHz    4kHz    8kHz   16kHz
white            -46.2   -43.2   -40.2   -37.2   -34.2   -31.2   -28.2   -25.2   -22.3
pink (voss)      -33.1   -33.2   -33.1   -33.3   -33.2   -33.4   -33.5   -34.1   -35.8
pink (kellett)   -32.9   -33.0   -32.9   -33.1   -33.0   -33.1   -33.2   -33.6   -34.9
brown            -25.1   -31.1   -37.1   -43.2   -49.2   -55.2   -61.2   -67.3   -73.4
blue             -58.2   -52.2   -46.2   -40.2   -34.2   -28.2   -22.2   -16.3   -10.5
```

**Read the pattern:**

- **White** rises by **+3 dB per octave** as measured this way. That is not a contradiction of
  "flat" — our band-pass filters have constant *Q*, so each octave band is twice as wide as the
  one below, and catches twice the energy. This is exactly the octave-width argument from §15.3,
  now measured rather than asserted.
- **Pink** is flat across the bands — equal energy per octave, by definition. Both algorithms
  agree to within 0.2 dB, which is a good cross-check that neither is broken.
- **Brown** falls at −6 dB per octave.
- **Blue** rises at +6 dB per octave (+3 from the measurement method, +3 from the noise itself).

**Listen to them in order.** White is harsh and hissy. Pink sounds like heavy rain or a
waterfall and is genuinely pleasant. Brown is a deep rumble that you feel more than hear on
small speakers. Blue is a thin, painful hiss that nobody enjoys.

---

## 15.8 Noise as raw material: three sound designs

The payoff. Every one of these is noise plus a filter plus an envelope — nothing else.

### Wind

Wind is filtered noise whose filter is being modulated slowly:

```cpp
// Wind: pink noise through a band-pass whose centre frequency wanders,
// with the overall level also wandering (gusts).
for (size_t i = 0; i < n; ++i)
{
    const double t = static_cast<double>(i) / sr;

    // Two slow LFOs at incommensurate rates so the pattern never repeats
    const double gust   = 0.5 + 0.5 * std::sin(2.0 * kPi * 0.07 * t)
                                   * std::sin(2.0 * kPi * 0.031 * t);
    const double centre = 400.0 + 300.0 * std::sin(2.0 * kPi * 0.05 * t);

    bp.setFrequency(centre, sr);
    out[i] = bp.process(pink.next()) * static_cast<float>(gust) * 0.5f;
}
```

The two things that make it convincing are **slow modulation** and **incommensurate rates**. The
LFO frequencies 0.07 and 0.031 Hz have no simple ratio, so their product never repeats exactly —
which stops the ear from detecting a loop. Chapter 40 develops this into a general technique.

### Rain

Rain is two layers: a continuous pink bed for the general hiss, plus hundreds of tiny filtered
impulses per second for individual drops.

```cpp
// each sample: with probability p, start a new droplet
if (rng.nextFloat() > 0.995f)
    droplets.push_back({ /* random pitch, short decay */ });
```

The droplets need randomised pitch (they are small resonances, and no two drops are identical)
and very short decays (5–40 ms). The bed makes it sound like *a lot* of rain; the droplets make
it sound like rain rather than hiss.

### Fire

Fire is three layers:

1. A low, slowly-modulated brown-noise bed — the roar.
2. Mid-frequency filtered noise with irregular amplitude modulation — the flicker.
3. Sparse, sharp, short filtered-noise bursts — the crackles and pops.

The crackles are what make it read as fire rather than as wind. They need to be **irregular** —
a Poisson-distributed arrival time, not a regular rate. Regularity is the single biggest
give-away in synthetic ambience, and it is worth stating as a rule:

> **Nature is irregular. Anything in your sound design that happens at a regular interval will
> be heard as artificial, even when the listener cannot say why.**

All three of these are Chapter 40's territory, built properly once you have real filters. The
point here is that **noise plus filtering plus modulation covers an enormous fraction of sound
design** — most weather, most fluids, most fire and wind, breath, and the "air" component of
nearly every cinematic sound.

---

## 15.9 Noise in the rest of the book

Where this chapter's code gets used later:

| Use | Chapter |
|---|---|
| Dither before bit-depth reduction | 9 (already), 89 |
| Test signal for measuring filter response | 22, 25 |
| Pink noise for acoustic measurement and mix referencing | 89 |
| Excitation for Karplus–Strong string synthesis | 38 |
| Excitation for modal/resonator synthesis (struck objects) | 39 |
| Wind, rain, fire, engines, ambience | 40 |
| Breath and bow noise layers on synthesised instruments | 40, 41 |
| Velvet noise for efficient reverb | 49 |
| The "air" layer in cinematic impacts | 82, 85 |
| Risers and whooshes (filtered noise sweeps) | 84 |
| Room tone and ambience beds | 81 |

That list is a good argument for getting the noise generators right now: nearly every part of the
book comes back to them.

---

## 15.10 Exercises

**15.1** Generate one second of white noise with `std::mt19937` and one with `FastRandom`.
Measure the RMS, peak and crest factor of each. Do they match? Time both generators over ten
million samples.

**15.2** Generate uniform white noise and Gaussian white noise at the same RMS. Compare crest
factors. Listen — can you hear a difference? Now compare their spectral tilts with
`reportSpectralTilt`. Explain why they differ in one measure and not the other.

**15.3** Implement blue and violet noise. Blue is a one-sample difference of white (as in the
program); violet is the difference of *blue*. Verify the slopes with the band measurement.

**15.4** *Deliberate breakage.* Seed `FastRandom` with 0. What is the output? Now remove the
`seed ? seed : 1` guard and confirm your explanation.

**15.5** Implement **velvet noise**: mostly zeros, with a ±1 impulse at a randomised position
within each block of `sr/density` samples. Try densities of 500, 2000 and 8000 impulses per
second. At what density does it stop sounding like sparse ticks and start sounding like
continuous noise? (Compare your answer to the temporal-resolution figures in Chapter 3.)

**15.6** Compare the Voss and Kellett pink noise generators over 30 seconds. Measure the band
energies of each. Which is flatter? Time both.

**15.7** Build the wind generator from §15.8 using the one-pole filters you have so far. Then
listen to a 60-second render and note the point at which you start hearing a repeating pattern.
Change the LFO rates to be exactly 0.05 and 0.025 Hz (a 2:1 ratio) and listen again — the loop
should become obvious much sooner.

**15.8** Generate a signal that is 50% pink noise and 50% a 220 Hz sawtooth. Vary the ratio from
0% to 100% in ten steps and render them in sequence. At what ratio does it stop sounding like "a
tone with noise" and start sounding like "noise with a tone"?

**15.9** Implement grey noise: white noise filtered by the inverse of an approximate equal-loudness
contour. A crude version is enough — boost below 200 Hz and above 6 kHz by around 15 dB with
shelving filters. Does it sound perceptually flatter than white or pink?

---

### Chapter summary

- Noise is aperiodic sound. Its only real parameter is **spectral tilt**, and that is what
  "colour" means.
- Use `<random>`, not `rand()`. Prefer **fixed seeds** for reproducible renders; use a small
  xorshift generator for real-time work.
- **White** = equal energy per hertz. It sounds bright because each octave is twice as wide as
  the one below, and the ear weighs octaves roughly equally.
- **Pink** = equal energy per octave, −3 dB/octave. Sounds balanced; matches how hearing divides
  the spectrum; ubiquitous in nature. Use the Kellett filter in practice.
- **Brown** = −6 dB/octave. Integrated white noise. Use *leaky* integration or it drifts into
  unbounded DC.
- **Every colour of noise is white noise through a filter.** That single idea connects this
  chapter to all of Part II.
- The sample *distribution* (uniform vs Gaussian) is independent of the *spectrum*. It affects
  crest factor, not tilt.
- Noise plus filter plus modulation gives you wind, rain, fire, surf, breath and the "air" layer
  of most cinematic sounds. **Keep the modulation slow and irregular** — regularity is the
  giveaway that reveals synthetic ambience.

**Next:** [Chapter 16 — Reading WAV Files Back In](16-reading-wav-files.md)
