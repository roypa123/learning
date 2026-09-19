// ch04_color.cpp
// ------------------------------------------------------------
// Chapter 4: Color for programmers.
// Produces three images:
//   images/ch04_gamma_ramps.bmp  - linear vs gamma-correct grey ramps
//   images/ch04_hsv_wheel.bmp    - a color wheel built from HSV
//   images/ch04_rgb_mixing.bmp   - red, green, blue lights adding up
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Gamma ramps --------------------------------------------
    // Top band: bytes go 0..255 evenly ("what the file says").
    // Middle band: LIGHT goes 0..1 evenly, saved with the sRGB curve.
    // Bottom band: 16 steps so you can compare the brightness jumps.
    {
        const int W = 768, H = 240;
        Image img(W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                double t = (double)x / (W - 1);
                Color c;
                if (y < 80) {
                    // We want the FILE to contain t*255, so undo the sRGB curve first.
                    c = srgb_to_linear(Color(t, t, t));
                } else if (y < 160) {
                    c = Color(t, t, t);                      // physically even light
                } else {
                    double step = std::floor(t * 16) / 15.0;  // 16 flat steps of light
                    c = Color(step, step, step);
                }
                img.at(x, y) = c;
            }
        save_image("images/ch04_gamma_ramps.bmp", img);
    }

    // ---------- 2. HSV color wheel ---------------------------------------
    {
        const int S = 400;
        Image img(S, S, Color(1, 1, 1));
        double cx = S / 2.0, cy = S / 2.0, R = S / 2.0 - 10;
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > R) continue;
                double hue = std::atan2(-dy, dx) * 180.0 / pi;   // angle = hue
                double sat = dist / R;                           // distance = saturation
                Color display = hsv(hue, sat, 1.0);              // 0..1 display values
                img.at(x, y) = srgb_to_linear(display);          // store as linear light
            }
        save_image("images/ch04_hsv_wheel.bmp", img);
    }

    // ---------- 3. Additive mixing: three colored spotlights -------------
    {
        const int W = 500, H = 460;
        Image img(W, H, Color(0, 0, 0));
        struct Spot { double x, y; Color c; };
        Spot spots[3] = { {250, 170, Color(1, 0, 0)},
                          {180, 290, Color(0, 1, 0)},
                          {320, 290, Color(0, 0, 1)} };
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                for (const Spot& s : spots) {
                    double dx = x + 0.5 - s.x, dy = y + 0.5 - s.y;
                    if (dx * dx + dy * dy < 130 * 130) img.at(x, y) += s.c;   // light ADDS
                }
        save_image("images/ch04_rgb_mixing.bmp", img);
    }
    return 0;
}
