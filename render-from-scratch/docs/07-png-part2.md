# Chapter 7 — Writing PNG files, part 2: compression

[← PNG part 1](06-png-part1.md) · [Contents](README.md) · [Next: Drawing basics →](08-drawing-basics.md)

> 📖 **Line by line:** [png explained line by line](line-by-line/png.md) · [ch07_png_compressed explained line by line](line-by-line/ch07_png_compressed.md)

---

## Goal

Make our PNGs **small**, using the same ideas inside ZIP, gzip and PNG:

1. **Filters** turn smooth images into runs of small, repeating numbers.
2. **LZ77** replaces repeated byte sequences with short "copy from back there" instructions.
3. **Huffman coding** gives common symbols shorter bit patterns.

By the end, `pixel/png.h` (our library's PNG encoder) will make files 2–100 times smaller than the
stored version, and they'll open everywhere.

This is the most "computer science" chapter in the book. If it feels heavy, it's fine to read
it lightly, trust the encoder, and come back later. Nothing else depends on understanding it.

---

## 1. Why compression works at all

Compression exploits **predictability**. Random noise can't be compressed: there's no
pattern to describe. But real images are full of patterns:

* big areas of the same color (a blue sky),
* smooth gradients (each pixel almost equal to its neighbor),
* repeated textures (stripes, tiles, text).

---

## 2. Step 1: filters

### 2.1 The idea

Consider one row of a smooth gradient:

```
raw bytes:   10  12  14  16  18  20  22  24
```

No byte repeats, so there's nothing obvious to compress. But store **the difference from the
previous byte** instead:

```
differences: 10   2   2   2   2   2   2   2
```

Now it's almost all 2s, which is very compressible! A decoder gets the original back by adding up.
This is PNG's **Sub** filter. PNG has five filters. Each one *predicts* a byte from its neighbors
and stores only the **prediction error**:

```
   c  b        c = byte above-left   b = byte above
   a  x        a = byte to the left  x = the byte we're encoding
```

| Type | Name | Prediction | Stored value |
|------|------|------------|--------------|
| 0 | None | 0 | x |
| 1 | Sub | a | x − a |
| 2 | Up | b | x − b |
| 3 | Average | ⌊(a + b) / 2⌋ | x − avg |
| 4 | Paeth | whichever of a, b, c is closest to a + b − c | x − that |

All arithmetic is done **mod 256** (bytes wrap around): `3 − 5` becomes `254`. "Left" means the same
color channel one *pixel* to the left, i.e. 3 bytes back for RGB.

### 2.2 The Paeth predictor

Paeth (named after Alan Paeth) guesses a gradient: `p = a + b − c` would be exact for a perfectly
flat slope in both directions. It then picks the real neighbor closest to that guess:

```cpp
int p = a + b - c;
int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
if (pa <= pb && pa <= pc) return a;
if (pb <= pc) return b;
return c;
```

### 2.3 Choosing a filter per row

Each row can use a different filter. How do we choose? The standard heuristic is to try all
five and pick the one whose output has the **smallest sum of absolute values**, treating the bytes as
signed: 255 counts as −1, which is "small". Small numbers compress better.

---

## 3. Step 2: LZ77 (repeats → back-references)

### 3.1 The idea

LZ77 (Lempel and Ziv, 1977) scans the data and, whenever the upcoming bytes already appeared
recently, writes a **(length, distance)** pair meaning *"copy `length` bytes starting `distance`
bytes back"*:

```
input:   a b c d e | a b c d e f | a b c d e f
output:  a b c d e   <5,5> f       <6,6>
                      │             └─ copy 6 bytes from 6 back: "abcdef"
                      └─ copy 5 bytes from 5 back: "abcde"
```

A lovely detail: the copy may **overlap itself**. `a <9,1>` means "copy 9 bytes from 1 back", which
repeats `a` nine more times. That's how a long flat area turns into just a few codes.

DEFLATE allows lengths 3–258 and distances 1–32,768 (the "window").

### 3.2 Finding matches fast: hash chains

Comparing against every earlier position would be slow. Instead:

1. Take the next **3 bytes** and compute a **hash** (a number from 0 to 32,767).
2. `head[hash]` remembers the most recent position with that hash; `prev[pos]` links to the
   position before that with the same hash, and so on. This forms a **chain** of candidates.
3. Check up to `MAX_CHAIN` (48) candidates, count how many bytes match, and keep the longest.

```
position:   0 1 2 3 4 5 6 7 8 9
data:       a b c x a b c y a b c ...
hash("abc") → head = 8 → prev[8] = 4 → prev[4] = 0 → -1 (end)
```

If the best match is at least 3 long, we output (length, distance). Otherwise we output the byte
itself (a **literal**).

---

## 4. Step 3: Huffman codes (short codes for common symbols)

### 4.1 The idea

Normally every byte takes 8 bits. But if some symbols are far more common than others, give them
shorter codes and give rare ones longer codes. Morse code works on the same principle
(E = `.`, Q = `--.-`).

The codes must be **prefix-free**: no code may be the beginning of another. Then a decoder can read
the bits one at a time and always know where a symbol ends. Example with four symbols:

```
symbol  frequency  code
  A        50%      0
  B        25%      10
  C        12.5%    110
  D        12.5%    111

"AABAC" -> 0 0 10 0 110 -> 00100110  (8 bits instead of 5 x 8 = 40)
```

### 4.2 DEFLATE's symbols

DEFLATE uses one alphabet of **288 symbols** for literals and lengths:

| Symbol | Meaning |
|--------|---------|
| 0–255 | a literal byte |
| 256 | end of block |
| 257–285 | a match **length** (plus extra bits for the exact value) |

and a second alphabet of **30 symbols** for **distances** (plus extra bits).

Lengths and distances use "base + extra bits". For example length symbol **270** means "length 23 to
26", followed by **2 extra bits** giving 0–3 to add to 23:

```
length 25  ->  symbol 270 (base 23)  +  extra bits "10" (value 2)
```

### 4.3 Fixed Huffman codes

DEFLATE lets you send your own optimized code table (**dynamic** Huffman, block type 2), or use a
**fixed** table from the specification (block type 1). We use the fixed table. It's simpler, and
combined with good filters and LZ77 it works very well for images.

| Literal/length symbols | Code length | Codes (binary) |
|------------------------|-------------|----------------|
| 0–143 | 8 bits | 00110000 – 10111111 |
| 144–255 | 9 bits | 110010000 – 111111111 |
| 256–279 | 7 bits | 0000000 – 0010111 |
| 280–287 | 8 bits | 11000000 – 11000111 |

Distances use plain 5-bit codes (0–29).

> **Improvement idea for later:** dynamic Huffman codes typically shave another 10–25% off. The DEFLATE
> spec (RFC 1951) describes them, and you'll have all the building blocks.

### 4.4 Bits, not bytes

DEFLATE is a **bit stream**. Values are packed starting from the **least significant bit** of each
byte. But Huffman codes are defined starting from their *most* significant bit. So we **reverse** the
bits of each Huffman code before packing it:

```cpp
void put_huffman(uint32_t code, int length) {
    uint32_t reversed = 0;
    for (int i = 0; i < length; i++)
        reversed = (reversed << 1) | ((code >> i) & 1u);
    put_bits(reversed, length);
}
```

The `BitWriter` keeps a small buffer: it ORs new bits in above the ones already waiting, and every
time 8 or more bits are waiting it outputs a byte.

---

## 5. Putting it all together

```
Image (linear doubles)
   │ to_rgb8()               exposure, tone map, sRGB, round to bytes
   ▼
RGB bytes
   │ filter_rows()           per row: try 5 filters, keep the best, prefix the filter type
   ▼
filtered bytes
   │ zlib_compress()         2-byte header; one fixed-Huffman block: LZ77 + codes; Adler-32
   ▼
zlib stream
   │ encode()                signature, IHDR, IDAT, IEND (each with CRC-32)
   ▼
.png file
```

### The library file `png.h`

**File: `include/pixel/png.h`**

```cpp
// pixel/png.h
// ------------------------------------------------------------
// Our own PNG encoder, written from the specification.
//   * CRC-32 checksum        (for every PNG chunk)
//   * Adler-32 checksum      (for the zlib stream)
//   * DEFLATE compression    (LZ77 + fixed Huffman codes)
//   * PNG row filters        (None, Sub, Up, Average, Paeth)
// Explained in docs/06-png-part1.md and docs/07-png-part2.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace pixel {
namespace png {

// ======================= Checksums ==========================================

// CRC-32 (polynomial 0xEDB88320), as used by PNG and ZIP.
inline uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        ready = true;
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Adler-32, as used at the end of a zlib stream.
inline uint32_t adler32(const uint8_t* data, size_t len) {
    const uint32_t MOD = 65521;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

// ======================= Bit writer =========================================
// DEFLATE packs values bit by bit, starting from the LEAST significant bit.

struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buffer = 0;   // bits waiting to be written
    int count = 0;         // how many bits are waiting

    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}

    // Write 'nbits' bits of 'value', least significant bit first.
    void put_bits(uint32_t value, int nbits) {
        buffer |= value << count;
        count += nbits;
        while (count >= 8) {
            out.push_back((uint8_t)(buffer & 0xFF));
            buffer >>= 8;
            count -= 8;
        }
    }

    // Huffman codes are defined most-significant-bit first,
    // so we reverse them before writing.
    void put_huffman(uint32_t code, int length) {
        uint32_t reversed = 0;
        for (int i = 0; i < length; i++)
            reversed = (reversed << 1) | ((code >> i) & 1u);
        put_bits(reversed, length);
    }

    void flush() {
        if (count > 0) out.push_back((uint8_t)(buffer & 0xFF));
        buffer = 0;
        count = 0;
    }
};

// ======================= DEFLATE (fixed Huffman) ============================

// Write a literal byte or the end-of-block symbol (0..287) with the fixed code.
inline void write_literal_symbol(BitWriter& bw, int sym) {
    if (sym <= 143)      bw.put_huffman(0x30 + sym, 8);
    else if (sym <= 255) bw.put_huffman(0x190 + (sym - 144), 9);
    else if (sym <= 279) bw.put_huffman(sym - 256, 7);
    else                 bw.put_huffman(0xC0 + (sym - 280), 8);
}

inline void write_length(BitWriter& bw, int length) {       // 3..258
    static const int base[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                  35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
                                  3,3,3,3,4,4,4,4,5,5,5,5,0};
    int i = 28;
    while (base[i] > length) i--;
    write_literal_symbol(bw, 257 + i);
    if (extra[i] > 0) bw.put_bits((uint32_t)(length - base[i]), extra[i]);
}

inline void write_distance(BitWriter& bw, int dist) {       // 1..32768
    static const int base[30]  = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                  257,385,513,769,1025,1537,2049,3073,4097,6145,
                                  8193,12289,16385,24577};
    static const int extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
                                  7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int i = 29;
    while (base[i] > dist) i--;
    bw.put_huffman((uint32_t)i, 5);                 // fixed distance codes are 5 bits
    if (extra[i] > 0) bw.put_bits((uint32_t)(dist - base[i]), extra[i]);
}

// Compress 'data' into a zlib stream (header + deflate + adler32).
// level 0 = store only (no compression), anything else = LZ77 + fixed Huffman.
inline std::vector<uint8_t> zlib_compress(const std::vector<uint8_t>& data, int level = 1) {
    std::vector<uint8_t> out;
    out.push_back(0x78);    // CMF: deflate, 32K window
    out.push_back(0x01);    // FLG: chosen so (CMF*256 + FLG) % 31 == 0

    const size_t n = data.size();

    if (level == 0) {
        // "Stored" blocks: up to 65535 raw bytes each.
        size_t pos = 0;
        do {
            size_t len = n - pos;
            if (len > 65535) len = 65535;
            bool final = (pos + len == n);
            out.push_back(final ? 1 : 0);       // BFINAL bit + BTYPE=00, byte aligned
            out.push_back((uint8_t)(len & 0xFF));
            out.push_back((uint8_t)(len >> 8));
            out.push_back((uint8_t)(~len & 0xFF));
            out.push_back((uint8_t)((~len >> 8) & 0xFF));
            out.insert(out.end(), data.begin() + pos, data.begin() + pos + len);
            pos += len;
        } while (pos < n);
    } else {
        BitWriter bw(out);
        bw.put_bits(1, 1);   // BFINAL = 1 (this is the last block)
        bw.put_bits(1, 2);   // BTYPE  = 01 (fixed Huffman codes)

        const int WINDOW = 32768;
        const int HASH_SIZE = 1 << 15;
        const int MAX_CHAIN = 48;
        std::vector<int> head(HASH_SIZE, -1);   // most recent position for each hash
        std::vector<int> prev(n > 0 ? n : 1, -1); // previous position with same hash

        auto hash3 = [&](size_t i) -> int {
            uint32_t h = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 | data[i + 2];
            return (int)((h * 2654435761u) >> 17) & (HASH_SIZE - 1);
        };
        auto insert = [&](size_t i) {
            if (i + 2 < n) {
                int h = hash3(i);
                prev[i] = head[h];
                head[h] = (int)i;
            }
        };

        size_t i = 0;
        while (i < n) {
            int best_len = 0, best_dist = 0;
            if (i + 2 < n) {
                int candidate = head[hash3(i)];
                int chain = 0;
                while (candidate >= 0 && (int)i - candidate <= WINDOW && chain < MAX_CHAIN) {
                    int len = 0;
                    int max_len = (int)((n - i) < 258 ? (n - i) : 258);
                    while (len < max_len && data[candidate + len] == data[i + len]) len++;
                    if (len > best_len) {
                        best_len = len;
                        best_dist = (int)i - candidate;
                        if (len == max_len) break;
                    }
                    candidate = prev[candidate];
                    chain++;
                }
            }
            if (best_len >= 3) {
                write_length(bw, best_len);
                write_distance(bw, best_dist);
                for (int k = 0; k < best_len; k++) insert(i + k);
                i += best_len;
            } else {
                write_literal_symbol(bw, data[i]);
                insert(i);
                i++;
            }
        }
        write_literal_symbol(bw, 256);   // end of block
        bw.flush();
    }

    uint32_t ad = adler32(data.data(), data.size());
    out.push_back((uint8_t)(ad >> 24));
    out.push_back((uint8_t)(ad >> 16));
    out.push_back((uint8_t)(ad >> 8));
    out.push_back((uint8_t)(ad));
    return out;
}

// ======================= PNG filters ========================================

inline uint8_t paeth_predictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return (uint8_t)a;
    if (pb <= pc) return (uint8_t)b;
    return (uint8_t)c;
}

// Apply the best of the 5 PNG filters to each row.
// 'rgb' is width*height*channels bytes. Output has one extra filter byte per row.
inline std::vector<uint8_t> filter_rows(const uint8_t* rgb, int width, int height, int channels) {
    const int stride = width * channels;
    std::vector<uint8_t> out;
    out.reserve((size_t)(stride + 1) * height);
    std::vector<uint8_t> candidate(stride);
    std::vector<uint8_t> best(stride);

    for (int y = 0; y < height; y++) {
        const uint8_t* row = rgb + (size_t)y * stride;
        const uint8_t* up  = y > 0 ? rgb + (size_t)(y - 1) * stride : nullptr;
        long best_score = -1;
        int best_type = 0;

        for (int type = 0; type < 5; type++) {
            long score = 0;
            for (int i = 0; i < stride; i++) {
                int a = i >= channels ? row[i - channels] : 0;          // left
                int b = up ? up[i] : 0;                                 // above
                int c = (up && i >= channels) ? up[i - channels] : 0;   // above-left
                int predicted = 0;
                switch (type) {
                    case 0: predicted = 0; break;
                    case 1: predicted = a; break;
                    case 2: predicted = b; break;
                    case 3: predicted = (a + b) / 2; break;
                    case 4: predicted = paeth_predictor(a, b, c); break;
                }
                uint8_t v = (uint8_t)(row[i] - predicted);
                candidate[i] = v;
                score += (v < 128) ? v : 256 - v;   // small values compress better
            }
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_type = type;
                best = candidate;
            }
        }
        out.push_back((uint8_t)best_type);
        out.insert(out.end(), best.begin(), best.end());
    }
    return out;
}

// ======================= PNG file ===========================================

inline void put_u32_be(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x));
}

// A chunk is: length (4 bytes), type (4 letters), data, CRC of type+data.
inline void write_chunk(std::vector<uint8_t>& file, const char type[4], const std::vector<uint8_t>& data) {
    put_u32_be(file, (uint32_t)data.size());
    std::vector<uint8_t> type_and_data(type, type + 4);
    type_and_data.insert(type_and_data.end(), data.begin(), data.end());
    file.insert(file.end(), type_and_data.begin(), type_and_data.end());
    put_u32_be(file, crc32_update(0, type_and_data.data(), type_and_data.size()));
}

// Encode 8-bit RGB (channels=3) or RGBA (channels=4) pixels as a PNG file in memory.
inline std::vector<uint8_t> encode(const uint8_t* pixels, int width, int height,
                                   int channels = 3, int level = 1) {
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr;
    put_u32_be(ihdr, (uint32_t)width);
    put_u32_be(ihdr, (uint32_t)height);
    ihdr.push_back(8);                          // bit depth: 8 bits per channel
    ihdr.push_back(channels == 4 ? 6 : 2);      // color type: 2 = RGB, 6 = RGBA
    ihdr.push_back(0);                          // compression method (always 0)
    ihdr.push_back(0);                          // filter method (always 0)
    ihdr.push_back(0);                          // no interlace
    write_chunk(file, "IHDR", ihdr);

    std::vector<uint8_t> raw = level == 0
        ? std::vector<uint8_t>()
        : filter_rows(pixels, width, height, channels);
    if (level == 0) {
        // No filters: filter byte 0 in front of each row.
        const int stride = width * channels;
        for (int y = 0; y < height; y++) {
            raw.push_back(0);
            raw.insert(raw.end(), pixels + (size_t)y * stride, pixels + (size_t)(y + 1) * stride);
        }
    }
    write_chunk(file, "IDAT", zlib_compress(raw, level));
    write_chunk(file, "IEND", std::vector<uint8_t>());
    return file;
}

inline bool write_file(const std::string& filename, const uint8_t* pixels,
                       int width, int height, int channels = 3, int level = 1) {
    std::vector<uint8_t> bytes = encode(pixels, width, height, channels, level);
    FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return written == bytes.size();
}

} // namespace png
} // namespace pixel
```

Notable details:

* `write_length` / `write_distance` walk the base tables backwards to find the right symbol, then
  write the extra bits.
* After a match, **every** position inside the match is added to the hash chains (`insert(i + k)`), so
  later matches can refer back into it.
* `level = 0` still produces the stored format from chapter 6, which is handy for comparisons.

---

## 6. The chapter program

**File: `chapters/ch07_png_compressed.cpp`**

```cpp
// ch07_png_compressed.cpp
// ------------------------------------------------------------
// Chapter 7: Real compression. We save the same images twice:
//   level 0 = stored (no compression)   level 1 = LZ77 + Huffman + filters
// and compare file sizes. Uses pixel/png.h (our own encoder).
// ------------------------------------------------------------
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "pixel/pixel.h"
using namespace pixel;

static void compare(const std::string& name, const Image& img) {
    std::vector<uint8_t> rgb = to_rgb8(img);
    std::vector<uint8_t> stored = png::encode(rgb.data(), img.width, img.height, 3, 0);
    std::vector<uint8_t> packed = png::encode(rgb.data(), img.width, img.height, 3, 1);
    png::write_file("images/ch07_" + name + ".png", rgb.data(), img.width, img.height, 3, 1);
    std::printf("%-10s  raw %8zu bytes   stored %8zu   compressed %8zu  (%.1f%% of raw)\n",
                name.c_str(), rgb.size(), stored.size(), packed.size(),
                100.0 * packed.size() / rgb.size());
}

int main() {
    ensure_parent_folder("images/x.png");
    const int W = 320, H = 240;

    // 1. Flat color: extremely repetitive -> compresses enormously.
    Image flat(W, H, hex_color(0x3B82F6));
    compare("flat", flat);

    // 2. Smooth gradient: each pixel differs a little from its neighbor.
    //    Raw bytes look different, but the Sub/Up filters make them repetitive.
    Image grad(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            grad.at(x, y) = srgb_to_linear(Color((double)x / W, (double)y / H, 0.5));
    compare("gradient", grad);

    // 3. Stripes: a repeating pattern -> LZ77 finds the repeats.
    Image stripes(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            stripes.at(x, y) = ((x / 8 + y / 8) % 3 == 0) ? hex_color(0xF59E0B) : hex_color(0x111827);
    compare("stripes", stripes);

    // 4. Random noise: no pattern at all -> can not be compressed.
    Image noise(W, H);
    for (auto& c : noise.data) c = random_vec();
    compare("noise", noise);
    return 0;
}
```

```bat
run ch07_png_compressed
```

### What you should see (console)

Something like this (exact numbers may differ slightly):

```
flat        raw   230400 bytes   stored   230723   compressed     ~900  (~0.4% of raw)
gradient    raw   230400 bytes   stored   230723   compressed    ~3000  (~1.3% of raw)
stripes     raw   230400 bytes   stored   230723   compressed    ~5000  (~2% of raw)
noise       raw   230400 bytes   stored   230723   compressed  ~245000  (~106% of raw)
```

(The "stored" size is exact: 230,640 bytes of rows with filter bytes, plus headers and checksums.
The compressed sizes are approximate and depend on the details.)

* **Flat**: after the first pixel everything is "copy from 3 back", a few hundred bytes in total.
* **Gradient**: the Sub/Up filters turn it into constant differences, which then compress like flat color.
* **Stripes**: LZ77 finds each repeated row.
* **Noise**: nothing to find. The output is slightly *bigger* than the input, because each literal
  costs 8–9 bits. This is true of every compressor: some inputs must grow.

### What you should see (images)

> **Image descriptions:** `ch07_flat.png` is a solid medium blue rectangle.
> `ch07_gradient.png` is a smooth gradient: black-blue top-left, red top-right, green bottom-left,
> yellow-ish bottom-right, with 50% blue throughout. `ch07_stripes.png` shows diagonal bands of amber
> and near-black, 8-pixel steps that look like a staircase pattern. `ch07_noise.png` looks like colorful TV
> static.

All four open in any image viewer. That proves our DEFLATE implementation is correct: a single wrong
bit anywhere would make viewers reject the file.

---

## Try it yourself

1. Force every row to use filter 0 (None) and compare the gradient's size. How much do filters help?
2. Change `MAX_CHAIN` from 48 to 4 and to 512. Measure file size and time for a big image. This is the
   classic speed vs compression trade-off (it's what "compression level" means in zip programs).
3. Render the chapter 5 checkerboard and save it as PNG. How small is it compared with the BMP?
4. **Challenge:** implement dynamic Huffman codes (block type 2). Count symbol frequencies, build
   code lengths (package-merge or a simple Huffman tree limited to 15 bits), and write the code-length
   header described in RFC 1951, section 3.2.7.

## Common problems

| Symptom | Cause |
|---------|-------|
| Viewer says "invalid code" / "invalid distance" | Huffman code bits not reversed, or wrong fixed table ranges |
| Image opens but lower part is garbage | A match copied with the wrong distance, or chains not updated inside matches |
| "Incorrect data check" | Adler-32 must be of the filtered rows (the data you compressed) |
| Colors slightly off vs expected | Filter arithmetic must wrap mod 256 (`uint8_t` does it for you) |

---

## Summary

* **Filters** predict each byte from its neighbors and store only the error. Smooth images
  become runs of small numbers.
* **LZ77** replaces repeats with (length, distance) pairs, found fast with hash chains.
* **Huffman codes** give frequent symbols fewer bits; DEFLATE has a fixed table we can use directly.
* Our encoder is ~250 lines, fully standard, and every image from now on is a real PNG.

We can now make and save any image. Time to **draw**: [Chapter 8 — Drawing basics →](08-drawing-basics.md)
