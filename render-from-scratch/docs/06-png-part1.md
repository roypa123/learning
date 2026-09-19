# Chapter 6 — Writing PNG files, part 1: the container

[← The Image class](05-image-class.md) · [Contents](README.md) · [Next: PNG part 2 →](07-png-part2.md)

---

## Goal

PNG is *the* lossless image format of the web. Every browser, phone and paint program opens it.
In this chapter you'll write a **complete, valid PNG file** in about 100 lines, without any library.
We'll skip compression for now (that's chapter 7), but everything else is real:

* the PNG **signature**,
* **chunks** and their **CRC-32** checksums,
* the **zlib** wrapper with **stored** (uncompressed) DEFLATE blocks and the **Adler-32** checksum,
* **filter bytes**.

---

## 1. The big picture

A PNG file is a signature followed by a list of **chunks**:

```
┌───────────────────────────┐
│ signature (8 bytes)       │   "this is a PNG"
├───────────────────────────┤
│ IHDR chunk                │   image header: width, height, color type...
├───────────────────────────┤
│ IDAT chunk(s)             │   the pixels, compressed with zlib
├───────────────────────────┤
│ IEND chunk                │   "the end"
└───────────────────────────┘
```

Inside IDAT, the pixel data is wrapped in two more layers:

```
pixels ──▶ add a filter byte per row ──▶ DEFLATE (compress) ──▶ zlib wrapper ──▶ IDAT chunk
```

We'll build it from the inside out.

---

## 2. The signature

Every PNG starts with the same 8 bytes:

```
hex:    89  50  4E  47  0D  0A  1A  0A
meaning:     P   N   G  \r  \n ^Z  \n
```

These bytes were chosen cleverly: the `0x89` (not a text character) catches transfers that strip the
8th bit, the `\r\n` and `\n` catch transfers that change line endings, and `^Z` stops the
`type` command on old DOS systems. If a PNG gets corrupted in any of these ways, programs notice
immediately.

---

## 3. Chunks

Every chunk has the same four parts:

```
┌──────────────┬──────────────┬───────────────────────┬──────────────┐
│ length (4 B) │  type (4 B)  │ data (length bytes)   │  CRC (4 B)   │
│ big-endian   │ e.g. "IHDR"  │                       │ of type+data │
└──────────────┴──────────────┴───────────────────────┴──────────────┘
```

**Big-endian:** the opposite of BMP. PNG stores the *most* significant byte first:

```
width 300 = 0x0000012C   ->  bytes 00 00 01 2C
```

### 3.1 IHDR: the image header (13 bytes of data)

| Bytes | Field | Our value |
|-------|-------|-----------|
| 4 | width | 300 |
| 4 | height | 200 |
| 1 | bit depth (bits per channel) | 8 |
| 1 | color type | 2 = RGB (6 = RGBA, 0 = grey, ...) |
| 1 | compression method | 0 (the only one: zlib/deflate) |
| 1 | filter method | 0 (the only one) |
| 1 | interlace | 0 = none |

### 3.2 IEND

A chunk with **zero** bytes of data. Just length 0, type "IEND", and its CRC.

---

## 4. CRC-32: catching damaged data

A **checksum** is a small number computed from a lot of data, so that if any bit changes the
checksum (almost certainly) changes too. PNG uses **CRC-32** ("cyclic redundancy check").

### 4.1 The idea in one paragraph

Think of the data as one enormous binary number. CRC divides it by a fixed 33-bit "magic" number (the
*polynomial*) using a special kind of arithmetic where addition is XOR (no carries). The
**remainder** of that division is the CRC. Just like ordinary division, changing any digit of the
number changes the remainder. And the arithmetic can be done one byte at a time, very quickly.

### 4.2 The table trick

Doing the division bit by bit is slow. Instead we precompute the effect of each possible byte (256
possibilities) once:

```cpp
for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++)
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);   // one bit of "division"
    crc_table[n] = c;
}
```

Then the CRC of any data is:

```cpp
uint32_t c = 0xFFFFFFFF;                             // start with all ones
for (uint8_t b : bytes)
    c = crc_table[(c ^ b) & 0xFF] ^ (c >> 8);        // process one byte
return c ^ 0xFFFFFFFF;                               // flip all bits at the end
```

`0xEDB88320` is the standard CRC-32 polynomial written "backwards" (because PNG processes bits
least-significant first). The starting and ending `0xFFFFFFFF` make sure leading zero bytes still
change the result. You don't need to understand the theory to use it. What matters is that it's the
same algorithm as in ZIP files, Ethernet and many others.

> **Sanity check:** the CRC-32 of the ASCII text `123456789` is `0xCBF43926`. If you ever write your
> own, test it with that.

The CRC covers the chunk **type and data**, not the length.

---

## 5. The zlib wrapper and DEFLATE "stored" blocks

The IDAT data must be a **zlib stream**:

```
┌─────┬─────┬──────────────────────────────┬──────────────────┐
│ CMF │ FLG │ DEFLATE data (blocks)        │ Adler-32 (4 B)   │
│0x78 │0x01 │                              │ big-endian       │
└─────┴─────┴──────────────────────────────┴──────────────────┘
```

* **CMF = 0x78**: compression method 8 (DEFLATE) with a 32 KB window.
* **FLG = 0x01**: chosen so that `(CMF × 256 + FLG)` is divisible by 31. That's a tiny checksum on
  the header itself: 0x7801 = 30721 = 31 × 991. ✓

### 5.1 DEFLATE blocks

DEFLATE data is a sequence of **blocks**. Each block begins with 3 header bits:

* `BFINAL` (1 bit): 1 if this is the last block.
* `BTYPE` (2 bits): `00` = stored (no compression), `01` = fixed Huffman, `10` = dynamic Huffman.

A **stored** block is the simplest thing possible: after the header bits (padded to a full byte)
come:

```
LEN   (2 bytes, little-endian!)     number of raw bytes, at most 65535
NLEN  (2 bytes, little-endian)      ~LEN (all bits flipped: another tiny check)
LEN raw bytes
```

Our data is bigger than 65,535 bytes (200 rows × 901 bytes), so we split it into several stored
blocks. For a stored block the first byte is simply `1` (last block) or `0` (not last), because the 3
header bits are `BFINAL, 0, 0` and the rest of the byte is padding.

> **Yes, DEFLATE is little-endian while PNG and zlib are big-endian.** They were designed by
> different people at different times. Welcome to file formats!

### 5.2 Adler-32

After the DEFLATE data comes an **Adler-32** checksum of the *uncompressed* data. It's simpler (and
weaker) than CRC:

```
a = 1 + sum of all bytes            (mod 65521)
b = sum of all the running values of a    (mod 65521)
adler = b * 65536 + a
```

65521 is the largest prime below 65536.

---

## 6. Filter bytes

The data inside the zlib stream isn't just the pixels. **Each row starts with one "filter type" byte**:

```
row 0:  [0] R G B R G B R G B ...
row 1:  [0] R G B R G B R G B ...
```

Filters transform the row to make it easier to compress (chapter 7). Type 0 means "no filter": the
bytes are stored as they are. So each row is `1 + width × 3` bytes.

---

## 7. The code

Everything in one file, top to bottom:

**File: `chapters/ch06_png_stored.cpp`**

```cpp
// ch06_png_stored.cpp
// ------------------------------------------------------------
// Chapter 6: A complete PNG writer in ~100 lines, no compression.
// Everything is in this one file so you can read it top to bottom:
//   signature -> IHDR chunk -> IDAT chunk (zlib "stored") -> IEND chunk
// Output: images/ch06_stored.png
// ------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <filesystem>

// ---------- CRC-32: detects damaged chunks ---------------------------------
uint32_t crc_table[256];

void make_crc_table() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
}

uint32_t crc32(const std::vector<uint8_t>& bytes) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : bytes) c = crc_table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ---------- Adler-32: checksum of the uncompressed data ---------------------
uint32_t adler32(const std::vector<uint8_t>& bytes) {
    uint32_t a = 1, b = 0;
    for (uint8_t x : bytes) {
        a = (a + x) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// ---------- helpers: numbers are stored BIG-endian in PNG ------------------
void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}

void write_chunk(std::vector<uint8_t>& file, const char* type, const std::vector<uint8_t>& data) {
    put32(file, (uint32_t)data.size());                    // 1. length
    std::vector<uint8_t> type_and_data(type, type + 4);   // 2. type (4 letters)
    type_and_data.insert(type_and_data.end(), data.begin(), data.end()); // 3. data
    file.insert(file.end(), type_and_data.begin(), type_and_data.end());
    put32(file, crc32(type_and_data));                     // 4. CRC
}

int main() {
    make_crc_table();
    const int width = 300, height = 200;

    // ---------- 1. Raw pixel rows, each starting with a filter byte (0) ----
    std::vector<uint8_t> raw;
    for (int y = 0; y < height; y++) {
        raw.push_back(0);                       // filter type 0 = "None"
        for (int x = 0; x < width; x++) {
            // A little sunset: orange at the bottom, purple at the top, a sun disc.
            double t = (double)y / (height - 1);
            double r = 0.35 + 0.65 * t, g = 0.15 + 0.45 * t * t, b = 0.45 - 0.3 * t;
            double dx = x - 150.0, dy = y - 120.0;
            if (dx * dx + dy * dy < 40 * 40) { r = 1.0; g = 0.9; b = 0.6; }
            raw.push_back((uint8_t)(255 * r));
            raw.push_back((uint8_t)(255 * g));
            raw.push_back((uint8_t)(255 * b));
        }
    }

    // ---------- 2. Wrap it in a zlib stream with "stored" deflate blocks ----
    std::vector<uint8_t> z;
    z.push_back(0x78);   // CMF: method 8 (deflate), window 32K
    z.push_back(0x01);   // FLG: makes (0x78*256 + 0x01) divisible by 31
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t len = raw.size() - pos;
        if (len > 65535) len = 65535;            // a stored block holds at most 65535 bytes
        bool last = (pos + len == raw.size());
        z.push_back(last ? 1 : 0);               // BFINAL bit, BTYPE = 00 (stored)
        z.push_back(len & 0xFF);                 // LEN (little-endian!)
        z.push_back((len >> 8) & 0xFF);
        z.push_back(~len & 0xFF);                // NLEN = one's complement of LEN
        z.push_back((~len >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    }
    put32(z, adler32(raw));

    // ---------- 3. Assemble the PNG file -----------------------------------
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    std::vector<uint8_t> ihdr;
    put32(ihdr, width);
    put32(ihdr, height);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(2);   // color type 2 = RGB
    ihdr.push_back(0);   // compression
    ihdr.push_back(0);   // filter method
    ihdr.push_back(0);   // interlace: none
    write_chunk(file, "IHDR", ihdr);
    write_chunk(file, "IDAT", z);
    write_chunk(file, "IEND", {});

    std::filesystem::create_directories("images");
    FILE* f = std::fopen("images/ch06_stored.png", "wb");
    if (!f) { std::printf("cannot open output file\n"); return 1; }
    std::fwrite(file.data(), 1, file.size(), f);
    std::fclose(f);
    std::printf("Wrote images/ch06_stored.png: %zu bytes for %d pixels (%.2f bytes/pixel)\n",
                file.size(), width * height, (double)file.size() / (width * height));
    return 0;
}
```

The image content is a tiny sunset: a vertical gradient from purple to orange with a pale yellow
disc. The colors are computed directly as bytes (no linear/sRGB conversion). That's fine for this
demo, since we're focusing on the file format.

```bat
run ch06_png_stored
```

Output:

```
Wrote images/ch06_stored.png: 180278 bytes for 60000 pixels (3.00 bytes/pixel)
```

## 8. What you should see

![Stored PNG](../images/ch06_stored.png)

> **Image description:** A 300×200 picture: the sky fades from a dusky purple at the top to warm
> orange at the bottom, and a pale yellow sun (a hard-edged circle) sits a bit below the center.

It opens in every program, but notice the **file size**: about 180 KB, slightly *more* than the raw
pixels (because of the headers and filter bytes). We've built a correct PNG that isn't compressed at
all. Let's fix that next chapter.

### Looking inside your file

Open `images/ch06_stored.png` in a hex editor. You'll find:

```
89 50 4E 47 0D 0A 1A 0A                 signature
00 00 00 0D 49 48 44 52                 length 13, "IHDR"
00 00 01 2C 00 00 00 C8 08 02 00 00 00  width 300, height 200, 8, RGB, 0, 0, 0
xx xx xx xx                             CRC of IHDR
.. .. .. .. 49 44 41 54                 length, "IDAT"
78 01                                   zlib header
00 FF FF 00 00 ...                      first stored block: not final, LEN=65535, NLEN=0
```

Seeing your own bytes in a real file format is a great feeling.

---

## Try it yourself

1. Change color type to 0 (greyscale) and write 1 byte per pixel instead of 3. Update IHDR and the
   row length.
2. Deliberately change one byte of the IHDR data *after* computing the CRC. Try to open the file.
   Most viewers refuse it with an error like "CRC error". That's the checksum doing its job.
3. Write the CRC-32 of `"123456789"` to the console and compare it with `0xCBF43926`.
4. Add a `tEXt` chunk with data `"Author\0Your Name"` before IEND. It's a valid PNG metadata chunk.
   Many image viewers show it in the file properties.

## Common problems

| Symptom | Cause |
|---------|-------|
| "Not a PNG file" | Signature wrong, or file opened in text mode (use `"wb"`) |
| "CRC error" | CRC computed over the wrong bytes (must be type + data, not length) |
| Image is garbled or skewed | Forgot the filter byte at the start of each row |
| "Invalid stored block lengths" | LEN/NLEN not little-endian, or NLEN not `~LEN` |
| "Incorrect data check" | Adler-32 computed over the wrong data (must be the *filtered, uncompressed* rows) |

---

## Summary

* PNG = signature + chunks (length, type, data, CRC-32).
* IHDR describes the image; IDAT holds a zlib stream; IEND ends the file.
* zlib = 2-byte header + DEFLATE blocks + Adler-32.
* The simplest DEFLATE block type ("stored") just copies bytes.
* Each pixel row starts with a filter-type byte.

Next: [Chapter 7 — Writing PNG files, part 2: real compression →](07-png-part2.md)
