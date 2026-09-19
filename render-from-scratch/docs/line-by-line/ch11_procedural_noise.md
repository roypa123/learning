# Line by line: `ch11_procedural_noise.cpp`

[← Line-by-line index](README.md) · [Chapter 11 (the theory)](../11-procedural-noise.md) · [noise.h explained](noise.md)

**What the whole program does, in one sentence:** it shows three kinds of noise side by side (white, value, fBm),
then paints a complete landscape (sky, clouds, sun, mountains, lake) using only noise and math.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–14 | |
| B. White noise | 16–25 | every pixel random |
| C. Value noise | 27–36 | smooth random hills |
| D. fBm | 38–47 | smooth hills + fine detail |
| E. Landscape: sky and clouds | 49–66 | gradient + noise clouds + sun |
| F. Landscape: mountains | 68–86 | three layers, heights from noise |
| G. Landscape: lake | 88–99 | a rippled, darkened mirror image |
| H. End | 100–102 | |

---

## Block A — Comments, includes (lines 1–14)

* Lines 1–11: comments, `<cmath>`, our library, `using namespace pixel`.
* Line 13: `main` starts.
* Line 14: `S = 256`: the size of the three small test images.

---

## Block B — White noise (lines 16–25)

```cpp
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = hash2_01(x, y);
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_white_noise.png", img);
```

* Line 18: a 256 × 256 image.
* Lines 19–20: every pixel.
* Line 21: `hash2_01(x, y)` = a random-looking number 0–1 **for this exact position** (see [noise.md](noise.md)).
  Neighboring pixels have unrelated values.
* Line 22: a grey of that brightness. We want brightness to look even on screen, so we convert from screen values to
  linear (saving converts back).
* Line 24: save. It looks like TV static.

---

## Block C — Value noise (lines 27–36)

```cpp
                double v = value_noise_2d(x / 32.0, y / 32.0);   // grid cell = 32 pixels
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
```

* Line 32: the same idea, but with **value noise**. Dividing the coordinates by 32 means one noise grid cell covers
  32 pixels, so we see smooth blobs about 32 pixels apart. (Without dividing, every pixel would be a new grid point,
  and it would look like white noise again.)
* Line 33: store as grey.

---

## Block D — fBm (lines 38–47)

```cpp
                double v = fbm_2d(x / 64.0, y / 64.0, 6);
```

Line 43: **fBm** with 6 layers. The first layer's blobs are 64 pixels apart; each extra layer adds details twice as
small. The result looks like clouds or terrain seen from above.

---

## Block E — Landscape: sky and clouds (lines 49–66)

```cpp
        const int W = 960, H = 540;
        Image img(W, H);
        const double horizon = 330;
```

* Lines 51–52: a 960 × 540 image.
* Line 53: the horizon (the line where the lake starts) is at row 330.

```cpp
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                double t = y / horizon;
                Color sky = lerp(hex_color(0x0F2027), hex_color(0xF8B195), std::pow(clamp01(t), 1.6));
```

* Lines 56–57: every pixel (the lake will be painted over later).
* Line 58: `t` = 0 at the top, 1 at the horizon (and above 1 below it).
* Line 59: the sky color, from dark blue-teal (top) to peach (horizon). `clamp01(t)` limits t to 0–1, and
  `pow(t, 1.6)` bends it so the sky stays dark longer and brightens mostly near the horizon.

```cpp
                double cloud = fbm_2d(x / 180.0, y / 60.0 + 3.7, 6);
                cloud = smoothstep(0.52, 0.75, cloud) * (1.0 - t * 0.6);
                img.at(x, y) = lerp(sky, hex_color(0xF67280) * 0.9 + Color(0.1, 0.05, 0.1), cloud * 0.8);
            }
```

* Line 60: cloud noise. Dividing x by 180 but y by 60 **stretches** the noise horizontally, since clouds are wider than
  they are tall. `+ 3.7` just moves to another part of the noise (a different look).
* Line 61: `smoothstep(0.52, 0.75, cloud)` turns the soft noise into distinct clouds: below 0.52 → 0 (clear sky),
  above 0.75 → 1 (full cloud), a smooth edge in between. `* (1 − 0.6t)` makes clouds fainter near the horizon.
* Line 62: blend the sky toward a pink cloud color by the cloud amount (at most 80%), and store it.

```cpp
        Canvas cv(img);
        cv.glow(W * 0.62, horizon - 30, 300, hex_color(0xFFB347) * 0.8);
        cv.fill_circle_aa(W * 0.62, horizon - 30, 34, hex_color(0xFFF1C1));
```

* Line 64: a canvas for drawing.
* Lines 65–66: the sun, at 62% of the width, 30 pixels above the horizon: a big orange glow, then a pale disc.

---

## Block F — Landscape: mountains (lines 68–86)

```cpp
        struct Layer { double base, amp, freq; uint32_t color; double seed; };
        Layer layers[] = {
            {horizon - 40, 110, 1 / 260.0, 0x6C5B7B, 10.0},
            {horizon - 10, 90, 1 / 200.0, 0x4B3F6B, 20.0},
            {horizon + 5, 60, 1 / 140.0, 0x2A2344, 30.0},
        };
```

* Line 69: settings for one layer: `base` = average height line, `amp` = how tall the peaks can go, `freq` = how
  quickly the height changes across (smaller = wider mountains), `color`, and `seed` = which part of the noise to
  use.
* Lines 70–74: three layers, from far (lighter, wider, taller) to near (darker, narrower).

```cpp
        for (const Layer& L : layers) {
            for (int x = 0; x < W; x++) {
                double h = fbm_2d(x * L.freq, L.seed, 6);
                double top = L.base - (h - 0.3) * L.amp * 2.0;
```

* Line 75: for each layer.
* Line 76: for each column x.
* Line 77: a noise value for this column. We use fBm with x as the first coordinate and a **fixed** y (the seed):
  that gives a 1D wiggly line: the mountain's outline.
* Line 78: turn it into the y position of the mountain top in this column. Higher noise → smaller y → higher on the
  screen. (`− 0.3` and `× 2` just shift and stretch the typical 0.3–0.7 fBm values into a nice range.)

```cpp
                for (int y = (int)std::max(0.0, std::floor(top)); y <= horizon && y < H; y++) {
                    double coverage = clamp01(y + 1 - top);            // anti-aliased top edge
                    double haze = clamp01((horizon - y) / 200.0) * 0.3; // aerial perspective
                    Color c = lerp(hex_color(L.color), hex_color(0xF8B195), haze);
                    cv.blend_pixel(x, y, c, coverage);
                }
            }
        }
```

* Line 79: fill this column from the mountain top down to the horizon (never above row 0).
* Line 80: **coverage** for a smooth top edge: the top pixel may be only partly covered (if `top` = 120.3, pixel 120 is
  70% covered). All lower pixels get coverage 1.
* Line 81: **haze**: higher parts of the mountain (farther from the horizon) get mixed a little (up to 30%) with the
  sky color, the way distant peaks look paler.
* Line 82: the final color.
* Line 83: draw it with the coverage as alpha.

---

## Block G — Landscape: lake (lines 88–99)

```cpp
        for (int y = (int)horizon + 1; y < H; y++) {
            double depth = (y - horizon) / (H - horizon);
```

* Line 89: every row below the horizon.
* Line 90: `depth` = 0 just below the horizon, 1 at the bottom of the image.

```cpp
            for (int x = 0; x < W; x++) {
                double ripple = (value_noise_2d(x / 40.0, y / 3.0) - 0.5) * 12.0 * depth;
```

* Line 91: every column.
* Line 92: a sideways **ripple** offset: noise from −0.5 to 0.5, times 12 pixels, times depth (stronger ripples closer
  to us). `y / 3.0` makes the noise change quickly from row to row, which looks like small horizontal waves.

```cpp
                int sx = (int)clampd(x + ripple, 0, W - 1);
                int sy = (int)clampd(2 * horizon - y, 0, horizon);
```

* Line 93: the source column: shifted by the ripple, kept inside the image.
* Line 94: the source row: the **mirror** of y around the horizon. 5 rows below the horizon reads 5 rows above it.
  That's the reflection.

```cpp
                Color reflected = img.at(sx, sy) * (0.6 - 0.3 * depth);
                img.at(x, y) = lerp(reflected, hex_color(0x0B0F1A), depth * 0.6);
            }
        }
        save_image("images/ch11_landscape.png", img);
```

* Line 95: read the mirrored pixel and darken it (60% of the light near the horizon, 30% at the bottom). Water
  reflects only part of the light.
* Line 96: also blend toward a very dark blue as we come closer (the water's own color), and store it.
* Line 99: save.

---

## Block H — End (lines 100–102)

End of block, `return 0;`, end of `main`.

---

## Check your understanding

1. Why divide x by 32 in the value-noise picture? *(So one noise cell covers 32 pixels: bigger, smoother blobs.)*
2. What does `smoothstep(0.52, 0.75, cloud)` do? *(Turns soft noise into clouds with clear sky between them.)*
3. How is the lake a mirror? *(Row y reads row `2 × horizon − y`.)*
4. Why are ripples stronger at the bottom? *(They are multiplied by `depth`: closer water looks more wavy.)*
