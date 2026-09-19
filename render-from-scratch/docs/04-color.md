# Chapter 4 — Color for programmers

[← Your first image](03-first-image.md) · [Contents](README.md) · [Next: The Image class →](05-image-class.md)

---

## Goal

Understand the few facts about color that separate "renders that look like computer graphics" from
"renders that look like photos":

* why screens use **red, green and blue**,
* the difference between **linear light** and **sRGB** (gamma), and why getting it wrong makes
  everything look fake,
* hex codes, HSV, and **luminance**,
* a first taste of **HDR**: light brighter than "white".

You'll also meet our library's first file, `color.h`, and generate three test images.

---

## 1. Light, eyes and RGB

Light is energy at many wavelengths. Our eyes have three kinds of color-sensitive cells ("cones"),
most sensitive roughly to **long** (reddish), **medium** (greenish) and **short** (bluish)
wavelengths. Every color you perceive is your brain comparing those three signals.

A screen takes advantage of this: each pixel has tiny red, green and blue lights. By mixing their
brightness it can trigger almost any cone response and therefore show almost any color. This is
**additive** color mixing, because the lights **add up**:

```
red   + green         = yellow
red   + blue          = magenta
green + blue          = cyan
red   + green + blue  = white
```

(Paint works the other way: pigments *remove* light, which is why mixing paints makes things
darker. Screens are light, not paint.)

---

## 2. The big one: linear vs gamma

### 2.1 An experiment

What is "half as bright as white"? Most people would say the byte value 128. Let's test it: in
chapter 3 the middle of our gradient had value 128. But if you measure the **light** coming out of a
monitor showing 128, it is only about **22%** of the light of 255, not 50%.

Why? Monitors don't turn bytes into light in a straight line. They follow a curve, roughly

```
light = (byte / 255) ^ 2.2
```

```
light
 1.0 ┤                                      ╭
     │                                   ╭──╯
     │                                ╭──╯
 0.5 ┤                            ╭───╯               this curve is "gamma 2.2"
     │                       ╭────╯
     │                ╭──────╯
 0.2 ┤ . . . . . ╭────╯  <- byte 128 gives only ~22% light
     │   ╭───────╯
 0.0 ┼───┴──────────────┬──────────────────────┬──
     0                 128                    255  byte
```

This is not a bug. Human vision is *more* sensitive to differences between dark shades than between
bright ones. Spending more of the 256 byte values on dark tones hides banding where our eyes would
notice it. The standard curve used by nearly all images and screens is called **sRGB**.

### 2.2 Two kinds of numbers

So there are two different kinds of "color numbers":

| | **Linear** values | **sRGB-encoded** values |
|---|---|---|
| What they measure | the actual amount of light | what's stored in image files / sent to screens |
| 50% means | half the photons | ~22% of the photons |
| Math on them (add, average, multiply) | **physically correct** | wrong |
| Where we use them | *everywhere inside the renderer* | only when reading/writing files |

**The rule of this book:** all colors inside our program are **linear**. We convert to sRGB only at
the very end, when saving a file. When we load an 8-bit image (a texture), we convert to linear
first.

### 2.3 Why it matters: averaging

Anti-aliasing (chapter 9), blur, motion blur, and path tracing itself all **average** colors.
Averaging only works on linear values. Example: a pixel half-covered by a white shape on black:

* In linear light: average light = (1.0 + 0.0) / 2 = **0.5** → encoded as sRGB byte **188**. Correct: the edge
  pixel looks halfway between black and white.
* Averaging sRGB bytes: (255 + 0) / 2 = **128** → which displays only 22% light. The edge
  looks too dark and too thin. Circles look "notchy"; bright objects look like they shrank.

The same problem makes lighting wrong: a light twice as strong must produce twice the linear value.

### 2.4 The exact sRGB formulas

The official curve has a short straight part near black (to avoid infinite slope), then a power
curve:

```
encode (linear -> sRGB):
    if x <= 0.0031308:   12.92 * x
    else:                1.055 * x^(1/2.4) - 0.055

decode (sRGB -> linear):
    if x <= 0.04045:     x / 12.92
    else:                ((x + 0.055) / 1.055)^2.4
```

Overall it behaves very much like a gamma of 2.2. Some tutorials (and some early chapters here)
use the simpler **gamma 2** approximation: encode = `sqrt(x)`, decode = `x * x`.

| Linear light | sRGB-encoded | byte |
|--------------|--------------|------|
| 0.0 | 0.0 | 0 |
| 0.01 | 0.0998 | 25 |
| 0.05 | 0.248 | 63 |
| 0.18 ("middle grey") | 0.461 | 118 |
| 0.214 | 0.5 | 128 |
| 0.5 | 0.735 | 188 |
| 1.0 | 1.0 | 255 |

Look at the "middle grey" row. Photographers call 18% reflectance "middle grey" because it *looks*
halfway between black and white. In sRGB it lands near the middle of the byte range. That's the
whole point of the curve.

---

## 3. Specifying colors: hex codes, 0–255, HSV

Designers write colors as **hex codes** like `#FF8800` (orange): two hex digits each for R, G, B
(`FF` = 255, `88` = 136, `00` = 0). These values are **sRGB encoded**, because they come from
screens. Our helper converts them to linear for rendering:

```cpp
Color orange = hex_color(0xFF8800);   // returns LINEAR values
```

**HSV** (hue, saturation, value) is a friendlier way to *choose* colors:

* **Hue** is the angle on the color wheel: 0° red, 60° yellow, 120° green, 180° cyan, 240° blue, 300° magenta.
* **Saturation** runs from 0 (grey) to 1 (pure color).
* **Value** is brightness, from 0 (black) to 1 (brightest).

```
             60° yellow
        ╱─────────────╲
  120° green          0° red
       │      ● grey   │          (center = saturation 0)
  180° cyan          300° magenta
        ╲─────────────╱
             240° blue
```

---

## 4. Luminance: how bright does a color *look*?

Our eyes are much more sensitive to green than to blue. The perceived brightness (**luminance**) of a
linear color is:

```
Y = 0.2126 R + 0.7152 G + 0.0722 B
```

Pure green (0,1,0) has luminance 0.72; pure blue has only 0.07. We use luminance for tone mapping,
bloom thresholds, grain and denoising.

---

## 5. HDR: brighter than white

In the real world, the sun is about **100,000 times** brighter than a shadow. Linear values in our
renderer can be anything from 0 to thousands: 1.0 isn't a limit, just "the brightness we'll show as
white". An image whose values go beyond 1.0 is a **High Dynamic Range** (HDR) image.

To show HDR on a screen we have to *squeeze* the range into 0–1. That's **tone mapping**, and the
end of `color.h` contains several tone mappers. For now the default is to simply **clamp** (cut off)
values above 1. Chapter 34 covers the rest.

---

## 6. The library file `color.h`

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

Key functions:

| Function | Does |
|----------|------|
| `linear_to_srgb(x)` / `srgb_to_linear(x)` | exact sRGB curve, for one number or a whole `Color` |
| `to_byte(x)` | clamp to 0–1, scale to 0–255, round |
| `rgb255(r, g, b)`, `hex_color(0xRRGGBB)` | designer colors → linear |
| `hsv(h, s, v)` | HSV → RGB (display values: wrap in `srgb_to_linear` for rendering) |
| `tonemap_*`, `apply_tonemap` | HDR → 0..1 (chapter 34) |

---

## 7. The program

The program makes three test images. It uses the library's `Image` class and `save_image`
(explained properly in the next chapter). For now, just know: `Image img(w, h)` is a grid of
linear colors, `img.at(x, y)` is one pixel, and `save_image("file.bmp", img)` writes it with
the sRGB curve applied.

**File: `chapters/ch04_color.cpp`**

```cpp
// ch04_color.cpp
// ------------------------------------------------------------
// Chapter 4: Color for programmers.
// Produces three images:
//   images/ch04_gamma_ramps.bmp  - linear vs gamma-correct grey ramps
//   images/ch04_hsv_wheel.bmp    - a color wheel built from HSV
//   images/ch04_rgb_mixing.bmp   - red, green, blue lights adding up
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Gamma ramps --------------------------------------------
    // Top band: bytes go 0..255 evenly ("what the file says").
    // Middle band: LIGHT goes 0..1 evenly, saved with the sRGB curve.
    // Bottom band: 16 steps so you can compare the brightness jumps.
    {
        const int W = 768, H = 240;
        Image img(W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                double t = (double)x / (W - 1);
                Color c;
                if (y < 80) {
                    // We want the FILE to contain t*255, so undo the sRGB curve first.
                    c = srgb_to_linear(Color(t, t, t));
                } else if (y < 160) {
                    c = Color(t, t, t);                      // physically even light
                } else {
                    double step = std::floor(t * 16) / 15.0;  // 16 flat steps of light
                    c = Color(step, step, step);
                }
                img.at(x, y) = c;
            }
        save_image("images/ch04_gamma_ramps.bmp", img);
    }

    // ---------- 2. HSV color wheel ---------------------------------------
    {
        const int S = 400;
        Image img(S, S, Color(1, 1, 1));
        double cx = S / 2.0, cy = S / 2.0, R = S / 2.0 - 10;
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > R) continue;
                double hue = std::atan2(-dy, dx) * 180.0 / pi;   // angle = hue
                double sat = dist / R;                           // distance = saturation
                Color display = hsv(hue, sat, 1.0);              // 0..1 display values
                img.at(x, y) = srgb_to_linear(display);          // store as linear light
            }
        save_image("images/ch04_hsv_wheel.bmp", img);
    }

    // ---------- 3. Additive mixing: three colored spotlights -------------
    {
        const int W = 500, H = 460;
        Image img(W, H, Color(0, 0, 0));
        struct Spot { double x, y; Color c; };
        Spot spots[3] = { {250, 170, Color(1, 0, 0)},
                          {180, 290, Color(0, 1, 0)},
                          {320, 290, Color(0, 0, 1)} };
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                for (const Spot& s : spots) {
                    double dx = x + 0.5 - s.x, dy = y + 0.5 - s.y;
                    if (dx * dx + dy * dy < 130 * 130) img.at(x, y) += s.c;   // light ADDS
                }
        save_image("images/ch04_rgb_mixing.bmp", img);
    }
    return 0;
}
```

Run it:

```bat
run ch04_color
```

---

## 8. What you should see

### 8.1 Gamma ramps

![Gamma ramps](../images/ch04_gamma_ramps.bmp)

> **Image description:** A wide image with three horizontal bands, each going from black on
> the left to white on the right.
> * **Top band:** the *bytes in the file* increase evenly (0, 1, 2, ... 255). It looks evenly spaced
>   to your eye: it seems to reach middle grey around the middle.
> * **Middle band:** the *light* increases evenly. It looks **too bright too soon**: it's already
>   light grey a quarter of the way in, and most of the right half looks nearly white.
> * **Bottom band:** 16 flat steps of evenly increasing light. The dark steps on the left look
>   far apart; the bright steps on the right are almost impossible to tell apart.

This shows you that **perception is not linear**. Evenly spaced *light* doesn't look evenly spaced.
It's why sRGB puts more bytes in the darks.

### 8.2 HSV wheel

![HSV wheel](../images/ch04_hsv_wheel.bmp)

> **Image description:** A color wheel on white. Pure red is on the right (3 o'clock), yellow at
> about 1 o'clock, green at 11, cyan on the left (9 o'clock), blue at 7 and magenta at 5. The
> center is white, and colors get more saturated toward the rim.

### 8.3 Additive mixing

![RGB mixing](../images/ch04_rgb_mixing.bmp)

> **Image description:** Three overlapping discs on black, like stage spotlights: red at the top,
> green bottom-left, blue bottom-right. Where red and green overlap: **yellow**. Red and blue:
> **magenta**. Green and blue: **cyan**. In the very center, where all three overlap: **white**.

This works because we literally **add** light values (`img.at(x, y) += s.c`). That's physics, and it
only works in linear space.

---

## Try it yourself

1. In the gamma ramps, save the image with `SaveOptions o; o.srgb = false;` and
   `save_image(..., img, o)`. Now the *middle* band looks even and the top band looks too dark. Explain
   why.
2. In the mixing image, make the three lights half strength (`s.c * 0.5`). Before running,
   work out the values: a single light is now 0.5 in one channel, a two-light overlap is 0.5 in two
   channels, and the triple overlap is (0.5, 0.5, 0.5). What color and brightness will the center be?
   (Answer: a light grey, byte 188, not middle grey, because of the sRGB curve.)
3. Make the lights **stronger** than 1 (e.g. `* 2`). With plain clamping, what happens to the
   overlaps? This is the problem tone mapping solves (chapter 34).
4. Print the luminance of `hex_color(0x0000FF)` and `hex_color(0x00FF00)`. How many times brighter
   does green look than blue?

## Common problems

| Symptom | Cause |
|---------|-------|
| Everything looks washed out / too bright | Applied the sRGB curve twice (e.g. to a hex color that was already encoded, then again when saving) |
| Everything looks too dark and contrasty | Forgot the sRGB curve when saving linear values |
| Soft edges look dark and thin | Blending sRGB values instead of linear |

---

## Summary

* Screens mix red, green and blue light additively.
* **Linear** values measure light; **sRGB** values are what files store. Do math in linear;
  convert only at input/output.
* `hex_color` and `rgb255` convert designer colors to linear.
* Luminance weighs green most and blue least.
* Linear values can exceed 1 (HDR); we'll tone map them later.

Next: [Chapter 5 — The Image class →](05-image-class.md)
