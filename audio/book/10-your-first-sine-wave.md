# Chapter 10 — Your First Sine Wave

> This is the chapter where the book stops being preparation. In about forty lines of C++ you
> will produce a tone, and from here to the end of the book everything is a variation on what
> happens in this chapter's inner loop.

---

## 10.1 Why the sine wave, specifically?

Chapter 2 claimed the sine is "the atom of sound". That deserves a proper justification, because
it is not obvious — why not the triangle, or the square, or any other shape?

Three reasons, in increasing order of depth.

**1. It is what simple physical systems actually do.**

Hang a weight on a spring and pull it down. The restoring force is proportional to the
displacement (Hooke's law), and the motion that results is called **simple harmonic motion**. Its
graph over time is exactly a sine wave. The same is true of a pendulum at small angles, a mass
on a beam, a molecule of air pushed out of equilibrium, and the membrane of your eardrum. When
nature oscillates freely, it oscillates sinusoidally. A square wave is something you have to
*construct*; a sine is what happens when you leave something alone.

**2. It is the only waveform that survives a linear system unchanged.**

Feed a sine into a filter, an amplifier, a room, a speaker, or a length of cable — into any
**linear time-invariant** system, which is nearly everything in audio that is not deliberately
distorting — and what comes out is still a sine, at the same frequency. Only two things can
change: its amplitude and its phase.

No other waveform has this property. Feed a square wave into a filter and you get something that
is no longer a square. Feed a sine in and you get a sine.

This is an enormous simplification, and it is the reason the whole of filter theory is expressed
in terms of what a system does to sines at each frequency. Chapter 20 makes this precise; for
now, note that it means **if you know what a system does to every sine, you know everything about
that system.**

**3. Fourier's theorem: everything decomposes into sines.**

Combined with (2), this is decisive. Any signal is a sum of sines. A linear system's effect on
each sine is just a gain and a phase shift. Therefore a linear system's effect on *any* signal
is completely determined by its effect on sines. This is why the sine is not merely *a* useful
waveform but *the* basis of the entire analytical framework.

So we start here, and everything else in the book is built from sines or analysed in terms of
them.

---

## 10.2 The sine wave as circular motion

Forget triangles. The right mental picture is a point going round a circle.

Picture a point on the rim of a wheel of radius 1, rotating anticlockwise at a steady speed.
Track only its **height** above the centre line.

```
        y
        |        . . .
      1 |    .         .            The point goes round;
        |  .             .          its HEIGHT traces a sine wave.
        | .               .
      0 +-------------------- x
        | .               .
        |  .             .
     -1 |    .         .
        |        . . .

   Height over time:

    1 |     .-'''-.             .-'''-.
      |   .'       '.         .'       '.
    0 +--'-----------'-------'-----------'----> time
      |               '.   .'
   -1 |                 '-'
```

The angle the point has turned through is the **phase**. Height as a function of phase is
`sin(phase)`.

This picture explains several things at once:

- **Why sine and cosine are the same shape.** Cosine tracks the *horizontal* position of the same
  point. Same motion, different projection, shifted by a quarter turn.
- **Why phase wraps.** After a full turn the point is back where it started. Phase is inherently
  circular, which is why we always wrap it back into a fixed range.
- **Why amplitude is just the radius.** A bigger wheel gives a taller wave, same shape.
- **Why we use radians.** See below.

### Radians

We measure the angle not in degrees but in **radians**: the arc length travelled along a unit
circle. A full circle has circumference `2πr`, so with `r = 1`:

| Turn | Degrees | Radians |
|---|---|---|
| Quarter | 90° | π/2 ≈ 1.5708 |
| Half | 180° | π ≈ 3.14159 |
| Three-quarter | 270° | 3π/2 ≈ 4.7124 |
| **Full** | **360°** | **2π ≈ 6.28319** |

Degrees are an arbitrary human convention (360 because the Babylonians liked 60). Radians are
the natural unit, and every maths library uses them. `std::sin` takes radians, and passing
degrees is the mistake everyone makes once.

Key values worth knowing on sight:

```
   sin(0)      = 0
   sin(π/2)    = 1       (quarter turn: at the top)
   sin(π)      = 0       (half turn: back at the centre line, going down)
   sin(3π/2)   = -1      (three-quarters: at the bottom)
   sin(2π)     = 0       (full turn: back to the start)
```

---

## 10.3 From circular motion to a formula

We want a wave of frequency `f` hertz — `f` complete rotations per second.

One rotation is `2π` radians. So in one second the point turns through `2πf` radians. In `t`
seconds, it has turned through `2πft` radians. Therefore:

```
   y(t) = A · sin(2π f t + φ)
```

| Symbol | Name | Meaning |
|---|---|---|
| `A` | amplitude | the radius: peak value of the wave |
| `f` | frequency | rotations per second, in hertz |
| `t` | time | seconds |
| `φ` | phase offset | where in the cycle we start, in radians |

The quantity `2πf` appears so often it has its own name: **angular frequency**, written `ω`
(omega), measured in radians per second.

```
   ω = 2πf            y(t) = A · sin(ωt + φ)
```

### Making it discrete

We do not have continuous time. We have sample number `n = 0, 1, 2, 3, ...`, and sample `n`
occurs at time:

```
   t = n / fs
```

where `fs` is the sample rate. Substituting:

```
   y[n] = A · sin(2π f n / fs + φ)
```

The square brackets are the standard notation for a discrete sequence, as opposed to the
parentheses of a continuous function. Chapter 18 formalises this.

This is a complete, working formula. Let us check it against intuition. At `f = 440`,
`fs = 44100`:

```
   2π × 440 / 44100 = 0.0626871  radians per sample
```

So each sample advances the phase by 0.0627 radians. How many samples for a full `2π`?

```
   6.28319 / 0.0626871 = 100.227  samples
```

Which matches `44100 / 440 = 100.227` — the samples-per-cycle figure from Chapter 6. The maths
is consistent, and that little cross-check is worth doing whenever you derive something new.

---

## 10.4 Two ways to generate it, and why one is wrong

### Method 1: the direct formula

```cpp
for (int n = 0; n < numSamples; ++n)
    buffer[n] = amplitude * std::sin(2.0 * kPi * freq * n / sampleRate);
```

Straightforward, and for a fixed frequency it is exactly correct.

### Method 2: the phase accumulator

```cpp
double phase = 0.0;
const double phaseIncrement = 2.0 * kPi * freq / sampleRate;

for (int n = 0; n < numSamples; ++n)
{
    buffer[n] = amplitude * std::sin(phase);
    phase += phaseIncrement;
    if (phase >= 2.0 * kPi)
        phase -= 2.0 * kPi;
}
```

Instead of recomputing the angle from scratch each time, we keep a running angle and advance it
by a fixed step.

### Why method 2 is the one everybody uses

**Reason 1: changing frequency.** This is the decisive one. Suppose you want the frequency to
glide from 200 Hz to 800 Hz. With the direct formula you might try:

```cpp
// BROKEN
for (int n = 0; n < numSamples; ++n)
{
    const double f = 200.0 + 600.0 * n / numSamples;     // frequency sweeps up
    buffer[n] = std::sin(2.0 * kPi * f * n / sampleRate);
}
```

This does not produce a smooth sweep. It produces a mess — something that sweeps roughly twice
as fast as intended and sounds wrong.

Why? Because `2πfn/fs` is the *total accumulated phase assuming the frequency was f the whole
time*. When `f` changes, you are retroactively rewriting history: the phase jumps discontinuously
from one sample to the next, because sample `n`'s phase was computed with a different frequency
than sample `n−1`'s. Discontinuities in phase are clicks, and a continuous stream of them is
distortion.

With the accumulator there is no such problem:

```cpp
// CORRECT
double phase = 0.0;
for (int n = 0; n < numSamples; ++n)
{
    const double f = 200.0 + 600.0 * n / numSamples;
    const double inc = 2.0 * kPi * f / sampleRate;

    buffer[n] = std::sin(phase);
    phase += inc;                      // phase continues from where it was
    if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
}
```

The phase always continues from where it was. Only the *rate* of change varies. The waveform is
continuous by construction, no matter how wildly the frequency moves.

This matters everywhere: vibrato, pitch bend, portamento, FM synthesis, sirens, risers,
Doppler. Which is to say: everywhere.

> **The principle, worth stating plainly:** *frequency is the rate of change of phase.* To change
> frequency, change the increment — never recompute the phase.

**Reason 2: precision.** `2π f n / fs` with large `n` multiplies a big integer by a fraction.
After ten minutes, `n` is 26 million and the product is around 1.6 million radians; a `double`
has plenty of digits, but you are now computing the sine of a huge number, and the library must
reduce it modulo 2π internally, losing precision. The accumulator keeps phase in `[0, 2π)`
always, so `std::sin` always receives a small, well-conditioned argument.

**Reason 3: it generalises.** The accumulator is not tied to sine. Replace `std::sin(phase)` with
anything that maps phase to amplitude and you have a different oscillator — a wavetable lookup, a
sawtooth, a square. Chapter 30 builds exactly that abstraction, and Chapters 31 through 41 are
built on it.

> **Note on the wrap.** We use `if (phase >= twoPi) phase -= twoPi;` rather than
> `std::fmod(phase, twoPi)`. For normal audio frequencies the phase can only overshoot by less
> than one increment, so a single subtraction always suffices and it is far cheaper than `fmod`.
> If the frequency could exceed the sample rate — possible with extreme FM — use a `while` loop
> instead. Chapter 30 revisits this.

---

## 10.5 The program

**Code — `code/ch10/sine.cpp`**

```cpp
#include "../ch09/wavwriter.h"

#include <iostream>
#include <vector>
#include <cmath>

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// Generate a sine wave using a phase accumulator.
//   freq       : frequency in hertz
//   amplitude  : peak level, 0.0 .. 1.0
//   seconds    : duration
//   sampleRate : samples per second
//   phase0     : starting phase in radians
std::vector<float> generateSine(double freq,
                                double amplitude,
                                double seconds,
                                double sampleRate,
                                double phase0 = 0.0)
{
    const size_t numSamples = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> buffer(numSamples);

    double phase = phase0;
    const double phaseIncrement = kTwoPi * freq / sampleRate;

    for (size_t n = 0; n < numSamples; ++n)
    {
        buffer[n] = static_cast<float>(amplitude * std::sin(phase));

        phase += phaseIncrement;
        if (phase >= kTwoPi)
            phase -= kTwoPi;
    }

    return buffer;
}

int main()
{
    const double sampleRate = 44100.0;
    const double seconds    = 2.0;

    // ---- 1. The canonical test tone: A4, 440 Hz ---------------------
    {
        auto tone = generateSine(440.0, 0.5, seconds, sampleRate);
        WavWriter::write("sine440.wav", tone, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote sine440.wav\n";

        // Print the first ten samples so you can check them by hand.
        std::cout << "First 10 samples:\n";
        for (int i = 0; i < 10; ++i)
            std::cout << "  n=" << i << "  " << tone[static_cast<size_t>(i)] << "\n";
    }

    // ---- 2. Three amplitudes, to hear what amplitude does -----------
    {
        std::vector<float> out;
        for (double amp : { 0.8, 0.25, 0.08 })
        {
            auto part = generateSine(440.0, amp, 0.7, sampleRate);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("amplitudes.wav", out, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote amplitudes.wav\n";
    }

    // ---- 3. An octave ladder ----------------------------------------
    {
        std::vector<float> out;
        for (double f : { 110.0, 220.0, 440.0, 880.0, 1760.0, 3520.0 })
        {
            auto part = generateSine(f, 0.4, 0.6, sampleRate);
            out.insert(out.end(), part.begin(), part.end());
        }
        WavWriter::write("octaves.wav", out, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote octaves.wav\n";
    }

    // ---- 4. Beating: 440 Hz and 443 Hz together ---------------------
    {
        auto a = generateSine(440.0, 0.35, 5.0, sampleRate);
        auto b = generateSine(443.0, 0.35, 5.0, sampleRate);

        std::vector<float> mix(a.size());
        for (size_t i = 0; i < a.size(); ++i)
            mix[i] = a[i] + b[i];                 // superposition: just add

        WavWriter::write("beating.wav", mix, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote beating.wav  (expect 3 pulses per second)\n";
    }

    // ---- 5. Cancellation: the same tone against its inverse ---------
    {
        auto a = generateSine(440.0, 0.5, 2.0, sampleRate);
        auto b = generateSine(440.0, 0.5, 2.0, sampleRate, kPi);   // 180 degrees

        std::vector<float> mix(a.size());
        float maxAbs = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
        {
            mix[i] = a[i] + b[i];
            maxAbs = std::max(maxAbs, std::fabs(mix[i]));
        }

        WavWriter::write("cancellation.wav", mix, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote cancellation.wav  peak = " << maxAbs
                  << "  (should be ~0)\n";
    }

    // ---- 6. A frequency sweep, done correctly -----------------------
    {
        const size_t n = static_cast<size_t>(6.0 * sampleRate);
        std::vector<float> sweep(n);

        double phase = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(n);

            // Exponential sweep: equal musical intervals per second.
            const double f = 20.0 * std::pow(1000.0, t);   // 20 Hz -> 20 kHz

            sweep[i] = static_cast<float>(0.4 * std::sin(phase));

            phase += kTwoPi * f / sampleRate;
            if (phase >= kTwoPi)
                phase -= kTwoPi;
        }

        WavWriter::write("sweep.wav", sweep, static_cast<int>(sampleRate), 1);
        std::cout << "Wrote sweep.wav  (20 Hz to 20 kHz over 6 s)\n";
    }

    return 0;
}
```

**Build**

```bash
g++ -std=c++17 -Wall -Wextra -O2 sine.cpp ../ch09/wavwriter.cpp -o sine.exe
./sine.exe
```

**Output**

```
Wrote sine440.wav
First 10 samples:
  n=0  0
  n=1  0.0313333
  n=2  0.0626246
  n=3  0.0938323
  n=4  0.124915
  n=5  0.155832
  n=6  0.186541
  n=7  0.217005
  n=8  0.247182
  n=9  0.277035
Wrote amplitudes.wav
Wrote octaves.wav
Wrote beating.wav  (expect 3 pulses per second)
Wrote cancellation.wav  peak = 1.19209e-07  (should be ~0)
Wrote sweep.wav  (20 Hz to 20 kHz over 6 s)
```

> **Volume down before you play these.** Chapter 3's rule. A 440 Hz sine at amplitude 0.5 is
> considerably louder than most music at the same peak level, because a sine has no dynamics —
> it is at full RMS the entire time.

---

## 10.6 Walkthrough

**`constexpr double kPi`** — `constexpr` means the value is computed at compile time. Chapter 6
introduced it; here it means `kTwoPi` costs nothing at runtime.

**`generateSine` returns a `std::vector<float>` by value.** Chapter 7 promised this is free: the
compiler moves the vector rather than copying it. Returning containers by value is the normal,
correct modern C++ style.

**`const size_t numSamples = static_cast<size_t>(seconds * sampleRate);`** — 2.0 seconds at
44,100 gives 88,200 samples. Note both operands are `double`, so no integer division trap.

**`const double phaseIncrement = kTwoPi * freq / sampleRate;`** — computed **once**, outside the
loop. This is the heart of the oscillator. For 440 Hz at 44,100 it is 0.0626871 radians per
sample.

**`buffer[n] = static_cast<float>(amplitude * std::sin(phase));`** — the maths is done in
`double` and narrowed to `float` for storage, exactly as Chapter 6 prescribed. The explicit cast
silences a narrowing warning and documents the intent.

**The wrap.** Without it `phase` grows forever; after an hour it would be around 10 million, and
`std::sin` of that is computed with reduced precision. With it, `phase` always lives in
`[0, 2π)`.

### Checking the first samples by hand

This is a worthwhile habit. Sample 1 should be:

```
   sin(0.0626871) ≈ 0.0626461
   × amplitude 0.5 = 0.0313231
```

The program printed `0.0313333`. Close but not identical — the difference is because the printed
value is a `float` (7 digits) and our hand calculation used more decimal places in the sine. Run
the numbers to more digits and they converge. **Verifying the first few samples by hand catches
an enormous number of bugs**, especially factor-of-2π errors and degree/radian confusion, both of
which would make these numbers obviously wrong rather than subtly so.

---

## 10.7 Listen, and what to listen for

### `sine440.wav`

A pure, steady tone. This is what A above middle C sounds like with every harmonic removed —
plain, slightly dull, faintly electronic. Real instruments never sound like this, which is
exactly why it is so useful as a reference.

**Notice the click at the start and end.** The waveform begins abruptly at full amplitude and
stops abruptly. Chapter 2 told you a discontinuity is broadband; here it is, audible. Chapter 13
fixes it with envelopes, and Exercise 10.3 asks you to fix it now.

Open it in Audacity and zoom in. You will see the sine. Zoom further, to individual samples, and
you will see the dots — the point measurements from Chapter 4, with the reconstructed curve
drawn through them.

### `amplitudes.wav`

Three tones: 0.8, then 0.25, then 0.08. In decibels (Chapter 11) that is roughly −1.9 dB,
−12 dB, −22 dB.

**The perceptual point:** the amplitude drops by a factor of 3.2 from the first to the second,
but it does not sound anywhere near one-third as loud. It sounds "somewhat quieter". This is
Chapter 3's loudness law made concrete — +10 dB for twice as loud. Sit with this for a moment,
because it is the reason the next chapter exists.

### `octaves.wav`

110, 220, 440, 880, 1760, 3520 Hz — each double the last. Each step should feel like **the same
musical distance**, even though the first step adds 110 Hz and the last adds 1,760 Hz.

That is Chapter 3's logarithmic pitch perception, and it is why we use ratios and not
differences for everything to do with frequency.

Also notice the loudness changes even though the amplitude is constant at 0.4. The 110 Hz tone
sounds quietest, and the tones around 1760–3520 Hz sound loudest. That is the equal-loudness
contour of Chapter 3, which you are now hearing directly on your own equipment. On laptop
speakers the 110 Hz tone may be nearly inaudible — the speaker physically cannot move enough air
at a 3-metre wavelength.

### `beating.wav`

440 and 443 Hz together. You should hear **one** tone that pulses in volume **three times per
second** — the frequency difference.

This is Chapter 2's interference, and it is the most convincing demonstration of superposition
there is, because it is so obviously *not* what you would predict from "two tones playing".

**Experiment 10.1.** Change 443 to 441 (one pulse per second), then 450 (ten pulses, a rough
warble), then 470 (too fast to count — you begin to hear roughness), then 550 (two separate
tones, a musical interval). Find, for your own ears, the frequency at which "one pulsing tone"
becomes "two tones". That transition point is your **critical band** at 440 Hz, and Chapter 72
will give you the published figure to compare against. Very few people have measured their own.

### `cancellation.wav`

Two identical 440 Hz sines, one started half a cycle later. Peak amplitude printed as
`1.19e-07`, which is floating-point dust — effectively zero.

**Silence.** Two loud tones added together produce nothing.

If you doubt it, temporarily change the phase offset from `kPi` to `kPi * 0.999` and listen
again: a faint tone appears. The cancellation is exact only when the phase relationship is
exact.

This is noise-cancelling headphones, mono-compatibility failures, comb filtering, and a good
deal of mixing trouble, all in one four-line demonstration.

### `sweep.wav`

Twenty hertz to twenty kilohertz over six seconds. This one file is a complete hearing test and
a complete speaker test.

Listen for:

- **Where it becomes audible.** Below ~30 Hz you probably hear nothing on headphones and feel
  nothing on laptop speakers. On a subwoofer you feel it before you hear it.
- **Where it disappears at the top.** This is your personal high-frequency limit, and it declines
  with age — roughly 17 kHz at 20 years old, 15 kHz at 40, 12 kHz at 60. Run it and find out.
  (Do not turn it up to chase the last kilohertz; you will damage the hearing you are testing.)
- **Unevenness.** The amplitude is *constant* throughout. Any change in loudness you hear is
  either your ears (the equal-loudness contour) or your speakers and room (frequency response).
  On headphones, most of it is your ears. On speakers in a room, a lot of it is the room — those
  sudden peaks and dips in the low end are the standing waves from Chapter 2.
- **Buzzing or rattling** at particular frequencies is your desk, your laptop chassis, or
  something on a shelf resonating. This is a genuinely useful diagnostic, and studios use swept
  sines for exactly this.

Why exponential rather than linear? `20 × 1000^t` multiplies by a constant factor per unit time,
so it covers each octave in equal time — matching perception. A linear sweep spends five of its
six seconds above 10 kHz and sounds like it barely moves and then whistles.

---

## 10.8 Sine waves in stereo

A small addition with a large payoff. Add this to `main`:

```cpp
// Two slightly detuned sines, one per channel.
{
    auto left  = generateSine(220.0, 0.4, 4.0, sampleRate);
    auto right = generateSine(220.5, 0.4, 4.0, sampleRate);

    std::vector<std::vector<float>> stereo = { left, right };
    WavWriter::writePlanar("detuned_stereo.wav", stereo,
                           static_cast<int>(sampleRate));
}
```

220 Hz in the left ear, 220.5 Hz in the right. Over headphones, the result does not sound like
two tones, and it does not pulse the way `beating.wav` did — because the two signals never
physically add, they arrive at separate ears.

Instead the sound seems to **rotate slowly around inside your head**, half a revolution per
second. This is **binaural beating**, and what you are hearing is your brain's interaural phase
comparison (Chapter 3's ITD) being fed a continuously changing phase relationship.

Two things follow:

1. It only works on headphones. On speakers the signals mix in the air and you get ordinary
   beating.
2. It is the crudest possible spatial audio, and it already works. Chapter 73 develops this into
   real localisation, and Chapter 76 into full binaural rendering.

**Experiment 10.2.** Try 220.0 and 220.1 (a very slow rotation), then 220 and 224. At what
detuning does it stop feeling like motion and start feeling like two separate sounds?

---

## 10.9 Building a square wave out of sines

A preview of Chapter 12, and the most persuasive demonstration of Fourier's theorem you can do
in ten lines.

A square wave is the sum of **odd** harmonics, each with amplitude `1/h`:

```
   square(t) = (4/π) · [ sin(ωt) + (1/3)sin(3ωt) + (1/5)sin(5ωt) + (1/7)sin(7ωt) + ... ]
```

```cpp
// Additive square wave: sum odd harmonics only.
{
    const double f0 = 220.0;
    const int    numHarmonics = 16;         // try 1, 2, 4, 8, 16, 64
    const size_t n = static_cast<size_t>(2.0 * sampleRate);

    std::vector<float> out(n, 0.0f);

    for (int h = 1; h <= numHarmonics * 2; h += 2)      // 1, 3, 5, 7, ...
    {
        const double freq = f0 * h;
        if (freq >= sampleRate / 2.0)                    // stay below Nyquist!
            break;

        double phase = 0.0;
        const double inc = kTwoPi * freq / sampleRate;

        for (size_t i = 0; i < n; ++i)
        {
            out[i] += static_cast<float>(0.5 * (4.0 / kPi) * (1.0 / h) * std::sin(phase));
            phase += inc;
            if (phase >= kTwoPi) phase -= kTwoPi;
        }
    }

    WavWriter::write("additive_square.wav", out, static_cast<int>(sampleRate), 1);
}
```

**Experiment 10.3.** Run it with `numHarmonics` set to 1, 2, 4, 8, 16, then 64. Open each in
Audacity and zoom in.

- 1 harmonic: a sine.
- 2: a sine with a lumpy top.
- 4: recognisably square-ish, with visible ripples.
- 16: clearly a square wave with ripples near the corners.
- 64: a very convincing square.

And listen: with each addition the tone gets brighter and buzzier while the **pitch stays the
same**. That is timbre changing while frequency does not — Chapter 2's claim, demonstrated.

Those ripples near the corners never go away, no matter how many harmonics you add; they get
narrower but not smaller. This is the **Gibbs phenomenon**, and it is a genuine property of
Fourier series, not a bug in your code. Chapter 26 explains it and Chapter 31 shows why it is
actually the *correct* thing for a band-limited square wave to do.

Notice the `if (freq >= sampleRate / 2.0) break;`. Without it, harmonics above Nyquist would
alias (Chapter 4) and the result would acquire an inharmonic metallic shimmer. Try removing it
with `numHarmonics = 200` and listen to what aliasing actually sounds like — deliberately, once,
so that you recognise it forever.

---

## 10.10 Exercises

**10.1** Write `generateCosine`. Then verify numerically that `cos(x) == sin(x + π/2)` for a
range of values. What does this mean for whether you ever need a separate cosine oscillator?

**10.2** Generate a 1,000 Hz tone at amplitude 1.0 and inspect the resulting WAV in a hex
editor. What is the maximum 16-bit value present? Is it exactly 32,767? Explain any discrepancy
using Chapter 9's conversion rule and the fact that 44,100/1,000 is not a whole number.

**10.3** Add a fade-in and fade-out of 10 ms to `generateSine` to remove the clicks. A linear
ramp will do for now. How many samples is 10 ms at 44,100 Hz? Listen before and after.

**10.4** Generate a tone at exactly the Nyquist frequency (22,050 Hz) and at exactly the sample
rate (44,100 Hz). Look at the samples. Explain what you see. (At Nyquist you get exactly two
samples per cycle; at `fs` you get one. Both are instructive and neither is a usable tone.)

**10.5** Generate 20 Hz, 50 Hz and 100 Hz tones at the same amplitude. Listen on headphones, then
on laptop speakers, then on any speaker you have. Write down what you hear on each. This is
calibrating your own monitoring, and it is a professional habit.

**10.6** Implement a linear sweep (`f = 20 + 19980*t`) alongside the exponential one and compare
them. Which is more useful for testing? Why do measurement systems use exponential ("log")
sweeps?

**10.7** *Deliberate breakage.* Move `phaseIncrement` inside the loop and recompute it from `n`
using the direct formula, while sweeping the frequency. Listen to the result and explain, in
terms of phase continuity, why it sounds wrong.

**10.8** Generate a tone at 440 Hz and another at 440.0001 Hz, each 30 seconds long, and mix
them. How long until the first cancellation? Verify by looking at the waveform envelope in
Audacity. (Expected: a full beat period is 1/0.0001 = 10,000 seconds, so within 30 seconds you
should see only the very beginning of the swell.)

**10.9** Write a function `std::vector<float> mix(const std::vector<std::vector<float>>&
sources)` that sums any number of signals. Handle differing lengths by treating short ones as
zero-padded. Use it to rewrite the beating and cancellation examples.

**10.10** Using the additive method, build a **sawtooth** wave: all harmonics (not just odd),
amplitude `1/h`, with alternating signs — `sum over h of ((-1)^(h+1) / h) · sin(hωt)`. Compare
its sound to the square. Which sounds brighter, and why? (Count the harmonics each one has below
5 kHz.)

---

### Chapter summary

- The sine wave is fundamental because it is what free physical oscillation does, because it is
  the only waveform a linear system passes unchanged, and because everything decomposes into
  sines.
- Think of it as the height of a point moving round a circle. Phase is the angle; amplitude is
  the radius; frequency is how fast it turns.
- `y[n] = A · sin(2πf·n/fs + φ)`, with angular frequency `ω = 2πf`.
- **Use a phase accumulator, not the direct formula.** Keep a running `phase`, add
  `phaseIncrement = 2πf/fs` each sample, and wrap at `2π`. *Frequency is the rate of change of
  phase* — to change frequency, change the increment.
- Amplitude changes are far less perceptually dramatic than they are numerically — the reason
  for Chapter 11.
- Octaves are ratios, not differences; equal ratios feel like equal musical distances.
- Two near-frequencies **beat** at their difference; two opposite-phase signals **cancel** to
  silence. Both follow from simple addition.
- A swept sine is a complete test of your hearing, your speakers and your room.
- Odd harmonics with `1/h` amplitude sum to a square wave — Fourier's theorem, audible in ten
  lines. Always stop at Nyquist.

**Next:** [Chapter 11 — Amplitude, Decibels, and Loudness](11-amplitude-and-decibels.md)
