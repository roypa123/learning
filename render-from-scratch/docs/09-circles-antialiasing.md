# Chapter 9 — Circles, anti-aliasing and transparency

[← Drawing basics](08-drawing-basics.md) · [Contents](README.md) · [Next: Triangles & gradients →](10-triangles-gradients.md)

---

## Goal

* Draw circles, filled and outlined.
* Understand **aliasing** (the "jaggies") and fix it with **anti-aliasing**.
* Draw see-through shapes with **alpha blending**.
* Make things glow with **additive** light.

The ideas in this chapter come back in every later chapter. Anti-aliasing in particular becomes
the reason a ray tracer shoots *many* rays per pixel.

---

## 1. Circles

### 1.1 Filled: the inside test

A point `(x, y)` is inside a circle with center `(cx, cy)` and radius `r` if its distance to the center
is at most `r`. By Pythagoras:

```
(x − cx)² + (y − cy)² ≤ r²
```

(We compare *squared* distances to avoid a slow square root.) So a filled circle is: visit every pixel
in the square around the circle and keep the ones that pass the test.

### 1.2 Outline: the midpoint circle algorithm

For an outline we can use a Bresenham-style trick. A circle has **8-fold symmetry**: if (x, y) is on
it, so are (y, x), (−x, y), (x, −y), and so on:

```
          (−y, x)  ·  ·  (y, x)
      (−x, y) ·          · (x, y)
             ·     ●      ·
      (−x,−y) ·          · (x,−y)
          (−y,−x)  ·  ·  (y,−x)
```

So we only compute one eighth (from the top going clockwise until x = y) and mirror each point 8
times. An integer error term decides when x must step inwards. See `draw_circle` in `canvas.h`.

---

## 2. Aliasing: why edges look jagged

A pixel is a square. A circle's edge passes *through* some squares, partly covering them. With the
hard inside test, a pixel is either fully colored (its **center** is inside) or not at all:

```
 true shape edge            hard test result
   ░░░░▒▒▓▓██              ░░░░░░████
   ░░▒▒▓▓████              ░░░░██████      <- staircase ("jaggies")
   ▒▓▓███████              ░░████████
```

This is called **aliasing**. The name comes from signal processing. We're *sampling* a continuous shape at
one point per pixel, and detail finer than a pixel gets misrepresented. It's visible as:

* staircases on edges,
* thin lines that break up or disappear,
* shimmering and crawling edges in animations,
* strange "moiré" patterns on fine stripes.

### 2.1 The fix: coverage

The honest color of an edge pixel is **the fraction of it covered by the shape**. If a white
circle covers 30% of a black pixel, the pixel should be 30% white. That's **anti-aliasing**.

There are two classic ways to estimate coverage.

**1. Supersampling:** test many points inside the pixel and average.

```
┌───────────┐
│ ·   ·   · │   test 3x3 = 9 points; if 4 are inside -> coverage 4/9
│ ·   ·   · │
│ ·   ·   · │
└───────────┘
```

This works for *any* shape, and it's exactly what ray tracers do (each point is a ray). We use it
for triangles in chapter 10.

**2. Analytic coverage from distance:** for smooth shapes we know the signed distance `d` from the
pixel center to the edge. The edge crosses the pixel roughly when `|d| < 0.5`, so we fade from 1 to 0
over one pixel:

```
coverage = clamp(r − d + 0.5, 0, 1)        (d = distance from pixel center to circle center)

   d:  r−1  r−0.5   r    r+0.5  r+1
cov:   1     1     0.5    0      0
```

It's one line of code, very cheap, and it looks excellent. That's `fill_circle_aa`. The same idea with
the distance to a line segment gives smooth, thick lines (`draw_line_aa`).

### 2.2 Distance to a line segment

For `draw_line_aa` we need the distance from a point P to the segment AB. Project P onto the line,
clamp the projection to the segment, and measure:

```
t = clamp( dot(P − A, B − A) / |B − A|², 0, 1 )   <- where along AB is the closest point (0..1)
closest = A + t (B − A)
d = |P − closest|
```

```
        P
        ·
        ¦ d
 A ●────·──────────● B
        closest (t ≈ 0.3)
```

(`dot` is the dot product, which we'll study properly in chapter 12. Here it's just
`(px·vx + py·vy)`.)

---

## 3. Transparency: alpha blending

Many shapes should be see-through: glass, bubbles, shadows, overlays. We give a color an **alpha**
value: 1 = opaque, 0 = invisible. Drawing color `C` with alpha `α` over an existing pixel `D` gives:

```
result = D · (1 − α) + C · α          ("the over operator")
```

Alpha 0.3 means "30% of the new color, 70% of what was there".

Coverage and alpha combine naturally: an edge pixel with 40% coverage of a shape with 55% alpha gets
`α = 0.4 × 0.55 = 0.22`. That's the line `blend_pixel(x, y, c, coverage * alpha)`.

> **Important:** blending must happen on **linear** colors (chapter 4). Our `Image` stores linear
> values, so we get this for free.

> **Order matters:** "over" isn't commutative. Drawing A then B gives a different result than B then
> A. Painters draw back to front, and so do we.

---

## 4. Glow: adding light

Painting *replaces* color. Light *adds* to it. A glowing star or lamp doesn't cover what's behind it, it
brightens it. The `glow` function adds a color whose strength falls off from the center:

```
k = (1 − d/R)²     for d < R           (smooth falloff to 0 at the rim)
pixel += color · k
```

Because it adds, overlapping glows get brighter, just like real light. Values can go above 1, which
our clamp tone mapping cuts off at white (chapter 34 does better).

---

## 5. The program

**File: `chapters/ch09_circles_antialiasing.cpp`**

```cpp
// ch09_circles_antialiasing.cpp
// ------------------------------------------------------------
// Chapter 9: Circles, anti-aliasing and transparency.
//   images/ch09_jaggies.png     - hard circle vs anti-aliased circle, zoomed 8x
//   images/ch09_bubbles.png     - transparent overlapping bubbles
//   images/ch09_night_sky.png   - glows: a moon and stars
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Enlarge an image by an integer factor, keeping pixels square (to SEE them).
static Image zoom(const Image& src, int factor) {
    Image out(src.width * factor, src.height * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x / factor, y / factor);
    return out;
}

int main() {
    // ---------- 1. Jaggies vs smooth edges ---------------------------------
    {
        Image hard(40, 40, Color(1, 1, 1)), smooth(40, 40, Color(1, 1, 1));
        Canvas a(hard), b(smooth);
        a.fill_circle(20, 20, 14, hex_color(0x1D3557));
        b.fill_circle_aa(20.0, 20.0, 14.3, hex_color(0x1D3557));
        a.draw_line(2, 36, 37, 30, hex_color(0xE63946));
        b.draw_line_aa(2.5, 36.5, 37.5, 30.5, 1.0, hex_color(0xE63946));
        Image both = post::side_by_side(zoom(hard, 8), zoom(smooth, 8), 16);
        save_image("images/ch09_jaggies.png", both);
    }

    // ---------- 2. Transparency (alpha blending) ---------------------------
    {
        Image img(480, 320);
        Canvas cv(img);
        cv.vertical_gradient(0, 320, hex_color(0x48CAE4), hex_color(0x023E8A));
        struct Bubble { double x, y, r; uint32_t color; };
        Bubble bubbles[] = { {150, 150, 90, 0xFF006E}, {260, 130, 80, 0xFFBE0B},
                             {220, 220, 85, 0x8338EC}, {360, 200, 60, 0x3A86FF},
                             {90, 260, 40, 0xFB5607} };
        for (const Bubble& b : bubbles) {
            cv.fill_circle_aa(b.x, b.y, b.r, hex_color(b.color), 0.55);          // see-through body
            cv.fill_circle_aa(b.x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.18,   // highlight
                              Color(1, 1, 1), 0.7);
        }
        save_image("images/ch09_bubbles.png", img);
    }

    // ---------- 3. Glow: adding light instead of painting over -------------
    {
        Image img(640, 360);
        Canvas cv(img);
        cv.vertical_gradient(0, 360, hex_color(0x03045E), hex_color(0x000814));
        Pcg32 rng(7);
        for (int i = 0; i < 350; i++) {                      // stars
            double x = rng.next_double() * 640, y = rng.next_double() * 300;
            double brightness = 0.2 + 2.0 * std::pow(rng.next_double(), 6);
            cv.glow(x, y, 2.0 + 4.0 * brightness, Color(0.8, 0.85, 1.0) * brightness);
        }
        cv.glow(480, 90, 160, hex_color(0x4A6FA5) * 0.6);      // halo around the moon
        cv.fill_circle_aa(480, 90, 38, hex_color(0xF1F1E6));   // the moon
        cv.fill_circle_aa(492, 80, 8, hex_color(0xD9D9C8));    // craters
        cv.fill_circle_aa(470, 104, 11, hex_color(0xD9D9C8));
        // dark hills in front (circles cut by the bottom edge)
        cv.fill_circle_aa(150, 560, 280, hex_color(0x020617));
        cv.fill_circle_aa(520, 600, 320, hex_color(0x030712));
        save_image("images/ch09_night_sky.png", img);
    }
    return 0;
}
```

Three images:

1. **Jaggies**: the same circle and line drawn with the hard test (left) and anti-aliased (right),
   both enlarged 8× with square pixels so you can see each one. `post::side_by_side` puts two images
   next to each other; we'll meet the rest of `post.h` in chapter 35.
2. **Bubbles**: five translucent circles over a blue gradient, each with a small white highlight.
3. **Night sky**: 350 stars as glows with random brightness (most dim, a few bright,
   thanks to `pow(random, 6)`), a moon with a halo and craters, and dark hills made from two huge
   circles mostly below the frame.

```bat
run ch09_circles_antialiasing
```

---

## 6. What you should see

![Jaggies](../images/ch09_jaggies.png)

> **Image description:** Two large square panels side by side on a white background, each showing a
> dark blue circle and a red diagonal line, magnified so every pixel is an 8×8 block.
> **Left:** the circle's edge is a hard staircase of full-blue and full-white blocks; the red line
> is a staircase of single blocks. **Right:** the edge blocks have in-between shades (light blue,
> pale red) that blend into the background. Seen from a distance, the right circle looks perfectly
> round and the right line looks smooth.

![Bubbles](../images/ch09_bubbles.png)

> **Image description:** A light-to-dark blue vertical gradient with five overlapping translucent
> circles: hot pink, amber-yellow, purple, bright blue and orange. Where circles overlap, their
> colors mix (pink over purple looks magenta-violet). Each bubble has a small soft white highlight at
> its upper left, making it look glossy. All edges are smooth.

![Night sky](../images/ch09_night_sky.png)

> **Image description:** A deep blue night sky getting darker toward the bottom, full of stars:
> hundreds of faint pinpricks and a few bright stars with soft glows. At the upper right, a pale
> cream moon with two grey craters sits in a large soft blue halo. Along the bottom, two black rolling
> hills form a silhouette.

---

## Try it yourself

1. Change the moon's halo radius and color. Try an orange harvest moon.
2. Draw a **ring** (donut): coverage = `min(coverage_outer, 1 − coverage_inner)`.
3. Make a **soft shadow** under each bubble: a dark circle with alpha 0.3, offset down-right, drawn
   *before* the bubble.
4. Draw 100 anti-aliased lines with random angles and thickness 0.5 to 4. Compare thin lines with
   `draw_line` (Bresenham).
5. Try alpha blending in **sRGB** instead of linear: convert both colors to sRGB, blend, convert back.
   Compare the overlaps. The linear version looks more like real light passing through tinted glass.

## Common problems

| Symptom | Cause |
|---------|-------|
| Circle looks slightly off-center | Using pixel corners (x) instead of pixel centers (x + 0.5) |
| Dark fringes around smooth shapes | Blending in sRGB space, or coverage computed twice |
| Glow looks cut off with a hard edge | Loop bounds smaller than the glow radius |

---

## Summary

* Circles: inside test `dx² + dy² ≤ r²`; the midpoint algorithm uses 8-fold symmetry.
* **Aliasing** comes from sampling a continuous shape at one point per pixel.
* **Anti-aliasing** = coverage. Estimate it by supersampling (general) or from distance (cheap and
  smooth).
* **Alpha blending**: `result = D(1−α) + Cα`, in linear space, back to front.
* **Light adds**: glows add to what's there.

Next: [Chapter 10 — Triangles and gradients →](10-triangles-gradients.md)
