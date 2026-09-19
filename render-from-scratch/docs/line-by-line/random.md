# Line by line: `include/pixel/random.h`

[← Line-by-line index](README.md) · [Chapter 16 (the theory)](../16-random-and-antialiasing.md)

**What this file does, in one sentence:** it makes **random numbers** with our own generator (PCG32), and
uses them to make random points and random directions.

Random numbers are used everywhere later: anti-aliasing, matte surfaces, depth of field, noise, fog...

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–12 | tools, namespace |
| B. `struct Pcg32` | 14–41 | the random number generator itself |
| C. One generator per thread | 43–49 | `thread_rng`, `seed_thread_rng` |
| D. Random numbers | 51–61 | `random_double`, `random_int` |
| E. Random vectors | 63–66 | `random_vec` |
| F. Random directions and points | 68–100 | unit sphere, hemisphere, disk, cosine |
| G. End | 102 | |

---

## Block A — Comments, includes (lines 1–12)

* Lines 1–6: comments.
* Line 7: `#pragma once`.
* Line 8 `<cstdint>`: exact-size numbers `uint32_t` (32 bits) and `uint64_t` (64 bits).
* Line 9 `<cmath>`: `std::sqrt`, `std::cos`, `std::sin`.
* Line 10 `"vec3.h"`: our `Vec3` (random vectors are Vec3s).
* Line 12: start `namespace pixel`.

---

## Block B — `struct Pcg32`, the generator (lines 14–41)

A computer can't make "real" random numbers by calculation. It makes **pseudo-random** numbers: a long sequence
that *looks* random, computed from a hidden number called the **state**. The same starting state (the **seed**)
always gives the same sequence. That's useful: a render is the same every time you run it.

### Lines 16–18: the hidden numbers

```cpp
struct Pcg32 {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;
```

* Line 17: `state` = the hidden 64-bit number. It changes every time we ask for a random number. The default value
  is just a fixed, random-looking starting number.
* Line 18: `inc` = "increment", a number added at every step. It chooses **which** of many possible sequences we
  get. It must be odd.
* `ULL` at the end = "unsigned long long", i.e. a 64-bit positive number (the literal is too big for a normal int).

### Lines 20–21: constructors

```cpp
    Pcg32() {}
    Pcg32(uint64_t seed, uint64_t sequence = 1) { seed_with(seed, sequence); }
```

* Line 20: create with the default state.
* Line 21: create from a **seed**: `Pcg32 rng(42);`. It calls `seed_with` (below). `sequence` is optional.

### Lines 23–29: `seed_with`: set up from a seed

```cpp
    void seed_with(uint64_t seed, uint64_t sequence = 1) {
        state = 0;
        inc = (sequence << 1u) | 1u;   // must be odd
        next_u32();
        state += seed;
        next_u32();
    }
```

* Line 24: start from 0.
* Line 25: `sequence << 1` doubles it (shift left by 1 bit), `| 1` sets the lowest bit, so `inc` is always **odd**.
* Lines 26–28: step once, mix in the seed, step again. These extra steps make sure nearby seeds (1, 2, 3...) give
  very different sequences. This is the official way PCG is seeded.

### Lines 31–37: `next_u32`: the heart of the generator

```cpp
    uint32_t next_u32() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
```

* Line 32: remember the current state.
* Line 33: move the state forward: multiply by a special big number and add `inc`. The result automatically wraps
  around at 2⁶⁴ (the number simply loses its highest bits). This step is called an **LCG** (linear congruential
  generator), a very old technique. On its own its output isn't random enough, so...

```cpp
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }
```

...we **scramble** the old state before returning it (the "P" in PCG = Permuted):

* Line 34: mix the high bits into the low bits (`old >> 18` then XOR `^` with itself), then take 32 bits from the
  middle (`>> 27`).
* Line 35: take the **top 5 bits** of the state (`old >> 59` leaves 5 bits: a number 0–31).
* Line 36: **rotate** the 32-bit number by that amount: bits pushed out on the right come back on the left. Because
  the rotation amount itself depends on the state, the output is very well mixed.
  (`& 31u` handles rot = 0 safely.)
* The result: a random-looking 32-bit number (0 to 4,294,967,295).

### Lines 39–40: a fraction between 0 and 1

```cpp
    // Uniform double in [0, 1)
    double next_double() { return next_u32() * (1.0 / 4294967296.0); }
};
```

* Divide the 32-bit number by 2³² = 4,294,967,296. The result is from 0.0 up to (but never exactly) 1.0.
* "Uniform" = every value is equally likely.
* `[0, 1)` = 0 included, 1 not included.
* Line 41: end of the struct.

---

## Block C — One generator per thread (lines 43–49)

```cpp
inline Pcg32& thread_rng() {
    static thread_local Pcg32 rng;
    return rng;
}
```

* Chapter 21 renders with several **threads** (several CPU cores at the same time). If they all shared one
  generator, they would change its state at the same moment and break it.
* Line 45: `static` = created once and kept. `thread_local` = **each thread gets its own copy**.
* Line 46: return a **reference** (`&`) to this thread's generator, so callers use the real one.

```cpp
inline void seed_thread_rng(uint64_t seed) { thread_rng().seed_with(seed, seed * 2 + 1); }
```

Line 49: re-seed the current thread's generator. The camera does this at the start of each image row, so the
picture is identical no matter which thread renders which row.

---

## Block D — Random numbers (lines 51–61)

```cpp
inline double random_double() { return thread_rng().next_double(); }
```

Line 52: a random fraction 0–1, from this thread's generator. **This is the function used everywhere.**

```cpp
inline double random_double(double min, double max) { return min + (max - min) * random_double(); }
```

Line 55: a random fraction between `min` and `max`. Start at `min`, add a random part of the range.
Example: `random_double(-1, 1)` → −1 + 2 × (0..1) → −1..1.

```cpp
inline int random_int(int min, int max) {
    int v = (int)random_double(min, max + 1.0);
    return v > max ? max : v;
}
```

* Line 59: a random fraction from `min` to `max + 1`, then cut to a whole number. For `random_int(1, 6)`: 1.0–6.99
  → 1, 2, 3, 4, 5, 6 (a dice roll).
* Line 60: safety against rounding giving `max + 1`.

---

## Block E — Random vectors (lines 63–66)

```cpp
inline Vec3 random_vec() { return Vec3(random_double(), random_double(), random_double()); }
inline Vec3 random_vec(double min, double max) {
    return Vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}
```

* Line 63: three random fractions 0–1: a random point in a unit cube, or a random color.
* Lines 64–66: the same with your own range for each part.

---

## Block F — Random directions and points (lines 68–100)

### Lines 69–75: a random direction (uniform on a sphere)

```cpp
inline Vec3 random_unit_vector() {
    while (true) {
        Vec3 p = random_vec(-1, 1);
        double lensq = p.length_squared();
        if (1e-160 < lensq && lensq <= 1.0) return p / std::sqrt(lensq);
    }
}
```

We want every direction to be equally likely. Method ("rejection sampling"):

* Line 70: `while (true)` = repeat forever... until a `return` exits.
* Line 71: pick a random point in the cube from −1 to 1.
* Line 72: its squared distance from the center.
* Line 73: accept it only if it's **inside the ball** of radius 1 (`lensq <= 1.0`) and not almost exactly at the
  center (`1e-160 < lensq`, to avoid dividing by ~0). Then divide by its length → a point **on** the sphere
  surface, i.e. a direction of length 1.
* If rejected (a corner of the cube), try again. Why not accept corners? Points in the corners would make diagonal
  directions more likely than others.

```
┌──────────┐
│ ×  ___ × │   × = rejected (outside the ball)
│  ╱  ·  ╲ │   · = accepted, then pushed out to the surface
│ │ ·   · ││
│  ╲_____╱ │
└──────────┘
```

### Lines 78–81: a random direction on one side (hemisphere)

```cpp
inline Vec3 random_on_hemisphere(const Vec3& normal) {
    Vec3 on_unit_sphere = random_unit_vector();
    return dot(on_unit_sphere, normal) > 0.0 ? on_unit_sphere : -on_unit_sphere;
}
```

* Line 79: a random direction.
* Line 80: if it points to the same side as `normal` (dot > 0), keep it; otherwise flip it (`-`). Now every
  direction is on the normal's side, still evenly spread.

### Lines 84–89: a random point in a disk (for camera lenses)

```cpp
inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() < 1.0) return p;
    }
}
```

The same rejection idea in 2D: a random point in a square (z = 0), kept only if inside the circle of radius 1.
Used for depth of field (chapter 20).

### Lines 92–100: a cosine-weighted direction

```cpp
inline Vec3 random_cosine_direction() {
    double r1 = random_double();
    double r2 = random_double();
    double phi = 2.0 * pi * r1;
    double x = std::cos(phi) * std::sqrt(r2);
    double y = std::sin(phi) * std::sqrt(r2);
    double z = std::sqrt(1.0 - r2);
    return Vec3(x, y, z);
}
```

This gives directions around the **+z** axis where directions **near the top are more likely** (as matte surfaces
reflect light, chapter 30).

* Lines 93–94: two random fractions.
* Line 95: `phi` = a random angle around the circle, 0 to 2π (a full turn).
* Lines 96–97: a random point in a disk: angle `phi`, distance `sqrt(r2)` from the center. (The square root makes
  points spread **evenly** over the disk area; without it they would bunch up in the middle.)
* Line 98: lift the point up onto a dome: `z = sqrt(1 − r2)`, so the total length is 1
  (x² + y² + z² = r2 + 1 − r2 = 1).
* Points in the middle of the disk become directions pointing straight up; points at the edge become directions
  near the horizon. Because the disk is evenly covered, "up" directions end up more common. That's exactly the
  cosine distribution.

---

## Block G — End (line 102)

`} // namespace pixel`

---

## Check your understanding

1. Why does the same seed give the same image? *(The generator is a calculation: the same starting state gives the
   same sequence.)*
2. Why `thread_local`? *(So each thread has its own generator and threads never interfere.)*
3. What range does `random_double(2, 5)` give? *(2 up to, but not including, 5.)*
4. Why are corner points rejected in `random_unit_vector`? *(They would make some directions more likely.)*
