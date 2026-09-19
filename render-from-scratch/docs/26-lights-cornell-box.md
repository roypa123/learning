# Chapter 26 — Lights and the Cornell box

[← Quads, triangles & meshes](25-quads-triangles-meshes.md) · [Contents](README.md) · [Next: Instances →](27-instances.md)

---

## Goal

So far all light came from the sky. Now objects themselves can **glow**. You'll learn:

* **emissive** materials (`DiffuseLight`),
* how the path tracer collects emitted light,
* **area lights** and why they make soft shadows,
* the **Cornell box**, the most famous test scene in rendering,
* why these images are **noisy**, and what we'll do about it.

---

## 1. Emission

A light source is just a surface that **emits** light. In the path tracer loop (chapter 17) there's a
line we haven't talked about:

```cpp
radiance += throughput * rec.mat->emitted(ray, rec, rec.u, rec.v, rec.p);
```

At every hit, we add the light the surface itself produces, filtered by the throughput so far. For
normal materials `emitted` returns black. For `DiffuseLight` it returns its color:

```cpp
class DiffuseLight : public Material {
public:
    Color emitted(const Ray&, const HitRecord& rec, double u, double v, const Point3& p) const override {
        if (!rec.front_face && !two_sided) return Color(0, 0, 0);   // lights shine one way
        return tex->value(u, v, p);
    }
    // scatter() is not overridden -> returns false: lights don't reflect anything
};
```

* **Brightness above 1**: a light with emission `(15, 15, 15)` is 15 times brighter than a white
  surface in the sky's light. Real lights are **much** brighter than surfaces, so values like 4, 15 and 400
  are normal.
* **One-sided**: a ceiling lamp panel shines down, not up. Only the **front** face emits (the side the
  normal points to, remember that the order of `u` and `v` in a quad decides this). `two_sided = true`
  makes both sides glow.

With lights in the scene, we can set the background to **black** and see *only* the light from the
lamps: `cam.background = solid_background(Color(0, 0, 0));`

---

## 2. Area lights and soft shadows

A tiny point light casts hard shadows. A **big** light (a window, an overcast sky, a softbox) casts **soft**
shadows, because a point partially hidden from the light sees only part of it:

```
      ████████████  area light
         ╲      ╱
          ╲    ╱
            ●  object
          ╱  ╲
    ─────▓▓▒▒░░──────  floor
         │ │ └ penumbra: part of the light visible  -> half-lit
         │ └── umbra: light fully hidden             -> dark
```

We get this for free: rays bouncing off the floor near the object hit the light's surface only some of
the time. The average is a smooth gradient: a **penumbra**.

---

## 3. The Cornell box

In 1984, researchers at Cornell University built a real box with colored walls and a light in the
ceiling, photographed it, and compared the photo to their renders. The **Cornell box** has been the "hello
world" of physically based rendering ever since:

```
        ┌──────────────── ceiling (white) with a light panel ──┐
        │                    ████████                           │
        │                                                       │
 green  │                                                       │  red
  wall  │            ┌──┐                                       │  wall
 (left) │            │  │ tall box            ┌───┐            │ (right)
        │            │  │                     │   │ short box   │
        └────────────┴──┴─────────────────────┴───┴────────────┘
                      floor (white), back wall (white), open front
```

(The original 1984 box had red on the left. Our coordinates follow the popular *Ray Tracing: The
Next Week* setup, where the red wall is at x = 0, the green wall at x = 555, and the camera looks in from
z = −800, so green ends up on the **left** of the image.)

It shows off everything global illumination does:

* **color bleeding**: the white boxes pick up red and green light from the walls,
* **soft shadows** from the area light,
* **indirect light**: the ceiling is lit only by light bounced from the floor and walls,
* subtle **darkening in the corners**.

In our program the box is 555 units wide (the original measurements in millimeters). The camera sits at
z = −800 looking in through the open front.

---

## 4. Why so noisy?

Render it and you'll see a very **noisy** image: lots of white and colored speckles ("fireflies"), even at
200 samples per pixel. Why?

The light panel is small: only about 5% of the ceiling. A diffuse bounce from the floor goes in a random
direction, and **very few** of those random directions hit the light. Most paths never find it and bring back
black. The rare ones that do find it bring back a very bright value (15 × albedos). The average is correct,
but the variance (noise) is huge.

```
 floor point ●  sends random rays:  ↗ ↑ ↖ ← → ↗ ↑ ...
                 only ~1 in 30 hits the small light panel
                 -> most samples: dark, a few: very bright -> NOISE
```

This is **the** central problem of path tracing, and chapters 29–31 solve it properly with **light
sampling**: sending some rays *on purpose* towards the light. The same scene becomes nearly clean at the
same sample count. For now, enjoy the noisy but correct result.

---

## 5. The program

**File: `chapters/ch26_cornell_box.cpp`**

```cpp
// ch26_cornell_box.cpp
// ------------------------------------------------------------
// Chapter 26: Lights and the Cornell box.
//   images/ch26_light_quad.png    - a glowing rectangle lights a marble scene
//   images/ch26_cornell_empty.png - the classic empty Cornell box
// Note: without light sampling (chapter 31) these images are NOISY.
// That is expected!
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. A simple area light --------------------------------------
    {
        auto marble = std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble);
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(marble)));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 0), 2, std::make_shared<Lambertian>(marble)));

        auto difflight = std::make_shared<DiffuseLight>(Color(4, 4, 4));
        world.add(std::make_shared<Sphere>(Point3(0, 7, 0), 2, difflight));
        world.add(std::make_shared<Quad>(Point3(3, 1, -2), Vec3(2, 0, 0), Vec3(0, 2, 0), difflight));

        Camera cam;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));   // no sky: darkness
        cam.vfov = 20;
        cam.lookfrom = Point3(26, 3, 6);
        cam.lookat = Point3(0, 2, 0);
        save_image("images/ch26_light_quad.png", cam.render(world));
    }

    // ---------- 2. Cornell box ----------------------------------------------
    {
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

        HittableList world;
        world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
        // The light's normal must point DOWN into the room: u x v = (-,0,0)x(0,0,-) points down.
        world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(555, 555, 555), Vec3(-555, 0, 0), Vec3(0, 0, -555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch26_cornell_empty.png", cam.render(world));
    }
    return 0;
}
```

Two scenes:

1. A marble sphere on a marble ground, lit **only** by a glowing sphere above and a glowing rectangle
   beside it, with a black background.
2. The empty Cornell box.

```bat
run ch26_cornell_box
```

---

## 6. What you should see

![Light quad](../images/ch26_light_quad.png)

> **Image description:** A dark scene: a marble-textured sphere on a marble floor, lit only by a white
> glowing sphere hovering above it (appearing as a bright disc at the top of the frame) and a small glowing white
> rectangle to its right. The marble sphere is lit from above and from the side, with soft light
> falloff and soft shadows on the floor. The rest of the scene fades into black. The image has visible
> grainy noise in the darker areas.

![Cornell empty](../images/ch26_cornell_empty.png)

> **Image description:** A square image of the empty Cornell box: a white room seen through its open
> front, **green** left wall, **red** right wall, white floor, back wall and ceiling, and a bright white
> rectangular light in the ceiling. The ceiling near the light is fairly dark (it's lit only by bounced light);
> the floor under the light is bright. The white surfaces near the colored walls are faintly tinted green or
> red. The whole image is **grainy**, especially the ceiling and corners, with scattered bright
> speckles.

---

## Try it yourself

1. Make the light **colored** (warm: `(15, 11, 7)`) and see the whole room change mood.
2. Make the light bigger (the whole ceiling). Less noise, softer shadows. Why less noise?
3. Swap the light quad's `u` and `v`. The light "disappears": it now shines up into the ceiling.
4. Put a glowing sphere in the middle of the box (like a lamp). Look for the soft shadows it casts.
5. Render the Cornell box with 1000 or 5000 samples and compare. Note the time.

## Common problems

| Symptom | Cause |
|---------|-------|
| Scene completely black | Light facing the wrong way, or background black and no light hit |
| Light looks white but illuminates nothing | Light too dim: emission must be well above 1 |
| Extreme noise / fireflies | Normal for small lights without light sampling (see chapter 31) |

---

## Summary

* Emissive materials add their light at each hit: `radiance += throughput × emitted`.
* Area lights give soft shadows; real lights are much brighter than 1.
* The Cornell box shows color bleeding, soft shadows and indirect light.
* Small lights → noisy images, because random bounces rarely find them. Light sampling fixes this (chapter 31).

Next: [Chapter 27 — Instances: moving and rotating things →](27-instances.md)
