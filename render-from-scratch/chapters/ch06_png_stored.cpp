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
