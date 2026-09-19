# Chapter 10 — Triangles and gradients

[← Circles & anti-aliasing](09-circles-antialiasing.md) · [Contents](README.md) · [Next: Procedural noise →](11-procedural-noise.md)

---

## Goal

Triangles are the most important shape in computer graphics. **Every** 3D model in games and
films is made of them. In this chapter you'll:

* test whether a point is inside a triangle with **edge functions**,
* compute **barycentric coordinates** and use them to blend colors smoothly across a triangle,
* anti-alias triangles with **supersampling**,
* fill arbitrary polygons with a **scanline** algorithm,
* make a low-poly mountain sunset poster.

---

## 1. Which side of a line?

Take a line through points A and B and a third point P. Is P on the left or the right of the
line (looking from A towards B)? The **edge function** answers that:

```
E(A, B, P) = (Bx − Ax)(Py − Ay) − (By − Ay)(Px − Ax)
```

* `E > 0`: P is on one side,
* `E < 0`: P is on the other side,
* `E = 0`: P is exactly on the line.

```
            P (E > 0)
            ·
   A ●──────────────● B
            ·
            Q (E < 0)
```

This is the 2D **cross product** of the vectors (B − A) and (P − A). It has a second meaning:
**|E| is twice the area of triangle ABP.** Keep that in mind.

---

## 2. Inside a triangle

A point is inside triangle ABC if it's on the **same side** of all three edges AB, BC and CA:

```
         A
        ╱ ╲
       ╱ P ╲         P is inside: E(B,C,P), E(C,A,P), E(A,B,P) all have the same sign
      ╱  ·  ╲        as the triangle's own area E(A,B,C)
     B───────C
```

To handle both clockwise and counter-clockwise triangles, divide each edge value by the area of the
whole triangle. Then *inside* means **all three results are ≥ 0**, whatever the winding.

---

## 3. Barycentric coordinates

Those three divided values have a beautiful meaning:

```
w0 = E(B, C, P) / E(A, B, C)     = area(PBC) / area(ABC)     "how much A"
w1 = E(C, A, P) / E(A, B, C)     = area(PCA) / area(ABC)     "how much B"
w2 = E(A, B, P) / E(A, B, C)     = area(PAB) / area(ABC)     "how much C"
```

* They always add up to 1.
* At corner A: `(w0, w1, w2) = (1, 0, 0)`. At B: `(0, 1, 0)`. At C: `(0, 0, 1)`.
* At the center of the triangle: `(1/3, 1/3, 1/3)`.
* Inside the triangle, all three are between 0 and 1.

They are called **barycentric coordinates** ("barycenter" = center of mass). Imagine weights `w0,
w1, w2` hanging at the corners: P is their balance point.

```
          A (1,0,0)
          ╱╲
         ╱  ╲
        ╱ ·P ╲         P = w0·A + w1·B + w2·C
       ╱      ╲
 (0,1,0)B──────C (0,0,1)
```

### 3.1 Interpolation

Anything defined at the corners can be **blended** across the triangle with the same weights:

```
color(P) = w0 · colorA + w1 · colorB + w2 · colorC
```

Colors, texture coordinates, normals, depth: every GPU does exactly this, billions of times a
second. We'll use the same idea in 3D for smooth-shaded triangles (chapter 25).

---

## 4. Rasterizing a triangle

```
for each pixel in the triangle's bounding box (clamped to the image):
    compute w0, w1, w2 at the pixel center
    if all ≥ 0: pixel = w0·c0 + w1·c1 + w2·c2
```

### 4.1 Anti-aliasing by supersampling

Triangle edges are straight lines at any angle, so we use **supersampling**: test `ss × ss` points
inside each pixel (16 points for `ss = 4`), average the colors of the samples that are inside, and use
the inside fraction as the coverage:

```
┌─────────┐
│ · · · · │   16 sample points; 11 inside the triangle
│ · · · · │   -> color = average of the 11 interpolated colors
│ · · · ▲ │   -> coverage = 11/16, used as alpha when blending
│ · · ╱ · │
└─────────┘
```

> **About seams:** when two triangles share an edge, each one blends its partial coverage over the
> background *separately*, so the shared edge pixels end up slightly see-through, and a faint line of
> background can show between them. This artifact is known as **conflation**. Look closely at the
> low-poly poster and you may spot it. Real renderers fix it by accumulating coverage for all shapes
> per sample (like a ray tracer does naturally!).

---

## 5. Any polygon: the scanline algorithm

For shapes with many corners (stars, letters, country outlines) we use a different approach, which
works for any **polygon**:

1. For each row of pixels (a **scanline**), find where it crosses the polygon's edges.
2. Sort those crossing x-positions.
3. Fill between the 1st and 2nd crossing, between the 3rd and 4th, and so on.

```
         ╱╲
        ╱  ╲        scanline y:  crossings at x = 3, 7, 11, 15
   ────●────●────●────●────      fill 3..7 and 11..15
      ╱      ╲  ╱      ╲
```

This "even-odd rule" also handles holes correctly. `fill_polygon` in `canvas.h` implements it with
horizontal coverage for smooth left/right edges and several sub-scanlines per pixel for smooth
top/bottom edges.

---

## 6. Gradients

A **vertical gradient** is `lerp(top, bottom, t)` per row, with `t` running from 0 to 1. A
**radial gradient** uses `t = distance / radius`. Both are in `canvas.h`. Since our colors are linear,
gradients between very different colors look natural. Try the same thing in sRGB and you'll often see
a muddy, dark middle.

---

## 7. The program

**File: `chapters/ch10_triangles.cpp`**

```cpp
// ch10_triangles.cpp
// ------------------------------------------------------------
// Chapter 10: Triangles, barycentric coordinates and a poster.
//   images/ch10_rgb_triangle.png   - the famous red/green/blue triangle
//   images/ch10_lowpoly.png        - a low-poly mountain sunset poster
// ------------------------------------------------------------
#include <cmath>
#include <vector>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Color interpolation inside one triangle -----------------
    {
        Image img(400, 360, hex_color(0x111111));
        Canvas cv(img);
        cv.fill_triangle(200, 20, 380, 330, 20, 330,
                         Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1));
        save_image("images/ch10_rgb_triangle.png", img);
    }

    // ---------- 2. Low-poly landscape --------------------------------------
    {
        const int W = 800, H = 450;
        Image img(W, H);
        Canvas cv(img);

        // Sky: vertical gradient, then a glowing sun.
        cv.vertical_gradient(0, H, hex_color(0x2B1055), hex_color(0xF7A072));
        cv.glow(560, 250, 260, hex_color(0xFF9E00) * 0.9);
        cv.fill_circle_aa(560, 250, 60, hex_color(0xFFE8A3));

        // Each mountain range is a jagged line of peaks, filled as triangles
        // down to the bottom of the image. Farther ranges are lighter (haze).
        struct Range { double base_y, height; uint32_t light, dark; uint64_t seed; };
        Range ranges[] = {
            {300, 140, 0x9D4EDD, 0x7B2CBF, 1},
            {340, 130, 0x5A189A, 0x3C096C, 2},
            {390, 120, 0x240046, 0x10002B, 3},
        };
        for (const Range& r : ranges) {
            Pcg32 rng(r.seed);
            std::vector<double> xs, ys;
            for (double x = -40; x <= W + 40; x += 40 + rng.next_double() * 60) {
                xs.push_back(x);
                ys.push_back(r.base_y - rng.next_double() * r.height);
            }
            Color light = hex_color(r.light), dark = hex_color(r.dark);
            for (size_t i = 0; i + 1 < xs.size(); i++) {
                double mx = 0.5 * (xs[i] + xs[i + 1]);
                // Two triangles per segment: a lit face and a shadow face.
                cv.fill_triangle(xs[i], ys[i], xs[i + 1], ys[i + 1], mx, H, light, dark, dark);
                cv.fill_triangle(xs[i], ys[i], mx, H, xs[i], H, dark, dark, dark);
                cv.fill_triangle(xs[i + 1], ys[i + 1], xs[i + 1], H, mx, H, light * 0.8, dark, dark);
            }
        }

        // A few birds: two short thick lines each.
        double birds[][2] = {{180, 120}, {215, 140}, {250, 110}};
        for (auto& b : birds) {
            cv.draw_line_aa(b[0] - 10, b[1] - 4, b[0], b[1], 2.0, hex_color(0x1A0033));
            cv.draw_line_aa(b[0], b[1], b[0] + 10, b[1] - 4, 2.0, hex_color(0x1A0033));
        }
        save_image("images/ch10_lowpoly.png", img);
    }
    return 0;
}
```

The poster is built in layers, back to front:

1. **Sky**: a vertical gradient from deep purple to peach.
2. **Sun**: a big additive glow plus a solid anti-aliased disc.
3. **Three mountain ranges**, each a random sequence of peaks (`Pcg32` with a different seed per range). Each
   segment between two peaks is filled with triangles down to the bottom of the image: one face uses
   a *light* color at the peaks fading to *dark*, and the other faces are dark. Farther ranges use
   lighter colors, a painter's trick called **atmospheric perspective**.
4. **Birds**: two short thick anti-aliased lines each.

```bat
run ch10_triangles
```

---

## 8. What you should see

![RGB triangle](../images/ch10_rgb_triangle.png)

> **Image description:** On a near-black background, a large upright triangle. Its top corner is
> pure **red**, bottom-right **green**, bottom-left **blue**. The colors blend smoothly inside;
> near the middle they mix to a soft grey-white, and along each edge you see the mix of the two
> corners (yellow-ish between red and green, magenta between red and blue, cyan between green and
> blue). The edges are smooth, not jagged.

![Low-poly poster](../images/ch10_lowpoly.png)

> **Image description:** A stylized sunset poster, 800×450. A gradient sky from deep violet at the
> top to warm peach near the horizon, with a large glowing pale-yellow sun on the right. Three layers
> of jagged, faceted mountains overlap in front: the farthest is light lavender-purple, the middle one
> deep purple, the nearest nearly black-violet. The mountains are made of flat triangular facets
> with a light-to-dark shading that suggests sunlit and shadowed slopes. Three small dark "V"-shaped
> birds fly in the upper left.

---

## Try it yourself

1. Change the triangle's corners to `hex_color` values of your choice. What does a
   black-white-black triangle look like?
2. Draw a **star** with `fill_polygon`: 10 points alternating between radius 100 and 40.
3. Replace the mountains' random peaks with a sine wave: `y = base − height · |sin(x / 50)|`.
4. Set `ss = 1` in `fill_triangle` and compare the edges. Then try `ss = 8`: is it better, and how
   much slower?
5. Add a reflection of the mountains in a lake at the bottom (flip the image vertically and darken it).
   Chapter 11 does this, but try it yourself first.

## Common problems

| Symptom | Cause |
|---------|-------|
| Triangle doesn't appear at all | Degenerate triangle (all points on a line) or completely off-screen |
| Only works for one winding order | Not dividing by the signed area |
| Visible thin lines between adjacent triangles | Conflation (see section 4.1); overlap triangles slightly or draw a solid base first |

---

## Summary

* The **edge function** tells which side of a line a point is on, and it's twice a triangle's area.
* Inside a triangle = on the inner side of all three edges.
* **Barycentric coordinates** (area ratios) interpolate any value across a triangle.
* Supersampling gives anti-aliased triangles; the scanline algorithm fills any polygon.

Next: [Chapter 11 — Procedural noise →](11-procedural-noise.md). Let's paint with randomness.
