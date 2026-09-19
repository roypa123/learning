# Chapter 13 — Rays and a virtual camera

[← Vectors](12-vectors.md) · [Contents](README.md) · [Next: Hitting a sphere →](14-hitting-a-sphere.md)

---

## Goal

Build the skeleton of a **ray tracer**:

* define a **ray** mathematically and in code,
* set up a virtual **camera** and a **viewport** (a window into the 3D world),
* shoot one ray through the center of every pixel,
* color each ray by its direction to make a **sky**.

This is the moment the book turns from 2D to 3D.

---

## 1. What is a ray?

A **ray** is a half-line: it starts at a point and goes on forever in one direction. We write it as a
function of a number `t`:

```
P(t) = A + t · b

A = origin (where the ray starts)
b = direction
t = how far along the ray (t = 0 is the origin, t = 1 is one "b" further, ...)
```

```
   A                  A + 1b              A + 2b
   ●───────────────────●───────────────────●──────────▶
  t=0                 t=1                 t=2
```

Negative `t` would be *behind* the origin. We'll ignore those, because a camera can't see behind itself.

In code (`pixel/ray.h`):

```cpp
class Ray {
public:
    Ray(const Point3& origin, const Vec3& direction, double time = 0.0);
    const Point3& origin() const;
    const Vec3& direction() const;
    double time() const;                // used for motion blur (chapter 22)
    Point3 at(double t) const { return orig + t * dir; }
};
```

The direction does **not** need to be a unit vector. `t` just measures distance in units of the
direction's length.

---

## 2. The camera model

Think of the camera as an **eye** looking through a **window**. The window is divided into a grid of
small squares, one per pixel. For each pixel, we shoot a ray from the eye through the center of that
pixel's square and ask what it sees.

```
                                viewport (the window), 2 units tall
                           ┌────────────────────────────┐
                           │ ·  ·  ·  ·  ·  ·  ·  ·  ·  │  <- each · is a pixel center
                           │ ·  ·  ·  ·  ·  ·  ·  ·  ·  │
     eye ●  ─ ─ ─ ─ ─ ─ ─ ─│─ ─ ─ ─ ─ ─ ─ ● ─ ─ ─ ─ ─ ─ │─ ─ ─▶  −z (looking direction)
   (0,0,0)                 │ ·  ·  ·  ·  ·  ·  ·  ·  ·  │
                           │ ·  ·  ·  ·  ·  ·  ·  ·  ·  │
                           └────────────────────────────┘
               ◀── focal length = 1 ──▶  (the viewport sits at z = −1)
```

### 2.1 Choosing the numbers

* **Image size**: 400 pixels wide, aspect ratio 16:9 → height = 400 / (16/9) = 225 pixels.
* **Viewport height**: 2.0 units (from y = −1 to y = +1).
* **Viewport width**: height × (image width / image height) = 2 × 400/225 ≈ 3.56. We use the *actual*
  pixel ratio, not 16/9, because the integer height was rounded.
* **Focal length**: the distance from the eye to the viewport: 1.0.

### 2.2 Walking across the viewport

Pixel rows go **down** the image while the y-axis goes **up** in 3D. We handle that with two edge
vectors:

```
viewport_u = (viewport_width, 0, 0)     across the top edge, left → right
viewport_v = (0, −viewport_height, 0)   down the left edge, top → bottom (note the minus!)
```

The distance between pixel centers:

```
pixel_delta_u = viewport_u / image_width
pixel_delta_v = viewport_v / image_height
```

The upper-left corner of the viewport, and the center of the upper-left pixel:

```
viewport_upper_left = camera_center − (0, 0, focal_length) − viewport_u/2 − viewport_v/2
pixel00_loc         = viewport_upper_left + 0.5 · (pixel_delta_u + pixel_delta_v)
```

(Pixel centers are half a pixel in from the corner, just like in 2D.)

```
 viewport_upper_left
      ┌───┬───┬───┬──
      │ ● │ ● │   │        ● pixel00_loc = corner + half a step right + half a step down
      ├───┼───┼───┼──
      │   │   │   │
```

Then pixel `(i, j)`'s center is:

```
pixel_center = pixel00_loc + i · pixel_delta_u + j · pixel_delta_v
ray direction = pixel_center − camera_center
```

---

## 3. What does a ray see? The sky

There's nothing in our world yet, so every ray flies off into the sky. Let's color the sky by the ray's
**direction**: blue when looking up, white near the horizon, like a real sky.

```cpp
Color ray_color(const Ray& r) {
    Vec3 unit_direction = unit_vector(r.direction());
    double a = 0.5 * (unit_direction.y + 1.0);           // y in [−1, 1]  →  a in [0, 1]
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}
```

This is `lerp(white, light_blue, a)`. The pattern `0.5 · (value + 1)`, which maps [−1, 1] to [0, 1],
appears often.

> **Why this looks different from the chapter 3 gradient:** the blend depends on the *direction*,
> not the pixel row. Rays through the left and right edges of the viewport are more tilted, so
> the gradient is slightly curved. A real camera sees the sky the same way.

---

## 4. The code

**File: `chapters/ch13_rays_sky.cpp`**

```cpp
// ch13_rays_sky.cpp
// ------------------------------------------------------------
// Chapter 13: Rays and a simple camera.
// One ray per pixel, shot from the eye through the pixel.
// A ray that hits nothing shows the sky: blue up, white down.
//   images/ch13_sky.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

// The color a ray "sees". For now there is nothing in the world but sky.
Color ray_color(const Ray& r) {
    Vec3 unit_direction = unit_vector(r.direction());
    double a = 0.5 * (unit_direction.y + 1.0);     // y in [-1,1]  ->  a in [0,1]
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    // ---------- image ----------
    const double aspect_ratio = 16.0 / 9.0;
    const int image_width = 400;
    const int image_height = (int)(image_width / aspect_ratio);   // 225

    // ---------- camera ----------
    const double focal_length = 1.0;               // distance eye -> viewport
    const double viewport_height = 2.0;
    const double viewport_width = viewport_height * (double(image_width) / image_height);
    const Point3 camera_center(0, 0, 0);

    // Vectors along the viewport edges (x to the right, y DOWN the image).
    const Vec3 viewport_u(viewport_width, 0, 0);
    const Vec3 viewport_v(0, -viewport_height, 0);
    // Distance between neighbouring pixel centers.
    const Vec3 pixel_delta_u = viewport_u / image_width;
    const Vec3 pixel_delta_v = viewport_v / image_height;
    // Location of the upper-left pixel's center.
    const Point3 viewport_upper_left = camera_center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    const Point3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // ---------- render ----------
    Image img(image_width, image_height);
    for (int j = 0; j < image_height; j++) {
        for (int i = 0; i < image_width; i++) {
            Point3 pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
            Vec3 ray_direction = pixel_center - camera_center;
            Ray r(camera_center, ray_direction);
            img.at(i, j) = ray_color(r);
        }
    }
    // These sky colors were chosen as display values, so save WITHOUT the sRGB curve.
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch13_sky.png", img, raw);
    return 0;
}
```

Note the last lines: the colors `(1,1,1)` and `(0.5,0.7,1.0)` were chosen by eye as *display* values,
so we save **without** the sRGB curve (`raw.srgb = false`) to get exactly the classic look. From chapter
17 on, when real light calculations start, we'll always save with the sRGB curve.

```bat
run ch13_rays_sky
```

## 5. What you should see

![Sky](../images/ch13_sky.png)

> **Image description:** A 400×225 image that is a soft, light sky-blue at the top, fading smoothly to
> white at the bottom. There's nothing else in it. Looking closely, the top corners are very slightly
> less blue than the top center, because rays through the corners point less steeply upward.

It doesn't look like much, but this is a working ray tracer: 90,000 rays, each asking the world
"what's out there?".

---

## 6. The `ray.h` library file

It also contains `Interval`, a tiny helper for "a range of t values" that we start using next chapter.

**File: `include/pixel/ray.h`**

```cpp
// pixel/ray.h
// ------------------------------------------------------------
// Ray: a half-line  P(t) = origin + t * direction
// Interval: a range of numbers [min, max]
// Explained in docs/13-rays-and-camera.md and docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"

namespace pixel {

class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction, double time = 0.0)
        : orig(origin), dir(direction), tm(time) {}

    const Point3& origin() const { return orig; }
    const Vec3& direction() const { return dir; }
    double time() const { return tm; }   // moment inside the camera shutter (motion blur)

    Point3 at(double t) const { return orig + t * dir; }

private:
    Point3 orig;
    Vec3 dir;
    double tm = 0.0;
};

struct Interval {
    double min = +infinity;   // default interval is empty
    double max = -infinity;

    Interval() {}
    Interval(double mn, double mx) : min(mn), max(mx) {}
    // The smallest interval containing both a and b.
    Interval(const Interval& a, const Interval& b)
        : min(a.min <= b.min ? a.min : b.min), max(a.max >= b.max ? a.max : b.max) {}

    double size() const { return max - min; }
    bool contains(double x) const { return min <= x && x <= max; }
    bool surrounds(double x) const { return min < x && x < max; }
    double clamp(double x) const { return x < min ? min : (x > max ? max : x); }
    Interval expand(double delta) const { double p = delta / 2; return Interval(min - p, max + p); }

    static Interval empty() { return Interval(+infinity, -infinity); }
    static Interval universe() { return Interval(-infinity, +infinity); }
};

inline Interval operator+(const Interval& ival, double displacement) {
    return Interval(ival.min + displacement, ival.max + displacement);
}

} // namespace pixel
```

---

## Try it yourself

1. Swap the sky colors for a sunset: orange at the horizon, deep purple at the top.
2. Change the focal length to 0.5. The viewport is closer, so the field of view is wider. Does the image
   change? (A little: the gradient curves more.) Try 3.0.
3. Make the ground: if `unit_direction.y < 0`, return a brown color. You now have a horizon!
4. Make a checkerboard floor: for rays going down, compute where they hit the plane `y = −1`
   (`t = −1 / direction.y`), then use `(int)floor(x) + (int)floor(z)` to alternate colors. This is your first
   ray–object intersection!

## Common problems

| Symptom | Cause |
|---------|-------|
| Image upside down | Forgot the minus sign in `viewport_v` |
| Image stretched | Viewport width computed from the ideal 16/9 instead of the actual pixel ratio |
| Everything the same color | Using `r.direction()` without `unit_vector`, or integer division |

---

## Summary

* A ray is `P(t) = origin + t · direction`.
* The camera: an eye, a viewport at a focal distance, and a grid of pixel centers on it.
* For each pixel: ray from the eye through the pixel center → `ray_color`.
* With nothing in the world, `ray_color` returns the sky.

Next: [Chapter 14 — Hitting a sphere →](14-hitting-a-sphere.md)
