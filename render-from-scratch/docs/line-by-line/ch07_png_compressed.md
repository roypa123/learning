# Line by line: `ch07_png_compressed.cpp`

[← Line-by-line index](README.md) · [Chapter 7 (the theory)](../07-png-part2.md) · [png.h explained](png.md)

**What the whole program does, in one sentence:** it makes four test pictures (flat, gradient, stripes, noise),
encodes each one **without** and **with** compression, and prints the file sizes so you can see how well
compression works on each kind of picture.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–12 | notes and tools |
| B. `compare` function | 14–22 | encode one image two ways, save it, print sizes |
| C. Start | 24–26 | make the folder, choose the size |
| D. Test 1: flat color | 28–30 | one color everywhere |
| E. Test 2: gradient | 32–38 | smooth color change |
| F. Test 3: stripes | 40–45 | a repeating pattern |
| G. Test 4: noise | 47–50 | random pixels |
| H. End | 51–52 | |

---

## Block A — Comments, includes (lines 1–12)

* Lines 1–6: the plan: level 0 = stored, level 1 = compressed.
* Lines 7–10: `printf`, math, `std::string`, `std::vector`.
* Line 11: our library. Line 12: use `pixel` names directly.

---

## Block B — The `compare` function (lines 14–22)

```cpp
static void compare(const std::string& name, const Image& img) {
```

* Line 14: a helper function, used 4 times below, so we don't repeat the same code.
* `static` here means "only visible inside this file" (a good habit for helpers).
* `void` = returns nothing. Inputs: a short `name` for the test, and the image.

```cpp
    std::vector<uint8_t> rgb = to_rgb8(img);
```

Line 15: convert the image to bytes (R, G, B for every pixel), with default settings (sRGB, clamp). See
[image.md block F](image.md).

```cpp
    std::vector<uint8_t> stored = png::encode(rgb.data(), img.width, img.height, 3, 0);
    std::vector<uint8_t> packed = png::encode(rgb.data(), img.width, img.height, 3, 1);
```

* Line 16: build a PNG in memory with **level 0** (no compression). We only want its size.
* Line 17: build a PNG with **level 1** (filters + LZ77 + Huffman).
* `rgb.data()` = pointer to the first byte. `3` = three channels (RGB).
* `png::` because `encode` lives in `pixel::png`.

```cpp
    png::write_file("images/ch07_" + name + ".png", rgb.data(), img.width, img.height, 3, 1);
```

Line 18: save the compressed version to disk so you can open it. The file name is glued together with `+`:
`"images/ch07_" + "flat" + ".png"` → `images/ch07_flat.png`.

```cpp
    std::printf("%-10s  raw %8zu bytes   stored %8zu   compressed %8zu  (%.1f%% of raw)\n",
                name.c_str(), rgb.size(), stored.size(), packed.size(),
                100.0 * packed.size() / rgb.size());
}
```

Lines 19–21: print one line of results. The format codes:

| Code | Meaning |
|------|---------|
| `%-10s` | text, left-aligned in 10 spaces (so the columns line up) |
| `%8zu` | a size number, right-aligned in 8 spaces |
| `%.1f` | a fraction with 1 decimal |
| `%%` | a real `%` sign |

The last value is the compressed size as a percentage of the raw bytes. `100.0 *` makes it a fraction calculation.

---

## Block C — Start (lines 24–26)

```cpp
int main() {
    ensure_parent_folder("images/x.png");
    const int W = 320, H = 240;
```

* Line 25: make sure the `images` folder exists. We pass any file name inside it (`x.png` is never created);
  the function creates the **folder** part. (`png::write_file` doesn't create folders by itself.)
* Line 26: all test images are 320 × 240.

---

## Block D — Test 1: flat color (lines 28–30)

```cpp
    Image flat(W, H, hex_color(0x3B82F6));
    compare("flat", flat);
```

* Line 29: an image where **every** pixel is the same blue.
* Line 30: test it. Expect tiny output: after the first pixel, everything is "copy from 3 bytes back".

---

## Block E — Test 2: gradient (lines 32–38)

```cpp
    Image grad(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            grad.at(x, y) = srgb_to_linear(Color((double)x / W, (double)y / H, 0.5));
    compare("gradient", grad);
```

* Line 34: a new image.
* Lines 35–36: every pixel.
* Line 37: red grows left→right, green grows top→bottom, blue 0.5. We make the color in screen values and convert
  to linear (so the file gets exactly these values back).
* Every byte differs from its neighbor, so the raw bytes don't repeat. But the **differences** are constant, so
  the Sub/Up filters turn the rows into repeating small numbers, and then LZ77 compresses them well.
* Line 38: test it.

---

## Block F — Test 3: stripes (lines 40–45)

```cpp
    Image stripes(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            stripes.at(x, y) = ((x / 8 + y / 8) % 3 == 0) ? hex_color(0xF59E0B) : hex_color(0x111827);
    compare("stripes", stripes);
```

* Line 44: `x / 8` and `y / 8` = which 8-pixel block we're in (whole-number division). If (column + row) is a
  multiple of 3 → amber, otherwise near-black. The result is a diagonal staircase pattern.
* The pattern repeats, so LZ77 finds long matches.

---

## Block G — Test 4: noise (lines 47–50)

```cpp
    Image noise(W, H);
    for (auto& c : noise.data) c = random_vec();
    compare("noise", noise);
```

* Line 49: for every pixel `c` in the image's data (the `&` means we change the real pixel): a random color.
  `random_vec()` (from `random.h`, chapter 16) gives three random numbers 0–1.
* Random data has **no pattern**, so nothing can be compressed. The "compressed" file is even slightly bigger than
  the raw data (each literal costs 8 or 9 bits). That's normal for every compressor.

---

## Block H — End (lines 51–52)

```cpp
    return 0;
}
```

---

## What the output teaches

```
flat       → almost nothing   (everything repeats)
gradient   → very small       (filters turn smooth changes into repeats)
stripes    → small            (LZ77 finds the repeating pattern)
noise      → about 106%       (nothing to find: compression can't help)
```

## Check your understanding

1. Why is `compare` a separate function? *(The same steps run for 4 images; one function avoids copying code.)*
2. Why can't noise be compressed? *(There's no pattern or repetition to describe.)*
3. Which PNG feature helps the gradient most? *(The row filters.)*
4. Why does line 25 pass a file name that is never created? *(The function creates the folder part of the path.)*
