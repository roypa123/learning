// ch32_microfacets.cpp
// ------------------------------------------------------------
// Chapter 32: Physically based materials with microfacets (GGX).
// A "material chart" like the ones used by film and game studios:
//   row 1: gold      RoughMetal, roughness 0.0 -> 1.0
//   row 2: copper    RoughMetal
//   row 3: red plastic (diffuse base + clear coat), roughness 0.0 -> 1.0
//   images/ch32_material_chart.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world, lights;

    // Studio floor.
    auto floor_mat = std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(0.5, Color(0.35, 0.35, 0.35), Color(0.25, 0.25, 0.25)));
    world.add(std::make_shared<Quad>(Point3(-50, -0.5, -50), Vec3(100, 0, 0), Vec3(0, 0, 100), floor_mat));

    // Measured "base reflectivity" (F0) of real metals, in linear RGB.
    const Color gold(1.000, 0.766, 0.336);
    const Color copper(0.955, 0.638, 0.538);
    const int N = 6;
    for (int i = 0; i < N; i++) {
        double roughness = (double)i / (N - 1);
        double x = (i - (N - 1) / 2.0) * 1.15;
        world.add(std::make_shared<Sphere>(Point3(x, 2.3, 0), 0.5, std::make_shared<RoughMetal>(gold, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 1.15, 0), 0.5, std::make_shared<RoughMetal>(copper, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 0.0, 0), 0.5, std::make_shared<Plastic>(Color(0.6, 0.05, 0.05), roughness)));
    }

    // Two big soft "softbox" lights, like a photo studio.
    auto softbox = std::make_shared<DiffuseLight>(Color(6, 6, 6));
    auto key = std::make_shared<Quad>(Point3(-6, 6, 4), Vec3(4, 0, 0), Vec3(0, -2, 2), softbox);   // faces down-back
    auto rim = std::make_shared<Quad>(Point3(4, 5, -6), Vec3(3, 0, 0), Vec3(0, 2, 2), softbox);
    world.add(key);
    world.add(rim);
    lights.add(key);
    lights.add(rim);

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 800;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 32;
    cam.lookfrom = Point3(0, 1.6, 10);
    cam.lookat = Point3(0, 1.1, 0);
    cam.background = gradient_sky(Color(0.05, 0.05, 0.06), Color(0.2, 0.22, 0.25));   // dim studio walls
    cam.max_sample_value = 30;   // tame rare fireflies from tiny highlights

    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = 1.4;
    save_image("images/ch32_material_chart.png", cam.render(world, &lights), opt);
    return 0;
}
