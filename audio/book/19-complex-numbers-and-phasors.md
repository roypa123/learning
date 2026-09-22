# Chapter 19 — Complex Numbers and Phasors

> This is the chapter people dread, and it should not be. The word "imaginary" is a 400-year-old
> naming mistake that has scared off generations of students. Complex numbers are not imaginary,
> not mysterious, and not advanced. They are **two-dimensional numbers**, and in audio they do
> exactly one job: hold an amplitude and a phase in a single value that you can do arithmetic
> with.
>
> By the end of this chapter you will see why every DSP formula is full of `e^jω`, and it will
> look like the simplest possible way to write "rotate".

---

## 19.1 The problem complex numbers solve

Chapter 2 said a sine wave has two independent properties that survive a linear system:
**amplitude** and **phase**. Chapter 10 showed why phase matters — two signals of equal amplitude
can add to double or to nothing, depending only on their phase relationship.

So when you ask "what does this filter do at 1 kHz?", the answer has two parts:

- It multiplies the amplitude by 0.5.
- It delays the phase by 37 degrees.

You could carry those around as two separate numbers. People tried; it is horrible. Combining
two filters means combining two pairs of numbers with two different rules (multiply the
amplitudes, add the phases), and every formula sprouts special cases.

**A complex number holds both, and ordinary multiplication does the right thing to both at
once.** That is the entire reason they are in DSP. Not elegance, not abstraction — bookkeeping
that works.

---

## 19.2 `i` is a rotation, not an imaginary thing

Forget "the square root of minus one" for a moment. Here is a better starting point.

Draw a number line. Put 1 on it.

```
   -----|-----|-----|-----|-----|-----
       -2    -1     0     1     2
```

**What does multiplying by −1 do?** It takes 1 to −1: a **180° rotation** about zero. Multiply by
−1 again and you are back at 1 — another 180°, total 360°.

So `−1` is "rotate half a turn".

Now ask the obvious question: **is there a number that rotates a quarter turn?** Something you
can multiply by twice to get the same effect as multiplying by −1 once?

Call it `i`. Then:

```
   i × i = -1
```

Which is exactly the definition you were taught — but now it means something concrete. `i` is
not an imaginary quantity. **`i` is the operation "rotate 90° anticlockwise".** Two of those make
180°, which is multiplying by −1. The algebra falls out of the geometry.

But a quarter turn takes you off the number line. So we need a second dimension:

```
            imaginary (the i axis)
                  ^
                2i|
                 i|      . 3 + 2i
                  |
   ---+---+---+---+---+---+---+---> real
     -3  -2  -1   0   1   2   3
                  |
                -i|
```

A **complex number** is a point on this plane. `3 + 2i` means "3 along the real axis, 2 along
the imaginary axis". Equivalently: a two-dimensional vector, or an arrow from the origin.

> **A note on `i` versus `j`.** Mathematicians write `i`. Engineers write `j`, because `i` was
> already taken for electric current. DSP literature uses **`j`** almost universally, so this
> book does too. They are the same thing.

---

## 19.3 Two ways to write the same point

### Rectangular (Cartesian) form

```
   z = a + bj
```

- `a` is the **real part**, written `Re(z)`.
- `b` is the **imaginary part**, written `Im(z)`.

Good for addition.

### Polar form

The same point, described by how far from the origin and in what direction:

```
   z = r ∠ θ
```

- `r` is the **magnitude** (modulus, absolute value), written `|z|`.
- `θ` is the **phase** (argument, angle), written `arg(z)`, in radians.

Good for multiplication — and, crucially, **this is the form that means something in audio**:
`r` is amplitude, `θ` is phase.

### Converting between them

```
   Rectangular -> polar:
      r = sqrt(a² + b²)              (Pythagoras)
      θ = atan2(b, a)

   Polar -> rectangular:
      a = r·cos(θ)
      b = r·sin(θ)
```

```
            |
          b +. . . . . . . * z = a + bj
            |            . |
            |      r   .   |
            |        .     |
            |      .       |
            |    . θ       |
   ---------+--------------+--------
            0              a
```

**Use `atan2(b, a)`, never `atan(b/a)`.** `atan` cannot tell which quadrant you are in — it
returns the same answer for `(1, 1)` and `(−1, −1)` — and it divides by zero when `a = 0`.
`atan2` takes both arguments and handles all four quadrants and both zero cases. Using `atan`
here is a classic bug, and its symptom in audio is phase values that are wrong by π for half
your spectrum.

---

## 19.4 Arithmetic, and why multiplication is the interesting one

### Addition: component-wise

```
   (a + bj) + (c + dj) = (a + c) + (b + d)j
```

Just add the parts. Geometrically, it is vector addition — put the arrows nose to tail.

**This is exactly what happens when two signals mix.** Chapter 2's superposition, in complex
form. And it immediately explains interference: two arrows pointing the same way add to a longer
arrow (constructive); two pointing opposite ways cancel (destructive); two at 90° give an arrow
of length `√2` times either, not 2 times.

### Multiplication: rotate and scale

```
   (a + bj)(c + dj) = ac + adj + bcj + bdj²
                     = ac + adj + bcj - bd            (since j² = -1)
                     = (ac - bd) + (ad + bc)j
```

Ugly in rectangular form. In polar form it is beautiful:

```
   (r1 ∠ θ1) × (r2 ∠ θ2) = (r1 · r2) ∠ (θ1 + θ2)
```

> **Multiplying complex numbers multiplies their magnitudes and ADDS their angles.**

That single sentence is why complex numbers are in DSP. Remember the two-part answer from §19.1 —
"multiply the amplitude by 0.5, add 37° to the phase"? That is *exactly* what multiplying by a
complex number does. Cascading two filters is one complex multiplication. No special cases, no
separate bookkeeping.

Check it against the rotation story: `j` is `1 ∠ 90°`. Multiplying by `j` keeps the magnitude and
adds 90° — a quarter turn, as promised. And `j × j = 1 ∠ 180° = −1`. ✓

### The conjugate

```
   z  = a + bj
   z* = a - bj          (the conjugate: flip the sign of the imaginary part)
```

Geometrically: reflect across the real axis; same magnitude, negated angle.

Its main use:

```
   z · z* = a² + b² = |z|²
```

The product of a number and its conjugate is a **real** number equal to the magnitude squared.
This is how you get magnitude (i.e. energy) out of a complex spectrum, and it is what the FFT
magnitude calculation in Chapter 25 does:

```cpp
const double magnitude = std::sqrt(re*re + im*im);       // |z|
const double power     = re*re + im*im;                  // |z|^2, no sqrt needed
```

Computing power rather than magnitude skips a square root, which matters when you are doing it
for thousands of bins per frame.

---

## 19.5 Euler's formula

Here is the equation that ties everything together:

```
   e^(jθ) = cos(θ) + j·sin(θ)
```

Read it as a statement about geometry, not about exponentials:

> **`e^(jθ)` is the point on the unit circle at angle `θ`.**

```
                  ^ imaginary
                  |
               .--|--.
            .     |     .
          .       |       . e^(jθ)
         .        |      / .
         .        |  θ  /  .
   ------.--------+----/---.------> real
         .        |        .
          .       |       .
            .     |     .
               '--|--'
```

Magnitude 1, angle `θ`. That is all it says.

### Why `e`, of all things?

A brief intuition, since "because Euler said so" is unsatisfying.

The function `e^x` has the defining property that **its rate of change equals its value**. Now
ask what function has the property that its rate of change is *perpendicular* to its value, and
of the same size.

That describes circular motion exactly: a point going round a circle at constant speed always
moves at right angles to its position vector. And "perpendicular" is multiplication by `j`.

So the function whose derivative is `j` times itself is `e^(jθ)` — and it traces a circle. The
`e` is not arbitrary; it is what makes the derivative come out right.

### The special cases

At `θ = π`:

```
   e^(jπ) = cos(π) + j·sin(π) = -1 + 0j = -1
```

which rearranges to `e^(jπ) + 1 = 0`, the famous identity. In our terms it says something
mundane: **rotating half a turn lands you at −1.** Which is where §19.2 started.

Other useful values:

```
   e^(j·0)     =  1
   e^(j·π/2)   =  j
   e^(j·π)     = -1
   e^(j·3π/2)  = -j
   e^(j·2π)    =  1        (back where we started)
```

### Sine and cosine out of exponentials

Add `e^(jθ)` and `e^(−jθ)`:

```
   e^(jθ)  =  cos θ + j sin θ
   e^(-jθ) =  cos θ - j sin θ
   -------------------------------
   sum     =  2 cos θ
```

So:

```
   cos(θ) = ( e^(jθ) + e^(-jθ) ) / 2
   sin(θ) = ( e^(jθ) - e^(-jθ) ) / (2j)
```

**This is where "negative frequency" comes from**, and it is worth pausing on because it confuses
everyone the first time.

A real cosine is the sum of **two** rotating vectors: one spinning anticlockwise at `+θ` and one
spinning clockwise at `−θ`, each of half the length. Their imaginary parts always cancel (one is
`+j sin`, the other `−j sin`), leaving a purely real result that oscillates.

```
   Two counter-rotating arrows:

        \   /              |              /   \
         \ /               |             /     \
   ---- ( X ) ----   ---- (|) ----  ---- (       ) ----
         / \               |             \     /
        /   \              |              \   /

   sum: 2·cos, real    sum: 0          sum: -2·cos
```

So **negative frequency is not a physical thing** — it is the bookkeeping that makes a real
signal come out real. You will see it in every FFT output as a mirror image in the upper half of
the spectrum (Chapter 25), and now you know why it is there rather than being told to ignore it.

---

## 19.6 The phasor: `e^(jωn)`

Now let the angle advance with time. Set `θ = ωn`, where `ω` is radians per sample (Chapter 18)
and `n` is the sample index:

```
   x[n] = e^(jωn)
```

This is a **phasor** (or complex exponential): a point spinning round the unit circle, advancing
`ω` radians per sample.

Compare with your oscillator:

```cpp
phase += phaseIncrement;               // <-- ωn, accumulated
out = std::sin(phase);                 // <-- taking the imaginary part
```

**Your phase accumulator has been computing `ωn` all along.** `std::sin(phase)` takes the
imaginary part of the phasor; `std::cos(phase)` takes the real part. The phasor is simply the
honest version that keeps both.

With amplitude and starting phase:

```
   x[n] = A · e^(j(ωn + φ)) = A · e^(jφ) · e^(jωn)
              \______________/   \________/  \______/
                                  fixed       spins
```

That factorisation is worth noticing. The constant part `A·e^(jφ)` carries the amplitude and the
initial phase — **that is what people mean when they say "a phasor"** in the static sense — and
`e^(jωn)` is the spinning.

### Why phasors are *the* signal for LTI systems

This is the punchline of the chapter, and it is the reason Chapters 20–29 work.

Feed a phasor into any linear time-invariant system. What comes out?

```
   x[n] = e^(jωn)      ---->  [ LTI system ]  ---->    y[n] = H(ω) · e^(jωn)
```

**The same phasor, multiplied by a complex number.** The frequency is unchanged. Only the
magnitude and phase change, and both changes are captured by a single complex number `H(ω)`.

In mathematical language, complex exponentials are the **eigenfunctions** of LTI systems: the
signals that pass through unchanged except for scaling. That word sounds forbidding and the idea
is simple — it is the precise version of Chapter 10's claim that "a sine in gives a sine out, at
the same frequency, with only amplitude and phase changed."

The consequence is the whole of filter theory:

> **If you know `H(ω)` for every `ω`, you know everything the system does.** `|H(ω)|` is the
> magnitude response — the curve you see on an EQ display. `arg(H(ω))` is the phase response.

Chapter 20 shows how to obtain `H(ω)`, and Chapter 25 computes it.

---

## 19.7 Complex numbers in C++

```cpp
#include <complex>

using Complex = std::complex<double>;

Complex z(3.0, 2.0);          // 3 + 2j

z.real();                     // 3.0
z.imag();                     // 2.0
std::abs(z);                  // magnitude:  sqrt(13) = 3.6056
std::arg(z);                  // phase in radians: 0.588
std::norm(z);                 // magnitude SQUARED: 13.0  (no sqrt -- faster)
std::conj(z);                 // 3 - 2j

Complex a(1.0, 2.0), b(3.0, -1.0);
a + b;                        // 4 + 1j
a * b;                        // 5 + 5j
a / b;                        // 0.1 + 0.7j

std::polar(2.0, kPi / 4.0);   // build from magnitude and angle: 1.414 + 1.414j
std::exp(Complex(0.0, kPi));  // e^(j·pi) = -1 (to floating-point precision)
```

Three things worth knowing:

**`std::norm` is magnitude *squared*, not a normalised value.** The name is a mathematical
convention and it surprises everyone. Use it when you want power and want to avoid the square
root.

**`std::complex<float>` versus `<double>`.** For FFT work, `double` is the safe default;
Chapter 25 discusses when `float` is acceptable.

**Performance.** `std::complex` arithmetic is usually as fast as hand-written real arithmetic,
because compilers inline it thoroughly. However, `std::complex<T>` multiplication in strict
IEEE mode includes NaN-handling branches. If profiling shows this mattering in a hot loop
(Chapter 63), hand-writing the four multiplications is a legitimate optimisation — but measure
first.

---

## 19.8 Seeing it work

**Code — `code/ch19/phasors.cpp`** (excerpt)

```cpp
#include <audio/audio.h>
#include <complex>
#include <iostream>
#include <iomanip>

using namespace audio;
using Complex = std::complex<double>;

int main()
{
    // --- 1. j really is a quarter turn --------------------------------
    std::cout << "--- rotation by j ---\n";
    Complex z(1.0, 0.0);
    for (int k = 0; k <= 4; ++k)
    {
        std::cout << "  j^" << k << " * 1 = ("
                  << std::setw(6) << std::fixed << std::setprecision(2) << z.real()
                  << ", " << std::setw(6) << z.imag() << "j)"
                  << "   magnitude " << std::abs(z)
                  << "   angle " << std::setw(7) << std::arg(z) * 180.0 / kPi << " deg\n";
        z *= Complex(0.0, 1.0);          // multiply by j
    }

    // --- 2. multiplication = multiply magnitudes, add angles ----------
    std::cout << "\n--- multiplication ---\n";
    const Complex a = std::polar(2.0, 30.0 * kPi / 180.0);
    const Complex b = std::polar(3.0, 45.0 * kPi / 180.0);
    const Complex c = a * b;

    std::cout << "  a: mag " << std::abs(a) << " angle " << std::arg(a)*180.0/kPi << "\n";
    std::cout << "  b: mag " << std::abs(b) << " angle " << std::arg(b)*180.0/kPi << "\n";
    std::cout << "  a*b: mag " << std::abs(c) << " angle " << std::arg(c)*180.0/kPi
              << "   (expected mag 6, angle 75)\n";

    // --- 3. Euler's formula -------------------------------------------
    std::cout << "\n--- Euler ---\n";
    for (double deg : { 0.0, 90.0, 180.0, 270.0, 360.0 })
    {
        const double th = deg * kPi / 180.0;
        const Complex e = std::exp(Complex(0.0, th));
        std::cout << "  e^(j*" << std::setw(5) << deg << " deg) = ("
                  << std::setw(6) << e.real() << ", " << std::setw(6) << e.imag() << "j)"
                  << "   cos=" << std::setw(6) << std::cos(th)
                  << " sin=" << std::setw(6) << std::sin(th) << "\n";
    }

    // --- 4. A phasor oscillator ---------------------------------------
    // Generate a sine by ROTATING, with no calls to sin() at all.
    std::cout << "\n--- phasor oscillator ---\n";
    {
        const double sr   = 44100.0;
        const double freq = 441.0;
        const double w    = kTwoPi * freq / sr;

        // One rotation step, precomputed.
        const Complex rotate = std::polar(1.0, w);

        Complex phasor(1.0, 0.0);                  // start at angle 0
        std::vector<float> out(static_cast<size_t>(sr * 2.0));

        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] = static_cast<float>(0.5 * phasor.imag());   // sine
            phasor *= rotate;                                    // ONE multiply

            // Magnitude drifts from 1.0 through rounding; renormalise
            // occasionally or the amplitude slowly decays or grows.
            if ((i & 0x3FF) == 0)
                phasor /= std::abs(phasor);
        }

        writeWav("phasor_sine.wav", out, static_cast<int>(sr), 1);
        std::cout << "  wrote phasor_sine.wav, peak "
                  << peakDb(out) << " dBFS (no sin() in the loop)\n";
    }

    // --- 5. Two counter-rotating phasors make a real cosine ----------
    std::cout << "\n--- negative frequency ---\n";
    {
        const double w = 0.3;
        for (int n = 0; n < 5; ++n)
        {
            const Complex pos = std::exp(Complex(0.0,  w * n));
            const Complex neg = std::exp(Complex(0.0, -w * n));
            const Complex sum = 0.5 * (pos + neg);
            std::cout << "  n=" << n
                      << "  0.5(e^+jwn + e^-jwn) = (" << std::setw(6) << sum.real()
                      << ", " << std::setw(6) << sum.imag() << "j)"
                      << "   cos(wn) = " << std::cos(w * n) << "\n";
        }
    }

    return 0;
}
```

**Expected output (abridged)**

```
--- rotation by j ---
  j^0 * 1 = (  1.00,   0.00j)   magnitude 1   angle    0.00 deg
  j^1 * 1 = (  0.00,   1.00j)   magnitude 1   angle   90.00 deg
  j^2 * 1 = ( -1.00,   0.00j)   magnitude 1   angle  180.00 deg
  j^3 * 1 = ( -0.00,  -1.00j)   magnitude 1   angle  -90.00 deg
  j^4 * 1 = (  1.00,  -0.00j)   magnitude 1   angle   -0.00 deg

--- multiplication ---
  a: mag 2.00 angle 30.00
  b: mag 3.00 angle 45.00
  a*b: mag 6.00 angle 75.00   (expected mag 6, angle 75)

--- Euler ---
  e^(j*  0.00 deg) = (  1.00,   0.00j)   cos=  1.00 sin=  0.00
  e^(j* 90.00 deg) = (  0.00,   1.00j)   cos=  0.00 sin=  1.00
  e^(j*180.00 deg) = ( -1.00,   0.00j)   cos= -1.00 sin=  0.00
  e^(j*270.00 deg) = ( -0.00,  -1.00j)   cos= -0.00 sin= -1.00
  e^(j*360.00 deg) = (  1.00,  -0.00j)   cos=  1.00 sin= -0.00

--- negative frequency ---
  n=0  0.5(e^+jwn + e^-jwn) = (  1.00,   0.00j)   cos(wn) = 1.00
  n=1  0.5(e^+jwn + e^-jwn) = (  0.96,   0.00j)   cos(wn) = 0.96
  n=2  0.5(e^+jwn + e^-jwn) = (  0.83,   0.00j)   cos(wn) = 0.83
```

**The imaginary parts are exactly zero in the last section.** The two counter-rotating phasors
cancel their imaginary components perfectly, leaving a real cosine. That is negative frequency
earning its keep, demonstrated rather than asserted.

### The phasor oscillator

Section 4 deserves comment because it is a genuinely useful technique.

Instead of calling `std::sin(phase)` every sample, we keep a complex number on the unit circle
and multiply it by a fixed rotation each sample. One complex multiply — four real multiplies and
two adds — replaces a transcendental function call.

**The catch is the renormalisation.** Each multiplication introduces a tiny floating-point error
in the magnitude. Repeated 44,100 times a second, those errors compound and the "unit" circle
slowly becomes a spiral: the oscillator's amplitude drifts up or down. Dividing by `std::abs`
occasionally pulls it back.

This is the same precision-accumulation issue as Chapter 6's phase accumulator, in a different
guise, and it is a nice illustration of why `double` and periodic correction are the norm in
oscillator code.

**Experiment 19.1.** Remove the renormalisation and render 60 seconds. Measure the peak in the
first and last second. Does it grow or shrink? Try `float` instead of `double` and see how much
faster the drift becomes.

**Experiment 19.2.** Renormalise every sample instead of every 1024. Time both versions. Is the
optimisation still worth it?

---

## 19.9 Where this goes

You will use complex numbers in exactly four places in the rest of this book. Knowing the list
now tells you how much of this chapter to hold in working memory.

**1. The frequency response, `H(ω)`** (Chapters 20, 23, 24). One complex number per frequency:
magnitude is the gain, angle is the phase shift. An EQ display is a plot of `|H(ω)|`.

**2. The DFT and FFT** (Chapter 25). The output is a complex number per frequency bin.
`std::abs` gives you the magnitude for a spectrum display; `std::arg` gives the phase, which
matters for pitch-shifting and vocoding (Chapters 54–55).

**3. Poles and zeros** (Chapter 24). Complex numbers on the z-plane. A filter is stable if all
its poles have magnitude less than 1 — that is, if they lie inside the unit circle. The rotation
picture from §19.2 is exactly the right intuition: a pole at radius `r` and angle `θ` produces a
decaying oscillation at frequency `θ` that decays by a factor `r` per sample. `r < 1` decays,
`r > 1` explodes. Chapter 18's `a^n` and Experiment 14.1's instability are the same fact.

**4. The analytic signal and the Hilbert transform** (Chapters 54, 76). Used to extract the
instantaneous amplitude and phase of a real signal — the basis of envelope followers that track
perfectly, of frequency shifting, and of some spatial processing.

That is all. Four uses, all of which reduce to "hold a magnitude and an angle, and multiply".

---

## 19.10 Exercises

**19.1** By hand, compute: `(2 + 3j) + (1 - 5j)`, `(2 + 3j)(1 - 5j)`, `|3 + 4j|`,
`arg(1 + j)` in degrees, `(1 + j)²`. Check each with `std::complex`.

**19.2** Convert to polar: `1 + j`, `-1 + j`, `-1 - j`, `1 - j`. All have the same magnitude —
what are the four angles? Now compute each with `atan(b/a)` instead of `atan2(b, a)` and see
which ones come out wrong. This is §19.3's warning, made concrete.

**19.3** Verify numerically that `e^(jπ) + 1 = 0` to within floating-point error. How large is
the error, and where does it come from?

**19.4** Write `Complex rotate(Complex z, double degrees)` that rotates `z` by the given angle
without changing its magnitude. Test it by rotating `1 + 0j` through 360° in 36 steps and
confirming you return to the start.

**19.5** Show, numerically, that multiplying by `e^(jω)` and then by `e^(-jω)` returns the
original number. What does this mean physically about a delay and an advance?

**19.6** Build a phasor oscillator that produces **both** a sine and a cosine (from `imag()` and
`real()`), and write them as the two channels of a stereo file. This pair is called a
**quadrature oscillator** and it is what Chapters 54 and 76 need. Verify the two channels are 90°
apart by checking that when one is at its peak, the other is at zero.

**19.7** Using `cos(θ) = (e^(jθ) + e^(-jθ))/2`, write a program that synthesises a cosine wave
using only `std::exp` on complex arguments — no `std::cos` anywhere. Confirm the output is real
to within 1e-15.

**19.8** *Deliberate breakage.* Build the phasor oscillator with `std::complex<float>` and no
renormalisation. Render 10 seconds and plot the peak amplitude per second. How fast does it
drift, and in which direction?

**19.9** A filter has `H(ω) = 0.5·e^(-j·0.6)` at ω = 0.2. What does it do to a sine at that
frequency — by how much in dB, and by how many samples of delay? (Hint: phase delay in samples is
`-arg(H)/ω`.)

**19.10** Compute `std::norm(z)` and `std::abs(z)*std::abs(z)` for a hundred random complex
numbers and time both. Which is faster, and by how much? Why does this matter for an FFT
magnitude display with 4,096 bins at 60 frames per second?

---

### Chapter summary

- Complex numbers are **two-dimensional numbers**. `j` means **"rotate 90°"**, and `j² = −1`
  because two quarter turns make a half turn. Nothing is imaginary.
- **Rectangular** `a + bj` is convenient for addition; **polar** `r ∠ θ` for multiplication. In
  audio, `r` is amplitude and `θ` is phase.
- Convert with `r = √(a²+b²)`, `θ = atan2(b, a)`. **Always `atan2`, never `atan`.**
- **Multiplication multiplies magnitudes and adds angles** — exactly the operation a filter
  performs on a sine. That is why complex numbers are in DSP.
- `z·z* = |z|²` gives magnitude squared with no square root.
- **Euler: `e^(jθ) = cos θ + j sin θ`** — the point on the unit circle at angle `θ`.
- A **phasor** `e^(jωn)` is a spinning point. Your phase accumulator has been computing `ωn` all
  along; `sin` and `cos` take its imaginary and real parts.
- A real cosine is **two counter-rotating phasors**, which is where negative frequency comes
  from, and why FFT output is mirrored.
- **Phasors are the eigenfunctions of LTI systems**: a phasor in gives the same phasor out,
  scaled by a single complex number `H(ω)`. Knowing `H(ω)` for all `ω` is knowing the system
  completely.
- In C++: `std::complex<double>`, with `abs`, `arg`, `norm` (magnitude **squared**), `conj`,
  `polar`, `exp`.

**Next:** [Chapter 20 — Linearity, Time-Invariance, and the Impulse Response](20-lti-and-impulse-response.md)
