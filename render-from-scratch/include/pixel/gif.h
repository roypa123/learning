// pixel/gif.h
// ------------------------------------------------------------
// Our own animated GIF encoder.
//   * fixed 252-color palette (6 red x 7 green x 6 blue levels)
//   * ordered (Bayer) dithering to hide the limited palette
//   * LZW compression (the algorithm GIF is built on)
// Explained in docs/39-animation.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "image.h"

namespace pixel {

class GifWriter {
public:
    // delay_cs = time per frame in 1/100 seconds (4 = 25 frames per second)
    bool begin(const std::string& filename, int width, int height, int delay_cs = 4, int loop_count = 0) {
        ensure_parent_folder(filename);
        f = std::fopen(filename.c_str(), "wb");
        if (!f) return false;
        w = width; h = height; delay = delay_cs;

        put_str("GIF89a");
        put_u16(w); put_u16(h);
        put_byte(0xF7);          // global color table, 8 bits color resolution, 256 entries
        put_byte(0);             // background color index
        put_byte(0);             // pixel aspect ratio (unused)
        // Global color table: our fixed palette.
        for (int i = 0; i < 256; i++) {
            int r = 0, g = 0, b = 0;
            if (i < 252) {
                r = (i / 42) * 255 / 5;
                g = ((i / 6) % 7) * 255 / 6;
                b = (i % 6) * 255 / 5;
            }
            put_byte(r); put_byte(g); put_byte(b);
        }
        // "NETSCAPE2.0" extension: makes the animation loop.
        put_byte(0x21); put_byte(0xFF); put_byte(0x0B);
        put_str("NETSCAPE2.0");
        put_byte(0x03); put_byte(0x01); put_u16(loop_count); put_byte(0x00);
        return true;
    }

    void add_frame(const Image& img, const SaveOptions& opt = SaveOptions()) {
        if (!f) return;
        std::vector<uint8_t> rgb = to_rgb8(img, opt);
        std::vector<uint8_t> indices((size_t)w * h);
        static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
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

        // Graphic Control Extension: frame delay.
        put_byte(0x21); put_byte(0xF9); put_byte(0x04);
        put_byte(0x04);          // disposal: leave frame in place
        put_u16(delay);
        put_byte(0);             // transparent color index (unused)
        put_byte(0);
        // Image descriptor.
        put_byte(0x2C);
        put_u16(0); put_u16(0); put_u16(w); put_u16(h);
        put_byte(0);             // no local color table, not interlaced
        lzw_encode(indices);
    }

    void end() {
        if (!f) return;
        put_byte(0x3B);          // trailer
        std::fclose(f);
        f = nullptr;
    }

    ~GifWriter() { end(); }

private:
    FILE* f = nullptr;
    int w = 0, h = 0, delay = 4;

    // Map 0..255 to 0..levels with a dither offset d (in units of one level).
    static int quantize(int value, int levels, double d) {
        double v = value / 255.0 * levels + d;
        int q = (int)(v + 0.5);
        return q < 0 ? 0 : (q > levels ? levels : q);
    }

    void put_byte(int b) { std::fputc(b & 0xFF, f); }
    void put_u16(int v) { put_byte(v & 0xFF); put_byte((v >> 8) & 0xFF); }
    void put_str(const char* s) { while (*s) put_byte(*s++); }

    // ---------------- LZW compression ------------------------------------
    // The dictionary starts with all 256 single colors. Whenever we see a
    // sequence we have not seen before, we give it a new code number.
    void lzw_encode(const std::vector<uint8_t>& data) {
        const int min_code_size = 8;
        const int clear_code = 1 << min_code_size;   // 256
        const int end_code = clear_code + 1;         // 257
        put_byte(min_code_size);

        std::vector<uint8_t> block;                  // bytes waiting for a sub-block
        uint32_t bit_buffer = 0;
        int bit_count = 0;
        int code_size = min_code_size + 1;

        auto flush_block = [&]() {
            if (block.empty()) return;
            put_byte((int)block.size());
            std::fwrite(block.data(), 1, block.size(), f);
            block.clear();
        };
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

        // dictionary[code * 256 + next_color] = code for (sequence + next_color), or -1
        std::vector<int16_t> dict(4096 * 256, -1);
        int next_code = end_code + 1;

        emit(clear_code);
        if (data.empty()) {
            emit(end_code);
        } else {
            int prefix = data[0];
            for (size_t i = 1; i < data.size(); i++) {
                int c = data[i];
                int key = prefix * 256 + c;
                if (dict[key] >= 0) {
                    prefix = dict[key];            // known sequence: keep growing it
                    continue;
                }
                emit(prefix);                      // output the longest known sequence
                int assigned = next_code++;
                dict[key] = (int16_t)assigned;     // remember the new, longer sequence
                if (assigned >= (1 << code_size)) code_size++;
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
        if (bit_count > 0) {
            block.push_back((uint8_t)(bit_buffer & 0xFF));
            if (block.size() == 255) flush_block();
        }
        flush_block();
        put_byte(0);   // block terminator
    }
};

} // namespace pixel
