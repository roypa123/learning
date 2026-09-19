# Line by line: `ch09_circles_antialiasing.cpp`

[← Line-by-line index](README.md) · [Chapter 9 (the theory)](../09-circles-antialiasing.md) · [canvas.h explained](canvas.md)

**What the whole program does, in one sentence:** it shows the difference between jagged and smooth edges, draws
see-through bubbles, and paints a night sky with glowing stars and a moon.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. `zoom` helper | 12–19 | enlarge an image so you can see its pixels |
| C. Picture 1: jaggies vs smooth | 21–32 | the same shapes, hard vs anti-aliased |
| D. Picture 2: bubbles | 34–49 | transparent circles |
| E. Picture 3: night sky | 51–70 | glowing stars, moon, hills |
| F. End | 71–72 | |

---

## Block A — Comments, includes (lines 1–10)

The usual: comments, `<cmath>` (for `std::pow`), our library, `using namespace pixel`.

---

## Block B — `zoom` helper (lines 12–19)

```cpp
static Image zoom(const Image& src, int factor) {
    Image out(src.width * factor, src.height * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x / factor, y / factor);
    return out;
}
```

* Line 13: a helper that returns a **bigger copy** of an image. `static` = only used in this file.
* Line 14: the new image is `factor` times bigger in both directions.
* Lines 15–16: every pixel of the big image.
* Line 17: take the color from the small image at `(x / factor, y / factor)`. Whole-number division: with factor 8,
  big pixels 0–7 all read small pixel 0, 8–15 read pixel 1, and so on. So each small pixel becomes an 8 × 8 block.
* Line 18: return the new image.

---

## Block C — Picture 1: jaggies vs smooth (lines 21–32)

```cpp
int main() {
    {
        Image hard(40, 40, Color(1, 1, 1)), smooth(40, 40, Color(1, 1, 1));
        Canvas a(hard), b(smooth);
```

* Line 24: two tiny white images (40 × 40), created in one line.
* Line 25: two canvases, `a` for the hard one, `b` for the smooth one.

```cpp
        a.fill_circle(20, 20, 14, hex_color(0x1D3557));
        b.fill_circle_aa(20.0, 20.0, 14.3, hex_color(0x1D3557));
```

* Line 26: a dark blue circle with **hard** edges (each pixel in or out).
* Line 27: the same circle, **anti-aliased** (edge pixels get partial coverage). The radius 14.3 makes it about the
  same size as the hard one.

```cpp
        a.draw_line(2, 36, 37, 30, hex_color(0xE63946));
        b.draw_line_aa(2.5, 36.5, 37.5, 30.5, 1.0, hex_color(0xE63946));
```

* Line 28: a red line with Bresenham (hard staircase).
* Line 29: the same line, smooth, 1 pixel thick. The `.5` values put the ends at pixel centers, to match line 28.

```cpp
        Image both = post::side_by_side(zoom(hard, 8), zoom(smooth, 8), 16);
        save_image("images/ch09_jaggies.png", both);
    }
```

* Line 30: enlarge both 8× (so each pixel is an 8 × 8 block you can see), then put them **side by side** with a
  16-pixel white gap. `post::side_by_side` comes from `post.h` (the `post` namespace).
* Line 31: save.

---

## Block D — Picture 2: bubbles (lines 34–49)

```cpp
        Image img(480, 320);
        Canvas cv(img);
        cv.vertical_gradient(0, 320, hex_color(0x48CAE4), hex_color(0x023E8A));
```

* Lines 36–37: a 480 × 320 image and canvas.
* Line 38: background: a vertical gradient from light blue (top) to deep blue (bottom), rows 0 to 320.

```cpp
        struct Bubble { double x, y, r; uint32_t color; };
        Bubble bubbles[] = { {150, 150, 90, 0xFF006E}, {260, 130, 80, 0xFFBE0B},
                             {220, 220, 85, 0x8338EC}, {360, 200, 60, 0x3A86FF},
                             {90, 260, 40, 0xFB5607} };
```

* Line 39: a small type for one bubble: center (x, y), radius r, and a hex color.
* Lines 40–42: five bubbles: pink, yellow, purple, blue, orange. `[]` lets the compiler count them.

```cpp
        for (const Bubble& b : bubbles) {
            cv.fill_circle_aa(b.x, b.y, b.r, hex_color(b.color), 0.55);          // see-through body
            cv.fill_circle_aa(b.x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.18,   // highlight
                              Color(1, 1, 1), 0.7);
        }
```

* Line 43: for each bubble `b`, in order (later bubbles are drawn on top).
* Line 44: the bubble's body: a smooth circle with **alpha 0.55**, so what's behind shows through (the gradient and
  earlier bubbles).
* Lines 45–46: a small white **highlight**: a circle 18% of the bubble's size, moved up-left by 35% of the radius,
  alpha 0.7. It makes the bubble look shiny, as if lit from the upper left.

```cpp
        save_image("images/ch09_bubbles.png", img);
```

Line 48: save.

---

## Block E — Picture 3: night sky (lines 51–70)

```cpp
        Image img(640, 360);
        Canvas cv(img);
        cv.vertical_gradient(0, 360, hex_color(0x03045E), hex_color(0x000814));
        Pcg32 rng(7);
```

* Lines 53–54: a 640 × 360 image and canvas.
* Line 55: a night-sky gradient: dark blue at the top, almost black at the bottom.
* Line 56: a random generator with seed 7 (always the same stars).

```cpp
        for (int i = 0; i < 350; i++) {                      // stars
            double x = rng.next_double() * 640, y = rng.next_double() * 300;
            double brightness = 0.2 + 2.0 * std::pow(rng.next_double(), 6);
            cv.glow(x, y, 2.0 + 4.0 * brightness, Color(0.8, 0.85, 1.0) * brightness);
        }
```

* Line 57: 350 stars.
* Line 58: a random position (x anywhere across; y in the top 300 rows).
* Line 59: a random brightness. `pow(random, 6)` is almost always tiny (e.g. 0.5⁶ = 0.016) and only sometimes
  large, so **most stars are dim and a few are bright**, like a real sky. The brightness range is 0.2–2.2.
* Line 60: draw the star as a **glow** (added light). Brighter stars are bigger (radius 2–11) and brighter. The
  color is slightly blue-white.

```cpp
        cv.glow(480, 90, 160, hex_color(0x4A6FA5) * 0.6);      // halo around the moon
        cv.fill_circle_aa(480, 90, 38, hex_color(0xF1F1E6));   // the moon
        cv.fill_circle_aa(492, 80, 8, hex_color(0xD9D9C8));    // craters
        cv.fill_circle_aa(470, 104, 11, hex_color(0xD9D9C8));
```

* Line 62: a big soft blue glow (radius 160) around where the moon will be: the halo.
* Line 63: the moon itself: a pale cream disc of radius 38.
* Lines 64–65: two slightly darker circles on it: craters.

```cpp
        cv.fill_circle_aa(150, 560, 280, hex_color(0x020617));
        cv.fill_circle_aa(520, 600, 320, hex_color(0x030712));
        save_image("images/ch09_night_sky.png", img);
    }
```

* Lines 67–68: two **huge** dark circles whose centers are **below** the image (y = 560 and 600, but the image is only
  360 tall). Only their tops show, which look like rolling hills. Clipping in the canvas makes this safe.
* Line 69: save.

---

## Block F — End (lines 71–72)

`return 0;` and `}`.

---

## Check your understanding

1. What does `zoom(img, 8)` do to a 40 × 40 image? *(Makes it 320 × 320, each pixel an 8 × 8 block.)*
2. Why can you see through the bubbles? *(They are drawn with alpha 0.55.)*
3. Why are most stars dim? *(`pow(random, 6)` is usually a very small number.)*
4. How can a circle centered outside the image be drawn? *(The canvas clips: only pixels inside the image are touched.)*
