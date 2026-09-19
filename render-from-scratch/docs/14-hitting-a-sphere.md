# Chapter 14 — Hitting a sphere

[← Rays & camera](13-rays-and-camera.md) · [Contents](README.md) · [Next: Normals & many objects →](15-normals-and-lists.md)

> 📖 **Line by line:** [ch14_sphere explained line by line](line-by-line/ch14_sphere.md)

---

## Goal

Put the first **object** in our world: a sphere. You'll learn:

* how to describe a sphere with an equation,
* how to find where a ray **intersects** it by solving a quadratic equation,
* what the **discriminant** tells us,
* how to find the **distance** to the hit.

---

## 1. The sphere equation

A sphere with center **C** and radius **r** is the set of all points **P** that are exactly `r` away
from C:

```
|P − C| = r
```

Square both sides (to get rid of the square root inside the length) and write the squared length as a
dot product with itself:

```
(P − C) · (P − C) = r²
```

* If `(P − C)·(P − C) < r²`, P is **inside** the sphere.
* If it's `= r²`, P is **on** the surface.
* If it's `> r²`, P is **outside**.

---

## 2. Where does a ray hit it?

Our ray is `P(t) = A + t·b`. It hits the sphere at the values of `t` where `P(t)` is on the surface:

```
(A + t·b − C) · (A + t·b − C) = r²
```

Let's call `oc = C − A` (the vector from the ray origin to the sphere center). Then
`A + t·b − C = t·b − oc`, and:

```
(t·b − oc) · (t·b − oc) = r²
```

Expand it like `(x − y)² = x² − 2xy + y²`, since the dot product follows the same rules:

```
t²·(b·b) − 2t·(b·oc) + (oc·oc) − r² = 0
```

That's a **quadratic equation** in t: `a·t² + B·t + c = 0` with

```
a = b · b
B = −2 · (b · oc)
c = oc · oc − r²
```

### 2.1 Solving a quadratic (a quick refresher)

The solutions of `a·t² + B·t + c = 0` are:

```
      −B ± sqrt(B² − 4ac)
t = ───────────────────────
            2a
```

The part under the square root, `D = B² − 4ac`, is called the **discriminant**, and it tells us
everything about how the ray meets the sphere:

```
   D < 0: no solution            D = 0: one solution           D > 0: two solutions
      the ray MISSES               the ray just TOUCHES          the ray goes IN and OUT
                                   (grazes the edge)

   ───────▶     ◯                ───────◯▶                     ─────●───────●────▶
                                                                    t1  ◯   t2
```

When there are two solutions, the smaller `t` is where the ray **enters** the sphere (the side
facing us), and the larger where it **leaves**.

### 2.2 A simplification (used by the library)

If `B = −2h` (with `h = b·oc`), the formula simplifies:

```
t = (h ± sqrt(h² − a·c)) / a
```

That saves a couple of multiplications, which matters when you run it a billion times. Chapter 15's
library `Sphere` uses this form; this chapter's program uses the textbook form so you can compare
it with your school notes.

---

## 3. From hit test to picture

### 3.1 Version 1: did we hit it?

```cpp
double t = hit_sphere(Point3(0, 0, -1), 0.5, r);
flat.at(i, j) = t > 0 ? Color(1, 0, 0) : sky(r);
```

Red if the ray hits the sphere, sky otherwise. The sphere sits one unit in front of the camera (z = −1)
with radius 0.5.

### 3.2 Version 2: how far away?

`t` is the distance along the ray (in units of the ray direction's length). Multiply by the
direction's length to get the real distance. We map closeness to brightness: the nearest point of the
sphere (distance ≈ 0.5) becomes white, and the edges (distance ≈ 1.0) become dark. This kind of picture
is called a **depth map**, and it's very useful for debugging.

### 3.3 The program

**File: `chapters/ch14_sphere.cpp`**

```cpp
// ch14_sphere.cpp
// ------------------------------------------------------------
// Chapter 14: Hitting a sphere.
// We solve a quadratic equation to find if a ray touches a sphere.
//   images/ch14_red_sphere.png     - yes/no hit test: a flat red disc
//   images/ch14_depth.png          - how FAR away each hit is (t value)
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Returns the smallest positive t where the ray hits the sphere, or -1 if it misses.
double hit_sphere(const Point3& center, double radius, const Ray& r) {
    Vec3 oc = center - r.origin();
    double a = dot(r.direction(), r.direction());
    double b = -2.0 * dot(r.direction(), oc);
    double c = dot(oc, oc) - radius * radius;
    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return -1.0;               // no real solution: miss
    return (-b - std::sqrt(discriminant)) / (2.0 * a);  // the nearer of the two solutions
}

Color sky(const Ray& r) {
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    const int W = 400, H = 225;
    const Point3 eye(0, 0, 0);
    const Vec3 du(3.5555 / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(3.5555 / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image flat(W, H), depth(W, H);
    for (int j = 0; j < H; j++) {
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            double t = hit_sphere(Point3(0, 0, -1), 0.5, r);

            // Image 1: red where we hit, sky elsewhere.
            flat.at(i, j) = t > 0 ? Color(1, 0, 0) : sky(r);

            // Image 2: brightness = closeness. Nearest point ~0.5 away, edges ~1.0.
            if (t > 0) {
                double closeness = clamp01((1.1 - t * r.direction().length()) / 0.6);
                depth.at(i, j) = Color(closeness, closeness, closeness);
            } else {
                depth.at(i, j) = Color(0.05, 0.05, 0.1);
            }
        }
    }
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch14_red_sphere.png", flat, raw);
    save_image("images/ch14_depth.png", depth, raw);
    return 0;
}
```

(The camera setup is a compact version of chapter 13's.)

```bat
run ch14_sphere
```

---

## 4. What you should see

![Red sphere](../images/ch14_red_sphere.png)

> **Image description:** The sky gradient from chapter 13 (light blue at the top, white at the
> bottom) with a **flat red disc** in the center, about half the image height across. There's no
> shading at all: it looks like a paper circle stuck on the sky. The edge is jagged if you zoom in.

![Depth](../images/ch14_depth.png)

> **Image description:** A dark navy background with a disc that is **bright white in the middle**,
> fading smoothly to dark grey at its rim. It already looks like a ball lit from the front,
> just from distance information.

The flat disc is correct! We only asked "hit or miss?", which gives a silhouette. To make it look
3D we need to know **which way the surface faces** at each hit point. That's the **normal**, in the next
chapter.

---

## 5. Two subtle cases

**A sphere behind the camera.** Put the sphere at `z = +1`. The quadratic doesn't know about "in front"
or "behind": it happily finds two solutions, but both are **negative** `t` values (the hits are behind
the eye). Our check `t > 0` correctly rejects them. Without it you'd see a sphere that isn't there!

**The camera inside a sphere.** Now make a big sphere around the camera: center `(0, 0, 0)`, radius 2.
Every ray starts inside it, so the smaller root is negative (the wall behind us) and the larger root
is positive (the wall in front of us). `hit_sphere` returns only the smaller root, sees `t < 0`, and
reports a miss, even though the ray clearly hits the inside wall. Chapter 15 fixes this properly: we
give each hit test a valid **range** of `t` values (an `Interval`) and try the second root when the
first one is outside the range.

---

## Try it yourself

1. Move the sphere: `Point3(0.5, 0.3, -1)`. Make it bigger (radius 0.8) or farther (z = −3).
2. Add a second sphere by calling `hit_sphere` twice and keeping the closer hit (smaller positive t).
   Which one should be visible where they overlap?
3. Print `D` (the discriminant) for the ray through the center pixel and for a ray through a corner.
4. Color the sphere by `t` with a rainbow: `hsv(t * 300, 1, 1)`.

## Common problems

| Symptom | Cause |
|---------|-------|
| Sphere looks like an ellipse | Viewport width doesn't match the image aspect ratio |
| Sphere visible when behind the camera | Not rejecting negative t (see section 5) |
| Nothing visible from inside a sphere | Only the smaller root is checked (see section 5) |
| NaN pixels | `sqrt` of a negative discriminant: check `D < 0` first |

---

## Summary

* A sphere: `(P − C)·(P − C) = r²`.
* Plugging in the ray gives a quadratic in `t`. The discriminant tells miss, touch or hit.
* The smaller root is the entry point; `t` measures distance.
* A hit test alone gives a flat silhouette. Shading needs normals.

Next: [Chapter 15 — Surface normals and many objects →](15-normals-and-lists.md)
