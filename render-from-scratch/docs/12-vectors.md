# Chapter 12 — Vectors: the language of 3D

[← Procedural noise](11-procedural-noise.md) · [Contents](README.md) · [Next: Rays & camera →](13-rays-and-camera.md)

---

## Goal

Everything in 3D graphics (positions, directions, velocities, normals, even colors) is
written with **vectors**. This chapter teaches the small set of vector operations used in the rest of
the book, with pictures instead of proofs:

* adding, subtracting and scaling vectors,
* length and **unit vectors**,
* the **dot product** (angles and lighting),
* the **cross product** (perpendicular directions),
* **reflection**.

Then we use them to shade a ball that *looks* 3D, in a 2D image, as a warm-up for ray tracing.

---

## 1. What is a vector?

A **vector** is a list of numbers. Ours have three: `(x, y, z)`. The same three numbers can mean
different things:

* a **point**: a position in space ("the lamp is at (2, 5, −3)"),
* a **direction** or **displacement**: an arrow ("go 1 right and 2 up": (1, 2, 0)),
* a **color**: (red, green, blue).

In code, all three are the same class, `Vec3`, with two aliases to make intent clear:

```cpp
using Point3 = Vec3;
using Color  = Vec3;
```

### 1.1 Our 3D coordinate system

```
            y (up)
            │
            │
            │
            └──────── x (right)
           ╱
          ╱
         z (towards you, out of the screen)
```

This is a **right-handed** coordinate system: point your right hand's fingers along x, curl them
towards y, and your thumb points along z. Our camera starts at the origin **looking down −z** (into
the screen).

---

## 2. Adding and subtracting

**Adding** vectors: put them tip to tail.

```
a = (3, 1)   b = (1, 2)      a + b = (4, 3)

        ╱│ b
       ╱ │
  ────▶  │                    component by component:
     a    ╲                   (3+1, 1+2) = (4, 3)
```

**Point + direction = point**: "start at P and move by d".

**Point − point = direction**: the arrow from one point to the other:

```
direction from A to B = B − A
```

This is used constantly: "the direction from the hit point to the light is `light_pos − hit_point`".

---

## 3. Scaling, length and unit vectors

**Multiplying by a number** (a *scalar*) stretches the vector: `2 · (1, 2, 0) = (2, 4, 0)`. A negative
number flips it around.

**Length** (magnitude) comes from Pythagoras, in 3D:

```
|v| = sqrt(x² + y² + z²)
```

`(3, 4, 0)` has length 5. We often need `length_squared()` (no square root, faster) for comparisons.

A **unit vector** has length 1. It represents a pure **direction**. To make one, divide by the length
("normalize"):

```
unit_vector(v) = v / |v|        unit_vector((3,4,0)) = (0.6, 0.8, 0)
```

Most formulas in this book expect certain vectors, especially surface normals, to be unit vectors. A
non-normalized normal is one of the most common bugs in graphics.

---

## 4. The dot product

### 4.1 Definition

```
dot(a, b) = a.x·b.x + a.y·b.y + a.z·b.z
```

Just multiply the matching components and add them up. The result is a **single number**.

### 4.2 What it means

For unit vectors, the dot product is the **cosine of the angle between them**:

```
dot(a, b) = |a| |b| cos(θ)

  same direction      perpendicular        opposite
   ──▶ ──▶              ──▶                 ──▶ ◀──
   dot = 1              │   dot = 0          dot = −1
                        ▼
```

So the dot product tells you "how much do these two directions agree?":

| dot | angle | meaning |
|-----|-------|---------|
| 1 | 0° | same direction |
| > 0 | < 90° | roughly the same way |
| 0 | 90° | perpendicular |
| < 0 | > 90° | roughly opposite |
| −1 | 180° | exactly opposite |

### 4.3 Uses in this book

* **Lighting (Lambert's law)**: a surface facing the light receives more light than one tilted away.
  The brightness is proportional to `dot(normal, direction_to_light)`.
* **Front or back?** `dot(ray_direction, normal) < 0` means the ray hits the front of a surface.
* **Projection**: `dot(v, unit_axis)` is how far v reaches along that axis.

---

## 5. The cross product

```
cross(a, b) = ( a.y·b.z − a.z·b.y,
                a.z·b.x − a.x·b.z,
                a.x·b.y − a.y·b.x )
```

The result is a **vector perpendicular to both a and b**, with length equal to the area of the
parallelogram they form:

```
        cross(a,b)
            ▲
            │
            │   b
            │  ╱
            │ ╱
            │╱_______ a
```

Its direction follows the right-hand rule: `cross(x_axis, y_axis) = z_axis`. Swapping the order flips it:
`cross(b, a) = −cross(a, b)`.

Uses:

* The **normal of a triangle** or a flat surface: `cross(edge1, edge2)`.
* Building a **camera coordinate system** (chapter 20): from "forward" and "up" we get "right".

---

## 6. Reflection

A ball bouncing off a wall, or light off a mirror: the incoming direction `v` bounces around the
surface normal `n` (unit length):

```
            n
            ▲
     v ╲    │    ╱ r            r = v − 2·dot(v, n)·n
        ╲   │   ╱
         ╲  │  ╱
    ──────╲─┴─╱──────  surface
```

Why? `dot(v, n)·n` is the part of `v` going *into* the surface (straight down). Subtracting it twice
flips that part to point *out*, and leaves the sideways part unchanged.

---

## 7. Other helpers

| Function | Meaning |
|----------|---------|
| `a * b` (two vectors) | component-wise product, used to **tint** colors: white light × red paint = red |
| `lerp(a, b, t)` | blend: `a + (b − a)·t` |
| `vmin`, `vmax` | component-wise min/max (bounding boxes) |
| `refract(uv, n, ratio)` | bending through glass (chapter 19) |
| `near_zero()` | "is this vector almost (0,0,0)?" (for catching degenerate cases) |
| `luminance(c)` | perceived brightness (chapter 4) |

---

## 8. The library file `vec3.h`

**File: `include/pixel/vec3.h`**

```cpp
// pixel/vec3.h
// ------------------------------------------------------------
// A tiny 3D vector class. We use it for:
//   * positions in space      (Point3)
//   * directions              (Vec3)
//   * colors (red,green,blue) (Color)
// Explained in docs/12-vectors.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <iostream>

namespace pixel {

// Our own constants (we do not rely on M_PI, which is not standard C++).
constexpr double pi       = 3.1415926535897932385;
constexpr double infinity = 1e300;

inline double degrees_to_radians(double degrees) { return degrees * pi / 180.0; }
inline double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double e0, double e1, double x) {
    double t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() {}
    Vec3(double v) : x(v), y(v), z(v) {}
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}

    // Access by index: v[0] == v.x, v[1] == v.y, v[2] == v.z
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    double& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }

    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(double t) { x *= t; y *= t; z *= t; return *this; }
    Vec3& operator*=(const Vec3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
    Vec3& operator/=(double t) { return *this *= 1.0 / t; }

    double length_squared() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(length_squared()); }

    // True if the vector is very close to zero in every direction.
    bool near_zero() const {
        const double s = 1e-8;
        return std::fabs(x) < s && std::fabs(y) < s && std::fabs(z) < s;
    }

    double max_component() const { return x > y ? (x > z ? x : z) : (y > z ? y : z); }
    double min_component() const { return x < y ? (x < z ? x : z) : (y < z ? y : z); }
};

// Aliases: same type, different meaning. Makes code easier to read.
using Point3 = Vec3;
using Color  = Vec3;

inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator*(double t, const Vec3& v) { return Vec3(t * v.x, t * v.y, t * v.z); }
inline Vec3 operator*(const Vec3& v, double t) { return t * v; }
inline Vec3 operator/(const Vec3& v, double t) { return (1.0 / t) * v; }
inline Vec3 operator/(const Vec3& a, const Vec3& b) { return Vec3(a.x / b.x, a.y / b.y, a.z / b.z); }

inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

inline Vec3 unit_vector(const Vec3& v) { return v / v.length(); }

inline Vec3 vmin(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z));
}
inline Vec3 vmax(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z));
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }
inline Vec3 vabs(const Vec3& v) { return Vec3(std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)); }

// Mirror reflection of v around the normal n (n must be unit length).
inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - 2.0 * dot(v, n) * n; }

// Snell's law refraction. uv and n must be unit length.
// etai_over_etat = (index of refraction we come from) / (index we go into)
inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

// Brightness of a linear color as the human eye perceives it (Rec.709 weights).
inline double luminance(const Color& c) { return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z; }

} // namespace pixel
```

Notes:

* `operator[]` lets you write `v[0]`, `v[1]`, `v[2]`, handy in loops over axes (bounding boxes, chapter 23).
* `Vec3(double v)` builds `(v, v, v)`, so `Color(0.5)` is middle grey.
* All the math is `inline` and very small, so the compiler turns it into a handful of machine
  instructions.

---

## 9. The program: a "fake 3D" sphere

**File: `chapters/ch12_vectors.cpp`**

```cpp
// ch12_vectors.cpp
// ------------------------------------------------------------
// Chapter 12: Vectors - the language of 3D.
// Part 1 prints vector math results so you can check them by hand.
// Part 2 uses vectors to shade a "fake 3D" ball in a 2D image:
//   images/ch12_fake_sphere.png
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Vector arithmetic you can check with pencil & paper -----
    Vec3 a(1, 2, 3);
    Vec3 b(4, 5, 6);
    std::cout << "a           = " << a << "\n";
    std::cout << "b           = " << b << "\n";
    std::cout << "a + b       = " << a + b << "   (expected (5, 7, 9))\n";
    std::cout << "b - a       = " << b - a << "   (expected (3, 3, 3))\n";
    std::cout << "2 * a       = " << 2.0 * a << "   (expected (2, 4, 6))\n";
    std::cout << "dot(a, b)   = " << dot(a, b) << "   (expected 32)\n";
    std::cout << "cross(a, b) = " << cross(a, b) << "   (expected (-3, 6, -3))\n";
    std::cout << "|a|         = " << a.length() << "   (expected 3.74166)\n";
    std::cout << "unit(a)     = " << unit_vector(a) << "   (length 1)\n";
    Vec3 x(1, 0, 0), y(0, 1, 0);
    std::cout << "cross(x, y) = " << cross(x, y) << "   (expected z = (0, 0, 1))\n";
    Vec3 down_right(1, -1, 0);
    std::cout << "reflect((1,-1,0), up) = " << reflect(down_right, y) << "   (expected (1, 1, 0))\n";

    // ---------- 2. Shading a ball with the dot product ---------------------
    const int S = 400;
    Image img(S, S);
    Vec3 light_dir = unit_vector(Vec3(-1, 1, 1));     // light comes from upper-left-front
    Vec3 view_dir(0, 0, 1);                           // we look along -z, so "towards us" is +z
    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {
            // Map the pixel to [-1, 1] x [-1, 1], y pointing up.
            double sx = (px + 0.5) / S * 2 - 1;
            double sy = 1 - (py + 0.5) / S * 2;
            double r2 = sx * sx + sy * sy;
            if (r2 > 0.8 * 0.8) {                         // background
                img.at(px, py) = lerp(hex_color(0x1E293B), hex_color(0x0F172A), (py + 0.5) / S);
                continue;
            }
            // On a sphere of radius R, the surface point above (sx, sy) has z = sqrt(R^2 - x^2 - y^2).
            double sz = std::sqrt(0.8 * 0.8 - r2);
            Vec3 normal = unit_vector(Vec3(sx, sy, sz));

            double diffuse = std::fmax(0.0, dot(normal, light_dir));                 // Lambert
            Vec3 reflected = reflect(-light_dir, normal);
            double specular = std::pow(std::fmax(0.0, dot(reflected, view_dir)), 40); // Phong
            Color base = hex_color(0xE11D48);
            Color c = base * (0.08 + 0.92 * diffuse) + Color(1, 1, 1) * 0.6 * specular;
            img.at(px, py) = c;
        }
    }
    save_image("images/ch12_fake_sphere.png", img);
    return 0;
}
```

### Part 1: checking the arithmetic

The console part prints results you can verify by hand, for example:

```
dot(a, b)   = 32   (expected 32)        1·4 + 2·5 + 3·6 = 4 + 10 + 18
cross(a, b) = (-3, 6, -3)               (2·6 − 3·5, 3·4 − 1·6, 1·5 − 2·4)
```

### Part 2: shading a ball

We draw a circle, but for each pixel inside it we **work out where on a 3D sphere that pixel would
be**:

```
seen from the side:

  eye →  │          z = sqrt(R² − x² − y²)
         │      ___
         │    ╱  ↗ normal points straight away from the center
         │   │  ●  │
         │    ╲___╱
```

If the sphere is centered at the origin, a surface point `(x, y, z)` is also its **normal** direction.
Then:

* **Diffuse (Lambert):** `max(0, dot(normal, light_dir))`. Bright where the surface faces the light,
  dark where it turns away. `max(0, ...)` because surfaces facing away get no light (not negative
  light!).
* **Specular (Phong):** reflect the light direction around the normal and check how close the result
  is to the direction towards the viewer: `pow(max(0, dot(reflected, view)), 40)`. The power 40 makes it
  a small, sharp highlight.
* A small constant (`0.08`) fakes light bouncing around the room ("ambient").

```bat
run ch12_vectors
```

---

## 10. What you should see

![Fake sphere](../images/ch12_fake_sphere.png)

> **Image description:** A glossy crimson ball on a dark slate-blue background. Light comes from the
> upper left: the upper-left part of the ball is bright red, and the shading gets smoothly darker toward
> the lower right, where it is nearly black. A small, sharp white highlight sits in the upper-left area.
> Although it's just a circle in a 2D image, it clearly looks like a 3D sphere.

This image is the whole idea of shading in miniature: **normal + light direction → brightness**.
In the next chapter we stop faking the geometry and start shooting real rays.

---

## Try it yourself

1. Move the light to the right: `Vec3(1, 0.3, 1)`. Watch the lit side move.
2. Change the specular power from 40 to 5 and to 200. What kind of material does each look like?
3. Add a **second, blue light** from the lower right, adding its diffuse term tinted blue. This is
   called a "rim" or "fill" light in photography.
4. Replace the red base color with the normal itself: `0.5 * (normal + Color(1,1,1))`. You'll get a
   colorful ball: this "normal visualization" will come back in chapter 15.
5. Verify by hand: `|unit_vector((1,2,2))|` is 1. What are its components?

## Common problems

| Symptom | Cause |
|---------|-------|
| Lighting looks inverted | Light direction pointing *from* the light instead of *towards* it |
| Black ring or NaN pixels at the edge | `sqrt` of a slightly negative number: clamp before `sqrt` |
| Lighting too strong on one side | Normal or light direction not normalized |

---

## Summary

* Vectors are `(x, y, z)` and can be points, directions or colors.
* `B − A` is the direction from A to B; `unit_vector` gives a pure direction.
* **dot** = how much two directions agree (cosine); used for lighting and front/back tests.
* **cross** = a perpendicular vector; used for normals and camera axes.
* **reflect**: `v − 2·dot(v,n)·n`.

Next: [Chapter 13 — Rays and a virtual camera →](13-rays-and-camera.md)
