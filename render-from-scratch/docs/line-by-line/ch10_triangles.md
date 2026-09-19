# Line by line: `ch10_triangles.cpp`

[← Line-by-line index](README.md) · [Chapter 10 (the theory)](../10-triangles-gradients.md) · [canvas.h explained](canvas.md)

**What the whole program does, in one sentence:** it draws the classic red-green-blue triangle, then a low-poly
sunset poster with a sun, three mountain ranges made of triangles, and birds.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. Picture 1: RGB triangle | 12–20 | one triangle with a different color at each corner |
| C. Picture 2: sky and sun | 22–31 | gradient sky, glow, sun disc |
| D. Mountain settings | 33–40 | three ranges: height, colors, random seed |
| E. Building each range | 41–56 | random peaks, filled with triangles |
| F. Birds | 58–63 | little "V" shapes |
| G. Save and end | 64–67 | |

---

## Block A — Comments, includes (lines 1–10)

Comments, `<cmath>`, `<vector>` (lists of peak positions), our library, `using namespace pixel`.

---

## Block B — Picture 1: RGB triangle (lines 12–20)

```cpp
        Image img(400, 360, hex_color(0x111111));
        Canvas cv(img);
        cv.fill_triangle(200, 20, 380, 330, 20, 330,
                         Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1));
        save_image("images/ch10_rgb_triangle.png", img);
```

* Line 15: a 400 × 360 near-black image.
* Lines 17–18: one filled triangle. The three corners: top (200, 20), bottom-right (380, 330), bottom-left (20, 330).
  The three colors, in the same order: red, green, blue. Inside, every pixel's color is a mix of the three, weighted by
  how close it is to each corner (barycentric coordinates, see [canvas.md block F](canvas.md)).
* Line 19: save.

---

## Block C — Picture 2: sky and sun (lines 22–31)

```cpp
        const int W = 800, H = 450;
        Image img(W, H);
        Canvas cv(img);
        cv.vertical_gradient(0, H, hex_color(0x2B1055), hex_color(0xF7A072));
        cv.glow(560, 250, 260, hex_color(0xFF9E00) * 0.9);
        cv.fill_circle_aa(560, 250, 60, hex_color(0xFFE8A3));
```

* Lines 24–26: an 800 × 450 image and canvas.
* Line 29: the sky, from deep purple (top) to peach (bottom).
* Line 30: a big orange **glow** (added light, radius 260) centered where the sun is.
* Line 31: the sun: a smooth pale-yellow disc of radius 60, on top of the glow.

---

## Block D — Mountain settings (lines 33–40)

```cpp
        struct Range { double base_y, height; uint32_t light, dark; uint64_t seed; };
        Range ranges[] = {
            {300, 140, 0x9D4EDD, 0x7B2CBF, 1},
            {340, 130, 0x5A189A, 0x3C096C, 2},
            {390, 120, 0x240046, 0x10002B, 3},
        };
```

* Line 35: settings for one mountain range:
  * `base_y` = the lowest the peaks go (a y position),
  * `height` = how much higher peaks can rise above that,
  * `light`, `dark` = the colors of the sunlit and shadowed faces,
  * `seed` = its own random seed (each range gets different peaks).
* Lines 36–40: three ranges, from **far** (first) to **near** (last). The far one is higher on the screen and lighter
  in color (distant mountains look paler because of the air); the near one is lower and nearly black.
  They're listed in this order so they're drawn **back to front**.

---

## Block E — Building each range (lines 41–56)

```cpp
        for (const Range& r : ranges) {
            Pcg32 rng(r.seed);
            std::vector<double> xs, ys;
```

* Line 41: for each range `r`.
* Line 42: a random generator with this range's seed.
* Line 43: two lists: the x and y positions of the peaks.

```cpp
            for (double x = -40; x <= W + 40; x += 40 + rng.next_double() * 60) {
                xs.push_back(x);
                ys.push_back(r.base_y - rng.next_double() * r.height);
            }
```

* Line 44: walk from x = −40 to W + 40 (a bit beyond both edges, so the mountains cover the whole width), in
  **random steps** of 40–100 pixels (`40 + random × 60`).
* Line 45: remember this peak's x.
* Line 46: its y: `base_y` minus a random part of `height`. Remember y goes **down**, so subtracting makes the peak
  **higher** on the screen.

```cpp
            Color light = hex_color(r.light), dark = hex_color(r.dark);
```

Line 48: turn the hex colors into linear colors.

```cpp
            for (size_t i = 0; i + 1 < xs.size(); i++) {
                double mx = 0.5 * (xs[i] + xs[i + 1]);
```

* Line 49: for each pair of neighboring peaks (i and i + 1). `i + 1 < xs.size()` stops before the last one, since it
  has no right neighbor.
* Line 50: `mx` = the x halfway between the two peaks.

```cpp
                cv.fill_triangle(xs[i], ys[i], xs[i + 1], ys[i + 1], mx, H, light, dark, dark);
                cv.fill_triangle(xs[i], ys[i], mx, H, xs[i], H, dark, dark, dark);
                cv.fill_triangle(xs[i + 1], ys[i + 1], xs[i + 1], H, mx, H, light * 0.8, dark, dark);
            }
        }
```

The area under the line between the two peaks, down to the bottom of the image, is filled with 3 triangles:

```
   peak i ●─────────● peak i+1
          │╲       ╱│
          │ ╲  1  ╱ │
          │2 ╲   ╱ 3│
          │   ╲ ╱   │
   bottom ●────●────●
               mx
```

* Line 52: triangle 1: both peaks and the bottom-middle point. The peaks are colored `light`, fading to `dark` at the
  bottom: this looks like a sunlit slope.
* Line 53: triangle 2: the left strip, all dark.
* Line 54: triangle 3: the right strip, starting a bit lighter (80% of `light`) at the right peak.
* Together they fill the whole shape, and the flat facets give the "low-poly" look.

---

## Block F — Birds (lines 58–63)

```cpp
        double birds[][2] = {{180, 120}, {215, 140}, {250, 110}};
        for (auto& b : birds) {
            cv.draw_line_aa(b[0] - 10, b[1] - 4, b[0], b[1], 2.0, hex_color(0x1A0033));
            cv.draw_line_aa(b[0], b[1], b[0] + 10, b[1] - 4, 2.0, hex_color(0x1A0033));
        }
```

* Line 59: a list of 3 bird positions. `double birds[][2]` = an array of pairs; `b[0]` = x, `b[1]` = y.
* Line 60: for each bird.
* Lines 61–62: two smooth lines, 2 pixels thick, meeting at the bird's position: the left wing goes up-left, the right
  wing up-right. Together they form a small "V" (a classic bird silhouette).

---

## Block G — Save and end (lines 64–67)

Save the poster, end the block, `return 0`, end of `main`.

---

## Check your understanding

1. What color is the RGB triangle exactly at its top corner? *(Pure red.)*
2. Why do the peaks start at x = −40 instead of 0? *(So the mountains reach past the left and right edges.)*
3. Why is the far range drawn first? *(Nearer things must be drawn on top: back to front.)*
4. Why is `ys` computed as `base_y − random × height`? *(y goes down, so subtracting moves the peak up.)*
