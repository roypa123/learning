# Chapter 13 — Envelopes: Making a Note Begin and End

> Every sound you have made so far starts and stops with a click. This chapter explains exactly
> what a click is, fixes it, and then turns the fix into the single most expressive control in
> synthesis.

---

## 13.1 The click, examined

Play `sine440.wav` from Chapter 10 again. There is a tick at the start and a tick at the end.
Zoom into the very beginning in Audacity and you will see the waveform leap from zero to its full
oscillation in a single sample.

Why does an abrupt start make a *click* — a broadband, percussive noise — when the signal itself
contains only one frequency?

### The answer, in three steps

**Step 1: what you actually created.** You did not create a 440 Hz sine wave. You created a
440 Hz sine wave **multiplied by a rectangular window** — 0 before the start, 1 during, 0 after.

```
   sine (infinite)    ×    rectangular gate    =    your file

    ~~~~~~~~~~~~~~         ____|‾‾‾‾‾‾‾|____        ____~~~~~~~____
```

**Step 2: what a rectangle contains.** Chapter 12 told you that a square wave — which is a
repeating rectangle — has harmonics extending to infinity, falling only as `1/h`. A single
rectangular pulse is worse: its spectrum spreads across *all* frequencies, falling off gently.
An instantaneous edge requires infinite bandwidth, because only infinitely high frequencies can
change infinitely fast.

**Step 3: what multiplication does.** Multiplying two signals in the time domain **convolves**
their spectra in the frequency domain. (Chapter 21 proves this; for now take it as a fact with a
name.) Convolving a single 440 Hz spike with the rectangle's broad, everywhere-non-zero spectrum
smears that spike across the entire frequency range.

So the click is not an artefact or a bug in the playback. **It is real broadband energy that you
genuinely put into the signal by switching it on abruptly.** Your speaker faithfully reproduces
it.

### The same phenomenon, everywhere

Once you see this, you see it constantly:

- **A discontinuity anywhere in a buffer is a click.** Splice two unrelated pieces of audio
  together mid-waveform and you get a tick at the join.
- **A parameter change that jumps is a click.** Move a volume fader from 0.5 to 0.8 in one
  sample and you have created a step, which is a discontinuity, which is broadband. Chapter 61 is
  entirely about smoothing parameters for this reason.
- **A loop point that does not match is a click, every time round.** Chapter 69.
- **Buffer-boundary bugs are clicks at a regular rate.** If you hear ticking at exactly 86 times
  per second, that is 44100/512 — your block size. Chapter 7's debugging table listed this, and
  now you know the mechanism.
- **Digital clipping is a series of small discontinuities in the waveform's derivative**, which
  is why it sounds harsh rather than merely loud. Chapter 14.

The general principle is worth stating plainly:

> **Anything that changes instantaneously produces energy at all frequencies. To avoid broadband
> artefacts, everything must change smoothly — over at least a few milliseconds.**

---

## 13.2 The simplest fix: a fade

Multiply the start of the signal by a ramp from 0 to 1, and the end by a ramp from 1 to 0.

```cpp
void applyFades(std::vector<float>& buf, int fadeSamples)
{
    const int n = static_cast<int>(buf.size());
    const int fade = std::min(fadeSamples, n / 2);

    for (int i = 0; i < fade; ++i)
    {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        buf[static_cast<size_t>(i)]             *= g;      // fade in
        buf[static_cast<size_t>(n - 1 - i)]     *= g;      // fade out
    }
}
```

### How long does a fade need to be?

This is a real engineering question with a real answer, and the answer comes from Chapter 2's
wavelength thinking applied to time.

A fade of duration `T` seconds limits how fast the amplitude can change. Roughly, the spectral
splatter produced by a transition of duration `T` extends up to about `1/T` hertz:

| Fade length | Splatter extends to | Audible as |
|---|---|---|
| 1 sample (0.023 ms) | ~44 kHz | A sharp click |
| 0.1 ms | ~10 kHz | A distinct tick |
| 1 ms | ~1 kHz | A soft click, or a percussive attack |
| 5 ms | ~200 Hz | A clean, fast start. Inaudible as a click |
| 20 ms | ~50 Hz | Clearly a fade; a gentle onset |
| 100 ms | ~10 Hz | An obvious swell |

**The practical rule: 2–10 ms removes clicks without being heard as a fade.** At 44,100 Hz that
is 88–441 samples. Use 5 ms (220 samples) as a default and you will almost never hear a problem.

There is a genuine trade-off here, and it is the same one that runs through all of DSP: a fast
transition has wide bandwidth; a narrow bandwidth requires a slow transition. You cannot have
both. Chapter 26 gives this its formal name (the time-frequency uncertainty principle) and shows
it is the same fact that governs FFT window sizes.

### Zero-crossing edits

There is a trick that sometimes avoids the problem entirely: cut only at **zero crossings**,
where the waveform passes through 0. If both sides of a splice are at zero and moving in the same
direction, there is no step.

This works, but only partially — the *slope* still jumps, and a slope discontinuity is audible
too, just less so. Audio editors offer "snap to zero crossing" for this reason, and it is why a
very short fade is still better than a perfectly placed cut.

---

## 13.3 Linear versus exponential

A linear fade is a straight line in **amplitude**. But Chapter 11 established that we perceive
amplitude logarithmically. So what does a linear fade sound like?

Consider a fade-out from 1.0 to 0.0 over one second. At the halfway point, the amplitude is 0.5,
which is −6 dB. So half the *time* has produced only 6 dB of the roughly 60 dB journey to
inaudibility. The remaining 54 dB is crammed into the second half, and most of it into the last
few percent.

**A linear fade-out sounds like nothing happens, then a sudden drop at the end.**

An **exponential** fade is a straight line in **decibels**:

```cpp
// Linear: amplitude falls in equal steps
gain = 1.0 - t;

// Exponential: amplitude is multiplied by a constant factor each step,
// so the dB value falls in equal steps
gain = std::pow(10.0, (-60.0 * t) / 20.0);    // 0 dB down to -60 dB over t=0..1
```

This sounds smooth and natural, because equal time gives equal perceptual change. It is also
what physical systems actually do: a struck bell, a plucked string, a room's reverberation, an
RC circuit discharging — all decay exponentially, because in each case the rate of energy loss is
proportional to the energy remaining.

```
   Linear fade-out                  Exponential fade-out
   1 |\                             1 |\
     | \                              | \
     |  \                             |  '.
     |   \                            |    '-.
     |    \                           |       '--.____
   0 +-----\----                    0 +---------------'''-----
     0        1 s                      0                  1 s

   Sounds: nothing, nothing,         Sounds: smooth, even decay
   then a sudden drop
```

**The rule of thumb:**

- **Fade-ins**: linear is usually fine, and often preferable. A linear fade-in starts from true
  silence; an exponential one technically never reaches zero.
- **Fade-outs and decays**: exponential, essentially always. This is what things in the world do.
- **Crossfades**: use an equal-power curve (Chapter 74 derives it), because a linear crossfade
  dips in the middle when the two signals are uncorrelated.

### Implementing exponential decay cheaply

You do not need `std::pow` per sample. An exponential is simply "multiply by a constant each
step":

```cpp
double gain  = 1.0;
const double decayPerSample = std::pow(10.0, -60.0 / (20.0 * decaySeconds * sampleRate));

for (...)
{
    out[i] = signal[i] * static_cast<float>(gain);
    gain *= decayPerSample;             // one multiply per sample
}
```

`decayPerSample` is computed once. Each sample costs a single multiplication. That constant is
the "60 dB in `decaySeconds`" coefficient — precisely the RT60 figure from Chapter 2, which is
not a coincidence: this is the same maths as a reverb tail.

> **Denormal warning.** An exponential decay approaches zero but never reaches it. After a few
> seconds the gain enters denormal territory (Chapter 6) and your CPU load can rise sharply
> during silence. The fix is to snap to zero below a threshold:
> `if (gain < 1e-8) gain = 0.0;`. Add it now as a habit; Chapter 59 explains the full problem.

---

## 13.4 ADSR: the standard envelope

A fade handles start and stop. Musical instruments need more structure, and the industry-standard
model has four stages:

```
   amplitude
     1 |      /\
       |     /  \
       |    /    \________________          <- sustain level
       |   /                      \
       |  /                        \
     0 |_/                          \____
       +---------------------------------> time
         |A |  D |      S          | R |

         ^                          ^
       key down                   key up
```

| Stage | Meaning | Typical range |
|---|---|---|
| **Attack** | Time from 0 to peak (1.0) after the note starts | 1 ms – 5 s |
| **Decay** | Time from peak down to the sustain level | 1 ms – 5 s |
| **Sustain** | The *level* held while the key is down. **Not a time.** | 0.0 – 1.0 |
| **Release** | Time from the current level down to 0 after key release | 1 ms – 20 s |

**Sustain is a level, not a duration.** This trips up every beginner. Attack, decay and release
are measured in seconds; sustain is an amplitude between 0 and 1. The sustain *stage* lasts as
long as the key is held, which the envelope does not control.

### What the stages mean musically

Some presets, with what they sound like:

| Sound | A | D | S | R | Why |
|---|---|---|---|---|---|
| **Organ** | 0 ms | 0 ms | 1.0 | 0 ms | On/off. Nothing changes while held. (Needs a 2 ms fade to avoid clicks!) |
| **Piano** | 2 ms | 1.5 s | 0.0 | 0.3 s | Instant strike, continuous decay, no true sustain. The sustain pedal lengthens the release. |
| **Plucked string** | 1 ms | 0.6 s | 0.0 | 0.1 s | Like piano but shorter. |
| **Bowed strings / pad** | 400 ms | 200 ms | 0.8 | 800 ms | Slow swell, sustained, gentle fade. |
| **Brass** | 60 ms | 100 ms | 0.85 | 150 ms | Fast but not instant; a slight overshoot before settling. |
| **Snare / percussion** | 0 ms | 150 ms | 0.0 | 0 ms | All attack and decay. |
| **Reverse cymbal** | 2 s | 10 ms | 0.0 | 10 ms | Slow rise, abrupt cut — the classic "suck in" sound. |
| **Cinematic riser** | 6 s | 0 | 1.0 | 50 ms | Long build, hard cut at the impact. Chapter 84. |

### The attack time is the identity of the sound

Chapter 3 mentioned this and it deserves repeating with numbers, because it is the most
actionable fact in the chapter.

Classic psychoacoustic experiments: take recordings of a piano, a trumpet and a violin, and
remove the first 50 milliseconds of each. Listeners' ability to identify the instrument collapses.
Do the opposite — keep only the first 50 ms — and identification stays surprisingly good.

**The attack transient carries more identifying information than the sustained tone.** This is
why:

- A piano with its attack removed sounds like an organ.
- Reversed piano sounds nothing like a piano.
- In sound design, swapping the attack layer of a composite sound changes what it "is", while
  swapping the tail changes only where it is. Chapter 82 is built on exactly this separation.
- A slow attack on anything makes it feel *approaching* rather than *present* — because in the
  real world, only distant or gradually-building things have slow onsets. Chapter 86 uses this
  as a tension device.

---

## 13.5 Implementing ADSR as a state machine

The envelope needs to remember where it is. That makes it a **state machine**: a small set of
states, with rules for moving between them.

**Code — `code/ch13/adsr.h`**

```cpp
#pragma once

#include <cmath>
#include <algorithm>

// A four-stage ADSR envelope generator with exponential curves.
//
// Usage:
//     ADSR env;
//     env.setSampleRate(44100.0);
//     env.setParameters(0.01, 0.2, 0.7, 0.5);   // A, D, S, R
//     env.noteOn();
//     for each sample:  out = signal * env.nextSample();
//     env.noteOff();    // starts the release stage
//
class ADSR
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr)
    {
        sampleRate = sr;
        recalculate();
    }

    // attack, decay, release in SECONDS; sustain is a LEVEL 0..1.
    void setParameters(double attackSec, double decaySec,
                       double sustainLevel, double releaseSec)
    {
        attackTime   = std::max(attackSec,  0.0);
        decayTime    = std::max(decaySec,   0.0);
        sustain      = std::clamp(sustainLevel, 0.0, 1.0);
        releaseTime  = std::max(releaseSec, 0.0);
        recalculate();
    }

    void noteOn()
    {
        stage = Stage::Attack;
        // Note: we do NOT reset `value` to 0. Starting the attack from the
        // current level makes retriggering click-free. See section 13.7.
    }

    void noteOff()
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    // Hard reset: silence, no release.
    void reset()
    {
        stage = Stage::Idle;
        value = 0.0;
    }

    bool isActive() const { return stage != Stage::Idle; }

    Stage currentStage() const { return stage; }

    // Advance one sample and return the envelope value (0..1).
    float nextSample()
    {
        switch (stage)
        {
            case Stage::Idle:
                value = 0.0;
                break;

            case Stage::Attack:
                // Aim ABOVE 1.0 so the exponential curve passes through 1.0
                // with a usable slope instead of crawling asymptotically.
                value = attackTarget + (value - attackTarget) * attackCoeff;
                if (value >= 1.0)
                {
                    value = 1.0;
                    stage = (decayTime > 0.0) ? Stage::Decay : Stage::Sustain;
                }
                break;

            case Stage::Decay:
                value = decayTarget + (value - decayTarget) * decayCoeff;
                if (value <= sustain + 0.0001)
                {
                    value = sustain;
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                value = sustain;
                break;

            case Stage::Release:
                value = releaseTarget + (value - releaseTarget) * releaseCoeff;
                if (value <= 0.0001)
                {
                    value = 0.0;
                    stage = Stage::Idle;
                }
                break;
        }

        return static_cast<float>(value);
    }

private:
    // Coefficient for a one-pole exponential that travels from 0 to 1
    // in `timeSec`, measured to within about 1%.
    static double calcCoeff(double timeSec, double sampleRate)
    {
        if (timeSec <= 0.0)
            return 0.0;                       // instantaneous
        return std::exp(-1.0 / (timeSec * sampleRate));
    }

    void recalculate()
    {
        attackCoeff  = calcCoeff(attackTime,  sampleRate);
        decayCoeff   = calcCoeff(decayTime,   sampleRate);
        releaseCoeff = calcCoeff(releaseTime, sampleRate);

        // Overshoot targets shape the curves. See the walkthrough.
        attackTarget  = 1.0 + attackOvershoot;
        decayTarget   = sustain - decayUndershoot;
        releaseTarget = -releaseUndershoot;
    }

    double sampleRate   = 44100.0;

    double attackTime   = 0.01;
    double decayTime    = 0.10;
    double sustain      = 0.70;
    double releaseTime  = 0.30;

    double attackCoeff  = 0.0, decayCoeff  = 0.0, releaseCoeff = 0.0;
    double attackTarget = 0.0, decayTarget = 0.0, releaseTarget = 0.0;

    // How far past the destination each curve aims. Larger values give
    // straighter (more linear) segments; smaller give more curved ones.
    static constexpr double attackOvershoot  = 0.3;
    static constexpr double decayUndershoot  = 0.05;
    static constexpr double releaseUndershoot = 0.05;

    double value = 0.0;
    Stage  stage = Stage::Idle;
};
```

### Walkthrough

**`enum class Stage`** — a strongly-typed enumeration. Unlike a plain `enum`, values do not
implicitly convert to integers and must be written `Stage::Attack`, which prevents a whole class
of mix-ups. Use `enum class` by default.

**The exponential update: `value = target + (value - target) * coeff`.**

This one line is worth understanding deeply, because it is the most reused formula in all of
audio DSP. It is a **one-pole filter**, and you will meet it again as a smoother (Chapter 61), an
envelope follower (Chapter 50), a low-pass filter (Chapter 23), and the damping inside a reverb
(Chapter 46).

What it does: each sample, move a fraction of the remaining distance toward `target`. The
remaining distance shrinks by a constant factor `coeff` each step — which is the definition of
exponential approach.

- `coeff = 0` → jump straight to the target (instant).
- `coeff = 0.5` → halve the distance each sample (very fast).
- `coeff = 0.9999` → creep (very slow).

**`calcCoeff` uses `exp(-1 / (time × sampleRate))`.** This is the standard time-constant formula.
The value reaches about 63% of the way in `time` seconds (one time constant), and about 99% after
five. Different libraries use different conventions — some multiply by 5 so the parameter means
"time to essentially arrive" — so when comparing envelope timings between synthesisers, check
which convention is in use before concluding one is "faster".

**The overshoot targets are the subtle part.** A pure exponential *approaches* its target but
never arrives, which for an attack means the envelope would asymptotically crawl toward 1.0 and
the note would never properly start. By aiming at 1.3 instead and stopping when we cross 1.0, we
use only the early, steeper part of the curve. The result is a curve that is closer to linear and
has a decisive arrival.

**Experiment 13.1.** Set `attackOvershoot` to 0.01 and then to 3.0 and listen. Small overshoot =
very curved, "analogue" attack that eases in. Large overshoot = nearly straight, "digital" attack.
This single parameter is a large part of why different synthesisers' envelopes "feel" different,
and hardware designers argue about it.

**`noteOn()` deliberately does not reset `value` to zero.** See §13.7.

**The `0.0001` thresholds** stop the exponential from chasing zero forever, which both ends the
stage and avoids denormals.

---

## 13.6 Putting it to work

**Code — `code/ch13/envelopes.cpp`** (abridged)

```cpp
#include "adsr.h"
#include "../ch09/wavwriter.h"
#include "../ch11/decibels.h"

#include <iostream>
#include <vector>
#include <cmath>

constexpr double kTwoPi = 6.283185307179586;

// Render one note: an oscillator through an ADSR.
std::vector<float> renderNote(double freq, double amp,
                              double heldSeconds, double tailSeconds,
                              double a, double d, double s, double r,
                              double sr)
{
    const size_t heldSamples  = static_cast<size_t>(heldSeconds * sr);
    const size_t totalSamples = static_cast<size_t>((heldSeconds + tailSeconds) * sr);

    std::vector<float> out(totalSamples, 0.0f);

    ADSR env;
    env.setSampleRate(sr);
    env.setParameters(a, d, s, r);
    env.noteOn();

    double phase = 0.0;
    const double inc = kTwoPi * freq / sr;

    for (size_t i = 0; i < totalSamples; ++i)
    {
        if (i == heldSamples)
            env.noteOff();

        const double osc = std::sin(phase);
        out[i] = static_cast<float>(amp * osc * env.nextSample());

        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }

    return out;
}

int main()
{
    const double sr = 44100.0;

    // ---- 1. The click, then the fix ---------------------------------
    {
        std::vector<float> out;

        // (a) No envelope at all: clicks at both ends.
        {
            std::vector<float> raw(static_cast<size_t>(sr));
            double phase = 0.0;
            const double inc = kTwoPi * 440.0 / sr;
            for (auto& v : raw)
            {
                v = static_cast<float>(0.4 * std::sin(phase));
                phase += inc;
                if (phase >= kTwoPi) phase -= kTwoPi;
            }
            out.insert(out.end(), raw.begin(), raw.end());
        }

        // (b) 5 ms fades: clean.
        out.insert(out.end(),
                   renderNote(440.0, 0.4, 0.995, 0.005,
                              0.005, 0.0, 1.0, 0.005, sr).begin(),
                   renderNote(440.0, 0.4, 0.995, 0.005,
                              0.005, 0.0, 1.0, 0.005, sr).end());

        WavWriter::write("click_vs_fade.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 2. The preset gallery ---------------------------------------
    {
        struct Preset { const char* name; double a, d, s, r, held, tail; };
        const Preset presets[] = {
            { "organ",       0.002, 0.000, 1.00, 0.005, 1.0, 0.2 },
            { "piano",       0.002, 1.500, 0.00, 0.300, 1.0, 1.5 },
            { "pluck",       0.001, 0.400, 0.00, 0.100, 0.5, 0.6 },
            { "pad",         0.400, 0.200, 0.80, 0.800, 1.5, 1.2 },
            { "brass",       0.060, 0.100, 0.85, 0.150, 1.0, 0.4 },
            { "percussion",  0.000, 0.150, 0.00, 0.010, 0.2, 0.3 },
            { "reverse",     2.000, 0.010, 0.00, 0.010, 2.0, 0.1 },
        };

        std::vector<float> out;
        for (const auto& p : presets)
        {
            std::cout << "  " << p.name << "\n";
            auto note = renderNote(220.0, 0.5, p.held, p.tail,
                                   p.a, p.d, p.s, p.r, sr);
            out.insert(out.end(), note.begin(), note.end());

            // 300 ms of silence between presets
            out.insert(out.end(), static_cast<size_t>(0.3 * sr), 0.0f);
        }
        WavWriter::write("adsr_presets.wav", out, static_cast<int>(sr), 1);
    }

    // ---- 3. Linear vs exponential decay -----------------------------
    {
        const size_t n = static_cast<size_t>(3.0 * sr);
        std::vector<float> lin(n), expo(n);

        double phase = 0.0;
        const double inc = kTwoPi * 330.0 / sr;
        const double decayPerSample = std::pow(10.0, -60.0 / (20.0 * 3.0 * sr));
        double g = 1.0;

        for (size_t i = 0; i < n; ++i)
        {
            const double osc = std::sin(phase);
            const double t   = static_cast<double>(i) / static_cast<double>(n);

            lin[i]  = static_cast<float>(0.5 * osc * (1.0 - t));
            expo[i] = static_cast<float>(0.5 * osc * g);

            g *= decayPerSample;
            if (g < 1e-8) g = 0.0;

            phase += inc;
            if (phase >= kTwoPi) phase -= kTwoPi;
        }

        std::vector<float> both;
        both.insert(both.end(), lin.begin(),  lin.end());
        both.insert(both.end(), expo.begin(), expo.end());
        WavWriter::write("linear_vs_exponential.wav", both, static_cast<int>(sr), 1);
    }

    return 0;
}
```

### What to listen for

**`click_vs_fade.wav`** — one second with clicks, one second without. The difference is obvious
and it is the whole point of the chapter. Note that the *tone* is identical; only the edges
changed.

**`adsr_presets.wav`** — seven envelope shapes on the same 220 Hz sine.

Listen to this one carefully, because it demonstrates something important: **the oscillator never
changes.** Every preset is the same sine wave. Yet "organ", "piano" and "percussion" sound like
different instruments. The envelope alone carried that difference.

This is the single most efficient lever in synthesis. Before you reach for a new waveform or a
filter, try changing the envelope.

**`linear_vs_exponential.wav`** — three seconds of linear decay, then three of exponential.

The linear one sounds like it holds steady and then drops away at the end. The exponential one
sounds like a natural decay, evenly paced. Both end in silence at the same moment; only the
distribution of the journey differs. This is Chapter 11's logarithmic perception, made audible in
the time domain.

---

## 13.7 Retriggering without clicks

What happens if a note is retriggered while the previous one is still sounding?

Naive approach: set `value = 0.0` and start the attack. But if the envelope was at 0.8 when the
new note arrived, the output jumps from 0.8 to 0.0 in one sample — a discontinuity, which is a
click. You have reintroduced exactly the problem you spent the chapter fixing.

Our `noteOn()` does not reset `value`. The attack simply begins from wherever the envelope
currently is and climbs to 1.0. No jump, no click. This is called **legato** or **analogue-style**
retriggering, and it is what analogue synths do because their envelope capacitors cannot
discharge instantly either.

The alternative is **reset triggering**, where you want each note to start from zero — essential
for percussive sounds where a partially-decayed envelope would make the second hit quieter. The
click-free way to do that is a very fast ramp to zero first:

```cpp
void noteOnWithReset()
{
    if (value > 0.0001)
        stage = Stage::FastRetrigger;    // ramp to 0 over ~1 ms, then Attack
    else
        stage = Stage::Attack;
}
```

Most professional synthesisers implement exactly this, with a 1–5 ms "retrigger ramp" that is too
short to hear as a gap but long enough to avoid the click. Exercise 13.6 asks you to add it.

The same problem appears when **stealing a voice** in a polyphonic synth (Chapter 41): you cannot
simply stop a voice mid-note and reassign it. You must ramp it down first. A synthesiser that
clicks under heavy polyphony is nearly always failing at this.

---

## 13.8 Beyond amplitude: envelopes on everything

An envelope is just a control signal that changes over time. Nothing restricts it to amplitude.
The standard destinations, all of which we will use:

**Filter cutoff (Chapter 33).** A separate envelope on a low-pass filter's cutoff is the defining
sound of subtractive synthesis. Fast attack + short decay on the filter = a percussive "pluck"
or an acid bassline. The amplitude envelope says *how loud*; the filter envelope says *how
bright*, and brightness changing over time is what makes a synth sound alive rather than static.

**Pitch (Chapter 34).** A short, fast downward pitch envelope on a sine is a kick drum. That is
genuinely the whole recipe: a sine at 50 Hz with a pitch envelope dropping from 150 Hz to 50 Hz
over 40 ms, and an amplitude envelope with a 300 ms decay. Chapter 85 builds it.

**Pan position (Chapter 74).** Movement across the stereo field over the life of a sound.

**Effect send / mix (Chapter 82).** More reverb at the tail than at the attack makes a sound seem
to recede into a space after it starts — an extremely common cinematic technique, because it
separates "where the event is" from "where the space is".

**Modulation depth (Chapter 35).** In FM, an envelope on the modulation index controls how the
*timbre* evolves: bright at the attack, mellow in the sustain. This is exactly what real
instruments do, and it is why FM with static modulation sounds sterile.

The generalisation worth carrying forward: **a sound that does not change over time sounds
synthetic, and the envelope is your primary tool for making it change.** Static is the enemy.
Nearly every "this sounds fake" problem is a missing envelope somewhere.

---

## 13.9 Exercises

**13.1** Add `applyFades` to Chapter 10's `generateSine` and verify by ear that the clicks are
gone. Try fade lengths of 1 sample, 10 samples, 100, 220, 1000. At what length does the click
become inaudible on your system?

**13.2** Render a 440 Hz tone gated on and off 10 times per second with no fades, then with 3 ms
fades. Look at both in Audacity's spectrum view. Where did the extra frequencies go?

**13.3** Implement a **DAHDSR** envelope by adding a Delay stage (silence before the attack) and a
Hold stage (stay at peak before decaying). These are standard on modern synths. What sounds need
them?

**13.4** Add an `attackCurve` parameter (0 = fully exponential, 1 = fully linear) by exposing
`attackOvershoot`. Render the same note with five curve values and listen.

**13.5** Build a kick drum: a sine oscillator with a pitch envelope from 150 Hz to 50 Hz over
40 ms (exponential) and an amplitude envelope of A=0, D=300 ms, S=0, R=0. Then try changing only
the pitch envelope's decay time, from 5 ms to 200 ms, and describe what happens to the character.

**13.6** Add a `FastRetrigger` stage as described in §13.7. Test it by triggering the same note
20 times per second and confirming there are no clicks.

**13.7** *Deliberate breakage.* Set the release time to 0.0 and trigger `noteOff()` while the
envelope is at 1.0. Listen. Now set it to 0.002. This tells you the minimum release time your
synth should ever allow.

**13.8** Measure how long the "organ" preset's envelope actually takes to reach 1.0 with
`attackTime = 0.002`, by printing the envelope value every sample until it crosses 0.99. Compare
with the nominal 2 ms. Explain the difference using the time-constant convention in §13.5.

**13.9** Write an envelope that follows an arbitrary list of `(time, level)` breakpoints with
exponential segments between them — a **multi-segment envelope**, as found on the Korg MS-20 and
in every modern DAW's automation. Use it to build a cinematic riser: 0 → 0.3 over 4 s, 0.3 → 1.0
over 1.5 s, then 1.0 → 0 over 30 ms.

---

### Chapter summary

- A **click is real broadband energy** created by an instantaneous change. An abrupt start
  multiplies your signal by a rectangular window, whose spectrum smears across all frequencies.
- Anything that changes instantaneously — a gate, a fader, a splice, a loop point, a parameter —
  produces a click. Everything must change over **at least a few milliseconds**.
- **2–10 ms fades** remove clicks without being heard. A transition of duration `T` splatters up
  to about `1/T` Hz, which is the time-frequency trade-off in its simplest form.
- **Linear fades** are fine for fade-ins; **exponential** is correct for decays, because
  perception is logarithmic and because physical systems decay exponentially.
- Implement exponential curves as `value = target + (value - target) * coeff` — **one multiply
  per sample**. This one-pole formula recurs throughout the book as a smoother, an envelope
  follower, a filter, and a reverb damper.
- **ADSR**: attack/decay/release are times, **sustain is a level**. The attack transient carries
  most of a sound's identity.
- The same oscillator with different envelopes sounds like different instruments. Change the
  envelope before you change anything else.
- Retriggering must not reset the envelope to zero instantaneously; ramp down over ~1 ms first.
  The same discipline applies to voice stealing.
- Envelopes control far more than amplitude: filter cutoff, pitch, pan, effect depth, modulation
  index. **Static sounds synthetic; change over time is what makes a sound feel real.**

**Next:** [Chapter 14 — Mixing, Gain Staging, and Clipping](14-mixing-and-clipping.md)
