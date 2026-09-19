// ch12_vectors.cpp
// ------------------------------------------------------------
// Chapter 12: Vectors - the language of 3D.
// Part 1 prints vector math results so you can check them by hand.
// Part 2 uses vectors to shade a "fake 3D" ball in a 2D image:
//   images/ch12_fake_sphere.png
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Vector arithmetic you can check with pencil & paper -----
    Vec3 a(1, 2, 3);
    Vec3 b(4, 5, 6);
    std::cout << "a           = " << a << "\n";
    std::cout << "b           = " << b << "\n";
    std::cout << "a + b       = " << a + b << "   (expected (5, 7, 9))\n";
    std::cout << "b - a       = " << b - a << "   (expected (3, 3, 3))\n";
    std::cout << "2 * a       = " << 2.0 * a << "   (expected (2, 4, 6))\n";
    std::cout << "dot(a, b)   = " << dot(a, b) << "   (expected 32)\n";
    std::cout << "cross(a, b) = " << cross(a, b) << "   (expected (-3, 6, -3))\n";
    std::cout << "|a|         = " << a.length() << "   (expected 3.74166)\n";
    std::cout << "unit(a)     = " << unit_vector(a) << "   (length 1)\n";
    Vec3 x(1, 0, 0), y(0, 1, 0);
    std::cout << "cross(x, y) = " << cross(x, y) << "   (expected z = (0, 0, 1))\n";
    Vec3 down_right(1, -1, 0);
    std::cout << "reflect((1,-1,0), up) = " << reflect(down_right, y) << "   (expected (1, 1, 0))\n";

    // ---------- 2. Shading a ball with the dot product ---------------------
    const int S = 400;
    Image img(S, S);
    Vec3 light_dir = unit_vector(Vec3(-1, 1, 1));     // light comes from upper-left-front
    Vec3 view_dir(0, 0, 1);                           // we look along -z, so "towards us" is +z
    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {
            // Map the pixel to [-1, 1] x [-1, 1], y pointing up.
            double sx = (px + 0.5) / S * 2 - 1;
            double sy = 1 - (py + 0.5) / S * 2;
            double r2 = sx * sx + sy * sy;
            if (r2 > 0.8 * 0.8) {                         // background
                img.at(px, py) = lerp(hex_color(0x1E293B), hex_color(0x0F172A), (py + 0.5) / S);
                continue;
            }
            // On a sphere of radius R, the surface point above (sx, sy) has z = sqrt(R^2 - x^2 - y^2).
            double sz = std::sqrt(0.8 * 0.8 - r2);
            Vec3 normal = unit_vector(Vec3(sx, sy, sz));

            double diffuse = std::fmax(0.0, dot(normal, light_dir));                 // Lambert
            Vec3 reflected = reflect(-light_dir, normal);
            double specular = std::pow(std::fmax(0.0, dot(reflected, view_dir)), 40); // Phong
            Color base = hex_color(0xE11D48);
            Color c = base * (0.08 + 0.92 * diffuse) + Color(1, 1, 1) * 0.6 * specular;
            img.at(px, py) = c;
        }
    }
    save_image("images/ch12_fake_sphere.png", img);
    return 0;
}
