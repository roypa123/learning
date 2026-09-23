# Chapter 38 — Karplus-Strong and Digital Waveguides

> Instead of describing what a sound looks like, describe what the *object* does. Physical
> modelling simulates the mechanism — a string, a tube, a membrane — and lets the sound emerge.
> It starts with an algorithm so simple it seems like a trick.

---

## 38.1 Karplus-Strong: the trick

**Fill a buffer with noise. Play it in a loop. Average each sample with the previous one as it
goes round.**

```cpp
class KarplusStrong
{
public:
    void pluck(double frequency, double sampleRate, float damping = 0.5f)
    {
        const size_t length = static_cast<size_t>(sampleRate / frequency);
        buffer_.assign(length, 0.0f);

        FastRandom rng(static_cast<uint32_t>(frequency * 1000.0));
        for (auto& s : buffer_) s = rng.nextFloat();

        index_   = 0;
        damping_ = damping;
        last_    = 0.0f;
    }

    float nextSample()
    {
        if (buffer_.empty()) return 0.0f;

        const float current = buffer_[index_];

        // The entire algorithm: a two-point average fed back into the buffer.
        const float filtered = damping_ * (current + last_) * 0.5f
                             + (1.0f - damping_) * current;

        buffer_[index_] = filtered * decay_;
        last_ = current;

        index_ = (index_ + 1) % buffer_.size();
        return current;
    }

private:
    std::vector<float> buffer_;
    size_t index_   = 0;
    float  last_    = 0.0f;
    float  damping_ = 0.5f;
    float  decay_   = 0.999f;
};
```

**That produces a startlingly convincing plucked string.** Karplus and Strong published it in
1983 and it remains one of the most efficient instrument models ever devised: a buffer, an
index, and one averaging operation.

### Why it works

Three mechanisms, each of which you already know:

**1. The loop sets the pitch.** A buffer of `N` samples repeats every `N` samples, so the
fundamental is `fs/N`. This is Chapter 21's delay line: a delay with feedback is a **comb filter**
whose resonances sit at multiples of `fs/N` — exactly a harmonic series.

**2. The averaging is a low-pass filter.** `(x[n] + x[n-1])/2` is the two-point averager from
Chapter 22, which passes DC and kills Nyquist. Each time round the loop, another pass. After `k`
loops the signal has been low-passed `k` times, so **high frequencies die faster than low ones.**

That is exactly what a real string does. Chapter 2: air and materials absorb high frequencies
faster; Chapter 36 encoded this as `decay/k^damping` and here it emerges from the physics for
free.

**3. The noise burst is the pluck.** A broadband excitation puts energy into every harmonic at
once, and the loop filters and sustains it. Change the excitation and you change the attack: a
short noise burst is a pick, a filtered burst is a finger, a click is a hammer.

### The tuning problem

`N = fs/frequency` is almost never an integer. At 44,100 Hz, 440 Hz needs 100.227 samples
(Chapter 6's number, returning).

Rounding to 100 gives 441 Hz — **4 cents sharp**, which is audible and gets worse at higher
pitches. At 4 kHz, the nearest integer buffer length can be 20 cents off.

**The fix: a fractional delay.** Use an all-pass filter (Chapter 24) to interpolate the
fractional part:

```cpp
// A one-pole all-pass gives a fractional delay of `frac` samples with
// a flat magnitude response -- essential, or tuning changes the timbre.
class AllpassDelay
{
public:
    void setDelay(double frac)          // frac in 0..1
    {
        // The standard first-order all-pass coefficient.
        coeff_ = (1.0 - frac) / (1.0 + frac);
    }

    float process(float x)
    {
        const double y = coeff_ * (x - lastOut_) + lastIn_;
        lastIn_  = x;
        lastOut_ = static_cast<float>(y);
        return lastOut_;
    }

private:
    double coeff_   = 0.0;
    float  lastIn_  = 0.0f;
    float  lastOut_ = 0.0f;
};
```

**Why an all-pass rather than linear interpolation?** Linear interpolation is a low-pass
(Chapter 28), so the amount of damping would change with the fractional part of the tuning —
meaning two adjacent semitones could have noticeably different brightness. An all-pass has flat
magnitude and only affects phase, so tuning and timbre stay independent.

This matters more than it sounds. It is the difference between an instrument that is evenly
voiced across the keyboard and one that has mysteriously dull notes.

---

## 38.2 Making it musical

The basic algorithm is a starting point. Real instrument models add:

**Excitation shaping.** The noise burst determines the attack character.

| Excitation | Result |
|---|---|
| White noise burst | Bright, generic pluck |
| Low-passed noise | Soft, fingerpicked |
| High-passed noise | Bright, plectrum |
| A single impulse | Very bright, harpsichord-like |
| A short recorded pluck | Realistic, hybrid sampling/modelling |
| A filtered noise burst with a fast decay envelope | The most controllable |

**Pluck position.** Plucking a string at a point suppresses harmonics with a node there. Pluck at
1/5 of the length and the 5th harmonic vanishes. Model it by mixing the excitation with a delayed,
inverted copy:

```cpp
// Pluck position p (0..1 along the string) suppresses harmonics at 1/p.
const size_t offset = static_cast<size_t>(p * length);
for (size_t i = 0; i < length; ++i)
    buffer[i] = excitation[i] - excitation[(i + offset) % length];
```

This is a comb filter applied to the excitation, and it is a *physical* model of the constraint.
Moving the pluck position from the bridge to the middle of the string audibly changes the timbre
from bright and thin to round and full — which is exactly what happens on a real guitar.

**Damping filter.** Replace the fixed two-point average with a proper one-pole low-pass whose
cutoff you control. Now you have a brightness knob, and it maps onto a physical property: string
material and the damping of the bridge.

**Dynamic damping.** Modulate the damping with an envelope to simulate palm muting, or with a
control to simulate a hand touching the string.

**Stretched tuning.** Adding a tiny all-pass to the loop makes higher partials slightly sharp,
which models string stiffness — Chapter 36's piano inharmonicity, arrived at from the other
direction.

---

## 38.3 Digital waveguides

Karplus-Strong is the simplest case of a general theory. **Digital waveguide synthesis** (Julius
Smith, 1980s) models wave propagation in one-dimensional media — strings, tubes, bars.

The physical picture: a wave on a string travels in **both directions** and reflects off both
ends. Model that with **two delay lines**:

```
        rightward-travelling wave
   ┌──────────────────────────────────┐
   │  →  →  →  →  →  →  →  →  →  →  →│
   │                                   │
   R1                                  R2    (reflections at each end)
   │                                   │
   │← ←  ←  ←  ←  ←  ←  ←  ←  ←  ←  ← │
   └──────────────────────────────────┘
        leftward-travelling wave

   Output = the sum of both waves at the pickup position.
```

```cpp
class WaveguideString
{
public:
    void prepare(double frequency, double sampleRate)
    {
        // Each delay line is HALF the round-trip length.
        const size_t half = static_cast<size_t>(sampleRate / frequency / 2.0);
        right_.assign(half, 0.0f);
        left_.assign(half, 0.0f);
        index_ = 0;
    }

    void excite(size_t position, float amount)
    {
        // Energy splits equally in both directions -- as it physically does.
        const size_t p = position % right_.size();
        right_[p] += amount * 0.5f;
        left_[p]  += amount * 0.5f;
    }

    float nextSample(size_t pickupPosition)
    {
        const size_t n = right_.size();
        const size_t out = (index_ + pickupPosition) % n;

        // The displacement is the SUM of the two travelling waves.
        const float y = right_[out] + left_[out];

        // Reflect at both ends. The bridge (R2) inverts and damps;
        // the nut (R1) inverts with less loss.
        const float atBridge = right_[(index_ + n - 1) % n];
        const float atNut    = left_[index_];

        left_[(index_ + n - 1) % n] = -bridgeFilter_.process(atBridge);
        right_[index_]              = -atNut * nutReflection_;

        index_ = (index_ + 1) % n;
        return y;
    }

private:
    std::vector<float> right_, left_;
    size_t index_ = 0;
    OnePoleLP bridgeFilter_;
    float nutReflection_ = 0.99f;
};
```

### Why bother, when Karplus-Strong sounds fine?

Because the two-delay-line model makes physically meaningful things **available as parameters**:

| Physical property | In the model |
|---|---|
| Pluck position | Where you inject the excitation |
| Pickup position | Where you read the output |
| Bridge damping and tone | The filter in the reflection at one end |
| Nut/fret damping | The reflection coefficient at the other end |
| String stiffness | An all-pass in the loop |
| Sympathetic resonance | Coupling between multiple waveguides |
| Bowing, blowing | A **nonlinear** excitation that interacts with the returning wave |

That last row is the important one. In Karplus-Strong the excitation is fired once and then
ignored. In a waveguide, a **continuous, nonlinear** excitation can interact with the wave coming
back — and that interaction is what makes sustained instruments work.

**Bowed string:** the bow sticks to the string, drags it, slips, catches again. The stick-slip
friction curve is a nonlinearity driven by the difference between bow velocity and string
velocity. The result is self-sustaining oscillation, and it exhibits the same behaviours a real
violin does, including the difficulty of getting a clean note out of it.

**Wind instruments:** a reed or an air jet is a nonlinear pressure-controlled valve at one end of
a tube. Model the tube as a waveguide and the reed as a nonlinear function of the pressure
difference, and you get a clarinet that overblows correctly and has a proper attack transient.

**This is the appeal of physical modelling: behaviours you did not program emerge.** A waveguide
clarinet squeaks when you overblow it. A bowed string model produces scratchy tones when the bow
pressure is wrong. Nobody wrote "squeak" or "scratch" — they fall out of the simulation.

---

## 38.4 Tubes and other topologies

The same machinery models more than strings.

**A tube open at both ends** (a flute) reflects with the *same* sign at both ends, giving all
harmonics.

**A tube closed at one end** (a clarinet) reflects with opposite signs, which suppresses even
harmonics — giving the hollow, odd-harmonic-dominant clarinet timbre that Chapter 12 described.
**The model produces that automatically**, from the reflection signs alone.

**A cone** (a saxophone, an oboe) behaves like an open tube and has all harmonics, which is why a
saxophone sounds fuller than a clarinet despite both being single-reed instruments. Again, this
falls out of the geometry rather than being programmed.

**Two-dimensional membranes** (drums) and three-dimensional bodies need a different approach —
waveguide *meshes*, or modal synthesis (Chapter 39). 1D waveguides are efficient precisely
because they are 1D.

---

## 38.5 Cost and character

| Method | Cost | Sounds like |
|---|---|---|
| Karplus-Strong | ~5 ops/sample | A plucked string, immediately |
| Extended K-S | ~15 ops/sample | A guitar, with controls that make sense |
| Waveguide string | ~25 ops/sample | A guitar you can bow, mute, and pick anywhere |
| Waveguide wind | ~40 ops/sample | A clarinet that overblows |
| Modal (Ch 39) | ~10 ops per mode | Bells, plates, anything struck |

**Physical modelling is astonishingly cheap.** A guitar for 25 operations per sample compares
with 40N for additive synthesis. The reason is that the model *is* the physics — you are not
describing the output, you are running the mechanism that produces it.

**Its character:** physical models excel at *playability*. Because the parameters are physical,
they respond the way a player expects — harder excitation is brighter *and* louder, muting damps
the high end, playing near the bridge is thinner. Samples cannot do that; you would need a
separate sample for every combination.

**Its weakness:** models are hard to design, and a model that is nearly right can sound very
wrong. Physical modelling is unforgiving in a way that additive and FM are not.

---

## 38.6 For cinematic work

Physical modelling appears in cinematic sound in three ways:

**Impossible instruments.** A string 50 metres long, or one with a bridge damping that changes
over ten seconds. The model does not care that the object could not exist, and the result sounds
*physical* rather than synthetic — which is exactly the uncanny quality wanted for alien or
supernatural material.

**Resonant bodies for impacts.** Feed an impact into a waveguide or resonator bank and it takes
on the character of a struck object. Chapter 85 uses this to give impacts a sense of *material*.

**Creaks, groans and stress.** A waveguide with a slowly-changing length produces the sound of
something under tension — ship's timbers, ice, a structure failing. Nonlinear excitation makes it
irregular and organic. This is very hard to achieve any other way.

---

## 38.7 Exercises

**38.1** Implement basic Karplus-Strong. Pluck notes across four octaves and measure the actual
pitch of each against the intended one. How many cents off is the top octave?

**38.2** Add the all-pass fractional delay and re-measure. Is the tuning now accurate?

**38.3** *Deliberate breakage.* Use linear interpolation instead of an all-pass for the fractional
delay. Play a chromatic scale and listen for brightness varying between adjacent semitones.

**38.4** Implement pluck position. Render the same note plucked at 0.5, 0.2, 0.1 and 0.02 of the
string length. Which harmonics disappear at each?

**38.5** Replace the two-point average with a one-pole low-pass whose cutoff you control. Sweep it
and listen — you have built a "string material" control.

**38.6** Build the two-delay-line waveguide. Verify that moving the pickup position changes the
timbre without changing the pitch.

**38.7** Implement a clarinet: a waveguide tube with a sign-inverting reflection at one end and a
nonlinear reed function at the other. Verify that the spectrum is dominated by odd harmonics.

**38.8** Excite a Karplus-Strong string with a recorded sample instead of noise. Try a drum hit,
a vocal fragment, and a filtered impulse.

**38.9** Build a "50-metre string": set the frequency to 3 Hz so the buffer is 15,000 samples.
Excite it with an impact. What does it sound like? This is a Chapter 84 technique.

---

### Chapter summary

- **Karplus-Strong**: fill a delay line with noise, loop it, low-pass on each pass. A convincing
  plucked string in about five operations per sample.
- It works because the **loop length sets the pitch** (a comb filter's harmonic series), the
  **averaging kills high frequencies faster** (exactly what real strings do), and the **noise
  burst excites all harmonics at once**.
- `N = fs/f` is not an integer, so tuning needs a **fractional delay**. Use an **all-pass**, not
  linear interpolation — linear is a low-pass, so tuning would change timbre.
- Extensions that make it musical: **excitation shaping**, **pluck position** (a comb on the
  excitation — physically correct), a **controllable damping filter**, and an all-pass for
  **stiffness/stretched tuning**.
- **Digital waveguides** use two delay lines for the two travelling waves, making pluck position,
  pickup position, bridge and nut damping, and **nonlinear continuous excitation** all available
  as physical parameters.
- Nonlinear excitation is what enables **bowing and blowing** — and it is why behaviours you did
  not program (squeaks, scratchy bowing, correct overblowing) emerge.
- **Reflection signs determine harmonic content**: a tube closed at one end suppresses even
  harmonics, which is why a clarinet is hollow and a saxophone is not. The model produces that
  from geometry alone.
- Physical modelling is **cheap** (25 ops for a guitar) and excellent at **playability**, because
  the parameters are physical. Its weakness is that models are hard to design.
- For cinematic work: **impossible instruments**, resonant bodies for impacts, and creaks/groans
  under stress.

**Next:** [Chapter 39 — Modal Synthesis and Resonators](39-modal-synthesis.md)
