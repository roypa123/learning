// ch21_first_masterpiece.cpp
// ------------------------------------------------------------
// Chapter 21: Multithreading and the final "one weekend" scene:
// hundreds of random small spheres + three big ones.
//   images/ch21_random_spheres.png
// Run with "final" for a bigger, cleaner image:   run ch21_first_masterpiece final
// ------------------------------------------------------------
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";

    HittableList world;
    auto ground_material = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, ground_material));

    Pcg32 rng(2024);   // fixed seed: the same scene every time
    auto rnd = [&]() { return rng.next_double(); };

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            double choose_mat = rnd();
            Point3 center(a + 0.9 * rnd(), 0.2, b + 0.9 * rnd());
            if ((center - Point3(4, 0.2, 0)).length() <= 0.9) continue;

            std::shared_ptr<Material> mat;
            if (choose_mat < 0.8) {            // 80% matte
                Color albedo(rnd() * rnd(), rnd() * rnd(), rnd() * rnd());
                mat = std::make_shared<Lambertian>(albedo);
            } else if (choose_mat < 0.95) {    // 15% metal
                Color albedo(0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd());
                mat = std::make_shared<Metal>(albedo, 0.5 * rnd());
            } else {                           // 5% glass
                mat = std::make_shared<Dielectric>(1.5);
            }
            world.add(std::make_shared<Sphere>(center, 0.2, mat));
        }
    }
    world.add(std::make_shared<Sphere>(Point3(0, 1, 0), 1.0, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(-4, 1, 0), 1.0, std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1))));
    world.add(std::make_shared<Sphere>(Point3(4, 1, 0), 1.0, std::make_shared<Metal>(Color(0.7, 0.6, 0.5), 0.0)));

    // A BVH makes this MUCH faster (explained in chapter 23).
    BVHNode bvh(world);

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = final_quality ? 1200 : 600;
    cam.samples_per_pixel = final_quality ? 300 : 50;
    cam.max_depth = 50;
    cam.vfov = 20;
    cam.lookfrom = Point3(13, 2, 3);
    cam.lookat = Point3(0, 0, 0);
    cam.vup = Vec3(0, 1, 0);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    save_image("images/ch21_random_spheres.png", cam.render(bvh));
    return 0;
}
