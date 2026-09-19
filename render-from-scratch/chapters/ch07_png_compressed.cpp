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
