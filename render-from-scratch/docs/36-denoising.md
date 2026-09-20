# Chapter 36 — Denoising

[← Post-processing](35-post-processing.md) · [Contents](README.md) · [Next: SDFs & ray marching →](37-sdf-raymarching.md)

> 📖 **Line by line:** [ch36_denoise explained line by line](line-by-line/ch36_denoise.md)

---

## Goal

Getting a clean path-traced image takes a lot of samples (the 1/√N rule). Modern renderers cheat: they
render **fewer** samples and then **remove the noise** with a smart filter. You'll learn:

* why a normal blur is a bad denoiser,
* **edge-aware** filtering (the **bilateral** filter),
* **AOVs** (extra output images: albedo and normals) and why they make denoising much better,
* the limits of hand-made denoisers vs the neural-network ones used in production.

---

## 1. Why not just blur?

Noise is random pixel-to-pixel variation; blurring averages neighbors, so noise goes away... along with
**every** edge, texture and detail. A blurred render looks like it was taken through frosted glass.

We want to average only the neighbors that belong to **the same surface** as the current pixel.

---

## 2. The bilateral filter

A **bilateral** filter weighs each neighbor by two things:

1. **distance** in the image (like a Gaussian blur): closer neighbors count more,
2. **similarity**: neighbors with a very different color count less.

```
weight = exp(−distance² / 2σ_s²) × exp(−color_difference² / 2σ_c²)
```

Across an edge (dark wall next to bright floor) the color difference is big, so the weight is tiny and the
edge survives. Inside a flat area, colors are similar (up to noise), so they get averaged.

The catch: in a *noisy* image, the colors themselves are unreliable. Is that a dark pixel because of an
edge or because of noise? The filter can't tell.

---

## 3. Guide images (AOVs)

The renderer knows far more than the final color. For each pixel we can also save, at the **first hit**:

* the **albedo** (the surface color, before lighting): noise-free, because it doesn't depend on random bounces,
* the **normal**: also noise-free.

These extra outputs are called **AOVs** ("arbitrary output variables"). They are cheap: we already compute
them.

```
 final color (noisy)      albedo (clean)           normals (clean)
 ░▒▓█▒░▓▒░█▓               checker squares,         smooth gradients,
 grainy                    sharp edges               sharp object outlines
```

A **joint** (or **cross**) bilateral filter uses these guides for the similarity test: two neighbors are
"the same surface" if they have similar normals **and** similar albedo. Texture edges (albedo) and
geometric edges (normals) are preserved even when the color is noisy.

Our `denoise` multiplies four weights:

```cpp
w  = exp(−d² / 2σ_spatial²);                 // distance
w *= exp(−dl² / 2σ_color²);                  // relative luminance difference
w *= exp(−|n − n0|² / 2σ_normal²);           // normal difference
w *= exp(−|a − a0|² / 2σ_albedo²);           // albedo difference
```

Using a *relative* luminance difference (`(l − l0) / (0.1 + l0)`) makes it work equally in dark and bright
areas. Running it for 2 passes smooths more.

The camera collects AOVs when you set `cam.collect_aovs = true`, and fills `cam.albedo_aov` and
`cam.normal_aov` (averaged over each pixel's samples, so edges are anti-aliased too).

---

## 4. Production denoisers

Film and game renderers use **neural network** denoisers (like Intel's Open Image Denoise and NVIDIA's
OptiX denoiser) trained on thousands of noisy/clean image pairs, using exactly the same inputs: noisy
color + albedo + normal. They're dramatically better than hand-written filters, but they are large external
libraries, and this book uses none. Our filter shows the principle; its results are good for moderate noise and
visibly "painterly" for heavy noise.

A rule of thumb that holds for all denoisers: **the better the input, the better the output.** Denoising
16 samples looks OK; denoising 128 samples usually looks perfect.

---

## 5. The program

**File: `chapters/ch36_denoise.cpp`**

```cpp
// ch36_denoise.cpp
// ------------------------------------------------------------
// Chapter 36: Denoising.
// Render with FEW samples, then clean the noise with an edge-aware
// filter guided by extra images (albedo & normals).
//   images/ch36_noisy.png        16 samples per pixel
//   images/ch36_albedo.png       surface color at the first hit (AOV)
//   images/ch36_normals.png      surface normals at the first hit (AOV)
//   images/ch36_blur.png         a plain Gaussian blur (for comparison: BAD)
//   images/ch36_denoised.png     our joint bilateral denoiser
//   images/ch36_reference.png    512 samples per pixel (the "truth")
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world, lights;
    auto checker = std::make_shared<CheckerTexture>(0.6, Color(0.7, 0.7, 0.7), Color(0.15, 0.15, 0.15));
    world.add(std::make_shared<Quad>(Point3(-20, 0, -20), Vec3(40, 0, 0), Vec3(0, 0, 40), std::make_shared<Lambertian>(checker)));
    world.add(std::make_shared<Sphere>(Point3(-1.2, 0.7, 0), 0.7, std::make_shared<Lambertian>(hex_color(0x2F6DB5))));
    world.add(std::make_shared<Sphere>(Point3(0.4, 0.5, 0.8), 0.5, std::make_shared<Plastic>(hex_color(0xD97706), 0.3)));
    world.add(std::make_shared<Sphere>(Point3(1.5, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.9, 0.9, 0.9), 0.25)));
    auto lamp = std::make_shared<Quad>(Point3(-2, 4, 2), Vec3(0, 0, -2), Vec3(2, 0, 0), std::make_shared<DiffuseLight>(Color(10, 9, 8)));
    world.add(lamp);
    lights.add(lamp);

    Camera cam;
    cam.image_width = 480;
    cam.max_depth = 20;
    cam.vfov = 40;
    cam.lookfrom = Point3(0, 2.2, 6);
    cam.lookat = Point3(0, 0.6, 0);
    cam.background = gradient_sky(Color(0.15, 0.15, 0.18), Color(0.25, 0.3, 0.4));
    cam.max_sample_value = 20;
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;

    // Noisy render + guide images.
    cam.samples_per_pixel = 16;
    cam.collect_aovs = true;
    Image noisy = cam.render(world, &lights);
    save_image("images/ch36_noisy.png", noisy, opt);
    save_image("images/ch36_albedo.png", cam.albedo_aov);
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch36_normals.png", post::visualize_normals(cam.normal_aov), raw);

    // A plain blur: removes noise AND all detail.
    save_image("images/ch36_blur.png", post::gaussian_blur(noisy, 4), opt);

    // Our denoiser.
    post::DenoiseSettings ds;
    ds.radius = 7;
    ds.passes = 2;
    Image clean = post::denoise(noisy, cam.albedo_aov, cam.normal_aov, ds);
    save_image("images/ch36_denoised.png", clean, opt);

    // The reference: many more samples (this takes a while).
    cam.samples_per_pixel = 512;
    cam.collect_aovs = false;
    save_image("images/ch36_reference.png", cam.render(world, &lights), opt);
    return 0;
}
```

```bat
run ch36_denoise
```

The last image (512 samples, the "ground truth") takes the longest.

---

## 6. What you should see

![Noisy](../images/ch36_noisy.png)

> **Image description:** A checkered floor with a blue matte ball, an orange glossy ball and a rough silver
> ball, lit by a square light above, with 16 samples per pixel: recognizable, but covered in heavy grain,
> especially in the shadows.

![Albedo](../images/ch36_albedo.png) ![Normals](../images/ch36_normals.png)

> **Image descriptions:** **Albedo:** a flat, unlit version of the scene: the checkerboard in crisp light
> and dark grey, the balls in their pure colors, and the sky gradient. No shading, no noise. **Normals:** the
> scene in pastel colors: the floor uniform light green (facing up), the balls as smooth rainbow gradients,
> the sky black.

![Blur](../images/ch36_blur.png)

> **Image description:** The noisy image with a plain Gaussian blur: the noise is gone, but so is every
> detail. The checkerboard is a grey smear and the ball outlines are fuzzy.

![Denoised](../images/ch36_denoised.png)

> **Image description:** The denoised result: smooth shading and soft shadows with little grain left, while the
> checkerboard squares and the ball outlines stay sharp. Some fine lighting details look slightly smoothed,
> "painted".

![Reference](../images/ch36_reference.png)

> **Image description:** The 512-sample reference: clean everywhere. The denoised 16-sample version is
> surprisingly close to it, at about 1/30 of the render time.

---

## Try it yourself

1. Try `ds.passes = 1` and `3`, and `radius = 3` and `12`. Watch the trade-off between smoothness and detail.
2. Denoise without guides: pass empty images (`Image()`) for albedo and normal. What's lost?
3. Denoise the 128-sample version of the chapter 35 night scene.
4. **Challenge:** implement the "À-Trous wavelet" filter (Dammertz et al. 2010): several passes with
   growing gaps between samples. It's fast and used in real-time ray tracing.

## Common problems

| Symptom | Cause |
|---------|-------|
| Everything blurry | Color/normal/albedo sigmas too large |
| Noise barely reduced | Sigmas too small, or radius too small |
| Blotchy "paint" look | Too little input quality: more samples, fewer passes |
| Albedo AOV black | `collect_aovs` not enabled before `render` |

---

## Summary

* Plain blur destroys detail; edge-aware filters average only similar neighbors.
* Albedo and normal AOVs are noise-free guides that preserve texture and geometry edges.
* Production uses neural denoisers with the same inputs; ours shows the principle.

Next: [Chapter 37 — Signed distance functions and ray marching →](37-sdf-raymarching.md)
