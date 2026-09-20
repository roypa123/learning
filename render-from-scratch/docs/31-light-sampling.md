# Chapter 31 — Light sampling and mixture PDFs

[← Importance sampling](30-importance-sampling.md) · [Contents](README.md) · [Next: Microfacet materials →](32-microfacet-materials.md)

> 📖 **Line by line:** [ch31_light_sampling explained line by line](line-by-line/ch31_light_sampling.md)

---

## Goal

Fix the noise problem from chapter 26 for good. You'll learn:

* how to send rays **towards the lights** on purpose (light sampling),
* converting "a random point on a light" into a **direction PDF** (area → solid angle),
* sampling the **cone** of directions towards a sphere,
* why we **mix** light sampling with material sampling (a mixture PDF, a simple form of **multiple
  importance sampling**),
* **Russian roulette** and **firefly clamping**, two practical tricks production renderers use.

The result: a Cornell box that's clean at 64 samples, where before it was a mess at 200.

---

## 1. The idea: aim at the light

In the Cornell box, most diffuse bounces from the floor miss the small ceiling light (chapter 26). But
we **know where the light is**. So: pick a random **point on the light**, and send the bounce ray there.

```
      ████  light
       ↑ ↗
       │╱        instead of random directions,
  ─────●─────    aim at random points ON the light
      floor
```

This is importance sampling (chapter 30) with a PDF concentrated where the incoming light is strongest.

### 1.1 From a point on the light to a direction PDF

We pick points **uniformly by area** on the light: density `1/A` per unit area. But our Monte Carlo
estimator (chapter 30) works with a density over **directions** (solid angle). How much solid angle does a
small patch `dA` of the light cover, seen from our point?

```
dω = dA · cos θ_light / distance²
```

* farther away → smaller (÷ distance²),
* tilted away → smaller (× cos θ_light, the angle between the light's normal and the direction back to us).

So the direction PDF is:

```
p(ω) = (1/A) · distance² / cos θ_light
```

That's `Quad::pdf_value`:

```cpp
double distance_squared = rec.t * rec.t * direction.length_squared();
double cosine = std::fabs(dot(direction, rec.normal) / direction.length());
return distance_squared / (cosine * area);
```

and `Quad::random(origin)` returns `random_point_on_quad − origin`. Triangles work the same way (with a
uniform point on the triangle).

### 1.2 Spheres: sample the cone

For a **sphere** light, picking a uniform point on its surface wastes half the samples (the back half is
hidden). Better: sample directions **uniformly inside the cone** that the sphere covers as seen from our
point:

```
            ╱‾‾‾‾╲
   ●───────(  ⊙   )      cos θ_max = sqrt(1 − R²/d²)
     ╲θmax  ╲____╱       solid angle = 2π (1 − cos θ_max)
                         pdf = 1 / solid angle
```

That's `Sphere::pdf_value` and `Sphere::random` (with `random_to_sphere`). This is how we sample the
**sun** in chapter 33: a tiny cone of directions.

### 1.3 Many lights

`HittableList` can also act as a light list: `random()` picks one light at random, and `pdf_value()` is
the **average** of the lights' PDFs (each chosen with probability 1/N).

---

## 2. Why not ONLY sample the light?

Light sampling is great for diffuse surfaces and small lights. But it's bad when:

* the light is **huge** (the sky, a wall-sized window), because the material's own distribution would be better,
* the surface is **glossy**, and only a narrow set of directions matters,
* the light is **hidden** from the point, and many light samples are wasted on shadowed directions.

Each strategy is good in some situations and bad in others. So we **combine** them: flip a coin, and
sample either the light or the material:

```
p_mix(ω) = 0.5 · p_light(ω) + 0.5 · p_material(ω)
```

The crucial part: we must divide by the **mixture density** `p_mix`, whichever strategy actually produced
the direction. Then samples that *both* strategies consider likely aren't over-counted, and a sample that one
strategy made but the other considers extremely unlikely doesn't blow up (the other half keeps `p_mix`
from being tiny). This is the **one-sample balance heuristic** of **multiple importance sampling**
(Eric Veach, 1995, one of the most important ideas in rendering).

```cpp
HittablePDF light_pdf(*light_list, rec.p);
MixturePDF mix(&light_pdf, srec.pdf_ptr.get());
dir = mix.generate();
pdf_val = mix.value(dir);
double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);
throughput *= srec.attenuation * (scattering_pdf / pdf_val);
```

That's the whole change in `Camera::trace`. And the math from chapter 30 guarantees the image converges to
the **same** result; only the noise changes.

### 2.1 How to use it

Pass a list of the light **shapes** as the second argument of `render`:

```cpp
HittableList lights;
lights.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), nullptr));
Image img = cam.render(world, &lights);
```

The shapes in `lights` only need geometry (the material can be `nullptr`). They're used to *choose
directions*; the real lights in `world` still do the emitting.

### 2.2 Aiming at glass

A glass ball **focuses** light into a bright spot (a **caustic**). The light that reaches the floor
through the ball is hard to find by random bounces. Adding the glass sphere to the "lights" list (even
though it doesn't emit) makes the renderer send more rays towards it, which cleans up the caustic.
Any object can be in the list: it just says "important directions are over there".

---

## 3. Two practical tricks

### 3.1 Russian roulette

Deep paths carry little light (throughput shrinks at each bounce), but they still cost as much to
trace. **Russian roulette** stops paths randomly, **without bias**:

```cpp
if (russian_roulette && depth >= rr_start_depth) {
    double p = clampd(throughput.max_component(), 0.05, 0.95);   // survival probability
    if (random_double() > p) break;                              // path dies
    throughput /= p;                                             // survivors count more
}
```

A weak path (throughput 0.1) survives only 10% of the time, but when it does, it counts 10× as much. On
average the result is unchanged (so there's no bias), and we save lots of work. This is how production
renderers can allow hundreds of bounces.

### 3.2 Firefly clamping

Some rare paths carry enormous values: a diffuse bounce that hits a tiny, extremely bright light through
glass. They show up as isolated bright pixels called **fireflies**, which take forever to average out.
Clamping each sample's brightness to a maximum (`cam.max_sample_value`) removes them. It's slightly
**biased** (it loses a little energy), but in film production that's a common, accepted trade-off.

---

## 4. The program

**File: `chapters/ch31_light_sampling.cpp`**

```cpp
// ch31_light_sampling.cpp
// ------------------------------------------------------------
// Chapter 31: Light sampling with mixture PDFs.
// Same Cornell box, same number of samples (64):
//   left  = only BSDF sampling (rays bounce randomly, hoping to find the light)
//   right = half the rays aimed AT the light, half by the material
//   images/ch31_compare.png
//   images/ch31_cornell_final.png   - the finished box with a glass ball (more samples)
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

static void build_cornell(HittableList& world, HittableList& lights, bool glass_ball) {
    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

    world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
    world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
    world.add(std::make_shared<Quad>(Point3(0, 555, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

    auto aluminum = std::make_shared<Metal>(Color(0.8, 0.85, 0.88), 0.0);
    std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), glass_ball ? aluminum : white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
    world.add(box1);

    if (glass_ball) {
        world.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, std::make_shared<Dielectric>(1.5)));
    } else {
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);
    }

    // The list of things worth aiming at. Materials don't matter here: only the shape.
    lights.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), nullptr));
    if (glass_ball)   // glass focuses light (caustics): aiming at it helps too
        lights.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, nullptr));
}

static Camera cornell_camera(int spp) {
    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = spp;
    cam.max_depth = 50;
    cam.background = solid_background(Color(0, 0, 0));
    cam.vfov = 40;
    cam.lookfrom = Point3(278, 278, -800);
    cam.lookat = Point3(278, 278, 0);
    return cam;
}

int main() {
    {
        HittableList world, lights;
        build_cornell(world, lights, false);
        Camera cam = cornell_camera(64);
        Image without = cam.render(world, nullptr);
        Image with = cam.render(world, &lights);
        save_image("images/ch31_compare.png", post::side_by_side(without, with, 8));
    }
    {
        HittableList world, lights;
        build_cornell(world, lights, true);
        Camera cam = cornell_camera(400);
        save_image("images/ch31_cornell_final.png", cam.render(world, &lights));
    }
    return 0;
}
```

```bat
run ch31_light_sampling
```

The second image (400 samples) takes a few minutes: it's the showpiece.

---

## 5. What you should see

![Compare](../images/ch31_compare.png)

> **Image description:** Two Cornell boxes side by side (green left wall, red right wall, two white
> boxes), both rendered with 64 samples per pixel. **Left (material sampling only):** extremely grainy,
> dark and speckled, with many white "firefly" dots; the ceiling and the shadows are barely
> recognizable. **Right (light + material sampling):** clean and smooth, with soft shadows under the boxes,
> green and red color bleeding on the boxes' sides, and only a fine, even grain.

![Cornell final](../images/ch31_cornell_final.png)

> **Image description:** The Cornell box with a tall **mirror-aluminium** box at the back left,
> reflecting the room around it, and a **glass ball** at the front right. The glass ball shows
> an inverted, distorted view of the room, and on the floor under it there's a bright, focused
> **caustic** spot where the ball concentrates the ceiling light. Clean, soft lighting throughout: it looks
> like a photograph of a real miniature room.

---

## Try it yourself

1. Remove the glass sphere from `lights` and compare the caustic noise.
2. Change the mixture weights to 0.8 light / 0.2 material (edit `MixturePDF`). Is it better or worse
   for the Cornell box? For a glossy floor?
3. Set `cam.russian_roulette = false` and compare render times.
4. Put three small lights in the ceiling and add all of them to `lights`.
5. Render a scene with light sampling where the light is **behind** an object from most points. See how
   the material half of the mixture saves the day.

## Common problems

| Symptom | Cause |
|---------|-------|
| Image much brighter/darker with light sampling | `pdf_value` doesn't match `random` (e.g. area in the wrong units) |
| Light sampling does nothing | The `lights` list is empty or not passed to `render` |
| Black spots near the light | Points on the light's plane: `cosine ≈ 0` → divide by ~0 (guarded) |
| Lights in `lights` but not in `world` | They won't emit! Keep the real light in `world` |

---

## Summary

* Light sampling picks directions towards lights: `p = distance² / (cos θ_light · A)` for quads, cone
  sampling for spheres.
* A 50/50 mixture of light and material sampling, divided by the mixture density, is robust (a form of MIS).
* Russian roulette ends weak paths without bias; clamping removes fireflies with a little bias.
* Same image, a fraction of the noise.

Next: [Chapter 32 — Microfacet materials (GGX) →](32-microfacet-materials.md)
