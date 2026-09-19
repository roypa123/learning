# Chapter 5 — The Image class

[← Color](04-color.md) · [Contents](README.md) · [Next: PNG part 1 →](06-png-part1.md)

---

## Goal

Build the data structure that holds every picture in this book: `pixel::Image`. By the end you'll be
able to:

* create an image of any size and read/write any pixel,
* save it as PPM, BMP (and soon PNG), with color conversion handled for you,
* load a PPM back in,
* **sample** an image at fractional coordinates, with *nearest* and *bilinear* filtering (needed
  later for textures).

---

## 1. Design decisions

### 1.1 Store colors as `double`, in linear light

We could store bytes (0–255) like the files do. We won't, because:

* rendering math needs fractions and values above 1 (HDR),
* repeated byte conversions lose precision ("banding"),
* chapter 4 taught us that math must happen in **linear** space.

So each pixel is a `Color` (three `double`s) holding **linear light**. It costs 24 bytes per pixel
instead of 3. A 1920×1080 image is ~50 MB, which is fine for a modern PC.

### 1.2 One vector, row by row

As in chapter 2: `data[y * width + x]`. Row 0 is the **top** of the image.

### 1.3 Conversion happens only when saving

All the "how should this look on screen" decisions (exposure, tone mapping, sRGB) are bundled in a
small struct, `SaveOptions`, and applied by `to_rgb8()` when writing a file:

```
 linear HDR Color ──▶ × exposure ──▶ tone map ──▶ sRGB curve ──▶ ×255, round ──▶ byte
     (double)                       (0..∞ → 0..1)  (optional)
```

---

## 2. The code: `image.h`

Here is the whole file. We'll go through the important parts below.

**File: `include/pixel/image.h`**

```cpp
// pixel/image.h
// ------------------------------------------------------------
// The Image class: a grid of linear-light colors (doubles),
// plus saving (PPM / BMP / PNG) and loading (PPM).
// Explained in docs/05-image-class.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include "vec3.h"
#include "color.h"
#include "png.h"

namespace pixel {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> data;   // row by row, top row first

    Image() {}
    Image(int w, int h, const Color& fill_color = Color(0, 0, 0))
        : width(w), height(h), data((size_t)w * h, fill_color) {}

    bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }

    Color& at(int x, int y) { return data[(size_t)y * width + x]; }
    const Color& at(int x, int y) const { return data[(size_t)y * width + x]; }

    // Like at(), but coordinates outside the image are clamped to the edge.
    Color get_clamped(int x, int y) const {
        x = x < 0 ? 0 : (x >= width ? width - 1 : x);
        y = y < 0 ? 0 : (y >= height ? height - 1 : y);
        return at(x, y);
    }

    void fill(const Color& c) { for (auto& p : data) p = c; }

    // Look up a color with texture coordinates u,v in [0,1].
    // v = 0 is the BOTTOM of the image (the usual convention for textures).
    // Bilinear filtering blends the 4 nearest pixels for a smooth result.
    Color sample_bilinear(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);   // magenta = "missing"
        u = u - std::floor(u);          // wrap around (tiling)
        v = v - std::floor(v);
        double fx = u * width - 0.5;
        double fy = (1.0 - v) * height - 0.5;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        double tx = fx - x0, ty = fy - y0;
        Color c00 = get_clamped(x0, y0),     c10 = get_clamped(x0 + 1, y0);
        Color c01 = get_clamped(x0, y0 + 1), c11 = get_clamped(x0 + 1, y0 + 1);
        return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
    }

    Color sample_nearest(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);
        u = u - std::floor(u);
        v = v - std::floor(v);
        int x = (int)(u * width), y = (int)((1.0 - v) * height);
        return get_clamped(x, y);
    }
};

// How to turn linear HDR colors into 8-bit screen colors.
struct SaveOptions {
    double exposure = 1.0;                  // multiply light before tone mapping
    ToneMapper tonemap = ToneMapper::Clamp;
    bool srgb = true;                       // apply the sRGB gamma curve
};

inline Color encode_pixel(const Color& linear, const SaveOptions& opt) {
    Color c = linear * opt.exposure;
    // NaN protection: a NaN is never equal to itself.
    if (c.x != c.x) c.x = 0;
    if (c.y != c.y) c.y = 0;
    if (c.z != c.z) c.z = 0;
    c = apply_tonemap(c, opt.tonemap);
    if (opt.srgb) c = linear_to_srgb(c);
    return c;
}

inline std::vector<uint8_t> to_rgb8(const Image& img, const SaveOptions& opt = SaveOptions()) {
    std::vector<uint8_t> bytes((size_t)img.width * img.height * 3);
    for (size_t i = 0; i < img.data.size(); i++) {
        Color c = encode_pixel(img.data[i], opt);
        bytes[i * 3 + 0] = to_byte(c.x);
        bytes[i * 3 + 1] = to_byte(c.y);
        bytes[i * 3 + 2] = to_byte(c.z);
    }
    return bytes;
}

// Make sure the folder for 'filename' exists (e.g. "images/").
inline void ensure_parent_folder(const std::string& filename) {
    std::filesystem::path p(filename);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
}

// ---------------- PPM (the simplest image format) --------------------------

inline bool write_ppm(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::ofstream f(filename, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << img.width << ' ' << img.height << "\n255\n";
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

// Reads P3 (text) and P6 (binary) PPM files. Result is converted to LINEAR color.
inline bool read_ppm(const std::string& filename, Image& out, bool file_is_srgb = true) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;

    // Read the next header token, skipping whitespace and # comments.
    auto next_token = [&](std::string& tok) -> bool {
        tok.clear();
        char ch;
        while (f.get(ch)) {
            if (ch == '#') { std::string line; std::getline(f, line); continue; }
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                if (!tok.empty()) return true;
                continue;
            }
            tok += ch;
        }
        return !tok.empty();
    };

    std::string magic, sw, sh, smax;
    if (!next_token(magic) || !next_token(sw) || !next_token(sh) || !next_token(smax)) return false;
    if (magic != "P6" && magic != "P3") return false;
    int w = std::stoi(sw), h = std::stoi(sh), maxval = std::stoi(smax);
    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 255) return false;

    out = Image(w, h);
    for (int i = 0; i < w * h; i++) {
        int rgb[3];
        for (int k = 0; k < 3; k++) {
            if (magic == "P6") {
                char ch;
                if (!f.get(ch)) return false;
                rgb[k] = (unsigned char)ch;
            } else {
                std::string tok;
                if (!next_token(tok)) return false;
                rgb[k] = std::stoi(tok);
            }
        }
        Color c(rgb[0] / (double)maxval, rgb[1] / (double)maxval, rgb[2] / (double)maxval);
        out.data[i] = file_is_srgb ? srgb_to_linear(c) : c;
    }
    return true;
}

// ---------------- BMP (opens in every Windows program) ----------------------

// Little-endian helpers: BMP stores numbers lowest byte first.
inline void put_u16_le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF)); v.push_back((uint8_t)((x >> 8) & 0xFF));
}
inline void put_u32_le(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)((x >> (8 * i)) & 0xFF));
}

inline bool write_bmp(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> rgb = to_rgb8(img, opt);
    const int row_size = (img.width * 3 + 3) & ~3;          // rows padded to 4 bytes
    const uint32_t pixel_bytes = (uint32_t)row_size * img.height;
    std::vector<uint8_t> f;
    // --- file header (14 bytes)
    f.push_back('B'); f.push_back('M');
    put_u32_le(f, 54 + pixel_bytes);   // total file size
    put_u32_le(f, 0);                  // reserved
    put_u32_le(f, 54);                 // where the pixels start
    // --- info header (40 bytes)
    put_u32_le(f, 40);
    put_u32_le(f, (uint32_t)img.width);
    put_u32_le(f, (uint32_t)img.height); // positive height = rows stored bottom-up
    put_u16_le(f, 1);                  // planes
    put_u16_le(f, 24);                 // bits per pixel
    put_u32_le(f, 0);                  // no compression
    put_u32_le(f, pixel_bytes);
    put_u32_le(f, 2835); put_u32_le(f, 2835);   // 72 DPI
    put_u32_le(f, 0); put_u32_le(f, 0);
    // --- pixels: bottom row first, in B,G,R order
    for (int y = img.height - 1; y >= 0; y--) {
        for (int x = 0; x < img.width; x++) {
            size_t i = ((size_t)y * img.width + x) * 3;
            f.push_back(rgb[i + 2]); f.push_back(rgb[i + 1]); f.push_back(rgb[i + 0]);
        }
        for (int pad = img.width * 3; pad < row_size; pad++) f.push_back(0);
    }
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out.write((const char*)f.data(), (std::streamsize)f.size());
    return (bool)out;
}

// ---------------- PNG (uses our own encoder in png.h) -----------------------

inline bool write_png(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    return png::write_file(filename, bytes.data(), img.width, img.height, 3, 1);
}

// Save as .png, .bmp or .ppm depending on the file extension. Prints a message.
inline bool save_image(const std::string& filename, const Image& img,
                       const SaveOptions& opt = SaveOptions()) {
    bool ok;
    std::string ext = std::filesystem::path(filename).extension().string();
    if (ext == ".ppm")      ok = write_ppm(filename, img, opt);
    else if (ext == ".bmp") ok = write_bmp(filename, img, opt);
    else                    ok = write_png(filename, img, opt);
    std::printf("%s %s (%dx%d)\n", ok ? "Saved" : "FAILED to save", filename.c_str(),
                img.width, img.height);
    return ok;
}

} // namespace pixel
```

### 2.1 The struct

```cpp
struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> data;
    ...
};
```

* `Image(w, h, fill_color)` creates the pixel vector with `w*h` copies of the fill color.
* `at(x, y)` returns a **reference** to the pixel, so you can both read and write it:
  `img.at(3, 4) = Color(1, 0, 0);`. It doesn't check bounds (for speed); `in_bounds(x, y)` does.
* `get_clamped(x, y)` returns the nearest edge pixel for coordinates outside the image. That's useful for
  filters that look at neighbors (blur, bilinear sampling).

### 2.2 Saving: `to_rgb8`, `write_ppm`, `write_bmp`, `save_image`

`encode_pixel` performs the conversion pipeline shown above, and also protects against **NaN**
("Not a Number"), a special value produced by invalid math like `0.0 / 0.0` or `sqrt(-1)`. A NaN has
the odd property that `x != x` is true, which is exactly how we detect it. One NaN pixel would
otherwise turn into garbage.

`write_bmp` is the chapter 3 code, rewritten as a reusable function. `save_image` looks at the file
extension and picks the right writer, and prints a message.

### 2.3 Loading: `read_ppm`

A PPM header is a sequence of *tokens* separated by whitespace, with `#` comments allowed. The
`next_token` lambda reads one token at a time, skipping whitespace and comments. After the header:

* P6: read 3 raw bytes per pixel,
* P3: read 3 text tokens per pixel.

Then we divide by `maxval` (usually 255) and convert sRGB → linear.

### 2.4 Sampling: reading "between" pixels

Textures (chapter 24) need to ask *"what's the color at position (0.37, 0.81) of this image?"*. The
position is a pair of **texture coordinates** `(u, v)` in the range 0–1, with `v = 0` at the
**bottom** (the usual convention in 3D). That rarely lands exactly on a pixel center.

**Nearest** filtering takes the pixel that contains the point. It's fast but blocky when enlarged.

**Bilinear** filtering blends the 4 closest pixel centers, weighted by distance:

```
   c00 ●──────────────● c10          top    = lerp(c00, c10, tx)
       │     ×        │              bottom = lerp(c01, c11, tx)
       │   (tx,ty)    │              result = lerp(top, bottom, ty)
   c01 ●──────────────● c11
```

where `lerp(a, b, t) = a + (b − a)·t` is **linear interpolation**, which you'll use a thousand times
in graphics. The `- 0.5` in the code accounts for pixel *centers* being at half-integer positions: pixel
0 covers 0.0–1.0, and its center is at 0.5.

`u - floor(u)` keeps only the fractional part, so `u = 1.25` becomes `0.25`: the texture
**repeats** (tiles).

---

## 3. The chapter program

**File: `chapters/ch05_image_class.cpp`**

```cpp
// ch05_image_class.cpp
// ------------------------------------------------------------
// Chapter 5: The Image class.
//  1. Build a checkerboard + gradient image with Image::at()
//  2. Save it as PPM and BMP
//  3. Load the PPM back, invert it, flip it, and save again
//  4. Scale a tiny image up with nearest and bilinear sampling
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Create --------------------------------------------------
    Image img(320, 240);
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            bool dark_square = ((x / 40) + (y / 40)) % 2 == 0;
            double u = (double)x / (img.width - 1);
            Color base = lerp(hex_color(0x2E86DE), hex_color(0xF368E0), u);  // blue -> pink
            img.at(x, y) = dark_square ? base * 0.35 : base;
        }

    // ---------- 2. Save ----------------------------------------------------
    save_image("images/ch05_checker.ppm", img);
    save_image("images/ch05_checker.bmp", img);

    // ---------- 3. Load, change, save -------------------------------------
    Image loaded;
    if (!read_ppm("images/ch05_checker.ppm", loaded)) {
        std::printf("Could not read the PPM back!\n");
        return 1;
    }
    Image changed(loaded.width, loaded.height);
    for (int y = 0; y < loaded.height; y++)
        for (int x = 0; x < loaded.width; x++) {
            Color c = loaded.at(x, loaded.height - 1 - y);        // upside down
            Color display = linear_to_srgb(c);                    // invert what we SEE
            Color inverted = Color(1, 1, 1) - display;
            changed.at(x, y) = srgb_to_linear(inverted);
        }
    save_image("images/ch05_inverted.bmp", changed);

    // ---------- 4. Resampling: making a 4x4 image big ---------------------
    Image tiny(4, 4);
    Color palette[4] = { hex_color(0xFF6B6B), hex_color(0xFECA57), hex_color(0x48DBFB), hex_color(0x1DD1A1) };
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            tiny.at(x, y) = palette[(x + 2 * y) % 4];

    Image big_nearest(256, 256), big_bilinear(256, 256);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            double u = (x + 0.5) / 256.0;
            double v = 1.0 - (y + 0.5) / 256.0;   // v = 0 is the bottom
            big_nearest.at(x, y) = tiny.sample_nearest(u, v);
            big_bilinear.at(x, y) = tiny.sample_bilinear(u, v);
        }
    save_image("images/ch05_nearest.bmp", big_nearest);
    save_image("images/ch05_bilinear.bmp", big_bilinear);
    return 0;
}
```

What it does:

1. Makes a 320×240 image: a blue→pink horizontal gradient with a checkerboard of darker squares.
   `(x / 40 + y / 40) % 2` uses integer division on purpose: `x / 40` is the *column number* of the
   40-pixel square. When column + row is even, the square is dark.
2. Saves it as PPM and BMP.
3. Loads the PPM, flips it upside down, and inverts its colors. To invert what we *see*, we convert
   to sRGB, compute `1 − value`, and convert back.
4. Enlarges a 4×4 image to 256×256 with both sampling methods.

```bat
run ch05_image_class
```

---

## 4. What you should see

![Checker](../images/ch05_checker.bmp)

> **Image description (ch05_checker):** A 320×240 image with a checkerboard of 40-pixel squares.
> The colors go smoothly from a medium blue on the left to a bright pink-magenta on the right. Every
> other square is the same color but much darker (35% of the light), so the checkerboard is visible
> across the whole gradient.

![Inverted](../images/ch05_inverted.bmp)

> **Image description (ch05_inverted):** The same checkerboard, upside down, with inverted colors:
> the left side is now orange-yellow and the right side green. The dark squares have become very light.

![Nearest](../images/ch05_nearest.bmp) ![Bilinear](../images/ch05_bilinear.bmp)

> **Image description (nearest vs bilinear):** Two 256×256 enlargements of the same 4×4 image of
> coral, yellow, sky-blue and green pixels. The **nearest** version is 16 crisp squares, a pixel-art
> look. The **bilinear** version shows the same colors as soft blobs that melt into each other, with
> smooth gradients between neighbors and flat color at the outer edges (from clamping).

---

## Try it yourself

1. Add a `flip_horizontal(const Image&)` function that mirrors an image left-right.
2. Write a function that converts an image to **greyscale** using luminance (chapter 4). Save
   the checker in grey.
3. Scale the 4×4 image with `sample_bilinear` but pass `u * 3` and `v * 3`. What does tiling look like?
4. Open `ch05_checker.ppm` in a hex editor (VS Code has a "Hex Editor" extension). Find the header
   text, then the first pixel's three bytes.

## Common problems

| Symptom | Cause |
|---------|-------|
| Crash when writing a pixel | `x` or `y` outside the image: use `in_bounds` or `get_clamped` |
| Loaded image looks too dark or too bright | Loaded sRGB data without `srgb_to_linear`, or converted twice |
| Texture looks shifted by half a pixel | Forgot the `- 0.5` pixel center offset |

---

## Summary

* `Image` = width, height, and a vector of **linear** colors, row by row, top row first.
* `SaveOptions` + `to_rgb8` turn linear HDR values into bytes: exposure → tone map → sRGB.
* PPM and BMP writers are simple; the PPM reader lets us load textures.
* Bilinear sampling blends the four nearest pixels, and `lerp` is your best friend.

Next: [Chapter 6 — Writing PNG files, part 1 →](06-png-part1.md). BMPs are big; let's make real
PNGs.
