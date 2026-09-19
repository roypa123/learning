// ch22_motion_blur.cpp
// ------------------------------------------------------------
// Chapter 22: Motion blur.
// Each ray gets a random time in [0,1). Moving spheres are at a
// different place for each time, so they smear like in a photo.
//   images/ch22_motion_blur.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world;
    auto checker = std::make_shared<CheckerTexture>(0.5, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(checker)));

    Pcg32 rng(11);
    for (int a = -6; a < 6; a++) {
        for (int b = -6; b < 6; b++) {
            Point3 center(a + 0.9 * rng.next_double(), 0.2, b + 0.9 * rng.next_double());
            Color albedo(rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double());
            auto mat = std::make_shared<Lambertian>(albedo);
            // Bounce upwards by a random amount during the shutter time.
            Point3 center2 = center + Vec3(0, 0.5 * rng.next_double(), 0);
            world.add(std::make_shared<Sphere>(center, center2, 0.2, mat));
        }
    }
    // A big sphere flying sideways fast.
    world.add(std::make_shared<Sphere>(Point3(-0.8, 1, 0), Point3(0.8, 1, 0), 1.0,
                                       std::make_shared<Lambertian>(Color(0.8, 0.2, 0.1))));

    BVHNode bvh(world);
    Camera cam;
    cam.image_width = 600;
    cam.samples_per_pixel = 100;
    cam.max_depth = 20;
    cam.vfov = 25;
    cam.lookfrom = Point3(13, 3, 3);
    cam.lookat = Point3(0, 0.5, 0);
    save_image("images/ch22_motion_blur.png", cam.render(bvh));
    return 0;
}
