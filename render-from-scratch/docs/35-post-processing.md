# Chapter 35 — Post-processing: the film look

[← HDR & tone mapping](34-hdr-tonemapping.md) · [Contents](README.md) · [Next: Denoising →](36-denoising.md)

---

## Goal

Real cameras and lenses aren't perfect, and those imperfections are part of what makes an image feel
"filmed" instead of "computed". In this chapter you'll add, step by step:

* **blur** (box and separable Gaussian),
* **bloom**: light bleeding around bright areas,
* a **teal & orange** grade,
* **chromatic aberration**, **vignetting**, **film grain**,
* **letterboxing** for the widescreen cinema frame,

and learn the **correct order** of operations. All these effects work on the finished image, so they
cost seconds, not hours.

---

## 1. Blur

Blurring replaces each pixel with a weighted average of its neighbors. A **Gaussian** blur uses weights
from the bell curve `exp(−x²/(2σ²))`: neighbors close by count a lot, far ones a little.

### 1.1 The separable trick

A 2D Gaussian of radius r needs (2r+1)² samples per pixel. But a 2D Gaussian is the product of two 1D
Gaussians, so we can blur **horizontally**, then **vertically**: (2r+1) + (2r+1) samples. For r = 10,
that's 42 instead of 441 samples: 10× faster, and exactly the same result.

```
 pass 1: blur each row      pass 2: blur each column of the result
 ◀────●────▶                       ▲
                                    ●
                                    ▼
```

### 1.2 Downsampling

For very wide blurs we **shrink** the image first (`downsample2` averages 2×2 blocks), blur the small
version with a small radius, and scale it back up. A 3-pixel blur at 1/32 size is a 96-pixel blur at full size.

---

## 2. Bloom

In a real lens and in the eye, very bright light scatters slightly inside the glass (and the eye's fluids),
producing a glow around bright things. Without it, a light bulb in a render looks like a white sticker. With it,
it **glows**.

```
 1. keep only what's brighter than a threshold       ░░ ██ ░░   (the bright bulb)
 2. blur it at several sizes (small + medium + big)  ░▒▓██▓▒░
 3. add it back on top of the original image         original + glow
```

Doing several blur sizes and adding them (a "mip chain") gives a natural glow: a tight core plus a wide soft
halo.

**Important: bloom must happen in HDR, before tone mapping.** A bulb with value 400 should glow much more
than a white wall with value 1. After tone mapping both are "1.0", and the difference is lost.

---

## 3. Teal and orange

Look at the posters of most action films since ~2000: skin tones and highlights push toward **orange**,
shadows and backgrounds toward **teal**. The two are complementary colors, so the contrast makes subjects
pop. `teal_orange` blends each pixel toward teal or orange depending on its brightness.

Use it lightly (0.2–0.35). At 1.0 it becomes a parody.

---

## 4. Lens and film effects

### 4.1 Chromatic aberration

A simple lens bends red and blue light by slightly different amounts, so colors don't line up exactly
near the edges of the frame. We fake it by sampling the red channel a bit **outward** and the blue
channel a bit **inward**, more the farther from the center:

```
 center: R G B aligned                edge:  R ← G → B   (thin colored fringes on edges)
```

A tiny amount (1–2 pixels at the corners) reads as "real lens". More looks broken.

### 4.2 Vignetting

Lenses deliver less light to the corners of the image. A gentle darkening toward the corners also guides the
viewer's eye to the center. `vignette` multiplies by `1 − strength · smoothstep(...)` of the distance from
the center.

### 4.3 Film grain

Film has grain; digital sensors have noise. A little random noise (strongest in the shadows, like real
film) makes an image feel organic and, interestingly, hides **banding** and small render noise. We use
the hash function from chapter 11, so the grain is the same every time (seeded).

### 4.4 Letterbox

Many films are shot in **2.39:1** "cinemascope". Shown on a 16:9 screen, they get black bars at the top
and bottom. `letterbox(img, 2.39)` paints those bars. (Even better: render at 2.39:1 directly, as
chapter 38 does.)

---

## 5. The order matters

```
render (HDR, linear)
  │
  ├─ denoise            (chapter 36: needs the raw, linear image)
  ├─ bloom              (HDR: bright values must still be bright)
  ├─ exposure
  ├─ tone map (ACES)    ── now values are 0..1 ──
  ├─ color grade        (teal & orange, saturation, lift/gamma/gain)
  ├─ lens effects       (chromatic aberration, vignette)
  ├─ film grain         (last: grain sits "on top" of everything)
  └─ letterbox
  │
save with sRGB
```

### `post.h`

**File: `include/pixel/post.h`**

```cpp
// pixel/post.h
// ------------------------------------------------------------
// Post-processing: what happens to the image AFTER rendering.
// This is where a render starts to look like a movie frame.
//   * blur (box & Gaussian)                        docs/35
//   * bloom / glow                                 docs/35
//   * color grading (exposure, contrast, saturation, temperature,
//     lift/gamma/gain)                             docs/34
//   * vignette, film grain, chromatic aberration,
//     letterbox bars                               docs/35
//   * denoising (edge-aware, guided by albedo+normal)  docs/36
//   * resizing                                     docs/35
// All functions work on LINEAR images and return new images.
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "vec3.h"
#include "color.h"
#include "image.h"
#include "random.h"
#include "noise.h"

namespace pixel {
namespace post {

// ---------------- blur -----------------------------------------------------

// 1D Gaussian weights for radius r (sigma = r/2), normalized to sum 1.
inline std::vector<double> gaussian_kernel(int radius) {
    double sigma = std::max(0.5, radius / 2.0);
    std::vector<double> k(2 * radius + 1);
    double sum = 0;
    for (int i = -radius; i <= radius; i++) {
        k[i + radius] = std::exp(-(i * i) / (2 * sigma * sigma));
        sum += k[i + radius];
    }
    for (auto& w : k) w /= sum;
    return k;
}

// Gaussian blur, done as two 1D passes (horizontal then vertical) - much faster.
inline Image gaussian_blur(const Image& src, int radius) {
    if (radius < 1) return src;
    std::vector<double> k = gaussian_kernel(radius);
    Image tmp(src.width, src.height), out(src.width, src.height);
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            Color sum(0, 0, 0);
            for (int i = -radius; i <= radius; i++) sum += k[i + radius] * src.get_clamped(x + i, y);
            tmp.at(x, y) = sum;
        }
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            Color sum(0, 0, 0);
            for (int i = -radius; i <= radius; i++) sum += k[i + radius] * tmp.get_clamped(x, y + i);
            out.at(x, y) = sum;
        }
    return out;
}

// Half-size image (average of 2x2 blocks).
inline Image downsample2(const Image& src) {
    int w = std::max(1, src.width / 2), h = std::max(1, src.height / 2);
    Image out(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            out.at(x, y) = 0.25 * (src.get_clamped(2 * x, 2 * y) + src.get_clamped(2 * x + 1, 2 * y) +
                                   src.get_clamped(2 * x, 2 * y + 1) + src.get_clamped(2 * x + 1, 2 * y + 1));
    return out;
}

// Resize with bilinear filtering (good for enlarging or mild shrinking).
inline Image resize(const Image& src, int new_w, int new_h) {
    Image out(new_w, new_h);
    for (int y = 0; y < new_h; y++)
        for (int x = 0; x < new_w; x++) {
            double u = (x + 0.5) / new_w;
            double v = 1.0 - (y + 0.5) / new_h;
            out.at(x, y) = src.sample_bilinear(u, v);
        }
    return out;
}

// ---------------- bloom ----------------------------------------------------
// Bright parts of the image "bleed" light into their surroundings, like in a
// real camera lens or the human eye.
inline Image bloom(const Image& src, double threshold = 1.0, double strength = 0.15, int levels = 5) {
    // 1. Keep only the part of each pixel brighter than the threshold.
    Image bright(src.width, src.height);
    for (size_t i = 0; i < src.data.size(); i++) {
        Color c = src.data[i];
        double l = luminance(c);
        double factor = l > threshold ? (l - threshold) / std::max(l, 1e-9) : 0.0;
        bright.data[i] = c * factor;
    }
    // 2. Blur it at several sizes and add them all up (wide + soft glow).
    Image out = src;
    Image level = bright;
    for (int n = 0; n < levels; n++) {
        level = gaussian_blur(downsample2(level), 3);
        for (int y = 0; y < out.height; y++)
            for (int x = 0; x < out.width; x++) {
                double u = (x + 0.5) / out.width, v = 1.0 - (y + 0.5) / out.height;
                out.at(x, y) += strength * level.sample_bilinear(u, v);
            }
        if (level.width < 4 || level.height < 4) break;
    }
    return out;
}

// ---------------- color grading --------------------------------------------

inline Image exposure(const Image& src, double stops) {
    Image out = src;
    double m = std::pow(2.0, stops);    // +1 stop = twice as bright
    for (auto& c : out.data) c *= m;
    return out;
}

// Warmer (positive) or cooler (negative) white balance.
inline Image temperature(const Image& src, double amount) {
    Image out = src;
    Color tint(1.0 + 0.1 * amount, 1.0, 1.0 - 0.1 * amount);
    for (auto& c : out.data) c = c * tint;
    return out;
}

// Saturation: 0 = black & white, 1 = unchanged, >1 = more colorful.
inline Image saturation(const Image& src, double amount) {
    Image out = src;
    for (auto& c : out.data) {
        double l = luminance(c);
        c = lerp(Color(l, l, l), c, amount);
        c = vmax(c, Color(0, 0, 0));
    }
    return out;
}

// Contrast around a mid grey (0.18 linear, a photographer's "middle grey").
inline Image contrast(const Image& src, double amount) {
    Image out = src;
    const double mid = 0.18;
    for (auto& c : out.data) {
        for (int k = 0; k < 3; k++) {
            double v = std::max(c[k], 1e-6);
            c[k] = mid * std::pow(v / mid, amount);
        }
    }
    return out;
}

// Lift / gamma / gain: the classic three-way color corrector in film grading.
//   lift  -> shadows, gamma -> midtones, gain -> highlights
// Works on display-referred values (after tone mapping), 0..1.
inline Color lift_gamma_gain(Color c, const Color& lift, const Color& gamma, const Color& gain) {
    for (int k = 0; k < 3; k++) {
        double v = clamp01(c[k]);
        v = gain[k] * (v + lift[k] * (1.0 - v));
        v = std::pow(std::max(v, 0.0), 1.0 / std::max(gamma[k], 1e-3));
        c[k] = v;
    }
    return c;
}

// Apply tone mapping into a new image (values mostly 0..1, still linear).
inline Image tonemap(const Image& src, ToneMapper tm) {
    Image out = src;
    for (auto& c : out.data) c = apply_tonemap(c, tm);
    return out;
}

// "Teal and orange": the famous blockbuster look. Shadows go teal, highlights orange.
// Works on tone mapped images.
inline Image teal_orange(const Image& src, double amount = 0.3) {
    Image out = src;
    Color teal(0.0, 0.5, 0.55), orange(1.0, 0.55, 0.2);
    for (auto& c : out.data) {
        double l = clamp01(luminance(c));
        Color tint = lerp(teal, orange, smoothstep(0.1, 0.7, l));
        Color graded = c * lerp(Color(1, 1, 1), tint * 1.6, amount);
        c = lerp(c, graded, amount);
    }
    return out;
}

// ---------------- lens & film effects --------------------------------------

// Darken the corners.
inline Image vignette(const Image& src, double strength = 0.5, double softness = 0.6) {
    Image out = src;
    double cx = src.width * 0.5, cy = src.height * 0.5;
    double maxd = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double dx = (x + 0.5 - cx) / maxd, dy = (y + 0.5 - cy) / maxd;
            double d = std::sqrt(dx * dx + dy * dy);
            double f = 1.0 - strength * smoothstep(1.0 - softness, 1.0, d);
            out.at(x, y) *= f;
        }
    return out;
}

// Red and blue are bent differently by cheap lenses: colored fringes near the edges.
inline Image chromatic_aberration(const Image& src, double amount_pixels = 1.5) {
    Image out(src.width, src.height);
    double cx = src.width * 0.5, cy = src.height * 0.5;
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double dx = (x + 0.5 - cx) / cx, dy = (y + 0.5 - cy) / cy;
            double ox = dx * amount_pixels, oy = dy * amount_pixels;
            auto sample = [&](double px, double py) {
                return src.sample_bilinear(px / src.width, 1.0 - py / src.height);
            };
            Color r = sample(x + 0.5 + ox, y + 0.5 + oy);
            Color g = src.at(x, y);
            Color b = sample(x + 0.5 - ox, y + 0.5 - oy);
            out.at(x, y) = Color(r.x, g.y, b.z);
        }
    return out;
}

// Film grain: a little random noise, stronger in the shadows.
inline Image film_grain(const Image& src, double strength = 0.03, uint32_t seed = 7) {
    Image out = src;
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double n = hash2_01(x + (int)seed * 7919, y) - 0.5;   // -0.5 .. 0.5
            Color& c = out.at(x, y);
            double l = luminance(c);
            double k = strength * (1.0 - 0.5 * clamp01(l));
            c = vmax(c + Color(n, n, n) * k, Color(0, 0, 0));
        }
    return out;
}

// Black bars top and bottom for a wide "cinemascope" frame (2.39:1).
inline Image letterbox(const Image& src, double target_aspect = 2.39) {
    Image out = src;
    int visible_h = (int)(src.width / target_aspect);
    if (visible_h >= src.height) return out;
    int bar = (src.height - visible_h) / 2;
    for (int y = 0; y < src.height; y++)
        if (y < bar || y >= src.height - bar)
            for (int x = 0; x < src.width; x++) out.at(x, y) = Color(0, 0, 0);
    return out;
}

// ---------------- denoising ------------------------------------------------
// A "joint bilateral" filter: average neighbouring pixels, but only those
// that look like the SAME surface (similar normal, similar albedo, similar
// color). Edges and textures survive, noise is smoothed away.
struct DenoiseSettings {
    int radius = 6;              // neighbourhood size in pixels
    double sigma_spatial = 3.0;  // how quickly distance reduces the weight
    double sigma_color = 0.6;    // tolerance for color differences (relative)
    double sigma_normal = 0.2;   // tolerance for normal differences
    double sigma_albedo = 0.1;   // tolerance for albedo differences
    int passes = 1;
};

inline Image denoise(const Image& noisy, const Image& albedo, const Image& normal,
                     const DenoiseSettings& s = DenoiseSettings()) {
    bool has_guides = albedo.width == noisy.width && normal.width == noisy.width &&
                      albedo.height == noisy.height && normal.height == noisy.height;
    Image current = noisy;
    for (int pass = 0; pass < s.passes; pass++) {
        Image out(noisy.width, noisy.height);
        for (int y = 0; y < noisy.height; y++) {
            for (int x = 0; x < noisy.width; x++) {
                Color c0 = current.at(x, y);
                double l0 = luminance(c0);
                Color sum(0, 0, 0);
                double wsum = 0;
                for (int dy = -s.radius; dy <= s.radius; dy++) {
                    for (int dx = -s.radius; dx <= s.radius; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (!current.in_bounds(xx, yy)) continue;
                        Color c = current.at(xx, yy);
                        double d2 = (double)(dx * dx + dy * dy);
                        double w = std::exp(-d2 / (2 * s.sigma_spatial * s.sigma_spatial));
                        // color similarity (relative, so it works for dark and bright areas)
                        double dl = (luminance(c) - l0) / (0.1 + l0);
                        w *= std::exp(-dl * dl / (2 * s.sigma_color * s.sigma_color));
                        if (has_guides) {
                            double dn = (normal.at(xx, yy) - normal.at(x, y)).length_squared();
                            w *= std::exp(-dn / (2 * s.sigma_normal * s.sigma_normal));
                            double da = (albedo.at(xx, yy) - albedo.at(x, y)).length_squared();
                            w *= std::exp(-da / (2 * s.sigma_albedo * s.sigma_albedo));
                        }
                        sum += w * c;
                        wsum += w;
                    }
                }
                out.at(x, y) = wsum > 0 ? sum / wsum : c0;
            }
        }
        current = out;
    }
    return current;
}

// Normals are -1..1; this turns them into a viewable 0..1 image.
inline Image visualize_normals(const Image& normals) {
    Image out = normals;
    for (auto& n : out.data) n = 0.5 * (n + Color(1, 1, 1));
    return out;
}

// Put two images side by side (for before/after comparisons).
inline Image side_by_side(const Image& a, const Image& b, int gap = 4, const Color& gap_color = Color(1, 1, 1)) {
    int h = std::max(a.height, b.height);
    Image out(a.width + gap + b.width, h, gap_color);
    for (int y = 0; y < a.height; y++) for (int x = 0; x < a.width; x++) out.at(x, y) = a.at(x, y);
    for (int y = 0; y < b.height; y++) for (int x = 0; x < b.width; x++) out.at(a.width + gap + x, y) = b.at(x, y);
    return out;
}

// Copy a smaller image into a bigger one at (x0, y0).
inline void paste(Image& dst, const Image& src, int x0, int y0) {
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++)
            if (dst.in_bounds(x0 + x, y0 + y)) dst.at(x0 + x, y0 + y) = src.at(x, y);
}

} // namespace post
} // namespace pixel
```

---

## 6. The program

**File: `chapters/ch35_post_effects.cpp`**

```cpp
// ch35_post_effects.cpp
// ------------------------------------------------------------
// Chapter 35: The film look - post-processing.
// A neon-lit night scene, developed step by step:
//   images/ch35_step1_raw.png        ACES tone mapping only
//   images/ch35_step2_bloom.png      + bloom (glow around bright lights)
//   images/ch35_step3_grade.png      + teal & orange color grade
//   images/ch35_step4_final.png      + vignette, chromatic aberration, grain, letterbox
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    HittableList world, lights;

    // Wet street: almost black base with a clear glossy coat -> reflections.
    world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60),
                                     std::make_shared<Plastic>(hex_color(0x0A0A0C), 0.08)));
    // Back wall.
    world.add(std::make_shared<Quad>(Point3(-30, 0, -4), Vec3(60, 0, 0), Vec3(0, 20, 0),
                                     std::make_shared<Lambertian>(hex_color(0x2A2320))));

    // Neon ring: 40 small glowing spheres.
    auto pink = std::make_shared<DiffuseLight>(Color(12, 1.5, 6));
    for (int i = 0; i < 40; i++) {
        double a = 2 * pi * i / 40;
        auto s = std::make_shared<Sphere>(Point3(1.8 * std::cos(a), 2.4 + 1.8 * std::sin(a), -3.6), 0.09, pink);
        world.add(s);
        lights.add(s);
    }
    // Two neon bars (glowing boxes).
    auto cyan = std::make_shared<DiffuseLight>(Color(1, 8, 12));
    auto bar1 = make_box(Point3(-3.6, 0.3, -3.7), Point3(-3.45, 3.8, -3.55), cyan);
    auto bar2 = make_box(Point3(3.45, 0.3, -3.7), Point3(3.6, 3.8, -3.55), cyan);
    world.add(bar1); world.add(bar2);
    lights.add(bar1); lights.add(bar2);

    // Hero objects.
    world.add(std::make_shared<Sphere>(Point3(-1.1, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.95, 0.95, 0.95), 0.05)));
    world.add(std::make_shared<Sphere>(Point3(1.2, 0.6, -0.2), 0.6, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(0.2, 0.3, 1.0), 0.3, std::make_shared<Plastic>(hex_color(0xF5C518), 0.25)));

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = final_quality ? 1280 : 640;
    cam.samples_per_pixel = final_quality ? 512 : 128;
    cam.max_depth = 20;
    cam.vfov = 38;
    cam.lookfrom = Point3(0, 1.3, 7.5);
    cam.lookat = Point3(0, 1.4, -1);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 8.0;
    cam.background = solid_background(Color(0.002, 0.003, 0.008));
    cam.max_sample_value = 40;

    Image hdr = cam.render(world, &lights);
    SaveOptions plain;   // images below are already tone mapped

    // Step 1: just tone mapping.
    Image step1 = post::tonemap(hdr, ToneMapper::Aces);
    save_image("images/ch35_step1_raw.png", step1, plain);

    // Step 2: bloom happens in HDR, BEFORE tone mapping.
    Image bloomed = post::bloom(hdr, 1.0, 0.25, 6);
    Image step2 = post::tonemap(bloomed, ToneMapper::Aces);
    save_image("images/ch35_step2_bloom.png", step2, plain);

    // Step 3: color grade.
    Image step3 = post::teal_orange(step2, 0.35);
    step3 = post::saturation(step3, 1.1);
    save_image("images/ch35_step3_grade.png", step3, plain);

    // Step 4: lens and film effects.
    Image step4 = post::chromatic_aberration(step3, 1.5);
    step4 = post::vignette(step4, 0.55, 0.7);
    step4 = post::film_grain(step4, 0.04);
    step4 = post::letterbox(step4, 2.39);
    save_image("images/ch35_step4_final.png", step4, plain);
    return 0;
}
```

A night scene: wet black street (a glossy black plastic), a dark wall, a **ring of 40 small pink lights**,
two **cyan neon bars**, a chrome ball, a glass ball and a small yellow ball. Every stage is saved
so you can compare.

```bat
run ch35_post_effects            :: 640 px, 128 samples
run ch35_post_effects final      :: 1280 px, 512 samples
```

---

## 7. What you should see

![Step 1](../images/ch35_step1_raw.png)

> **Image description — step 1 (ACES only):** A dark neon scene. At the back, a circle of small glowing
> pink dots and two vertical cyan bars light up a brown wall. The black street in front reflects the ring and
> the bars as long, soft streaks. A chrome ball on the left reflects the neon; a glass ball on the right shows
> a distorted, flipped ring; a small yellow ball sits in front. The lights are bright but look like flat
> stickers: they have hard edges and no glow.

![Step 2](../images/ch35_step2_bloom.png)

> **Image description — step 2 (+ bloom):** The same, but now every light has a soft glow: the pink ring has
> a pink haze around it, the cyan bars bleed cyan into the air, and their reflections on the wet street glow too.
> The scene suddenly feels like it has air and atmosphere.

![Step 3](../images/ch35_step3_grade.png)

> **Image description — step 3 (+ grade):** The shadows and dark areas lean toward teal, the pink and yellow
> highlights toward warm orange-pink, and colors are a little richer.

![Step 4](../images/ch35_step4_final.png)

> **Image description — step 4 (final):** Black bars at the top and bottom turn the frame into a wide 2.39:1
> cinema shot. The corners are darker, thin colored fringes appear around the brightest lights near the
> edges, and a fine grain covers everything. It looks like a still from a neo-noir film.

---

## Try it yourself

1. Crank bloom strength to 1.0. When does it look "too much"? (Most first-time users overdo bloom!)
2. Apply bloom **after** tone mapping instead. Compare: the glow becomes weak and grey.
3. Make a "warm vintage" look: temperature +2, saturation 0.8, lifted blacks, strong grain.
4. Add a **lens flare** or **light streaks**: blur the bright-pass image only horizontally with a big radius
   (the "anamorphic streak" look of sci-fi films).

## Common problems

| Symptom | Cause |
|---------|-------|
| Bloom makes everything hazy | Threshold too low: only really bright things should bloom |
| Bloom invisible | Applied after tone mapping, or threshold above the brightest pixel |
| Grain looks like colored confetti | Using different random values per channel (use the same n for R, G, B) |
| Dark or colored edges around the image after blur | Blur samples outside the image: clamp to the edge |

---

## Summary

* Separable Gaussian blur and downsampling make big blurs cheap.
* Bloom = bright pass → multi-size blur → add, **in HDR**.
* Grading, chromatic aberration, vignette, grain and letterbox sell the "filmed" look.
* Order: denoise → bloom → exposure → tone map → grade → lens → grain → letterbox.

Next: [Chapter 36 — Denoising →](36-denoising.md)
