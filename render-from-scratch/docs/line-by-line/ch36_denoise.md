# Line by line: `ch36_denoise.cpp`

[← Line-by-line index](README.md) · [Chapter 36 (the theory)](../36-denoising.md) · [post.h: denoise](post.md)

**What the whole program does, in one sentence:** it renders a scene with only 16 samples (very noisy), saves the
denoiser's guide images, compares a plain blur with our edge-aware denoiser, and finally renders a 512-sample
reference to compare against.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–14 | |
| B. The scene | 16–25 | checkered floor, three balls, a lamp |
| C. Camera | 27–36 | |
| D. Noisy render + guides | 38–46 | 16 samples, albedo and normal images |
| E. A plain blur | 48–49 | for comparison |
| F. The denoiser | 51–56 | |
| G. The reference | 58–61 | 512 samples |
| H. End | 62–63 | |

---

## Block A — Comments, includes (lines 1–14)

Comments listing the six images, our library, `using namespace pixel`.

---

## Block B — The scene (lines 16–25)

```cpp
    HittableList world, lights;
    auto checker = std::make_shared<CheckerTexture>(0.6, Color(0.7, 0.7, 0.7), Color(0.15, 0.15, 0.15));
    world.add(std::make_shared<Quad>(Point3(-20, 0, -20), Vec3(40, 0, 0), Vec3(0, 0, 40), std::make_shared<Lambertian>(checker)));
```

Lines 17–19: a light/dark checkered floor. The checker is on purpose: it's a **texture edge**, and we want to see
whether the denoiser keeps it sharp.

```cpp
    world.add(std::make_shared<Sphere>(Point3(-1.2, 0.7, 0), 0.7, std::make_shared<Lambertian>(hex_color(0x2F6DB5))));
    world.add(std::make_shared<Sphere>(Point3(0.4, 0.5, 0.8), 0.5, std::make_shared<Plastic>(hex_color(0xD97706), 0.3)));
    world.add(std::make_shared<Sphere>(Point3(1.5, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.9, 0.9, 0.9), 0.25)));
```

Lines 20–22: a blue matte ball, an orange glossy plastic ball, and a rough silver metal ball (three different kinds of
noise).

```cpp
    auto lamp = std::make_shared<Quad>(Point3(-2, 4, 2), Vec3(0, 0, -2), Vec3(2, 0, 0), std::make_shared<DiffuseLight>(Color(10, 9, 8)));
    world.add(lamp);
    lights.add(lamp);
```

Lines 23–25: a 2 × 2 glowing panel above and to the left. Its edges are ordered so its front faces **down**.

---

## Block C — Camera (lines 27–36)

```cpp
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
```

Note that `samples_per_pixel` is **not** set here: it changes between renders below. Lines 35–36 prepare the ACES save
options used for every color image.

---

## Block D — Noisy render + guides (lines 38–46)

```cpp
    cam.samples_per_pixel = 16;
    cam.collect_aovs = true;
    Image noisy = cam.render(world, &lights);
    save_image("images/ch36_noisy.png", noisy, opt);
```

* Line 39: only 16 samples per pixel: fast, but noisy.
* Line 40: **also collect the guide images** (albedo and normal). This costs almost nothing.
* Lines 41–42: render and save the noisy result.

```cpp
    save_image("images/ch36_albedo.png", cam.albedo_aov);
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch36_normals.png", post::visualize_normals(cam.normal_aov), raw);
```

* Line 43: the **albedo** image: each pixel's plain surface color, with no lighting: perfectly clean.
* Lines 44–46: the **normal** image. Normals go from −1 to 1, so `visualize_normals` shifts them into 0–1, and we save
  without the sRGB curve (these are directions, not real colors).

---

## Block E — A plain blur (lines 48–49)

```cpp
    save_image("images/ch36_blur.png", post::gaussian_blur(noisy, 4), opt);
```

Line 49: a normal blur with radius 4. The noise disappears, and so does every detail: this is the "bad" comparison.

---

## Block F — The denoiser (lines 51–56)

```cpp
    post::DenoiseSettings ds;
    ds.radius = 7;
    ds.passes = 2;
    Image clean = post::denoise(noisy, cam.albedo_aov, cam.normal_aov, ds);
    save_image("images/ch36_denoised.png", clean, opt);
```

* Lines 52–54: look at neighbors up to 7 pixels away, and run the filter twice.
* Line 55: denoise the noisy image **using the guides**. Neighbors on a different surface (different normal) or with a
  different texture color (different albedo) are ignored, so edges stay sharp while the noise is averaged away.
* Line 56: save.

---

## Block G — The reference (lines 58–61)

```cpp
    cam.samples_per_pixel = 512;
    cam.collect_aovs = false;
    save_image("images/ch36_reference.png", cam.render(world, &lights), opt);
```

* Line 59: 512 samples: 32 times more work than the noisy render.
* Line 60: no guides needed here.
* Line 61: render and save. Compare it with the denoised 16-sample image: they should look very close, at a fraction of
  the time.

---

## Block H — End (lines 62–63)

`return 0;` `}`

---

## Check your understanding

1. Why does the albedo image have no noise? *(It records the surface color at the first hit, which doesn't depend on
   random bounces.)*
2. Why save the normals without sRGB? *(They are directions mapped into 0–1, not light values.)*
3. What is the plain blur for? *(To show what you lose without edge-aware weights.)*
