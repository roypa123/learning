# Line by line: `include/pixel/image.h`

[← Line-by-line index](README.md) · [Chapter 5 (the theory)](../05-image-class.md)

**What this file does, in one sentence:** it defines `Image` (a grid of colors in memory) and the
functions that **save** it as PPM, BMP or PNG, and **load** PPM files.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments and includes | 1–19 | tools we need, start of namespace |
| B. `struct Image`: data and creation | 21–28 | width, height, the list of pixels |
| C. `Image`: reading and writing pixels | 30–42 | `in_bounds`, `at`, `get_clamped`, `fill` |
| D. `Image`: sampling between pixels | 44–67 | `sample_bilinear`, `sample_nearest` |
| E. `SaveOptions` | 69–74 | how to convert light into screen colors |
| F. Converting to bytes | 76–96 | `encode_pixel`, `to_rgb8` |
| G. Making folders | 98–105 | `ensure_parent_folder` |
| H. PPM writing | 107–118 | `write_ppm` |
| I. PPM reading | 120–164 | `read_ppm` |
| J. BMP writing | 166–210 | `put_u16_le`, `put_u32_le`, `write_bmp` |
| K. PNG writing | 212–219 | `write_png` |
| L. `save_image` | 221–232 | pick the format from the file name |
| M. End | 234 | close namespace |

---

## Block A — Comments and includes (lines 1–19)

* Lines 1–6: comments.
* Line 7 `#pragma once`: include only once.
* Lines 8–14: standard tools:

| Line | Include | Used for |
|------|---------|----------|
| 8 | `<cmath>` | `std::floor` |
| 9 | `<cstdint>` | `uint8_t`, `uint32_t` |
| 10 | `<cstdio>` | `std::printf` |
| 11 | `<string>` | `std::string` (text like file names) |
| 12 | `<vector>` | `std::vector` |
| 13 | `<fstream>` | reading and writing files |
| 14 | `<filesystem>` | folders, file extensions |

* Lines 15–17: our own files: `vec3.h` (Color), `color.h` (sRGB, tone mapping, `to_byte`), `png.h` (the
  PNG encoder, chapter 7).
* Line 19: start of `namespace pixel`.

---

## Block B — `struct Image`: data and creation (lines 21–28)

```cpp
struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> data;   // row by row, top row first
```

* Line 21: a new type `Image`.
* Lines 22–23: its size in pixels. They start at 0 (an "empty" image).
* Line 24: `data` = a list of `Color`s, **one per pixel**, stored row by row from the top row. Each Color is 3
  `double`s of **linear** light (not bytes!), so values can be fractions and even above 1.

```cpp
    Image() {}
    Image(int w, int h, const Color& fill_color = Color(0, 0, 0))
        : width(w), height(h), data((size_t)w * h, fill_color) {}
```

* Line 26: the empty constructor: `Image img;` gives a 0 × 0 image (used when we'll fill it in later, for
  example by loading a file).
* Line 27: the normal constructor: `Image img(400, 300);` or `Image img(400, 300, Color(1,1,1));`.
  * `const Color& fill_color = Color(0, 0, 0)`: the third input is optional. If you don't give it, it's black.
* Line 28: the initializer list:
  * `width(w), height(h)` store the size.
  * `data((size_t)w * h, fill_color)` creates the vector with `w × h` elements, **all equal** to `fill_color`.
  * `(size_t)` makes the multiplication use a big number type, so huge images don't overflow.
  * `{}` = nothing else to do.

---

## Block C — Reading and writing pixels (lines 30–42)

### Line 30: is (x, y) inside the image?

```cpp
    bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
```

`true` only if x is between 0 and width−1 **and** y is between 0 and height−1. Use it before touching a
pixel you're not sure about.

### Lines 32–33: get a pixel

```cpp
    Color& at(int x, int y) { return data[(size_t)y * width + x]; }
    const Color& at(int x, int y) const { return data[(size_t)y * width + x]; }
```

* The famous formula `y * width + x` finds pixel (x, y) in the row-by-row list (see chapter 2).
* Line 32 returns `Color&`, a **reference** to the real pixel. So `img.at(3, 4) = red;` changes the image.
* Line 33 is the same for a `const` image (read-only). C++ picks this one automatically when the image can't be
  changed.
* **Warning:** `at` does not check the range, for speed. A wrong x or y can crash the program.

### Lines 35–40: get a pixel, but safely clamp the position

```cpp
    // Like at(), but coordinates outside the image are clamped to the edge.
    Color get_clamped(int x, int y) const {
        x = x < 0 ? 0 : (x >= width ? width - 1 : x);
        y = y < 0 ? 0 : (y >= height ? height - 1 : y);
        return at(x, y);
    }
```

* Line 37: if x is too small, use 0; if too big, use the last column; otherwise keep x.
* Line 38: the same for y.
* Line 39: now it's safe to call `at`.
* Example: in a 10-wide image, `get_clamped(-3, 5)` returns pixel (0, 5), and `get_clamped(12, 5)` returns
  pixel (9, 5). Blur and sampling use this so they never read outside the image.

### Line 42: fill everything

```cpp
    void fill(const Color& c) { for (auto& p : data) p = c; }
```

* `void` = returns nothing.
* `for (auto& p : data)`: for each pixel `p` in the list (`&` = the real one, not a copy).
* `p = c`: set it to color c.

---

## Block D — Sampling between pixels (lines 44–67)

Textures (chapter 24) ask for the color at a position like u = 0.37, v = 0.81, which usually falls **between**
pixel centers. These two functions answer that.

`u` goes left → right from 0 to 1. `v` goes **bottom → top** from 0 to 1 (the usual rule for textures, the
opposite of image rows).

### Lines 47–58: bilinear (smooth)

```cpp
    Color sample_bilinear(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);   // magenta = "missing"
```

* Line 48: if the image is empty, return **magenta** (bright pink-purple). In graphics, magenta traditionally
  means "texture is missing". You'll notice it immediately.

```cpp
        u = u - std::floor(u);          // wrap around (tiling)
        v = v - std::floor(v);
```

* Lines 49–50: keep only the fractional part. `std::floor(1.25)` = 1, so 1.25 → 0.25. `floor(-0.25)` = −1, so
  −0.25 → 0.75. This makes the texture **repeat** (tile) forever.

```cpp
        double fx = u * width - 0.5;
        double fy = (1.0 - v) * height - 0.5;
```

* Line 51: the position in **pixel units** across. `- 0.5` because pixel centers sit at 0.5, 1.5, 2.5 ...
  So `fx = 0` means "exactly at the center of pixel 0".
* Line 52: the same downwards. `1.0 - v` flips v (v = 0 is the bottom, but row 0 is the top).

```cpp
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        double tx = fx - x0, ty = fy - y0;
```

* Line 53: `x0`, `y0` = the pixel to the upper-left of our position.
* Line 54: `tx`, `ty` = how far (0–1) we are from that pixel toward the next one, in x and in y.

```cpp
        Color c00 = get_clamped(x0, y0),     c10 = get_clamped(x0 + 1, y0);
        Color c01 = get_clamped(x0, y0 + 1), c11 = get_clamped(x0 + 1, y0 + 1);
```

* Lines 55–56: read the 4 surrounding pixels: `c00` upper-left, `c10` upper-right, `c01` lower-left,
  `c11` lower-right. `get_clamped` keeps us safe at the edges.

```cpp
        return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
    }
```

* Line 57: blend in two steps:
  1. `lerp(c00, c10, tx)`: blend the top two pixels left→right,
  2. `lerp(c01, c11, tx)`: blend the bottom two,
  3. blend those two results top→bottom with `ty`.
* The result changes smoothly as u and v change: no blocky pixels.

```
 c00 ●──────────● c10
     │    ×     │      × = our position; tx = how far right, ty = how far down
 c01 ●──────────● c11
```

### Lines 60–66: nearest (blocky, fast)

```cpp
    Color sample_nearest(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);
        u = u - std::floor(u);
        v = v - std::floor(v);
        int x = (int)(u * width), y = (int)((1.0 - v) * height);
        return get_clamped(x, y);
    }
```

* Lines 61–63: the same checks and wrapping as above.
* Line 64: simply find the pixel that contains the position (no blending). `(int)` drops the fraction.
* Line 65: return that pixel. When enlarged, the image looks like big squares (pixel art).

### Line 67

```cpp
};
```

End of `struct Image`.

---

## Block E — `SaveOptions` (lines 69–74)

```cpp
// How to turn linear HDR colors into 8-bit screen colors.
struct SaveOptions {
    double exposure = 1.0;                  // multiply light before tone mapping
    ToneMapper tonemap = ToneMapper::Clamp;
    bool srgb = true;                       // apply the sRGB gamma curve
};
```

A small bundle of settings for saving:

* `exposure`: multiply all light by this first (2.0 = twice as bright).
* `tonemap`: how to squeeze values above 1 (see [color.md](color.md), block F). Default: just cut (clamp).
* `srgb`: apply the sRGB curve (true, normally) or not.

Usage: `SaveOptions opt; opt.tonemap = ToneMapper::Aces; save_image("x.png", img, opt);`

---

## Block F — Converting to bytes (lines 76–96)

### Lines 76–85: one pixel

```cpp
inline Color encode_pixel(const Color& linear, const SaveOptions& opt) {
    Color c = linear * opt.exposure;
```

* Line 76: takes one linear color plus the options, and returns the color ready for the screen (0–1).
* Line 77: step 1, exposure: multiply.

```cpp
    // NaN protection: a NaN is never equal to itself.
    if (c.x != c.x) c.x = 0;
    if (c.y != c.y) c.y = 0;
    if (c.z != c.z) c.z = 0;
```

* **NaN** = "Not a Number". It appears after impossible math, like 0 ÷ 0 or the square root of −1.
* A NaN has one strange property: it is **not equal to itself**. So `c.x != c.x` is true only for NaN.
* Lines 79–81: replace any NaN with 0, so one bad pixel can't produce garbage in the file.

```cpp
    c = apply_tonemap(c, opt.tonemap);
    if (opt.srgb) c = linear_to_srgb(c);
    return c;
}
```

* Line 82: step 2, tone mapping (now the values are 0–1).
* Line 83: step 3, the sRGB curve, if enabled.
* Line 84: return the finished color.

### Lines 87–96: the whole image

```cpp
inline std::vector<uint8_t> to_rgb8(const Image& img, const SaveOptions& opt = SaveOptions()) {
    std::vector<uint8_t> bytes((size_t)img.width * img.height * 3);
```

* Line 87: returns a list of bytes: R, G, B, R, G, B, ... for the whole image. `opt` is optional (default settings).
* Line 88: make room: 3 bytes per pixel.

```cpp
    for (size_t i = 0; i < img.data.size(); i++) {
        Color c = encode_pixel(img.data[i], opt);
        bytes[i * 3 + 0] = to_byte(c.x);
        bytes[i * 3 + 1] = to_byte(c.y);
        bytes[i * 3 + 2] = to_byte(c.z);
    }
    return bytes;
}
```

* Line 89: for every pixel `i` (`size_t` is a big whole-number type, used for sizes).
* Line 90: encode it (exposure, tone map, sRGB).
* Lines 91–93: convert each channel to a byte 0–255 (rounded) and store it in the right place.
* Line 95: give back the full list.

---

## Block G — Making folders (lines 98–105)

```cpp
inline void ensure_parent_folder(const std::string& filename) {
    std::filesystem::path p(filename);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
}
```

* Line 100: turn the file name into a `path` object, which understands folders.
* Line 101: does the name include a folder? `"images/a.png"` → yes (`images`); `"a.png"` → no.
* Line 102: a place to store an error, if any (instead of crashing).
* Line 103: create the folder (and any parent folders). If it already exists, nothing happens.

---

## Block H — PPM writing (lines 107–118)

```cpp
inline bool write_ppm(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::ofstream f(filename, std::ios::binary);
    if (!f) return false;
```

* Lines 109–110: returns `true` if saving worked. The definition is split over two lines just because it's long.
* Line 111: make sure the folder exists.
* Line 112: open the file for writing, in binary mode (bytes exactly as they are).
* Line 113: `if (!f)` = "if the file could not be opened" (for example, no permission). Then give up and
  return `false`.

```cpp
    f << "P6\n" << img.width << ' ' << img.height << "\n255\n";
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}
```

* Line 114: the header as text: `P6` (= **binary** PPM), width, height, 255.
* Line 115: get all pixel bytes.
* Line 116: write them all at once, as raw bytes. (P6 stores bytes, not text numbers like the P3 file in
  chapter 3. It's about 4 times smaller.)
* Line 117: `(bool)f` is true if no error happened while writing.

---

## Block I — PPM reading (lines 120–164)

**What this block does:** opens a PPM file (text P3 or binary P6), reads the header, then reads every pixel into
an `Image`, converting to linear light.

### Lines 121–123: open the file

```cpp
inline bool read_ppm(const std::string& filename, Image& out, bool file_is_srgb = true) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;
```

* Line 121: inputs: the file name, `out` = the image to fill (passed by reference `&`, so we can change the
  caller's image), and whether the file is sRGB (almost always true).
* Line 122: `std::ifstream` = an **input** file stream (for reading).
* Line 123: if the file doesn't exist, return false.

### Lines 125–138: a helper that reads one "word" of the header

```cpp
    auto next_token = [&](std::string& tok) -> bool {
        tok.clear();
        char ch;
```

* A lambda (small function) called `next_token`. It reads the next **token**: a piece of text between spaces.
* `-> bool`: it returns true if it found one.
* Line 127: empty the output string first.
* Line 128: a variable for one character.

```cpp
        while (f.get(ch)) {
```

* Line 129: `f.get(ch)` reads **one character** into `ch`. It's true while there are characters left. So
  this loop reads the file character by character.

```cpp
            if (ch == '#') { std::string line; std::getline(f, line); continue; }
```

* Line 130: a `#` starts a **comment** in PPM files. `std::getline` reads (and throws away) the rest of that line,
  then `continue` goes back to the top of the loop.

```cpp
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                if (!tok.empty()) return true;
                continue;
            }
```

* Line 131: is this character **whitespace** (space, new line, carriage return, or tab)?
* Line 132: if we already collected some characters, the token has ended: return true.
* Line 133: otherwise it's just extra spaces before a token: skip them.

```cpp
            tok += ch;
        }
        return !tok.empty();
    };
```

* Line 135: a normal character: add it to the token.
* Line 137: the file ended. Return true if we collected something.
* Line 138: end of the lambda.

### Lines 140–144: read and check the header

```cpp
    std::string magic, sw, sh, smax;
    if (!next_token(magic) || !next_token(sw) || !next_token(sh) || !next_token(smax)) return false;
    if (magic != "P6" && magic != "P3") return false;
    int w = std::stoi(sw), h = std::stoi(sh), maxval = std::stoi(smax);
    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 255) return false;
```

* Line 140: four strings for the four header words.
* Line 141: read them in order. If any is missing, the file is broken: return false. (`||` = "or": stop as soon
  as one fails.)
* Line 142: the first word must be `P6` or `P3`.
* Line 143: `std::stoi` = "string to integer": the text `"256"` becomes the number 256.
* Line 144: sanity checks. We only support up to 255 (1 byte per value).

### Lines 146–162: read all pixels

```cpp
    out = Image(w, h);
    for (int i = 0; i < w * h; i++) {
        int rgb[3];
        for (int k = 0; k < 3; k++) {
```

* Line 146: create the output image with the right size.
* Line 147: for every pixel.
* Line 148: space for its three values.
* Line 149: for red (k=0), green (k=1), blue (k=2).

```cpp
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
```

* Lines 150–153: **binary** file: read one byte. `(unsigned char)` makes sure values like 200 aren't read as
  negative numbers (a plain `char` can be negative on some systems).
* Lines 154–158: **text** file: read the next word and convert it to a number.
* Line 159: end of the 3-channel loop.

```cpp
        Color c(rgb[0] / (double)maxval, rgb[1] / (double)maxval, rgb[2] / (double)maxval);
        out.data[i] = file_is_srgb ? srgb_to_linear(c) : c;
    }
    return true;
}
```

* Line 160: turn 0–255 into 0.0–1.0 (fraction division thanks to `(double)`).
* Line 161: convert to linear (if the file is sRGB) and store it.
* Line 163: success.

---

## Block J — BMP writing (lines 166–210)

This is the chapter 3 BMP code, rewritten as a reusable function. See
[ch03 block F](ch03_first_image.md) for the BMP rules.

### Lines 169–174: helpers for little-endian numbers

```cpp
inline void put_u16_le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF)); v.push_back((uint8_t)((x >> 8) & 0xFF));
}
inline void put_u32_le(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)((x >> (8 * i)) & 0xFF));
}
```

* `put_u16_le`: append a 2-byte number to the list, lowest byte first ("le" = little-endian).
* `put_u32_le`: append a 4-byte number. The loop takes byte 0, 1, 2, 3 by shifting 0, 8, 16, 24 bits.
* `push_back` = add one element at the end of a vector.

### Lines 176–181: prepare

```cpp
inline bool write_bmp(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> rgb = to_rgb8(img, opt);
    const int row_size = (img.width * 3 + 3) & ~3;          // rows padded to 4 bytes
    const uint32_t pixel_bytes = (uint32_t)row_size * img.height;
```

* Line 179: get all pixels as bytes (R, G, B order).
* Line 180: round the row length up to a multiple of 4. `& ~3` is a bit trick: `~3` is a number whose lowest two
  bits are 0, so `& ~3` clears the lowest two bits, i.e. rounds **down** to a multiple of 4. Adding 3 first makes
  it round **up**. (Same result as `/ 4 * 4` in chapter 3.)
* Line 181: total pixel bytes.

### Lines 182–197: the 54-byte header

```cpp
    std::vector<uint8_t> f;
    f.push_back('B'); f.push_back('M');
    put_u32_le(f, 54 + pixel_bytes);   // total file size
    put_u32_le(f, 0);                  // reserved
    put_u32_le(f, 54);                 // where the pixels start
    put_u32_le(f, 40);
    put_u32_le(f, (uint32_t)img.width);
    put_u32_le(f, (uint32_t)img.height); // positive height = rows stored bottom-up
    put_u16_le(f, 1);                  // planes
    put_u16_le(f, 24);                 // bits per pixel
    put_u32_le(f, 0);                  // no compression
    put_u32_le(f, pixel_bytes);
    put_u32_le(f, 2835); put_u32_le(f, 2835);   // 72 DPI
    put_u32_le(f, 0); put_u32_le(f, 0);
```

* Line 182: `f` = the whole file, built in memory.
* This time, instead of writing into fixed positions, we **append** fields in order. Count them:
  2 + 4 + 4 + 4 = 14 bytes of file header, then 4+4+4+2+2+4+4+4+4+4+4 = 40 bytes of info header. Total **54**.
* Line 196: 2835 pixels per meter ≈ 72 dots per inch. Only matters for printing.
* Line 197: palette info: 0 = none.

### Lines 199–205: the pixels

```cpp
    for (int y = img.height - 1; y >= 0; y--) {
        for (int x = 0; x < img.width; x++) {
            size_t i = ((size_t)y * img.width + x) * 3;
            f.push_back(rgb[i + 2]); f.push_back(rgb[i + 1]); f.push_back(rgb[i + 0]);
        }
        for (int pad = img.width * 3; pad < row_size; pad++) f.push_back(0);
    }
```

* Line 199: rows from the **bottom** up.
* Lines 200–203: each pixel as **B, G, R**.
* Line 204: add zero bytes until the row reaches `row_size` (the padding). If no padding is needed, this loop
  runs 0 times.

### Lines 206–210: write the file

```cpp
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out.write((const char*)f.data(), (std::streamsize)f.size());
    return (bool)out;
}
```

Open, check, write everything at once, report success.

---

## Block K — PNG writing (lines 212–219)

```cpp
inline bool write_png(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    return png::write_file(filename, bytes.data(), img.width, img.height, 3, 1);
}
```

* Line 217: get the bytes.
* Line 218: hand them to our PNG encoder (`png.h`, chapter 7): 3 channels (RGB), compression level 1.
  `png::` because the encoder lives in the inner namespace `pixel::png`.

---

## Block L — `save_image` (lines 221–232)

```cpp
inline bool save_image(const std::string& filename, const Image& img,
                       const SaveOptions& opt = SaveOptions()) {
    bool ok;
    std::string ext = std::filesystem::path(filename).extension().string();
```

* Line 224: will hold success / failure.
* Line 225: get the **extension** of the file name: `"images/a.png"` → `".png"`.

```cpp
    if (ext == ".ppm")      ok = write_ppm(filename, img, opt);
    else if (ext == ".bmp") ok = write_bmp(filename, img, opt);
    else                    ok = write_png(filename, img, opt);
```

* Lines 226–228: pick the writer. Anything else (including `.png`) → PNG.

```cpp
    std::printf("%s %s (%dx%d)\n", ok ? "Saved" : "FAILED to save", filename.c_str(),
                img.width, img.height);
    return ok;
}
```

* Line 229: print a message, e.g. `Saved images/x.png (400x225)`.
  * `%s` = insert text, `%d` = insert a whole number, in the order given after the format.
  * `ok ? "Saved" : "FAILED to save"` chooses the word.
  * `.c_str()` gives the old C-style text that `printf` needs.
* Line 231: return success.

---

## Block M — End (line 234)

```cpp
} // namespace pixel
```

---

## Check your understanding

1. Why does `Image` store `Color` (doubles) instead of bytes? *(Math needs fractions and values above 1; bytes are
   only made when saving.)*
2. What's the difference between `at` and `get_clamped`? *(`at` is fast but unsafe outside the image;
   `get_clamped` moves the position to the nearest edge.)*
3. In what order does `encode_pixel` do its three steps? *(exposure → tone map → sRGB)*
4. How does `save_image` decide the format? *(From the file extension.)*
5. What color do you get from sampling an empty image? *(Magenta, meaning "missing".)*
