# Chapter 29 — Monte Carlo integration

[← Volumes](28-volumes.md) · [Contents](README.md) · [Next: Importance sampling →](30-importance-sampling.md)

---

## Goal

Part 6 makes our renderer **physically correct and much less noisy**. To get there we need the
mathematical idea behind everything we've been doing: **Monte Carlo integration**. You'll learn:

* what an **integral** is (without calculus class),
* why rendering is an integral,
* how random sampling estimates integrals, and why the error shrinks like 1/√N,
* **stratification**, a cheap way to reduce error.

This chapter is mostly ideas and a small console program. No new renderer features, but the next two
chapters depend on it.

---

## 1. Estimating π with darts

Here's the most famous Monte Carlo experiment. Draw a square from −1 to 1 on each side, with a circle of
radius 1 inside. Throw darts randomly at the square:

```
 ┌──────────────────┐
 │ ·    ____    ·   │      area of square = 4
 │    ╱  ·   ╲   ·  │      area of circle = π · 1² = π
 │ · │  ·   · │     │
 │   │ ·   ·  │  ·  │      fraction of darts inside ≈ π / 4
 │ ·  ╲______╱   ·  │      so π ≈ 4 × (darts inside / all darts)
 └──────────────────┘
```

It works! And we never computed any curved shape. We only asked "is this point inside?" many times.

---

## 2. What is an integral?

An **integral** is a way to add up infinitely many infinitely small pieces: an **area under a curve**, a
**volume**, a **total amount**:

```
 f(x)
  │        ╱‾‾‾╲
  │      ╱       ╲             ∫ f(x) dx from a to b  =  the shaded area
  │    ╱▓▓▓▓▓▓▓▓▓▓▓╲
  │  ╱▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╲
  └──┴──────────────────┴── x
     a                  b
```

For simple functions you can compute integrals exactly with calculus. For example
`∫₀² x² dx = 8/3 ≈ 2.667`. For complicated ones (like "all the light arriving at this pixel") you can't.

### 2.1 Rendering is an integral

The color of a pixel is the **average** of the light arriving through every point of the pixel, over every
point of the lens, over the whole time the shutter is open. And the light leaving a surface point is the
**sum** of light arriving from **every direction** of the hemisphere above it, weighted by the material.
In 1986 James Kajiya wrote this down as the **rendering equation**:

```
L_out(p, ω_o) = L_emit(p, ω_o) + ∫ f(p, ω_i, ω_o) · L_in(p, ω_i) · cos θ_i  dω_i
                                  hemisphere
```

In words: *the light leaving point p toward the viewer = the light p emits itself + for every incoming
direction ω_i: (how much the material sends from ω_i toward the viewer) × (light arriving from ω_i) ×
(cos of its angle to the normal).*

* `f` is the material's **BRDF** (bidirectional reflectance distribution function). For a Lambertian surface
  it's constant: `albedo / π`.
* `L_in` is itself `L_out` of whatever point is visible in direction ω_i. So the equation is **recursive**,
  which is exactly why our path tracer bounces.

You don't need to memorize it. Just know that **our path tracer is a Monte Carlo estimate of this
integral**: each path is one random sample.

---

## 3. Monte Carlo integration

The method is simple:

1. Pick N random points `x₁ … x_N` uniformly in the domain `[a, b]`.
2. Evaluate the function at each: `f(xᵢ)`.
3. The integral ≈ **(width of the domain) × (average of the f values)**:

```
∫ f(x) dx  ≈  (b − a) · (1/N) · Σ f(xᵢ)
```

Why? The integral is the area under the curve; the area equals width × average height; and the
average of random samples estimates the average height.

For `∫₀² x² dx`: pick x in [0, 2], average x², multiply by 2. The chapter program does exactly this.

### 3.1 How fast does it converge?

The **error** of a Monte Carlo estimate shrinks like `1/√N`:

| N | expected error |
|---|----------------|
| 100 | 10% |
| 10,000 | 1% |
| 1,000,000 | 0.1% |

To gain one more correct digit you need **100×** more samples. That's slow, but:

* it works for **any** function, however complicated, and in any number of dimensions,
* the error doesn't depend on the number of dimensions. A pixel integral over position, lens, time and 10
  bounces of directions is a ~25-dimensional integral! Grid-based methods would need something like 10²⁵
  samples.

That's why film rendering is Monte Carlo.

In images, the error appears as **noise**. "Noise" in a render is Monte Carlo error.

### 3.2 Variance

The noise level is measured by the **variance**: how much individual samples differ from the true
average. Samples that are all close to the true value → low variance → little noise. Samples that are usually 0
and occasionally huge (like the Cornell box floor rays that occasionally hit the light) → high
variance → lots of noise.

**All noise reduction techniques are about reducing variance.** The next two chapters reduce it by choosing
samples more cleverly.

---

## 4. Stratification

Random points clump and leave gaps (chapter 16). If we divide the domain into a grid and put one random
sample in each cell (**stratified** or **jittered** sampling), the samples are spread evenly and the error
shrinks faster, close to `1/N` for smooth functions in 1–2 dimensions.

The catch: in high dimensions the benefit fades (a grid in 25 dimensions has too many cells), so
renderers stratify the most important dimensions (the pixel area, the lens) and use plain random numbers for the rest.
Our camera stratifies the pixel area.

---

## 5. The program

**File: `chapters/ch29_monte_carlo.cpp`**

```cpp
// ch29_monte_carlo.cpp
// ------------------------------------------------------------
// Chapter 29: Monte Carlo - answering questions with random numbers.
//  1. Estimate pi by throwing darts at a square     (console + image)
//  2. Watch the error shrink as N grows             (console)
//  3. Stratified (jittered) darts converge faster   (console)
//  4. Estimate an integral:  area under x^2 on [0,2] = 8/3
//   images/ch29_darts.png
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1 + 2. Plain Monte Carlo estimate of pi --------------------
    std::printf("Estimating pi with random darts in the square [-1,1]^2:\n");
    std::printf("%12s %14s %12s\n", "darts", "estimate", "error");
    long long inside = 0, total = 0;
    for (long long n = 10; n <= 10000000; n *= 10) {
        while (total < n) {
            double x = random_double(-1, 1), y = random_double(-1, 1);
            if (x * x + y * y < 1) inside++;
            total++;
        }
        double estimate = 4.0 * inside / total;
        std::printf("%12lld %14.8f %12.8f\n", total, estimate, std::fabs(estimate - pi));
    }

    // ---------- 3. Stratified: one dart per grid cell ----------------------
    std::printf("\nStratified (one jittered dart per cell of a grid):\n");
    for (int grid = 10; grid <= 3000; grid *= 3) {
        long long in = 0;
        for (int i = 0; i < grid; i++)
            for (int j = 0; j < grid; j++) {
                double x = 2 * ((i + random_double()) / grid) - 1;
                double y = 2 * ((j + random_double()) / grid) - 1;
                if (x * x + y * y < 1) in++;
            }
        double estimate = 4.0 * in / ((double)grid * grid);
        std::printf("%12lld %14.8f %12.8f\n", (long long)grid * grid, estimate, std::fabs(estimate - pi));
    }

    // ---------- 4. An integral ---------------------------------------------
    // Integral of x^2 from 0 to 2. Average of f(x) at random x, times the width (2).
    std::printf("\nIntegral of x^2 on [0,2] (exact = 2.66666667):\n");
    for (int n = 10; n <= 1000000; n *= 10) {
        double sum = 0;
        for (int i = 0; i < n; i++) {
            double x = random_double(0, 2);
            sum += x * x;
        }
        std::printf("%12d %14.8f\n", n, 2.0 * sum / n);
    }

    // ---------- Picture of the darts ---------------------------------------
    const int S = 500;
    Image img(S, S, hex_color(0x0F172A));
    Canvas cv(img);
    cv.draw_circle(S / 2, S / 2, S / 2 - 20, hex_color(0x94A3B8));
    cv.draw_rect(20, 20, S - 40, S - 40, hex_color(0x94A3B8));
    Pcg32 rng(3);
    for (int i = 0; i < 2000; i++) {
        double x = rng.next_double() * 2 - 1, y = rng.next_double() * 2 - 1;
        bool in = x * x + y * y < 1;
        double px = 20 + (x + 1) * 0.5 * (S - 40), py = 20 + (y + 1) * 0.5 * (S - 40);
        cv.fill_circle_aa(px, py, 2.2, in ? hex_color(0xF43F5E) : hex_color(0x38BDF8));
    }
    save_image("images/ch29_darts.png", img);
    return 0;
}
```

```bat
run ch29_monte_carlo
```

### What you should see (console)

The numbers below are an **illustrative example**. Your exact digits will differ, but the pattern will be the same:

```
Estimating pi with random darts in the square [-1,1]^2:
       darts       estimate        error
          10     3.60000000   0.45840735
         100     3.12000000   0.02159265
        1000     3.14800000   0.00640735
       10000     3.14280000   0.00120735
      100000     3.14332000   0.00172735
     1000000     3.14171600   0.00012335
    10000000     3.14166880   0.00007615

Stratified (one jittered dart per cell of a grid):
         100     3.16000000   0.01840735
         900     3.14222222   0.00062957
        8100     3.14172840   0.00013575
       72900     3.14161454   0.00002189
      656100     3.14159786   0.00000521
     5904900     3.14159347   0.00000082

Integral of x^2 on [0,2] (exact = 2.66666667):
          10     2.37000000
         100     2.61000000
        ...
     1000000     2.66688000
```

Notice:

* The plain estimate improves slowly and **not steadily** (sometimes more samples give a slightly worse
  number by chance), but the *trend* is 1/√N.
* The stratified estimate reaches in ~6 million samples an accuracy the plain method would need billions for.

### What you should see (image)

![Darts](../images/ch29_darts.png)

> **Image description:** A dark square with a grey circle outline inscribed in a grey square outline.
> 2000 small dots are scattered uniformly: **pink-red** dots inside the circle and **light blue** dots in the
> corners outside it. About 78.5% of the dots are red.

---

## Try it yourself

1. Estimate the area of a heart shape or an ellipse with darts.
2. Estimate `∫₀^π sin(x) dx` (exact: 2).
3. Estimate the **volume** of a unit sphere with random points in a cube (exact: 4π/3 ≈ 4.18879).
4. Run the π estimate 10 times with N = 1000 and write down the results. How spread out are they? Now with
   N = 100,000. Is the spread about 10× smaller? (It should be: √100 = 10.)

## Common problems

| Symptom | Cause |
|---------|-------|
| Estimate always exactly the same | Seeded identically every run (that's fine for reproducibility) |
| Estimate way off | Forgot to multiply by the domain size |
| Stratified worse than plain | Cells not covering the domain evenly, or sample not jittered inside its cell |

---

## Summary

* An integral is a sum over infinitely many pieces; rendering is a big, high-dimensional integral (the
  rendering equation).
* Monte Carlo: average random samples of f, times the domain size. Error ∝ 1/√N, in any dimension.
* Noise = variance. Reducing variance is the name of the game.
* Stratification spreads samples evenly for lower error.

Next: [Chapter 30 — Importance sampling →](30-importance-sampling.md)
