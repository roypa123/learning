# Chapter 17 — Diffuse (matte) materials

[← Random numbers & anti-aliasing](16-random-and-antialiasing.md) · [Contents](README.md) · [Next: Metal →](18-metal.md)

---

## Goal

This is the chapter where the images start to look **real**. You'll learn:

* how light bounces off matte ("diffuse") surfaces like paper, clay and chalk,
* how a **path tracer** follows that light backwards, bounce by bounce,
* why **shadows and soft light appear by themselves**, without any shadow code,
* the "shadow acne" bug and how `t_min` fixes it,
* why gamma correction is essential (for real, this time),
* how to use the library's `Camera` and `Material` classes.

---

## 1. How matte surfaces work

Look at a sheet of paper under a lamp. It looks equally bright from every angle. That's because its
surface is microscopically rough and **scatters incoming light in all directions**. Some of the light is
absorbed (that's what gives it a color). The fraction that's reflected, per color channel, is called the
**albedo**:

| Material | Albedo (roughly) |
|----------|------------------|
| fresh snow | 0.8–0.9 |
| white paper | 0.7–0.8 |
| grey concrete | 0.3 |
| grass | 0.2 (more in green) |
| charcoal | 0.04 |

A red paint with albedo (0.8, 0.1, 0.1) reflects 80% of red light and 10% of green and blue.

### 1.1 Lambert's law, as a direction rule

A perfectly matte surface is called **Lambertian** (after Johann Lambert, 1760). The light it scatters
is distributed so that **directions near the normal are more likely** than grazing ones. Precisely, the
probability is proportional to `cos θ`, where θ is the angle from the normal:

```
            normal
              ▲
        ·  ·  │  ·  ·            many bounced rays go roughly "up",
      ·    ·  │  ·    ·          fewer skim along the surface
    ·  ·   ·  │ ·   ·   ·
  ──────────────────────────  surface
```

A neat way to pick such a direction: take the normal, add a random unit vector, and normalize:

```
scatter_direction = normal + random_unit_vector()
```

(Imagine a unit sphere sitting on the surface at the hit point, touching it. Pick a random point
*on that sphere*: the direction from the hit point to it follows exactly the cos θ distribution.
Chapter 30 proves it.) Our library does the same thing through a slightly more general mechanism,
`CosinePDF`, that we'll need later for light sampling. For now, just picture "normal + random".

---

## 2. Path tracing: following light backwards

Real light starts at a lamp or the sun, bounces around, and a tiny fraction enters the camera. We go
backwards: from the camera, into the scene, bouncing, until we reach the sky (or give up).

```
camera ●──▶ hits the ground ──▶ bounces randomly ──▶ hits the sphere ──▶ bounces ──▶ reaches the sky ☁
         color seen = sky_color × albedo_sphere × albedo_ground
```

Each bounce **multiplies** by the surface's albedo, because only that fraction of light survives.

### 2.1 The loop

The heart of our renderer is `Camera::trace` in `camera.h`. Here's a simplified version with only what
matters for this chapter:

```cpp
Color trace(Ray ray) {
    Color radiance(0, 0, 0);       // light collected so far
    Color throughput(1, 1, 1);     // how much of the light at this point reaches the camera

    for (int depth = 0; depth < max_depth; depth++) {
        HitRecord rec;
        if (!world.hit(ray, Interval(t_min, infinity), rec)) {
            radiance += throughput * background(ray);   // escaped to the sky: collect its light
            break;
        }
        ScatterRecord srec;
        if (!rec.mat->scatter(ray, rec, srec)) break;    // absorbed
        throughput *= srec.attenuation;                  // e.g. × albedo
        ray = Ray(rec.p, new_random_direction);          // bounce
    }
    return radiance;
}
```

Read it twice. **This is a path tracer.** Everything else in the book adds to this loop: more materials,
light sampling, volumes. The core stays the same.

* **throughput** starts at white (1, 1, 1). Every bounce multiplies it by the surface color. After
  bouncing off red then grey, it's (0.8·0.5, 0.1·0.5, 0.1·0.5).
* When the ray escapes, we add `throughput × sky_color`: the sky's light, filtered by every surface
  on the way.
* **max_depth** limits the number of bounces. After that we give up and return what we have (a tiny bit
  too dark, but after 50 bounces the throughput is basically zero anyway).

### 2.2 Shadows for free

Why is the ground under the sphere darker? Rays bouncing up from there often hit the sphere
instead of the sky, and need more bounces (each costing 50%) to get out. We never wrote any "shadow
code". **Shadows are an emergent result of the simulation.** So are soft contact shadows, darkening in
corners ("ambient occlusion"), and color bleeding between objects.

---

## 3. Materials in code

A material answers one question: *"a ray hit me here. What happens next?"*

```cpp
class Material {
public:
    virtual Color emitted(...) const { return Color(0,0,0); }          // do I glow? (chapter 26)
    virtual bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const;
    virtual double scattering_pdf(...) const;                          // chapter 30
};
```

`scatter` fills in a `ScatterRecord`:

```cpp
struct ScatterRecord {
    Color attenuation;               // how much of each color survives the bounce
    std::shared_ptr<PDF> pdf_ptr;    // for diffuse-like surfaces: how to pick directions
    bool skip_pdf = false;           // for mirror-like surfaces: direction is fixed...
    Ray skip_pdf_ray;                // ...and this is it
};
```

For a Lambertian surface:

```cpp
bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
    srec.attenuation = tex->value(rec.u, rec.v, rec.p);          // the albedo (a color or a texture)
    srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);      // "normal + random" directions
    srec.skip_pdf = false;
    return true;
}
```

> **About the PDF fields:** a PDF (probability density function) describes *how likely each
> direction is*. For now, `CosinePDF` just means "pick directions the Lambertian way". In chapters 30–31
> we'll use PDFs to aim rays at lights, which removes most of the noise. The math in `trace` is written
> so that, for a Lambertian surface without light sampling, it reduces exactly to
> `throughput *= albedo`.

---

## 4. Shadow acne

When a ray bounces off a surface, the new ray starts **at** the surface. Floating-point numbers are not
exact. The computed hit point might be a hair *below* the surface, and then the bounced ray immediately
hits the same surface again at `t ≈ 0.0000001`, getting darker for no reason:

```
   ideal:     ────●──────── surface           actual:  ────────── surface
                   ↘ bounce                                ●  <- hit point slightly below
                                                            ↘ hits the surface again from inside!
```

The result is **shadow acne**: a speckled, dirty-looking darkening over all surfaces. The fix is to
ignore hits that are extremely close: search from `t_min = 0.001` instead of 0. The camera has a
`t_min` setting, and the chapter program deliberately renders one image with `t_min = 0` so you
can see the bug.

---

## 5. Gamma, for real

Chapter 4 explained the theory. Here it matters in practice: a surface with albedo 0.5 reflects half the
light, and that must *look* half as bright, which in sRGB is byte 188, not 128. The chapter program
saves the same render with and without the sRGB curve. The version without it looks too dark and too
contrasty, which is the classic "CG look" of old renderers that forgot this step.

---

## 6. The Camera class

From now on we use `pixel::Camera`. It wraps everything from chapters 13 and 16 (viewport, pixel
grid, stratified anti-aliasing) plus the path tracing loop, multithreading and more. You set public
fields and call `render`:

```cpp
Camera cam;
cam.aspect_ratio = 16.0 / 9.0;
cam.image_width = 400;
cam.samples_per_pixel = 100;
cam.max_depth = 50;
Image img = cam.render(world);
```

Here's the whole file. Several parts will make sense only in later chapters; each is marked with the
chapter that explains it.

**File: `include/pixel/camera.h`**

```cpp
// pixel/camera.h
// ------------------------------------------------------------
// The Camera: builds rays for every pixel, traces them through the
// world, and averages the results. This is the heart of the renderer.
//   * positionable camera (lookfrom / lookat / vup / vfov)   docs/20
//   * depth of field (defocus blur)                          docs/20
//   * anti-aliasing with stratified samples                  docs/16
//   * multithreading                                         docs/21
//   * motion blur (random ray time)                          docs/22
//   * light sampling with mixture PDFs                       docs/31
//   * Russian roulette + firefly clamping                    docs/31
//   * AOV buffers (albedo / normal) for the denoiser         docs/36
// ------------------------------------------------------------
#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "random.h"
#include "hittable.h"
#include "material.h"
#include "pdf.h"
#include "image.h"
#include "sky.h"

namespace pixel {

class Camera {
public:
    // ---------------- image settings ----------------
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 400;
    int samples_per_pixel = 10;      // rays per pixel: more = less noise, slower
    int max_depth = 10;              // maximum number of bounces

    // ---------------- lens & position ----------------
    double vfov = 90;                        // vertical field of view in degrees
    Point3 lookfrom = Point3(0, 0, 0);       // where the camera is
    Point3 lookat = Point3(0, 0, -1);        // what it looks at
    Vec3 vup = Vec3(0, 1, 0);                // which way is "up"
    double defocus_angle = 0;                // 0 = everything sharp
    double focus_dist = 10;                  // distance that is perfectly sharp
    double shift_x = 0, shift_y = 0;         // lens shift (moves the image window)

    // ---------------- lighting ----------------
    Background background = gradient_sky();

    // ---------------- quality & speed ----------------
    int threads = 0;                    // 0 = use all CPU cores
    bool show_progress = true;
    bool russian_roulette = true;       // randomly stop weak paths (faster, still unbiased)
    int rr_start_depth = 3;
    double max_sample_value = 0.0;      // > 0: clamp very bright samples (removes fireflies)
    uint64_t seed = 1;
    double t_min = 0.001;               // ignore hits closer than this (avoids "shadow acne")

    // ---------------- extra outputs ----------------
    bool collect_aovs = false;          // also fill albedo_aov and normal_aov
    Image albedo_aov;
    Image normal_aov;

    // Extra per-sample information for the denoiser (first surface hit).
    struct AovSlot { Color albedo; Vec3 normal; };

    int image_height() const { int h = (int)(image_width / aspect_ratio); return h < 1 ? 1 : h; }

    // Render the whole image. 'lights' may be nullptr (then no light sampling).
    Image render(const Hittable& world, const Hittable* lights = nullptr) {
        initialize();
        Image img(image_width, height);
        if (collect_aovs) {
            albedo_aov = Image(image_width, height);
            normal_aov = Image(image_width, height);
        }
        light_list = lights;
        if (auto list = dynamic_cast<const HittableList*>(lights))
            if (list->objects.empty()) light_list = nullptr;

        int n_threads = threads > 0 ? threads : (int)std::thread::hardware_concurrency();
        if (n_threads < 1) n_threads = 1;

        std::atomic<int> next_row(0);
        std::atomic<int> rows_done(0);
        std::mutex print_mutex;
        auto start_time = std::chrono::steady_clock::now();

        auto worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1);
                if (j >= height) break;
                // Seed per row: the image is identical no matter how many threads we use.
                seed_thread_rng(seed * 1000003ULL + (uint64_t)j * 7919ULL + 17);
                for (int i = 0; i < image_width; i++) {
                    Color pixel_color(0, 0, 0);
                    Color albedo_sum(0, 0, 0);
                    Vec3 normal_sum(0, 0, 0);
                    for (int s = 0; s < samples_per_pixel; s++) {
                        Ray r = get_ray(i, j, s);
                        AovSlot slot;
                        pixel_color += trace(r, world, collect_aovs ? &slot : nullptr);
                        albedo_sum += slot.albedo;
                        normal_sum += slot.normal;
                    }
                    img.at(i, j) = pixel_color / samples_per_pixel;
                    if (collect_aovs) {
                        albedo_aov.at(i, j) = albedo_sum / samples_per_pixel;
                        normal_aov.at(i, j) = normal_sum / samples_per_pixel;
                    }
                }
                int done = ++rows_done;
                if (show_progress) {
                    std::lock_guard<std::mutex> lock(print_mutex);
                    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                    std::printf("\rRendering: %3d%%  (%d/%d rows, %.1fs)   ", 100 * done / height, done, height, secs);
                    std::fflush(stdout);
                }
            }
        };

        std::vector<std::thread> pool;
        for (int t = 0; t < n_threads; t++) pool.emplace_back(worker);
        for (auto& th : pool) th.join();

        if (show_progress) {
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
            std::printf("\rRendering: done in %.1f seconds using %d threads.            \n", secs, n_threads);
        }
        return img;
    }

    // Trace one ray and return the light (radiance) coming back along it.
    // This is the "path tracer": an iterative loop over bounces.
    Color trace(const Ray& start_ray, const Hittable& world, AovSlot* aov = nullptr) const {
        Color radiance(0, 0, 0);
        Color throughput(1, 1, 1);     // how much light survives the bounces so far
        Ray ray = start_ray;

        for (int depth = 0; depth < max_depth; depth++) {
            HitRecord rec;
            if (!world.hit(ray, Interval(t_min, infinity), rec)) {
                Color bg = background(ray);
                radiance += throughput * bg;
                if (aov && depth == 0) { aov->albedo = bg; aov->normal = Vec3(0, 0, 0); }
                break;
            }
            if (!rec.mat) break;   // an object without a material: treat as black
            if (aov && depth == 0) { aov->albedo = rec.mat->aov_albedo(rec); aov->normal = rec.normal; }

            // 1. Light emitted by the surface itself.
            radiance += throughput * rec.mat->emitted(ray, rec, rec.u, rec.v, rec.p);

            // 2. Does the light bounce?
            ScatterRecord srec;
            if (!rec.mat->scatter(ray, rec, srec)) break;

            if (srec.skip_pdf) {
                // Mirror-like: the direction was decided by the material.
                throughput *= srec.attenuation;
                ray = srec.skip_pdf_ray;
            } else {
                // Diffuse-like: choose a direction, half towards lights, half by the material.
                Vec3 dir;
                double pdf_val;
                if (light_list) {
                    HittablePDF light_pdf(*light_list, rec.p);
                    MixturePDF mix(&light_pdf, srec.pdf_ptr.get());
                    dir = mix.generate();
                    pdf_val = mix.value(dir);
                } else {
                    dir = srec.pdf_ptr->generate();
                    pdf_val = srec.pdf_ptr->value(dir);
                }
                if (pdf_val <= 1e-12) break;
                Ray scattered(rec.p, dir, ray.time());
                double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);
                throughput *= srec.attenuation * (scattering_pdf / pdf_val);
                ray = scattered;
            }

            // 3. Russian roulette: weak paths are stopped at random; survivors get stronger.
            if (russian_roulette && depth >= rr_start_depth) {
                double p = clampd(throughput.max_component(), 0.05, 0.95);
                if (random_double() > p) break;
                throughput /= p;
            }
        }

        if (max_sample_value > 0.0) {
            double m = radiance.max_component();
            if (m > max_sample_value) radiance *= max_sample_value / m;
        }
        if (radiance.x != radiance.x || radiance.y != radiance.y || radiance.z != radiance.z)
            return Color(0, 0, 0);   // drop NaNs
        return radiance;
    }

    // Build the camera coordinate system. Called automatically by render().
    void initialize() {
        height = image_height();
        center = lookfrom;

        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        double viewport_height = 2 * h * focus_dist;
        double viewport_width = viewport_height * ((double)image_width / height);

        // u = right, v = up, w = backwards (camera looks along -w)
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        Vec3 viewport_u = viewport_width * u;      // across the top edge
        Vec3 viewport_v = viewport_height * -v;    // down the left edge
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / height;

        Point3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2
                                   + shift_x * viewport_u - shift_y * viewport_v;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;

        sqrt_spp = (int)std::sqrt((double)samples_per_pixel);
        if (sqrt_spp < 1) sqrt_spp = 1;
    }

    // Ray for pixel (i, j), sample number s.
    Ray get_ray(int i, int j, int s) const {
        double ox, oy;
        if (s < sqrt_spp * sqrt_spp) {
            // Stratified: split the pixel into a grid, one random point per cell.
            int si = s % sqrt_spp, sj = s / sqrt_spp;
            ox = (si + random_double()) / sqrt_spp - 0.5;
            oy = (sj + random_double()) / sqrt_spp - 0.5;
        } else {
            ox = random_double() - 0.5;
            oy = random_double() - 0.5;
        }
        Point3 pixel_sample = pixel00_loc + ((i + ox) * pixel_delta_u) + ((j + oy) * pixel_delta_v);
        Point3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        Vec3 ray_direction = pixel_sample - ray_origin;
        double ray_time = random_double();       // for motion blur: shutter open during [0,1)
        return Ray(ray_origin, ray_direction, ray_time);
    }

private:
    int height = 1;
    Point3 center;
    Point3 pixel00_loc;
    Vec3 pixel_delta_u, pixel_delta_v;
    Vec3 u, v, w;
    Vec3 defocus_disk_u, defocus_disk_v;
    int sqrt_spp = 1;
    const Hittable* light_list = nullptr;

    Point3 defocus_disk_sample() const {
        Vec3 p = random_in_unit_disk();
        return center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
    }
};

} // namespace pixel
```

And here's the first part of `material.h`: the `Material` base class, `ScatterRecord`, and
`Lambertian`. The full file, with metal, glass, lights and more, is shown in chapter 18 and after.

```cpp
struct ScatterRecord {
    Color attenuation;
    std::shared_ptr<PDF> pdf_ptr;
    bool skip_pdf = false;
    Ray skip_pdf_ray;
};

class Lambertian : public Material {
public:
    Lambertian(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Lambertian(std::shared_ptr<Texture> tex) : tex(tex) {}

    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }
private:
    std::shared_ptr<Texture> tex;
};
```

---

## 7. The program

**File: `chapters/ch17_diffuse.cpp`**

```cpp
// ch17_diffuse.cpp
// ------------------------------------------------------------
// Chapter 17: Diffuse (matte) materials.
// From now on we use the library Camera, which traces bounces for us.
//   images/ch17_diffuse.png          - correct result (sRGB gamma)
//   images/ch17_no_gamma.png         - same numbers saved without gamma: too dark
//   images/ch17_shadow_acne.png      - what happens with t_min = 0
//   images/ch17_bounces.png          - max_depth 1, 2, 3, 50 side by side
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    auto grey = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, grey));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, grey));

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = gradient_sky();

    Image img = cam.render(world);
    save_image("images/ch17_diffuse.png", img);

    SaveOptions no_gamma;
    no_gamma.srgb = false;
    save_image("images/ch17_no_gamma.png", img, no_gamma);

    // Shadow acne: rays re-hit the surface they just left because of rounding errors.
    Camera acne = cam;
    acne.t_min = 0.0;
    acne.russian_roulette = false;
    save_image("images/ch17_shadow_acne.png", acne.render(world));

    // How many bounces do we need?
    Camera small = cam;
    small.image_width = 200;
    small.show_progress = false;
    Image strip;
    int depths[4] = {1, 2, 3, 50};
    for (int k = 0; k < 4; k++) {
        small.max_depth = depths[k];
        Image part = small.render(world);
        strip = (k == 0) ? part : post::side_by_side(strip, part, 4);
    }
    save_image("images/ch17_bounces.png", strip);
    return 0;
}
```

```bat
run ch17_diffuse
```

It prints progress while rendering:

```
Rendering:  64%  (144/225 rows, 1.9s)
Rendering: done in 2.9 seconds using 8 threads.
Saved images/ch17_diffuse.png (400x225)
```

---

## 8. What you should see

![Diffuse](../images/ch17_diffuse.png)

> **Image description:** A matte grey ball sitting on a matte grey ground, under the blue-white sky.
> The ball's top is lighter and slightly bluish (it sees the blue sky); its underside is darker. Under
> the ball there's a **soft, dark contact shadow** that fades out smoothly around it. The ground gets a
> little lighter toward the horizon. It looks like a clay ball photographed on an overcast day.

![No gamma](../images/ch17_no_gamma.png)

> **Image description:** The same image but much darker and more contrasty: the ball's lower half
> is nearly black, and the shadow is a harsh dark blob. This is what forgetting the sRGB curve looks like.

![Shadow acne](../images/ch17_shadow_acne.png)

> **Image description:** The same scene, but everything is covered with a fine dark speckle or
> "dirty" noise, and the whole image is darker. Surfaces look like they have measles. That's shadow acne.

![Bounces](../images/ch17_bounces.png)

> **Image description:** Four small renders side by side with max_depth 1, 2, 3 and 50.
> **Depth 1:** the ball and ground are pure black, only the sky is visible (a ray that hits a surface
> can't bounce, so it collects no light). **Depth 2:** the objects appear, lit only by light that
> reaches the sky in one bounce, with very dark shadows. **Depth 3:** brighter, with softer shadows.
> **Depth 50:** the final result, and only slightly brighter than depth 3.

---

## Try it yourself

1. Change the ball's albedo to `Color(0.8, 0.2, 0.2)`. Look at the ground right under the ball: it picks
   up a faint **red glow**. That's color bleeding, and it's free.
2. Change the sky to a sunset: `cam.background = gradient_sky(Color(1.0, 0.6, 0.3), Color(0.2, 0.3, 0.7));`
3. Render with 10, 100 and 1000 samples. Watch the noise decrease (and the time increase).
4. Make the ground **black** (albedo 0). What happens to the shadow and to the ball's underside?
5. Find the time: how long does 100 spp take on your machine? Double the image width. How much longer does
   it take, and why? (Four times, because there are 4× as many pixels.)

## Common problems

| Symptom | Cause |
|---------|-------|
| Speckled dark noise everywhere | Shadow acne: `t_min` is 0 |
| Image too dark and contrasty | Saved without sRGB |
| Image completely black | `max_depth` too small, or `background` is black and there are no lights |
| Crash with a null material | An object was added with `nullptr` as material (the camera now treats that as black) |

---

## Summary

* Matte (Lambertian) surfaces scatter light in random directions, favoring the normal (cos θ).
* A path tracer follows rays backwards from the camera, multiplying by the albedo at each bounce, until
  they escape to the sky.
* Shadows, soft light and color bleeding **emerge** from the simulation.
* Offset bounce rays with `t_min` to avoid shadow acne. Always save with sRGB.

Next: [Chapter 18 — Metal →](18-metal.md)
