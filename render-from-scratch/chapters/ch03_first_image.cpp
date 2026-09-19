// ch03_first_image.cpp
// ------------------------------------------------------------
// Chapter 3: Your very first image. NO library, nothing hidden.
// We compute a color for every pixel and write it into two files:
//   images/ch03_gradient.ppm  (the simplest format in the world)
//   images/ch03_gradient.bmp  (opens in Windows Photos / Paint)
// Build & run:   run ch03_first_image
// ------------------------------------------------------------
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>

int main() {
    const int width = 256;
    const int height = 256;

    // Make the "images" folder if it does not exist yet.
    std::filesystem::create_directories("images");

    // ---- 1. Compute every pixel and keep it in memory -------------------
    // We store 3 bytes per pixel: red, green, blue (0..255 each).
    std::vector<uint8_t> pixels(width * height * 3);

    for (int y = 0; y < height; y++) {          // rows, top to bottom
        for (int x = 0; x < width; x++) {       // columns, left to right
            double r = double(x) / (width - 1);   // 0.0 at left  -> 1.0 at right
            double g = double(y) / (height - 1);  // 0.0 at top   -> 1.0 at bottom
            double b = 0.25;                      // a little blue everywhere

            int index = (y * width + x) * 3;      // where this pixel lives in the array
            pixels[index + 0] = (uint8_t)(255.999 * r);
            pixels[index + 1] = (uint8_t)(255.999 * g);
            pixels[index + 2] = (uint8_t)(255.999 * b);
        }
    }

    // ---- 2. Write a PPM file (plain text version "P3") -------------------
    {
        std::ofstream ppm("images/ch03_gradient.ppm");
        ppm << "P3\n" << width << ' ' << height << "\n255\n";
        for (int i = 0; i < width * height; i++) {
            ppm << (int)pixels[i * 3 + 0] << ' '
                << (int)pixels[i * 3 + 1] << ' '
                << (int)pixels[i * 3 + 2] << '\n';
        }
        std::printf("Wrote images/ch03_gradient.ppm\n");
    }

    // ---- 3. Write a BMP file ------------------------------------------
    // BMP = 54 byte header + pixels stored bottom row first, as B,G,R,
    // with every row padded to a multiple of 4 bytes.
    {
        const int row_size = (width * 3 + 3) / 4 * 4;
        const uint32_t data_size = row_size * height;
        const uint32_t file_size = 54 + data_size;

        std::vector<uint8_t> header(54, 0);
        auto put32 = [&](int offset, uint32_t v) {    // little-endian: low byte first
            header[offset + 0] = v & 0xFF;
            header[offset + 1] = (v >> 8) & 0xFF;
            header[offset + 2] = (v >> 16) & 0xFF;
            header[offset + 3] = (v >> 24) & 0xFF;
        };
        header[0] = 'B'; header[1] = 'M';
        put32(2, file_size);
        put32(10, 54);          // pixel data starts after the 54 header bytes
        put32(14, 40);          // size of the "info" part of the header
        put32(18, width);
        put32(22, height);
        header[26] = 1;         // one color plane
        header[28] = 24;        // 24 bits per pixel
        put32(34, data_size);

        std::ofstream bmp("images/ch03_gradient.bmp", std::ios::binary);
        bmp.write((const char*)header.data(), header.size());
        std::vector<uint8_t> row(row_size, 0);
        for (int y = height - 1; y >= 0; y--) {       // bottom row first!
            for (int x = 0; x < width; x++) {
                int i = (y * width + x) * 3;
                row[x * 3 + 0] = pixels[i + 2];   // blue
                row[x * 3 + 1] = pixels[i + 1];   // green
                row[x * 3 + 2] = pixels[i + 0];   // red
            }
            bmp.write((const char*)row.data(), row_size);
        }
        std::printf("Wrote images/ch03_gradient.bmp  (double-click it!)\n");
    }
    return 0;
}
