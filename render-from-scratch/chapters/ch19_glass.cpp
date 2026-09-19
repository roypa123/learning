// ch19_glass.cpp
// ------------------------------------------------------------
// Chapter 19: Dielectrics - glass, water and bubbles.
//   images/ch19_glass.png        - solid glass ball (left), matte (middle), gold (right)
//   images/ch19_hollow.png       - hollow glass ball: a bubble inside glass
//   images/ch19_ior.png          - index of refraction 1.0, 1.33, 1.5, 2.4
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

static Camera base_camera() {
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    return cam;
}

int main() {
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);   // air inside glass
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 0.0);

    // ---------- 1. Solid glass ball --------------------------------------
    {
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
        world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
        world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
        world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));
        save_image("images/ch19_glass.png", base_camera().render(world));

        // ---------- 2. Add a bubble inside the glass: hollow sphere -------
        world.add(std::make_shared<Sphere>(Point3(-1.0, 0.0, -1.0), 0.4, bubble));
        save_image("images/ch19_hollow.png", base_camera().render(world));
    }

    // ---------- 3. Different materials, same shape ------------------------
    {
        auto checker = std::make_shared<CheckerTexture>(0.25, Color(0.9, 0.9, 0.9), Color(0.15, 0.15, 0.15));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, std::make_shared<Lambertian>(checker)));
        double iors[4] = {1.0, 1.33, 1.5, 2.4};   // air, water, glass, diamond
        for (int k = 0; k < 4; k++)
            world.add(std::make_shared<Sphere>(Point3(-1.5 + k, 0, -1.6), 0.45,
                                               std::make_shared<Dielectric>(iors[k])));
        Camera cam = base_camera();
        cam.lookfrom = Point3(0, 0.3, 1);
        cam.lookat = Point3(0, 0, -1.6);
        cam.vfov = 60;
        save_image("images/ch19_ior.png", cam.render(world));
    }
    return 0;
}
