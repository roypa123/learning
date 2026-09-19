# Line by line: `ch13_rays_sky.cpp`

[← Line-by-line index](README.md) · [Chapter 13 (the theory)](../13-rays-and-camera.md) · [ray.h explained](ray.md)

**What the whole program does, in one sentence:** it sets up a simple 3D camera, shoots one ray through every pixel,
and colors each pixel by the ray's direction, which gives a blue-to-white sky.

This is the skeleton of every ray tracer. Understand this file well: all later chapters build on it.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. `ray_color` | 11–16 | what color does a ray see? (only sky for now) |
| C. Image size | 18–22 | 400 × 225 |
| D. Camera and viewport | 24–38 | where the eye is; where each pixel is in 3D |
| E. Render loop | 40–49 | one ray per pixel |
| F. Save and end | 50–55 | |

---

## Block A — Comments, includes (lines 1–9)

Comments; our library; `using namespace pixel`.

---

## Block B — `ray_color` (lines 11–16)

```cpp
Color ray_color(const Ray& r) {
    Vec3 unit_direction = unit_vector(r.direction());
    double a = 0.5 * (unit_direction.y + 1.0);     // y in [-1,1]  ->  a in [0,1]
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}
```

* Line 12: a function: give it a ray, it returns the color seen along that ray.
* Line 13: the ray's direction with length 1. Its `y` part is then between −1 (pointing straight down) and +1 (straight
  up).
* Line 14: turn y (−1..1) into `a` (0..1): add 1 (→ 0..2), halve (→ 0..1). So `a` = 0 looking down, 1 looking up.
* Line 15: blend: `(1 − a) × white + a × light blue`. Looking up → blue; looking straight ahead or down → white.

---

## Block C — Image size (lines 18–22)

```cpp
int main() {
    const double aspect_ratio = 16.0 / 9.0;
    const int image_width = 400;
    const int image_height = (int)(image_width / aspect_ratio);   // 225
```

* Line 20: the **aspect ratio** = width ÷ height, 16:9 like a widescreen TV. `16.0 / 9.0` (with .0) gives 1.777...
  (with `16 / 9` it would be whole-number division: 1!).
* Line 21: 400 pixels wide.
* Line 22: the height that matches: 400 ÷ 1.777 = 225.

---

## Block D — Camera and viewport (lines 24–38)

Picture the camera as an **eye** looking through a **window** (the viewport). The window has a grid of small squares:
one square per pixel.

```
                         viewport (window) at z = -1
                ┌─────────────────────────────────┐
                │ ·  ·  ·  ·  ·  ·  ·  ·  ·  ·    │   · = pixel centers
   eye ●  ─ ─ ─ │ ─ ─ ─ ─ ─ ─ ● ─ ─ ─ ─ ─ ─ ─ ─ ─│─ ─▶ looking along -z
 (0,0,0)        │ ·  ·  ·  ·  ·  ·  ·  ·  ·  ·    │
                └─────────────────────────────────┘
```

```cpp
    const double focal_length = 1.0;               // distance eye -> viewport
    const double viewport_height = 2.0;
    const double viewport_width = viewport_height * (double(image_width) / image_height);
    const Point3 camera_center(0, 0, 0);
```

* Line 25: the window is 1 unit in front of the eye.
* Line 26: the window is 2 units tall (from y = −1 to y = +1).
* Line 27: its width matches the image's shape: 2 × (400 ÷ 225) ≈ 3.56. (We use the real pixel counts, so pixels are
  exactly square.)
* Line 28: the eye is at the origin (0, 0, 0).

```cpp
    const Vec3 viewport_u(viewport_width, 0, 0);
    const Vec3 viewport_v(0, -viewport_height, 0);
```

* Line 31: a vector along the **top edge** of the window, left → right.
* Line 32: a vector along the **left edge**, top → bottom. The **minus** is because pixel rows go down, but 3D y goes up.

```cpp
    const Vec3 pixel_delta_u = viewport_u / image_width;
    const Vec3 pixel_delta_v = viewport_v / image_height;
```

Lines 34–35: the step from one pixel to the next: the window's width ÷ 400 across, and its height ÷ 225 down.

```cpp
    const Point3 viewport_upper_left = camera_center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    const Point3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
```

* Line 37: the window's upper-left corner. Start at the eye, go 1 unit forward (−z), then half the width to the left
  (− u/2) and half the height up (− v/2; v points down, so subtracting goes up).
* Line 38: the **center of the first pixel** (top-left): half a pixel step right and half a step down from the corner.

---

## Block E — Render loop (lines 40–49)

```cpp
    Image img(image_width, image_height);
    for (int j = 0; j < image_height; j++) {
        for (int i = 0; i < image_width; i++) {
```

* Line 41: the output image.
* Lines 42–43: every pixel: `j` = row, `i` = column.

```cpp
            Point3 pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
            Vec3 ray_direction = pixel_center - camera_center;
            Ray r(camera_center, ray_direction);
            img.at(i, j) = ray_color(r);
        }
    }
```

* Line 44: where this pixel's center is in 3D: start at pixel (0, 0), take `i` steps right and `j` steps down.
* Line 45: the direction from the eye to that point ("point − point = direction").
* Line 46: a ray from the eye in that direction.
* Line 47: ask what color it sees, and store it.

---

## Block F — Save and end (lines 50–55)

```cpp
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch13_sky.png", img, raw);
    return 0;
}
```

* Lines 51–52: save options **without** the sRGB curve: the sky colors on line 15 were chosen as screen colors, so we
  store them as they are.
* Line 53: save.

---

## Check your understanding

1. What does `a` equal for a ray pointing straight ahead (y = 0)? *(0.5: halfway between white and blue.)*
2. Why is `viewport_v` negative in y? *(Rows go down the image, but 3D y goes up.)*
3. Where is pixel (0, 0)'s center relative to the window corner? *(Half a pixel right and half a pixel down.)*
4. If `image_width` became 800, which lines change automatically? *(The height, the deltas: everything computed from it.)*
