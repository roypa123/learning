// ch18_metal.cpp
// ------------------------------------------------------------
// Chapter 18: Metal - reflection and fuzz.
//   images/ch18_metal_mirror.png   - perfect mirrors (fuzz = 0)
//   images/ch18_metal_fuzzy.png    - brushed metal (fuzz 0.3 and 1.0)
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

static Image render_scene(double fuzz_left, double fuzz_right) {
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto left   = std::make_shared<Metal>(Color(0.8, 0.8, 0.8), fuzz_left);
    auto right  = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), fuzz_right);

    HittableList world;
    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, left));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, right));

    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    return cam.render(world);
}

int main() {
    save_image("images/ch18_metal_mirror.png", render_scene(0.0, 0.0));
    save_image("images/ch18_metal_fuzzy.png", render_scene(0.3, 1.0));
    return 0;
}
