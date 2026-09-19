# Line by line: `ch06_png_stored.cpp`

[← Line-by-line index](README.md) · [Chapter 6 (the theory)](../06-png-part1.md)

**What the whole program does, in one sentence:** it writes a real PNG file by hand, byte by byte,
without compression, so you can see exactly how a PNG file is built.

A PNG file is: **signature** + **IHDR chunk** (size, color type) + **IDAT chunk** (the pixels, wrapped in
"zlib") + **IEND chunk** (the end). Each chunk carries a **CRC** checksum.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–12 | notes and tools |
| B. CRC-32 | 14–30 | a checksum that detects damaged chunks |
| C. Adler-32 | 32–40 | a second, simpler checksum, for the pixel data |
| D. Helpers | 42–53 | write big-endian numbers; write one chunk |
| E. Start and pixels | 55–73 | compute a sunset picture, row by row with a filter byte |
| F. zlib wrapping | 75–92 | wrap the pixels in "stored" (uncompressed) blocks |
| G. Assemble the file | 94–107 | signature + IHDR + IDAT + IEND |
| H. Save | 109–117 | write to disk, print the size |

---

## Block A — Comments, includes (lines 1–12)

* Lines 1–7: comments: the plan of a PNG file.
* Line 8 `<cstdint>`: `uint8_t`, `uint32_t`.
* Line 9 `<cstdio>`: `printf`, and the C-style file functions `fopen`, `fwrite`, `fclose`.
* Line 10 `<cmath>`: math (not really needed here, harmless).
* Line 11 `<vector>`: lists of bytes.
* Line 12 `<filesystem>`: to create the `images` folder.

No library of ours: this file is completely standalone.

---

## Block B — CRC-32 (lines 14–30)

A **checksum** is a small number calculated from a lot of data. If even one bit of the data changes, the
checksum changes. PNG stores a CRC-32 at the end of each chunk; programs that open the file recompute it and
compare, to detect damaged files.

### Line 15: a table

```cpp
uint32_t crc_table[256];
```

An array of 256 numbers, created **outside** any function (a "global" variable), so every function can use it.
We fill it once, then use it to compute CRCs fast.

### Lines 17–24: fill the table

```cpp
void make_crc_table() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
}
```

* Line 17: a function with no inputs and no result (`void`).
* Line 18: for each possible byte value n = 0 … 255.
* Line 19: start with c = n.
* Lines 20–21: repeat 8 times (once per bit of the byte):
  * `c & 1` = the lowest bit of c (1 or 0).
  * If it's 1: shift c right by one bit (`c >> 1`) and XOR (`^`) it with the magic number `0xEDB88320`.
  * If it's 0: just shift right.
  * This is "division" in a special kind of math where subtraction is XOR. The magic number is the official
    CRC-32 **polynomial**. You don't need the theory: it's the standard recipe.
  * The `u` at the end of `0xEDB88320u` means "unsigned" (a positive-only number).
* Line 22: store the result for byte value n.

### Lines 26–30: compute the CRC of some bytes

```cpp
uint32_t crc32(const std::vector<uint8_t>& bytes) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : bytes) c = crc_table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
```

* Line 27: start with all 32 bits set to 1 (`0xFFFFFFFF`). That's part of the standard.
* Line 28: for each byte `b`:
  * `(c ^ b) & 0xFF`: mix the byte into the lowest 8 bits of c and take those 8 bits. That gives a number 0–255.
  * look it up in the table, and XOR it with `c >> 8` (c shifted down by one byte).
  * This processes one whole byte in one step, thanks to the table.
* Line 29: flip all bits at the end (`^ 0xFFFFFFFF`), also part of the standard, and return.

> Test value: the CRC-32 of the text `123456789` must be `0xCBF43926`.

---

## Block C — Adler-32 (lines 32–40)

```cpp
uint32_t adler32(const std::vector<uint8_t>& bytes) {
    uint32_t a = 1, b = 0;
    for (uint8_t x : bytes) {
        a = (a + x) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}
```

The zlib wrapper (inside the IDAT chunk) needs a different checksum, Adler-32. It's simpler:

* Line 34: two running sums: `a` starts at 1, `b` at 0.
* Line 35: for each byte `x`:
  * Line 36: `a` = a + x (the sum of the bytes). `% 65521` keeps it below 65521 (the remainder after dividing).
    65521 is the largest prime number below 65536.
  * Line 37: `b` = b + a (the sum of all the `a` values so far). This makes the **order** of bytes matter.
* Line 39: combine: `b << 16` moves b into the top 16 bits, `| a` puts a into the lower 16 bits.

---

## Block D — Helpers (lines 42–53)

### Lines 43–45: a 4-byte number, **biggest byte first**

```cpp
void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
```

* PNG stores numbers **big-endian**: the highest byte first (the opposite of BMP!).
* `x >> 24` = the top byte. `x >> 16`, `x >> 8`, `x` = the next ones. When stored into a `uint8_t`, only the
  lowest 8 bits are kept, which is exactly the byte we want.
* Example: 300 = `0x0000012C` → bytes `00 00 01 2C`.

### Lines 47–53: write one chunk

```cpp
void write_chunk(std::vector<uint8_t>& file, const char* type, const std::vector<uint8_t>& data) {
```

* Inputs: `file` (the bytes of the whole file so far, which we add to), `type` (4 letters like `"IHDR"`),
  and `data` (the content of the chunk).
* `const char*` = a pointer to text (the 4 letters).

A chunk is: **length** (4 bytes) + **type** (4 letters) + **data** + **CRC** (4 bytes).

```cpp
    put32(file, (uint32_t)data.size());                    // 1. length
```

Line 48: the length of the data, big-endian.

```cpp
    std::vector<uint8_t> type_and_data(type, type + 4);   // 2. type (4 letters)
    type_and_data.insert(type_and_data.end(), data.begin(), data.end()); // 3. data
```

* Line 49: a new list holding the 4 letters of the type. `(type, type + 4)` = "copy from the first letter up to
  (not including) the 5th".
* Line 50: append the data after the type. `insert(where, from, to)` copies a range of elements.
  We build "type + data" together because the CRC is computed over both.

```cpp
    file.insert(file.end(), type_and_data.begin(), type_and_data.end());
    put32(file, crc32(type_and_data));                     // 4. CRC
}
```

* Line 51: add type + data to the file.
* Line 52: add the CRC of type + data.

---

## Block E — Start and pixels (lines 55–73)

```cpp
int main() {
    make_crc_table();
    const int width = 300, height = 200;
```

* Line 56: fill the CRC table once, before any chunk is written.
* Line 57: image size.

```cpp
    std::vector<uint8_t> raw;
    for (int y = 0; y < height; y++) {
        raw.push_back(0);                       // filter type 0 = "None"
```

* Line 60: `raw` will hold all pixel rows.
* Line 61: for each row.
* Line 62: PNG requires **one extra byte at the start of every row**: the **filter type**. 0 = "no filter"
  (chapter 7 explains filters).

```cpp
        for (int x = 0; x < width; x++) {
            double t = (double)y / (height - 1);
            double r = 0.35 + 0.65 * t, g = 0.15 + 0.45 * t * t, b = 0.45 - 0.3 * t;
```

* Line 63: for each pixel in the row.
* Line 65: `t` = 0 at the top row, 1 at the bottom row.
* Line 66: the sky colors change with t:
  * red grows from 0.35 to 1.0,
  * green grows from 0.15 to 0.6 (using t², so slowly at first),
  * blue shrinks from 0.45 to 0.15.
  * Top: (0.35, 0.15, 0.45) = purple. Bottom: (1.0, 0.6, 0.15) = orange.

```cpp
            double dx = x - 150.0, dy = y - 120.0;
            if (dx * dx + dy * dy < 40 * 40) { r = 1.0; g = 0.9; b = 0.6; }
```

* Line 67: distance parts from the point (150, 120).
* Line 68: inside a circle of radius 40? Then use a pale yellow: the **sun**.

```cpp
            raw.push_back((uint8_t)(255 * r));
            raw.push_back((uint8_t)(255 * g));
            raw.push_back((uint8_t)(255 * b));
        }
    }
```

* Lines 69–71: append the 3 bytes (R, G, B). Here we directly use byte values without the linear/sRGB idea,
  to keep this file simple.
* Lines 72–73: end of the loops. `raw` now holds 200 rows × (1 + 900) bytes = 180,200 bytes.

---

## Block F — zlib wrapping (lines 75–92)

The pixel data inside IDAT must be a **zlib stream**: 2 header bytes, then DEFLATE **blocks**, then the Adler-32.
We use the simplest block type, "stored" = no compression.

```cpp
    std::vector<uint8_t> z;
    z.push_back(0x78);   // CMF: method 8 (deflate), window 32K
    z.push_back(0x01);   // FLG: makes (0x78*256 + 0x01) divisible by 31
```

* Line 76: `z` = the zlib stream.
* Line 77: `0x78` = "compression method 8 (deflate), window size 32 KB".
* Line 78: `0x01` = flags. The rule: the 2-byte number `0x7801` must be divisible by 31 (a mini check).
  30721 = 31 × 991. ✓

```cpp
    size_t pos = 0;
    while (pos < raw.size()) {
```

* Line 79: `pos` = how many bytes of `raw` we've already packed.
* Line 80: repeat while there are bytes left. (A **while** loop repeats as long as its condition is true.)

```cpp
        size_t len = raw.size() - pos;
        if (len > 65535) len = 65535;            // a stored block holds at most 65535 bytes
        bool last = (pos + len == raw.size());
```

* Line 81: how many bytes are left.
* Line 82: a stored block can hold at most 65,535 bytes, so we take at most that many. (Our 180,200 bytes become 3
  blocks: 65,535 + 65,535 + 49,130.)
* Line 83: is this the **last** block? True if this block reaches the end of `raw`.

```cpp
        z.push_back(last ? 1 : 0);               // BFINAL bit, BTYPE = 00 (stored)
```

Line 84: the block header byte. Bit 0 = "final block?" (1 or 0). The next 2 bits = block type 00 = stored. The
other bits are padding. So the byte is just 1 (last) or 0 (not last).

```cpp
        z.push_back(len & 0xFF);                 // LEN (little-endian!)
        z.push_back((len >> 8) & 0xFF);
        z.push_back(~len & 0xFF);                // NLEN = one's complement of LEN
        z.push_back((~len >> 8) & 0xFF);
```

* Lines 85–86: the block length in 2 bytes, **little-endian** (lowest byte first). Yes, inside PNG, DEFLATE uses
  the opposite order to PNG. Different inventors!
* Lines 87–88: `~len` = all bits of len flipped. This copy lets a reader check that the length wasn't damaged.

```cpp
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    }
```

* Line 89: copy `len` raw bytes, starting at `pos`, into the stream.
* Line 90: move forward.
* Line 91: end of the while loop.

```cpp
    put32(z, adler32(raw));
```

Line 92: finish the zlib stream with the Adler-32 of the **uncompressed** data, big-endian.

---

## Block G — Assemble the file (lines 94–107)

```cpp
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
```

Line 95: start the file with the 8-byte **PNG signature**. Every PNG begins with these exact bytes. `'\r'` is
byte 13, `'\n'` is byte 10.

```cpp
    std::vector<uint8_t> ihdr;
    put32(ihdr, width);
    put32(ihdr, height);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(2);   // color type 2 = RGB
    ihdr.push_back(0);   // compression
    ihdr.push_back(0);   // filter method
    ihdr.push_back(0);   // interlace: none
```

Lines 97–104 build the IHDR data (13 bytes):

| Bytes | Value | Meaning |
|-------|-------|---------|
| 4 | 300 | width |
| 4 | 200 | height |
| 1 | 8 | 8 bits per channel (one byte each) |
| 1 | 2 | color type RGB |
| 1 | 0 | compression method (only 0 exists) |
| 1 | 0 | filter method (only 0 exists) |
| 1 | 0 | no interlacing |

```cpp
    write_chunk(file, "IHDR", ihdr);
    write_chunk(file, "IDAT", z);
    write_chunk(file, "IEND", {});
```

* Line 105: the header chunk.
* Line 106: the data chunk with our zlib stream.
* Line 107: the end chunk, with **no data**. `{}` means an empty list.

---

## Block H — Save (lines 109–117)

```cpp
    std::filesystem::create_directories("images");
    FILE* f = std::fopen("images/ch06_stored.png", "wb");
    if (!f) { std::printf("cannot open output file\n"); return 1; }
```

* Line 109: make sure the folder exists.
* Line 110: open the file the old C way. `"wb"` = **w**rite, **b**inary. `FILE*` is a pointer to the open file.
* Line 111: if it failed (`f` is null), print an error and stop with code 1.

```cpp
    std::fwrite(file.data(), 1, file.size(), f);
    std::fclose(f);
```

* Line 112: write `file.size()` items of 1 byte each, from the start of our vector.
* Line 113: close the file (important: this makes sure everything is written).

```cpp
    std::printf("Wrote images/ch06_stored.png: %zu bytes for %d pixels (%.2f bytes/pixel)\n",
                file.size(), width * height, (double)file.size() / (width * height));
    return 0;
}
```

* Lines 114–115: print the file size. `%zu` = a size number, `%d` = an int, `%.2f` = a fraction with 2 decimals.
  The result is about 180,278 bytes: **3.00 bytes per pixel**. No compression yet! Chapter 7 fixes that.
* Line 116: success.

---

## The whole program as a picture

```
make CRC table
pixels:  for each row: [0] + R G B R G B ...        → raw (180,200 bytes)
zlib:    78 01 | block(0, len, ~len, bytes) | block ... | block(1, ...) | adler32
file:    89 P N G \r \n 1A \n
         [len][IHDR][13 bytes][crc]
         [len][IDAT][zlib stream][crc]
         [0][IEND][crc]
save to images/ch06_stored.png
```

## Check your understanding

1. Why does every row start with a 0 byte? *(It's the filter type, required by PNG. 0 = no filter.)*
2. Which parts are big-endian and which are little-endian? *(PNG numbers and Adler-32: big; the stored block
   LEN/NLEN: little.)*
3. Why is the data split into several blocks? *(A stored block can hold at most 65,535 bytes.)*
4. What does the CRC cover? *(The chunk type and data, not the length.)*
