# Line by line: `include/pixel/noise.h`

[← Line-by-line index](README.md) · [Chapter 11 (the theory)](../11-procedural-noise.md)

**What this file does, in one sentence:** it makes **smooth randomness** ("noise") used to create natural-looking
things: clouds, mountains, marble, smoke, terrain.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–13 | tools, namespace |
| B. Hash functions | 15–27 | turn whole-number coordinates into random-looking numbers |
| C. 2D value noise | 29–38 | smooth random hills |
| D. 2D fBm | 40–50 | many layers of noise = natural detail |
| E. Perlin: creating it | 52–68 | random gradients and shuffled tables |
| F. Perlin: `noise` | 70–86 | the smooth 3D noise value at a point |
| G. Perlin: `turbulence` and `fbm` | 88–111 | layered versions |
| H. Perlin: private helpers | 113–144 | tables, shuffle, blending |
| I. End | 146 | |

---

## Block A — Comments, includes (lines 1–13)

* Lines 1–6: comments.
* Line 7: `#pragma once`.
* Lines 8–9: `<cmath>` (floor, fabs), `<cstdint>` (uint32_t, uint64_t).
* Lines 10–11: our `vec3.h` (Vec3, lerpd, dot) and `random.h` (Pcg32).
* Line 13: `namespace pixel`.

---

## Block B — Hash functions (lines 15–27)

For noise we need random values that are **always the same for the same position**. A random generator gives
new numbers every time. A **hash** gives "random-looking" numbers from an input, and always the same one for the
same input.

### Lines 16–21: scramble one number

```cpp
inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
```

* Each step mixes the bits:
  * `x ^= x >> 16`: XOR the number with a shifted copy of itself, so high bits affect low bits.
  * `x *= 0x7feb352dU`: multiply by a big odd number, so low bits affect high bits. (Numbers wrap around at 2³².)
* After a few rounds, changing the input by 1 changes about half of the output bits. The result looks random.
* These specific constants are known to mix very well (a published "hash prospector" result).

### Lines 22–25: hash of 2 or 3 coordinates

```cpp
inline uint32_t hash2(int x, int y) { return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u)); }
inline uint32_t hash3(int x, int y, int z) {
    return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u ^ hash_u32((uint32_t)z * 83492791u)));
}
```

* Combine several coordinates into one scrambled number: multiply each by a different large prime number, mix them
  with XOR `^` and `hash_u32`.
* `(uint32_t)x` turns a possibly negative int into an unsigned number, so negative coordinates work too.
* So `hash2(12, 34)` is always the same number, but `hash2(12, 35)` is completely different.

### Line 27: a fraction 0–1

```cpp
inline double hash2_01(int x, int y) { return hash2(x, y) * (1.0 / 4294967296.0); }
```

Divide by 2³² to get a fraction from 0 to (almost) 1, like `next_double` in the random generator.

---

## Block C — 2D value noise (lines 29–38)

**Idea:** give every whole-number grid point a random value (0–1), and for points **between** grid points, blend the
four surrounding values smoothly.

```cpp
inline double value_noise_2d(double x, double y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    double tx = x - xi, ty = y - yi;
```

* Line 32: `xi`, `yi` = the grid point to the lower-left of (x, y) (floor rounds down; also correct for negatives).
* Line 33: `tx`, `ty` = where we are inside the grid cell, 0–1.

```cpp
    double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);  // smoothstep
```

Line 34: bend `tx` and `ty` with the smoothstep curve `t²(3 − 2t)`. Plain linear blending would leave visible sharp
"creases" along grid lines; smoothstep makes the change start and end gently.

```cpp
    double v00 = hash2_01(xi, yi),     v10 = hash2_01(xi + 1, yi);
    double v01 = hash2_01(xi, yi + 1), v11 = hash2_01(xi + 1, yi + 1);
    return lerpd(lerpd(v00, v10, sx), lerpd(v01, v11, sx), sy);   // 0..1
}
```

* Lines 35–36: the random values at the 4 corners of the cell.
* Line 37: blend: first along x (the two bottom corners, the two top corners), then along y. The result is 0–1.

---

## Block D — 2D fBm (lines 40–50)

**Idea:** nature has big shapes, medium shapes and tiny details. Add several **octaves** (layers) of noise: each
one twice as detailed and half as strong.

```cpp
inline double fbm_2d(double x, double y, int octaves = 5, double lacunarity = 2.0, double gain = 0.5) {
    double sum = 0.0, amp = 0.5, norm = 0.0;
```

* Line 41: inputs: position, number of layers (default 5), **lacunarity** = how much more detailed each layer is
  (×2), **gain** = how much weaker each layer is (×0.5).
* Line 42: `sum` = total, `amp` = the current layer's strength, `norm` = the sum of all strengths (for scaling back
  to 0–1).

```cpp
    for (int i = 0; i < octaves; i++) {
        sum += amp * value_noise_2d(x, y);
        norm += amp;
        x *= lacunarity; y *= lacunarity;
        amp *= gain;
    }
    return sum / norm;   // 0..1
}
```

* Line 43: for each layer.
* Line 44: add this layer's noise, times its strength.
* Line 45: remember the strength.
* Line 46: multiply the coordinates by 2: the next layer's hills are **half as wide** (more detail).
* Line 47: the next layer is half as strong.
* Line 49: divide by the total strength so the result stays 0–1.

---

## Block E — Perlin: creating it (lines 52–68)

**Perlin noise** (Ken Perlin, 1983) is smoother and more natural than value noise. At every grid corner it stores a
random **direction** (a gradient), not a value. It's 3D, so it works for solid objects.

```cpp
class Perlin {
public:
    explicit Perlin(uint64_t seed = 42) {
        Pcg32 rng(seed);
```

* Line 53: a **class**: like a struct, but members are private unless marked `public:`.
* Line 55: the constructor, with an optional seed (different seeds → different noise patterns).
* Line 56: our random generator (from `random.h`), seeded.

```cpp
        for (int i = 0; i < point_count; i++) {
            Vec3 v;
            do {
                v = Vec3(rng.next_double() * 2 - 1, rng.next_double() * 2 - 1, rng.next_double() * 2 - 1);
            } while (v.length_squared() > 1.0 || v.length_squared() < 1e-6);
            randvec[i] = unit_vector(v);
        }
```

* Line 57: make 256 random directions.
* Lines 60–62: a **do-while** loop (runs at least once): pick a random point in the cube −1..1 and try again if it's
  outside the ball or almost at the center (the same rejection idea as `random_unit_vector`).
* Line 63: make it length 1 and store it.

```cpp
        generate_perm(perm_x, rng);
        generate_perm(perm_y, rng);
        generate_perm(perm_z, rng);
    }
```

Lines 65–67: make three **shuffled lists** of the numbers 0–255 (one for each axis). They are used to pick a
"random" gradient for each grid corner (see line 82).

---

## Block F — Perlin: `noise` (lines 70–86)

```cpp
    double noise(const Point3& p) const {
        double u = p.x - std::floor(p.x);
        double v = p.y - std::floor(p.y);
        double w = p.z - std::floor(p.z);
        int i = (int)std::floor(p.x);
        int j = (int)std::floor(p.y);
        int k = (int)std::floor(p.z);
```

* Lines 72–74: `u, v, w` = the position inside the grid cube, each 0–1.
* Lines 75–77: `i, j, k` = the cube's corner with the smallest coordinates.

```cpp
        Vec3 c[2][2][2];
        for (int di = 0; di < 2; di++)
            for (int dj = 0; dj < 2; dj++)
                for (int dk = 0; dk < 2; dk++)
                    c[di][dj][dk] = randvec[perm_x[(i + di) & 255] ^
                                            perm_y[(j + dj) & 255] ^
                                            perm_z[(k + dk) & 255]];
        return interpolate(c, u, v, w);
    }
```

* Line 78: a 2×2×2 array for the gradients at the cube's **8 corners**.
* Lines 79–81: loop over the 8 corners (di, dj, dk are each 0 or 1).
* Lines 82–84: pick the corner's gradient:
  * `(i + di) & 255` = the corner's x coordinate wrapped to 0–255 (`& 255` keeps the lowest 8 bits; this also works
    for negative numbers).
  * look it up in the shuffled list for x, same for y and z, and XOR the three results → an index 0–255.
  * The same corner always gets the same gradient, and neighboring corners get unrelated ones.
* Line 85: blend the 8 corners' contributions (block H). The result is roughly −1 to 1.

---

## Block G — Perlin: `turbulence` and `fbm` (lines 88–111)

### Lines 89–98: turbulence

```cpp
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
```

* The same octave idea as fBm (block D): `depth` layers, each twice as detailed (`temp *= 2`) and half as strong
  (`weight *= 0.5`).
* Line 97: take the absolute value of the sum. This makes a swirly, smoky look (used for marble veins).

### Lines 101–111: fbm (3D)

```cpp
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
```

The same as `fbm_2d`, in 3D with Perlin noise. Line 108 uses 2.03 instead of 2 so the layers' grids don't line up
exactly (lining up can show visible patterns). Result: roughly −1 to 1.

---

## Block H — Perlin: private helpers (lines 113–144)

```cpp
private:
    static const int point_count = 256;
    Vec3 randvec[point_count];
    int perm_x[point_count];
    int perm_y[point_count];
    int perm_z[point_count];
```

* Line 113: `private:` = only code inside the class can use what follows.
* Line 114: the table size, 256, as a constant shared by all Perlin objects (`static`).
* Line 115: the 256 random directions.
* Lines 116–118: the three shuffled lists.

### Lines 120–126: shuffle (Fisher–Yates)

```cpp
    static void generate_perm(int* p, Pcg32& rng) {
        for (int i = 0; i < point_count; i++) p[i] = i;
        for (int i = point_count - 1; i > 0; i--) {     // Fisher-Yates shuffle
            int target = (int)(rng.next_double() * (i + 1));
            int tmp = p[i]; p[i] = p[target]; p[target] = tmp;
        }
    }
```

* Line 121: fill with 0, 1, 2, ... 255.
* Lines 122–125: the classic fair shuffle: go from the end to the start; swap each element with a random element at
  or before it. (`tmp` holds one value while swapping.) Every order is equally likely.

### Lines 128–143: blend the 8 corners

```cpp
    static double interpolate(const Vec3 c[2][2][2], double u, double v, double w) {
        double uu = u * u * (3 - 2 * u);
        double vv = v * v * (3 - 2 * v);
        double ww = w * w * (3 - 2 * w);
        double accum = 0.0;
```

* Lines 129–131: smoothstep of u, v, w (smooth blending, like value noise).
* Line 132: the total.

```cpp
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
```

For each of the 8 corners:

* Line 136: `weight_v` = the vector from this corner to our point.
* Line 140: `dot(gradient, that vector)` = the corner's **contribution**: how far "uphill" our point is along the
  corner's random slope. At the corner itself this is 0, which is why Perlin noise is 0 at every grid point.
* Lines 137–139: the blend weight of this corner: for the x part, `uu` if the corner is on the far side (i = 1), or
  `1 − uu` if it's on the near side (i = 0). The same for y and z; multiply the three.
* Line 142: the sum of all 8 weighted contributions.
* Line 144: end of the class.

---

## Block I — End (line 146)

`} // namespace pixel`

---

## Check your understanding

1. Why use a hash instead of `random_double()` for noise? *(The same position must always give the same value.)*
2. What does smoothstep avoid? *(Sharp creases along grid lines.)*
3. In fBm, what do lacunarity 2 and gain 0.5 mean? *(Each layer is twice as detailed and half as strong.)*
4. What is Perlin noise exactly at a grid corner? *(0.)*
