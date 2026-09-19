// ch09_circles_antialiasing.cpp
// ------------------------------------------------------------
// Chapter 9: Circles, anti-aliasing and transparency.
//   images/ch09_jaggies.png     - hard circle vs anti-aliased circle, zoomed 8x
//   images/ch09_bubbles.png     - transparent overlapping bubbles
//   images/ch09_night_sky.png   - glows: a moon and stars
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Enlarge an image by an integer factor, keeping pixels square (to SEE them).
static Image zoom(const Image& src, int factor) {
    Image out(src.width * factor, src.height * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x / factor, y / factor);
    return out;
}

int main() {
    // ---------- 1. Jaggies vs smooth edges ---------------------------------
    {
        Image hard(40, 40, Color(1, 1, 1)), smooth(40, 40, Color(1, 1, 1));
        Canvas a(hard), b(smooth);
        a.fill_circle(20, 20, 14, hex_color(0x1D3557));
        b.fill_circle_aa(20.0, 20.0, 14.3, hex_color(0x1D3557));
        a.draw_line(2, 36, 37, 30, hex_color(0xE63946));
        b.draw_line_aa(2.5, 36.5, 37.5, 30.5, 1.0, hex_color(0xE63946));
        Image both = post::side_by_side(zoom(hard, 8), zoom(smooth, 8), 16);
        save_image("images/ch09_jaggies.png", both);
    }

    // ---------- 2. Transparency (alpha blending) ---------------------------
    {
        Image img(480, 320);
        Canvas cv(img);
        cv.vertical_gradient(0, 320, hex_color(0x48CAE4), hex_color(0x023E8A));
        struct Bubble { double x, y, r; uint32_t color; };
        Bubble bubbles[] = { {150, 150, 90, 0xFF006E}, {260, 130, 80, 0xFFBE0B},
                             {220, 220, 85, 0x8338EC}, {360, 200, 60, 0x3A86FF},
                             {90, 260, 40, 0xFB5607} };
        for (const Bubble& b : bubbles) {
            cv.fill_circle_aa(b.x, b.y, b.r, hex_color(b.color), 0.55);          // see-through body
            cv.fill_circle_aa(b.x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.18,   // highlight
                              Color(1, 1, 1), 0.7);
        }
        save_image("images/ch09_bubbles.png", img);
    }

    // ---------- 3. Glow: adding light instead of painting over -------------
    {
        Image img(640, 360);
        Canvas cv(img);
        cv.vertical_gradient(0, 360, hex_color(0x03045E), hex_color(0x000814));
        Pcg32 rng(7);
        for (int i = 0; i < 350; i++) {                      // stars
            double x = rng.next_double() * 640, y = rng.next_double() * 300;
            double brightness = 0.2 + 2.0 * std::pow(rng.next_double(), 6);
            cv.glow(x, y, 2.0 + 4.0 * brightness, Color(0.8, 0.85, 1.0) * brightness);
        }
        cv.glow(480, 90, 160, hex_color(0x4A6FA5) * 0.6);      // halo around the moon
        cv.fill_circle_aa(480, 90, 38, hex_color(0xF1F1E6));   // the moon
        cv.fill_circle_aa(492, 80, 8, hex_color(0xD9D9C8));    // craters
        cv.fill_circle_aa(470, 104, 11, hex_color(0xD9D9C8));
        // dark hills in front (circles cut by the bottom edge)
        cv.fill_circle_aa(150, 560, 280, hex_color(0x020617));
        cv.fill_circle_aa(520, 600, 320, hex_color(0x030712));
        save_image("images/ch09_night_sky.png", img);
    }
    return 0;
}
