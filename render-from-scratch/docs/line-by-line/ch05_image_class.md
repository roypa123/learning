# Line by line: `ch05_image_class.cpp`

[← Line-by-line index](README.md) · [Chapter 5 (the theory)](../05-image-class.md) · [image.h explained](image.md)

**What the whole program does, in one sentence:** it practices everything the `Image` class can do: draw a
checkerboard, save it, load it back, flip and invert it, and enlarge a tiny image in two ways.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | notes and our library |
| B. Create a checkerboard | 13–22 | a gradient with dark squares |
| C. Save | 24–26 | as PPM and as BMP |
| D. Load, flip, invert, save | 28–42 | read the PPM back and change it |
| E. Make a tiny 4×4 image | 44–49 | 16 colored pixels |
| F. Enlarge it two ways | 51–60 | nearest vs bilinear |
| G. End | 61–62 | |

---

## Block A — Comments, includes (lines 1–11)

* Lines 1–8: the four things this program does.
* Line 9: math tools. Line 10: our library. Line 11: use `pixel` names without the prefix.
  (Same as [ch04 block B](ch04_color.md).)

---

## Block B — Create a checkerboard (lines 13–22)

```cpp
int main() {
    // ---------- 1. Create --------------------------------------------------
    Image img(320, 240);
```

* Line 13: the program starts.
* Line 15: a new 320 × 240 image, all black.

```cpp
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
```

* Lines 16–17: every pixel. We use `img.height` and `img.width` instead of typing 240 and 320 again, so if
  we change the size on line 15, the loops follow automatically.

```cpp
            bool dark_square = ((x / 40) + (y / 40)) % 2 == 0;
```

Line 18 decides whether this pixel is in a **dark** square of the checkerboard:

* `x / 40` is **whole-number division on purpose**: pixels 0–39 give 0, 40–79 give 1, and so on. It's the
  column number of the 40-pixel square.
* `y / 40` is the row number of the square.
* `% 2` = the remainder after dividing by 2: 0 for even numbers, 1 for odd.
* `== 0` → true when (column + row) is even.
* `bool` = true or false.

```
 square column:  0   1   2   3 ...
 row 0:         dark  .  dark  .
 row 1:          .  dark  .  dark        (column + row even → dark)
```

```cpp
            double u = (double)x / (img.width - 1);
```

Line 19: `u` goes from 0.0 (left) to 1.0 (right).

```cpp
            Color base = lerp(hex_color(0x2E86DE), hex_color(0xF368E0), u);  // blue -> pink
```

Line 20: the base color blends from **blue** (`#2E86DE`) on the left to **pink** (`#F368E0`) on the right.
`hex_color` gives linear colors, and `lerp` blends them by `u` (see [vec3.md](vec3.md) line 92).

```cpp
            img.at(x, y) = dark_square ? base * 0.35 : base;
        }
```

* Line 21: if it's a dark square, use 35% of the light (`base * 0.35`); otherwise use the base color.
  Store it in pixel (x, y).
* Line 22: end of the loop body.

---

## Block C — Save (lines 24–26)

```cpp
    save_image("images/ch05_checker.ppm", img);
    save_image("images/ch05_checker.bmp", img);
```

The same image saved twice. `save_image` picks the format from the extension (`.ppm` → binary PPM, `.bmp` → BMP).
See [image.md block L](image.md).

---

## Block D — Load, flip, invert, save (lines 28–42)

```cpp
    Image loaded;
```

Line 29: an empty image (0 × 0). `read_ppm` will fill it.

```cpp
    if (!read_ppm("images/ch05_checker.ppm", loaded)) {
        std::printf("Could not read the PPM back!\n");
        return 1;
    }
```

* Line 30: read the file we just saved into `loaded`. `read_ppm` returns true on success; `!` means "not", so
  the `if` is entered only on **failure**.
* Line 31: explain the problem.
* Line 32: `return 1;` stops the program. A non-zero number tells Windows "something went wrong".

```cpp
    Image changed(loaded.width, loaded.height);
```

Line 34: a new image with the same size, for the result.

```cpp
    for (int y = 0; y < loaded.height; y++)
        for (int x = 0; x < loaded.width; x++) {
            Color c = loaded.at(x, loaded.height - 1 - y);        // upside down
```

* Lines 35–36: every pixel of the result.
* Line 37: read from the **mirrored row**: for y = 0 (top) we read the last row, for the last row we read row 0.
  `loaded.height - 1 - y` does that. So the result is **upside down**.

```cpp
            Color display = linear_to_srgb(c);                    // invert what we SEE
            Color inverted = Color(1, 1, 1) - display;
            changed.at(x, y) = srgb_to_linear(inverted);
        }
```

* Line 38: convert to what the screen shows (sRGB, 0–1).
* Line 39: invert: `1 − value` for each channel (white ↔ black, blue ↔ orange...). This is what "invert colors"
  does in a paint program, which works on screen values. That's why we converted first.
* Line 40: convert back to linear and store.
* Line 41: end of loop.

```cpp
    save_image("images/ch05_inverted.bmp", changed);
```

Line 42: save the flipped, inverted image.

---

## Block E — A tiny 4×4 image (lines 44–49)

```cpp
    Image tiny(4, 4);
    Color palette[4] = { hex_color(0xFF6B6B), hex_color(0xFECA57), hex_color(0x48DBFB), hex_color(0x1DD1A1) };
```

* Line 45: a 4 × 4 image (only 16 pixels).
* Line 46: an array of 4 colors: coral red, yellow, sky blue, green.

```cpp
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            tiny.at(x, y) = palette[(x + 2 * y) % 4];
```

* Lines 47–49: each pixel gets one of the 4 colors. `(x + 2 * y) % 4` gives a number 0–3 that changes across
  and down, making a small pattern:

```
 y=0:  0 1 2 3
 y=1:  2 3 0 1
 y=2:  0 1 2 3
 y=3:  2 3 0 1
```

---

## Block F — Enlarge it two ways (lines 51–60)

```cpp
    Image big_nearest(256, 256), big_bilinear(256, 256);
```

Line 51: two big 256 × 256 images for the results.

```cpp
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            double u = (x + 0.5) / 256.0;
            double v = 1.0 - (y + 0.5) / 256.0;   // v = 0 is the bottom
```

* Lines 52–53: every pixel of the big images.
* Line 54: `u` = the horizontal position of this pixel's **center**, as a fraction 0–1.
* Line 55: `v` = the vertical position, but flipped (`1.0 - ...`) because in texture coordinates v = 0 is the
  **bottom**, while y = 0 is the top row. The sampling functions flip it back, so the picture stays upright.

```cpp
            big_nearest.at(x, y) = tiny.sample_nearest(u, v);
            big_bilinear.at(x, y) = tiny.sample_bilinear(u, v);
        }
```

* Line 56: "which tiny pixel is at this position?" Each tiny pixel covers a 64 × 64 block → big squares.
* Line 57: "blend the 4 nearest tiny pixels" → smooth gradients between the colors.
  (How both work: [image.md block D](image.md).)

```cpp
    save_image("images/ch05_nearest.bmp", big_nearest);
    save_image("images/ch05_bilinear.bmp", big_bilinear);
    return 0;
}
```

Lines 59–60: save both. Lines 61–62: success, end of program.

---

## The whole program as a picture

```
create 320x240: gradient blue→pink, dark squares where (col + row) is even
   └─ save .ppm and .bmp
read the .ppm back
   └─ for each pixel: take the mirrored row, invert (in screen colors) → save
make 4x4 pattern
   └─ enlarge to 256x256 with nearest (blocky) and bilinear (smooth) → save both
```

## Check your understanding

1. What does `(x / 40) % 2` give for x = 85? *(85 / 40 = 2, 2 % 2 = 0)*
2. Why does line 32 return 1? *(To signal an error to whoever started the program.)*
3. Why convert to sRGB before inverting? *(Invert should work on what we **see**, like in a paint program.)*
4. What does line 37 do to the picture? *(Flips it upside down.)*
