// ch29_monte_carlo.cpp
// ------------------------------------------------------------
// Chapter 29: Monte Carlo - answering questions with random numbers.
//  1. Estimate pi by throwing darts at a square     (console + image)
//  2. Watch the error shrink as N grows             (console)
//  3. Stratified (jittered) darts converge faster   (console)
//  4. Estimate an integral:  area under x^2 on [0,2] = 8/3
//   images/ch29_darts.png
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1 + 2. Plain Monte Carlo estimate of pi --------------------
    std::printf("Estimating pi with random darts in the square [-1,1]^2:\n");
    std::printf("%12s %14s %12s\n", "darts", "estimate", "error");
    long long inside = 0, total = 0;
    for (long long n = 10; n <= 10000000; n *= 10) {
        while (total < n) {
            double x = random_double(-1, 1), y = random_double(-1, 1);
            if (x * x + y * y < 1) inside++;
            total++;
        }
        double estimate = 4.0 * inside / total;
        std::printf("%12lld %14.8f %12.8f\n", total, estimate, std::fabs(estimate - pi));
    }

    // ---------- 3. Stratified: one dart per grid cell ----------------------
    std::printf("\nStratified (one jittered dart per cell of a grid):\n");
    for (int grid = 10; grid <= 3000; grid *= 3) {
        long long in = 0;
        for (int i = 0; i < grid; i++)
            for (int j = 0; j < grid; j++) {
                double x = 2 * ((i + random_double()) / grid) - 1;
                double y = 2 * ((j + random_double()) / grid) - 1;
                if (x * x + y * y < 1) in++;
            }
        double estimate = 4.0 * in / ((double)grid * grid);
        std::printf("%12lld %14.8f %12.8f\n", (long long)grid * grid, estimate, std::fabs(estimate - pi));
    }

    // ---------- 4. An integral ---------------------------------------------
    // Integral of x^2 from 0 to 2. Average of f(x) at random x, times the width (2).
    std::printf("\nIntegral of x^2 on [0,2] (exact = 2.66666667):\n");
    for (int n = 10; n <= 1000000; n *= 10) {
        double sum = 0;
        for (int i = 0; i < n; i++) {
            double x = random_double(0, 2);
            sum += x * x;
        }
        std::printf("%12d %14.8f\n", n, 2.0 * sum / n);
    }

    // ---------- Picture of the darts ---------------------------------------
    const int S = 500;
    Image img(S, S, hex_color(0x0F172A));
    Canvas cv(img);
    cv.draw_circle(S / 2, S / 2, S / 2 - 20, hex_color(0x94A3B8));
    cv.draw_rect(20, 20, S - 40, S - 40, hex_color(0x94A3B8));
    Pcg32 rng(3);
    for (int i = 0; i < 2000; i++) {
        double x = rng.next_double() * 2 - 1, y = rng.next_double() * 2 - 1;
        bool in = x * x + y * y < 1;
        double px = 20 + (x + 1) * 0.5 * (S - 40), py = 20 + (y + 1) * 0.5 * (S - 40);
        cv.fill_circle_aa(px, py, 2.2, in ? hex_color(0xF43F5E) : hex_color(0x38BDF8));
    }
    save_image("images/ch29_darts.png", img);
    return 0;
}
