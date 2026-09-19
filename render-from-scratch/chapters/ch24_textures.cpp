// ch24_textures.cpp
// ------------------------------------------------------------
// Chapter 24: Textures.
//   images/ch24_checker.png     - 3D checker texture on two big spheres
//   images/ch24_planet.png      - an image texture (a planet map we paint ourselves)
//   images/ch24_perlin.png      - Perlin noise: smooth, turbulent, marble
//   images/ch24_planet_map.png  - the flat 2:1 "world map" we generated
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

static Camera make_camera(Point3 from, Point3 at, double vfov) {
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 64;
    cam.max_depth = 20;
    cam.vfov = vfov;
    cam.lookfrom = from;
    cam.lookat = at;
    return cam;
}

// Paint a fantasy planet map procedurally: oceans, land, ice caps.
static Image paint_planet_map(int W, int H) {
    Image map(W, H);
    Perlin noise(99);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            // Turn map coordinates into a point on a sphere so the noise wraps seamlessly.
            double lon = (double)x / W * 2 * pi;
            double lat = ((double)y / H - 0.5) * pi;
            Point3 p(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
            double h = noise.fbm(p * 2.2, 6);                  // -1 .. 1 "height"
            Color c;
            if (h < 0.0)       c = lerp(hex_color(0x0B3D91), hex_color(0x1E6FD9), clamp01(1 + h * 3));   // ocean
            else if (h < 0.05) c = hex_color(0xD8C690);                                                  // beach
            else if (h < 0.3)  c = lerp(hex_color(0x3B7D2A), hex_color(0x2A5A1E), h / 0.3);             // forest
            else               c = lerp(hex_color(0x7A6A58), hex_color(0xEEEEEE), clamp01((h - 0.3) * 3)); // mountains
            double polar = std::fabs(std::sin(lat));
            if (polar + 0.1 * h > 0.85) c = hex_color(0xF4F8FF);                                         // ice caps
            map.at(x, y) = c;
        }
    return map;
}

int main() {
    // ---------- 1. Checkered spheres -------------------------------------
    {
        auto checker = std::make_shared<CheckerTexture>(0.32, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -10, 0), 10, std::make_shared<Lambertian>(checker)));
        world.add(std::make_shared<Sphere>(Point3(0, 10, 0), 10, std::make_shared<Lambertian>(checker)));
        save_image("images/ch24_checker.png", make_camera(Point3(13, 2, 3), Point3(0, 0, 0), 20).render(world));
    }

    // ---------- 2. Image texture -----------------------------------------
    {
        Image map = paint_planet_map(1024, 512);
        save_image("images/ch24_planet_map.png", map);
        save_image("images/ch24_planet_map.ppm", map);          // also as PPM...
        auto tex = std::make_shared<ImageTexture>("images/ch24_planet_map.ppm");   // ...and load it back

        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, 0, 0), 2, std::make_shared<Lambertian>(tex)));
        Camera cam = make_camera(Point3(0, 1.5, 12), Point3(0, 0, 0), 20);
        cam.background = solid_background(Color(0.01, 0.01, 0.02));
        // The sun: a very bright sphere to the side. (Lights are explained in chapter 26.)
        world.add(std::make_shared<Sphere>(Point3(-30, 10, 20), 8, std::make_shared<DiffuseLight>(Color(12, 11, 10))));
        cam.samples_per_pixel = 200;
        save_image("images/ch24_planet.png", cam.render(world));
    }

    // ---------- 3. Perlin textures ---------------------------------------
    {
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, -2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(2.0, NoiseTexture::Smooth))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(3.0, NoiseTexture::Turbulence))));
        save_image("images/ch24_perlin.png", make_camera(Point3(13, 3, 3), Point3(0, 1.5, 0), 30).render(world));
    }
    return 0;
}
