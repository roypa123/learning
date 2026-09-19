# Line by line: `ch16_antialiasing.cpp`

[← Line-by-line index](README.md) · [Chapter 16 (the theory)](../16-random-and-antialiasing.md) · [random.h](random.md)

**What the whole program does, in one sentence:** it renders the chapter 15 scene with 1 ray per pixel and with 64
random rays per pixel, and shows an enlarged piece of both side by side, so you can see how averaging many rays smooths
the edges.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. `ray_color` | 12–18 | same as chapter 15 |
| C. `render` | 20–41 | render the image with N samples per pixel |
| D. `crop_zoom` | 43–50 | cut out a piece and enlarge it |
| E. `main`: the comparison | 52–66 | |
| F. `main`: random pixels | 68–73 | a picture of random numbers |

---

## Block A — Comments, includes (lines 1–10)

Comments, our library, `using namespace pixel`.

---

## Block B — `ray_color` (lines 12–18)

Exactly as in [ch15](ch15_normals.md): normal color if hit, sky otherwise. Line 14 does the hit test and the return in one
line.

---

## Block C — `render` (lines 20–41)

```cpp
Image render(const Hittable& world, int samples_per_pixel) {
```

Line 20: a function that renders the whole image and returns it. `samples_per_pixel` = how many rays per pixel.

Lines 21–25: the same compact camera as before.

```cpp
    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Color sum(0, 0, 0);
```

* Line 27: the output image.
* Lines 28–29: every pixel.
* Line 30: we'll add up the colors of all rays for this pixel.

```cpp
            for (int s = 0; s < samples_per_pixel; s++) {
                double ox = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
                double oy = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
```

* Line 31: for each sample (ray).
* Lines 33–34: a random offset inside the pixel: `random_double() − 0.5` is between −0.5 and +0.5. So the ray goes
  through a **random point of the pixel square**, not its exact center. With only 1 sample, use the center (offset 0),
  like before.

```cpp
                Point3 target = pixel00 + (i + ox) * du + (j + oy) * dv;
                sum += ray_color(Ray(eye, target - eye), world);
            }
```

* Line 35: the 3D point we aim at: pixel (i + ox, j + oy).
* Line 36: shoot the ray and add its color to the sum.

```cpp
            img.at(i, j) = sum / samples_per_pixel;   // the average of all samples
        }
    return img;
}
```

* Line 38: the pixel's color = the **average** of all its rays. At an edge, some rays hit the sphere and some see the sky,
  so the average is a color in between: a smooth edge.
* Line 40: return the finished image.

---

## Block D — `crop_zoom` (lines 43–50)

```cpp
Image crop_zoom(const Image& src, int x0, int y0, int w, int h, int factor) {
    Image out(w * factor, h * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x0 + x / factor, y0 + y / factor);
    return out;
}
```

Cut out the rectangle starting at (x0, y0) with size w × h, and enlarge it `factor` times with square blocks (like
[ch09's zoom](ch09_circles_antialiasing.md), plus an offset).

---

## Block E — `main`: the comparison (lines 52–66)

```cpp
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));
```

Lines 53–55: the same world as chapter 15.

```cpp
    Image one = render(world, 1);
    Image many = render(world, 64);
```

Lines 57–58: render twice: 1 ray per pixel and 64 rays per pixel (64 times more work).

```cpp
    Image left = crop_zoom(one, 230, 50, 50, 50, 6);
    Image right = crop_zoom(many, 230, 50, 50, 50, 6);
```

Lines 61–62: cut the same 50 × 50 area (containing the sphere's upper-right edge) from both, enlarged 6×.

```cpp
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch16_aa_compare.png", post::side_by_side(left, right, 10), raw);
    save_image("images/ch16_full_64spp.png", many, raw);
```

Lines 63–66: save the two crops side by side (a 10-pixel gap), and the full smooth image.

---

## Block F — `main`: random pixels (lines 68–73)

```cpp
    Image rnd(256, 128);
    for (auto& c : rnd.data) { double v = random_double(); c = Color(v, v, v); }
    save_image("images/ch16_random_pixels.png", rnd, raw);
    return 0;
}
```

* Line 69: a small image.
* Line 70: every pixel gets a random grey. `auto& c` = a reference to each pixel, so we change the real pixel.
* Line 71: save. A good random generator shows **no patterns** here. It's a quick visual test.

---

## Check your understanding

1. What range do `ox` and `oy` have? *(−0.5 to 0.5: anywhere inside the pixel.)*
2. Why divide `sum` by the number of samples? *(To get the average color.)*
3. How much slower is 64 samples than 1? *(About 64 times.)*
4. Why use the pixel center when there is only 1 sample? *(So the 1-sample image matches the old chapters exactly.)*
