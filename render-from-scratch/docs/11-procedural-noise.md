# Chapter 11 — Procedural noise: nature from math

[← Triangles & gradients](10-triangles-gradients.md) · [Contents](README.md) · [Next: Vectors →](12-vectors.md)

---

## Goal

Nature is full of **controlled randomness**: clouds, mountains, marble, wood, smoke, water ripples. None of
it is a perfect shape, and none of it is pure chaos either. In this chapter you'll learn the tool that film
and game studios use to create these patterns from math:

* **hash functions**: randomness without memory,
* **value noise**: smooth random hills,
* **fractal Brownian motion (fBm)**: noise at many scales, which is what makes things look natural,
* **Perlin gradient noise** (in 3D, used later for textures, clouds and terrain).

Then we'll paint an entire landscape (sky, clouds, mountains, lake with reflections) using nothing but noise.

---

## 1. White noise: pure randomness

If every pixel gets an independent random value, you get **white noise**: TV static. It's random, but it
looks nothing like nature, because nature is *smooth at small scales and varied at large scales*.

### 1.1 Randomness from a hash

For noise we need random values that are **repeatable**: the value at position (12, 34) must be the same
every time we ask. We could store a big table of random numbers, but a neater trick is a **hash function**:
a function that scrambles its input so thoroughly that the output looks random.

```cpp
inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
```

Each line mixes the bits: XOR with a shifted copy, then multiply by a big odd constant. Changing one input
bit changes about half of the output bits (the "avalanche effect"). To hash 2D positions we combine x and y
with large primes (`hash2`), and to get a number in [0, 1) we divide by 2³².

---

## 2. Value noise: smooth randomness

### 2.1 The idea

1. Put a random value at every **integer grid point**: (0,0), (1,0), (0,1), ...
2. For any point between the grid points, **blend** the four surrounding values.

```
  v01 ●───────────● v11
      │           │
      │    · P    │      P = (x, y) between grid points
      │           │      blend v00, v10, v01, v11 by P's position in the cell
  v00 ●───────────● v10
```

That's bilinear interpolation again (chapter 5). But linear blending leaves visible creases at the grid
lines, like a crumpled sheet of paper. We fix it with a **smoothstep** curve, which eases in and out:

```
smoothstep(t) = t² (3 − 2t)

  1 ┤          ╭───
    │        ╭╯
    │      ╭╯          flat (zero slope) at t=0 and t=1,
    │    ╭╯            so neighboring cells join smoothly
  0 ┼───╯
    0          1
```

Scale the input to change the size of the "hills": `value_noise_2d(x / 32.0, y / 32.0)` puts a grid
point every 32 pixels.

---

## 3. Fractal Brownian motion (fBm)

### 3.1 Nature has detail at every scale

Look at a mountain range: big shapes (the mountains), medium shapes (ridges), small shapes (rocks),
tiny shapes (pebbles). Each smaller scale has **less height** than the one above it. The same holds for
coastlines, clouds and tree bark. Such patterns are called **fractal**.

### 3.2 Adding octaves

fBm builds this by adding several layers (**octaves**) of noise. Each one has double the frequency (half
the size) and half the amplitude (strength):

```
octave 1:  frequency 1,  amplitude 0.5      big, strong hills
octave 2:  frequency 2,  amplitude 0.25     smaller hills
octave 3:  frequency 4,  amplitude 0.125    bumps
octave 4:  frequency 8,  amplitude 0.0625   roughness
...
sum ──▶ natural-looking terrain
```

```cpp
double fbm_2d(double x, double y, int octaves, double lacunarity = 2.0, double gain = 0.5) {
    double sum = 0, amp = 0.5, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * value_noise_2d(x, y);
        norm += amp;
        x *= lacunarity; y *= lacunarity;   // higher frequency
        amp *= gain;                        // lower amplitude
    }
    return sum / norm;                      // back to 0..1
}
```

* **lacunarity** (usually 2): how much the frequency grows per octave.
* **gain** (usually 0.5): how much the amplitude shrinks. Higher gain = rougher.

---

## 4. Perlin noise (gradient noise)

Value noise has a weakness: its hills and valleys tend to sit on the grid points, which gives a
slightly "blobby", grid-aligned look. In 1983, while working on the film *Tron*, **Ken Perlin** invented a
better noise (and later won an Academy Award for it). Instead of random *values* at grid points, it
places random **gradients** (slopes, as direction vectors):

* each grid corner has a random direction,
* the noise at P blends, over the corners, `dot(gradient, P − corner)`: how far "uphill" P is from
  each corner along that corner's slope.

The result is zero at every grid point, with hills and valleys *between* grid points, and it looks
much more organic. Our `Perlin` class (in `noise.h`) is 3D, so it can texture solid objects (marble
through a sphere) and animate (use z as time). It includes:

* `noise(p)`: smooth noise in roughly [−1, 1],
* `turbulence(p)`: sum of |octaves|, which looks like smoke and fire,
* `fbm(p)`: fBm with Perlin noise.

We'll use it heavily from chapter 24 on. Here's the complete file:

**File: `include/pixel/noise.h`**

```cpp
// pixel/noise.h
// ------------------------------------------------------------
// Procedural noise: value noise, Perlin gradient noise, fBm and
// turbulence. Used for clouds, marble, terrain, film grain...
// Explained in docs/11-procedural-noise.md and docs/24-textures.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"
#include "random.h"

namespace pixel {

// Hash an integer into a pseudo-random 32 bit value (no state needed).
inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline uint32_t hash2(int x, int y) { return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u)); }
inline uint32_t hash3(int x, int y, int z) {
    return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u ^ hash_u32((uint32_t)z * 83492791u)));
}
// Random double in [0,1) from integer coordinates.
inline double hash2_01(int x, int y) { return hash2(x, y) * (1.0 / 4294967296.0); }

// ---------------- 2D value noise --------------------------------------------
// Random values at integer grid points, smoothly interpolated in between.
inline double value_noise_2d(double x, double y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    double tx = x - xi, ty = y - yi;
    double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);  // smoothstep
    double v00 = hash2_01(xi, yi),     v10 = hash2_01(xi + 1, yi);
    double v01 = hash2_01(xi, yi + 1), v11 = hash2_01(xi + 1, yi + 1);
    return lerpd(lerpd(v00, v10, sx), lerpd(v01, v11, sx), sy);   // 0..1
}

// Fractal Brownian motion: add several octaves of noise, each smaller and weaker.
inline double fbm_2d(double x, double y, int octaves = 5, double lacunarity = 2.0, double gain = 0.5) {
    double sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * value_noise_2d(x, y);
        norm += amp;
        x *= lacunarity; y *= lacunarity;
        amp *= gain;
    }
    return sum / norm;   // 0..1
}

// ---------------- 3D Perlin (gradient) noise --------------------------------
class Perlin {
public:
    explicit Perlin(uint64_t seed = 42) {
        Pcg32 rng(seed);
        for (int i = 0; i < point_count; i++) {
            // random unit vector
            Vec3 v;
            do {
                v = Vec3(rng.next_double() * 2 - 1, rng.next_double() * 2 - 1, rng.next_double() * 2 - 1);
            } while (v.length_squared() > 1.0 || v.length_squared() < 1e-6);
            randvec[i] = unit_vector(v);
        }
        generate_perm(perm_x, rng);
        generate_perm(perm_y, rng);
        generate_perm(perm_z, rng);
    }

    // Smooth noise in roughly [-1, 1].
    double noise(const Point3& p) const {
        double u = p.x - std::floor(p.x);
        double v = p.y - std::floor(p.y);
        double w = p.z - std::floor(p.z);
        int i = (int)std::floor(p.x);
        int j = (int)std::floor(p.y);
        int k = (int)std::floor(p.z);
        Vec3 c[2][2][2];
        for (int di = 0; di < 2; di++)
            for (int dj = 0; dj < 2; dj++)
                for (int dk = 0; dk < 2; dk++)
                    c[di][dj][dk] = randvec[perm_x[(i + di) & 255] ^
                                            perm_y[(j + dj) & 255] ^
                                            perm_z[(k + dk) & 255]];
        return interpolate(c, u, v, w);
    }

    // Sum of absolute noise at several scales: looks like turbulent smoke.
    double turbulence(const Point3& p, int depth = 7) const {
        double accum = 0.0, weight = 1.0;
        Point3 temp = p;
        for (int i = 0; i < depth; i++) {
            accum += weight * noise(temp);
            weight *= 0.5;
            temp *= 2.0;
        }
        return std::fabs(accum);
    }

    // fBm with Perlin noise, result roughly in [-1, 1].
    double fbm(const Point3& p, int octaves = 6, double gain = 0.5) const {
        double sum = 0.0, amp = 1.0, norm = 0.0;
        Point3 q = p;
        for (int i = 0; i < octaves; i++) {
            sum += amp * noise(q);
            norm += amp;
            amp *= gain;
            q = q * 2.03;   // not exactly 2, avoids visible repetition
        }
        return sum / norm;
    }

private:
    static const int point_count = 256;
    Vec3 randvec[point_count];
    int perm_x[point_count];
    int perm_y[point_count];
    int perm_z[point_count];

    static void generate_perm(int* p, Pcg32& rng) {
        for (int i = 0; i < point_count; i++) p[i] = i;
        for (int i = point_count - 1; i > 0; i--) {     // Fisher-Yates shuffle
            int target = (int)(rng.next_double() * (i + 1));
            int tmp = p[i]; p[i] = p[target]; p[target] = tmp;
        }
    }

    static double interpolate(const Vec3 c[2][2][2], double u, double v, double w) {
        double uu = u * u * (3 - 2 * u);
        double vv = v * v * (3 - 2 * v);
        double ww = w * w * (3 - 2 * w);
        double accum = 0.0;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    Vec3 weight_v(u - i, v - j, w - k);
                    accum += (i * uu + (1 - i) * (1 - uu)) *
                             (j * vv + (1 - j) * (1 - vv)) *
                             (k * ww + (1 - k) * (1 - ww)) *
                             dot(c[i][j][k], weight_v);
                }
        return accum;
    }
};

} // namespace pixel
```

Implementation notes:

* The 256 random gradients and three **permutation tables** are created once from a seeded `Pcg32`, so the
  same seed always gives the same noise.
* `perm_x[i & 255] ^ perm_y[j & 255] ^ perm_z[k & 255]` picks a pseudo-random gradient for corner (i,j,k).
  The `& 255` wraps coordinates, so the noise repeats every 256 units (far enough to never notice).
* `interpolate` uses smoothstep weights, like value noise.
* `fbm` multiplies coordinates by 2.03 instead of exactly 2, so that octaves don't line up and show
  repetition.

---

## 5. The landscape program

**File: `chapters/ch11_procedural_noise.cpp`**

```cpp
// ch11_procedural_noise.cpp
// ------------------------------------------------------------
// Chapter 11: Procedural noise - nature from math.
//   images/ch11_white_noise.png   - pure randomness (TV static)
//   images/ch11_value_noise.png   - smooth random hills
//   images/ch11_fbm.png           - fractal noise (octaves added)
//   images/ch11_landscape.png     - a whole landscape made from noise
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    const int S = 256;

    // ---------- 1. White noise: every pixel independent ---------------------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = hash2_01(x, y);
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_white_noise.png", img);
    }

    // ---------- 2. Value noise: random values on a grid, blended -----------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = value_noise_2d(x / 32.0, y / 32.0);   // grid cell = 32 pixels
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_value_noise.png", img);
    }

    // ---------- 3. fBm: 6 octaves ------------------------------------------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = fbm_2d(x / 64.0, y / 64.0, 6);
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_fbm.png", img);
    }

    // ---------- 4. A landscape painted only with noise ---------------------
    {
        const int W = 960, H = 540;
        Image img(W, H);
        const double horizon = 330;

        // (a) Sky with noise clouds.
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                double t = y / horizon;
                Color sky = lerp(hex_color(0x0F2027), hex_color(0xF8B195), std::pow(clamp01(t), 1.6));
                double cloud = fbm_2d(x / 180.0, y / 60.0 + 3.7, 6);
                cloud = smoothstep(0.52, 0.75, cloud) * (1.0 - t * 0.6);
                img.at(x, y) = lerp(sky, hex_color(0xF67280) * 0.9 + Color(0.1, 0.05, 0.1), cloud * 0.8);
            }
        Canvas cv(img);
        cv.glow(W * 0.62, horizon - 30, 300, hex_color(0xFFB347) * 0.8);
        cv.fill_circle_aa(W * 0.62, horizon - 30, 34, hex_color(0xFFF1C1));

        // (b) Mountain layers: each column's height comes from 1D fBm.
        struct Layer { double base, amp, freq; uint32_t color; double seed; };
        Layer layers[] = {
            {horizon - 40, 110, 1 / 260.0, 0x6C5B7B, 10.0},
            {horizon - 10, 90, 1 / 200.0, 0x4B3F6B, 20.0},
            {horizon + 5, 60, 1 / 140.0, 0x2A2344, 30.0},
        };
        for (const Layer& L : layers) {
            for (int x = 0; x < W; x++) {
                double h = fbm_2d(x * L.freq, L.seed, 6);
                double top = L.base - (h - 0.3) * L.amp * 2.0;
                for (int y = (int)std::max(0.0, std::floor(top)); y <= horizon && y < H; y++) {
                    double coverage = clamp01(y + 1 - top);            // anti-aliased top edge
                    double haze = clamp01((horizon - y) / 200.0) * 0.3; // aerial perspective
                    Color c = lerp(hex_color(L.color), hex_color(0xF8B195), haze);
                    cv.blend_pixel(x, y, c, coverage);
                }
            }
        }

        // (c) Lake: mirror the image above the horizon, darken, and ripple it.
        for (int y = (int)horizon + 1; y < H; y++) {
            double depth = (y - horizon) / (H - horizon);
            for (int x = 0; x < W; x++) {
                double ripple = (value_noise_2d(x / 40.0, y / 3.0) - 0.5) * 12.0 * depth;
                int sx = (int)clampd(x + ripple, 0, W - 1);
                int sy = (int)clampd(2 * horizon - y, 0, horizon);
                Color reflected = img.at(sx, sy) * (0.6 - 0.3 * depth);
                img.at(x, y) = lerp(reflected, hex_color(0x0B0F1A), depth * 0.6);
            }
        }
        save_image("images/ch11_landscape.png", img);
    }
    return 0;
}
```

The landscape is made in three steps:

**(a) Sky and clouds.** The sky color is a gradient from deep teal-navy to peach at the horizon, using `pow` to keep
the sky dark longer. Clouds are fBm stretched horizontally (`x / 180, y / 60`: clouds are wider than tall),
passed through `smoothstep(0.52, 0.75, ...)` to turn soft noise into distinct clouds with clear sky between
them, and faded out near the horizon.

**(b) Mountains.** For every column `x`, a 1D slice of fBm gives the mountain height. Three layers with
different seeds, frequencies and colors, drawn back to front. Each layer is mixed a little toward the sky color
the closer it is to the horizon, which imitates haze. The top edge is anti-aliased with the
coverage trick from chapter 9.

**(c) The lake.** For each pixel below the horizon we look up the mirrored pixel above it (`2·horizon − y`),
shifted sideways by a little value noise to make **ripples**. Ripples get stronger farther down (closer to
the viewer). Then we darken toward the bottom.

```bat
run ch11_procedural_noise
```

---

## 6. What you should see

![White noise](../images/ch11_white_noise.png) ![Value noise](../images/ch11_value_noise.png) ![fBm](../images/ch11_fbm.png)

> **Image descriptions (three 256×256 squares):**
> * **White noise:** grey TV static; every pixel an unrelated shade. Harsh and grainy.
> * **Value noise:** soft blurry blobs of light and dark grey, about 8 across the image. Smooth, but you
>   may notice a faint grid-like regularity.
> * **fBm:** the same big blobs, now with finer and finer detail on top. It looks like a cloudy sky
>   or a satellite photo of terrain: natural and detailed.

![Noise landscape](../images/ch11_landscape.png)

> **Image description:** A painterly dusk landscape, 960×540. The sky fades from dark blue-teal at
> the top to warm peach near the horizon, with scattered pinkish, wispy clouds. A pale sun with a big warm
> glow sits right of center, just above the mountains. Three ranges of soft, irregular mountains,
> lavender in the back, darker purple in the middle, deep indigo in front, overlap across the frame.
> The lower third is a calm lake reflecting the mountains and the sunset glow, darker and rippled
> with small horizontal distortions that grow toward the bottom.

Every part of this picture comes from a formula. That's the power of **procedural** content.

---

## Try it yourself

1. Change the number of fBm octaves from 6 to 1, 2, 3, 8. Watch the detail appear.
2. Change `gain` to 0.7 for rough, rocky mountains, or 0.3 for soft rolling hills.
3. **Ridged noise:** replace `h` with `1 − |2h − 1|` for sharp mountain ridges.
4. **Domain warping:** compute `q = fbm(x, y)` and then `fbm(x + 4q, y + 4q)`. This gives swirly,
   marble-like patterns, used for alien planets and gas giants.
5. Make the clouds move: add a `time` variable to the x coordinate and render several frames
   (chapter 39 shows how to make an animation).

## Common problems

| Symptom | Cause |
|---------|-------|
| Visible grid lines / creases | Using linear instead of smoothstep interpolation |
| Noise repeats obviously | Scale too large relative to the table size, or octaves aligned at exact ×2 |
| Everything looks the same brightness | Forgot to normalize fBm (divide by the sum of amplitudes) |
| Pattern changes every run | Using an unseeded random generator instead of a hash or a fixed seed |

---

## Summary

* A **hash** gives repeatable randomness from coordinates.
* **Value noise** blends random grid values with smoothstep.
* **fBm** adds octaves: doubling frequency, halving amplitude. That's the recipe for natural detail.
* **Perlin noise** uses random gradients and looks more organic; ours is 3D.

That's the end of **Part 2**. You can now draw anything in 2D. Time to add a dimension: [Chapter 12 —
Vectors →](12-vectors.md)
