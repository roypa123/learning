// ch10_triangles.cpp
// ------------------------------------------------------------
// Chapter 10: Triangles, barycentric coordinates and a poster.
//   images/ch10_rgb_triangle.png   - the famous red/green/blue triangle
//   images/ch10_lowpoly.png        - a low-poly mountain sunset poster
// ------------------------------------------------------------
#include <cmath>
#include <vector>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Color interpolation inside one triangle -----------------
    {
        Image img(400, 360, hex_color(0x111111));
        Canvas cv(img);
        cv.fill_triangle(200, 20, 380, 330, 20, 330,
                         Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1));
        save_image("images/ch10_rgb_triangle.png", img);
    }

    // ---------- 2. Low-poly landscape --------------------------------------
    {
        const int W = 800, H = 450;
        Image img(W, H);
        Canvas cv(img);

        // Sky: vertical gradient, then a glowing sun.
        cv.vertical_gradient(0, H, hex_color(0x2B1055), hex_color(0xF7A072));
        cv.glow(560, 250, 260, hex_color(0xFF9E00) * 0.9);
        cv.fill_circle_aa(560, 250, 60, hex_color(0xFFE8A3));

        // Each mountain range is a jagged line of peaks, filled as triangles
        // down to the bottom of the image. Farther ranges are lighter (haze).
        struct Range { double base_y, height; uint32_t light, dark; uint64_t seed; };
        Range ranges[] = {
            {300, 140, 0x9D4EDD, 0x7B2CBF, 1},
            {340, 130, 0x5A189A, 0x3C096C, 2},
            {390, 120, 0x240046, 0x10002B, 3},
        };
        for (const Range& r : ranges) {
            Pcg32 rng(r.seed);
            std::vector<double> xs, ys;
            for (double x = -40; x <= W + 40; x += 40 + rng.next_double() * 60) {
                xs.push_back(x);
                ys.push_back(r.base_y - rng.next_double() * r.height);
            }
            Color light = hex_color(r.light), dark = hex_color(r.dark);
            for (size_t i = 0; i + 1 < xs.size(); i++) {
                double mx = 0.5 * (xs[i] + xs[i + 1]);
                // Two triangles per segment: a lit face and a shadow face.
                cv.fill_triangle(xs[i], ys[i], xs[i + 1], ys[i + 1], mx, H, light, dark, dark);
                cv.fill_triangle(xs[i], ys[i], mx, H, xs[i], H, dark, dark, dark);
                cv.fill_triangle(xs[i + 1], ys[i + 1], xs[i + 1], H, mx, H, light * 0.8, dark, dark);
            }
        }

        // A few birds: two short thick lines each.
        double birds[][2] = {{180, 120}, {215, 140}, {250, 110}};
        for (auto& b : birds) {
            cv.draw_line_aa(b[0] - 10, b[1] - 4, b[0], b[1], 2.0, hex_color(0x1A0033));
            cv.draw_line_aa(b[0], b[1], b[0] + 10, b[1] - 4, 2.0, hex_color(0x1A0033));
        }
        save_image("images/ch10_lowpoly.png", img);
    }
    return 0;
}
