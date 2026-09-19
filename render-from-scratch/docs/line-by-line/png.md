# Line by line: `include/pixel/png.h`

[← Line-by-line index](README.md) · [Chapter 7 (the theory)](../07-png-part2.md) · [ch06 (the simple version)](ch06_png_stored.md)

**What this file does, in one sentence:** it turns pixel bytes into a **compressed** PNG file, using
checksums, row filters, LZ77 ("copy from earlier") and Huffman codes (short codes for common symbols).

This is the most difficult file of Part 1. Read chapter 7 first for the ideas. Here we go through the code.
Many parts are the same as in [ch06](ch06_png_stored.md); we'll point to it when so.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes, namespaces | 1–18 | tools; `pixel::png` |
| B. CRC-32 | 20–39 | chunk checksum (with a self-building table) |
| C. Adler-32 | 41–50 | zlib checksum |
| D. `BitWriter` | 52–87 | write single bits into bytes |
| E. Fixed Huffman symbols | 89–97 | write a literal byte or a special symbol |
| F. Lengths and distances | 99–120 | write "copy N bytes from D back" |
| G. `zlib_compress` start + stored mode | 122–145 | header, and the no-compression option |
| H. `zlib_compress` LZ77 search | 146–201 | find repeats and write them |
| I. Adler at the end | 203–209 | finish the zlib stream |
| J. Paeth predictor | 211–219 | one of the 5 filters |
| K. `filter_rows` | 221–264 | choose the best filter per row |
| L. Chunks | 266–282 | big-endian numbers, `write_chunk` |
| M. `encode` | 284–313 | build the whole PNG in memory |
| N. `write_file` | 315–323 | save it |
| O. End | 325–326 | close namespaces |

---

## Block A — Comments, includes, namespaces (lines 1–18)

* Lines 1–9: what's inside the file.
* Line 10: `#pragma once`.
* Lines 11–15: `<cstdint>` (byte types), `<cstdlib>` (`std::abs`), `<cstdio>` (`fopen`...), `<string>`, `<vector>`.
* Lines 17–18: **two** namespaces: `pixel`, and inside it `png`. So these functions are called
  `pixel::png::encode` and so on. That keeps names like `write_chunk` from clashing with other code.

---

## Block B — CRC-32 (lines 20–39)

Same math as [ch06 block B](ch06_png_stored.md), but packed into one function that builds its table
automatically the first time.

```cpp
inline uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
```

Line 23: inputs: a starting CRC (0 for a new one), a **pointer** to the first byte (`const uint8_t*`), and how
many bytes (`len`). Using a pointer + length works with any kind of byte storage.

```cpp
    static uint32_t table[256];
    static bool ready = false;
```

* Lines 24–25: `static` inside a function means these variables are created **once** and **keep their values**
  between calls. `ready` remembers whether the table has been filled yet.

```cpp
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        ready = true;
    }
```

* Lines 26–34: only the first time: fill the table (same as ch06 lines 17–24), then mark it ready.

```cpp
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
```

* Lines 35–38: same as ch06 lines 27–29. `data[i]` = the i-th byte after the pointer.

---

## Block C — Adler-32 (lines 41–50)

The same as [ch06 block C](ch06_png_stored.md), using a pointer and length. `MOD` (line 43) is a named constant
for 65521 so the number isn't repeated.

---

## Block D — `BitWriter` (lines 52–87)

Compressed data isn't made of whole bytes: some codes are 7 bits, some 9, some 5. The `BitWriter` collects
bits and outputs a byte whenever 8 are ready.

DEFLATE fills each byte starting from its **lowest** bit.

```cpp
struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buffer = 0;   // bits waiting to be written
    int count = 0;         // how many bits are waiting
```

* Line 56: `out` is a **reference** to the output vector (the writer adds bytes directly to someone else's vector).
* Line 57: `buffer` holds bits that don't yet make a complete byte.
* Line 58: `count` = how many bits are in the buffer.

```cpp
    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}
```

Line 60: the constructor stores the reference. `explicit` prevents C++ from converting a vector into a
BitWriter by accident.

### Lines 62–71: `put_bits`

```cpp
    void put_bits(uint32_t value, int nbits) {
        buffer |= value << count;
        count += nbits;
        while (count >= 8) {
            out.push_back((uint8_t)(buffer & 0xFF));
            buffer >>= 8;
            count -= 8;
        }
    }
```

* Line 64: put the new bits **above** the bits already waiting. `value << count` shifts them into place; `|=` adds
  them in (bitwise OR).
* Line 65: now we have more bits waiting.
* Lines 66–70: while at least 8 bits are waiting:
  * Line 67: output the lowest 8 bits as one byte.
  * Line 68: remove them from the buffer (shift right by 8).
  * Line 69: 8 fewer bits waiting.

Example: buffer has 3 bits `101`, then `put_bits(0b11110, 5)` → buffer = `11110 101` = 8 bits → one byte
`11110101` is written, and the buffer is empty again.

### Lines 73–80: `put_huffman`

```cpp
    void put_huffman(uint32_t code, int length) {
        uint32_t reversed = 0;
        for (int i = 0; i < length; i++)
            reversed = (reversed << 1) | ((code >> i) & 1u);
        put_bits(reversed, length);
    }
```

* Huffman codes are defined to be read **highest bit first**, but `put_bits` writes **lowest bit first**. So we
  reverse the bit order before writing.
* Lines 77–78: take bit i of `code` (`(code >> i) & 1`) and push it onto `reversed` from the right. After the loop
  the bits are in reverse order.
* Example: code `110` (3 bits) → reversed `011`.
* Line 79: write the reversed code.

### Lines 82–86: `flush`

```cpp
    void flush() {
        if (count > 0) out.push_back((uint8_t)(buffer & 0xFF));
        buffer = 0;
        count = 0;
    }
```

At the end, a few bits may still wait (fewer than 8). Write them as a last byte (the unused top bits are 0),
and reset.

---

## Block E — Fixed Huffman symbols (lines 89–97)

```cpp
inline void write_literal_symbol(BitWriter& bw, int sym) {
    if (sym <= 143)      bw.put_huffman(0x30 + sym, 8);
    else if (sym <= 255) bw.put_huffman(0x190 + (sym - 144), 9);
    else if (sym <= 279) bw.put_huffman(sym - 256, 7);
    else                 bw.put_huffman(0xC0 + (sym - 280), 8);
}
```

DEFLATE has 288 symbols: 0–255 are the byte values ("literals"), 256 means "end of block", 257–285 mean "a
repeat of some length". The "fixed Huffman" table (from the official specification) gives each one a code:

| Symbols | Code length | First code | Line |
|---------|-------------|------------|------|
| 0–143 | 8 bits | `0x30` (00110000) | 93 |
| 144–255 | 9 bits | `0x190` (110010000) | 94 |
| 256–279 | 7 bits | `0` (0000000) | 95 |
| 280–287 | 8 bits | `0xC0` (11000000) | 96 |

Each line: the code = first code of the range + (symbol − first symbol of the range), written with the right
number of bits. Note: the most common symbols in images aren't necessarily the short ones; this is a
general-purpose fixed table (chapter 7 mentions "dynamic" tables as an improvement).

---

## Block F — Lengths and distances (lines 99–120)

A repeat is written as **(length, distance)**: "copy `length` bytes from `distance` bytes back".

### Lines 99–108: the length

```cpp
inline void write_length(BitWriter& bw, int length) {       // 3..258
    static const int base[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                  35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
                                  3,3,3,3,4,4,4,4,5,5,5,5,0};
```

* Lengths 3–258 don't each get their own symbol. There are only 29 length symbols (257–285). Each covers a range:
  it has a **base** length and some **extra bits** that say how much to add to the base.
* Lines 100–101: base lengths for symbols 257, 258, ... 285.
* Lines 102–103: how many extra bits each needs. Example: symbol index 8 has base 11 and 1 extra bit, so it covers
  lengths 11 and 12.
* `static const` = these tables are created once and never change.

```cpp
    int i = 28;
    while (base[i] > length) i--;
```

Lines 104–105: start from the last entry and walk back until `base[i] <= length`. That's the range our length
belongs to.

```cpp
    write_literal_symbol(bw, 257 + i);
    if (extra[i] > 0) bw.put_bits((uint32_t)(length - base[i]), extra[i]);
}
```

* Line 106: write the length **symbol** (257 + index), with the same code table as literals.
* Line 107: write the **extra bits**: how far above the base. Example: length 25 → base 23 (index 13), 2 extra bits,
  value 2 → symbol 270 + bits `10`.

### Lines 110–120: the distance

The same idea for distances 1–32,768: 30 distance symbols with bases and extra bits (lines 111–115). One
difference, on line 118: distance symbols have their own simple fixed code, **5 bits** each (just the index
number).

---

## Block G — `zlib_compress`: start and stored mode (lines 122–145)

```cpp
inline std::vector<uint8_t> zlib_compress(const std::vector<uint8_t>& data, int level = 1) {
    std::vector<uint8_t> out;
    out.push_back(0x78);    // CMF: deflate, 32K window
    out.push_back(0x01);    // FLG: chosen so (CMF*256 + FLG) % 31 == 0
    const size_t n = data.size();
```

* Line 124: input `data` (the filtered pixel rows); `level` 0 = no compression, 1 = compress.
* Lines 126–127: the zlib header (as in ch06).
* Line 129: `n` = number of input bytes.

```cpp
    if (level == 0) {
        size_t pos = 0;
        do {
            ...same as ch06 lines 81–90...
        } while (pos < n);
```

* Lines 131–145: level 0 writes "stored" blocks exactly like [ch06 block F](ch06_png_stored.md).
* It uses a **do-while** loop: the body runs **at least once**, then repeats while the condition is true. That
  way even empty data gets one (empty) block.

---

## Block H — `zlib_compress`: the LZ77 search (lines 146–201)

This is the real compressor. Big picture:

```
for each position i in the data:
    look for an earlier place where the same bytes appear (up to 32 KB back)
    if the longest match is 3+ bytes: write (length, distance), skip ahead by length
    else: write the byte itself (a literal), move ahead by 1
write "end of block"
```

### Lines 147–149: one block, fixed codes

```cpp
        BitWriter bw(out);
        bw.put_bits(1, 1);   // BFINAL = 1 (this is the last block)
        bw.put_bits(1, 2);   // BTYPE  = 01 (fixed Huffman codes)
```

* Line 147: a bit writer that appends to `out`.
* Line 148: 1 bit: "this is the final block" (we put everything in one block).
* Line 149: 2 bits: block type 1 = fixed Huffman.

### Lines 151–155: settings and search tables

```cpp
        const int WINDOW = 32768;
        const int HASH_SIZE = 1 << 15;
        const int MAX_CHAIN = 48;
        std::vector<int> head(HASH_SIZE, -1);   // most recent position for each hash
        std::vector<int> prev(n > 0 ? n : 1, -1); // previous position with same hash
```

* Line 151: we may copy from at most 32,768 bytes back (a DEFLATE rule).
* Line 152: `1 << 15` = 32,768 hash buckets.
* Line 153: check at most 48 candidates per position. More = better compression but slower.
* Line 154: `head[h]` = the **latest** position whose next 3 bytes have hash h. −1 = none yet.
* Line 155: `prev[pos]` = the **previous** position with the same hash as `pos`. Together, `head` and `prev` form a
  chain: newest → older → older → ... (−1 ends the chain). `n > 0 ? n : 1` avoids a zero-size vector.

### Lines 157–160: the hash of 3 bytes

```cpp
        auto hash3 = [&](size_t i) -> int {
            uint32_t h = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 | data[i + 2];
            return (int)((h * 2654435761u) >> 17) & (HASH_SIZE - 1);
        };
```

* A **hash** turns the 3 bytes at position i into a number 0–32,767, so that equal byte triples give equal numbers.
* Line 158: pack the 3 bytes into one number (first byte highest).
* Line 159: multiply by a large "magic" constant (it mixes the bits well), keep the top part (`>> 17`), and keep
  only 15 bits (`& (HASH_SIZE - 1)`, which is `& 32767`).

### Lines 161–167: remember a position

```cpp
        auto insert = [&](size_t i) {
            if (i + 2 < n) {
                int h = hash3(i);
                prev[i] = head[h];
                head[h] = (int)i;
            }
        };
```

* Line 162: only if 3 bytes are available from i.
* Line 164: the old newest position with this hash becomes the "previous" of i.
* Line 165: i is now the newest. (Like adding to the front of a linked list.)

### Lines 169–187: find the best match at position i

```cpp
        size_t i = 0;
        while (i < n) {
            int best_len = 0, best_dist = 0;
            if (i + 2 < n) {
                int candidate = head[hash3(i)];
                int chain = 0;
```

* Line 169: start at the beginning.
* Line 170: until all bytes are processed.
* Line 171: the best match found so far (none yet).
* Line 172: we need at least 3 bytes left to look for a match.
* Line 173: the first candidate = the newest earlier position with the same hash.
* Line 174: how many candidates we've checked.

```cpp
                while (candidate >= 0 && (int)i - candidate <= WINDOW && chain < MAX_CHAIN) {
```

Line 175: keep checking while there **is** a candidate, it's within 32 KB, and we haven't checked too many.

```cpp
                    int len = 0;
                    int max_len = (int)((n - i) < 258 ? (n - i) : 258);
                    while (len < max_len && data[candidate + len] == data[i + len]) len++;
```

* Line 177: the longest match allowed: 258 (a DEFLATE rule), or fewer if the data ends sooner.
* Line 178: count how many bytes are equal, starting at the candidate and at i. (The hash only *suggests* a match;
  here we check the real bytes.)

```cpp
                    if (len > best_len) {
                        best_len = len;
                        best_dist = (int)i - candidate;
                        if (len == max_len) break;
                    }
                    candidate = prev[candidate];
                    chain++;
                }
            }
```

* Lines 179–183: if this match is longer, remember it. The distance = how far back it is. If it's the maximum
  possible, stop searching (`break` leaves the while loop).
* Line 184: go to the next older candidate in the chain.
* Line 185: count it.

### Lines 188–198: write a match or a literal

```cpp
            if (best_len >= 3) {
                write_length(bw, best_len);
                write_distance(bw, best_dist);
                for (int k = 0; k < best_len; k++) insert(i + k);
                i += best_len;
```

* Line 188: DEFLATE's minimum match length is 3 (shorter isn't worth it).
* Lines 189–190: write "copy best_len bytes from best_dist back".
* Line 191: add **every** position inside the match to the hash chains, so future matches can point into it.
* Line 192: jump ahead past the copied bytes.

```cpp
            } else {
                write_literal_symbol(bw, data[i]);
                insert(i);
                i++;
            }
        }
```

* Lines 193–197: no good match: write the byte itself, remember its position, move ahead by one.

### Lines 199–201: end the block

```cpp
        write_literal_symbol(bw, 256);   // end of block
        bw.flush();
    }
```

* Line 199: symbol 256 = "end of block".
* Line 200: write any leftover bits.

---

## Block I — Adler at the end (lines 203–209)

```cpp
    uint32_t ad = adler32(data.data(), data.size());
    out.push_back((uint8_t)(ad >> 24));
    out.push_back((uint8_t)(ad >> 16));
    out.push_back((uint8_t)(ad >> 8));
    out.push_back((uint8_t)(ad));
    return out;
}
```

Adler-32 of the **uncompressed** input, written big-endian (4 bytes), then return the finished zlib stream.
(Used for both levels.)

---

## Block J — Paeth predictor (lines 211–219)

```cpp
inline uint8_t paeth_predictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return (uint8_t)a;
    if (pb <= pc) return (uint8_t)b;
    return (uint8_t)c;
}
```

A **predictor** guesses a byte from its neighbors: `a` = left, `b` = above, `c` = above-left.

* Line 214: `p` = a guess that works perfectly on smooth slopes.
* Line 215: how far each neighbor is from that guess.
* Lines 216–218: return the neighbor **closest** to the guess (ties go to a, then b). The order matters: it's
  part of the PNG standard, and decoders do exactly the same.

---

## Block K — `filter_rows` (lines 221–264)

**What this block does:** for every row, it tries all 5 PNG filters, keeps the one that produces the "smallest"
numbers (which compress best), and writes the filter number followed by the filtered row.

```cpp
inline std::vector<uint8_t> filter_rows(const uint8_t* rgb, int width, int height, int channels) {
    const int stride = width * channels;
    std::vector<uint8_t> out;
    out.reserve((size_t)(stride + 1) * height);
    std::vector<uint8_t> candidate(stride);
    std::vector<uint8_t> best(stride);
```

* Line 224: `stride` = bytes per row (e.g. 3 × width).
* Line 226: `reserve` makes room in advance (a speed-up; the size is still 0).
* Line 227: `candidate` = the row filtered with the filter we are trying.
* Line 228: `best` = the best filtered row so far.

```cpp
    for (int y = 0; y < height; y++) {
        const uint8_t* row = rgb + (size_t)y * stride;
        const uint8_t* up  = y > 0 ? rgb + (size_t)(y - 1) * stride : nullptr;
        long best_score = -1;
        int best_type = 0;
```

* Line 231: `row` = pointer to the start of row y (pointer + number = move that many bytes forward).
* Line 232: `up` = pointer to the row above, or `nullptr` (nothing) for the first row.
* Lines 233–234: no best filter yet (score −1 means "none").

```cpp
        for (int type = 0; type < 5; type++) {
            long score = 0;
            for (int i = 0; i < stride; i++) {
                int a = i >= channels ? row[i - channels] : 0;          // left
                int b = up ? up[i] : 0;                                 // above
                int c = (up && i >= channels) ? up[i - channels] : 0;   // above-left
```

* Line 236: try filter types 0, 1, 2, 3, 4.
* Line 238: for each byte in the row.
* Line 239: `a` = the same color channel of the pixel to the **left** (`channels` bytes back), or 0 at the left edge.
* Line 240: `b` = the byte **above**, or 0 in the first row.
* Line 241: `c` = the byte **above-left**, or 0.

```cpp
                int predicted = 0;
                switch (type) {
                    case 0: predicted = 0; break;
                    case 1: predicted = a; break;
                    case 2: predicted = b; break;
                    case 3: predicted = (a + b) / 2; break;
                    case 4: predicted = paeth_predictor(a, b, c); break;
                }
```

Lines 242–249: the five filters' guesses: None (0), Sub (left), Up (above), Average, Paeth. `break` leaves the
`switch`.

```cpp
                uint8_t v = (uint8_t)(row[i] - predicted);
                candidate[i] = v;
                score += (v < 128) ? v : 256 - v;   // small values compress better
            }
```

* Line 250: store the **difference** from the guess. Converting to `uint8_t` wraps around: −2 becomes 254.
* Line 251: keep it in the candidate row.
* Line 252: score = sum of "how far from zero". 254 is really −2, so it counts as 2 (`256 - v`). Smaller total =
  better.

```cpp
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_type = type;
                best = candidate;
            }
        }
```

Lines 254–258: if this filter is the first one tried or the best so far, remember it (copying the whole row).

```cpp
        out.push_back((uint8_t)best_type);
        out.insert(out.end(), best.begin(), best.end());
    }
    return out;
}
```

* Line 260: write the filter type byte (0–4) at the start of the row.
* Line 261: write the filtered row.
* Line 263: return all rows.

---

## Block L — Chunks (lines 266–282)

* Lines 268–273 `put_u32_be`: a 4-byte number, **b**ig-**e**ndian (as in ch06 `put32`).
* Lines 276–282 `write_chunk`: exactly like [ch06 lines 47–53](ch06_png_stored.md), using `crc32_update(0, ...)`.

---

## Block M — `encode` (lines 284–313)

```cpp
inline std::vector<uint8_t> encode(const uint8_t* pixels, int width, int height,
                                   int channels = 3, int level = 1) {
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
```

* Lines 285–286: inputs: pixel bytes, size, channels (3 = RGB, 4 = RGBA with transparency), and level.
* Line 287: the PNG signature.

```cpp
    std::vector<uint8_t> ihdr;
    put_u32_be(ihdr, (uint32_t)width);
    put_u32_be(ihdr, (uint32_t)height);
    ihdr.push_back(8);
    ihdr.push_back(channels == 4 ? 6 : 2);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    write_chunk(file, "IHDR", ihdr);
```

Lines 289–297: the IHDR chunk, as in ch06. Line 293: color type 6 (RGBA) if 4 channels, else 2 (RGB).

```cpp
    std::vector<uint8_t> raw = level == 0
        ? std::vector<uint8_t>()
        : filter_rows(pixels, width, height, channels);
```

Lines 299–301: if compressing, filter the rows (block K). If not, start with an empty list.

```cpp
    if (level == 0) {
        const int stride = width * channels;
        for (int y = 0; y < height; y++) {
            raw.push_back(0);
            raw.insert(raw.end(), pixels + (size_t)y * stride, pixels + (size_t)(y + 1) * stride);
        }
    }
```

Lines 302–309: no compression: for each row, a 0 filter byte plus the row copied unchanged (as in ch06).

```cpp
    write_chunk(file, "IDAT", zlib_compress(raw, level));
    write_chunk(file, "IEND", std::vector<uint8_t>());
    return file;
}
```

* Line 310: compress the rows and put them in the IDAT chunk.
* Line 311: the empty IEND chunk.
* Line 312: return the complete file (still in memory).

---

## Block N — `write_file` (lines 315–323)

```cpp
inline bool write_file(const std::string& filename, const uint8_t* pixels,
                       int width, int height, int channels = 3, int level = 1) {
    std::vector<uint8_t> bytes = encode(pixels, width, height, channels, level);
    FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return written == bytes.size();
}
```

* Line 317: build the PNG in memory.
* Lines 318–319: open the file for binary writing; give up if that fails.
* Line 320: write it; `fwrite` returns how many bytes were actually written.
* Line 321: close.
* Line 322: success only if everything was written (for example, a full disk would write less).

---

## Block O — End (lines 325–326)

```cpp
} // namespace png
} // namespace pixel
```

Close the inner namespace, then the outer one.

---

## The whole flow

```
pixels (bytes)
  → filter_rows:  per row try None/Sub/Up/Average/Paeth, keep the smallest, prefix the type byte
  → zlib_compress: 78 01
                   one block: [final=1][type=01]
                     for each position: best earlier match (hash chains)?
                        yes (≥3): length code + extra bits, distance code + extra bits
                        no:       literal code
                     end-of-block code (256)
                   Adler-32
  → encode:  signature + IHDR + IDAT(zlib) + IEND, each chunk with CRC-32
  → write_file
```

## Check your understanding

1. Why does `put_huffman` reverse the bits? *(Huffman codes are defined highest bit first, but DEFLATE packs
   bits lowest first.)*
2. What does a match of length 5, distance 3 mean? *(Copy 5 bytes starting 3 bytes back. It may overlap itself,
   which repeats a 3-byte pattern.)*
3. Why do we `insert` every position inside a match? *(So later matches can start inside it.)*
4. What does a filtered value of 255 mean? *(−1: the byte is one less than the prediction.)*
5. What happens with `MAX_CHAIN` larger? *(Better compression, slower.)*
