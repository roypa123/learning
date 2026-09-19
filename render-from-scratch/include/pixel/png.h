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
