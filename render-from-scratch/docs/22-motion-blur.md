# Chapter 22 — Motion blur

[← Multithreading & first masterpiece](21-multithreading-final-scene.md) · [Contents](README.md) · [Next: BVH →](23-bvh.md)

> 📖 **Line by line:** [ch22_motion_blur explained line by line](line-by-line/ch22_motion_blur.md)

---

## Goal

Real cameras keep the shutter open for a short time (for film, usually 1/48 of a second). Anything
that moves during that time leaves a **blur**. Without it, animation looks choppy and fake, like
stop-motion. You'll learn:

* how to simulate a shutter by giving each ray a random **time**,
* how moving objects use that time,
* why this is an elegant example of "integrating by random sampling".

---

## 1. The idea

A pixel in a photo is the **average light over the time the shutter is open**. We already average over
the pixel's area (anti-aliasing) and over the lens (depth of field). Motion blur adds one more
dimension: **time**.

```
shutter opens (t=0)                   shutter closes (t=1)
      │                                     │
      ●  ball here   ─── moving ───▶   ●  ball here
      │                                     │
 each ray picks a random time in [0, 1) and sees the ball wherever it is at that moment;
 averaging many rays gives a smeared, semi-transparent streak
```

### 1.1 In code: two small changes

**1. Rays carry a time.** `Ray` has had a `time` field since chapter 13. The camera fills it in:

```cpp
double ray_time = random_double();       // shutter open during [0, 1)
return Ray(ray_origin, ray_direction, ray_time);
```

Bounced rays inherit the time of the ray that created them (look for `r_in.time()` in the materials), so
the whole light path happens at one instant.

**2. Moving objects use the time.** A moving sphere stores its center as a ray from the start position
to the end position:

```cpp
// Sphere(center1, center2, radius, material): at time 0 it's at center1, at time 1 at center2
Point3 current_center = center.at(r.time());      // linear motion
```

Its bounding box (chapter 23) must include the **whole path**, so it's the union of the boxes at time 0 and time 1.

That's it. Everything else (the averaging) was already there.

### 1.2 Why this is beautiful

We didn't write any "blur" code. There's no smearing filter. We just made the simulation **include
time**, and the correct result appears on its own, including correct shadows of moving objects,
reflections of moving objects in mirrors, moving objects seen through glass... This is the power of
Monte Carlo rendering: **every effect is an integral, and every integral is solved by random samples.**

| Effect | We average over... |
|--------|-------------------|
| Anti-aliasing | positions within the pixel |
| Depth of field | positions on the lens |
| Motion blur | times within the shutter |
| Soft shadows (chapter 26) | positions on the light |
| Glossy reflections | directions around the mirror direction |

---

## 2. The program

**File: `chapters/ch22_motion_blur.cpp`**

```cpp
// ch22_motion_blur.cpp
// ------------------------------------------------------------
// Chapter 22: Motion blur.
// Each ray gets a random time in [0,1). Moving spheres are at a
// different place for each time, so they smear like in a photo.
//   images/ch22_motion_blur.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world;
    auto checker = std::make_shared<CheckerTexture>(0.5, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(checker)));

    Pcg32 rng(11);
    for (int a = -6; a < 6; a++) {
        for (int b = -6; b < 6; b++) {
            Point3 center(a + 0.9 * rng.next_double(), 0.2, b + 0.9 * rng.next_double());
            Color albedo(rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double());
            auto mat = std::make_shared<Lambertian>(albedo);
            // Bounce upwards by a random amount during the shutter time.
            Point3 center2 = center + Vec3(0, 0.5 * rng.next_double(), 0);
            world.add(std::make_shared<Sphere>(center, center2, 0.2, mat));
        }
    }
    // A big sphere flying sideways fast.
    world.add(std::make_shared<Sphere>(Point3(-0.8, 1, 0), Point3(0.8, 1, 0), 1.0,
                                       std::make_shared<Lambertian>(Color(0.8, 0.2, 0.1))));

    BVHNode bvh(world);
    Camera cam;
    cam.image_width = 600;
    cam.samples_per_pixel = 100;
    cam.max_depth = 20;
    cam.vfov = 25;
    cam.lookfrom = Point3(13, 3, 3);
    cam.lookat = Point3(0, 0.5, 0);
    save_image("images/ch22_motion_blur.png", cam.render(bvh));
    return 0;
}
```

The small balls bounce upward during the shutter by a random amount (up to 0.5), and the big red ball flies sideways
1.6 units. The ground uses a checker texture (chapter 24) so you can judge the motion against it.

```bat
run ch22_motion_blur
```

## 3. What you should see

![Motion blur](../images/ch22_motion_blur.png)

> **Image description:** A green-and-white checkered ground with a grid of small matte balls in muted
> colors. Each small ball is **stretched vertically** into a soft capsule shape: sharp-ish at its
> bottom position and fading out at the top, like it was jumping when the photo was taken. Some barely
> moved and look almost round. A large red ball in the center is heavily **smeared horizontally**, a
> semi-transparent red streak through which you can see the ground and balls behind it. The shadows
> underneath the moving balls are blurred too.

---

## Try it yourself

1. Make the shutter shorter: in the camera, change `random_double()` to `0.2 * random_double()`
   (and rebuild). The blur gets 5× shorter. (A real "shutter angle" setting would be a nice feature to
   add to `Camera`.)
2. Make a ball move **towards** the camera. What does the blur look like?
3. Motion blur also works with depth of field. Enable `defocus_angle` too.
4. Advanced: implement **rotation** blur for a spinning object. Hint: rotate the ray into object space
   with an angle that depends on `r.time()` (see chapter 27).

## Common problems

| Symptom | Cause |
|---------|-------|
| Moving objects look sharp | Rays all have time 0; or the object ignores `r.time()` |
| Moving objects get cut off | Bounding box only covers the start position |
| Shadows of moving objects look wrong | Scattered rays don't inherit the incoming ray's time |

---

## Summary

* Each ray gets a random time within the shutter interval.
* Moving objects compute their position from the ray's time.
* Averaging over time gives motion blur, with no special blur code: it's just another integral.

Next: [Chapter 23 — Bounding volume hierarchies →](23-bvh.md)
