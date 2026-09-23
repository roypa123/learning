# Chapter 40 — Procedural Sound Design: Wind, Rain, Fire, Engines

> Everything in Part III has been an instrument. This chapter uses the same tools to make the
> world: weather, fire, machines, footsteps, creatures. Procedural sound never repeats, responds
> to parameters in real time, and costs kilobytes instead of gigabytes — which is why games use
> it and why cinematic work increasingly does too.

---

## 40.1 Why generate rather than record

| | Recorded samples | Procedural |
|---|---|---|
| Realism | **Perfect** | Good to very good |
| Memory | Megabytes to gigabytes | **Kilobytes** |
| Variation | Limited to what was recorded | **Infinite** |
| Parametric control | None (or crossfade between takes) | **Continuous** |
| Repetition | Audible after a few plays | **Never** |
| Effort | Record, edit, loop, manage | Design once, tune forever |

The decisive column is **parametric control**. A recorded wind loop is one wind. A procedural
wind has a "strength" input that a game or a scene can drive continuously, and everything —
level, spectrum, gustiness — follows correctly.

**The honest limitation:** for a sound the audience will scrutinise closely and that has a
specific, familiar character — a particular car, a named animal, a human voice — recording wins.
Procedural excels at *backgrounds*, *systems*, and anything that must respond.

---

## 40.2 The universal recipe

Nearly every environmental sound follows the same pattern:

```
   NOISE  →  FILTER  →  MODULATION  →  LAYERS
```

1. **Noise** (Chapter 15) provides the raw material.
2. **Filters** (Chapters 22–23) carve it into the right spectral shape.
3. **Modulation** (Chapter 34) — slow, irregular, multi-rate — makes it alive.
4. **Layers** combine several of these at different scales.

And three rules that separate convincing from obviously fake:

> **Rule 1: Nothing is regular.** Any periodicity is detected instantly, even unconsciously.
>
> **Rule 2: Use incommensurate modulation rates.** Two LFOs at 0.07 and 0.031 Hz never repeat;
> two at 0.05 and 0.025 repeat every 40 seconds.
>
> **Rule 3: Layer at different time scales.** Something changing every 10 ms, something every
> second, something every 30 seconds. Real environments have structure at every scale.

---

## 40.3 Wind

Wind is filtered noise whose filter and level wander.

```cpp
class Wind
{
public:
    void prepare(double sr)
    {
        sr_ = sr;
        bandpass_.setSampleRate(sr);
        lowShelf_.setSampleRate(sr);
        for (auto& l : lfos_) l.setSampleRate(sr);

        // Incommensurate rates -- see Rule 2.
        lfos_[0].setRateHz(0.073);
        lfos_[1].setRateHz(0.031);
        lfos_[2].setRateHz(0.011);
        lfos_[3].setRateHz(0.211);
        for (auto& l : lfos_) l.setShape(LFO::Shape::SmoothRandom);
    }

    // strength: 0 = still, 1 = gale
    void setStrength(double s) { strength_ = std::clamp(s, 0.0, 1.0); }

    float nextSample()
    {
        const double slow   = lfos_[0].next();          // gusts
        const double slower = lfos_[1].next();          // weather trend
        const double drift  = lfos_[2].next();          // very slow character change
        const double fast   = lfos_[3].next();          // fine turbulence

        // Gust envelope: the product of several LFOs never repeats.
        const double gust = std::clamp(
            0.25 + 0.75 * (0.5 + 0.5 * slow) * (0.6 + 0.4 * slower), 0.0, 1.5);

        const double level = strength_ * gust;

        // Stronger wind is BRIGHTER as well as louder -- this is the cue
        // that sells it. Level alone reads as a volume fade.
        const double centre = 300.0 + 1400.0 * level + 200.0 * drift;
        const double q      = 0.7 + 1.5 * level;

        if (++counter_ >= 32)                            // coefficients at ~1.4 kHz
        {
            counter_ = 0;
            bandpass_.setFilter(FilterType::BandPass,
                                std::clamp(centre, 80.0, sr_ * 0.4), q);
            lowShelf_.setFilter(FilterType::LowShelf, 200.0, 0.7,
                                -6.0 + 10.0 * level);
        }

        float n = pink_.process(rng_.nextFloat());
        n += static_cast<float>(fast * 0.15);            // turbulence

        float out = bandpass_.processSample(n, 0);
        out = lowShelf_.processSample(out, 0);

        return static_cast<float>(out * level * 3.0);
    }

private:
    double sr_ = kDefaultRate, strength_ = 0.5;
    std::array<LFO, 4> lfos_;
    Biquad bandpass_, lowShelf_;
    PinkKellett pink_;
    FastRandom rng_{ 8191 };
    int counter_ = 0;
};
```

**The key insight is in the comment:** *stronger wind is brighter as well as louder.* If
"strength" only changes the level, it sounds like someone turning a knob. Coupling strength to
the filter's centre frequency and Q makes it sound like more air moving faster, which is what is
physically happening.

**Variants** are a matter of the filter:

| Wind type | Filter |
|---|---|
| Open field | Broad band-pass, 200–2000 Hz |
| Through trees | Add a second band around 3–6 kHz for leaf rustle |
| Through a gap or window | Narrow, high-Q band-pass — it whistles |
| Howling round a structure | Two or three narrow resonances at fixed frequencies |
| Inside a vehicle | Heavy low-pass plus low-frequency rumble |

Fixed narrow resonances are effective because a real structure *has* resonances — the gap, the
window frame, the canyon all have modes (Chapter 39). Adding two or three makes the wind sound
like it is interacting with *something specific*.

---

## 40.4 Rain

Rain is two layers with completely different characters.

**Layer 1: the bed.** Filtered pink noise, slowly modulated — the collective hiss of thousands of
drops too distant to resolve.

**Layer 2: the droplets.** Individual short events, each a resonator ping. This is what makes it
*rain* rather than hiss.

```cpp
void nextDroplet()
{
    // Poisson arrival -- Chapter 37's reasoning. Uniform randomness
    // sounds mechanical; exponential inter-arrival sounds like rain.
    const double u = 0.5 * (rng_.nextFloat() + 1.0);
    nextDropIn_ = -std::log(std::max(u, 1e-9)) / dropsPerSecond_;

    Drop& d = pool_[nextSlot_++ % pool_.size()];
    d.freq  = 400.0 + rng_.nextFloat() * 3000.0 + 3400.0;   // 400 .. 6800 Hz
    d.decay = 0.004 + 0.5 * (rng_.nextFloat() + 1.0) * 0.03;
    d.amp   = 0.1f + 0.4f * (rng_.nextFloat() + 1.0f) * 0.5f;
    d.pan   = 0.5f + rng_.nextFloat() * 0.5f;
    d.resonator.set(d.freq, d.decay, sr_, d.amp);
    d.trigger();
}
```

**Each drop is a modal resonator with one mode** (Chapter 39) — a tiny ping with a randomised
pitch and a very short decay. Randomising the *pitch* is what makes it sound like drops hitting
different surfaces at different distances.

**Surfaces** are a matter of the droplet parameters:

| Surface | Droplet character |
|---|---|
| Water / puddles | Low pitch (300–1500 Hz), longer decay, "plop" |
| Leaves | Mid pitch, very short decay, damped |
| Metal (car roof, gutter) | High pitch, long decay, ringing |
| Glass / window | High pitch, medium decay, bright |
| Concrete | Broadband, very short — almost a click |
| Umbrella | Mid-low, short, with a body resonance |

Intensity controls both the bed's level and the droplet rate — and, importantly, the *ratio*
between them. Heavy rain is mostly bed; light rain is mostly discrete drops.

---

## 40.5 Fire

Fire needs three layers, and the third is what sells it.

**1. The roar.** Low-passed brown noise, slowly modulated. This is the bulk of the energy and
almost none of the identity.

**2. The flicker.** Band-passed noise (200–1200 Hz) with irregular amplitude modulation at
5–20 Hz. This is combustion turbulence.

**3. The crackles.** Sparse, sharp, short filtered bursts at Poisson-distributed times.

```cpp
// A crackle: a very short, bright, resonant burst.
void spawnCrackle()
{
    Crackle& c = pool_[slot_++ % pool_.size()];
    c.resonator.set(800.0 + rng_.nextFloat() * 2500.0 + 2700.0,
                    0.008 + 0.04 * (rng_.nextFloat() + 1.0) * 0.5,
                    sr_, 0.3f + rng_.nextFloat() * 0.3f);
    c.noiseLength = static_cast<size_t>(0.002 * sr_);
    c.age = 0;
    c.active = true;
}
```

**Without the crackles it is wind, not fire.** The roar and flicker alone sound like a
low-frequency wind; the sharp irregular pops are the identifying feature, and they must be
**irregular** — a regular crackle rate sounds like a machine.

Crackle **rate** should also vary with fire size, and the *pitch distribution* should shift: a
small fire crackles high and fast; a large fire has lower, slower, bigger pops among the fast
ones.

---

## 40.6 Engines

Engines are the most parametric of all, and the most rewarding.

The physical picture: a four-stroke engine fires **half as many times per revolution as it has
cylinders**. At `rpm` revolutions per minute with `n` cylinders:

```
   firing frequency = rpm / 60 * n / 2   Hz
```

A 4-cylinder at 3000 rpm fires at `3000/60 × 2 = 100 Hz`. That is the fundamental.

```cpp
class Engine
{
public:
    void setRPM(double rpm)      { rpm_ = rpm; }
    void setLoad(double load)    { load_ = std::clamp(load, 0.0, 1.0); }
    void setCylinders(int n)     { cylinders_ = n; }

    float nextSample()
    {
        const double firing = rpm_ / 60.0 * cylinders_ * 0.5;

        // 1. The pulse train: one impulse per firing event.
        pulsePhase_ += firing / sr_;
        bool fired = false;
        if (pulsePhase_ >= 1.0) { pulsePhase_ -= 1.0; fired = true; }

        // 2. Each firing excites the exhaust resonances (Chapter 39).
        float exc = 0.0f;
        if (fired)
        {
            // Cylinder-to-cylinder variation: real engines are not even.
            exc = 0.7f + 0.3f * rng_.nextFloat();
            exc *= static_cast<float>(0.3 + 0.7 * load_);
        }

        double body = 0.0;
        for (auto& r : exhaust_) body += r.process(exc);

        // 3. Mechanical noise: valves, belts, induction. Scales with RPM.
        const double mech = rng_.nextFloat() * 0.08 * (0.3 + rpm_ / 6000.0);

        // 4. Induction roar: filtered noise that rises with load.
        indFilter_.setFilter(FilterType::BandPass,
                             std::clamp(firing * 4.0, 100.0, sr_ * 0.4), 1.2);
        const double induction = indFilter_.processSample(
            static_cast<float>(rng_.nextFloat() * load_ * 0.5), 0);

        return static_cast<float>((body + mech + induction) * 0.4);
    }

private:
    std::vector<ModalResonator> exhaust_;   // tuned to the exhaust system
    Biquad indFilter_;
    FastRandom rng_{ 555 };
    double sr_ = kDefaultRate, rpm_ = 800.0, load_ = 0.0;
    double pulsePhase_ = 0.0;
    int cylinders_ = 4;
};
```

**The cylinder-to-cylinder variation is what makes it sound like an engine.** A perfectly even
pulse train sounds like a synthesiser playing a low note. Real engines have slightly different
compression, timing and fuelling per cylinder, and randomising each firing's amplitude by ±15%
reproduces that lumpy, mechanical quality instantly.

**Load versus RPM are independent and both matter.** High RPM at low load (coasting downhill) is
bright and thin; low RPM at high load (labouring uphill) is deep and strained. Games that
model only RPM sound wrong during deceleration, and it is a noticeable failure.

**Engine type is the exhaust resonator set**: a V8's burble, a straight-six's smoothness, a flat-4
Subaru's offbeat character all come from the exhaust geometry and the firing order. Changing the
mode ratios changes the engine.

---

## 40.7 Footsteps, impacts and creatures

**Footsteps** are exciter + resonator (Chapter 39), plus layers:

```
   1. IMPACT:   a short noise burst through the SURFACE's modes
   2. SCUFF:    filtered noise with a short envelope, randomised
   3. MATERIAL: the shoe's own resonance (a short modal bank)
   4. VARIATION: randomise everything by 10-20% per step
```

Surface is the mode set: concrete (short, broadband), wood (a few mid modes, medium decay), metal
grating (many modes, long decay), gravel (many tiny impacts — granular, Chapter 37), snow (a
noise burst with a fast high-frequency decay, almost no resonance).

**The variation is not optional.** Ten identical footsteps is instantly recognisable as a game
from 1998. Randomising pitch by ±8%, level by ±3 dB, and timing by ±15 ms makes it read as a
person walking.

**Creatures** combine:
- A pitched layer (a modal or waveguide model — Chapters 38–39) for vocal-cord-like excitation
- A formant filter (Chapter 55) for the vocal tract
- Noise for breath
- Granular texture (Chapter 37) for something unplaceable
- Heavy pitch and formant modulation, since the whole point is that it is not human

---

## 40.8 The parameter interface

The most valuable part of a procedural system is not the synthesis — it is the *control surface*.

**Bad:** twenty knobs for filter frequencies and LFO rates.
**Good:** three parameters that mean something: `windStrength`, `rainIntensity`, `fireSize`.

```cpp
void Wind::setStrength(double s)
{
    strength_ = s;
    // ONE input drives everything, with the correct couplings.
    level_        = 0.1 + 0.9 * s;
    filterCentre_ = 300.0 + 1400.0 * s;
    filterQ_      = 0.7 + 1.5 * s;
    gustDepth_    = 0.3 + 0.5 * s;
    gustRate_     = 0.05 + 0.15 * s;
    turbulence_   = s * s;                   // nonlinear: gales are much rougher
}
```

**Designing those couplings *is* the sound design.** Getting `windStrength` to feel right across
its whole range takes far longer than writing the synthesis, and it is what makes the difference
between a system a game designer can use and one they cannot.

Note `turbulence_ = s * s`. Not every coupling is linear; strong wind is disproportionately more
turbulent than moderate wind. Choosing those curves is judgement, informed by listening.

---

## 40.9 Exercises

**40.1** Build the wind generator. Render 3 minutes and listen for any repetition. Then set both
LFO rates to exact multiples of each other and listen again.

**40.2** *Deliberate breakage.* Couple wind strength to level only, not to the filter. Sweep
strength and compare with the full version.

**40.3** Build rain with both layers. Vary the bed/droplet ratio from 0 to 1 and find where it
crosses from "light rain" to "downpour".

**40.4** Implement four rain surfaces by changing only the droplet parameters. Can a listener
identify them?

**40.5** Build fire with all three layers. Then mute the crackles. What does it sound like?

**40.6** Make the fire crackles regular (fixed interval) instead of Poisson. How obvious is it?

**40.7** Build the engine. Sweep RPM from 800 to 6500 over 10 seconds at full load, then again at
zero load. Do they sound different?

**40.8** Remove the cylinder-to-cylinder variation from the engine. Describe what it becomes.

**40.9** Build a footstep system with four surfaces and per-step randomisation. Render 20 steps
on each. Then remove the randomisation and render again.

**40.10** Design a single-parameter interface for one of these. Spend an hour tuning the
couplings. This exercise is the actual job.

---

### Chapter summary

- Procedural sound trades a little realism for **infinite variation, continuous parametric
  control and tiny memory**. It wins for backgrounds and systems; recording wins for specific,
  scrutinised sounds.
- The universal recipe: **noise → filter → modulation → layers**.
- **Three rules: nothing is regular; use incommensurate modulation rates; layer at different time
  scales.**
- **Wind** is filtered noise with wandering filter and level. Stronger wind must be **brighter as
  well as louder** — level alone reads as a volume fade. Fixed narrow resonances make it interact
  with a specific structure.
- **Rain** is a noise bed plus **Poisson-scheduled droplets**, each a single-mode resonator with
  randomised pitch. Surface = droplet parameters.
- **Fire** is roar + flicker + **irregular crackles**. Without the crackles it is wind.
- **Engines**: firing frequency is `rpm/60 × cylinders/2`, exciting **exhaust resonances**.
  **Cylinder-to-cylinder amplitude variation** is what makes it mechanical rather than musical.
  **RPM and load are independent** and both must be modelled.
- **Footsteps** are exciter + surface modes + variation. Per-step randomisation of pitch, level
  and timing is mandatory.
- The **parameter interface is the design**. One meaningful control driving many coupled
  parameters — with non-linear curves where the physics demands them — is what makes a procedural
  system usable.

**Next:** [Chapter 41 — Project: A Polyphonic Synthesizer](41-project-polyphonic-synth.md)
