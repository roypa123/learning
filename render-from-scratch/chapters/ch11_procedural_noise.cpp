// ch11_procedural_noise.cpp
// ------------------------------------------------------------
// Chapter 11: Procedural noise - nature from math.
//   images/ch11_white_noise.png   - pure randomness (TV static)
//   images/ch11_value_noise.png   - smooth random hills
//   images/ch11_fbm.png           - fractal noise (octaves added)
//   images/ch11_landscape.png     - a whole landscape made from noise
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    const int S = 256;

    // ---------- 1. White noise: every pixel independent ---------------------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = hash2_01(x, y);
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_white_noise.png", img);
    }

    // ---------- 2. Value noise: random values on a grid, blended -----------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = value_noise_2d(x / 32.0, y / 32.0);   // grid cell = 32 pixels
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_value_noise.png", img);
    }

    // ---------- 3. fBm: 6 octaves ------------------------------------------
    {
        Image img(S, S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double v = fbm_2d(x / 64.0, y / 64.0, 6);
                img.at(x, y) = srgb_to_linear(Color(v, v, v));
            }
        save_image("images/ch11_fbm.png", img);
    }

    // ---------- 4. A landscape painted only with noise ---------------------
    {
        const int W = 960, H = 540;
        Image img(W, H);
        const double horizon = 330;

        // (a) Sky with noise clouds.
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                double t = y / horizon;
                Color sky = lerp(hex_color(0x0F2027), hex_color(0xF8B195), std::pow(clamp01(t), 1.6));
                double cloud = fbm_2d(x / 180.0, y / 60.0 + 3.7, 6);
                cloud = smoothstep(0.52, 0.75, cloud) * (1.0 - t * 0.6);
                img.at(x, y) = lerp(sky, hex_color(0xF67280) * 0.9 + Color(0.1, 0.05, 0.1), cloud * 0.8);
            }
        Canvas cv(img);
        cv.glow(W * 0.62, horizon - 30, 300, hex_color(0xFFB347) * 0.8);
        cv.fill_circle_aa(W * 0.62, horizon - 30, 34, hex_color(0xFFF1C1));

        // (b) Mountain layers: each column's height comes from 1D fBm.
        struct Layer { double base, amp, freq; uint32_t color; double seed; };
        Layer layers[] = {
            {horizon - 40, 110, 1 / 260.0, 0x6C5B7B, 10.0},
            {horizon - 10, 90, 1 / 200.0, 0x4B3F6B, 20.0},
            {horizon + 5, 60, 1 / 140.0, 0x2A2344, 30.0},
        };
        for (const Layer& L : layers) {
            for (int x = 0; x < W; x++) {
                double h = fbm_2d(x * L.freq, L.seed, 6);
                double top = L.base - (h - 0.3) * L.amp * 2.0;
                for (int y = (int)std::max(0.0, std::floor(top)); y <= horizon && y < H; y++) {
                    double coverage = clamp01(y + 1 - top);            // anti-aliased top edge
                    double haze = clamp01((horizon - y) / 200.0) * 0.3; // aerial perspective
                    Color c = lerp(hex_color(L.color), hex_color(0xF8B195), haze);
                    cv.blend_pixel(x, y, c, coverage);
                }
            }
        }

        // (c) Lake: mirror the image above the horizon, darken, and ripple it.
        for (int y = (int)horizon + 1; y < H; y++) {
            double depth = (y - horizon) / (H - horizon);
            for (int x = 0; x < W; x++) {
                double ripple = (value_noise_2d(x / 40.0, y / 3.0) - 0.5) * 12.0 * depth;
                int sx = (int)clampd(x + ripple, 0, W - 1);
                int sy = (int)clampd(2 * horizon - y, 0, horizon);
                Color reflected = img.at(sx, sy) * (0.6 - 0.3 * depth);
                img.at(x, y) = lerp(reflected, hex_color(0x0B0F1A), depth * 0.6);
            }
        }
        save_image("images/ch11_landscape.png", img);
    }
    return 0;
}
