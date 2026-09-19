# Line by line: `include/pixel/color.h`

[← Line-by-line index](README.md) · [Chapter 4 (the theory)](../04-color.md)

**What this file does, in one sentence:** it converts between **light numbers** (what the renderer
computes) and **screen numbers** (what files and monitors use), and gives us easy ways to make colors.

Remember from chapter 4: inside our program all colors are **linear** (they measure real light). Files and
screens use **sRGB** (bent by a "gamma" curve). This file does the conversions.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes, namespace | 1–14 | Notes, tools, the `pixel` family name |
| B. sRGB curve for one number | 16–30 | linear ↔ sRGB, the exact official formula |
| C. Gamma 2 and whole colors | 32–40 | a simple version, and versions for a whole `Color` |
| D. To a byte | 42–46 | 0.0–1.0 → 0–255 |
| E. Making colors | 48–77 | from 0–255 numbers, from hex codes, from HSV |
| F. Tone mapping | 79–124 | squeeze very bright light into 0–1 |
| G. End | 126 | close the namespace |

---

## Block A — Comments, includes, namespace (lines 1–14)

```cpp
// pixel/color.h
// ... (comment lines) ...
#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"

namespace pixel {
```

* Lines 1–8: comments describing the file.
* Line 9 `#pragma once`: include this header only once (see [vec3.md](vec3.md)).
* Line 10 `<cmath>`: `std::pow` (power), `std::sqrt`, `std::fmod`, `std::fabs`.
* Line 11 `<cstdint>`: `uint8_t` (a byte) and `uint32_t` (a 4-byte number).
* Line 12 `#include "vec3.h"`: **our own** file (quotes, not `< >`). We need `Color` and `clamp01` from it.
* Line 14: start of the `pixel` namespace.

---

## Block B — The sRGB curve (lines 16–30)

### Lines 16–19: comments

```cpp
// ---------- Gamma / sRGB ----------------------------------------------------
// Monitors expect "gamma encoded" values. Our renderer works with linear light.

// Exact sRGB transfer function (linear 0..1 -> encoded 0..1)
```

A section title and an explanation.

### Lines 20–24: linear → sRGB (used when **saving**)

```cpp
inline double linear_to_srgb(double x) {
    if (x <= 0.0) return 0.0;
    if (x <= 0.0031308) return 12.92 * x;
    return 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}
```

* Line 20: a function that takes a linear value `x` (0 to 1) and returns the sRGB value (0 to 1).
* Line 21: `if (x <= 0.0) return 0.0;`: negative or zero light becomes 0. `return` immediately ends the function.
* Line 22: for **very dark** values the official formula is a straight line: multiply by 12.92.
  (This avoids a problem with the curve being too steep near 0.)
* Line 23: for everything else: `1.055 × x^(1/2.4) − 0.055`.
  * `std::pow(x, 1.0 / 2.4)` = x to the power 1/2.4, roughly the square root, but a bit stronger.
  * This **lifts** dark values: linear 0.214 becomes 0.5.
* Line 24: `}` ends the function.

### Lines 26–30: sRGB → linear (used when **loading** files and for hex colors)

```cpp
// Inverse: encoded 0..1 -> linear 0..1
inline double srgb_to_linear(double x) {
    if (x <= 0.04045) return x / 12.92;
    return std::pow((x + 0.055) / 1.055, 2.4);
}
```

The exact **opposite** of the function above:

* Line 28: small values: divide by 12.92 (undo line 22).
* Line 29: others: undo line 23 step by step: add 0.055, divide by 1.055, then raise to the power 2.4.
* Example: sRGB 0.5 → linear 0.214.

---

## Block C — Gamma 2 and whole colors (lines 32–40)

### Lines 32–33: the simple approximation

```cpp
// The simple "gamma 2" approximation used in early chapters.
inline double linear_to_gamma2(double x) { return x > 0.0 ? std::sqrt(x) : 0.0; }
```

* A simpler curve: just the square root. Very close to sRGB. Many tutorials use it.
* `x > 0.0 ? std::sqrt(x) : 0.0`: square root if positive, otherwise 0 (you can't take the square root of a
  negative number).

### Lines 35–40: the same for a whole Color

```cpp
inline Color linear_to_srgb(const Color& c) {
    return Color(linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z));
}
inline Color srgb_to_linear(const Color& c) {
    return Color(srgb_to_linear(c.x), srgb_to_linear(c.y), srgb_to_linear(c.z));
}
```

* These have the **same names** as the functions above but take a `Color` instead of a `double`. C++
  chooses the right one by the input type. This is called **overloading**.
* They simply apply the one-number version to red (`c.x`), green (`c.y`) and blue (`c.z`), and build a new Color.

---

## Block D — To a byte (lines 42–46)

```cpp
// Convert a 0..1 value to a byte 0..255 (with rounding and clamping).
inline uint8_t to_byte(double x) {
    int v = (int)(clamp01(x) * 255.0 + 0.5);
    return (uint8_t)(v > 255 ? 255 : v);
}
```

* Line 43: takes a 0–1 number and returns a byte (`uint8_t`).
* Line 44, step by step:
  1. `clamp01(x)`: first force it into 0–1 (a value like 1.3 becomes 1, −0.2 becomes 0).
  2. `* 255.0`: scale to 0–255.
  3. `+ 0.5` then `(int)`: this is **rounding** to the nearest whole number. `(int)` cuts off the fraction,
     so adding 0.5 first makes 127.6 → 128.1 → 128, and 127.4 → 127.9 → 127.
* Line 45: safety: never above 255. Then convert to `uint8_t`.

(Chapter 3 used `255.999 *` and cutting; here we use proper rounding. Both are fine.)

---

## Block E — Making colors (lines 48–77)

### Lines 50–54: from three numbers 0–255

```cpp
// From 0..255 integers, already sRGB encoded (like colors in a paint program).
// Returned color is LINEAR, ready for rendering math.
inline Color rgb255(int r, int g, int b) {
    return srgb_to_linear(Color(r / 255.0, g / 255.0, b / 255.0));
}
```

* A paint program says "orange is 255, 136, 0". Those numbers are sRGB.
* Line 53: divide each by 255.0 to get 0–1 (the `.0` makes it a fraction division!), build a `Color`, then convert
  to **linear** so the renderer can do correct math with it.

### Lines 56–59: from a hex code

```cpp
// From a hex code like 0xFF8800 (orange). Returned color is LINEAR.
inline Color hex_color(uint32_t hex) {
    return rgb255((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}
```

* A hex code like `0xFF8800` packs red, green, blue into one number: `FF` `88` `00`.
* `hex >> 16` shifts right by 16 bits (2 bytes), leaving `0xFF`, which is red. `& 0xFF` keeps only one byte.
* `hex >> 8` then `& 0xFF` → `0x88`, green.
* `hex & 0xFF` → `0x00`, blue.
* Then it calls `rgb255` (lines 52–54) with 255, 136, 0.

### Lines 61–77: from HSV (hue, saturation, value)

```cpp
// Hue (0..360), saturation (0..1), value (0..1) -> RGB (0..1, not linearized)
inline Color hsv(double h, double s, double v) {
```

* Hue = the color's angle on the color wheel (0 = red, 120 = green, 240 = blue).
* Saturation = how colorful (0 = grey).
* Value = how bright.
* The result is **display** (sRGB) values. To render with it, wrap it in `srgb_to_linear(...)`.

```cpp
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360.0;
```

* Line 63: `std::fmod(h, 360.0)` = the remainder after dividing by 360. So 400° becomes 40°.
* Line 64: a negative angle like −30° becomes 330°. Now h is always 0–360.

```cpp
    double c = v * s;
    double hp = h / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
```

* Line 65: `c` ("chroma") = the strength of the color part.
* Line 66: `hp` = which of the 6 sections of the color wheel we're in (each section is 60° wide), as a number 0–6.
* Line 67: `x` = the strength of the **second** color in this section. It rises and falls in a zig-zag inside
  each section. (For example, between red and yellow the green part grows from 0 to full.)

```cpp
    Color rgb;
    if      (hp < 1) rgb = Color(c, x, 0);
    else if (hp < 2) rgb = Color(x, c, 0);
    else if (hp < 3) rgb = Color(0, c, x);
    else if (hp < 4) rgb = Color(0, x, c);
    else if (hp < 5) rgb = Color(x, 0, c);
    else             rgb = Color(c, 0, x);
```

* Line 68: an empty color to fill in.
* Lines 69–74: pick which channels get `c` (full) and `x` (partial) depending on the section:

| Section | Degrees | From → to | R | G | B |
|---------|---------|-----------|---|---|---|
| 0 | 0–60 | red → yellow | c | x | 0 |
| 1 | 60–120 | yellow → green | x | c | 0 |
| 2 | 120–180 | green → cyan | 0 | c | x |
| 3 | 180–240 | cyan → blue | 0 | x | c |
| 4 | 240–300 | blue → magenta | x | 0 | c |
| 5 | 300–360 | magenta → red | c | 0 | x |

```cpp
    double m = v - c;
    return rgb + Color(m, m, m);
}
```

* Line 75: `m` = the grey part (it's 0 when saturation is 1).
* Line 76: add the grey part to all three channels. Less saturated colors get more grey.
* Line 77: end of the function.

---

## Block F — Tone mapping (lines 79–124)

Rendered light can be **brighter than 1** (a lamp can be 400). A screen can only show up to 1. A **tone
mapper** squeezes the values into 0–1. (Full story in chapter 34.)

### Lines 83–85: clamp

```cpp
inline Color tonemap_clamp(const Color& c) {
    return Color(clamp01(c.x), clamp01(c.y), clamp01(c.z));
}
```

Simply cut everything above 1. Bright areas become flat white.

### Lines 87–89: Reinhard

```cpp
inline Color tonemap_reinhard(const Color& c) {
    return Color(c.x / (1.0 + c.x), c.y / (1.0 + c.y), c.z / (1.0 + c.z));
}
```

For each channel: `x / (1 + x)`. 1 → 0.5, 3 → 0.75, 100 → 0.99. It never reaches 1 and never clips, but images
look a bit flat.

### Lines 91–98: ACES (the "movie look")

```cpp
// ACES filmic curve (Krzysztof Narkowicz's fit). The "movie look".
inline double aces_curve(double x) {
    const double a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}
inline Color tonemap_aces(const Color& c) {
    return Color(aces_curve(c.x), aces_curve(c.y), aces_curve(c.z));
}
```

* Line 93: five fixed numbers. They were chosen (by fitting) so that this small formula copies the
  official film-industry ACES curve closely.
* Line 94: the formula. It gives an S-shape: soft dark tones, strong middle contrast, bright values that roll
  gently to white. `clamp01` makes sure the result stays within 0–1.
* Lines 96–98: apply it to each channel.

### Lines 100–112: Hable (Uncharted 2)

```cpp
inline double hable_partial(double x) {
    const double A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
```

* Another film-like S-curve, made by John Hable for the game *Uncharted 2*. A–F control the shape of the toe
  (dark part) and shoulder (bright part).

```cpp
inline Color tonemap_hable(const Color& c) {
    const double exposure_bias = 2.0;
    const double white = 11.2;
    double w = 1.0 / hable_partial(white);
    return Color(hable_partial(c.x * exposure_bias) * w,
                 hable_partial(c.y * exposure_bias) * w,
                 hable_partial(c.z * exposure_bias) * w);
}
```

* Line 106: brighten the input 2× first (the curve is designed for that).
* Line 107: `white` = the input value that should become pure white (11.2).
* Line 108: `w` = a scale factor so that `white` maps exactly to 1.
* Lines 109–111: apply the curve to each channel, times `w`.

### Line 114: a list of choices

```cpp
enum class ToneMapper { Clamp, Reinhard, Aces, Hable };
```

* An `enum class` is a type with a fixed list of named values. A `ToneMapper` can only be one of these four.
* We use it in `SaveOptions` (in `image.h`): `opt.tonemap = ToneMapper::Aces;`.

### Lines 116–124: choose and apply

```cpp
inline Color apply_tonemap(const Color& c, ToneMapper tm) {
    switch (tm) {
        case ToneMapper::Clamp:    return tonemap_clamp(c);
        case ToneMapper::Reinhard: return tonemap_reinhard(c);
        case ToneMapper::Aces:     return tonemap_aces(c);
        case ToneMapper::Hable:    return tonemap_hable(c);
    }
    return tonemap_clamp(c);
}
```

* Line 117: `switch (tm)` jumps to the `case` that matches the value of `tm`.
* Lines 118–121: each case calls the matching tone mapper and returns its result.
* Line 123: a safety default (the compiler likes every path to return something).

---

## Block G — End (line 126)

```cpp
} // namespace pixel
```

Closes the `pixel` namespace.

---

## Check your understanding

1. What does `hex_color(0x00FF00)` return? *(Pure green, (0, 1, 0), which is already linear because 0 and 1
   don't change.)*
2. Why does `rgb255` divide by `255.0` and not `255`? *(Whole-number division would give 0 for every value
   below 255.)*
3. `to_byte(0.5)` gives? *(0.5 × 255 + 0.5 = 128.0 → 128)*
4. A pixel has linear value 3.0. What does clamp give? Reinhard? *(1.0 and 0.75)*
