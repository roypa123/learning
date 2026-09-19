// ch37_sdf.cpp
// ------------------------------------------------------------
// Chapter 37: Signed distance functions and ray marching.
// Shapes that are hard to build from triangles:
//   * three spheres melted together (smooth union)
//   * a rounded cube with a spherical bite taken out (subtraction)
//   * a field of pillars repeated with one line of code (repetition)
//   * the Mandelbulb fractal
//   images/ch37_sdf_scene.png
//   images/ch37_mandelbulb.png
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. A gallery of SDF shapes ----------------------------------
    {
        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-40, 0, -40), Vec3(80, 0, 0), Vec3(0, 0, 80),
                                         std::make_shared<Lambertian>(hex_color(0xB8B2A7))));

        // (a) Blob: three spheres blended with a smooth union.
        auto blob = [](const Point3& p) {
            Point3 q = p - Point3(-2.2, 0.9, 0);
            double d1 = sdf::sphere(q - Vec3(0, 0, 0), 0.7);
            double d2 = sdf::sphere(q - Vec3(0.6, 0.5, 0.2), 0.5);
            double d3 = sdf::sphere(q - Vec3(-0.4, 0.6, -0.3), 0.45);
            return sdf::op_smooth_union(sdf::op_smooth_union(d1, d2, 0.35), d3, 0.35);
        };
        world.add(std::make_shared<SDFObject>(blob, AABB(Point3(-3.4, 0, -1.2), Point3(-1.0, 2.2, 1.2)),
                                              std::make_shared<Plastic>(hex_color(0xE11D48), 0.2)));

        // (b) Rounded cube minus a sphere.
        auto bitten = [](const Point3& p) {
            Point3 q = sdf::rotate_y(p - Point3(0, 0.8, 0), 0.6);
            double cube = sdf::round_box(q, Vec3(0.7, 0.7, 0.7), 0.12);
            double bite = sdf::sphere(q - Vec3(0.55, 0.55, 0.55), 0.6);
            return sdf::op_subtract(cube, bite);
        };
        world.add(std::make_shared<SDFObject>(bitten, AABB(Point3(-1.2, 0, -1.2), Point3(1.2, 1.7, 1.2)),
                                              std::make_shared<RoughMetal>(Color(0.95, 0.64, 0.54), 0.25)));

        // (c) Torus.
        auto ring = [](const Point3& p) {
            Point3 q = p - Point3(2.3, 0.75, 0);
            Point3 tilted(q.x, q.y * std::cos(1.2) - q.z * std::sin(1.2), q.y * std::sin(1.2) + q.z * std::cos(1.2));
            return sdf::torus(tilted, 0.55, 0.18);
        };
        world.add(std::make_shared<SDFObject>(ring, AABB(Point3(1.5, 0, -0.9), Point3(3.1, 1.5, 0.9)),
                                              std::make_shared<Plastic>(hex_color(0x0EA5E9), 0.1)));

        // (d) Endless pillars behind: ONE capsule, repeated every 1.5 units.
        auto pillars = [](const Point3& p) {
            Point3 q = sdf::op_repeat_xz(p, 1.5);
            return sdf::capsule_y(q, 3.0, 0.2);
        };
        world.add(std::make_shared<SDFObject>(pillars, AABB(Point3(-20, 0, -20), Point3(20, 3.3, -3.0)),
                                              std::make_shared<Lambertian>(hex_color(0xE7E5E4))));

        Vec3 sun_dir = unit_vector(Vec3(-0.6, 0.7, 0.5));
        auto sun = make_sun(sun_dir, 1.0, Color(3000, 2800, 2500));
        world.add(sun);
        lights.add(sun);
        SkySettings sky;
        sky.sun_direction = sun_dir;

        Camera cam;
        cam.image_width = 800;
        cam.samples_per_pixel = 64;
        cam.max_depth = 12;
        cam.vfov = 36;
        cam.lookfrom = Point3(0, 2.4, 8);
        cam.lookat = Point3(0, 0.9, 0);
        cam.background = physical_sky(sky);
        cam.max_sample_value = 50;
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        opt.exposure = 0.7;
        save_image("images/ch37_sdf_scene.png", cam.render(world, &lights), opt);
    }

    // ---------- 2. The Mandelbulb -------------------------------------------
    {
        HittableList world, lights;
        auto bulb = [](const Point3& p) { return sdf::mandelbulb(p, 12, 8.0); };
        world.add(std::make_shared<SDFObject>(bulb, AABB(Point3(-1.25, -1.25, -1.25), Point3(1.25, 1.25, 1.25)),
                                              std::make_shared<Plastic>(hex_color(0xF59E0B), 0.35),
                                              400, 2e-4, 0.9));
        auto key = std::make_shared<Sphere>(Point3(4, 5, 4), 1.2, std::make_shared<DiffuseLight>(Color(18, 16, 14)));
        world.add(key);
        lights.add(key);

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 500;
        cam.samples_per_pixel = 64;
        cam.max_depth = 8;
        cam.vfov = 30;
        cam.lookfrom = Point3(2.2, 1.6, 3.0);
        cam.lookat = Point3(0, 0, 0);
        cam.background = gradient_sky(Color(0.05, 0.03, 0.08), Color(0.12, 0.15, 0.3));
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        save_image("images/ch37_mandelbulb.png", cam.render(world, &lights), opt);
    }
    return 0;
}
