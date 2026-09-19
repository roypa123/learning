// ch05_image_class.cpp
// ------------------------------------------------------------
// Chapter 5: The Image class.
//  1. Build a checkerboard + gradient image with Image::at()
//  2. Save it as PPM and BMP
//  3. Load the PPM back, invert it, flip it, and save again
//  4. Scale a tiny image up with nearest and bilinear sampling
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Create --------------------------------------------------
    Image img(320, 240);
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            bool dark_square = ((x / 40) + (y / 40)) % 2 == 0;
            double u = (double)x / (img.width - 1);
            Color base = lerp(hex_color(0x2E86DE), hex_color(0xF368E0), u);  // blue -> pink
            img.at(x, y) = dark_square ? base * 0.35 : base;
        }

    // ---------- 2. Save ----------------------------------------------------
    save_image("images/ch05_checker.ppm", img);
    save_image("images/ch05_checker.bmp", img);

    // ---------- 3. Load, change, save -------------------------------------
    Image loaded;
    if (!read_ppm("images/ch05_checker.ppm", loaded)) {
        std::printf("Could not read the PPM back!\n");
        return 1;
    }
    Image changed(loaded.width, loaded.height);
    for (int y = 0; y < loaded.height; y++)
        for (int x = 0; x < loaded.width; x++) {
            Color c = loaded.at(x, loaded.height - 1 - y);        // upside down
            Color display = linear_to_srgb(c);                    // invert what we SEE
            Color inverted = Color(1, 1, 1) - display;
            changed.at(x, y) = srgb_to_linear(inverted);
        }
    save_image("images/ch05_inverted.bmp", changed);

    // ---------- 4. Resampling: making a 4x4 image big ---------------------
    Image tiny(4, 4);
    Color palette[4] = { hex_color(0xFF6B6B), hex_color(0xFECA57), hex_color(0x48DBFB), hex_color(0x1DD1A1) };
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            tiny.at(x, y) = palette[(x + 2 * y) % 4];

    Image big_nearest(256, 256), big_bilinear(256, 256);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            double u = (x + 0.5) / 256.0;
            double v = 1.0 - (y + 0.5) / 256.0;   // v = 0 is the bottom
            big_nearest.at(x, y) = tiny.sample_nearest(u, v);
            big_bilinear.at(x, y) = tiny.sample_bilinear(u, v);
        }
    save_image("images/ch05_nearest.bmp", big_nearest);
    save_image("images/ch05_bilinear.bmp", big_bilinear);
    return 0;
}
