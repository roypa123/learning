# Line by line: `include/pixel/gif.h`

[← Line-by-line index](README.md) · [Chapter 39 (the theory)](../39-animation.md)

**What this file does, in one sentence:** it writes **animated GIF** files: a fixed palette, dithering to hide it, and
LZW compression, all by hand.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–17 | |
| B. `begin` | 19–48 | file header, palette, loop extension |
| C. `add_frame` | 50–78 | colors → palette indices (with dithering), frame headers |
| D. `end`, destructor | 80–87 | close the file properly |
| E. Small helpers | 89–102 | quantize, write bytes |
| F. `lzw_encode` | 104–172 | the compression |
| G. End | 173–175 | |

---

## Block A — Comments, includes (lines 1–17)

Comments listing the three parts (palette, dithering, LZW); `#pragma once`; `<algorithm>` (`std::fill`), `<cstdint>`,
`<cstdio>` (C file functions), `<string>`, `<vector>`; our `image.h`; `namespace pixel`.

---

## Block B — `begin` (lines 19–48)

```cpp
    bool begin(const std::string& filename, int width, int height, int delay_cs = 4, int loop_count = 0) {
        ensure_parent_folder(filename);
        f = std::fopen(filename.c_str(), "wb");
        if (!f) return false;
        w = width; h = height; delay = delay_cs;
```

* Line 22: start a GIF. `delay_cs` = how long each frame stays on screen, in **hundredths of a second** (4 ≈ 25 frames
  per second). `loop_count` 0 means "loop forever".
* Lines 23–26: create the folder if needed, open the file for binary writing, remember the size and delay.

```cpp
        put_str("GIF89a");
        put_u16(w); put_u16(h);
        put_byte(0xF7);          // global color table, 8 bits color resolution, 256 entries
        put_byte(0);             // background color index
        put_byte(0);             // pixel aspect ratio (unused)
```

* Line 28: the file signature: the six letters "GIF89a".
* Line 29: width and height, as 2-byte little-endian numbers.
* Line 30: a packed byte of flags: there **is** a global color table, and it has 256 entries.
* Lines 31–32: two more header bytes we don't need.

```cpp
        for (int i = 0; i < 256; i++) {
            int r = 0, g = 0, b = 0;
            if (i < 252) {
                r = (i / 42) * 255 / 5;
                g = ((i / 6) % 7) * 255 / 6;
                b = (i % 6) * 255 / 5;
            }
            put_byte(r); put_byte(g); put_byte(b);
        }
```

Lines 34–42: the **palette**: 256 colors, three bytes each. We use 6 levels of red × 7 of green × 6 of blue = 252
colors (green gets more levels because eyes are most sensitive to it); the last 4 stay black.

* `i / 42` is the red level (0–5): each red level covers 42 entries (7 greens × 6 blues).
* `(i / 6) % 7` is the green level (0–6).
* `i % 6` is the blue level (0–5).
* `× 255 / 5` spreads the levels over the full 0–255 range.

```cpp
        put_byte(0x21); put_byte(0xFF); put_byte(0x0B);
        put_str("NETSCAPE2.0");
        put_byte(0x03); put_byte(0x01); put_u16(loop_count); put_byte(0x00);
        return true;
```

Lines 44–47: the **NETSCAPE2.0** extension block, the historical way to say "loop this animation". Without it, browsers
play the GIF once.

---

## Block C — `add_frame` (lines 50–78)

```cpp
    void add_frame(const Image& img, const SaveOptions& opt = SaveOptions()) {
        if (!f) return;
        std::vector<uint8_t> rgb = to_rgb8(img, opt);
        std::vector<uint8_t> indices((size_t)w * h);
        static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
```

* Line 52: convert the image to bytes (exposure, tone map, sRGB as usual).
* Line 53: room for one **palette index** per pixel.
* Line 54: the 4 × 4 **Bayer matrix**: a fixed pattern of thresholds used for dithering.

```cpp
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int sx = x < img.width ? x : img.width - 1;
                int sy = y < img.height ? y : img.height - 1;
                size_t i = ((size_t)sy * img.width + sx) * 3;
                double d = (bayer[y & 3][x & 3] + 0.5) / 16.0 - 0.5;   // -0.5 .. 0.5
                int r = quantize(rgb[i + 0], 5, d);
                int g = quantize(rgb[i + 1], 6, d);
                int b = quantize(rgb[i + 2], 5, d);
                indices[(size_t)y * w + x] = (uint8_t)(r * 42 + g * 6 + b);
            }
```

* Lines 57–59: read the source pixel (clamped, in case a frame is a different size).
* Line 60: the dither offset for this pixel: `x & 3` and `y & 3` are x and y modulo 4, so the 4 × 4 pattern repeats
  across the image. The value becomes −0.5 … +0.5.
* Lines 61–63: convert each channel to a palette **level**, adding the offset first, so neighboring pixels round
  differently and the eye mixes them into the in-between color.
* Line 64: combine the three levels into the palette index, using the same formula as the palette itself.

```cpp
        put_byte(0x21); put_byte(0xF9); put_byte(0x04);
        put_byte(0x04);          // disposal: leave frame in place
        put_u16(delay);
        put_byte(0);             // transparent color index (unused)
        put_byte(0);
```

Lines 68–72: the **Graphic Control Extension**: it carries this frame's delay and says what to do with the previous
frame (leave it).

```cpp
        put_byte(0x2C);
        put_u16(0); put_u16(0); put_u16(w); put_u16(h);
        put_byte(0);             // no local color table, not interlaced
        lzw_encode(indices);
```

* Lines 74–76: the **Image Descriptor**: this frame covers the whole canvas, and uses the global palette.
* Line 77: compress and write the pixel indices (block F).

---

## Block D — `end`, destructor (lines 80–87)

```cpp
    void end() {
        if (!f) return;
        put_byte(0x3B);          // trailer
        std::fclose(f);
        f = nullptr;
    }

    ~GifWriter() { end(); }
```

* Lines 80–85: write the final `0x3B` byte (the GIF "end of file" marker) and close the file. Setting `f` to `nullptr`
  makes calling `end()` twice safe.
* Line 87: the **destructor**: if you forget to call `end()`, it runs automatically when the writer is destroyed, so the
  file is never left broken.

---

## Block E — Small helpers (lines 89–102)

```cpp
    static int quantize(int value, int levels, double d) {
        double v = value / 255.0 * levels + d;
        int q = (int)(v + 0.5);
        return q < 0 ? 0 : (q > levels ? levels : q);
    }
```

Lines 94–98: turn a 0–255 value into a level 0..`levels`, with the dither offset `d` added before rounding, and clamped
to the valid range.

```cpp
    void put_byte(int b) { std::fputc(b & 0xFF, f); }
    void put_u16(int v) { put_byte(v & 0xFF); put_byte((v >> 8) & 0xFF); }
    void put_str(const char* s) { while (*s) put_byte(*s++); }
```

Lines 100–102: write one byte; a 2-byte little-endian number; and a piece of text (`while (*s)` runs until the text's
terminating zero byte; `*s++` reads a character and moves on).

---

## Block F — `lzw_encode` (lines 104–172)

**LZW** builds a dictionary of sequences while it writes. The decoder builds exactly the same dictionary from the codes,
so the dictionary itself is never stored.

```cpp
        const int min_code_size = 8;
        const int clear_code = 1 << min_code_size;   // 256
        const int end_code = clear_code + 1;         // 257
        put_byte(min_code_size);
```

* Lines 108–110: codes 0–255 are the single colors; **256** means "clear the dictionary", **257** means "end".
* Line 111: GIF stores the starting code size in the file.

```cpp
        std::vector<uint8_t> block;                  // bytes waiting for a sub-block
        uint32_t bit_buffer = 0;
        int bit_count = 0;
        int code_size = min_code_size + 1;
```

* Line 113: GIF pixel data is written in **sub-blocks** of at most 255 bytes, each with a length byte in front.
* Lines 114–116: a bit buffer (codes are 9–12 bits, not whole bytes) and the current code size, starting at 9.

```cpp
        auto flush_block = [&]() {
            if (block.empty()) return;
            put_byte((int)block.size());
            std::fwrite(block.data(), 1, block.size(), f);
            block.clear();
        };
```

Lines 118–123: write the waiting bytes as one sub-block: first its length, then the bytes.

```cpp
        auto emit = [&](int code) {
            bit_buffer |= (uint32_t)code << bit_count;
            bit_count += code_size;
            while (bit_count >= 8) {
                block.push_back((uint8_t)(bit_buffer & 0xFF));
                bit_buffer >>= 8;
                bit_count -= 8;
                if (block.size() == 255) flush_block();
            }
        };
```

Lines 124–133: write one code, lowest bit first, exactly like the `BitWriter` in [png.md](png.md), and start a new
sub-block whenever 255 bytes are full.

```cpp
        std::vector<int16_t> dict(4096 * 256, -1);
        int next_code = end_code + 1;
```

* Line 136: the dictionary as a big table: `dict[code * 256 + next_color]` = the code for "that sequence followed by
  that color", or −1 if we haven't seen it. (4096 possible codes × 256 colors.)
* Line 137: the next code number to hand out (258).

```cpp
        emit(clear_code);
        if (data.empty()) {
            emit(end_code);
        } else {
            int prefix = data[0];
```

* Line 139: always start with a clear code.
* Lines 140–143: an empty frame just ends; otherwise start with the first pixel as the current **sequence**.

```cpp
            for (size_t i = 1; i < data.size(); i++) {
                int c = data[i];
                int key = prefix * 256 + c;
                if (dict[key] >= 0) {
                    prefix = dict[key];            // known sequence: keep growing it
                    continue;
                }
```

* Lines 144–150: for each next pixel: if "current sequence + this pixel" is already in the dictionary, just move to that
  longer sequence and keep reading. This is how long runs collapse into single codes.

```cpp
                emit(prefix);                      // output the longest known sequence
                int assigned = next_code++;
                dict[key] = (int16_t)assigned;     // remember the new, longer sequence
                if (assigned >= (1 << code_size)) code_size++;
```

* Line 151: the sequence can't grow: write its code.
* Lines 152–153: give the longer sequence a **new code number** for next time.
* Line 154: when the numbers no longer fit in the current code size, use one more bit (9 → 10 → 11 → 12). The decoder
  does exactly the same, so both stay in step.

```cpp
                if (assigned == 4095) {            // dictionary full: start over
                    emit(clear_code);
                    std::fill(dict.begin(), dict.end(), (int16_t)-1);
                    next_code = end_code + 1;
                    code_size = min_code_size + 1;
                }
                prefix = c;
            }
            emit(prefix);
            emit(end_code);
        }
```

* Lines 155–160: 4096 codes is the maximum: send a clear code, wipe the dictionary and start again at 9 bits.
* Line 161: the new sequence starts with the pixel we couldn't add.
* Lines 163–164: after the loop, write the last sequence and the end code.

```cpp
        if (bit_count > 0) {
            block.push_back((uint8_t)(bit_buffer & 0xFF));
            if (block.size() == 255) flush_block();
        }
        flush_block();
        put_byte(0);   // block terminator
```

Lines 166–171: write any leftover bits, flush the last sub-block, and write a 0 length byte, which means "no more
sub-blocks".

---

## Block G — End (lines 173–175)

`};` ends the class, `}` ends the namespace.

---

## Check your understanding

1. Why does GIF need a palette? *(A GIF frame can only use 256 colors.)*
2. What does dithering do? *(Makes neighboring pixels round differently, so the eye sees in-between shades instead of
   bands.)*
3. Why is the dictionary never written into the file? *(The decoder builds the same one from the codes.)*
4. What happens when the dictionary is full? *(A clear code is sent and it starts over.)*
