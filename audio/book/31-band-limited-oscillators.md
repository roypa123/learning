# Chapter 31 — Band-Limited Oscillators: PolyBLEP and BLIT

> Chapter 12 let you hear a naive sawtooth alias. Chapter 29 gave you oversampling, which helps
> and costs a lot. This chapter gives the technique that actually solves the problem: correct the
> waveform *at the discontinuity*, for about twelve operations per sample.

---

## 31.1 Where the aliasing actually comes from

A naive sawtooth is a straight line — a perfectly well-behaved, band-limited signal — except at
one instant per cycle, where it jumps from +1 to −1.

**The ramp is not the problem. The jump is.**

An instantaneous step has infinite bandwidth (Chapter 13). Sample that and everything above
Nyquist folds down. The rest of the waveform is innocent.

```
   Sawtooth:    /|  /|  /|
               / | / | / |
              /  |/  |/  |
                 ^   ^   ^
                 the ONLY problem is here
```

So the fix is local: **repair the waveform in the neighbourhood of each discontinuity**, and
leave the rest alone.

| Waveform | Discontinuities per cycle | What kind |
|---|---|---|
| Sine | 0 | None — already band-limited |
| Sawtooth | 1 | Step (value jumps) |
| Square / pulse | 2 | Step (value jumps) |
| Triangle | 2 | **Slope** jumps, value does not |

The triangle's value is continuous, so it aliases far less — which is why Chapter 12 measured it
at `1/h²` roll-off. It still needs correction (a **PolyBLAMP**), but it is a much smaller
problem.

---

## 31.2 BLIT and BLEP, conceptually

**BLIT** — Band-Limited Impulse Train. Build waveforms by integrating a band-limited impulse
train rather than generating them directly. Mathematically clean, historically important
(Stilson & Smith, 1996), and awkward: integration introduces DC drift that must be corrected, and
the impulse train itself is expensive.

**BLEP** — Band-Limited stEP. Instead of integrating impulses, note that a band-limited *step*
is the integral of a band-limited impulse, and simply substitute it for the naive step.

The difference between an ideal step and a band-limited step is a small correction function:

```
   ideal step:          ______|‾‾‾‾‾‾      (instantaneous)

   band-limited step:        _.-'‾‾‾‾      (rings before and after)
                        __.-'
                      '´

   the residual:       the difference between them -- what we ADD
```

**minBLEP** is the minimum-phase version: all the ringing moves *after* the transition, so the
oscillator stays causal without added latency. Used where pre-ringing is unacceptable.

**PolyBLEP** approximates the BLEP residual with a **polynomial**, evaluated only for the one or
two samples adjacent to each discontinuity. It is ten lines of code and it is what virtually
every modern virtual-analogue synth uses.

---

## 31.3 PolyBLEP, derived

The residual for a 2-sample-wide polynomial approximation is:

```
   For the sample just AFTER the discontinuity (0 <= t < 1):
       residual(t) = t² / 2 - t + 0.5        ... actually: t - t²/2 - 0.5

   For the sample just BEFORE it (-1 < t < 0):
       residual(t) = t²/2 + t + 0.5
```

In the standard formulation, with `t` expressed as a fraction of the phase increment:

```cpp
// PolyBLEP correction. `t` is the phase (0..1), `dt` the phase increment.
// Returns a value to ADD to the naive waveform.
inline double polyBlep(double t, double dt)
{
    if (t < dt)                       // just after the wrap
    {
        t /= dt;                      // normalise to 0..1
        return t + t - t * t - 1.0;
    }
    else if (t > 1.0 - dt)            // just before the wrap
    {
        t = (t - 1.0) / dt;           // normalise to -1..0
        return t * t + t + t + 1.0;
    }
    return 0.0;                        // not near a discontinuity: no correction
}
```

**Read the three branches:**

- `t < dt` — we are within one sample of having just wrapped. Apply the "after" correction.
- `t > 1 - dt` — we are within one sample of about to wrap. Apply the "before" correction.
- Otherwise — the waveform is a plain ramp here and needs nothing. **This is the common case**,
  so the function is usually two comparisons and a return.

**Why `dt` appears.** The discontinuity almost never lands exactly on a sample. `t/dt` tells you
*what fraction of a sample* has elapsed since the jump, and the correction is scaled accordingly.
Getting this sub-sample position right is the entire point — it is what distinguishes PolyBLEP
from simply smoothing the edge.

### Applying it

**Sawtooth** — one downward step per cycle, so subtract the correction:

```cpp
float polyBlepSaw(double phase, double dt)
{
    double v = 2.0 * phase - 1.0;          // the naive saw
    v -= polyBlep(phase, dt);              // correct the one discontinuity
    return static_cast<float>(v);
}
```

**Square / pulse** — two steps per cycle, one up and one down, at phase 0 and phase `width`:

```cpp
float polyBlepPulse(double phase, double dt, double width)
{
    double v = (phase < width) ? 1.0 : -1.0;

    v += polyBlep(phase, dt);                        // the rising edge at 0

    double t2 = phase - width;                       // the falling edge at `width`
    if (t2 < 0.0) t2 += 1.0;
    v -= polyBlep(t2, dt);

    return static_cast<float>(v);
}
```

Note the second edge is handled by *shifting the phase* so the same function can be reused. The
`if (t2 < 0.0) t2 += 1.0` wraps it back into range.

**Triangle** — integrate a PolyBLEP square:

```cpp
float polyBlepTriangle(double phase, double dt, double& state)
{
    const double sq = polyBlepPulse(phase, dt, 0.5);

    // A leaky integrator. `dt` scales the step so the amplitude stays
    // constant with frequency; the 0.999 leak prevents DC drift.
    state = 4.0 * dt * sq + (1.0 - 4.0 * dt) * state;
    return static_cast<float>(state);
}
```

**The leak is essential.** A pure integrator accumulates any DC offset without bound — exactly
Chapter 15's brown noise problem. The `(1 - 4·dt)` factor is a one-pole high-pass that pulls the
output back toward zero, and it is why this needs state while the saw and square do not.

---

## 31.4 What it costs and what it buys

Measured against a naive oscillator, feeding a 5 kHz saw and summing all non-harmonic energy:

```
  method                       aliasing      ops/sample     notes
  -----------------------------------------------------------------------
  naive                        -14.2 dB           3         unusable above bass
  PolyBLEP (2-point)           -48.7 dB          12         the standard
  PolyBLEP (4-point)           -61.3 dB          20         diminishing returns
  naive + 8x oversampling      -57.1 dB         ~90         expensive
  wavetable (Chapter 32)       -92.4 dB           8         best, needs memory
  additive (reference)        -140.0 dB       4000+         exact, unusable live
```

**PolyBLEP gives about 35 dB of alias reduction for four times the cost of naive.** Oversampling
gives slightly more for roughly thirty times the cost. That ratio is why PolyBLEP won.

**And it is not perfect.** −49 dB of aliasing is inaudible in a mix but measurable, and it
degrades at very high frequencies where `dt` becomes a large fraction of a cycle. Above about
`fs/8` — 5.5 kHz at 44.1 kHz — PolyBLEP's quality falls off, because the correction region starts
to occupy a significant part of the waveform.

**The practical combination used by good synths:** PolyBLEP for most of the range, wavetables for
the top octaves, or PolyBLEP with 2× oversampling for the highest notes only.

---

## 31.5 Listening

`examples/ch31_polyblep.cpp` renders the same comparisons as Chapter 12, with PolyBLEP added.

**The sweep.** A sawtooth from 100 Hz to 4 kHz over eight seconds:

- **Naive:** the descending metallic ghosts from Chapter 12. Chaos above 2 kHz.
- **PolyBLEP:** clean. A tone that thins out as it rises, exactly as it should.
- **Additive reference:** indistinguishable from PolyBLEP by ear.

**The high-note test.** A saw at 4 kHz, 6 kHz and 8 kHz:

- Naive is unusable at all three.
- PolyBLEP is clean at 4 kHz, very good at 6 kHz, slightly gritty at 8 kHz.
- The wavetable version (next chapter) is clean throughout.

**The spectrogram** (Chapter 27) makes it unmistakable: the naive version shows a lattice of
crossing diagonals; the PolyBLEP version shows a clean fan of ascending harmonics that disappear
one by one as they cross Nyquist — which is exactly the correct behaviour.

That "disappearing one by one" is worth noticing. A correct band-limited oscillator **loses
harmonics as it plays higher**, and therefore gets *thinner* toward the top of the keyboard.
That is not a defect; it is what Chapter 6's harmonic-count table predicted. A naive oscillator
keeps its brightness by fabricating garbage, which is why beginners sometimes prefer the sound of
aliasing without knowing what they are hearing.

---

## 31.6 Hard sync, correctly

Chapter 30 introduced oscillator sync and noted that it aliases badly. Now we can say why it is
hard.

When the master wraps, the slave's phase is forced to zero. That creates a step of arbitrary
size, at an **arbitrary sub-sample position** — the master's wrap almost never lands on a sample
boundary either.

The correct treatment:

1. Compute exactly where within the sample the master wrapped: `frac = masterPhase / masterInc`.
2. Compute the slave's value just before and just after the reset.
3. Apply a PolyBLEP scaled by that step size, positioned at `frac`.

```cpp
if (masterWrapped)
{
    const double frac    = master.phase() / master.increment();
    const double before  = 2.0 * slavePhase - 1.0;
    slavePhase = frac * slaveInc;           // the slave restarts partway through
    const double after   = 2.0 * slavePhase - 1.0;

    pendingStep     = after - before;       // the discontinuity's size
    pendingStepFrac = frac;
}
```

Then apply a scaled PolyBLEP over the next sample or two. Getting the sub-sample position right
is what separates a sync that sounds like a classic analogue lead from one that sounds like
digital noise.

This is genuinely fiddly, and it is why many synths implement sync by oversampling instead.

---

## 31.7 When PolyBLEP is not the answer

| Situation | Better approach | Why |
|---|---|---|
| Arbitrary waveform shapes | Wavetable (Ch 32) | PolyBLEP only knows about steps and corners |
| The top two octaves | Wavetable | PolyBLEP degrades above `fs/8` |
| Waveform morphing | Wavetable | Interpolate between tables |
| Highest possible quality | Wavetable or oversampled | −92 dB vs −49 dB |
| Extreme FM | Oversampling | Modulation moves discontinuities unpredictably |
| Wave-folding, hard shaping | Oversampling (Ch 29) | It is a nonlinearity, not a discontinuity |

**The rule:** PolyBLEP fixes discontinuities you can locate analytically. When you cannot locate
them — because the waveform is arbitrary, or because a nonlinearity is creating them — you need
oversampling or a table.

---

## 31.8 Exercises

**31.1** Implement `polyBlep` and the saw. Render at 100 Hz, 1 kHz and 4 kHz and compare each
with the naive version by ear and by spectrogram.

**31.2** Measure aliasing energy (Chapter 29's `aliasingEnergyDb`) for naive and PolyBLEP saws at
500 Hz, 1 kHz, 2 kHz, 4 kHz and 8 kHz. Build a table. Where does PolyBLEP start to degrade?

**31.3** Implement the PolyBLEP pulse with adjustable width. Verify both edges are corrected by
rendering at width 0.5 and 0.1 and checking the spectrum.

**31.4** *Deliberate breakage.* Correct only the first edge of the pulse, not the second. What
does the spectrum show? What does it sound like?

**31.5** Implement the PolyBLEP triangle. Remove the leak (use a pure integrator) and measure the
DC offset after 10 seconds. Then restore it.

**31.6** Compare PolyBLEP against naive + 2× and naive + 4× oversampling, measuring both aliasing
and CPU time. Plot quality against cost. Where does PolyBLEP sit on that curve?

**31.7** Render a supersaw (Chapter 30) with naive oscillators and with PolyBLEP. Seven detuned
saws multiply the aliasing — how much worse is the naive version than a single saw?

**31.8** Implement 4-point PolyBLEP (a higher-order polynomial covering two samples either side).
How much better is it, and is it worth the extra cost?

**31.9** Implement hard sync with PolyBLEP correction as described in §31.6. Compare with naive
sync by spectrogram.

---

### Chapter summary

- Aliasing in classic waveforms comes from the **discontinuities**, not the ramps. Fix the
  discontinuity and you fix the waveform.
- Saw = 1 step/cycle, square = 2 steps, triangle = 2 **slope** corners (hence far less aliasing,
  `1/h²`).
- **PolyBLEP** approximates the band-limited-step residual with a polynomial, applied only to the
  one or two samples adjacent to each discontinuity. Ten lines, ~12 operations per sample.
- The `t/dt` normalisation captures the **sub-sample position** of the discontinuity — that is
  what makes it work rather than merely smoothing the edge.
- A pulse needs **both** edges corrected; a triangle integrates a corrected square and needs a
  **leaky** integrator or it accumulates DC.
- PolyBLEP gives about **−49 dB aliasing for 4× the cost of naive**; 8× oversampling gives −57 dB
  for 30× the cost. That ratio is why PolyBLEP is the standard.
- It **degrades above `fs/8`**. Good synths switch to wavetables for the top octaves.
- A correct band-limited oscillator **loses harmonics and thins out as it plays higher**. That is
  correct behaviour, not a defect.
- **Hard sync needs the sub-sample reset position** to be computed and the step scaled
  accordingly — the difference between a classic analogue lead and digital noise.

**Next:** [Chapter 32 — Wavetable Synthesis](32-wavetable-synthesis.md)
