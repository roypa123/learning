# Line by line: `ch04_color.cpp`

[← Line-by-line index](README.md) · [Chapter 4 (the theory)](../04-color.md)

**What the whole program does, in one sentence:** it makes three test pictures that teach color: grey
ramps (to see gamma), a color wheel (HSV), and three colored spotlights (light adding up).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments | 1–8 | Notes |
| B. Includes and namespace | 9–11 | Load math and **our library** |
| C. Start | 13 | `main` begins |
| D. Picture 1: gamma ramps | 14–37 | three grey bands, left dark → right light |
| E. Picture 2: HSV wheel | 39–55 | a circle of all hues |
| F. Picture 3: RGB mixing | 57–72 | three overlapping colored discs |
| G. End | 73–74 | finish |

---

## Block A — Comments (lines 1–8)

The file name, the chapter, and the three pictures it makes. Ignored by the computer.

---

## Block B — Includes and namespace (lines 9–11)

```cpp
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;
```

* Line 9: math functions: `std::floor`, `std::sqrt`, `std::atan2`.
* Line 10: **our whole library** in one line. `pixel.h` includes every file in `include/pixel/`
  (`Image`, `Color`, `save_image`, `hsv`, ...). The quotes `" "` mean "our own file". The build command's
  `-Iinclude` tells the compiler to look for it in the `include` folder.
* Line 11: `using namespace pixel;` lets us write `Image` instead of `pixel::Image`.

---

## Block C — Start (line 13)

```cpp
int main() {
```

The program starts here (see [ch03 line 15](ch03_first_image.md)).

---

## Block D — Picture 1: gamma ramps (lines 14–37)

**What this block does:** it makes a 768 × 240 image with three bands (80 pixels tall each). Every band goes
from black on the left to white on the right, but each in a different way, so you can *see* what gamma does.

### Lines 14–18

```cpp
    // ---------- 1. Gamma ramps --------------------------------------------
    // Top band: bytes go 0..255 evenly ("what the file says").
    // Middle band: LIGHT goes 0..1 evenly, saved with the sRGB curve.
    // Bottom band: 16 steps so you can compare the brightness jumps.
    {
```

Comments explaining the three bands, then `{` opens a block. (Using a block lets us reuse names like
`img` in the next pictures.)

### Lines 19–20

```cpp
        const int W = 768, H = 240;
        Image img(W, H);
```

* Line 19: width 768 and height 240, as two constants in one line.
* Line 20: creates an **Image** (from our library, see [image.md](image.md)): a grid of 768 × 240 colors, all
  black at the start. Each pixel is a `Color` holding **linear** light.

### Lines 21–22: visit every pixel

```cpp
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
```

The two nested loops from chapter 3: every row `y`, and inside it every column `x`.
Note the `y` loop has no `{ }`: when a loop has exactly **one** statement (here, the inner loop), braces are
optional.

### Line 23: position as a fraction

```cpp
                double t = (double)x / (W - 1);
```

`t` goes from 0.0 at the left edge to 1.0 at the right edge. `(double)x` makes sure it's a fraction division.

### Line 24

```cpp
                Color c;
```

An empty color (0, 0, 0). We'll decide its value below.

### Lines 25–27: top band (y from 0 to 79)

```cpp
                if (y < 80) {
                    // We want the FILE to contain t*255, so undo the sRGB curve first.
                    c = srgb_to_linear(Color(t, t, t));
```

* `if (y < 80)`: only for the top 80 rows.
* We want the **bytes in the file** to go evenly 0, 1, 2 ... 255.
* But `save_image` will apply the sRGB curve when saving. So we first apply the **opposite** curve
  (`srgb_to_linear`). The two cancel out, and the file gets exactly `t × 255`.
* `Color(t, t, t)`: the same value for red, green and blue gives a **grey**.

### Lines 28–29: middle band (y from 80 to 159)

```cpp
                } else if (y < 160) {
                    c = Color(t, t, t);                      // physically even light
```

* `else if`: only if the first test failed and this one is true.
* Here the **light** goes evenly from 0 to 1. After saving (with the sRGB curve) this band looks
  "too bright too soon", because your eyes are more sensitive to changes in dark tones.

### Lines 30–33: bottom band (y from 160 to 239)

```cpp
                } else {
                    double step = std::floor(t * 16) / 15.0;  // 16 flat steps of light
                    c = Color(step, step, step);
                }
```

* `else`: all remaining rows.
* `t * 16` goes from 0 to 16. `std::floor` rounds **down** to a whole number: 0, 1, 2 ... 15 (and 16 at the very
  last pixel). So the band is split into 16 flat blocks.
* `/ 15.0` turns block numbers 0–15 into light values 0.0–1.0. (The very last pixel gets 16/15 ≈ 1.07, which
  is simply cut to white when saving. No harm.)
* Result: 16 grey blocks with **equal steps of light**. The dark blocks look very different from each
  other; the bright ones look almost the same. That's the eye's non-linear response.

### Line 34: store the color

```cpp
                img.at(x, y) = c;
```

`img.at(x, y)` is the pixel at column x, row y. We put our color there.

### Line 35

```cpp
            }
```

End of the inner loop body (opened by `{` on line 22).

### Line 36: save

```cpp
        save_image("images/ch04_gamma_ramps.bmp", img);
```

* Our library function. It looks at the file extension (`.bmp`) and writes a BMP file.
* On the way, it converts every linear color to sRGB and then to bytes 0–255.
* It prints `Saved images/ch04_gamma_ramps.bmp (768x240)`.

### Line 37

```cpp
    }
```

End of the block. `img` is destroyed, which frees its memory.

---

## Block E — Picture 2: HSV color wheel (lines 39–55)

**What this block does:** draws a disc where the **angle** around the center is the hue (the color) and the
**distance** from the center is the saturation (grey in the middle → strong color at the edge).

### Lines 41–43: setup

```cpp
        const int S = 400;
        Image img(S, S, Color(1, 1, 1));
        double cx = S / 2.0, cy = S / 2.0, R = S / 2.0 - 10;
```

* Line 41: the picture is 400 × 400 (S = "size").
* Line 42: an image filled with **white** `Color(1, 1, 1)` (the third input of the Image constructor is the
  fill color).
* Line 43: the center of the circle (`cx`, `cy` = 200, 200) and its radius `R` = 190 (10 pixels of white margin).
  `S / 2.0` with `.0` gives a fraction result.

### Lines 44–45: every pixel

```cpp
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
```

### Lines 46–48: how far from the center?

```cpp
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > R) continue;
```

* Line 46: `dx`, `dy` = how far this pixel's **center** is from the circle's center, horizontally and vertically.
  `+ 0.5` because the center of pixel x is at x + 0.5 (a pixel is a small square from x to x+1).
* Line 47: the straight-line distance, by Pythagoras: √(dx² + dy²).
* Line 48: if the pixel is outside the circle, `continue` = **skip the rest** of this loop round and go to the
  next pixel. That pixel stays white.

### Line 49: angle → hue

```cpp
                double hue = std::atan2(-dy, dx) * 180.0 / pi;   // angle = hue
```

* `std::atan2(y, x)` gives the **angle** of the point (x, y), in radians, from −π to π. Angle 0 points right.
* We use `-dy` because in images y goes **down**, but in math angles go **up** (counter-clockwise). The minus
  sign flips it so the wheel goes the usual way.
* `* 180.0 / pi` converts radians to degrees (−180 to 180). The `hsv` function handles negative values.
* So: right side = 0° = red, top = 90° ≈ yellow-green, left = 180° = cyan, bottom = 270° = blue-magenta.

### Line 50: distance → saturation

```cpp
                double sat = dist / R;                           // distance = saturation
```

0 in the center (grey/white), 1 at the edge (pure color).

### Lines 51–52: make the color and store it

```cpp
                Color display = hsv(hue, sat, 1.0);              // 0..1 display values
                img.at(x, y) = srgb_to_linear(display);          // store as linear light
```

* Line 51: `hsv` (from `color.h`) turns hue, saturation and value (brightness 1.0 = full) into an RGB color.
  This color is in **display** (sRGB) terms, the way color pickers work.
* Line 52: our images store **linear** light, so we convert. When saving, the sRGB curve is applied again, and the
  file gets exactly the display color. (Convert in → convert out = unchanged.)

### Lines 53–55

```cpp
            }
        save_image("images/ch04_hsv_wheel.bmp", img);
    }
```

End of the loops, save the picture, end of the block.

---

## Block F — Picture 3: RGB mixing (lines 57–72)

**What this block does:** three round "spotlights" (red, green, blue) on black. Where they overlap, the light
**adds up**: red + green = yellow, all three = white.

### Lines 59–60

```cpp
        const int W = 500, H = 460;
        Image img(W, H, Color(0, 0, 0));
```

A 500 × 460 image, filled with black (no light).

### Line 61: a small struct for a spotlight

```cpp
        struct Spot { double x, y; Color c; };
```

* We define a tiny new type `Spot` right here inside `main`. It holds a center (`x`, `y`) and a color `c`.
* Written on one line: `struct Name { members };`.

### Lines 62–64: the three spots

```cpp
        Spot spots[3] = { {250, 170, Color(1, 0, 0)},
                          {180, 290, Color(0, 1, 0)},
                          {320, 290, Color(0, 0, 1)} };
```

* An **array** of 3 spots. `spots[0]`, `spots[1]`, `spots[2]`.
* Each `{ x, y, color }` fills one Spot in order.
* Red at the top (250, 170); green bottom-left (180, 290); blue bottom-right (320, 290). A triangle
  arrangement, so all three overlap in the middle.

### Lines 65–66: every pixel

```cpp
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
```

### Line 67: for each spotlight

```cpp
                for (const Spot& s : spots) {
```

* A **range-based for loop**: "for each `s` in `spots`". It runs 3 times, with `s` = each spot in turn.
* `const Spot&` = look at the spot directly (no copy) and don't change it.

### Lines 68–69: is this pixel inside the spot? If yes, add its light

```cpp
                    double dx = x + 0.5 - s.x, dy = y + 0.5 - s.y;
                    if (dx * dx + dy * dy < 130 * 130) img.at(x, y) += s.c;   // light ADDS
```

* Line 68: distance parts from the pixel center to the spot center (like line 46).
* Line 69: inside the circle of radius 130? We compare **squared** distances (dx² + dy² < 130²). That's the same
  test without a slow square root.
* `+=` **adds** the spot's color to whatever is already there. That's the key idea: **light adds**.
  * One spot: e.g. (1, 0, 0) = red.
  * Red + green spots: (1, 1, 0) = yellow.
  * All three: (1, 1, 1) = white.

### Lines 70–72

```cpp
                }
        save_image("images/ch04_rgb_mixing.bmp", img);
    }
```

End of the spot loop, save, end of block.

---

## Block G — End (lines 73–74)

```cpp
    return 0;
}
```

Success, and the end of `main`.

---

## The whole program as a picture

```
main
 ├─ picture 1 (768x240): for each pixel, t = x / 767
 │     rows   0–79 : srgb_to_linear(t)   → file bytes go evenly
 │     rows  80–159: t                   → light goes evenly
 │     rows 160–239: 16 steps of light
 │     save BMP
 ├─ picture 2 (400x400, white): for each pixel inside radius 190
 │     hue = angle, saturation = distance → hsv → linear → store
 │     save BMP
 ├─ picture 3 (500x460, black): for each pixel, for each of 3 spots
 │     inside the spot? → ADD its color
 │     save BMP
 └─ return 0
```

## Check your understanding

1. Why does line 27 use `srgb_to_linear`? *(To cancel the sRGB curve that `save_image` applies, so the file
   bytes go evenly.)*
2. What does `continue` on line 48 do? *(Skips the rest of this pixel's work: it stays white.)*
3. What color is a pixel inside all three spots? *(1, 1, 1): white.)*
4. Why `-dy` on line 49? *(Image y goes down, math angles go up.)*
