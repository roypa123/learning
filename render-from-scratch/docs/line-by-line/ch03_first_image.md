# Line by line: `ch03_first_image.cpp`

[← Line-by-line index](README.md) · [Chapter 3 (the theory)](../03-first-image.md)

This page explains **every line** of `chapters/ch03_first_image.cpp` in simple words.
Read it with the code file open next to it.

**What the whole program does, in one sentence:**
it makes a 256 × 256 picture where the color changes smoothly from corner to corner, and saves it in two
file formats (PPM and BMP).

The program has 5 blocks:

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments | 1–8 | Notes for humans. The computer ignores them. |
| B. Includes | 9–13 | Load ready-made tools from the C++ standard library. |
| C. Setup | 15–20 | Start the program, choose the image size, make a folder. |
| D. Compute the pixels | 22–37 | Calculate a color for every pixel and keep it in memory. |
| E. Save as PPM | 39–49 | Write the pixels into a text file. |
| F. Save as BMP | 51–89 | Write the pixels into a Windows picture file. |
| G. End | 90–91 | Finish the program. |

---

## Block A — Comments (lines 1–8)

```cpp
// ch03_first_image.cpp
// ------------------------------------------------------------
// Chapter 3: Your very first image. NO library, nothing hidden.
// We compute a color for every pixel and write it into two files:
//   images/ch03_gradient.ppm  (the simplest format in the world)
//   images/ch03_gradient.bmp  (opens in Windows Photos / Paint)
// Build & run:   run ch03_first_image
// ------------------------------------------------------------
```

**What this block does:** nothing for the computer. It's a note for you.

* `//` means "comment". Everything after `//` on that line is ignored by the compiler.
* The note says: the file name, what the program does, which two files it creates, and how to run it.
* The `-----` lines are only decoration, to make the note easy to see.

> **Tip:** Good programmers write comments to explain *why* code exists. You'll see many comments in
> this book.

---

## Block B — Includes (lines 9–13)

```cpp
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>
```

**What this block does:** C++ itself is small. Most useful tools live in the **standard library**, which
comes with every compiler. `#include` says "please load this tool box before compiling my code".

| Line | Tool box | What we use from it |
|------|----------|---------------------|
| 9 `#include <cstdio>` | C standard input/output | `std::printf` to print messages on the screen |
| 10 `#include <cstdint>` | exact-size number types | `uint8_t` (a number 0–255, exactly one byte) and `uint32_t` (a 4-byte number) |
| 11 `#include <fstream>` | **f**ile **stream**s | `std::ofstream` to write files |
| 12 `#include <vector>` | growable arrays | `std::vector` to store many pixels |
| 13 `#include <filesystem>` | folders and paths | `std::filesystem::create_directories` to make the `images` folder |

* `#include` lines start with `#`. They are handled *before* real compiling, by a step called the
  **preprocessor**. It literally copies the tool box's code into your file.
* `< >` means "a standard tool box". Later chapters use `" "` for **our own** files, like
  `#include "pixel/pixel.h"`.

---

## Block C — Setup (lines 15–20)

### Line 15

```cpp
int main() {
```

* Every C++ program starts running at a function called **`main`**. When you double-click the `.exe`,
  Windows calls `main`.
* `int` means `main` gives back a whole number when it finishes (see line 90).
* `()` means `main` takes no inputs here.
* `{` opens the **body** of `main`. Everything until the matching `}` on line 91 is inside `main`.

### Lines 16–17

```cpp
    const int width = 256;
    const int height = 256;
```

* We create two **variables**, named boxes that hold a value.
* `int` = the box holds a whole number.
* `width = 256` = the picture is 256 pixels wide. `height = 256` = 256 pixels tall.
* `const` = "constant": this value can never change later. If we try to change it by mistake, the compiler
  stops us. That protects us from bugs.
* `;` ends each statement (like a full stop at the end of a sentence).

**Why 256?** Because then the red value goes 0, 1, 2 … 255 from left to right: exactly one step per pixel.
It makes the result easy to understand.

### Lines 19–20

```cpp
    // Make the "images" folder if it does not exist yet.
    std::filesystem::create_directories("images");
```

* Line 19 is a comment.
* Line 20 creates a folder called `images` in the **current folder** (the folder you ran the program from).
* If the folder already exists, it does nothing, with no error.
* We need it because we are going to save files into `images/`. Without the folder, saving would fail.
* `std::` means "from the standard library". `filesystem::` is the part of it that handles folders.

---

## Block D — Compute the pixels (lines 22–37)

**What this block does:** it goes to every pixel, one by one, calculates its color, and writes 3 numbers
(red, green, blue) into a long list in memory. Nothing is saved to a file yet.

### Lines 22–24: make room for all pixels

```cpp
    // ---- 1. Compute every pixel and keep it in memory -------------------
    // We store 3 bytes per pixel: red, green, blue (0..255 each).
    std::vector<uint8_t> pixels(width * height * 3);
```

* Lines 22–23 are comments.
* Line 24 creates a **vector** (a list) called `pixels`.
* `<uint8_t>` = each element of the list is one byte, a number from 0 to 255.
* `(width * height * 3)` = how many elements: 256 × 256 pixels × 3 colors = **196,608** bytes.
* All elements start as 0 (black).

How the list is organized: first pixel's red, green, blue, then the second pixel's red, green, blue, and so on,
row by row from the top:

```
index:   0   1   2   3   4   5   6   7   8  ...
value:   R   G   B   R   G   B   R   G   B  ...
        └─ pixel 0 ─┘└─ pixel 1 ─┘└─ pixel 2 ─┘
```

### Line 26: loop over the rows

```cpp
    for (int y = 0; y < height; y++) {          // rows, top to bottom
```

A **for loop** repeats code. Read it as three parts separated by `;`:

| Part | Meaning |
|------|---------|
| `int y = 0` | start with a counter `y` = 0 (the top row) |
| `y < height` | keep going while `y` is less than 256 |
| `y++` | after each round, add 1 to `y` |

So the code inside runs for y = 0, 1, 2, … 255: once for **every row**.

### Line 27: loop over the columns

```cpp
        for (int x = 0; x < width; x++) {       // columns, left to right
```

The same idea for `x` (the column). This loop is **inside** the row loop, so for **each** row it goes
through **all** 256 columns. Together, the two loops visit all 256 × 256 = 65,536 pixels:

```
(0,0) (1,0) (2,0) ... (255,0)      <- first y = 0: x goes 0..255
(0,1) (1,1) (2,1) ... (255,1)      <- then y = 1: x goes 0..255 again
...
(0,255) ...           (255,255)    <- last row
```

### Line 28: the red amount

```cpp
            double r = double(x) / (width - 1);   // 0.0 at left  -> 1.0 at right
```

* `double` = a number that can have a fraction, like 0.5 or 0.25.
* `r` = how much red this pixel has, from **0.0** (none) to **1.0** (full).
* `width - 1` = 255.
* So `r = x / 255`: at the left edge x = 0 → r = 0.0; at the right edge x = 255 → r = 1.0; in the middle
  x = 127 → r ≈ 0.5.
* `double(x)` turns `x` from a whole number into a fraction number **before** dividing. This matters!
  In C++, whole ÷ whole throws away the fraction: `127 / 255` would be **0**, and the whole image would be
  black. With `double(x)` we get `0.498...`.

### Line 29: the green amount

```cpp
            double g = double(y) / (height - 1);  // 0.0 at top   -> 1.0 at bottom
```

The same thing, but using the row `y`: green is 0.0 at the top and 1.0 at the bottom.

### Line 30: the blue amount

```cpp
            double b = 0.25;                      // a little blue everywhere
```

Blue is the same (25%) for every pixel. It just makes the colors a bit nicer.

**Result of lines 28–30:** each corner gets a different color:

| Corner | r | g | b | Looks like |
|--------|---|---|---|------------|
| top-left | 0 | 0 | 0.25 | dark blue |
| top-right | 1 | 0 | 0.25 | pink-red |
| bottom-left | 0 | 1 | 0.25 | green |
| bottom-right | 1 | 1 | 0.25 | yellow |

### Line 32: find where this pixel lives in the list

```cpp
            int index = (y * width + x) * 3;      // where this pixel lives in the array
```

* `y * width + x` = the pixel's number, counting row by row. Example: pixel (x = 2, y = 1) is number
  `1 × 256 + 2 = 258` (a whole row of 256 pixels comes before it, then 2 more).
* `* 3` because each pixel uses 3 places in the list. So pixel 258 starts at index 774.
* `index` = the position of this pixel's **red** value. Green is at `index + 1`, blue at `index + 2`.

### Lines 33–35: store the three bytes

```cpp
            pixels[index + 0] = (uint8_t)(255.999 * r);
            pixels[index + 1] = (uint8_t)(255.999 * g);
            pixels[index + 2] = (uint8_t)(255.999 * b);
```

* `pixels[index + 0]` = the box in the list where red goes. `[ ]` means "the element at this position".
* `255.999 * r` turns 0.0–1.0 into 0–255.999.
* `(uint8_t)( ... )` converts it into a byte. The conversion **cuts off** the fraction (it doesn't round):
  127.9 becomes 127, and 255.999 becomes 255.
* **Why 255.999 and not 255?** With 255, only a perfect 1.0 would give 255. With 255.999, the top
  slice of values also reaches 255, so all 256 byte values are used equally.
* Example: r = 0.5 → 127.9995 → **127**. b = 0.25 → 63.99975 → **63**.

### Lines 36–37: close the loops

```cpp
        }
    }
```

* Line 36 `}` ends the **column** loop (from line 27).
* Line 37 `}` ends the **row** loop (from line 26).

When the program reaches line 39, all 65,536 pixels are computed and stored in `pixels`.

---

## Block E — Save as PPM (lines 39–49)

**What this block does:** writes the pixels into a **text** file in the PPM format. PPM is the
simplest image format there is: a short header, then the numbers.

### Lines 39–40

```cpp
    // ---- 2. Write a PPM file (plain text version "P3") -------------------
    {
```

* Line 39 is a comment.
* Line 40 `{` opens a **block**. Variables created inside a block disappear at its closing `}` (line 49).
  We use it so the file is **closed automatically** at the end of the block (see line 49).

### Line 41: open the file

```cpp
        std::ofstream ppm("images/ch03_gradient.ppm");
```

* `std::ofstream` = an **o**utput **f**ile **stream**: something you can write into, which goes to a file.
* `ppm` = the name we give it.
* `"images/ch03_gradient.ppm"` = the file to create. If it already exists, it is replaced.

### Line 42: the header

```cpp
        ppm << "P3\n" << width << ' ' << height << "\n255\n";
```

* `<<` means "write this into the file". You can chain several `<<`.
* `"P3\n"` = the magic code for "text PPM", then `\n` = new line.
* `width << ' ' << height` = `256 256` (with a space between).
* `"\n255\n"` = a new line, `255` (the largest color value), and another new line.

So the file now begins with:

```
P3
256 256
255
```

### Lines 43–47: one line per pixel

```cpp
        for (int i = 0; i < width * height; i++) {
            ppm << (int)pixels[i * 3 + 0] << ' '
                << (int)pixels[i * 3 + 1] << ' '
                << (int)pixels[i * 3 + 2] << '\n';
        }
```

* Line 43: a loop with counter `i` from 0 to 65,535, once for every pixel (this time we count pixels
  directly instead of using x and y).
* `i * 3 + 0`, `i * 3 + 1`, `i * 3 + 2` = the positions of this pixel's red, green, blue (same idea as line 32).
* `(int)` is important: a `uint8_t` is treated like a **character** when written to a stream. Without `(int)`,
  the value 65 would be written as the letter `A`! `(int)` makes it write the number `65`.
* `' '` puts spaces between the three numbers, and `'\n'` ends the line.
* The statement is split over 3 lines only to make it easier to read; it is one single statement ending at the `;`.

The file now continues like this:

```
0 0 63
1 0 63
2 0 63
...
```

### Line 48: tell the user

```cpp
        std::printf("Wrote images/ch03_gradient.ppm\n");
```

Prints a message in the terminal so you know it worked.

### Line 49: close the block

```cpp
    }
```

The block from line 40 ends. `ppm` is destroyed here, and when a file stream is destroyed it **closes the
file automatically** and makes sure everything is written to disk.

---

## Block F — Save as BMP (lines 51–89)

**What this block does:** writes the same pixels as a **BMP** file, which Windows can open with a
double-click. BMP has a 54-byte header, then the pixels, but with three strange rules:

1. rows are stored **from the bottom** row to the top row,
2. each pixel is stored as **Blue, Green, Red** (not RGB),
3. each row must have a length that is a **multiple of 4** bytes (extra zero bytes are added if needed).

### Lines 51–54

```cpp
    // ---- 3. Write a BMP file ------------------------------------------
    // BMP = 54 byte header + pixels stored bottom row first, as B,G,R,
    // with every row padded to a multiple of 4 bytes.
    {
```

Comments explaining the rules above, then a new block `{`.

### Line 55: how long is one row in the file?

```cpp
        const int row_size = (width * 3 + 3) / 4 * 4;
```

* One row has `width * 3` bytes (3 per pixel) = 768.
* The formula **rounds up to the next multiple of 4**:
  * `+ 3` then `/ 4` (whole-number division, which drops the fraction) then `* 4`.
  * Example with width 5: 5 × 3 = 15 → (15 + 3) = 18 → 18 / 4 = 4 → 4 × 4 = **16**. So 1 padding byte.
  * With width 256: 768 → 771 / 4 = 192 → 192 × 4 = **768**. No padding needed.

### Lines 56–57: sizes for the header

```cpp
        const uint32_t data_size = row_size * height;
        const uint32_t file_size = 54 + data_size;
```

* `data_size` = the size of all pixel rows: 768 × 256 = 196,608 bytes.
* `file_size` = header (54) + pixels = 196,662 bytes.
* `uint32_t` = a whole number stored in exactly 4 bytes, which is what the BMP header uses.

### Line 59: an empty header

```cpp
        std::vector<uint8_t> header(54, 0);
```

A list of 54 bytes, all set to 0. We'll fill in the important ones. The rest stay 0.

### Lines 60–65: a little helper to write 4-byte numbers

```cpp
        auto put32 = [&](int offset, uint32_t v) {    // little-endian: low byte first
            header[offset + 0] = v & 0xFF;
            header[offset + 1] = (v >> 8) & 0xFF;
            header[offset + 2] = (v >> 16) & 0xFF;
            header[offset + 3] = (v >> 24) & 0xFF;
        };
```

This creates a small function (a **lambda**) called `put32`. It writes the number `v` into 4 bytes of the
header, starting at position `offset`.

* `auto put32 =` : store the little function in a variable called `put32`.
* `[&]` : the function is allowed to use (and change) variables from outside, here the `header`.
* `(int offset, uint32_t v)` : its two inputs.
* A big number needs 4 bytes. BMP stores them **smallest byte first** ("little-endian").
  * `v & 0xFF` = keep only the lowest 8 bits (the lowest byte). `0xFF` is 255 written in hexadecimal.
  * `v >> 8` = shift the number 8 bits to the right (like dividing by 256), so the second byte becomes the
    lowest; then `& 0xFF` takes it.
  * `>> 16` and `>> 24` do the same for the third and fourth bytes.
* Example: `v = 196662` = hex `0x00030036` → bytes `36 00 03 00`.
* Line 65 `};` ends the lambda (the `;` is needed because it is a variable definition).

### Lines 66–74: fill in the header

```cpp
        header[0] = 'B'; header[1] = 'M';
        put32(2, file_size);
        put32(10, 54);          // pixel data starts after the 54 header bytes
        put32(14, 40);          // size of the "info" part of the header
        put32(18, width);
        put32(22, height);
        header[26] = 1;         // one color plane
        header[28] = 24;        // 24 bits per pixel
        put32(34, data_size);
```

Each line puts one piece of information at the exact position the BMP format expects:

| Line | Position | Value | Meaning |
|------|----------|-------|---------|
| 66 | 0–1 | `'B' 'M'` | the "signature": this is a BMP file |
| 67 | 2–5 | file_size | total size of the file |
| 68 | 10–13 | 54 | the pixels start at byte 54 |
| 69 | 14–17 | 40 | the second part of the header is 40 bytes long |
| 70 | 18–21 | 256 | image width |
| 71 | 22–25 | 256 | image height (positive = rows go bottom to top) |
| 72 | 26 | 1 | number of color planes (always 1) |
| 73 | 28 | 24 | bits per pixel: 24 = 3 bytes (B, G, R) |
| 74 | 34–37 | data_size | size of the pixel data |

All other header bytes stay 0, which means "no compression" and "default resolution". That's fine.

### Line 76: open the file in binary mode

```cpp
        std::ofstream bmp("images/ch03_gradient.bmp", std::ios::binary);
```

* Same as line 41, but with `std::ios::binary`.
* **Binary** mode means "write the bytes exactly as they are". Without it, Windows would change every
  byte 10 (new line) into two bytes (13, 10), which would damage the picture.

### Line 77: write the header

```cpp
        bmp.write((const char*)header.data(), header.size());
```

* `bmp.write(address, count)` writes `count` raw bytes.
* `header.data()` = the address in memory where the header's bytes are.
* `(const char*)` changes the type so that `write` accepts it (it expects "characters"; for it,
  bytes are characters).
* `header.size()` = 54.

### Line 78: a buffer for one row

```cpp
        std::vector<uint8_t> row(row_size, 0);
```

A list big enough for one row of the file (768 bytes), all zeros. The zeros also serve as the padding
bytes when padding is needed.

### Line 79: loop over rows from the bottom

```cpp
        for (int y = height - 1; y >= 0; y--) {       // bottom row first!
```

* Start at `y = 255` (the bottom row).
* Keep going while `y >= 0`.
* `y--` = subtract 1 each time.
* So rows are visited 255, 254, … 0: **bottom to top**, as BMP requires.

### Lines 80–85: fill the row buffer, swapping colors

```cpp
            for (int x = 0; x < width; x++) {
                int i = (y * width + x) * 3;
                row[x * 3 + 0] = pixels[i + 2];   // blue
                row[x * 3 + 1] = pixels[i + 1];   // green
                row[x * 3 + 2] = pixels[i + 0];   // red
            }
```

* Line 80: for every column `x` in this row.
* Line 81: `i` = where pixel (x, y) is in our `pixels` list (same formula as line 32).
* Lines 82–84: copy the three bytes into the row buffer in **B, G, R** order:
  * position 0 of the pixel in the row ← our **blue** (`i + 2`),
  * position 1 ← **green** (`i + 1`),
  * position 2 ← **red** (`i + 0`).
* Line 85: end of the column loop.

### Line 86: write the row

```cpp
            bmp.write((const char*)row.data(), row_size);
```

Writes the whole row buffer (`row_size` bytes, which includes any padding) to the file.

### Lines 87–89

```cpp
        }
        std::printf("Wrote images/ch03_gradient.bmp  (double-click it!)\n");
    }
```

* Line 87: end of the row loop. All 256 rows are written.
* Line 88: print a message.
* Line 89: end of the block, so `bmp` is destroyed and the file is closed.

---

## Block G — End (lines 90–91)

```cpp
    return 0;
}
```

* `return 0;` ends `main` and gives the number 0 back to Windows. By tradition, **0 means "success"**.
* `}` closes `main` (opened on line 15). The program is finished.

---

## The whole program as a picture

```
start (main)
  │
  ├─ size = 256 x 256, create folder "images"
  │
  ├─ for every row y (top → bottom)
  │     for every column x (left → right)
  │         red = x/255, green = y/255, blue = 0.25
  │         store 3 bytes in "pixels"
  │
  ├─ PPM file:  "P3 256 256 255" + one text line "R G B" per pixel
  │
  ├─ BMP file:  54-byte header
  │             + rows from bottom to top, each pixel as B G R (+ padding)
  │
  └─ return 0 (success)
```

## Check your understanding

1. What would happen if line 28 were `double r = x / (width - 1);` (without `double(...)`)?
   *(Answer: whole-number division, so r = 0 for every pixel except the last column. The image would be
   almost no red.)*
2. Why does line 44 use `(int)`? *(So the byte is written as a number, not as a letter.)*
3. Why does line 79 count **down**? *(BMP stores the bottom row first.)*
4. Why is blue at `i + 2` on line 82? *(BMP wants B, G, R order; our list is R, G, B.)*
5. What does `std::ios::binary` protect against? *(Windows changing new-line bytes inside the picture.)*
