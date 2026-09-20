# Chapter 34 — HDR, exposure and tone mapping

[← Skies](33-sky-and-environment.md) · [Contents](README.md) · [Next: Post-processing →](35-post-processing.md)

> 📖 **Line by line:** [post explained line by line](line-by-line/post.md) · [ch34_hdr_tonemapping explained line by line](line-by-line/ch34_hdr_tonemapping.md)

---

## Goal

Our renders contain light values from 0.001 (deep shadow) to thousands (a bulb, the sun). A screen shows
0 to 1. How we squeeze one into the other decides a huge part of how "cinematic" an image feels.
You'll learn:

* **dynamic range** and **stops**,
* **exposure**, the photographer's first control,
* **tone mapping** curves: clamp, Reinhard, **ACES filmic**, Hable/Uncharted 2,
* **false color** images to *see* the range of light,
* basic **color grading**: contrast, saturation, white balance, lift/gamma/gain.

---

## 1. Dynamic range and stops

Photographers measure light in **stops**: +1 stop = twice as much light, −1 stop = half.

| Scene | Range |
|-------|-------|
| a printed photo | ~6 stops (1 : 64) |
| a typical screen | ~8–10 stops |
| a real sunny scene with shadows and sky | ~15–20 stops (1 : 1,000,000) |
| the human eye (adapting) | ~20+ stops |

A render is **HDR** (high dynamic range): it has all 20 stops. A screen is **LDR**. Something has to give.

---

## 2. Exposure

First, choose which part of the range is "middle grey". **Exposure** simply multiplies all values:

```
exposed = value × 2^stops
```

`post::exposure(img, stops)` does this, and `SaveOptions::exposure` does it on save (as a factor, not
stops). +1 stop brightens everything 2×; −2 stops divides by 4.

---

## 3. Tone mapping

After exposure, values above 1 remain. What do we do with them?

### 3.1 Clamp

Cut everything above 1 to 1. Bright areas become flat white blobs with no detail ("blown-out
highlights"), and colored lights become white or ugly saturated primaries. It's what we've done so far.

### 3.2 Reinhard

`x / (1 + x)`: a smooth curve that never quite reaches 1. Nothing clips, but the whole image looks flat and
washed out, and highlights look grey rather than bright.

### 3.3 Filmic curves (ACES, Hable)

Real photographic **film** has an S-shaped response: a gentle **toe** in the shadows, a straight middle,
and a soft **shoulder** that rolls highlights off gracefully to white. Film curves are why movies look like
movies.

```
display
 1.0 ┤                          ___________  shoulder: highlights roll off smoothly
     │                    __,--'
     │                 ,-'
     │               ╱                        straight middle: normal contrast
     │             ╱
     │         _,-'
 0.0 ┼____,---'                               toe: deep shadows compressed slightly
     └───────────────────────────────────────▶ scene light (log scale)
```

* **ACES** (Academy Color Encoding System) is the film industry standard. We use Krzysztof Narkowicz's
  popular fit: `x(2.51x + 0.03) / (x(2.43x + 0.59) + 0.14)`. It gives punchy contrast and natural
  saturation, and very bright colored lights fade toward white like on film.
* **Hable** ("Uncharted 2", by John Hable): a similar filmic curve with a softer toe, originally made for games.

**File: `include/pixel/color.h`**

```cpp
// pixel/color.h
// ------------------------------------------------------------
// Everything about turning "light numbers" into "screen numbers".
//   * linear  <->  sRGB (gamma)
//   * tone mapping (squeezing bright HDR values into 0..1)
//   * helpers to build colors from hex codes / HSV
// Explained in docs/04-color.md and docs/34-hdr-tonemapping.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"

namespace pixel {

// ---------- Gamma / sRGB ----------------------------------------------------
// Monitors expect "gamma encoded" values. Our renderer works with linear light.

// Exact sRGB transfer function (linear 0..1 -> encoded 0..1)
inline double linear_to_srgb(double x) {
    if (x <= 0.0) return 0.0;
    if (x <= 0.0031308) return 12.92 * x;
    return 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

// Inverse: encoded 0..1 -> linear 0..1
inline double srgb_to_linear(double x) {
    if (x <= 0.04045) return x / 12.92;
    return std::pow((x + 0.055) / 1.055, 2.4);
}

// The simple "gamma 2" approximation used in early chapters.
inline double linear_to_gamma2(double x) { return x > 0.0 ? std::sqrt(x) : 0.0; }

inline Color linear_to_srgb(const Color& c) {
    return Color(linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z));
}
inline Color srgb_to_linear(const Color& c) {
    return Color(srgb_to_linear(c.x), srgb_to_linear(c.y), srgb_to_linear(c.z));
}

// Convert a 0..1 value to a byte 0..255 (with rounding and clamping).
inline uint8_t to_byte(double x) {
    int v = (int)(clamp01(x) * 255.0 + 0.5);
    return (uint8_t)(v > 255 ? 255 : v);
}

// ---------- Handy color constructors -----------------------------------------

// From 0..255 integers, already sRGB encoded (like colors in a paint program).
// Returned color is LINEAR, ready for rendering math.
inline Color rgb255(int r, int g, int b) {
    return srgb_to_linear(Color(r / 255.0, g / 255.0, b / 255.0));
}

// From a hex code like 0xFF8800 (orange). Returned color is LINEAR.
inline Color hex_color(uint32_t hex) {
    return rgb255((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// Hue (0..360), saturation (0..1), value (0..1) -> RGB (0..1, not linearized)
inline Color hsv(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360.0;
    double c = v * s;
    double hp = h / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    Color rgb;
    if      (hp < 1) rgb = Color(c, x, 0);
    else if (hp < 2) rgb = Color(x, c, 0);
    else if (hp < 3) rgb = Color(0, c, x);
    else if (hp < 4) rgb = Color(0, x, c);
    else if (hp < 5) rgb = Color(x, 0, c);
    else             rgb = Color(c, 0, x);
    double m = v - c;
    return rgb + Color(m, m, m);
}

// ---------- Tone mapping ----------------------------------------------------
// Real light can be 1000x brighter than white paper. Screens only show 0..1.
// A tone mapper squeezes 0..infinity into 0..1 in a pleasing way.

inline Color tonemap_clamp(const Color& c) {
    return Color(clamp01(c.x), clamp01(c.y), clamp01(c.z));
}

inline Color tonemap_reinhard(const Color& c) {
    return Color(c.x / (1.0 + c.x), c.y / (1.0 + c.y), c.z / (1.0 + c.z));
}

// ACES filmic curve (Krzysztof Narkowicz's fit). The "movie look".
inline double aces_curve(double x) {
    const double a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}
inline Color tonemap_aces(const Color& c) {
    return Color(aces_curve(c.x), aces_curve(c.y), aces_curve(c.z));
}

// Uncharted 2 "filmic" curve by John Hable.
inline double hable_partial(double x) {
    const double A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
inline Color tonemap_hable(const Color& c) {
    const double exposure_bias = 2.0;
    const double white = 11.2;
    double w = 1.0 / hable_partial(white);
    return Color(hable_partial(c.x * exposure_bias) * w,
                 hable_partial(c.y * exposure_bias) * w,
                 hable_partial(c.z * exposure_bias) * w);
}

enum class ToneMapper { Clamp, Reinhard, Aces, Hable };

inline Color apply_tonemap(const Color& c, ToneMapper tm) {
    switch (tm) {
        case ToneMapper::Clamp:    return tonemap_clamp(c);
        case ToneMapper::Reinhard: return tonemap_reinhard(c);
        case ToneMapper::Aces:     return tonemap_aces(c);
        case ToneMapper::Hable:    return tonemap_hable(c);
    }
    return tonemap_clamp(c);
}

} // namespace pixel
```

---

## 4. Seeing light: false color

Our eyes are bad at judging absolute brightness. A **false color** image maps luminance (on a log scale) to
a heat-map palette: dark blue → cyan → green → yellow → red. You can immediately see where the light is,
how big the range is, and which areas will clip. Cinematographers use false color on set.

---

## 5. Color grading basics

After tone mapping comes **grading**: artistic color changes. `post.h` provides the classic ones:

| Function | Effect |
|----------|--------|
| `exposure(img, stops)` | overall brightness |
| `contrast(img, k)` | a power curve around middle grey (0.18) |
| `saturation(img, k)` | 0 = black & white, 1 = unchanged, >1 = more colorful |
| `temperature(img, k)` | positive = warmer (more red, less blue), negative = cooler |
| `lift_gamma_gain(c, lift, gamma, gain)` | the three-way color corrector: shadows / midtones / highlights |
| `teal_orange(img, k)` | the blockbuster look (chapter 35) |

**Lift, gamma, gain** is the heart of every film grading tool (DaVinci Resolve, Baselight):

* **lift** raises the blacks (a blue lift makes shadows bluish),
* **gamma** bends the midtones,
* **gain** scales the highlights (a warm gain makes bright areas golden).

---

## 6. The program

**File: `chapters/ch34_hdr_tonemapping.cpp`**

```cpp
// ch34_hdr_tonemapping.cpp
// ------------------------------------------------------------
// Chapter 34: HDR, exposure and tone mapping.
// We render ONE high-dynamic-range image (a dusk scene with a very
// bright lamp) and develop it in different ways, like a photographer.
//   images/ch34_clamp.png, ch34_reinhard.png, ch34_aces.png, ch34_hable.png
//   images/ch34_exposure_bracket.png  - exposure -2 .. +2 stops (ACES)
//   images/ch34_false_color.png       - brightness as a heat map (log scale)
//   images/ch34_graded.png            - ACES + color grading
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Blue (dark) -> green -> yellow -> red (bright), on a log scale.
static Color heat(double lum) {
    double t = clamp01((std::log10(std::max(lum, 1e-4)) + 3.0) / 5.0);   // 0.001 .. 100
    Color stops[5] = {Color(0, 0, 0.5), Color(0, 0.6, 1), Color(0, 1, 0), Color(1, 1, 0), Color(1, 0, 0)};
    double f = t * 4;
    int i = std::min(3, (int)f);
    return lerp(stops[i], stops[i + 1], f - i);
}

int main() {
    HittableList world, lights;
    world.add(std::make_shared<Quad>(Point3(-50, 0, -50), Vec3(100, 0, 0), Vec3(0, 0, 100),
                                     std::make_shared<Lambertian>(hex_color(0x6B6B6B))));
    world.add(std::make_shared<Sphere>(Point3(-1.4, 0.6, 0.3), 0.6, std::make_shared<Lambertian>(hex_color(0xD9D9D9))));
    world.add(std::make_shared<Sphere>(Point3(1.3, 0.6, 0.2), 0.6, std::make_shared<Plastic>(hex_color(0xB91C1C), 0.2)));
    world.add(std::make_shared<Sphere>(Point3(0.0, 0.45, 1.3), 0.45, std::make_shared<Dielectric>(1.5)));

    // A street lamp: a thin pole and a VERY bright bulb (radiance 400!).
    auto pole = make_box(Point3(-0.05, 0, -0.05), Point3(0.05, 2.6, 0.05), std::make_shared<RoughMetal>(Color(0.3, 0.3, 0.3), 0.4));
    world.add(std::make_shared<Translate>(pole, Vec3(0, 0, -1.2)));
    auto bulb = std::make_shared<Sphere>(Point3(0, 2.75, -1.2), 0.15, std::make_shared<DiffuseLight>(Color(400, 300, 180)));
    world.add(bulb);
    lights.add(bulb);

    Camera cam;
    cam.image_width = 480;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 40;
    cam.lookfrom = Point3(0, 1.5, 6.5);
    cam.lookat = Point3(0, 1.0, 0);
    SkySettings dusk;
    dusk.zenith = Color(0.01, 0.02, 0.06);
    dusk.horizon = Color(0.10, 0.07, 0.12);
    dusk.ground = Color(0.01, 0.01, 0.01);
    dusk.glow_strength = 0.0;
    cam.background = physical_sky(dusk);

    Image hdr = cam.render(world, &lights);   // values from ~0.001 up to 400!

    SaveOptions o;
    o.tonemap = ToneMapper::Clamp;    save_image("images/ch34_clamp.png", hdr, o);
    o.tonemap = ToneMapper::Reinhard; save_image("images/ch34_reinhard.png", hdr, o);
    o.tonemap = ToneMapper::Aces;     save_image("images/ch34_aces.png", hdr, o);
    o.tonemap = ToneMapper::Hable;    save_image("images/ch34_hable.png", hdr, o);

    // Exposure bracket: -2, -1, 0, +1, +2 stops.
    Image small = post::resize(hdr, 240, 135);
    Image strip;
    for (int stop = -2; stop <= 2; stop++) {
        Image developed = post::tonemap(post::exposure(small, stop), ToneMapper::Aces);
        strip = stop == -2 ? developed : post::side_by_side(strip, developed, 4);
    }
    SaveOptions plain;   // already tone mapped: just apply sRGB
    save_image("images/ch34_exposure_bracket.png", strip, plain);

    // False color: see the real range of light values.
    Image fc(hdr.width, hdr.height);
    for (size_t i = 0; i < hdr.data.size(); i++) fc.data[i] = heat(luminance(hdr.data[i]));
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch34_false_color.png", fc, raw);

    // A graded version: warm, slightly more contrast and saturation.
    Image graded = post::contrast(hdr, 1.1);
    graded = post::temperature(graded, 1.0);
    graded = post::saturation(graded, 1.15);
    graded = post::tonemap(post::exposure(graded, 0.3), ToneMapper::Aces);
    for (auto& c : graded.data)
        c = post::lift_gamma_gain(c, Color(0.02, 0.01, 0.04), Color(1.0, 1.0, 1.05), Color(1.0, 0.97, 0.92));
    save_image("images/ch34_graded.png", graded, plain);
    return 0;
}
```

A dusk scene lit by a street lamp with a bulb of radiance 400: a really wide dynamic range.

```bat
run ch34_hdr_tonemapping
```

---

## 7. What you should see

![Clamp](../images/ch34_clamp.png) ![Reinhard](../images/ch34_reinhard.png)
![ACES](../images/ch34_aces.png) ![Hable](../images/ch34_hable.png)

> **Image descriptions:** The same night scene four times: a thin lamp post with a glowing bulb, a white ball,
> a glossy red ball and a glass ball on a grey ground under a dark blue dusk sky.
> * **Clamp:** the bulb is a flat white blob; the ground right under the lamp is a big flat, featureless
>   white-yellow area; colors near the light are harsh.
> * **Reinhard:** nothing is burnt out, but the whole image looks dull and grey; the bulb doesn't look bright
>   at all.
> * **ACES:** punchy and natural. The bulb glows white-hot, the pool of light on the ground fades smoothly
>   into darkness, the red ball has rich color, and the shadows are deep. It looks like a night photo.
> * **Hable:** similar to ACES, a bit softer and lower in contrast.

![Bracket](../images/ch34_exposure_bracket.png)

> **Image description:** Five small versions side by side, from −2 stops (very dark, only the bulb and its
> pool of light visible) to +2 stops (bright, the dusk sky turns a lighter blue and the shadows open up, the
> lit area becomes very bright but still rolls off smoothly thanks to ACES).

![False color](../images/ch34_false_color.png)

> **Image description:** The same scene as a heat map: the bulb is **red** (very bright), the ground under
> the lamp **yellow and green**, the balls green-cyan on their lit sides, the sky and far ground dark **blue**.

![Graded](../images/ch34_graded.png)

> **Image description:** The ACES version with a grade: slightly warmer and more saturated, a bit more
> contrast, and bluish-purple shadows. It feels more like a moody film still.

---

## Try it yourself

1. Change the bulb from 400 to 40 and to 4000. Which tone mapper copes best?
2. Write your own curve: `1 − exp(−x)` ("exponential"). Add it to `ToneMapper`.
3. Make a black & white "film noir" grade: saturation 0, higher contrast, a slight blue lift.
4. Check `ch34_false_color.png` for the white ball's lit side. How many stops brighter than the sky is it?

## Common problems

| Symptom | Cause |
|---------|-------|
| Image flat and grey | Reinhard, or tone mapping applied twice |
| Colors shift strangely when bright | Tone mapping each channel separately (normal, "hue shift"); ACES handles it pleasantly |
| Grade looks too strong | Grading functions applied before tone mapping when meant for after (or vice versa) |

---

## Summary

* Renders are HDR; screens are LDR. Exposure picks middle grey; tone mapping compresses the rest.
* Filmic curves (ACES) give the S-shaped film response: soft toe, soft shoulder.
* False color shows the light range; grading (lift/gamma/gain, saturation, temperature) sets the mood.

Next: [Chapter 35 — Post-processing: the film look →](35-post-processing.md)
