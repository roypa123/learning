# Line by line: `include/pixel/canvas.h`

[← Line-by-line index](README.md) · Chapters [8](../08-drawing-basics.md), [9](../09-circles-antialiasing.md), [10](../10-triangles-gradients.md)

**What this file does, in one sentence:** it gives an `Image` a set of **2D drawing tools**: pixels, rectangles,
lines, circles, glows, triangles, polygons and gradients.

Coordinates: x goes **right**, y goes **down**, (0, 0) is the top-left pixel. Pixel (x, y) is a small square from
x to x+1 and y to y+1; its **center** is at (x + 0.5, y + 0.5).

| Block | Lines | Job | Chapter |
|-------|-------|-----|---------|
| A. Comments, includes | 1–17 | tools, namespace | |
| B. The Canvas and pixels | 19–45 | hold the image; set, blend, add one pixel | 8, 9 |
| C. Rectangles | 47–60 | filled and outlined | 8 |
| D. Lines | 62–97 | Bresenham (sharp) and anti-aliased (smooth) | 8, 9 |
| E. Circles and glow | 99–152 | filled, outline, smooth, glowing | 9 |
| F. Triangles | 154–198 | edge function, filled triangle with 3 colors | 10 |
| G. Polygons | 200–234 | any shape, scanline method | 10 |
| H. Gradients | 236–255 | vertical and radial | 10 |
| I. End | 256–258 | | |

---

## Block A — Comments, includes (lines 1–17)

* Lines 1–8: comments. Line 7 is a reminder: all colors are **linear** (make them with `hex_color`).
* Line 9: `#pragma once`.
* Lines 10–13: `<cmath>` (floor, sqrt), `<vector>`, `<algorithm>` (`std::min`, `std::max`, `std::sort`,
  `std::fill`), `<utility>` (`std::pair`).
* Lines 14–15: our `vec3.h` (Color) and `image.h` (Image).
* Line 17: `namespace pixel`.

---

## Block B — The Canvas and pixels (lines 19–45)

```cpp
struct Canvas {
    Image& img;
    explicit Canvas(Image& target) : img(target) {}
```

* Line 19: a new type `Canvas`.
* Line 20: `img` is a **reference** to an existing image. The canvas doesn't own a copy; it draws directly **on**
  your image. Usage: `Canvas cv(my_image);`.
* Line 21: the constructor connects the canvas to the image. `explicit` prevents accidental conversions.

```cpp
    int width() const { return img.width; }
    int height() const { return img.height; }
    void clear(const Color& c) { img.fill(c); }
```

* Lines 23–24: short ways to ask the image's size.
* Line 26: fill the whole image with one color.

### Line 30–32: set one pixel (safely)

```cpp
    void set_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) = c;
    }
```

Only writes if (x, y) is inside the image. So shapes that go off the edge are simply cut (**clipped**) instead of
crashing the program.

### Lines 34–40: blend a pixel (transparency)

```cpp
    void blend_pixel(int x, int y, const Color& c, double alpha) {
        if (!img.in_bounds(x, y) || alpha <= 0.0) return;
        if (alpha >= 1.0) { img.at(x, y) = c; return; }
        Color& dst = img.at(x, y);
        dst = dst * (1.0 - alpha) + c * alpha;
    }
```

`alpha` = how solid the new color is: 0 = invisible, 1 = fully covers.

* Line 36: outside the image or invisible → do nothing.
* Line 37: fully solid → just replace (faster).
* Line 38: `dst` = a reference to the existing pixel.
* Line 39: the **"over" formula**: keep `(1 − alpha)` of the old color and add `alpha` of the new one.
  Example: alpha 0.3 → 70% old + 30% new.

### Lines 43–45: add light to a pixel

```cpp
    void add_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) += c;
    }
```

`+=` **adds** light instead of replacing it, like shining a lamp on it. Used for glows and stars.

---

## Block C — Rectangles (lines 47–60)

```cpp
    void fill_rect(int x0, int y0, int w, int h, const Color& c, double alpha = 1.0) {
        for (int y = y0; y < y0 + h; y++)
            for (int x = x0; x < x0 + w; x++)
                blend_pixel(x, y, c, alpha);
    }
```

* Lines 49–53: every pixel from column x0 to x0+w−1 and row y0 to y0+h−1 gets the color. `alpha` is optional
  (default solid). Using `blend_pixel` means it's clipped and can be transparent.

```cpp
    void draw_rect(int x0, int y0, int w, int h, const Color& c) {
        draw_line(x0, y0, x0 + w - 1, y0, c);
        draw_line(x0, y0 + h - 1, x0 + w - 1, y0 + h - 1, c);
        draw_line(x0, y0, x0, y0 + h - 1, c);
        draw_line(x0 + w - 1, y0, x0 + w - 1, y0 + h - 1, c);
    }
```

* Lines 55–60: an **outline**: four lines: top, bottom, left, right. `− 1` because the last pixel of a
  rectangle w pixels wide is at x0 + w − 1.

---

## Block D — Lines (lines 62–97)

### Lines 64–76: Bresenham's line (sharp, integer only)

```cpp
    void draw_line(int x0, int y0, int x1, int y1, const Color& c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
```

Draw from (x0, y0) to (x1, y1), stepping one pixel at a time.

* Line 66: `dx` = how far to go in x (always positive). `sx` = which way to step: +1 (right) or −1 (left).
* Line 67: `dy` = how far in y, stored as a **negative** number (a trick that makes the next lines simpler). `sy` =
  +1 (down) or −1 (up).
* Line 68: `err` = the **error counter**: it tracks whether the ideal line is more in the x or y direction from us.

```cpp
        while (true) {
            set_pixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
```

* Line 69: repeat until we reach the end.
* Line 70: draw the current pixel. (Note: we move `x0`, `y0` themselves as the "current position".)
* Line 71: reached the end point? Stop.
* Line 72: `e2` = twice the error (doubling avoids fractions).
* Line 73: if it's time to step in x: step, and update the error.
* Line 74: if it's time to step in y: step, and update the error. For diagonal parts, **both** happen.
* The result: a line with no gaps, in any direction, using only whole numbers. (A hand-traced example is in
  chapter 8, section 4.4.)

### Lines 78–97: a smooth, thick line

```cpp
    void draw_line_aa(double x0, double y0, double x1, double y1, double thickness, const Color& c) {
        double r = thickness * 0.5;
```

* Positions are `double`, so a line can start between pixels.
* Line 80: `r` = half the thickness: pixels closer than `r` to the line's center are inside.

```cpp
        int minx = (int)std::floor(std::min(x0, x1) - r - 1), maxx = (int)std::ceil(std::max(x0, x1) + r + 1);
        int miny = (int)std::floor(std::min(y0, y1) - r - 1), maxy = (int)std::ceil(std::max(y0, y1) + r + 1);
        minx = std::max(minx, 0); miny = std::max(miny, 0);
        maxx = std::min(maxx, width() - 1); maxy = std::min(maxy, height() - 1);
```

* Lines 81–82: the **bounding box**: the smallest rectangle around the line (plus thickness and 1 pixel extra).
  Only these pixels can be touched, so we don't check the whole image. `floor` = round down, `ceil` = round up.
* Lines 83–84: cut the box to the image edges.

```cpp
        double vx = x1 - x0, vy = y1 - y0;
        double len2 = vx * vx + vy * vy;
```

* Line 85: `(vx, vy)` = the line's direction (from start to end).
* Line 86: its squared length.

```cpp
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
```

For every pixel in the box:

* Line 89: `(px, py)` = the pixel center, measured from the line's start.
* Line 90: `t` = where along the line the **closest point** is: 0 = at the start, 1 = at the end. The formula is a dot
  product divided by the length² (a "projection"). `clamp01` keeps it on the segment (not beyond the ends).
  If the line has zero length, use 0.
* Line 91: `(dx, dy)` = from that closest point to the pixel center.
* Line 92: `d` = the distance from the pixel to the line.
* Line 93: **coverage**: fully inside (d much less than r) → 1; fully outside → 0; on the edge → in between.
  `+ 0.5` centers the soft edge on the true edge, and the fade is 1 pixel wide. This is **anti-aliasing**.
* Line 94: blend the color with that coverage as alpha.

---

## Block E — Circles and glow (lines 99–152)

### Lines 102–108: a filled circle (hard edge)

```cpp
    void fill_circle(int cx, int cy, int radius, const Color& c) {
        for (int y = cy - radius; y <= cy + radius; y++)
            for (int x = cx - radius; x <= cx + radius; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set_pixel(x, y, c);
            }
    }
```

* Lines 103–104: every pixel in the square around the circle.
* Line 105: distance parts from the center.
* Line 106: inside if dx² + dy² ≤ radius² (Pythagoras; comparing squares avoids a square root). Each pixel is either
  fully in or out, so the edge looks like a staircase.

### Lines 111–122: a circle outline (midpoint algorithm)

```cpp
    void draw_circle(int cx, int cy, int radius, const Color& c) {
        int x = radius, y = 0, err = 1 - radius;
        while (x >= y) {
```

* Line 112: start at the rightmost point of the circle (x = radius, y = 0). `err` decides when x must step inward.
* Line 113: we only walk **one eighth** of the circle: from the right side going around until x = y (45°).

```cpp
            set_pixel(cx + x, cy + y, c); set_pixel(cx + y, cy + x, c);
            set_pixel(cx - y, cy + x, c); set_pixel(cx - x, cy + y, c);
            set_pixel(cx - x, cy - y, c); set_pixel(cx - y, cy - x, c);
            set_pixel(cx + y, cy - x, c); set_pixel(cx + x, cy - y, c);
```

Lines 114–117: a circle looks the same after mirroring. Each point (x, y) we compute gives **8 points** by swapping
x/y and flipping signs. So 1/8 of the work draws the full circle.

```cpp
            y++;
            if (err < 0) err += 2 * y + 1;
            else { x--; err += 2 * (y - x) + 1; }
        }
    }
```

* Line 118: always step one pixel along y.
* Lines 119–120: the error says whether the true circle is now closer to x or x − 1. If needed, step x inward.
  All whole-number math, like Bresenham's line.

### Lines 125–137: a smooth circle

```cpp
    void fill_circle_aa(double cx, double cy, double radius, const Color& c, double alpha = 1.0) {
        int minx = std::max(0, (int)std::floor(cx - radius - 1));
        int maxx = std::min(width() - 1, (int)std::ceil(cx + radius + 1));
        int miny = std::max(0, (int)std::floor(cy - radius - 1));
        int maxy = std::min(height() - 1, (int)std::ceil(cy + radius + 1));
```

* Line 125: center and radius can be fractions; optional `alpha` for transparency.
* Lines 126–129: the bounding box, cut to the image.

```cpp
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(radius - d + 0.5);
                blend_pixel(x, y, c, coverage * alpha);
            }
    }
```

* Line 132: from the circle center to this pixel's **center**.
* Line 133: the distance.
* Line 134: coverage, the same idea as for smooth lines: 1 well inside, 0 well outside, a 1-pixel fade on the edge.
* Line 135: blend with coverage × alpha (partly covered **and** partly transparent).

### Lines 140–152: glow

```cpp
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
```

* Lines 141–142: the bounding box.
* Line 146: `d` = the distance as a **fraction** of the radius (0 at the center, 1 at the rim).
* Line 147: only inside the radius.
* Line 148: `k` = strength: 1 at the center, falling to 0 at the rim. Squaring `(1 − d)` makes it fall fast at first,
  then gently, so the glow looks soft.
* Line 149: **add** light (not replace), so overlapping glows get brighter.

---

## Block F — Triangles (lines 154–198)

### Lines 158–160: the edge function

```cpp
    static double edge(double ax, double ay, double bx, double by, double px, double py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }
```

* Tells **which side** of the line from A to B the point P is on: positive on one side, negative on the other,
  0 exactly on the line.
* Its size is twice the **area** of triangle A-B-P.
* `static` = it doesn't need a canvas; it's just math.

### Lines 164–193: a filled triangle with a color at each corner

```cpp
    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c0, const Color& c1, const Color& c2, int ss = 4) {
```

Three corners (x0,y0), (x1,y1), (x2,y2) with colors c0, c1, c2. `ss` = anti-aliasing samples per pixel side
(4 → 4×4 = 16 samples per pixel).

```cpp
        double area = edge(x0, y0, x1, y1, x2, y2);
        if (std::fabs(area) < 1e-12) return;   // degenerate (flat) triangle
```

* Line 166: twice the triangle's area (with a sign that depends on the corner order).
* Line 167: if it's zero (all points on a line), there's nothing to draw (and we'd divide by zero).

```cpp
        int minx = std::max(0, (int)std::floor(std::min({x0, x1, x2})));
        int maxx = std::min(width() - 1, (int)std::ceil(std::max({x0, x1, x2})));
        int miny = std::max(0, (int)std::floor(std::min({y0, y1, y2})));
        int maxy = std::min(height() - 1, (int)std::ceil(std::max({y0, y1, y2})));
```

Lines 168–171: the bounding box. `std::min({a, b, c})` with `{ }` finds the smallest of three numbers.

```cpp
        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                int inside = 0;
                Color sum(0, 0, 0);
```

* Lines 173–174: every pixel in the box.
* Line 175: how many of the pixel's samples are inside the triangle.
* Line 176: the sum of their colors.

```cpp
                for (int sy = 0; sy < ss; sy++) {
                    for (int sx = 0; sx < ss; sx++) {
                        double px = x + (sx + 0.5) / ss;
                        double py = y + (sy + 0.5) / ss;
```

* Lines 177–178: a small grid of ss × ss sample points **inside** this pixel.
* Lines 179–180: the position of sample (sx, sy). For ss = 4: offsets 0.125, 0.375, 0.625, 0.875.

```cpp
                        double w0 = edge(x1, y1, x2, y2, px, py) / area;
                        double w1 = edge(x2, y2, x0, y0, px, py) / area;
                        double w2 = edge(x0, y0, x1, y1, px, py) / area;
```

Lines 181–183: the **barycentric coordinates** of the sample: how much of each corner. Each is an area ratio (the
small triangle opposite a corner ÷ the whole triangle). Inside the triangle all three are between 0 and 1 and add up
to 1. Dividing by `area` (with its sign) makes this work for corners given clockwise or anticlockwise.

```cpp
                        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                            inside++;
                            sum += w0 * c0 + w1 * c1 + w2 * c2;
                        }
                    }
                }
```

* Line 184: all three ≥ 0 → the sample is inside.
* Line 185: count it.
* Line 186: its color = the corners' colors mixed by the weights (**interpolation**). Near corner 0, w0 ≈ 1 → mostly
  c0.

```cpp
                if (inside > 0) blend_pixel(x, y, sum / inside, (double)inside / (ss * ss));
            }
        }
    }
```

Line 190: if any sample was inside: the pixel color = the average of the inside samples, and the alpha = the fraction
of samples inside (the coverage). Edge pixels get partial coverage, so edges are smooth.

### Lines 195–198: one-color version

```cpp
    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c, int ss = 4) {
        fill_triangle(x0, y0, x1, y1, x2, y2, c, c, c, ss);
    }
```

The same function name with different inputs (**overloading**): it calls the version above with the same color for
all three corners.

---

## Block G — Polygons (lines 200–234)

Fills any shape given as a list of corner points, using the **scanline** method: for each horizontal line, find where
it crosses the shape's edges, and fill between pairs of crossings.

```cpp
    void fill_polygon(const std::vector<std::pair<double, double>>& pts, const Color& c, int ss = 4) {
        if (pts.size() < 3) return;
```

* Line 202: `pts` = a list of points; each `std::pair` holds two numbers: `.first` = x, `.second` = y.
* Line 203: fewer than 3 points isn't a shape.

```cpp
        double miny = pts[0].second, maxy = pts[0].second;
        for (auto& p : pts) { miny = std::min(miny, p.second); maxy = std::max(maxy, p.second); }
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(height() - 1, (int)std::ceil(maxy));
        std::vector<double> coverage(width());
```

* Lines 204–205: the lowest and highest y of all points.
* Line 206: the rows to process, cut to the image.
* Line 207: one coverage number per column, reused for every row.

```cpp
        for (int y = y0; y <= y1; y++) {
            std::fill(coverage.begin(), coverage.end(), 0.0);
            for (int s = 0; s < ss; s++) {
                double sy = y + (s + 0.5) / ss;
                std::vector<double> xs;
```

* Line 208: for each row.
* Line 209: reset coverage to 0.
* Lines 210–211: `ss` thin sub-lines inside this pixel row (for smooth top and bottom edges). `sy` = the height of
  this sub-line.
* Line 212: the x positions where this sub-line crosses the edges.

```cpp
                for (size_t i = 0; i < pts.size(); i++) {
                    auto a = pts[i], b = pts[(i + 1) % pts.size()];
                    if ((a.second <= sy && b.second > sy) || (b.second <= sy && a.second > sy)) {
                        double t = (sy - a.second) / (b.second - a.second);
                        xs.push_back(a.first + t * (b.first - a.first));
                    }
                }
```

* Line 213: for each edge.
* Line 214: the edge goes from point i to point i+1. `% pts.size()` wraps the last point back to the first, closing the
  shape.
* Line 215: does the edge cross height `sy`? (One end at or below, the other above. Using `<=` on one side only
  prevents counting a corner twice.)
* Line 216: `t` = how far along the edge the crossing is.
* Line 217: the crossing's x = start x + t × the edge's width. Add it to the list.

```cpp
                std::sort(xs.begin(), xs.end());
```

Line 220: sort the crossings from left to right.

```cpp
                for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                    double xa = xs[i], xb = xs[i + 1];
                    int ia = std::max(0, (int)std::floor(xa)), ib = std::min(width() - 1, (int)std::floor(xb));
                    for (int x = ia; x <= ib; x++) {
                        double left = std::max(xa, (double)x), right = std::min(xb, (double)x + 1);
                        if (right > left) coverage[x] += (right - left) / ss;
                    }
                }
            }
```

* Line 221: take the crossings in **pairs** (1st–2nd, 3rd–4th...). Between each pair we're inside the shape. This is
  the **even-odd rule**, and it also makes holes work.
* Line 223: the span from `xa` to `xb`.
* Line 224: the pixels the span touches.
* Lines 225–227: for each pixel, how much of its width (0–1) the span covers: the overlap of [xa, xb] with [x, x+1].
  Divided by `ss` because each sub-line counts for 1/ss of the pixel's height. Partial pixels at the ends give
  smooth left and right edges.

```cpp
            for (int x = 0; x < width(); x++)
                if (coverage[x] > 0) blend_pixel(x, y, c, std::min(1.0, coverage[x]));
        }
    }
```

Lines 231–232: after all sub-lines, blend each touched pixel with its total coverage (at most 1).

---

## Block H — Gradients (lines 236–255)

### Lines 239–245: vertical gradient

```cpp
    void vertical_gradient(int y0, int y1, const Color& top, const Color& bottom) {
        for (int y = std::max(0, y0); y < std::min(height(), y1); y++) {
            double t = (y1 - y0) > 1 ? (double)(y - y0) / (y1 - y0 - 1) : 0.0;
            Color c = lerp(top, bottom, t);
            for (int x = 0; x < width(); x++) img.at(x, y) = c;
        }
    }
```

* Line 240: rows from y0 up to (not including) y1, cut to the image.
* Line 241: `t` = 0 at row y0 and 1 at the last row. (If the range has only 1 row, use 0, to avoid dividing by 0.)
* Line 242: blend the two colors.
* Line 243: paint the whole row that color.

### Lines 248–255: radial gradient

```cpp
    void radial_gradient(double cx, double cy, double radius, const Color& inner, const Color& outer) {
        for (int y = 0; y < height(); y++)
            for (int x = 0; x < width(); x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double t = clamp01(std::sqrt(dx * dx + dy * dy) / radius);
                img.at(x, y) = lerp(inner, outer, t);
            }
    }
```

For every pixel: `t` = distance from the center ÷ radius (clamped to 1). Blend from `inner` (center) to `outer`
(radius and beyond).

---

## Block I — End (lines 256–258)

* Line 256: `};` ends the `Canvas` struct.
* Line 258: ends the namespace.

---

## Check your understanding

1. What's the difference between `set_pixel`, `blend_pixel` and `add_pixel`? *(Replace / mix by alpha / add light.)*
2. Why does `draw_circle` set 8 pixels per step? *(A circle is symmetric in 8 ways.)*
3. What does coverage 0.5 mean for a pixel? *(About half the pixel is covered by the shape.)*
4. What are w0, w1, w2 at the triangle's center? *(1/3 each.)*
5. Why sort the crossings in `fill_polygon`? *(To pair them left-to-right; inside is between pairs.)*
