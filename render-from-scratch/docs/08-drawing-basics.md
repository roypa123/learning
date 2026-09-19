# Chapter 8 — Drawing: pixels, rectangles, lines

[← PNG part 2](07-png-part2.md) · [Contents](README.md) · [Next: Circles & anti-aliasing →](09-circles-antialiasing.md)

> 📖 **Line by line:** [canvas explained line by line](line-by-line/canvas.md) · [ch08_drawing explained line by line](line-by-line/ch08_drawing.md)

---

## Goal

Start **2D graphics**, the kind of drawing used by paint programs, fonts and user interfaces. We'll
build a `Canvas` that draws onto an `Image`, and learn:

* safe pixel writing (clipping),
* filled and outlined rectangles,
* **Bresenham's line algorithm**, a classic that draws perfect lines using only integer math,

and make three pictures: pixel art, a starburst of lines, and a painting in the style of Piet
Mondrian.

---

## 1. Coordinates on the screen

In 2D image coordinates, **x goes right and y goes down**, starting at the top-left pixel `(0, 0)`:

```
(0,0) ──────────────────▶ x
  │   ┌──┬──┬──┬──┐
  │   │  │  │  │  │   pixel (x=2, y=1) is the 3rd column, 2nd row
  │   ├──┼──┼──┼──┤
  │   │  │  │██│  │
  ▼   └──┴──┴──┴──┘
  y
```

A pixel is a small **square**. Pixel `(2, 1)` covers the area from x = 2.0 to 3.0 and from y = 1.0 to
2.0; its **center** is at (2.5, 1.5). This will matter a lot for anti-aliasing (next chapter).

---

## 2. The Canvas

A `Canvas` holds a reference to an `Image` and adds drawing functions:

```cpp
Image img(400, 300, hex_color(0xFFFFFF));   // white background
Canvas cv(img);                              // draw onto img
cv.fill_rect(10, 10, 100, 50, hex_color(0xFF0000));
save_image("images/test.png", img);
```

Why a separate class instead of adding functions to `Image`? *Separation of concerns*: `Image` stores
pixels, `Canvas` knows how to draw 2D shapes. Later the ray tracer fills images a completely different
way and never needs the drawing code.

### 2.1 Clipping: never write outside the image

If a shape is partly off-screen, writing its pixels would go outside the vector and crash (or,
worse, silently corrupt memory). Every write goes through `set_pixel`, which checks first:

```cpp
void set_pixel(int x, int y, const Color& c) {
    if (img.in_bounds(x, y)) img.at(x, y) = c;
}
```

Checking every pixel costs a little speed, but it keeps drawing safe and simple. (Later functions
also compute a bounding box and clamp it to the image, so they don't even visit off-screen pixels.)

---

## 3. Rectangles

A filled rectangle is two nested loops:

```cpp
void fill_rect(int x0, int y0, int w, int h, const Color& c, double alpha = 1.0) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            blend_pixel(x, y, c, alpha);
}
```

(`blend_pixel` with `alpha = 1` just sets the pixel; chapter 9 explains alpha.)

An outlined rectangle is four lines. And for lines we need an algorithm.

---

## 4. Lines: Bresenham's algorithm

### 4.1 The problem

Draw a line from `(x0, y0)` to `(x1, y1)` on a grid of pixels. The line is mathematically thin and
continuous; the grid is chunky. Which pixels should light up?

```
  y
  0  ██
  1    ████
  2        ████          a line from (0,0) to (9,4):
  3            ████      one pixel per column, stepping down
  4                ██    when the ideal line gets closer to the next row
     0 1 2 3 4 5 6 7 8 9  x
```

For a shallow line (wider than tall): walk along x one pixel at a time, and decide at each step
whether y should also step.

### 4.2 The naive way

```cpp
for (int x = x0; x <= x1; x++) {
    double t = double(x - x0) / (x1 - x0);
    int y = (int)std::round(y0 + t * (y1 - y0));
    set_pixel(x, y, c);
}
```

This works for shallow lines, but it leaves gaps in steep lines (only one pixel per column when we
need several), it divides by zero for vertical lines, and it uses floating point. In 1962 Jack
Bresenham, working on pen plotters at IBM, found a better way.

### 4.3 The idea: keep an error counter

Track how far the ideal line is from the pixel we're drawing, as an **integer error term**. Each step
we move one pixel in x and/or y and update the error by adding or subtracting `dx` and `dy`. When the
error passes the halfway point, we step in the other direction too.

The version in our library handles **all eight directions** (steep, shallow, left, right, up, down)
in one loop:

```cpp
void draw_line(int x0, int y0, int x1, int y1, const Color& c) {
    int dx =  std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;   // step direction in x
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;   // note: dy is NEGATIVE
    int err = dx + dy;
    while (true) {
        set_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }   // time to step in x
        if (e2 <= dx) { err += dx; y0 += sy; }   // time to step in y
    }
}
```

### 4.4 Tracing it by hand

Line from (0, 0) to (5, 2): `dx = 5`, `dy = -2`, `err = 3`.

| step | pixel | e2 = 2·err | e2 ≥ dy (−2)? | e2 ≤ dx (5)? | new err |
|------|-------|-----------|---------------|--------------|---------|
| 1 | (0,0) | 6 | yes → x=1, err=1 | no | 1 |
| 2 | (1,0) | 2 | yes → x=2, err=−1 | yes → y=1, err=4 | 4 |
| 3 | (2,1) | 8 | yes → x=3, err=2 | no | 2 |
| 4 | (3,1) | 4 | yes → x=4, err=0 | yes → y=2, err=5 | 5 |
| 5 | (4,2) | 10 | yes → x=5, err=3 | no | 3 |
| 6 | (5,2) | stop | | | |

Pixels: (0,0) (1,0) (2,1) (3,1) (4,2) (5,2). That's a perfect staircase with no gaps, using only
additions and comparisons. Try tracing a steep line yourself.

### 4.5 Why it's still worth knowing

Modern GPUs rasterize differently, but Bresenham-style **incremental error** thinking shows up
everywhere: in circles (next chapter), texture stepping and audio resampling. And it's beautiful.

---

## 5. The library: `canvas.h`

Here's the complete Canvas. This chapter covers the pixel, rectangle and line functions; the circle,
triangle, polygon and gradient functions are explained in chapters 9 and 10.

**File: `include/pixel/canvas.h`**

```cpp
// pixel/canvas.h
// ------------------------------------------------------------
// 2D drawing on an Image: pixels, lines, rectangles, circles,
// triangles, gradients, alpha blending and anti-aliasing.
// Explained in docs/08-drawing-basics.md, docs/09-circles-antialiasing.md,
//              docs/10-triangles-gradients.md
// All colors are LINEAR (use hex_color()/rgb255() to make them).
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <utility>
#include "vec3.h"
#include "image.h"

namespace pixel {

struct Canvas {
    Image& img;
    explicit Canvas(Image& target) : img(target) {}

    int width() const { return img.width; }
    int height() const { return img.height; }

    void clear(const Color& c) { img.fill(c); }

    // ---------- single pixels ----------------------------------------------

    void set_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) = c;
    }

    // Mix a color over the existing pixel. alpha = 0 (invisible) .. 1 (solid)
    void blend_pixel(int x, int y, const Color& c, double alpha) {
        if (!img.in_bounds(x, y) || alpha <= 0.0) return;
        if (alpha >= 1.0) { img.at(x, y) = c; return; }
        Color& dst = img.at(x, y);
        dst = dst * (1.0 - alpha) + c * alpha;
    }

    // Add light to a pixel (for glows and stars).
    void add_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) += c;
    }

    // ---------- rectangles -------------------------------------------------

    void fill_rect(int x0, int y0, int w, int h, const Color& c, double alpha = 1.0) {
        for (int y = y0; y < y0 + h; y++)
            for (int x = x0; x < x0 + w; x++)
                blend_pixel(x, y, c, alpha);
    }

    void draw_rect(int x0, int y0, int w, int h, const Color& c) {
        draw_line(x0, y0, x0 + w - 1, y0, c);
        draw_line(x0, y0 + h - 1, x0 + w - 1, y0 + h - 1, c);
        draw_line(x0, y0, x0, y0 + h - 1, c);
        draw_line(x0 + w - 1, y0, x0 + w - 1, y0 + h - 1, c);
    }

    // ---------- lines ------------------------------------------------------

    // Bresenham's line algorithm: only integer math, no gaps, no anti-aliasing.
    void draw_line(int x0, int y0, int x1, int y1, const Color& c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set_pixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Smooth (anti-aliased) thick line using distance to a line segment.
    void draw_line_aa(double x0, double y0, double x1, double y1, double thickness, const Color& c) {
        double r = thickness * 0.5;
        int minx = (int)std::floor(std::min(x0, x1) - r - 1), maxx = (int)std::ceil(std::max(x0, x1) + r + 1);
        int miny = (int)std::floor(std::min(y0, y1) - r - 1), maxy = (int)std::ceil(std::max(y0, y1) + r + 1);
        minx = std::max(minx, 0); miny = std::max(miny, 0);
        maxx = std::min(maxx, width() - 1); maxy = std::min(maxy, height() - 1);
        double vx = x1 - x0, vy = y1 - y0;
        double len2 = vx * vx + vy * vy;
        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                double px = x + 0.5 - x0, py = y + 0.5 - y0;
                double t = len2 > 0 ? clamp01((px * vx + py * vy) / len2) : 0.0;
                double dx = px - t * vx, dy = py - t * vy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(r - d + 0.5);   // 1 inside, fades over 1 pixel
                blend_pixel(x, y, c, coverage);
            }
        }
    }

    // ---------- circles ----------------------------------------------------

    // Hard-edged filled circle: a pixel is in or out.
    void fill_circle(int cx, int cy, int radius, const Color& c) {
        for (int y = cy - radius; y <= cy + radius; y++)
            for (int x = cx - radius; x <= cx + radius; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set_pixel(x, y, c);
            }
    }

    // Midpoint circle algorithm: outline only, integer math.
    void draw_circle(int cx, int cy, int radius, const Color& c) {
        int x = radius, y = 0, err = 1 - radius;
        while (x >= y) {
            set_pixel(cx + x, cy + y, c); set_pixel(cx + y, cy + x, c);
            set_pixel(cx - y, cy + x, c); set_pixel(cx - x, cy + y, c);
            set_pixel(cx - x, cy - y, c); set_pixel(cx - y, cy - x, c);
            set_pixel(cx + y, cy - x, c); set_pixel(cx + x, cy - y, c);
            y++;
            if (err < 0) err += 2 * y + 1;
            else { x--; err += 2 * (y - x) + 1; }
        }
    }

    // Smooth-edged circle. Edge pixels get partial coverage -> no "staircase".
    void fill_circle_aa(double cx, double cy, double radius, const Color& c, double alpha = 1.0) {
        int minx = std::max(0, (int)std::floor(cx - radius - 1));
        int maxx = std::min(width() - 1, (int)std::ceil(cx + radius + 1));
        int miny = std::max(0, (int)std::floor(cy - radius - 1));
        int maxy = std::min(height() - 1, (int)std::ceil(cy + radius + 1));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(radius - d + 0.5);
                blend_pixel(x, y, c, coverage * alpha);
            }
    }

    // A soft glowing disc: brightness falls off smoothly from the center.
    void glow(double cx, double cy, double radius, const Color& c) {
        int minx = std::max(0, (int)(cx - radius)), maxx = std::min(width() - 1, (int)(cx + radius));
        int miny = std::max(0, (int)(cy - radius)), maxy = std::min(height() - 1, (int)(cy + radius));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy) / radius;
                if (d < 1.0) {
                    double k = (1.0 - d) * (1.0 - d);
                    add_pixel(x, y, c * k);
                }
            }
    }

    // ---------- triangles --------------------------------------------------

    // "Edge function": tells on which side of the line a->b the point p lies.
    // It is also twice the signed area of triangle (a, b, p).
    static double edge(double ax, double ay, double bx, double by, double px, double py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }

    // Filled triangle with a color at each corner, smoothly blended
    // using barycentric coordinates. ss = samples per pixel side (anti-aliasing).
    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c0, const Color& c1, const Color& c2, int ss = 4) {
        double area = edge(x0, y0, x1, y1, x2, y2);
        if (std::fabs(area) < 1e-12) return;   // degenerate (flat) triangle
        int minx = std::max(0, (int)std::floor(std::min({x0, x1, x2})));
        int maxx = std::min(width() - 1, (int)std::ceil(std::max({x0, x1, x2})));
        int miny = std::max(0, (int)std::floor(std::min({y0, y1, y2})));
        int maxy = std::min(height() - 1, (int)std::ceil(std::max({y0, y1, y2})));

        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                int inside = 0;
                Color sum(0, 0, 0);
                for (int sy = 0; sy < ss; sy++) {
                    for (int sx = 0; sx < ss; sx++) {
                        double px = x + (sx + 0.5) / ss;
                        double py = y + (sy + 0.5) / ss;
                        double w0 = edge(x1, y1, x2, y2, px, py) / area;
                        double w1 = edge(x2, y2, x0, y0, px, py) / area;
                        double w2 = edge(x0, y0, x1, y1, px, py) / area;
                        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                            inside++;
                            sum += w0 * c0 + w1 * c1 + w2 * c2;
                        }
                    }
                }
                if (inside > 0) blend_pixel(x, y, sum / inside, (double)inside / (ss * ss));
            }
        }
    }

    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c, int ss = 4) {
        fill_triangle(x0, y0, x1, y1, x2, y2, c, c, c, ss);
    }

    // Filled polygon (any shape, given as a list of points) using the
    // even-odd scanline rule. Points are (x,y) pairs.
    void fill_polygon(const std::vector<std::pair<double, double>>& pts, const Color& c, int ss = 4) {
        if (pts.size() < 3) return;
        double miny = pts[0].second, maxy = pts[0].second;
        for (auto& p : pts) { miny = std::min(miny, p.second); maxy = std::max(maxy, p.second); }
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(height() - 1, (int)std::ceil(maxy));
        std::vector<double> coverage(width());
        for (int y = y0; y <= y1; y++) {
            std::fill(coverage.begin(), coverage.end(), 0.0);
            for (int s = 0; s < ss; s++) {
                double sy = y + (s + 0.5) / ss;
                std::vector<double> xs;
                for (size_t i = 0; i < pts.size(); i++) {
                    auto a = pts[i], b = pts[(i + 1) % pts.size()];
                    if ((a.second <= sy && b.second > sy) || (b.second <= sy && a.second > sy)) {
                        double t = (sy - a.second) / (b.second - a.second);
                        xs.push_back(a.first + t * (b.first - a.first));
                    }
                }
                std::sort(xs.begin(), xs.end());
                for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                    // add horizontal coverage of span [xs[i], xs[i+1]]
                    double xa = xs[i], xb = xs[i + 1];
                    int ia = std::max(0, (int)std::floor(xa)), ib = std::min(width() - 1, (int)std::floor(xb));
                    for (int x = ia; x <= ib; x++) {
                        double left = std::max(xa, (double)x), right = std::min(xb, (double)x + 1);
                        if (right > left) coverage[x] += (right - left) / ss;
                    }
                }
            }
            for (int x = 0; x < width(); x++)
                if (coverage[x] > 0) blend_pixel(x, y, c, std::min(1.0, coverage[x]));
        }
    }

    // ---------- gradients --------------------------------------------------

    // Vertical gradient from 'top' color to 'bottom' color over rows [y0, y1).
    void vertical_gradient(int y0, int y1, const Color& top, const Color& bottom) {
        for (int y = std::max(0, y0); y < std::min(height(), y1); y++) {
            double t = (y1 - y0) > 1 ? (double)(y - y0) / (y1 - y0 - 1) : 0.0;
            Color c = lerp(top, bottom, t);
            for (int x = 0; x < width(); x++) img.at(x, y) = c;
        }
    }

    // Radial gradient: 'inner' at the center fading to 'outer' at 'radius'.
    void radial_gradient(double cx, double cy, double radius, const Color& inner, const Color& outer) {
        for (int y = 0; y < height(); y++)
            for (int x = 0; x < width(); x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double t = clamp01(std::sqrt(dx * dx + dy * dy) / radius);
                img.at(x, y) = lerp(inner, outer, t);
            }
    }
};

} // namespace pixel
```

---

## 6. The chapter program

**File: `chapters/ch08_drawing.cpp`**

```cpp
// ch08_drawing.cpp
// ------------------------------------------------------------
// Chapter 8: Drawing basics - pixels, rectangles and lines.
//   images/ch08_pixels.png      - a zoomed pixel-art heart
//   images/ch08_lines.png       - Bresenham starburst + grid
//   images/ch08_mondrian.png    - a Mondrian-style painting from rectangles
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Pixel art: every pixel set by hand -----------------------
    {
        const char* heart[] = {
            "..XX...XX..",
            ".XXXX.XXXX.",
            "XXXXXXXXXXX",
            "XXXXXXXXXXX",
            ".XXXXXXXXX.",
            "..XXXXXXX..",
            "...XXXXX...",
            "....XXX....",
            ".....X.....",
        };
        const int rows = 9, cols = 11, zoom = 24;
        Image img(cols * zoom, rows * zoom, hex_color(0xFDF6E3));
        Canvas cv(img);
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (heart[r][c] == 'X')
                    cv.fill_rect(c * zoom, r * zoom, zoom, zoom, hex_color(0xE63946));
        // grid lines so you can see individual "pixels"
        for (int c = 0; c <= cols; c++) cv.draw_line(c * zoom, 0, c * zoom, rows * zoom - 1, hex_color(0xCCCCCC));
        for (int r = 0; r <= rows; r++) cv.draw_line(0, r * zoom, cols * zoom - 1, r * zoom, hex_color(0xCCCCCC));
        save_image("images/ch08_pixels.png", img);
    }

    // ---------- 2. Lines in every direction --------------------------------
    {
        Image img(400, 400, hex_color(0x0B132B));
        Canvas cv(img);
        for (int i = 0; i < 400; i += 20) {       // background grid
            cv.draw_line(i, 0, i, 399, hex_color(0x1C2541));
            cv.draw_line(0, i, 399, i, hex_color(0x1C2541));
        }
        const int N = 48;
        for (int k = 0; k < N; k++) {             // starburst
            double a = 2 * pi * k / N;
            int x1 = (int)(200 + 180 * std::cos(a));
            int y1 = (int)(200 + 180 * std::sin(a));
            Color c = srgb_to_linear(hsv(360.0 * k / N, 0.8, 1.0));
            cv.draw_line(200, 200, x1, y1, c);
        }
        cv.draw_rect(20, 20, 360, 360, hex_color(0xFFFFFF));
        save_image("images/ch08_lines.png", img);
    }

    // ---------- 3. A painting made only of rectangles ----------------------
    {
        Image img(420, 420, hex_color(0xF5F1E6));
        Canvas cv(img);
        cv.fill_rect(0, 0, 250, 250, hex_color(0xD62828));      // big red
        cv.fill_rect(330, 0, 90, 140, hex_color(0x003F88));     // blue
        cv.fill_rect(0, 330, 120, 90, hex_color(0xFFD60A));     // yellow
        cv.fill_rect(330, 360, 90, 60, hex_color(0x111111));    // black
        const Color black = hex_color(0x111111);
        const int t = 10;                                       // line thickness
        cv.fill_rect(250, 0, t, 420, black);
        cv.fill_rect(0, 250, 420, t, black);
        cv.fill_rect(320, 0, t, 420, black);
        cv.fill_rect(120, 250, t, 170, black);
        cv.fill_rect(320, 140, 100, t, black);
        cv.fill_rect(320, 350, 100, t, black);
        cv.fill_rect(0, 320, 120, t, black);
        save_image("images/ch08_mondrian.png", img);
    }
    return 0;
}
```

Things to notice:

* **Pixel art**: the heart is stored as text, where `X` means "pixel on". Each "art pixel" is drawn as a
  24×24 square (a zoom factor), and grid lines show the individual cells.
* **Starburst**: 48 lines from the center, each with a hue from the color wheel (`hsv(...)`,
  converted to linear with `srgb_to_linear`).
* **Mondrian**: a composition made only from `fill_rect` calls. It's a reminder that great images
  can be simple.

```bat
run ch08_drawing
```

---

## 7. What you should see

![Pixel heart](../images/ch08_pixels.png)

> **Image description:** A red pixel-art heart, 11 cells wide and 9 cells tall, on a cream
> background, with thin light-grey grid lines separating every cell, like graph paper. The heart
> has the classic shape: two bumps on top, a point at the bottom.

![Lines](../images/ch08_lines.png)

> **Image description:** A dark navy square with a faint grid. From the center, 48 thin straight
> lines radiate outward like a firework, each in a different rainbow color that cycles through the
> whole color wheel. A thin white square frame surrounds everything. If you zoom in, the lines are
> made of single-pixel "staircases" with jagged edges.

![Mondrian](../images/ch08_mondrian.png)

> **Image description:** A Mondrian-style painting: a cream background divided by thick black
> lines; a big red square in the upper left, a blue rectangle top right, a yellow rectangle bottom
> left and a small black block bottom right.

Zoom into the starburst: the diagonal lines look like staircases. That's called **aliasing**, and it's
the problem we fix in the next chapter.

---

## Try it yourself

1. Draw a **grid of squares** in random colors (use `Pcg32 rng(1);` and `rng.next_double()`).
2. Draw a **spiral** of connected line segments: points at angle `a` and radius `r = 5 * a`.
3. Implement `draw_polyline(std::vector<std::pair<int,int>> pts, Color c)` that connects points.
4. Trace Bresenham by hand for (0,0) → (2,5). Check that each row gets exactly one pixel.
5. Make your own Mondrian, or copy a flag (for example Japan's or Sweden's) with rectangles.

## Common problems

| Symptom | Cause |
|---------|-------|
| Crash when shapes touch the border | Writing without `in_bounds` checks |
| Lines have gaps | Using the naive algorithm for steep lines |
| Rectangle is one pixel too big | Loop uses `<=` instead of `<` (width `w` covers `x0 .. x0+w-1`) |

---

## Summary

* Screen coordinates: x right, y down, pixel (x, y) is a unit square with its center at (x+0.5, y+0.5).
* Always clip: never write outside the image.
* Bresenham's algorithm draws gap-free lines with integer steps and an error counter.
* Hard-edged shapes show **jaggies**. Next chapter we'll smooth them.

Next: [Chapter 9 — Circles, anti-aliasing and transparency →](09-circles-antialiasing.md)
