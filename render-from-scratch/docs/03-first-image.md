# Chapter 3 — Your first image

[← C++ crash course](02-cpp-crash-course.md) · [Contents](README.md) · [Next: Color →](04-color.md)

> 📖 **Line by line:** [ch03_first_image explained line by line](line-by-line/ch03_first_image.md)

---

## Goal

Write a program, **with no library at all**, that:

1. computes a color for each of 65,536 pixels,
2. saves them as a **PPM** file (the simplest image format there is), and
3. saves them as a **BMP** file (which Windows opens with a double-click).

Along the way you'll learn how an image file is laid out byte by byte.

---

## 1. The idea: a color for every (x, y)

We'll make a 256 × 256 image where:

* **red** grows from 0 at the left edge to full at the right edge,
* **green** grows from 0 at the top edge to full at the bottom edge,
* **blue** is a constant 25%.

```
           x = 0 ─────────────────────────▶ x = 255
  y = 0    (r=0, g=0)   dark blue    ...   (r=1, g=0)   red/magenta
    │
    │
    ▼
  y = 255  (r=0, g=1)   green        ...   (r=1, g=1)   yellow
```

Every pixel's color is a **formula of its position**. This idea (*color = f(x, y)*) is the seed of
everything in this book. Ray tracing, later on, is just a very clever `f`.

### From "fraction" to "byte"

We compute colors as fractions between **0.0 and 1.0**, because that's natural for math: 0 = none,
1 = full. Files want bytes from **0 to 255**. To convert:

```
byte = (int)(255.999 * fraction)
```

Why 255.999 and not 255? Because `(int)` *cuts off* the fraction (it doesn't round). With 255, only
exactly 1.0 would become 255; with 255.999, the whole top slice [0.99609…, 1.0] maps to 255, so all
256 byte values get an equal share of the 0–1 range.

| fraction | 255 × fraction | (int) | 255.999 × fraction | (int) |
|----------|----------------|-------|--------------------|-------|
| 0.0 | 0 | 0 | 0 | 0 |
| 0.5 | 127.5 | 127 | 127.9995 | 127 |
| 0.999 | 254.745 | 254 | 255.743 | **255** |
| 1.0 | 255 | 255 | 255.999 | 255 |

---

## 2. Pixels in memory

We keep all pixels in one `std::vector<uint8_t>`, three bytes per pixel (R, G, B), row after row:

```
index:   0   1   2   3   4   5   6   7   8  ...
byte:   R00 G00 B00 R10 G10 B10 R20 G20 B20 ...     (Rxy = red of pixel x,y)
        └─ pixel (0,0) ┘└─ pixel (1,0) ┘└─ pixel (2,0) ┘
```

The first byte of pixel `(x, y)` is at:

```
index = (y * width + x) * 3
```

---

## 3. The PPM format

PPM ("Portable PixMap") is a format from the 1980s, designed to be easy to write. There are two
flavors. **P3** is plain text:

```
P3              <- "magic number": this is a text PPM
256 256         <- width height
255             <- maximum value of a color component
0 0 63          <- pixel (0,0): R G B
1 0 63          <- pixel (1,0)
2 0 63
...
```

That's the *entire* format. You can open a P3 file in Notepad and read the pixels.
**P6** is the binary flavor: the same header, then raw bytes instead of text numbers (3 bytes per
pixel, ~4× smaller). We'll use P6 in the library.

The downside: Windows can't show PPM files. That's why we also write a BMP.

---

## 4. The BMP format

BMP ("bitmap") is Windows' own simple format. It's a bit more complicated than PPM because of
some historical quirks, but every Windows program can open it.

### 4.1 The header: 54 bytes

```
offset  size  value              meaning
──────  ────  ─────────────────  ─────────────────────────────────────────
  0      2    'B' 'M'            magic: "this is a BMP"
  2      4    file size          total number of bytes in the file
  6      4    0                  reserved
 10      4    54                 where pixel data starts (right after the header)
 14      4    40                 size of the "info header" that follows
 18      4    width
 22      4    height             (positive = rows stored bottom-up)
 26      2    1                  color planes (always 1)
 28      2    24                 bits per pixel: 24 = 3 bytes (B, G, R)
 30      4    0                  compression: none
 34      4    pixel data size
 38..53       0s                 resolution and palette info we don't need
```

### 4.2 Little-endian numbers

A number like the width (256) needs **4 bytes**. In which order? BMP stores the **least significant
byte first**. This is called **little-endian**:

```
256 = 0x00000100

big-endian (PNG):    00 00 01 00
little-endian (BMP): 00 01 00 00
```

Our `put32` helper does this with **bit shifts**: `v >> 8` moves the number 8 bits to the right
(dividing by 256), and `& 0xFF` keeps only the lowest 8 bits (one byte).

```cpp
header[offset + 0] = v & 0xFF;          // lowest byte
header[offset + 1] = (v >> 8) & 0xFF;
header[offset + 2] = (v >> 16) & 0xFF;
header[offset + 3] = (v >> 24) & 0xFF;  // highest byte
```

### 4.3 Three quirks of BMP pixels

1. **Bottom-up**: the first row in the file is the *bottom* row of the image.
2. **BGR**: each pixel is stored Blue, Green, Red, not RGB.
3. **Padding**: each row must be a multiple of 4 bytes long. A row of 5 pixels is 15 bytes, so we
   add 1 padding byte to make 16. The formula `(width * 3 + 3) / 4 * 4` rounds up to a multiple of 4.

```
width = 5:   B G R | B G R | B G R | B G R | B G R | 0      <- 15 bytes + 1 padding = 16
```

For our 256-pixel-wide image a row is 768 bytes (already a multiple of 4), but the code handles
any width.

---

## 5. The code

Here's the complete program. Read it top to bottom; each part matches a section above.

**File: `chapters/ch03_first_image.cpp`**

```cpp
// ch03_first_image.cpp
// ------------------------------------------------------------
// Chapter 3: Your very first image. NO library, nothing hidden.
// We compute a color for every pixel and write it into two files:
//   images/ch03_gradient.ppm  (the simplest format in the world)
//   images/ch03_gradient.bmp  (opens in Windows Photos / Paint)
// Build & run:   run ch03_first_image
// ------------------------------------------------------------
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>

int main() {
    const int width = 256;
    const int height = 256;

    // Make the "images" folder if it does not exist yet.
    std::filesystem::create_directories("images");

    // ---- 1. Compute every pixel and keep it in memory -------------------
    // We store 3 bytes per pixel: red, green, blue (0..255 each).
    std::vector<uint8_t> pixels(width * height * 3);

    for (int y = 0; y < height; y++) {          // rows, top to bottom
        for (int x = 0; x < width; x++) {       // columns, left to right
            double r = double(x) / (width - 1);   // 0.0 at left  -> 1.0 at right
            double g = double(y) / (height - 1);  // 0.0 at top   -> 1.0 at bottom
            double b = 0.25;                      // a little blue everywhere

            int index = (y * width + x) * 3;      // where this pixel lives in the array
            pixels[index + 0] = (uint8_t)(255.999 * r);
            pixels[index + 1] = (uint8_t)(255.999 * g);
            pixels[index + 2] = (uint8_t)(255.999 * b);
        }
    }

    // ---- 2. Write a PPM file (plain text version "P3") -------------------
    {
        std::ofstream ppm("images/ch03_gradient.ppm");
        ppm << "P3\n" << width << ' ' << height << "\n255\n";
        for (int i = 0; i < width * height; i++) {
            ppm << (int)pixels[i * 3 + 0] << ' '
                << (int)pixels[i * 3 + 1] << ' '
                << (int)pixels[i * 3 + 2] << '\n';
        }
        std::printf("Wrote images/ch03_gradient.ppm\n");
    }

    // ---- 3. Write a BMP file ------------------------------------------
    // BMP = 54 byte header + pixels stored bottom row first, as B,G,R,
    // with every row padded to a multiple of 4 bytes.
    {
        const int row_size = (width * 3 + 3) / 4 * 4;
        const uint32_t data_size = row_size * height;
        const uint32_t file_size = 54 + data_size;

        std::vector<uint8_t> header(54, 0);
        auto put32 = [&](int offset, uint32_t v) {    // little-endian: low byte first
            header[offset + 0] = v & 0xFF;
            header[offset + 1] = (v >> 8) & 0xFF;
            header[offset + 2] = (v >> 16) & 0xFF;
            header[offset + 3] = (v >> 24) & 0xFF;
        };
        header[0] = 'B'; header[1] = 'M';
        put32(2, file_size);
        put32(10, 54);          // pixel data starts after the 54 header bytes
        put32(14, 40);          // size of the "info" part of the header
        put32(18, width);
        put32(22, height);
        header[26] = 1;         // one color plane
        header[28] = 24;        // 24 bits per pixel
        put32(34, data_size);

        std::ofstream bmp("images/ch03_gradient.bmp", std::ios::binary);
        bmp.write((const char*)header.data(), header.size());
        std::vector<uint8_t> row(row_size, 0);
        for (int y = height - 1; y >= 0; y--) {       // bottom row first!
            for (int x = 0; x < width; x++) {
                int i = (y * width + x) * 3;
                row[x * 3 + 0] = pixels[i + 2];   // blue
                row[x * 3 + 1] = pixels[i + 1];   // green
                row[x * 3 + 2] = pixels[i + 0];   // red
            }
            bmp.write((const char*)row.data(), row_size);
        }
        std::printf("Wrote images/ch03_gradient.bmp  (double-click it!)\n");
    }
    return 0;
}
```

### Walk-through

* `std::filesystem::create_directories("images")` creates the output folder if needed.
* The double loop visits every pixel. `double(x) / (width - 1)` gives exactly 0.0 at the left column
  and exactly 1.0 at the right column. (Remember chapter 2: without `double(...)` this would be
  integer division and always 0!)
* The PPM part writes text with `<<`, the C++ stream operator.
* The BMP part builds the 54-byte header in a vector, then writes rows from the bottom up, swapping R
  and B.

---

## 6. Run it

```bat
run ch03_first_image
```

Output:

```
Wrote images/ch03_gradient.ppm
Wrote images/ch03_gradient.bmp  (double-click it!)
```

## 7. What you should see

![First gradient](../images/ch03_gradient.bmp)

> **Image description:** A 256 × 256 square filled with a smooth four-corner gradient. The
> **top-left** is a very dark navy blue (almost black: only the 25% blue). Moving **right** along
> the top, red increases, reaching a deep **pink-red** at the top-right. Moving **down** the left
> edge, green increases, reaching a bright **green** (slightly teal) at the bottom-left. The
> **bottom-right** is a vivid **yellow** (full red + full green). The middle is an olive/orange-brown
> mix. There are no visible steps or bands.

Also open `images/ch03_gradient.ppm` in Notepad. You'll see the P3 header followed by 65,536 lines
of three numbers. That's your image, as text.

---

## 8. Understanding what you made

Some things to notice:

* **File sizes.** The BMP is 54 + 256×256×3 = **196,662 bytes**. The text PPM is about **650 KB**,
  because each number takes 1–3 characters plus spaces. In chapters 6–7 we'll write PNG files that
  are much smaller.
* **Gradients are just linear formulas.** Red = x/255. Nothing more.
* **The image is upside-down-safe.** We compute top-down (row 0 = top), and the BMP writer handles
  the flip. Always be clear about which way your y axis points! (In 3D, later, y will point *up*.)

---

## Try it yourself

1. **Swap the channels**: make blue go left→right and red top→bottom. Predict the corner colors
   before running.
2. **A radial gradient**: set all three channels to `1 - distance_from_center / 181` (181 ≈ the
   distance from the center to a corner). Clamp at 0. You should get a white glow in the middle.
3. **Stripes**: `double r = (x / 16) % 2;` gives vertical stripes 16 pixels wide. Why does this use
   integer division *on purpose*?
4. **Non-square image**: change width to 300 and height to 200 and check the BMP still opens. Try
   width 301. Does the padding code still work?
5. **Break it on purpose**: remove the row padding and use width 301. What happens to the image?
   (Every row gets shifted a little, so the image looks *sheared* diagonally. It's a classic
   bug, and now you'll recognize it.)

## Common problems

| Symptom | Cause |
|---------|-------|
| The image is black | Integer division: `x / (width - 1)` without `double(...)` |
| Red and blue are swapped in the BMP | You wrote RGB instead of BGR |
| The BMP is upside down | Rows written top-down instead of bottom-up |
| The image is diagonally sheared | Missing row padding when `width * 3` isn't a multiple of 4 |
| "File format not supported" | Header sizes or offsets wrong: check each byte in the table |

---

## Summary

* An image is a grid of pixels; each pixel is three numbers (R, G, B).
* We compute colors as 0–1 fractions and convert them to 0–255 bytes.
* PPM: tiny header + numbers. BMP: 54-byte header, little-endian numbers, bottom-up BGR rows padded
  to 4 bytes.
* *Color = f(x, y)* is the core idea of rendering.

Next: [Chapter 4 — Color for programmers →](04-color.md). Why is our gradient **not quite right**?
(Spoiler: gamma.)
