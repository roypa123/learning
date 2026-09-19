// ch08_drawing.cpp
// ------------------------------------------------------------
// Chapter 8: Drawing basics - pixels, rectangles and lines.
//   images/ch08_pixels.png      - a zoomed pixel-art heart
//   images/ch08_lines.png       - Bresenham starburst + grid
//   images/ch08_mondrian.png    - a Mondrian-style painting from rectangles
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Pixel art: every pixel set by hand -----------------------
    {
        const char* heart[] = {
            "..XX...XX..",
            ".XXXX.XXXX.",
            "XXXXXXXXXXX",
            "XXXXXXXXXXX",
            ".XXXXXXXXX.",
            "..XXXXXXX..",
            "...XXXXX...",
            "....XXX....",
            ".....X.....",
        };
        const int rows = 9, cols = 11, zoom = 24;
        Image img(cols * zoom, rows * zoom, hex_color(0xFDF6E3));
        Canvas cv(img);
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (heart[r][c] == 'X')
                    cv.fill_rect(c * zoom, r * zoom, zoom, zoom, hex_color(0xE63946));
        // grid lines so you can see individual "pixels"
        for (int c = 0; c <= cols; c++) cv.draw_line(c * zoom, 0, c * zoom, rows * zoom - 1, hex_color(0xCCCCCC));
        for (int r = 0; r <= rows; r++) cv.draw_line(0, r * zoom, cols * zoom - 1, r * zoom, hex_color(0xCCCCCC));
        save_image("images/ch08_pixels.png", img);
    }

    // ---------- 2. Lines in every direction --------------------------------
    {
        Image img(400, 400, hex_color(0x0B132B));
        Canvas cv(img);
        for (int i = 0; i < 400; i += 20) {       // background grid
            cv.draw_line(i, 0, i, 399, hex_color(0x1C2541));
            cv.draw_line(0, i, 399, i, hex_color(0x1C2541));
        }
        const int N = 48;
        for (int k = 0; k < N; k++) {             // starburst
            double a = 2 * pi * k / N;
            int x1 = (int)(200 + 180 * std::cos(a));
            int y1 = (int)(200 + 180 * std::sin(a));
            Color c = srgb_to_linear(hsv(360.0 * k / N, 0.8, 1.0));
            cv.draw_line(200, 200, x1, y1, c);
        }
        cv.draw_rect(20, 20, 360, 360, hex_color(0xFFFFFF));
        save_image("images/ch08_lines.png", img);
    }

    // ---------- 3. A painting made only of rectangles ----------------------
    {
        Image img(420, 420, hex_color(0xF5F1E6));
        Canvas cv(img);
        cv.fill_rect(0, 0, 250, 250, hex_color(0xD62828));      // big red
        cv.fill_rect(330, 0, 90, 140, hex_color(0x003F88));     // blue
        cv.fill_rect(0, 330, 120, 90, hex_color(0xFFD60A));     // yellow
        cv.fill_rect(330, 360, 90, 60, hex_color(0x111111));    // black
        const Color black = hex_color(0x111111);
        const int t = 10;                                       // line thickness
        cv.fill_rect(250, 0, t, 420, black);
        cv.fill_rect(0, 250, 420, t, black);
        cv.fill_rect(320, 0, t, 420, black);
        cv.fill_rect(120, 250, t, 170, black);
        cv.fill_rect(320, 140, 100, t, black);
        cv.fill_rect(320, 350, 100, t, black);
        cv.fill_rect(0, 320, 120, t, black);
        save_image("images/ch08_mondrian.png", img);
    }
    return 0;
}
