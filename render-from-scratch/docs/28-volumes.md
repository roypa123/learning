# Chapter 28 — Volumes: smoke, fog and clouds

[← Instances](27-instances.md) · [Contents](README.md) · [Next: Monte Carlo →](29-monte-carlo.md)

> 📖 **Line by line:** [volume explained line by line](line-by-line/volume.md) · [ch28_volumes explained line by line](line-by-line/ch28_volumes.md)

---

## Goal

Not everything has a surface. Smoke, fog, mist, clouds, dusty air and murky water are **volumes**:
spaces filled with tiny particles that scatter light. In this chapter you'll learn:

* how light travels through a medium: **absorption, scattering, density**,
* how to sample a random **free-flight distance** (the exponential distribution),
* the `ConstantMedium` class (uniform fog or smoke),
* **delta tracking** for media whose density changes (a noise-shaped cloud),
* the **isotropic phase function**.

Volumes are one of the biggest ingredients of a "cinematic" look: god rays, atmospheric haze, dusty sunbeams.
Chapter 38 uses one.

---

## 1. Light in a medium

Imagine a ray of light going through fog. Every so often it hits a water droplet. Then it's either
**absorbed** (turned into heat) or **scattered** (sent off in a new direction). Between collisions, it
travels in a straight line.

```
    ray ──────────●     ●───────────●──────▶
                   ╲   ╱             collision: scattered
                    ● ╱
            collision: scattered
```

The key number is **density** σ (sigma): the chance of a collision per unit of distance. With density
0.01, a ray travels 100 units on average before hitting a particle.

### 1.1 How far until the next collision?

The probability of travelling at least a distance `d` without hitting anything falls off
**exponentially** (this is called Beer–Lambert's law):

```
P(no collision in distance d) = e^(−σ·d)
```

To pick a random collision distance with exactly that distribution, invert it (this is called
**inverse transform sampling**, and it's covered properly in chapter 30):

```
d = −(1/σ) · ln(1 − ξ)          ξ = random number in [0, 1)
```

```cpp
double hit_distance = neg_inv_density * std::log(1.0 - random_double());
```

(`1 − ξ` instead of `ξ` avoids `log(0)`.)

---

## 2. A constant-density volume

We give the medium a **shape** (the "boundary", any closed object such as a box or sphere) and a
**density**. For a ray:

1. Find where the ray **enters** and **leaves** the boundary (two hits: `t1` and `t2`).
2. Pick a random collision distance `d`.
3. If `d` is beyond the exit, the ray passes straight through: **no hit**.
4. Otherwise, report a hit at `t1 + d` with a special material that scatters in a random direction.

```
          boundary
     ┌──────────────────┐
ray ─┼──────●───────────┼──▶       ● = random collision point (if it's before the exit)
     t1     t1+d        t2
```

The volume is a `Hittable` like anything else. It pretends to have a "surface" at the random collision
point. That's why fog works with the rest of the renderer (lists, BVH, lights) with no changes at all.

### 2.1 Where do the particles send light? The phase function

For surfaces, the material decides the bounce direction. For volumes this is the **phase function**.
The simplest is **isotropic**: all directions equally likely (the `Isotropic` material, which uses
`SpherePDF`). Real clouds and haze scatter mostly *forward* (this is why a sunbeam looks brighter when you
look towards the sun); the "Henyey–Greenstein" phase function models that and is a great extension.

**Albedo** of the medium: the fraction of light scattered rather than absorbed. White fog ≈ (1, 1, 1),
black smoke ≈ (0, 0, 0) (all absorbed, so it just darkens things).

### 2.2 Code

```cpp
bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    HitRecord rec1, rec2;
    if (!boundary->hit(r, Interval::universe(), rec1)) return false;            // entry
    if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;  // exit
    // clip to the ray's valid range, handle rays starting INSIDE the volume
    ...
    double hit_distance = neg_inv_density * std::log(1.0 - random_double());
    if (hit_distance > distance_inside_boundary) return false;
    rec.t = rec1.t + hit_distance / ray_length;
    rec.mat = phase_function.get();   // Isotropic
    ...
}
```

We search for the entry with `Interval::universe()` (including negative t) so that rays that **start
inside** the volume (like a ray that just scattered inside it) still find the boundary behind them,
and then the exit ahead.

> **Limitation:** the boundary must be **convex** (a box or a sphere, not a donut), because we only
> look for one entry and one exit.

---

## 3. Variable density: delta tracking

A cloud isn't uniform. It's thick in the middle and wispy at the edges. With varying density σ(p),
the exponential formula no longer works directly. **Delta tracking** (also called Woodcock tracking, from
nuclear physics in the 1960s!) is a neat trick:

1. Pretend the medium has the **maximum** density σ_max everywhere, and take an exponential step.
2. At the new point, accept it as a **real** collision with probability `σ(p) / σ_max`.
3. Otherwise it was a "null collision" (an imaginary particle): keep going from there.

```
 σ_max steps:   ●────●──●────────●───●──── ...
 real?          no   no  YES (thick part)
```

In thin areas most tentative collisions are rejected, so the ray goes through. In thick areas most are
accepted. The result is **exactly** correct, as long as `σ_max ≥ σ(p)` everywhere.

That's `VariableMedium`, which takes the density as a function (a lambda), for example noise.

### `volume.h`

**File: `include/pixel/volume.h`**

```cpp
// pixel/volume.h
// ------------------------------------------------------------
// Participating media: fog, smoke, mist, clouds.
// A ray inside the volume has a chance to scatter at every step.
// Explained in docs/28-volumes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include <functional>
#include "vec3.h"
#include "hittable.h"
#include "material.h"
#include "random.h"

namespace pixel {

// A volume of constant density, shaped like 'boundary' (a closed object).
class ConstantMedium : public Hittable {
public:
    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, const Color& albedo)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, std::shared_ptr<Texture> tex)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(tex)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        // Where does the ray enter and leave the boundary?
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        // Random distance until the next collision with a particle.
        double hit_distance = neg_inv_density * std::log(1.0 - random_double());
        if (hit_distance > distance_inside_boundary) return false;   // passed through

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);
        rec.normal = Vec3(1, 0, 0);   // arbitrary: particles have no surface
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double neg_inv_density;
    std::shared_ptr<Material> phase_function;
};

// A volume whose density changes in space (e.g. smoke, clouds).
// Uses "delta tracking" (Woodcock tracking): max_density must be >= density(p) everywhere.
class VariableMedium : public Hittable {
public:
    VariableMedium(std::shared_ptr<Hittable> boundary, double max_density,
                   std::function<double(const Point3&)> density, const Color& albedo)
        : boundary(boundary), max_density(max_density), density(density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;
        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double t = rec1.t;
        while (true) {
            // Step as if the medium had max density everywhere...
            t -= std::log(1.0 - random_double()) / (max_density * ray_length);
            if (t >= rec2.t) return false;
            // ...then accept a real collision with probability density/max_density.
            if (random_double() < density(r.at(t)) / max_density) break;
        }
        rec.t = t;
        rec.p = r.at(t);
        rec.normal = Vec3(1, 0, 0);
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double max_density;
    std::function<double(const Point3&)> density;
    std::shared_ptr<Material> phase_function;
};

} // namespace pixel
```

---

## 4. The program

**File: `chapters/ch28_volumes.cpp`**

```cpp
// ch28_volumes.cpp
// ------------------------------------------------------------
// Chapter 28: Volumes - smoke, fog and clouds.
//   images/ch28_cornell_smoke.png  - the two boxes turned into smoke and fog
//   images/ch28_cloud.png          - a noise-shaped cloud (variable density)
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Constant density smoke in the Cornell box ---------------
    {
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));

        HittableList world;
        world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
        world.add(std::make_shared<Quad>(Point3(113, 554, 127), Vec3(330, 0, 0), Vec3(0, 0, 305), light));
        world.add(std::make_shared<Quad>(Point3(0, 555, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));

        world.add(std::make_shared<ConstantMedium>(box1, 0.01, Color(0, 0, 0)));   // black smoke
        world.add(std::make_shared<ConstantMedium>(box2, 0.01, Color(1, 1, 1)));   // white fog

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch28_cornell_smoke.png", cam.render(world));
    }

    // ---------- 2. A cloud with density from noise -------------------------
    {
        auto noise = std::make_shared<Perlin>(17);
        // Density: a squashed ball, eroded by noise at the edges.
        auto density = [noise](const Point3& p) {
            Vec3 q(p.x / 2.2, p.y / 1.1, p.z / 1.6);
            double shape = 1.0 - q.length();                         // 1 at center, 0 at the edge
            double n = noise->fbm(p * 1.3, 5);                       // -1 .. 1
            return std::fmax(0.0, shape + 0.6 * n) * 4.0;            // clamp to >= 0
        };
        auto bounds = make_box(Point3(-2.6, -1.4, -2.0), Point3(2.6, 1.4, 2.0), nullptr);
        HittableList world;
        world.add(std::make_shared<VariableMedium>(bounds, 7.0, density, Color(0.95, 0.95, 0.95)));
        world.add(std::make_shared<Sphere>(Point3(0, -1003, 0), 1000, std::make_shared<Lambertian>(hex_color(0x5B7F5A))));

        Camera cam;
        cam.image_width = 480;
        cam.samples_per_pixel = 128;
        cam.max_depth = 30;
        cam.vfov = 40;
        cam.lookfrom = Point3(0, 1, 9);
        cam.lookat = Point3(0, 0.3, 0);
        SkySettings sky;
        sky.sun_direction = unit_vector(Vec3(0.5, 0.6, 0.3));
        cam.background = physical_sky(sky);
        save_image("images/ch28_cloud.png", cam.render(world));
    }
    return 0;
}
```

1. **Cornell smoke**: the two boxes from chapter 27 become a black smoke block and a white fog block. The
   light is bigger here (like in *The Next Week*), to reduce noise.
2. **A cloud**: density = a squashed ball shape, plus noise, clamped to ≥ 0. Lit by the physical sky
   (chapter 33) over a green ground.

```bat
run ch28_volumes
```

---

## 5. What you should see

![Cornell smoke](../images/ch28_cornell_smoke.png)

> **Image description:** The Cornell box (green left wall, red right wall) with a large ceiling light.
> Instead of solid boxes there are two box-shaped volumes: the tall one at the back is **dark grey smoke**,
> semi-transparent, so you can faintly see the walls through it; the short one at the front is **white
> fog**, glowing softly, with fuzzy edges. Both cast soft, faint shadows on the floor.

![Cloud](../images/ch28_cloud.png)

> **Image description:** A single puffy white cloud floating above green grass, against a blue sky
> that fades to pale near the horizon. The cloud is dense and bright on top where the sun hits it, grey
> and shadowed underneath, with irregular, wispy, cauliflower-like edges. It's somewhat grainy.

---

## Try it yourself

1. Change the smoke density from 0.01 to 0.001 and 0.1. How does the look change?
2. Make **colored** fog: albedo `(1.0, 0.6, 0.3)`, like dust at sunset.
3. Put a glass sphere inside the fog box.
4. Fill the **whole** Cornell box with thin fog (density 0.001 inside a box the size of the room) and use
   a small bright light. You'll see a beam of light: **volumetric lighting**.
5. **Challenge:** implement the Henyey–Greenstein phase function with a parameter g (−1..1) and
   make the fog scatter forward (g = 0.7).

## Common problems

| Symptom | Cause |
|---------|-------|
| Volume invisible | Density far too low for the size of the scene (units matter: 555-unit box vs 1-unit spheres) |
| Volume looks like a solid object | Density far too high |
| Volume looks sliced or has hard edges inside | Boundary not convex, or a hole in the boundary mesh |
| Very noisy | Volumes need many samples; light sampling (chapter 31) and denoising (36) help |

---

## Summary

* In a medium, the distance to the next collision is exponential: `d = −ln(1−ξ)/σ`.
* A constant medium is a `Hittable` that reports a random collision inside its boundary.
* Isotropic scattering sends light in all directions; albedo decides how much survives.
* Delta tracking handles varying density exactly by adding imaginary "null" collisions.

🎉 **That's Part 5.** You can build complex scenes. Next, we make them **clean and physically
correct**: [Chapter 29 — Monte Carlo integration →](29-monte-carlo.md)
