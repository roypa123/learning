# Chapter 37 — Granular Synthesis

> Chop sound into fragments so short they have no identity, then rebuild reality out of the
> cloud. Granular synthesis is the technique behind time-stretching, pitch-shifting, texture
> design, and a very large fraction of modern cinematic sound.

---

## 37.1 The grain

A **grain** is a very short piece of audio — typically **1 to 100 milliseconds** — with an
amplitude envelope applied.

```
   Source:   [====================================================]
                    ↑
             grain window (20 ms)
                 .-'''''-.
                /         \
   Grain:      /~~~~~~~~~~~\        the source, windowed
```

The envelope is not optional. A raw fragment starts and ends abruptly, and Chapter 13 told you
what that is: a click. With thousands of grains per second, unwindowed grains produce a buzzing
mess at the grain rate.

```cpp
struct Grain
{
    const std::vector<float>* source = nullptr;
    double position     = 0.0;     // read position in the source, in samples
    double increment    = 1.0;     // playback rate -> pitch
    size_t age          = 0;       // samples since the grain started
    size_t duration     = 0;       // total length in samples
    float  amplitude    = 1.0f;
    float  pan          = 0.5f;
    bool   active       = false;

    float nextSample()
    {
        if (!active || age >= duration) { active = false; return 0.0f; }

        const float s = interp::hermite(*source, position);
        const float w = window(static_cast<double>(age)
                             / static_cast<double>(duration));

        position += increment;
        ++age;

        return s * w * amplitude;
    }

    static float window(double t)      // t is 0..1 across the grain
    {
        return static_cast<float>(0.5 - 0.5 * std::cos(kTwoPi * t));   // Hann
    }
};
```

**The window shape matters and is a creative parameter:**

| Window | Character |
|---|---|
| **Hann** | Smooth, neutral. The default. |
| **Gaussian** | Very smooth; minimal spectral splatter. Good for dense clouds. |
| **Triangle** | Slightly harder; cheaper. |
| **Trapezoid** (fast attack, sustain, fast release) | Preserves more of the source's transient — good for rhythmic material |
| **Percussive** (instant attack, exponential decay) | Each grain becomes a tiny event; good for texture |

---

## 37.2 The cloud

One grain is a click. Thousands of overlapping grains are a texture.

```cpp
class GranularEngine
{
public:
    void setSource(const std::vector<float>* src) { source_ = src; }

    void setGrainSize(double ms)      { grainMs_ = ms; }
    void setDensity(double perSecond) { density_ = perSecond; }
    void setPosition(double pos)      { position_ = pos; }      // 0..1 in the source
    void setPitch(double ratio)       { pitch_ = ratio; }
    void setSpray(double ms)          { sprayMs_ = ms; }        // position randomisation
    void setPitchJitter(double cents) { pitchJitter_ = cents; }
    void setPanSpread(double s)       { panSpread_ = s; }

    void processStereo(float& left, float& right)
    {
        // Schedule new grains according to the density.
        grainTimer_ -= 1.0;
        if (grainTimer_ <= 0.0)
        {
            spawnGrain();
            grainTimer_ += sr_ / std::max(density_, 0.1);
        }

        double l = 0.0, r = 0.0;

        for (auto& g : grains_)
        {
            if (!g.active) continue;
            const float s = g.nextSample();
            l += s * (1.0f - g.pan);
            r += s * g.pan;
        }

        // Normalise for the expected overlap, or density changes cause
        // large level jumps.
        const double overlap = std::max(1.0, density_ * grainMs_ * 0.001);
        const double norm    = 1.0 / std::sqrt(overlap);

        left  = static_cast<float>(l * norm);
        right = static_cast<float>(r * norm);
    }

private:
    void spawnGrain()
    {
        for (auto& g : grains_)
        {
            if (g.active) continue;

            const double sprayS = (rng_.nextFloat() * sprayMs_ * 0.001) * sr_;

            g.source    = source_;
            g.position  = position_ * static_cast<double>(source_->size()) + sprayS;
            g.position  = std::clamp(g.position, 0.0,
                                     static_cast<double>(source_->size() - 2));
            g.increment = pitch_ * centsToRatio(rng_.nextFloat() * pitchJitter_);
            g.duration  = static_cast<size_t>(grainMs_ * 0.001 * sr_);
            g.age       = 0;
            g.amplitude = 1.0f;
            g.pan       = 0.5f + static_cast<float>(rng_.nextFloat() * panSpread_ * 0.5);
            g.active    = true;
            return;
        }
        // No free grain: silently drop it. Better than stealing one mid-grain.
    }

    std::array<Grain, 256> grains_{};
    const std::vector<float>* source_ = nullptr;
    FastRandom rng_{ 31337 };

    double sr_ = kDefaultRate;
    double grainMs_ = 50.0, density_ = 20.0, position_ = 0.0;
    double pitch_ = 1.0, sprayMs_ = 0.0, pitchJitter_ = 0.0, panSpread_ = 0.0;
    double grainTimer_ = 0.0;
};
```

### The parameters and what they do perceptually

| Parameter | Range | Effect |
|---|---|---|
| **Grain size** | 1–500 ms | Below ~20 ms the grain has no pitch of its own and you hear texture; above ~50 ms you hear recognisable fragments |
| **Density** | 1–1000 /s | Below ~20/s you hear individual events; above, a continuous cloud |
| **Position** | 0–1 | Where in the source to read. **Freezing it is the "spectral freeze" effect** |
| **Pitch** | 0.25–4× | Playback rate of each grain — pitch *without* changing the cloud's timing |
| **Spray** | 0–500 ms | Randomises position. Small = smoothing; large = scattering |
| **Pitch jitter** | 0–1200 cents | Randomises each grain's pitch. Small = chorus; large = clouds |
| **Pan spread** | 0–1 | Randomises each grain's position in the stereo field |

**The decisive insight**: grain playback rate (pitch) and grain scheduling rate (time) are
**completely independent**. That decoupling is why granular synthesis is *the* technique for
time-stretching and pitch-shifting:

- **Time stretch:** advance `position` more slowly than real time; grains still play at rate 1.0.
- **Pitch shift:** set `pitch` to the desired ratio; advance `position` at real time.
- **Both:** do both independently.

Nothing else gives you that separation so directly.

---

## 37.3 The density/size trade-off

There is a relationship between grain size, density and character that is worth internalising.

**Overlap** = `density × grainSize`. Some regimes:

| Overlap | Sound |
|---|---|
| **< 1** | Gaps between grains. Sparse, pointillist, rhythmic. Individual events. |
| **1–2** | Just continuous. Slightly gritty, characteristically "granular" |
| **4–8** | Smooth and dense. The usual range for time-stretching |
| **> 16** | Very smooth, but expensive and prone to comb filtering if grains are regular |

**Regular grain timing causes comb filtering.** If grains are spawned at exactly regular
intervals from exactly the same position, the overlapping copies are correlated and you get a
pitched resonance at the grain rate — a metallic ringing that is the classic amateur-granular
artefact.

**The fix is randomisation.** Randomise the spawn timing by ±20% and add a few milliseconds of
spray, and the correlation disappears. This is Chapter 15's rule again:

> **Nature is irregular. Regularity is the giveaway.**

---

## 37.4 What granular is used for

### Time stretching without pitch change

The workhorse application. Advance the read position at a fraction of real time while grains play
at normal rate.

**It works well for 0.5× to 2×.** Beyond that, artefacts appear: at extreme stretches you hear
the same fragment repeating, which sounds like stuttering or a "frozen" quality. The mitigation is
more spray and more pitch jitter, which trades faithfulness for smoothness.

For musical material with clear transients, granular stretching smears the transients — a drum
hit becomes several small hits. **Transient-aware** stretchers detect onsets (Chapter 67) and
pass them through unstretched, stretching only the sustain portions. That is how commercial
algorithms handle percussive material.

### Spectral freeze

Stop advancing the position. Grains keep reading the same region forever, and the sound sustains
indefinitely — a snapshot of a moment, held.

**This is one of the most recognisable cinematic sounds there is.** A word held into an infinite
drone; an orchestral chord frozen under dialogue; a scream becoming a texture. Chapter 84 uses
it, and it is four lines of code plus randomisation to keep it from ringing.

### Texture and cloud design

Very short grains (5–15 ms) at high density from a complex source, with heavy spray and pitch
jitter, produce textures that sound organic and unplaceable. This is the standard route to:

- Insect swarms, crowd murmurs, wind through structures
- Alien and creature ambiences
- "Something is wrong" beds under horror scenes
- The shimmer layer on top of a cinematic impact

### Pitch shifting

Change the grain playback rate. Unlike a simple resample, the duration is unaffected, because the
grains are scheduled independently.

Quality is moderate — granular pitch shifting has a characteristic warble, especially on
sustained tones — which is why Chapter 54's phase vocoder is preferred for clean musical shifts.
For sound design, the warble is often desirable.

---

## 37.5 Grain scheduling strategies

Three approaches, each giving a distinct character:

**Synchronous.** Grains at exactly regular intervals. Produces a pitched artefact at the grain
rate (see §37.3), which can be exploited: set the grain rate to a musical frequency and it becomes
a pitched tone whose timbre is determined by the source. This is **granular formant synthesis**,
and it is how convincing synthetic vowels can be made.

**Asynchronous.** Random intervals, typically Poisson-distributed. Smooth, natural, no comb
filtering. The default for texture work.

**Quasi-synchronous.** Regular with jitter — the best of both. A base rate that gives a sense of
pulse, with enough randomness to avoid ringing. Most musical granular effects use this.

```cpp
// Poisson scheduling: exponentially distributed inter-arrival times.
double nextGrainDelay(double meanRate, FastRandom& rng)
{
    const double u = 0.5 * (rng.nextFloat() + 1.0);     // 0..1, avoid 0
    return -std::log(std::max(u, 1e-9)) / meanRate;
}
```

Poisson arrival is what genuinely random events look like — raindrops, Geiger clicks, popcorn.
Using it rather than uniform randomisation makes granular textures noticeably more natural, and
it is the same reasoning as Chapter 15's fire crackles.

---

## 37.6 Practical notes

**Grain pool size.** 128–512 grains is typical. Compute the maximum needed:
`maxGrains = density × grainSize`. At 200 grains/s and 100 ms grains, that is 20 concurrent
— but transients in density can spike it, so allocate generously.

**Never steal an active grain.** Cutting a grain mid-envelope is a click. If the pool is
exhausted, drop the new grain — one missing grain out of hundreds is inaudible; one truncated
grain is a click.

**Normalise for overlap.** Doubling the density doubles the level, which makes density
unusable as a performance control. Dividing by `√overlap` (uncorrelated summing, Chapter 11)
keeps the level roughly constant.

**Interpolation quality matters.** Grains read at fractional positions constantly, so Chapter 28
applies directly. Linear is audibly gritty on bright sources; **Hermite is the right default**.

**Allocation.** Pre-allocate the grain pool. Spawning a grain must not allocate — Chapter 59.

---

## 37.7 Exercises

**37.1** Build the granular engine. Load a speech recording and render it at grain sizes of 2, 10,
50 and 200 ms with density 30/s. At what size do words become recognisable?

**37.2** *Deliberate breakage.* Remove the grain window. What do you hear, and at what frequency?

**37.3** Implement spectral freeze: stop advancing the position. Add spray from 0 to 50 ms and
listen to the metallic ringing disappear.

**37.4** Compare synchronous, asynchronous (Poisson) and quasi-synchronous scheduling at the same
density. Which sounds most natural? Which has audible pitch?

**37.5** Time-stretch a drum loop 4×. Then implement simple transient detection (energy rise
between frames) and pass transients through unstretched. Compare.

**37.6** Build a texture: 8 ms grains, 400/s density, 200 ms spray, ±600 cents pitch jitter, full
pan spread, from a recording of metal being struck. This is a Chapter 84 building block.

**37.7** Implement granular formant synthesis: synchronous grains at a musical rate (110 Hz) from
a short source. Sweep the grain rate and listen to the pitch change while the timbre stays.

**37.8** Compare grain interpolation: linear vs Hermite vs sinc on a bright source at 0.5× pitch.
Measure the noise floor of each.

**37.9** Verify the overlap normalisation: measure the RMS at densities of 10, 50, 200 and 500 per
second with 50 ms grains, with and without the `1/√overlap` factor.

---

### Chapter summary

- A **grain** is 1–100 ms of source audio with an amplitude envelope. **The envelope is
  mandatory** — unwindowed grains buzz at the grain rate.
- A **cloud** of overlapping grains becomes a texture. Key parameters: grain size, density,
  position, pitch, spray, pitch jitter, pan spread.
- **Grain playback rate and grain scheduling rate are independent.** That decoupling is why
  granular is *the* technique for time stretching and pitch shifting.
- **Overlap = density × grain size.** Below 1 is pointillist; 4–8 is the smooth working range.
- **Regular grain timing causes comb filtering** — a metallic ring at the grain rate. Randomise
  timing and add spray. Nature is irregular.
- **Poisson-distributed** scheduling sounds more natural than uniform randomisation, for the same
  reason fire crackles do.
- Applications: **time stretch** (good 0.5×–2×, needs transient handling beyond), **spectral
  freeze** (a cinematic staple), **texture/cloud design**, and pitch shifting (warbly, which is
  often desirable for sound design).
- Practical: pre-allocate the pool, **never steal an active grain** (drop instead), normalise by
  `1/√overlap`, and use **Hermite interpolation**.

**Next:** [Chapter 38 — Karplus-Strong and Digital Waveguides](38-karplus-strong-waveguides.md)
