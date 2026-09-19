# Chapter 16 — Random numbers and anti-aliasing

[← Normals & many objects](15-normals-and-lists.md) · [Contents](README.md) · [Next: Diffuse materials →](17-diffuse-materials.md)

---

## Goal

* Build our own **random number generator** (PCG32), fast, high quality and reproducible.
* Generate random points and directions (in a square, a disk, a sphere).
* **Anti-alias** the ray tracer by shooting many randomly placed rays per pixel and averaging them.
* Understand **stratified** sampling and the "1/√N" rule for noise.

Randomness is the engine of everything that follows: soft shadows, matte surfaces, glossy metals, depth
of field, motion blur, fog. All of them are "shoot random rays and average".

---

## 1. Why our own random numbers?

C has `rand()`, and C++ has `<random>`. We write our own because:

* `rand()` is low quality and not safe to call from several threads at once.
* `<random>` engines like `std::mt19937` are good but big and slow to seed, and it's harder to control
  exactly which numbers each pixel gets.
* We want **reproducible** renders: the same image every run, whatever the number of threads.
* It's 20 lines, and understanding it is fun.

### 1.1 Pseudo-randomness

Computers are deterministic. A *pseudo*-random generator keeps a hidden **state** number, and each call
scrambles the state into a new one and outputs something derived from it. With the same starting state
(the **seed**) you get the same sequence, which is great for debugging.

### 1.2 PCG32

**PCG** ("Permuted Congruential Generator", by Melissa O'Neill, 2014) has two steps:

1. **Advance** the state with a *linear congruential generator*, the oldest trick in the book:
   ```
   state = state × 6364136223846793005 + inc          (mod 2⁶⁴, which is automatic with uint64_t)
   ```
   This alone produces numbers with poor low bits.

2. **Permute** the output: shift and XOR the old state, then **rotate** it by an amount taken from its
   own top bits. This scrambling fixes the weaknesses and passes demanding statistical tests.

```cpp
uint32_t next_u32() {
    uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}
```

To get a `double` in `[0, 1)`, divide by 2³² = 4294967296.

### 1.3 One generator per thread

In chapter 21 several threads render at once. If they shared one generator they'd fight over its state
(a *data race*, which gives wrong results or crashes). So each thread gets its own:

```cpp
inline Pcg32& thread_rng() {
    static thread_local Pcg32 rng;     // one per thread, created on first use
    return rng;
}
```

The camera re-seeds it at the start of each image row, based on the row number. That's why a render is
identical whether you use 1 thread or 16.

---

## 2. Random points and directions

### 2.1 Rejection sampling

How do you pick a random point **inside a sphere** of radius 1, with every point equally likely? The
simplest correct method: pick a random point in the cube around it, and if it's outside the sphere,
**try again**:

```
┌─────────────┐
│ ×   ___   · │     × = rejected (outside the circle)
│   ╱  ·  ╲   │     · = accepted
│  │ ·   ·  │ │
│   ╲_____╱ × │     the circle covers π/4 ≈ 78.5% of the square (52% for sphere/cube in 3D),
│ ·         × │     so on average we need about 2 tries
└─────────────┘
```

Dividing an accepted point by its length gives a uniformly random **direction** (a point on the
sphere's surface): `random_unit_vector()`. We reject tiny vectors so we never divide by (almost)
zero.

### 2.2 The helpers in `random.h`

| Function | Returns |
|----------|---------|
| `random_double()` | uniform in [0, 1) |
| `random_double(a, b)` | uniform in [a, b) |
| `random_int(a, b)` | integer in [a, b] |
| `random_unit_vector()` | uniform direction |
| `random_on_hemisphere(n)` | uniform direction on the side of `n` |
| `random_in_unit_disk()` | point in the disk of radius 1 (for camera lenses) |
| `random_cosine_direction()` | direction around +z, favoring the top (chapter 30) |

**File: `include/pixel/random.h`**

```cpp
// pixel/random.h
// ------------------------------------------------------------
// Our own random number generator (PCG32) and helpers that
// produce random points/directions. No <random> needed.
// Explained in docs/16-random-and-antialiasing.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cmath>
#include "vec3.h"

namespace pixel {

// PCG32: a small, fast, high quality generator by Melissa O'Neill.
// State is 64 bits, output is 32 bits.
struct Pcg32 {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    Pcg32() {}
    Pcg32(uint64_t seed, uint64_t sequence = 1) { seed_with(seed, sequence); }

    void seed_with(uint64_t seed, uint64_t sequence = 1) {
        state = 0;
        inc = (sequence << 1u) | 1u;   // must be odd
        next_u32();
        state += seed;
        next_u32();
    }

    uint32_t next_u32() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    // Uniform double in [0, 1)
    double next_double() { return next_u32() * (1.0 / 4294967296.0); }
};

// Each thread gets its own generator, so threads never fight over it.
inline Pcg32& thread_rng() {
    static thread_local Pcg32 rng;
    return rng;
}

inline void seed_thread_rng(uint64_t seed) { thread_rng().seed_with(seed, seed * 2 + 1); }

// Random real number in [0,1).
inline double random_double() { return thread_rng().next_double(); }

// Random real number in [min,max).
inline double random_double(double min, double max) { return min + (max - min) * random_double(); }

// Random integer in [min,max] (both included).
inline int random_int(int min, int max) {
    int v = (int)random_double(min, max + 1.0);
    return v > max ? max : v;
}

inline Vec3 random_vec() { return Vec3(random_double(), random_double(), random_double()); }
inline Vec3 random_vec(double min, double max) {
    return Vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

// Uniform random direction (a point on the unit sphere).
inline Vec3 random_unit_vector() {
    while (true) {
        Vec3 p = random_vec(-1, 1);
        double lensq = p.length_squared();
        if (1e-160 < lensq && lensq <= 1.0) return p / std::sqrt(lensq);
    }
}

// Random direction in the hemisphere around the normal.
inline Vec3 random_on_hemisphere(const Vec3& normal) {
    Vec3 on_unit_sphere = random_unit_vector();
    return dot(on_unit_sphere, normal) > 0.0 ? on_unit_sphere : -on_unit_sphere;
}

// Random point inside a disk of radius 1 in the xy plane (for camera lenses).
inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() < 1.0) return p;
    }
}

// Cosine-weighted random direction around +z (see docs/30-importance-sampling.md).
inline Vec3 random_cosine_direction() {
    double r1 = random_double();
    double r2 = random_double();
    double phi = 2.0 * pi * r1;
    double x = std::cos(phi) * std::sqrt(r2);
    double y = std::sin(phi) * std::sqrt(r2);
    double z = std::sqrt(1.0 - r2);
    return Vec3(x, y, z);
}

} // namespace pixel
```

---

## 3. Anti-aliasing a ray tracer

### 3.1 The problem, again

In chapter 14 each pixel shot **one ray through its center**. That's exactly the "hard test" from
chapter 9: jagged edges.

### 3.2 The fix: many rays per pixel

Shoot **N rays per pixel**, each through a *random point inside the pixel square*, and average the
colors. An edge pixel half covered by the sphere gets about half sphere rays and half sky rays, so its
color is correctly in between.

```
┌───────────────┐
│  ·      ·     │     random sample positions inside one pixel
│     ·  ╱  ·   │     rays to the left of the edge see the sphere,
│  ·    ╱   ·   │     rays to the right see the sky,
│   · ╱   ·     │     average = partial coverage
└───────────────┘
```

```cpp
Color sum(0, 0, 0);
for (int s = 0; s < samples_per_pixel; s++) {
    double ox = random_double() - 0.5;          // offset in [-0.5, 0.5)
    double oy = random_double() - 0.5;
    Point3 target = pixel00 + (i + ox) * du + (j + oy) * dv;
    sum += ray_color(Ray(eye, target - eye), world);
}
img.at(i, j) = sum / samples_per_pixel;
```

This is **Monte Carlo** estimation (chapter 29): the true pixel color is the average over the whole
pixel square, and we estimate it with random samples.

### 3.3 How many samples? The 1/√N rule

With random samples the result is *noisy*: two runs give slightly different edge pixels. The error
shrinks with the number of samples N, but slowly:

```
error ∝ 1 / √N

  N:       1     4     16     64     256    1024
  error:   1    1/2   1/4    1/8    1/16    1/32
```

**To halve the noise you need 4× as many samples**, and therefore 4× the render time. This rule rules
the life of every rendering engineer and explains why film frames take hours.

### 3.4 Stratified sampling: a free improvement

Purely random samples can clump together and leave gaps. **Stratified** (or "jittered") sampling
divides the pixel into a grid and puts **one random sample in each cell**:

```
   random (16 samples)        stratified (4 × 4 grid)
┌─────────────────┐        ┌────┬────┬────┬────┐
│ ··      ·       │        │ ·  │  · │ ·  │   ·│
│  ·   ·      ·   │        ├────┼────┼────┼────┤
│      ··         │        │  · │·   │  · │ ·  │
│ ·        ·    · │        ├────┼────┼────┼────┤
│    ·   ·    ·   │        │ ·  │  · │   ·│ ·  │
│                 │        ├────┼────┼────┼────┤
│   ·        ·    │        │  · │ ·  │ ·  │  · │
└─────────────────┘        └────┴────┴────┴────┘
  clumps and gaps            evenly spread, still random
```

Same cost, noticeably less noise, especially at edges. The library `Camera` (next chapter) uses it
automatically when samples_per_pixel has a whole square root (4, 16, 64, 100...).

---

## 4. The program

**File: `chapters/ch16_antialiasing.cpp`**

```cpp
// ch16_antialiasing.cpp
// ------------------------------------------------------------
// Chapter 16: Random numbers and anti-aliasing.
// Same scene as chapter 15, rendered with 1 and with 64 samples per pixel.
// We zoom into the edge of the sphere to see the difference.
//   images/ch16_aa_compare.png   left: 1 sample, right: 64 samples (zoomed 6x)
//   images/ch16_random_pixels.png  (a picture of our random number generator)
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) return 0.5 * (rec.normal + Color(1, 1, 1));
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

Image render(const Hittable& world, int samples_per_pixel) {
    const int W = 400, H = 225;
    const double vw = 2.0 * W / H;
    const Point3 eye(0, 0, 0);
    const Vec3 du(vw / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(vw / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Color sum(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; s++) {
                // A random point inside the pixel square instead of its exact center.
                double ox = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
                double oy = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
                Point3 target = pixel00 + (i + ox) * du + (j + oy) * dv;
                sum += ray_color(Ray(eye, target - eye), world);
            }
            img.at(i, j) = sum / samples_per_pixel;   // the average of all samples
        }
    return img;
}

// Cut out a rectangle and enlarge it with square pixels.
Image crop_zoom(const Image& src, int x0, int y0, int w, int h, int factor) {
    Image out(w * factor, h * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x0 + x / factor, y0 + y / factor);
    return out;
}

int main() {
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));

    Image one = render(world, 1);
    Image many = render(world, 64);

    // The sphere's upper-right edge lives around pixel (250, 70).
    Image left = crop_zoom(one, 230, 50, 50, 50, 6);
    Image right = crop_zoom(many, 230, 50, 50, 50, 6);
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch16_aa_compare.png", post::side_by_side(left, right, 10), raw);
    save_image("images/ch16_full_64spp.png", many, raw);

    // What do random numbers look like? Each pixel is a random grey.
    Image rnd(256, 128);
    for (auto& c : rnd.data) { double v = random_double(); c = Color(v, v, v); }
    save_image("images/ch16_random_pixels.png", rnd, raw);
    return 0;
}
```

We render the chapter 15 scene twice (1 sample and 64 samples per pixel), crop the same 50 × 50 pixel
area at the sphere's upper-right edge, and enlarge both crops 6× with square pixels.

```bat
run ch16_antialiasing
```

## 5. What you should see

![AA compare](../images/ch16_aa_compare.png)

> **Image description:** Two large square crops side by side (separated by a white bar), showing the
> edge of the rainbow-colored normal sphere against the pale blue sky, magnified so each pixel is a
> visible block. **Left (1 sample):** the edge is a hard staircase: each block is either sphere color
> or sky color. **Right (64 samples):** the edge blocks have in-between colors, so from a distance the
> edge looks smooth and round.

![Full 64 spp](../images/ch16_full_64spp.png)

> **Image description:** The chapter 15 scene (pastel normal-colored sphere on the pale green ground,
> blue-white sky), now with perfectly smooth edges everywhere.

![Random pixels](../images/ch16_random_pixels.png)

> **Image description:** A 256 × 128 rectangle of grey static: every pixel a random brightness. There
> should be no visible pattern, stripes or repetition. If you ever see patterns in such an image, your
> random generator is broken.

---

## Try it yourself

1. Try 4, 16 and 256 samples. Can you still see a difference between 64 and 256?
2. Replace random offsets with a fixed 4×4 grid of offsets (no randomness). Compare with random: fixed
   grids can show regular artifacts on nearly-horizontal edges.
3. Print the first 5 numbers of `Pcg32 rng(42)` twice. They're identical. Now seed with 43.
4. Estimate π: pick random points in a square, count how many land in the circle (chapter 29 does this
   properly).

## Common problems

| Symptom | Cause |
|---------|-------|
| Image identical with 1 and 64 samples | Offsets are always 0: forgot to add `random_double() - 0.5` |
| Image too bright/dark with many samples | Forgot to divide the sum by the number of samples |
| Visible repeating patterns in noise | Poor generator or re-seeding with the same seed too often |
| Different image every run | Seeding from the clock; seed deterministically for debugging |

---

## Summary

* PCG32: an LCG step plus a permutation. It's small, fast and high quality. One generator per thread.
* Rejection sampling gives uniform points in a sphere or disk, and uniform directions.
* Anti-aliasing = many random rays per pixel, averaged. Noise falls as 1/√N.
* Stratified sampling spreads samples evenly for free.

Next: [Chapter 17 — Diffuse materials →](17-diffuse-materials.md). The picture finally starts to look real.
