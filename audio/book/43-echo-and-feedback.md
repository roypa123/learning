# Chapter 43 — Echo, Feedback, and Stability

> A delay with feedback is four lines of code and forty years of musical culture. This chapter
> builds a usable echo, covers the filtering that makes it sound like something rather than
> nothing, and deals honestly with the ways feedback can hurt you.

---

## 43.1 The basic echo

```cpp
class Echo
{
public:
    float process(float x)
    {
        const float delayed = delay_.read(delaySamples_);

        // Feed the input PLUS the attenuated echo back into the line.
        delay_.write(x + delayed * feedback_);

        // Mix dry and wet.
        return x * (1.0f - mix_) + delayed * mix_;
    }

private:
    DelayLine delay_;
    double delaySamples_ = 22050.0;
    float  feedback_ = 0.4f;
    float  mix_      = 0.3f;
};
```

Four parameters, and each has a musically meaningful range:

| Parameter | Range | Notes |
|---|---|---|
| **Delay time** | 1 ms – 5 s | Below 50 ms it is coloration, not echo (Ch 3's precedence) |
| **Feedback** | 0 – 0.95 | The number of audible repeats |
| **Mix** | 0 – 1 | Dry/wet balance |
| **Tone** | — | See §43.3 — the difference between usable and not |

**Repeats from feedback:**

```
   audible repeats ≈ ln(0.001) / ln(feedback) = -6.908 / ln(g)
```

| Feedback | Repeats to −60 dB |
|---|---|
| 0.3 | 6 |
| 0.5 | 10 |
| 0.7 | 19 |
| 0.85 | 42 |
| 0.95 | 135 |
| 0.99 | 687 |

That is the same `n60` formula as Chapter 24's pole radius, because it is the same thing: a
feedback delay's pole sits at radius `g^(1/D)`.

---

## 43.2 Musical delay times

Delay is usually synced to tempo, and the conversions are worth having:

```cpp
double beatMs(double bpm)            { return 60000.0 / bpm; }
double delayMs(double bpm, double beats) { return beatMs(bpm) * beats; }
```

| Division | Beats | At 120 bpm |
|---|---|---|
| Whole note | 4.0 | 2000 ms |
| Half | 2.0 | 1000 ms |
| Quarter | 1.0 | 500 ms |
| **Dotted eighth** | **0.75** | **375 ms** |
| Eighth | 0.5 | 250 ms |
| Triplet eighth | 0.333 | 167 ms |
| Sixteenth | 0.25 | 125 ms |

**The dotted eighth is the famous one** — three repeats land in the space of two beats, creating
a cross-rhythm against the pulse. It is the sound of a great deal of guitar music from 1983
onward, and it works because the repeats syncopate against the beat rather than reinforcing it.

**Quarter-note delays disappear into the groove** (every repeat lands on a beat), which is
sometimes exactly what you want — a thickening you do not consciously hear.

---

## 43.3 Filtering in the loop

**An unfiltered echo is wrong**, and the reason is physical: a real echo has bounced off
something, and every bounce absorbs high frequencies (Chapter 2). An echo whose repeats are as
bright as the original does not sound like a space; it sounds like a copy.

```cpp
float process(float x)
{
    const float delayed = delay_.read(delaySamples_);

    // Filter in the FEEDBACK PATH, so each repeat is filtered again.
    float fb = delayed * feedback_;
    fb = lowpass_.processSample(fb, 0);
    fb = highpass_.processSample(fb, 0);

    delay_.write(x + fb);

    return x * (1.0f - mix_) + delayed * mix_;
}
```

**The filter must be inside the loop, not after it.** Inside, each repeat is filtered
cumulatively — repeat 5 has been low-passed five times, so it is much darker than repeat 1. That
progressive darkening is what makes echoes recede into the distance. Outside the loop, every
repeat gets the same single filtering and the effect is static.

**Typical settings:**

| Emulation | Low-pass | High-pass |
|---|---|---|
| Tape echo | 3–6 kHz | 100–200 Hz |
| Analogue BBD | 2–4 kHz | 80 Hz |
| Digital, clean | none | 40 Hz |
| Lo-fi | 1.5 kHz | 400 Hz |

**The high-pass matters as much as the low-pass.** Without it, low-frequency content accumulates
with each pass and the feedback path builds up a muddy rumble that can eventually dominate. This
is a real failure mode at high feedback, and a 100 Hz high-pass in the loop prevents it
completely.

---

## 43.4 Emulating analogue delays

Three families, three sets of imperfections — and the imperfections *are* the sound.

**Tape echo (Echoplex, Space Echo, RE-201):**

```cpp
float tapeProcess(float x)
{
    // 1. WOW AND FLUTTER: the delay time wanders. This is the defining
    //    character -- the pitch of each repeat drifts slightly.
    const double wow    = wowLFO_.next()    * 0.004;    // ~0.5 Hz, slow
    const double flutter = flutterLFO_.next() * 0.0008; // ~7 Hz, fast
    const double t = delaySamples_ * (1.0 + wow + flutter);

    const float delayed = delay_.read(t);               // Hermite: it moves fast

    // 2. TAPE SATURATION in the loop -- repeats get progressively dirtier.
    float fb = std::tanh(delayed * feedback_ * driveGain_) / driveGain_;

    // 3. Head bump: a low-mid resonance from the record/playback heads.
    fb = headBump_.processSample(fb, 0);                // peaking, ~100 Hz, +3 dB
    fb = lowpass_.processSample(fb, 0);

    // 4. Tape hiss, scaled by feedback so it accumulates like the signal.
    fb += rng_.nextFloat() * 0.0008f * feedback_;

    delay_.write(x + fb);
    return x * (1.0f - mix_) + delayed * mix_;
}
```

**Wow and flutter are the single most identifiable element.** Without them a "tape" delay is just
a filtered digital delay. With them, each repeat is slightly detuned from the last, so repeats
smear into a chorus-like wash rather than stacking into a comb filter. Sweeping the feedback
toward self-oscillation then produces the classic drifting, blooming texture.

**Bucket-brigade (BBD) delay (analogue chorus/delay pedals):** these clock analogue samples down
a chain of capacitors. Their character comes from:
- A **limited clock rate** — so longer delays mean lower bandwidth, typically 2–4 kHz at maximum
  delay. The delay time and the tone are *coupled*, which digital delays never do.
- **Companding artefacts** — BBDs use a compander to fight noise, and it "breathes".
- **Significant noise**, again scaling with delay time.

**Digital (rack units, 1980s):** 12-bit converters, aliasing on modulation, and a hard-edged
clarity. Emulating this means *adding* quantisation (Chapter 4) rather than removing it.

---

## 43.5 Feedback above 1.0

Set the feedback above 1.0 and the signal grows without bound — an exponential runaway that
reaches full scale in seconds and then keeps going.

**This is genuinely dangerous.** Chapter 3's hearing-damage warning applies directly: runaway
feedback through headphones at a normal monitoring level can reach damaging levels in under a
second.

**Three protections, all cheap:**

```cpp
// 1. Clamp the parameter. The user simply cannot ask for instability.
feedback_ = std::clamp(feedback_, 0.0f, 0.99f);

// 2. Soft-limit inside the loop. This is what makes musical
//    self-oscillation possible rather than destructive.
fb = std::tanh(fb);

// 3. Guard against NaN/inf poisoning the buffer forever.
if (!std::isfinite(fb)) { delay_.clear(); fb = 0.0f; }
```

**Number 2 is the interesting one.** A hard clamp at ±1.0 stops the runaway but sounds like
digital clipping. A `tanh` in the loop means the signal *saturates* as it grows, so feedback at
0.99 produces a sustained, saturated drone rather than an explosion — which is exactly what a
tape echo does when you push it, and it is a genuinely musical behaviour.

**Self-oscillation as an instrument.** With `tanh` limiting and filtering in the loop, feedback
near 1.0 turns the delay into an oscillator. Change the delay time and the pitch changes. Many
producers use this deliberately, and it is the basis of the "dub siren" sound.

---

## 43.6 Ping-pong and stereo topologies

```cpp
void pingPong(float inL, float inR, float& outL, float& outR)
{
    const float dl = delayL_.read(delaySamples_);
    const float dr = delayR_.read(delaySamples_);

    // CROSSED feedback: left's output feeds the right line and vice versa.
    delayL_.write(inL + dr * feedback_);
    delayR_.write(inR + dl * feedback_);

    outL = inL * (1 - mix_) + dl * mix_;
    outR = inR * (1 - mix_) + dr * mix_;
}
```

Each repeat alternates sides. The crossing is the whole trick, and it is one line.

**Other useful topologies:**

| Topology | Structure | Effect |
|---|---|---|
| **Dual mono** | Independent L and R, same time | Wide but static |
| **Offset stereo** | L and R at slightly different times (e.g. 375 / 390 ms) | Wide, drifting, natural |
| **Ping-pong** | Crossed feedback | Bouncing |
| **Haas** | One side delayed 10–30 ms, no feedback | Widening without audible echo (Ch 3) |
| **Multi-tap stereo** | Many taps, panned | Rhythmic space |

**The offset-stereo topology is underrated.** Two delays at 375 and 390 ms drift in and out of
alignment, producing a slowly-evolving stereo image that sounds far more natural than either
dual-mono or ping-pong. The offset should be a few percent, not a musical subdivision.

---

## 43.7 Ducking delay

A practical technique worth knowing: the echoes duck out of the way while the source is playing,
then swell up in the gaps.

```cpp
// Envelope-follow the dry input (Chapter 50 builds this properly).
env_ = std::max(std::fabs(x), env_ * releaseCoeff_);

const float duck = 1.0f - duckAmount_ * std::min(env_ / threshold_, 1.0f);

return x + delayed * mix_ * duck;
```

The result: a vocal or lead is never competing with its own echoes, but the space appears
instantly in every pause. This is used on essentially every modern vocal, and it is the reason a
heavy delay can be present without muddying the mix.

It is also a small example of a large idea from Chapter 3: **masking**. Rather than fight for
space, get out of the way and return when the space is free.

---

## 43.8 Exercises

**43.1** Build the basic echo. Verify the repeat count formula for feedback 0.3, 0.5, 0.7 and
0.9.

**43.2** Add a low-pass in the feedback path and compare with the same filter placed *after* the
delay. Count how many repeats it takes for the difference to become obvious.

**43.3** *Deliberate breakage.* Remove the high-pass from the feedback loop and set feedback to
0.9 on bass-heavy material. How long until it becomes unusable?

**43.4** Implement tempo sync. Render a drum loop at 120 bpm with quarter, dotted-eighth and
triplet-eighth delays. Which feels most rhythmically interesting?

**43.5** Build the tape emulation with wow and flutter. Then remove only the wow and flutter and
compare. What has been lost?

**43.6** Sweep feedback from 0.5 to 1.05 with `tanh` in the loop. Keep your volume low. Describe
what happens at each stage.

**43.7** *Deliberate breakage, volume at zero.* Remove the `tanh` and repeat 43.6. Note how many
seconds until full scale.

**43.8** Implement ping-pong and offset-stereo. Compare their stereo images. Which sounds more
natural on a sustained pad?

**43.9** Build the ducking delay. Test it on a vocal or lead line with gaps. Adjust the release
time until the echoes swell in naturally.

**43.10** Build a self-oscillating delay as an instrument: feedback at 0.99 with `tanh` and a
band-pass in the loop. Sweep the delay time and play it.

---

### Chapter summary

- An echo is a delay with feedback. Repeats to −60 dB: **`−6.908 / ln(g)`** — the same formula as
  Chapter 24's pole radius, because it is the same pole.
- **Tempo sync** matters. The **dotted eighth (0.75 beats)** syncopates against the pulse and is
  the classic musical delay; quarter notes disappear into the groove.
- **Filter inside the feedback loop, not after it.** Cumulative filtering makes each repeat
  darker, which is what makes echoes recede. A **high-pass in the loop** is as important as the
  low-pass — without it, low frequencies accumulate into mud.
- **Tape emulation**: wow and flutter are the defining element (they detune each repeat so
  repeats smear rather than comb), plus saturation in the loop, a head-bump resonance and hiss.
  **BBD delays couple delay time to bandwidth** — a coupling digital delays never have.
- **Feedback above 1.0 is a hearing hazard.** Clamp the parameter, put a **`tanh` in the loop**
  (which turns runaway into musical saturation and enables self-oscillation), and guard against
  NaN.
- **Ping-pong is crossed feedback**, one line. **Offset stereo** (e.g. 375/390 ms) is underrated
  and sounds more natural than either alternative.
- **Ducking delay** applies Chapter 3's masking insight: get out of the way while the source
  plays, swell up in the gaps.

**Next:** [Chapter 44 — Chorus, Flanger, and Fractional Delay](44-chorus-and-flanger.md)
