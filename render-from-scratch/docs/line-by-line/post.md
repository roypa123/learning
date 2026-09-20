# Line by line: `include/pixel/post.h`

[← Line-by-line index](README.md) · Chapters [34](../34-hdr-tonemapping.md), [35](../35-post-processing.md), [36](../36-denoising.md)

**What this file does, in one sentence:** it holds all the **after the render** steps: blur, bloom, color grading, lens
and film effects, and the denoiser.

Every function takes an image and returns a **new** image (the original is never changed). They all work on **linear**
colors.

| Block | Lines | Job | Chapter |
|-------|-------|-----|---------|
| A. Comments, includes | 1–26 | two namespaces: `pixel::post` | |
| B. Gaussian blur | 28–61 | blur weights, and a fast two-pass blur | 35 |
| C. `downsample2`, `resize` | 63–84 | make images smaller/bigger | 35 |
| D. `bloom` | 86–111 | glow around bright areas | 35 |
| E. Color grading | 113–186 | exposure, temperature, saturation, contrast, lift/gamma/gain, tonemap, teal & orange | 34, 35 |
| F. Lens and film effects | 188–248 | vignette, chromatic aberration, grain, letterbox | 35 |
| G. Denoising | 250–~310 | edge-aware filter with albedo/normal guides | 36 |
| H. Helpers | end | visualize normals, side by side, paste | |

---

## Block A — Comments, includes (lines 1–26)

Comments listing everything in the file, `#pragma once`, `<cmath>`, `<vector>`, `<algorithm>`, and our `vec3.h`,
`color.h`, `image.h`, `random.h`, `noise.h`. Lines 25–26 open **two** namespaces, so these functions are called
`post::bloom`, `post::vignette`, and so on.

---

## Block B — Gaussian blur (lines 28–61)

```cpp
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
```

The **weights** for a blur: how much each neighbor counts.

* Line 32: `sigma` = the width of the bell curve (half the radius).
* Line 33: a list of `2 × radius + 1` weights (from −radius to +radius).
* Lines 35–38: the bell curve formula `exp(−i²/(2σ²))`: the center gets the biggest weight, far neighbors almost none.
* Line 39: divide by the total, so the weights add up to 1 (otherwise the image would get brighter or darker).

```cpp
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
```

* Line 45: radius 0 → nothing to do.
* Lines 48–53: **pass 1**: blur each row **horizontally**: for each pixel, add up its neighbors to the left and right,
  each times its weight. `get_clamped` keeps us safe at the edges.

```cpp
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            Color sum(0, 0, 0);
            for (int i = -radius; i <= radius; i++) sum += k[i + radius] * tmp.get_clamped(x, y + i);
            out.at(x, y) = sum;
        }
    return out;
}
```

Lines 54–59: **pass 2**: blur the result **vertically**. Two 1D passes give exactly the same result as one 2D blur, but
much faster: for radius 10 it's 42 samples per pixel instead of 441.

---

## Block C — `downsample2` and `resize` (lines 63–84)

```cpp
inline Image downsample2(const Image& src) {
    int w = std::max(1, src.width / 2), h = std::max(1, src.height / 2);
    Image out(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            out.at(x, y) = 0.25 * (src.get_clamped(2 * x, 2 * y) + src.get_clamped(2 * x + 1, 2 * y) +
                                   src.get_clamped(2 * x, 2 * y + 1) + src.get_clamped(2 * x + 1, 2 * y + 1));
    return out;
}
```

Half-size image: each new pixel is the **average of a 2 × 2 block** (`0.25 ×` the sum of four). Used by bloom to make
wide blurs cheap.

```cpp
inline Image resize(const Image& src, int new_w, int new_h) {
    ...
            double u = (x + 0.5) / new_w;
            double v = 1.0 - (y + 0.5) / new_h;
            out.at(x, y) = src.sample_bilinear(u, v);
```

Lines 75–83: any size: for each new pixel, work out its position as (u, v) in 0–1 and look up the smoothly blended
color from the source (`sample_bilinear`, see [image.md](image.md)). The `1.0 −` flips v, because v = 0 is the bottom.

---

## Block D — `bloom` (lines 86–111)

Bright things glow in real lenses and eyes. Bloom copies that.

```cpp
inline Image bloom(const Image& src, double threshold = 1.0, double strength = 0.15, int levels = 5) {
    Image bright(src.width, src.height);
    for (size_t i = 0; i < src.data.size(); i++) {
        Color c = src.data[i];
        double l = luminance(c);
        double factor = l > threshold ? (l - threshold) / std::max(l, 1e-9) : 0.0;
        bright.data[i] = c * factor;
    }
```

**Step 1** (lines 91–97): keep only the part **above** the threshold. A pixel with brightness 5 and threshold 1 keeps
(5 − 1)/5 = 80% of itself; a pixel at 0.5 keeps nothing. This must happen on **HDR** values, before tone mapping.

```cpp
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
```

**Step 2** (lines 99–110): repeat `levels` times:

* Line 102: halve the image and blur it a little. Each round, the image is smaller, so the same small blur covers a
  **wider** area of the original picture.
* Lines 103–107: add this blurred layer back on top of the original (stretched back to full size), times `strength`.
* Line 108: stop when the image becomes tiny.

Adding several sizes gives a natural glow: a tight core plus a wide halo.

---

## Block E — Color grading (lines 113–186)

| Lines | Function | What it does |
|-------|----------|--------------|
| 115–120 | `exposure(img, stops)` | multiply by 2^stops: +1 stop = twice as bright |
| 123–128 | `temperature(img, amount)` | warmer (more red, less blue) or cooler |
| 131–139 | `saturation(img, amount)` | blend between grey (its luminance) and the color; 0 = black & white, >1 = more colorful |
| 142–152 | `contrast(img, amount)` | a power curve around **middle grey** 0.18: values above get brighter, below get darker |
| 157–165 | `lift_gamma_gain(c, ...)` | the three-way corrector: lift = shadows, gamma = midtones, gain = highlights |
| 168–172 | `tonemap(img, tm)` | apply a tone-mapping curve to every pixel |
| 176–186 | `teal_orange(img, amount)` | the blockbuster look |

A few details:

```cpp
        double l = luminance(c);
        c = lerp(Color(l, l, l), c, amount);
```

Lines 134–135 (`saturation`): `lerp(grey, color, amount)`: amount 0 gives grey, 1 gives the original, 2 pushes past it.

```cpp
            double v = std::max(c[k], 1e-6);
            c[k] = mid * std::pow(v / mid, amount);
```

Lines 147–148 (`contrast`): divide by middle grey, raise to a power, multiply back. Values equal to 0.18 stay the same;
everything else moves away from it (for amount > 1).

```cpp
        v = gain[k] * (v + lift[k] * (1.0 - v));
        v = std::pow(std::max(v, 0.0), 1.0 / std::max(gamma[k], 1e-3));
```

Lines 160–161 (`lift_gamma_gain`): first lift the darks and scale by gain, then bend the midtones with a power. Each
channel has its own number, so you can tint shadows and highlights differently.

```cpp
        double l = clamp01(luminance(c));
        Color tint = lerp(teal, orange, smoothstep(0.1, 0.7, l));
        Color graded = c * lerp(Color(1, 1, 1), tint * 1.6, amount);
        c = lerp(c, graded, amount);
```

Lines 180–183 (`teal_orange`): dark pixels are tinted **teal**, bright ones **orange** (with a smooth change between),
and the result is mixed back with the original by `amount`, so you can dial the effect down.

---

## Block F — Lens and film effects (lines 188–248)

### `vignette` (lines 191–203)

```cpp
            double dx = (x + 0.5 - cx) / maxd, dy = (y + 0.5 - cy) / maxd;
            double d = std::sqrt(dx * dx + dy * dy);
            double f = 1.0 - strength * smoothstep(1.0 - softness, 1.0, d);
            out.at(x, y) *= f;
```

Lines 197–200: `d` = the distance from the image's center, as a fraction (1 at the corners). `smoothstep` makes the
darkening start gradually, and `f` multiplies the pixel: 1 in the middle, `1 − strength` at the corners.

### `chromatic_aberration` (lines 206–222)

```cpp
            double dx = (x + 0.5 - cx) / cx, dy = (y + 0.5 - cy) / cy;
            double ox = dx * amount_pixels, oy = dy * amount_pixels;
            auto sample = [&](double px, double py) {
                return src.sample_bilinear(px / src.width, 1.0 - py / src.height);
            };
            Color r = sample(x + 0.5 + ox, y + 0.5 + oy);
            Color g = src.at(x, y);
            Color b = sample(x + 0.5 - ox, y + 0.5 - oy);
            out.at(x, y) = Color(r.x, g.y, b.z);
```

* Lines 211–212: how far this pixel is from the center (as a fraction), times the effect strength. The offset grows
  toward the edges: exactly how a simple lens behaves.
* Lines 213–215: a small helper to read a color at a pixel position.
* Lines 216–219: take **red** from slightly further out, **green** from here, **blue** from slightly further in, and
  combine them. Near the edges you get thin colored fringes.

### `film_grain` (lines 225–236)

```cpp
            double n = hash2_01(x + (int)seed * 7919, y) - 0.5;   // -0.5 .. 0.5
            double l = luminance(c);
            double k = strength * (1.0 - 0.5 * clamp01(l));
            c = vmax(c + Color(n, n, n) * k, Color(0, 0, 0));
```

* Line 229: a repeatable random value from the pixel position (so the grain doesn't change between runs), shifted to
  −0.5..0.5.
* Line 232: the grain is stronger in the shadows (like real film).
* Line 233: add the same value to all three channels (grey grain), never below 0.

### `letterbox` (lines 239–248)

```cpp
    int visible_h = (int)(src.width / target_aspect);
    if (visible_h >= src.height) return out;
    int bar = (src.height - visible_h) / 2;
    for (int y = 0; y < src.height; y++)
        if (y < bar || y >= src.height - bar)
            for (int x = 0; x < src.width; x++) out.at(x, y) = Color(0, 0, 0);
```

Lines 241–246: how tall the picture would be at the target shape (2.39:1), and black bars above and below the rest.

---

## Block G — Denoising (lines 250 onward)

```cpp
struct DenoiseSettings {
    int radius = 6;              // neighbourhood size in pixels
    double sigma_spatial = 3.0;  // how quickly distance reduces the weight
    double sigma_color = 0.6;    // tolerance for color differences (relative)
    double sigma_normal = 0.2;   // tolerance for normal differences
    double sigma_albedo = 0.1;   // tolerance for albedo differences
    int passes = 1;
};
```

Lines 254–261: the knobs. A bigger **sigma** means "accept bigger differences", so more smoothing.

```cpp
inline Image denoise(const Image& noisy, const Image& albedo, const Image& normal,
                     const DenoiseSettings& s = DenoiseSettings()) {
    bool has_guides = albedo.width == noisy.width && normal.width == noisy.width &&
                      albedo.height == noisy.height && normal.height == noisy.height;
```

Lines 263–266: the noisy image plus two **guide** images (the surface color and the surface direction at each pixel,
from the camera's AOVs). If the guides are missing or the wrong size, we work without them.

```cpp
    Image current = noisy;
    for (int pass = 0; pass < s.passes; pass++) {
        Image out(noisy.width, noisy.height);
        for (int y = 0; y < noisy.height; y++) {
            for (int x = 0; x < noisy.width; x++) {
                Color c0 = current.at(x, y);
                double l0 = luminance(c0);
                Color sum(0, 0, 0);
                double wsum = 0;
```

Lines 267–275: for each pixel: remember its color and brightness, and prepare a weighted sum.

```cpp
                for (int dy = -s.radius; dy <= s.radius; dy++) {
                    for (int dx = -s.radius; dx <= s.radius; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (!current.in_bounds(xx, yy)) continue;
                        Color c = current.at(xx, yy);
```

Lines 276–280: look at every neighbor inside the radius (skipping those outside the image).

```cpp
                        double d2 = (double)(dx * dx + dy * dy);
                        double w = std::exp(-d2 / (2 * s.sigma_spatial * s.sigma_spatial));
                        double dl = (luminance(c) - l0) / (0.1 + l0);
                        w *= std::exp(-dl * dl / (2 * s.sigma_color * s.sigma_color));
```

* Lines 281–282: weight 1: **distance**. Close neighbors count more (a bell curve again).
* Lines 284–285: weight 2: **brightness difference**, measured **relative** to the pixel's own brightness, so the filter
  behaves the same in dark and bright areas.

```cpp
                        if (has_guides) {
                            double dn = (normal.at(xx, yy) - normal.at(x, y)).length_squared();
                            w *= std::exp(-dn / (2 * s.sigma_normal * s.sigma_normal));
                            double da = (albedo.at(xx, yy) - albedo.at(x, y)).length_squared();
                            w *= std::exp(-da / (2 * s.sigma_albedo * s.sigma_albedo));
                        }
                        sum += w * c;
                        wsum += w;
```

* Lines 286–291: weights 3 and 4: **normal** and **albedo** differences. A neighbor on a different surface, or with a
  different texture color, gets almost no weight. That's what keeps edges sharp: these guide images are **noise-free**.
* Lines 292–293: add the neighbor's color times its weight, and the weight itself.

```cpp
                out.at(x, y) = wsum > 0 ? sum / wsum : c0;
            }
        }
        current = out;
    }
```

Line 296: the result = the weighted average. Line 299: more passes smooth further, using the previous result.

---

## Block H — Helpers (end of file)

* `visualize_normals(img)`: normals go from −1..1 to 0..1 so they can be viewed as colors.
* `side_by_side(a, b, gap, color)`: glue two images together with a gap (used for all the comparison figures in this
  book).
* `paste(dst, src, x0, y0)`: copy a small image into a bigger one.

---

## Check your understanding

1. Why does a blur use two 1D passes? *(Same result, far fewer samples: (2r+1) + (2r+1) instead of (2r+1)².)*
2. Why must bloom run before tone mapping? *(Only HDR values show which parts are truly bright.)*
3. Why do the denoiser's guide images have no noise? *(They record the surface color and direction of the first hit,
   which don't depend on random bounces.)*
4. What does `smoothstep` do in the vignette? *(Makes the darkening start gradually instead of at a hard ring.)*
