# Chapter 33 — Skies and environment lighting

[← Microfacet materials](32-microfacet-materials.md) · [Contents](README.md) · [Next: HDR & tone mapping →](34-hdr-tonemapping.md)

> 📖 **Line by line:** [sky explained line by line](line-by-line/sky.md) · [ch33_sky explained line by line](line-by-line/ch33_sky.md)

---

## Goal

Outdoors, the sky is the most important light there is. In this chapter you'll learn:

* why the sky is blue and sunsets are orange (in one paragraph),
* how our **artistic physical sky** works: zenith, horizon, ground, sun glow,
* why the **sun must be a real light object** (a far-away sphere) instead of a bright spot in the
  background,
* **environment maps**: lighting a scene with a 360° image,
* how the time of day changes the whole mood of a shot.

---

## 1. Why the sky looks the way it does

Sunlight is white. On its way through the atmosphere, it's scattered by air molecules. Small molecules scatter
**blue** light much more than red (**Rayleigh scattering**, strength ∝ 1/wavelength⁴). So:

* Looking **up**, away from the sun, you see blue light scattered towards you from every direction: a
  **blue sky**.
* Near the **horizon**, you look through much more air. Light gets scattered many times and mixes back to
  a pale, whitish blue.
* At **sunset**, sunlight travels through so much air that most of the blue has been scattered away before it
  reaches you: the sun and the sky around it turn **orange and red**.
* Larger particles (dust, water droplets) scatter all colors mostly **forward** (**Mie scattering**): the
  bright white or golden **halo around the sun**.

Physically simulating this (a "spectral atmosphere") is a big project. We use a simple **artistic** model
that captures the look.

---

## 2. Our sky model

`physical_sky(SkySettings)` returns a `Background`: a function from a ray direction to a color.

```
            zenith color  (deep blue)
                 ▲
                 │  blend with pow(d.y, 0.45): quick change near the horizon, slow higher up
                 │
 horizon color ──┼──────────────  (pale / orange at sunset)
                 │  quick fade (smoothstep over 0..0.08)
 ground color ───▼─ (below the horizon)

 + glow around the sun: sun_glow × (cos^8 + 2·cos^64 of the angle to the sun) × glow_strength
```

Two glow terms: a wide soft one (power 8) and a tighter bright one (power 64). The sun direction is used
only for the glow; the sun **itself** is separate.

### `sky.h`

**File: `include/pixel/sky.h`**

```cpp
// pixel/sky.h
// ------------------------------------------------------------
// Backgrounds: what a ray sees when it hits nothing.
// Because our renderer is physically based, the sky is also a
// giant light source that lights the whole scene.
// Explained in docs/33-sky-and-environment.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "image.h"
#include "material.h"
#include "sphere.h"

namespace pixel {

using Background = std::function<Color(const Ray&)>;

// One color in every direction. Color(0,0,0) = a dark room.
inline Background solid_background(const Color& c) {
    return [c](const Ray&) { return c; };
}

// The classic blue-white gradient from "Ray Tracing in One Weekend".
inline Background gradient_sky(const Color& horizon = Color(1.0, 1.0, 1.0),
                               const Color& zenith = Color(0.5, 0.7, 1.0)) {
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double a = 0.5 * (d.y + 1.0);
        return (1.0 - a) * horizon + a * zenith;
    };
}

// A simple artistic "physical-looking" daylight / sunset sky.
struct SkySettings {
    Vec3 sun_direction = unit_vector(Vec3(0.3, 0.5, -1.0));  // points TOWARDS the sun
    Color zenith = Color(0.10, 0.25, 0.65);     // straight up
    Color horizon = Color(0.70, 0.80, 0.95);    // at the horizon
    Color ground = Color(0.18, 0.16, 0.14);     // below the horizon
    Color sun_glow = Color(1.0, 0.7, 0.4);      // halo around the sun
    double glow_strength = 0.6;
    double intensity = 1.0;                     // overall multiplier
    bool draw_sun_disk = false;                 // usually the sun is a real light (make_sun)
    Color sun_radiance = Color(50, 45, 40);
    double sun_angular_radius_deg = 1.5;
};

inline Background physical_sky(const SkySettings& s) {
    return [s](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        Vec3 sun = unit_vector(s.sun_direction);
        double cos_sun = dot(d, sun);
        Color c;
        if (d.y < 0.0) {
            // Ground: fade from horizon color to ground color quickly.
            double t = smoothstep(0.0, 0.08, -d.y);
            c = lerp(s.horizon, s.ground, t);
        } else {
            // Sky: horizon -> zenith. pow makes the horizon band thin.
            double t = std::pow(d.y, 0.45);
            c = lerp(s.horizon, s.zenith, t);
        }
        // Glow around the sun (Mie scattering look), stronger near the horizon.
        double g = std::pow(std::fmax(0.0, cos_sun), 8.0) * s.glow_strength;
        double g2 = std::pow(std::fmax(0.0, cos_sun), 64.0) * s.glow_strength * 2.0;
        c += s.sun_glow * (g + g2);
        if (s.draw_sun_disk) {
            double cos_r = std::cos(degrees_to_radians(s.sun_angular_radius_deg));
            if (cos_sun > cos_r) c += s.sun_radiance;
        }
        return c * s.intensity;
    };
}

// A far-away glowing sphere that acts as the sun. Add it to the world AND to
// the lights list so the renderer can aim shadow rays at it.
inline std::shared_ptr<Sphere> make_sun(const Vec3& direction_to_sun, double angular_radius_deg,
                                        const Color& radiance, double distance = 10000.0) {
    double radius = distance * std::tan(degrees_to_radians(angular_radius_deg));
    Point3 center = unit_vector(direction_to_sun) * distance;
    return std::make_shared<Sphere>(center, radius, std::make_shared<DiffuseLight>(radiance));
}

// A 360 degree photo (equirectangular / "latitude-longitude") as the sky.
inline Background environment_map(std::shared_ptr<Image> env, double intensity = 1.0, double rotation_deg = 0.0) {
    double rot = degrees_to_radians(rotation_deg);
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double phi = std::atan2(d.z, d.x) + rot;
        double theta = std::acos(clampd(d.y, -1.0, 1.0));
        double u = phi / (2 * pi) + 0.5;
        double v = 1.0 - theta / pi;
        return env->sample_bilinear(u, v) * intensity;
    };
}

} // namespace pixel
```

---

## 3. The sun is a light, not a background

The sun is tiny in the sky (about 0.5° across) and **enormously** bright: around 100,000 times brighter than
the sky per unit of solid angle. If it were just a bright disc in the background, diffuse bounces would find
it only by luck: the Cornell box problem again, but much worse (chapter 26).

So `make_sun(direction, angular_radius, radiance)` creates a **real sphere** with a `DiffuseLight`,
10,000 units away, sized to cover the right angle. We add it to both the `world` and the `lights` list,
and light sampling (chapter 31) aims at its tiny cone. The result is crisp, clean sun shadows.

### How bright?

The light a sun of radiance L and angular radius r gives a surface facing it is about
`E = L · π · r²` (r in radians). For crisp daylight, the sun's irradiance should be several times that
of the sky:

* sky radiance ≈ 0.5–1 → sky irradiance ≈ π × 0.75 ≈ 2.4
* sun: `L = 8000`, r = 0.8° = 0.014 rad → E ≈ 8000 × π × 0.000195 ≈ 4.9

That's why the chapter program uses values like 8000. Tone mapping (chapter 34) and exposure handle the rest.

---

## 4. Environment maps

Film VFX teams photograph the real set with a 360° camera in HDR, and use that image to light CG objects
so they fit in perfectly. That image is an **environment map** (or "HDRI"). The standard layout is
**equirectangular** (latitude–longitude): x = angle around (0–360°), y = angle up (from the top to the bottom).

```cpp
double phi = atan2(d.z, d.x) + rotation;        // around
double theta = acos(d.y);                       // from straight up
u = phi / (2π) + 0.5;   v = 1 − theta / π;
color = env->sample_bilinear(u, v) * intensity;
```

The chapter paints a simple "studio" environment in code: bright top, dark floor, and three very bright
rectangles (windows/softboxes). Values like 8.0 are fine, because an environment map is HDR.

> Environment maps with a small bright sun are noisy with our renderer, because we don't importance-sample
> the map. Real renderers build a 2D distribution from the image brightness to sample it. It's a great
> advanced exercise (chapter 40).

---

## 5. The program

**File: `chapters/ch33_sky.cpp`**

```cpp
// ch33_sky.cpp
// ------------------------------------------------------------
// Chapter 33: Skies and environment lighting.
// One scene lit by four different skies. The sky is not just a
// background: it is the main light of every outdoor shot.
//   images/ch33_noon.png
//   images/ch33_golden_hour.png
//   images/ch33_blue_hour.png
//   images/ch33_env_map.png    - lit by a 360-degree image we paint ourselves
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

static void build_scene(HittableList& world) {
    auto ground = std::make_shared<Lambertian>(hex_color(0x8A8A80));
    world.add(std::make_shared<Quad>(Point3(-100, 0, -100), Vec3(200, 0, 0), Vec3(0, 0, 200), ground));
    world.add(std::make_shared<Sphere>(Point3(-1.3, 0.7, 0), 0.7, std::make_shared<Lambertian>(Color(0.8, 0.8, 0.8))));
    world.add(std::make_shared<Sphere>(Point3(0.3, 0.7, -0.6), 0.7, std::make_shared<RoughMetal>(Color(0.95, 0.93, 0.88), 0.15)));
    world.add(std::make_shared<Sphere>(Point3(1.6, 0.5, 0.6), 0.5, std::make_shared<Dielectric>(1.5)));
    std::shared_ptr<Hittable> box = make_box(Point3(-0.4, 0, -0.4), Point3(0.4, 1.6, 0.4),
                                             std::make_shared<Plastic>(hex_color(0x2563EB), 0.3));
    box = std::make_shared<RotateY>(box, 30);
    world.add(std::make_shared<Translate>(box, Vec3(-0.2, 0, -2.2)));
}

static Camera make_camera() {
    Camera cam;
    cam.image_width = 640;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 35;
    cam.lookfrom = Point3(0, 1.6, 7);
    cam.lookat = Point3(0, 0.6, 0);
    cam.max_sample_value = 50;
    return cam;
}

// Render the scene lit by a sky + a sun (the sun is a real, samplable light).
static void render_with(const char* file, SkySettings sky, double sun_elevation_deg, double sun_azimuth_deg,
                        Color sun_radiance, double exposure) {
    HittableList world, lights;
    build_scene(world);
    double el = degrees_to_radians(sun_elevation_deg), az = degrees_to_radians(sun_azimuth_deg);
    Vec3 sun_dir(std::cos(el) * std::sin(az), std::sin(el), -std::cos(el) * std::cos(az));
    sky.sun_direction = sun_dir;
    if (sun_radiance.max_component() > 0) {     // a sun below the horizon adds nothing
        auto sun = make_sun(sun_dir, 0.8, sun_radiance);
        world.add(sun);
        lights.add(sun);
    }

    Camera cam = make_camera();
    cam.background = physical_sky(sky);
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = exposure;
    save_image(file, cam.render(world, &lights), opt);
}

int main() {
    // ---------- Noon: high white sun, deep blue sky -------------------------
    {
        SkySettings s;
        s.zenith = Color(0.15, 0.35, 0.85);
        s.horizon = Color(0.65, 0.78, 0.95);
        s.sun_glow = Color(1.0, 0.95, 0.85);
        s.glow_strength = 0.3;
        s.intensity = 1.0;
        render_with("images/ch33_noon.png", s, 60, 40, Color(8000, 7700, 7200), 0.6);
    }
    // ---------- Golden hour: low orange sun, long shadows -------------------
    {
        SkySettings s;
        s.zenith = Color(0.12, 0.22, 0.55);
        s.horizon = Color(1.0, 0.62, 0.35);
        s.ground = Color(0.2, 0.12, 0.08);
        s.sun_glow = Color(1.0, 0.45, 0.15);
        s.glow_strength = 1.5;
        s.intensity = 0.8;
        render_with("images/ch33_golden_hour.png", s, 6, 60, Color(6000, 3000, 1100), 0.8);
    }
    // ---------- Blue hour: sun just below the horizon ----------------------
    {
        SkySettings s;
        s.zenith = Color(0.03, 0.05, 0.18);
        s.horizon = Color(0.25, 0.28, 0.55);
        s.ground = Color(0.02, 0.02, 0.04);
        s.sun_glow = Color(0.8, 0.35, 0.3);
        s.glow_strength = 0.8;
        s.intensity = 0.6;
        render_with("images/ch33_blue_hour.png", s, -4, 70, Color(0, 0, 0), 2.5);
    }
    // ---------- Environment map: a painted 360 degree "studio" -------------
    {
        auto env = std::make_shared<Image>(512, 256);
        for (int y = 0; y < env->height; y++)
            for (int x = 0; x < env->width; x++) {
                double u = (x + 0.5) / env->width, v = (y + 0.5) / env->height;   // v: 0 = top
                Color c = lerp(Color(0.6, 0.6, 0.65), Color(0.15, 0.13, 0.12), v);   // bright top, dark floor
                // Three bright rectangular "windows"/softboxes around the horizon.
                for (int k = 0; k < 3; k++) {
                    double cu = k / 3.0 + 0.1;
                    if (std::fabs(u - cu) < 0.05 && v > 0.25 && v < 0.45) c = Color(8, 7.5, 7);
                }
                env->at(x, y) = c;
            }
        save_image("images/ch33_env_map_texture.png", *env, SaveOptions{0.3, ToneMapper::Aces, true});

        HittableList world;
        build_scene(world);
        Camera cam = make_camera();
        cam.background = environment_map(env, 1.0, 0.0);
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        save_image("images/ch33_env_map.png", cam.render(world), opt);
    }
    return 0;
}
```

One little scene (a white matte ball, a polished silver ball, a glass ball, and a tall blue plastic
box on a grey ground) is rendered four times.

```bat
run ch33_sky
```

---

## 6. What you should see

![Noon](../images/ch33_noon.png)

> **Image description — noon:** A bright, clear day. Deep blue sky at the top, fading to pale blue at the
> horizon. The objects cast **short, sharp, dark shadows** almost straight down, slightly to one side. The
> white ball is bright on top with a bluish tint in its shadowed side (sky light). The silver ball reflects
> the blue sky and the grey ground. The glass ball throws a bright focused spot (caustic) inside its shadow.

![Golden hour](../images/ch33_golden_hour.png)

> **Image description — golden hour:** The sun is low. The horizon glows orange and the upper sky is a
> deeper blue. **Long shadows** stretch across the ground. The sides of the objects facing the sun are
> painted **warm orange-gold**, while their shadowed sides are cool blue: the classic warm/cool contrast
> photographers love. A golden glow hangs over the horizon where the sun is.

![Blue hour](../images/ch33_blue_hour.png)

> **Image description — blue hour:** The sun has set. No direct light, no hard shadows. Everything is lit
> softly by a dark blue sky with a faint pink-red glow on one side of the horizon. The scene is calm, dim
> and bluish, with only soft contact shadows under the objects.

![Env map](../images/ch33_env_map.png)

> **Image description — environment map:** The same objects lit by the painted studio. There are soft
> shadows in several directions (one per "window"). The silver ball clearly reflects three bright
> rectangles and the light top / dark bottom split of the environment. The background shows the painted
> image itself: a grey gradient with bright white rectangles.

---

## Try it yourself

1. Animate the sun from noon to sunset (elevation 60° → 2°) over 12 frames (chapter 39 shows how).
2. Make an **alien sky**: green zenith, purple horizon, a big pink sun.
3. Add the **sun disk** to the background too (`draw_sun_disk = true`) and compare. Does it change the
   lighting? (It shouldn't: the sphere sun already does that. It only changes what you see when looking at it.)
4. Rotate the environment map (`rotation_deg`) and watch the shadows turn.

## Common problems

| Symptom | Cause |
|---------|-------|
| Very noisy sunlight | Sun not in the `lights` list |
| Shadows fuzzy like on an overcast day | Sun angular radius too large |
| Everything blown out white | Sun far too bright for the exposure: lower the exposure or use ACES |
| No shadows at all | The sun is below the horizon, or hidden by a terrain/wall |

---

## Summary

* Blue sky = Rayleigh scattering; orange sunsets = long path through air; sun halo = Mie scattering.
* Our sky blends zenith, horizon and ground colors, plus a glow around the sun.
* The sun is a real, far-away, very bright sphere, sampled as a light.
* Environment maps light a scene with a 360° HDR image.

🎉 **That's Part 6.** Your renderer is physically based. Now let's make it look like **cinema**:
[Chapter 34 — HDR, exposure and tone mapping →](34-hdr-tonemapping.md)
