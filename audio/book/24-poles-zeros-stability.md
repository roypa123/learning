# Chapter 24 — Poles, Zeros, and Filter Stability

> Chapter 23 gave you filter coefficients from a cookbook. This chapter explains what those
> numbers *mean* geometrically, and gives you a test — one line of code — that tells you whether
> a filter will work or blow up. It also explains, at last, exactly why `R > 1` in Chapter 14's
> DC blocker destroyed everything.

---

## 24.1 The z-transform, gently

You do not need the full theory. You need one substitution.

**Let `z⁻¹` mean "delay by one sample".**

That is it. From Chapter 18's block diagrams you already know the `z⁻¹` box. Now we use it
algebraically.

Take the biquad difference equation:

```
   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] - a1·y[n-1] - a2·y[n-2]
```

Replace "delayed by one sample" with "multiplied by `z⁻¹`":

```
   Y = b0·X + b1·X·z⁻¹ + b2·X·z⁻² - a1·Y·z⁻¹ - a2·Y·z⁻²
```

Now it is ordinary algebra. Collect terms:

```
   Y·(1 + a1·z⁻¹ + a2·z⁻²) = X·(b0 + b1·z⁻¹ + b2·z⁻²)
```

And divide:

```
            Y      b0 + b1·z⁻¹ + b2·z⁻²
   H(z) =  ---  =  ----------------------
            X       1 + a1·z⁻¹ + a2·z⁻²
```

`H(z)` is the **transfer function**. It is the whole filter, in one expression — and it is a
*ratio of two polynomials*, which is where "bi-quadratic" came from.

### Why this is useful

Polynomials factor. Multiply top and bottom by `z²` to get positive powers:

```
            b0·z² + b1·z + b2
   H(z) =  -------------------
             z² + a1·z + a2
```

and then factor both:

```
             b0·(z - q1)·(z - q2)
   H(z) =  ------------------------
               (z - p1)·(z - p2)
```

- `q1, q2` — the roots of the numerator. These are the **zeros**: values of `z` where `H(z) = 0`.
- `p1, p2` — the roots of the denominator. These are the **poles**: values of `z` where `H(z)`
  becomes infinite.

Those four numbers (generally complex) are an alternative, completely equivalent description of
the filter. And, as we are about to see, a far more *revealing* one.

---

## 24.2 The z-plane and the unit circle

Plot poles and zeros on the complex plane (Chapter 19). By convention:

```
                  imaginary
                      ^
                 .----|----.
              .       |       .          o = zero
            .         |         .        x = pole
           .      x   |             .
           .          |             .    The circle is |z| = 1,
   --------.----------+-------------.--> the UNIT CIRCLE
           .          |             .    real
           .      x   |             .
            .         |          .
              .       |       .
                 '----|----'
```

The **unit circle** — all points with `|z| = 1` — is the single most important feature of this
picture, because:

> **The frequency response lives on the unit circle.**

Specifically, `z = e^(jω)` is a point on the unit circle at angle `ω`. So:

```
   H(ω) = H(z) evaluated at z = e^(jω)
```

Walk around the unit circle from angle 0 to angle π, evaluating `H(z)` as you go, and you trace
out the filter's frequency response from DC to Nyquist.

| Position on the circle | Angle | Frequency |
|---|---|---|
| `z = 1` (rightmost point) | 0 | DC, 0 Hz |
| `z = j` (top) | π/2 | `fs/4` |
| `z = -1` (leftmost point) | π | **Nyquist, `fs/2`** |
| Lower half | negative | negative frequencies (the mirror) |

This is why Chapter 23's `magnitudeDb` function substituted `z = e^(-jω)` — it was evaluating the
transfer function on the unit circle. Now you know what that line was doing.

---

## 24.3 The geometric interpretation

Here is the picture that makes poles and zeros intuitive, and it is genuinely useful for
designing filters by hand.

From the factored form, the magnitude at frequency `ω` is:

```
              |z - q1| · |z - q2|                          (with z = e^(jω))
   |H(ω)| = ------------------------ · |b0|
              |z - p1| · |z - p2|
```

Each `|z - q|` is the **distance** from your point on the unit circle to that zero. Each
`|z - p|` is the distance to that pole. So:

> **Gain at a frequency = (product of distances to the zeros) / (product of distances to the
> poles).**

Which gives two rules you can apply by eye:

**A zero near the circle at angle θ creates a dip at frequency θ.** Walk your point past it and
the numerator distance becomes small, so the gain drops. A zero *on* the circle gives a gain of
exactly **zero** — a perfect notch.

**A pole near the circle at angle θ creates a peak at frequency θ.** The denominator distance
becomes small, so the gain rises. A pole *on* the circle gives infinite gain — which is
oscillation. A pole *outside* gives instability.

```
   Zero ON the circle at angle theta:        Pole NEAR the circle at angle theta:

              o                                       x
          .--/|\--.                               .---|---.
        .    | |    .                           .     |     .
       .     | |     .                         .      |      .
   ----.-----+-+-----.---                  ----.------+------.---
       .           .                           .            .
        '--.....--'                             '--.......--'

   |H|:  ___     ___                       |H|:        /\
            \   /                                 ____/  \____
             \_/  <- notch at theta                    ^ peak at theta
```

### Reading the biquad filter types

Now the cookbook formulas make sense. Look at where each type puts its zeros:

| Filter | Zeros | Why |
|---|---|---|
| **Low-pass** | Both at `z = -1` | `z = -1` is Nyquist. Zeros there kill the top of the spectrum. |
| **High-pass** | Both at `z = +1` | `z = +1` is DC. Zeros there kill the bottom. |
| **Band-pass** | One at `z = +1`, one at `z = -1` | Kills both DC and Nyquist, leaving a band. |
| **Notch** | Pair exactly **on** the circle at `±ω0` | Exact zero at the notch frequency. |
| **All-pass** | Mirror images of the poles, reflected across the circle | Distances cancel, so magnitude is flat — only phase changes. |
| **Peaking** | Same angle as the poles, slightly different radius | A small local boost or cut. |

**The all-pass row is worth pausing on.** A zero at radius `r` and a pole at radius `1/r`, both at
the same angle, produce distance ratios that are constant at every frequency. Magnitude is
perfectly flat; phase is not. That is how an all-pass filter can change a signal's phase without
touching its spectrum — and it is the basis of phasers (Chapter 45) and of the diffusion stages
inside reverbs (Chapter 46).

---

## 24.4 What a pole *is*, physically

Each pole corresponds to a decaying oscillation in the impulse response. A pole at radius `r` and
angle `θ` contributes:

```
   h[n] includes:  r^n · cos(θn + φ)
```

- **The radius `r` controls the decay.** `r^n` is Chapter 18's exponential.
- **The angle `θ` controls the frequency** of that oscillation, in radians per sample.

| Radius `r` | Behaviour |
|---|---|
| 0.5 | Dies almost immediately |
| 0.9 | Decays over ~50 samples |
| 0.99 | Decays over ~500 samples |
| 0.999 | Rings for ~5,000 samples — very resonant |
| **1.0** | **Never decays. Oscillates forever.** |
| 1.001 | **Grows. Unstable.** |
| 2.0 | Doubles every sample. Catastrophic. |

And here, at last, is Chapter 14's `R > 1` experiment explained: setting `R = 1.001` placed a
pole at radius 1.001, just outside the unit circle, and `1.001^n` grows without bound. Within a
second, `1.001^44100` is about 10^19.

**The time to decay 60 dB**, in samples:

```
   n60 = ln(0.001) / ln(r) = -6.908 / ln(r)
```

For `r = 0.999`, that is 6,900 samples, or 157 ms at 44.1 kHz. This is the RT60 of Chapter 2, now
computable directly from a pole radius — which is exactly how reverb decay times are set in
Chapter 47's feedback delay networks.

---

## 24.5 The stability rule

> **An IIR filter is stable if and only if every pole lies strictly inside the unit circle:
> `|p| < 1` for all poles.**

That is the whole of stability theory for our purposes. Zeros can be anywhere — inside, outside,
on the circle — without affecting stability at all. Only poles matter.

(Zeros do matter for one thing: a filter with all zeros inside the circle is **minimum phase**,
which means it has an invertible, causal, stable inverse. That is what makes room-correction and
deconvolution possible. Zeros outside the circle make a filter non-invertible.)

### Testing a biquad

You could compute the roots of `z² + a1·z + a2 = 0` with the quadratic formula. But there is a
much simpler test, known as the **stability triangle**:

```cpp
bool isStable() const
{
    return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
}
```

Two comparisons. Plotted in the `(a1, a2)` plane, the stable region is a triangle with vertices
at `(-2, 1)`, `(2, 1)` and `(0, -1)`:

```
        a2
      1 |\         /|
        | \       / |
        |  \     /  |
      0 |   \   /   |
        |    \ /    |
     -1 |     V     |
        +-----------+---> a1
       -2     0     2
```

Inside the triangle: stable. Outside: the filter will blow up.

This test costs two comparisons and should be run on **every** set of coefficients you compute at
runtime — particularly when interpolating coefficients during a sweep, because an intermediate
interpolated value can fall outside the triangle even when both endpoints are inside it.

### Computing the actual poles

When you want the poles themselves (to know the resonant frequency and decay time):

```cpp
std::pair<std::complex<double>, std::complex<double>> poles(const BiquadCoeffs& c)
{
    // Roots of z^2 + a1*z + a2 = 0
    const double disc = c.a1 * c.a1 - 4.0 * c.a2;

    if (disc >= 0.0)                      // two real poles
    {
        const double s = std::sqrt(disc);
        return { { (-c.a1 + s) * 0.5, 0.0 },
                 { (-c.a1 - s) * 0.5, 0.0 } };
    }

    // Complex conjugate pair -- the interesting case
    const double re = -c.a1 * 0.5;
    const double im = std::sqrt(-disc) * 0.5;
    return { { re, im }, { re, -im } };
}
```

For a complex pair, there is a shortcut worth knowing:

```
   pole radius    r = sqrt(a2)
   pole frequency θ = acos( -a1 / (2·sqrt(a2)) )
```

So you can read a resonant biquad's ringing frequency and decay time straight off two
coefficients. `a2` alone tells you the resonance: `a2` close to 1 means a long ring.

---

## 24.6 Where instability actually comes from

In practice, a stable design becomes an unstable implementation for four reasons. Knowing them
saves hours.

**1. Coefficients computed at an invalid frequency.** A cutoff at or above Nyquist makes `cos(w0)`
and `sin(w0)` produce coefficients outside the triangle. This is why Chapter 23's `design`
clamps to `0.495 · fs`. **A modulated cutoff that briefly exceeds Nyquist is a very common cause
of a synth that occasionally screams.**

**2. Insufficient precision.** At low cutoff frequencies the poles cluster extremely close to
`z = 1`, and `a1` approaches −2 while `a2` approaches 1. In `float`, the difference between
`a2 = 0.99999` and `a2 = 1.00001` may not be representable — and one is stable while the other
is not. This is the concrete reason for `double` state and `double` coefficients, and it bites
hardest at exactly the settings people use most: a 30 Hz high-pass on a 96 kHz project.

**3. Interpolating coefficients.** Two stable coefficient sets can have an unstable point on the
straight line between them. Either check stability after interpolating, or interpolate a
parameterisation (frequency and Q) that cannot leave the stable region.

**4. Denormals stalling the decay.** Not instability, but the same family of problem. A filter's
state decays toward zero, enters denormal range (Chapter 6), and the CPU slows by 10–100×. Your
CPU meter rises during silence. Chapter 59's flush-to-zero fixes it.

### Defensive practice

```cpp
float processSample(float x)
{
    const double y = /* ... */;

    // A NaN or inf in the state poisons everything downstream forever.
    // Catch it, reset, and carry on rather than emitting full-scale noise.
    if (!std::isfinite(y)) { reset(); return 0.0f; }

    return static_cast<float>(y);
}
```

Three lines. It converts "the filter went unstable and produced full-scale noise into the user's
headphones" into "the filter went briefly silent". Given Chapter 3's warning about hearing
damage, this is not merely defensive programming — it is a safety feature.

---

## 24.7 Designing by placing poles and zeros

You can design filters directly in the z-plane, and for a few useful filters it is the *easiest*
method.

### A resonator (two poles, no zeros)

Put a conjugate pole pair at radius `r` and angle `θ`:

```cpp
BiquadCoeffs resonator(double freqHz, double sampleRate, double radius)
{
    const double theta = kTwoPi * freqHz / sampleRate;

    BiquadCoeffs c;
    c.b0 = 1.0 - radius;               // rough gain normalisation
    c.b1 = 0.0;
    c.b2 = 0.0;
    c.a1 = -2.0 * radius * std::cos(theta);
    c.a2 = radius * radius;
    return c;
}
```

Those last two lines follow directly from expanding `(z - p)(z - p*)` where `p = r·e^(jθ)`.

This is a **modal resonator** — it rings at `freqHz` and decays at a rate set by `radius`. Hit it
with an impulse and it sounds like a struck object. A bank of these, tuned to an object's natural
frequencies, is **modal synthesis**, and it is how Chapter 39 makes bells, metal, wood and glass.

### A notch (two zeros on the circle)

Put a conjugate zero pair exactly on the unit circle at angle `θ`, and a matching pole pair
slightly inside to keep the notch narrow:

```cpp
BiquadCoeffs notch(double freqHz, double sampleRate, double poleRadius = 0.99)
{
    const double theta = kTwoPi * freqHz / sampleRate;

    BiquadCoeffs c;
    c.b0 = 1.0;
    c.b1 = -2.0 * std::cos(theta);          // zeros ON the circle
    c.b2 = 1.0;
    c.a1 = -2.0 * poleRadius * std::cos(theta);
    c.a2 = poleRadius * poleRadius;
    return c;
}
```

`poleRadius = 0.99` gives a wide notch; `0.9999` gives a surgical one that removes almost nothing
but the exact frequency. This is the correct tool for removing mains hum at 50 or 60 Hz — and
you need notches at the harmonics too, because hum is never a pure sine.

### An all-pass

Zero at `1/r`, pole at `r`, same angle — the mirror-image pair from §24.3. The `AllPass` entry in
Chapter 23's cookbook is exactly this, and Chapters 45 and 46 use it heavily.

---

## 24.8 Seeing it

**`examples/ch24_polezero.cpp`** prints an ASCII z-plane plot for any set of coefficients:

```
  Low-pass, 1000 Hz, Q=0.707

                      imag
                 . . . . . . . .
             .                     .
          .                           .
        .                               .
       .                                 .
      .                                   .
      .              +                    o     <- two zeros at z = -1
      .                                   o        (Nyquist)
       .            x x                  .          poles at r=0.906
        .                               .
          .                           .
             .                     .
                 . . . . . . . .

  poles:  0.8659 +/- 0.2679j    radius 0.9064   freq 1000.0 Hz
  zeros: -1.0000, -1.0000       radius 1.0000   freq 22050.0 Hz
  stable: yes     (|a2| = 0.8216 < 1, |a1| = -1.7319 < 1.8216)
  RT60 of the resonance: 1.6 ms
```

And a table showing how pole radius maps to ring time:

```
  radius     n60 (samples)    time at 44.1 kHz    character
  ------------------------------------------------------------
   0.500           10            0.2 ms           dead
   0.900           66            1.5 ms           damped
   0.990          687           15.6 ms           resonant
   0.999         6905          156.6 ms           ringing
   0.9999       69074         1566.3 ms           nearly a sine
   1.0000         inf            forever          oscillator
   1.0001       -- grows --      explodes         UNSTABLE
```

**Listen to the accompanying renders.** The program fires an impulse into resonators at radius
0.9, 0.99, 0.999 and 0.9999, tuned to 220 Hz. At 0.9 you hear a click with a hint of pitch. At
0.999 you hear a clear struck-metal tone. At 0.9999 it is essentially a bell that rings for a
second and a half.

**That progression is the entire aesthetic range from "percussive" to "sustained", controlled by
one number between 0 and 1.** Chapter 39 builds instruments out of it.

---

## 24.9 Exercises

**24.1** Write out `H(z)` for `y[n] = 0.5·x[n] + 0.5·x[n-1]`. Where is its zero? Evaluate `|H|` at
`z = 1` and `z = -1` and confirm they match §22.2's hand analysis.

**24.2** For Chapter 14's DC blocker, `y[n] = x[n] - x[n-1] + R·y[n-1]`, find the pole and the
zero. Where is the zero, and why does that remove DC? What happens to the pole as `R → 1`?

**24.3** Implement `poles()` and `zeros()` for a biquad. Print them for a low-pass at 100 Hz,
1 kHz and 10 kHz, all at Q = 0.707. How does the pole radius change with frequency?

**24.4** Design a low-pass at Q = 0.5, 1, 5, 20 and print the pole radius for each. Plot radius
against Q. At what Q does the radius exceed 0.999?

**24.5** Implement the stability triangle test and verify it against directly computing the pole
magnitudes, over a thousand random `(a1, a2)` pairs. Do they ever disagree?

**24.6** Build a resonator at 440 Hz with radius 0.9995. Fire an impulse into it and measure the
RT60. Compare with `-6.908/ln(r)`.

**24.7** Build a 50 Hz notch with pole radius 0.999 and cascade three of them at 50, 100 and
150 Hz. Apply it to a signal with synthetic mains hum and measure the reduction.

**24.8** *Deliberate breakage, volume at zero.* Design a biquad, then manually set `a2 = 1.01`.
Confirm `isStable()` reports false. Run an impulse through it and print the output every 100
samples. How many samples until it exceeds 1e10?

**24.9** Demonstrate the precision problem: implement a 20 Hz high-pass biquad at 96 kHz in both
`float` and `double`, run 60 seconds of music through each, and compare. How large is the
difference, and does the `float` version stay stable?

**24.10** Design an all-pass at 1 kHz. Verify with `magnitudeDb` that its magnitude is within
0.001 dB of 0 at twenty frequencies across the spectrum, while `phaseRadians` varies
substantially. This is the property phasers exploit.

---

### Chapter summary

- **`z⁻¹` means "delay by one sample".** Substituting it turns a difference equation into
  algebra, giving the **transfer function** `H(z)` — a ratio of two polynomials.
- **Zeros** are the numerator's roots; **poles** are the denominator's.
- **The frequency response lives on the unit circle**: `H(ω) = H(z)` at `z = e^(jω)`. `z = 1` is
  DC, `z = -1` is Nyquist.
- **Gain = (product of distances to zeros) / (product of distances to poles).** A zero near the
  circle makes a dip; a pole near the circle makes a peak.
- A pole at radius `r`, angle `θ` contributes `r^n·cos(θn)` to the impulse response: **radius
  sets decay, angle sets frequency**. `n60 = -6.908/ln(r)`.
- **Stability: every pole strictly inside the unit circle.** Zeros can be anywhere. For a biquad,
  the test is two comparisons: `|a2| < 1 && |a1| < 1 + a2` — the **stability triangle**.
- Real instabilities come from: cutoff frequencies at or above Nyquist, insufficient precision at
  low frequencies (**use `double`**), interpolating coefficients through an unstable region, and
  denormals stalling decay.
- **Always guard with `if (!std::isfinite(y)) { reset(); return 0; }`** — three lines that turn
  full-scale noise into silence.
- You can design directly in the z-plane: a **resonator** is a pole pair (and is modal synthesis
  in embryo), a **notch** is a zero pair on the circle, and an **all-pass** is a pole/zero mirror
  pair.

**Next:** [Chapter 25 — The DFT, and Then the FFT](25-dft-and-fft.md)
