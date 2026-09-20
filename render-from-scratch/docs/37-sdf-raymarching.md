# Chapter 37 — Signed distance functions and ray marching

[← Denoising](36-denoising.md) · [Contents](README.md) · [Next: The final shot →](38-final-shot.md)

> 📖 **Line by line:** [sdf explained line by line](line-by-line/sdf.md) · [ch37_sdf explained line by line](line-by-line/ch37_sdf.md)

---

## Goal

There's a completely different way to describe shapes: not with triangles or equations to solve, but
with a function that tells you **how far away the surface is**. You'll learn:

* **signed distance functions** (SDFs) for spheres, boxes, tori and capsules,
* combining shapes: union, subtraction, intersection, and the magical **smooth union**,
* **infinite repetition** with one line of code,
* **sphere tracing** (ray marching) to render them,
* normals from the **gradient**,
* rendering a **fractal**: the Mandelbulb.

SDFs are the tool of the demoscene and of sites like Shadertoy, and they're used in films for clouds,
fluids and procedural shapes.

---

## 1. Distance functions

An SDF takes a point p and returns:

* a **positive** number: p is outside, and that's the distance to the nearest surface,
* **zero**: p is on the surface,
* a **negative** number: p is inside (how deep).

The sphere is the simplest:

```
sphere(p, r) = |p| − r
```

A box is a little more work (`q = |p| − half_size` per axis; outside distance = length of the positive
parts; inside = the largest negative part):

```cpp
inline double box(const Point3& p, const Vec3& half_size) {
    Vec3 q = vabs(p) - half_size;
    return vmax(q, Vec3(0, 0, 0)).length() + std::fmin(q.max_component(), 0.0);
}
```

A **rounded box** is a smaller box minus a radius: subtracting r from any SDF "inflates" it by r with
rounded corners. A **torus** is "distance to a circle, minus the tube radius". A **capsule** is "distance to
a line segment, minus a radius". To **move** a shape, evaluate it at `p − center`; to rotate it, rotate `p`
the opposite way (like instances in chapter 27).

---

## 2. Combining shapes

This is where SDFs shine:

| Operation | Formula | Result |
|-----------|---------|--------|
| union (A or B) | `min(a, b)` | both shapes |
| intersection (A and B) | `max(a, b)` | only the overlap |
| subtraction (A minus B) | `max(a, −b)` | A with a bite taken out |
| **smooth union** | `lerp(b, a, h) − k·h·(1−h)` with `h = clamp(0.5 + 0.5(b−a)/k)` | shapes **melt** together |

```
 union        intersection    subtraction      smooth union
 ◯◯           ◯∩◯             ◯(               ◯═◯
```

The smooth union (by Inigo Quilez) blends two shapes over a distance `k`, like clay or liquid
metal. Try doing *that* with triangles!

### 2.1 Infinite repetition

Replace each coordinate by its position within a repeating cell:

```cpp
p.x = p.x − cell · round(p.x / cell);
```

Now every point "thinks" it's near the origin of its own cell, and **one** shape becomes an infinite field
of copies. That costs nothing, whereas an infinite number of triangles would need infinite memory.

---

## 3. Rendering: sphere tracing

We can't solve an equation for a general SDF, but we don't need to. The SDF tells us the distance to the
**nearest** surface in **any** direction. So from the ray origin, we can safely step forward by exactly that
distance: there's nothing closer, so we can't jump through anything.

```
   ●───────▶●─────▶●──▶●─▶●▶|  surface
   d=5       d=3    d=1.5 ...   steps shrink as we approach
   (circles of radius d are guaranteed empty)
```

```
t = start
repeat:
    d = sdf(ray.at(t))
    if d < epsilon: HIT at t
    t += d
    if t > end or too many steps: MISS
```

This is **sphere tracing** (John Hart, 1996). Each step is "the largest empty sphere around the current
point".

Details in our `SDFObject`:

* We only march inside the object's **bounding box** (clipped with the slab test from chapter 23), so the BVH
  and the empty space outside cost nothing.
* `step_scale < 1` takes cautious steps for functions that aren't exact distances (fractals, heavily distorted
  shapes).
* If the ray **starts inside** (a ray refracting through a glass SDF), we flip the sign to march outward.
* Directions don't need to be normalized: we divide the step by the direction's length.

### 3.1 Normals from the gradient

The surface normal is the direction in which the distance **grows fastest**: the **gradient** of the SDF.
Estimate it with **central differences**, i.e. sample the SDF a tiny step on each side along each axis:

```
n = normalize( sdf(p+hx) − sdf(p−hx),  sdf(p+hy) − sdf(p−hy),  sdf(p+hz) − sdf(p−hz) )
```

Six extra evaluations per hit, and it works for **any** SDF, however complex.

---

## 4. The Mandelbulb

The Mandelbulb (Daniel White and Paul Nylander, 2009) is a 3D cousin of the famous Mandelbrot set. You
repeatedly apply `z → z⁸ + p` in 3D, using spherical coordinates to define the "power" of a 3D point.
Points whose sequence stays bounded are inside. A **distance estimator** based on how fast the sequence
escapes (`0.5 · log(r) · r / dr`) lets us sphere-trace it. It's not an exact distance, so we use
`step_scale = 0.9`. The result is an endlessly detailed, organic, alien structure from ~20 lines of code.

### `sdf.h`

**File: `include/pixel/sdf.h`**

```cpp
// pixel/sdf.h
// ------------------------------------------------------------
// Signed Distance Functions (SDFs) and ray marching.
// An SDF answers: "how far is point p from the surface?"
//   > 0 outside, < 0 inside, = 0 exactly on the surface.
// With SDFs you can blend shapes like clay, repeat them forever,
// and build fractals - things that are hard with triangles.
// Explained in docs/37-sdf-raymarching.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include "vec3.h"
#include "hittable.h"

namespace pixel {
namespace sdf {

// ---------- primitives (all centered at the origin) ------------------------
inline double sphere(const Point3& p, double r) { return p.length() - r; }

inline double box(const Point3& p, const Vec3& half_size) {
    Vec3 q = vabs(p) - half_size;
    return vmax(q, Vec3(0, 0, 0)).length() + std::fmin(q.max_component(), 0.0);
}

inline double round_box(const Point3& p, const Vec3& half_size, double radius) {
    return box(p, half_size - Vec3(radius)) - radius;
}

// Torus lying in the XZ plane. R = ring radius, r = tube radius.
inline double torus(const Point3& p, double R, double r) {
    double qx = std::sqrt(p.x * p.x + p.z * p.z) - R;
    return std::sqrt(qx * qx + p.y * p.y) - r;
}

// Infinite plane y = height.
inline double plane_y(const Point3& p, double height) { return p.y - height; }

// Vertical capsule from y=0 to y=h.
inline double capsule_y(Point3 p, double h, double r) {
    p.y -= clampd(p.y, 0.0, h);
    return p.length() - r;
}

// ---------- combining shapes -----------------------------------------------
inline double op_union(double a, double b) { return std::fmin(a, b); }
inline double op_subtract(double a, double b) { return std::fmax(a, -b); }   // a minus b
inline double op_intersect(double a, double b) { return std::fmax(a, b); }

// Smooth union: melts two shapes together. k = blend size.
inline double op_smooth_union(double a, double b, double k) {
    double h = clamp01(0.5 + 0.5 * (b - a) / k);
    return lerpd(b, a, h) - k * h * (1.0 - h);
}

// Infinite repetition every 'cell' units in x and z.
inline Point3 op_repeat_xz(const Point3& p, double cell) {
    auto rep = [cell](double v) { return v - cell * std::round(v / cell); };
    return Point3(rep(p.x), p.y, rep(p.z));
}

inline Point3 rotate_y(const Point3& p, double radians) {
    double c = std::cos(radians), s = std::sin(radians);
    return Point3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

// The Mandelbulb fractal (power 8). Returns a distance estimate.
inline double mandelbulb(const Point3& pos, int iterations = 10, double power = 8.0) {
    Vec3 z = pos;
    double dr = 1.0, r = 0.0;
    for (int i = 0; i < iterations; i++) {
        r = z.length();
        if (r > 2.0) break;
        double theta = std::acos(clampd(z.z / r, -1.0, 1.0));
        double phi = std::atan2(z.y, z.x);
        dr = std::pow(r, power - 1.0) * power * dr + 1.0;
        double zr = std::pow(r, power);
        theta *= power;
        phi *= power;
        z = zr * Vec3(std::sin(theta) * std::cos(phi), std::sin(phi) * std::sin(theta), std::cos(theta));
        z += pos;
    }
    if (r < 1e-12) return 0.0;
    return 0.5 * std::log(r) * r / dr;
}

} // namespace sdf

// A Hittable defined by a distance function, found by "sphere tracing":
// step forward by the distance to the nearest surface until we touch it.
class SDFObject : public Hittable {
public:
    using DistanceFn = std::function<double(const Point3&)>;

    SDFObject(DistanceFn fn, const AABB& bounds, std::shared_ptr<Material> mat,
              int max_steps = 256, double epsilon = 1e-4, double step_scale = 1.0)
        : fn(fn), bounds(bounds), mat(mat), max_steps(max_steps), epsilon(epsilon), step_scale(step_scale) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // Only march inside the bounding box.
        double t0 = ray_t.min, t1 = ray_t.max;
        if (!clip_to_box(r, t0, t1)) return false;

        double len = r.direction().length();
        double t = t0;
        // If we start inside the surface (e.g. a bounce from it), we look for the way out.
        double sign = fn(r.at(t)) < 0 ? -1.0 : 1.0;
        for (int i = 0; i < max_steps && t < t1; i++) {
            Point3 p = r.at(t);
            double d = sign * fn(p);
            if (d < epsilon) {
                if (t <= ray_t.min) { t += 2 * epsilon / len; continue; }
                rec.t = t;
                rec.p = p;
                rec.set_face_normal(r, normal_at(p));
                rec.u = rec.v = 0;
                rec.mat = mat.get();
                return true;
            }
            t += step_scale * d / len;    // distance d along a direction of length 'len'
        }
        return false;
    }

    AABB bounding_box() const override { return bounds; }

    // The gradient of the distance field points away from the surface.
    Vec3 normal_at(const Point3& p) const {
        const double h = 1e-4;
        Vec3 n(fn(p + Vec3(h, 0, 0)) - fn(p - Vec3(h, 0, 0)),
               fn(p + Vec3(0, h, 0)) - fn(p - Vec3(0, h, 0)),
               fn(p + Vec3(0, 0, h)) - fn(p - Vec3(0, 0, h)));
        double l = n.length();
        return l > 0 ? n / l : Vec3(0, 1, 0);
    }

private:
    DistanceFn fn;
    AABB bounds;
    std::shared_ptr<Material> mat;
    int max_steps;
    double epsilon;
    double step_scale;

    bool clip_to_box(const Ray& r, double& t0, double& t1) const {
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = bounds.axis_interval(axis);
            double inv = 1.0 / r.direction()[axis];
            double a = (ax.min - r.origin()[axis]) * inv;
            double b = (ax.max - r.origin()[axis]) * inv;
            if (a > b) { double tmp = a; a = b; b = tmp; }
            if (a > t0) t0 = a;
            if (b < t1) t1 = b;
            if (t1 <= t0) return false;
        }
        return true;
    }
};

} // namespace pixel
```

---

## 5. The program

**File: `chapters/ch37_sdf.cpp`**

```cpp
// ch37_sdf.cpp
// ------------------------------------------------------------
// Chapter 37: Signed distance functions and ray marching.
// Shapes that are hard to build from triangles:
//   * three spheres melted together (smooth union)
//   * a rounded cube with a spherical bite taken out (subtraction)
//   * a field of pillars repeated with one line of code (repetition)
//   * the Mandelbulb fractal
//   images/ch37_sdf_scene.png
//   images/ch37_mandelbulb.png
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. A gallery of SDF shapes ----------------------------------
    {
        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-40, 0, -40), Vec3(80, 0, 0), Vec3(0, 0, 80),
                                         std::make_shared<Lambertian>(hex_color(0xB8B2A7))));

        // (a) Blob: three spheres blended with a smooth union.
        auto blob = [](const Point3& p) {
            Point3 q = p - Point3(-2.2, 0.9, 0);
            double d1 = sdf::sphere(q - Vec3(0, 0, 0), 0.7);
            double d2 = sdf::sphere(q - Vec3(0.6, 0.5, 0.2), 0.5);
            double d3 = sdf::sphere(q - Vec3(-0.4, 0.6, -0.3), 0.45);
            return sdf::op_smooth_union(sdf::op_smooth_union(d1, d2, 0.35), d3, 0.35);
        };
        world.add(std::make_shared<SDFObject>(blob, AABB(Point3(-3.4, 0, -1.2), Point3(-1.0, 2.2, 1.2)),
                                              std::make_shared<Plastic>(hex_color(0xE11D48), 0.2)));

        // (b) Rounded cube minus a sphere.
        auto bitten = [](const Point3& p) {
            Point3 q = sdf::rotate_y(p - Point3(0, 0.8, 0), 0.6);
            double cube = sdf::round_box(q, Vec3(0.7, 0.7, 0.7), 0.12);
            double bite = sdf::sphere(q - Vec3(0.55, 0.55, 0.55), 0.6);
            return sdf::op_subtract(cube, bite);
        };
        world.add(std::make_shared<SDFObject>(bitten, AABB(Point3(-1.2, 0, -1.2), Point3(1.2, 1.7, 1.2)),
                                              std::make_shared<RoughMetal>(Color(0.95, 0.64, 0.54), 0.25)));

        // (c) Torus.
        auto ring = [](const Point3& p) {
            Point3 q = p - Point3(2.3, 0.75, 0);
            Point3 tilted(q.x, q.y * std::cos(1.2) - q.z * std::sin(1.2), q.y * std::sin(1.2) + q.z * std::cos(1.2));
            return sdf::torus(tilted, 0.55, 0.18);
        };
        world.add(std::make_shared<SDFObject>(ring, AABB(Point3(1.5, 0, -0.9), Point3(3.1, 1.5, 0.9)),
                                              std::make_shared<Plastic>(hex_color(0x0EA5E9), 0.1)));

        // (d) Endless pillars behind: ONE capsule, repeated every 1.5 units.
        auto pillars = [](const Point3& p) {
            Point3 q = sdf::op_repeat_xz(p, 1.5);
            return sdf::capsule_y(q, 3.0, 0.2);
        };
        world.add(std::make_shared<SDFObject>(pillars, AABB(Point3(-20, 0, -20), Point3(20, 3.3, -3.0)),
                                              std::make_shared<Lambertian>(hex_color(0xE7E5E4))));

        Vec3 sun_dir = unit_vector(Vec3(-0.6, 0.7, 0.5));
        auto sun = make_sun(sun_dir, 1.0, Color(3000, 2800, 2500));
        world.add(sun);
        lights.add(sun);
        SkySettings sky;
        sky.sun_direction = sun_dir;

        Camera cam;
        cam.image_width = 800;
        cam.samples_per_pixel = 64;
        cam.max_depth = 12;
        cam.vfov = 36;
        cam.lookfrom = Point3(0, 2.4, 8);
        cam.lookat = Point3(0, 0.9, 0);
        cam.background = physical_sky(sky);
        cam.max_sample_value = 50;
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        opt.exposure = 0.7;
        save_image("images/ch37_sdf_scene.png", cam.render(world, &lights), opt);
    }

    // ---------- 2. The Mandelbulb -------------------------------------------
    {
        HittableList world, lights;
        auto bulb = [](const Point3& p) { return sdf::mandelbulb(p, 12, 8.0); };
        world.add(std::make_shared<SDFObject>(bulb, AABB(Point3(-1.25, -1.25, -1.25), Point3(1.25, 1.25, 1.25)),
                                              std::make_shared<Plastic>(hex_color(0xF59E0B), 0.35),
                                              400, 2e-4, 0.9));
        auto key = std::make_shared<Sphere>(Point3(4, 5, 4), 1.2, std::make_shared<DiffuseLight>(Color(18, 16, 14)));
        world.add(key);
        lights.add(key);

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 500;
        cam.samples_per_pixel = 64;
        cam.max_depth = 8;
        cam.vfov = 30;
        cam.lookfrom = Point3(2.2, 1.6, 3.0);
        cam.lookat = Point3(0, 0, 0);
        cam.background = gradient_sky(Color(0.05, 0.03, 0.08), Color(0.12, 0.15, 0.3));
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        save_image("images/ch37_mandelbulb.png", cam.render(world, &lights), opt);
    }
    return 0;
}
```

```bat
run ch37_sdf
```

The Mandelbulb is slow (hundreds of fractal iterations per ray step). Be patient, or reduce the image
size while experimenting.

---

## 6. What you should see

![SDF scene](../images/ch37_sdf_scene.png)

> **Image description:** A sunny outdoor scene on a light stone-colored floor. **Left:** a glossy red blob
> made of three spheres melted together, like a drop of liquid. **Center:** a copper rounded cube, turned
> slightly, with a smooth spherical bite taken out of one corner. **Right:** a glossy light-blue ring (torus)
> standing tilted. **Behind them:** a field of white pill-shaped pillars in a perfect grid, stretching to the
> left and right and into the distance. The sun shines from the upper left, behind the camera, and
> everything casts crisp shadows toward the back right.

![Mandelbulb](../images/ch37_mandelbulb.png)

> **Image description:** A square image of the Mandelbulb fractal in glossy orange-gold against a dark
> purple-blue background: a roughly spherical, bulbous shape covered in layered, curling lobes and
> tentacle-like buds that repeat at smaller and smaller scales, lit from the upper right with deep shadows
> between the lobes.

---

## Try it yourself

1. Change the smooth-union `k` from 0.35 to 0.05 and to 1.0.
2. **Twist** the rounded box: rotate `p` around y by an angle proportional to `p.y` before evaluating it.
3. Make the repeated pillars wobble: add `0.1 · sin(p.y · 4 + cell index)` to the distance.
4. Try Mandelbulb powers 4, 6, 12 (the `power` parameter).
5. Make a glass SDF blob (`Dielectric(1.5)`) and check that refraction works (the sign flip).

## Common problems

| Symptom | Cause |
|---------|-------|
| Holes, speckles or "onion rings" | Steps too big (distance not exact): lower `step_scale` |
| Surface missing at grazing angles | `max_steps` too low |
| Black acne on surfaces | Epsilon too small relative to scene size, or normal step `h` too small |
| Shape cut off | Bounding box too small |

---

## Summary

* An SDF gives the (signed) distance to a surface; shapes combine with min/max and melt with smooth union.
* Repetition makes infinite copies for free.
* Sphere tracing steps by the distance until it touches the surface; normals come from the gradient.
* Fractals like the Mandelbulb are just distance estimators.

Next: [Chapter 38 — The final shot →](38-final-shot.md)
