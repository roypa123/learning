// ch17_diffuse.cpp
// ------------------------------------------------------------
// Chapter 17: Diffuse (matte) materials.
// From now on we use the library Camera, which traces bounces for us.
//   images/ch17_diffuse.png          - correct result (sRGB gamma)
//   images/ch17_no_gamma.png         - same numbers saved without gamma: too dark
//   images/ch17_shadow_acne.png      - what happens with t_min = 0
//   images/ch17_bounces.png          - max_depth 1, 2, 3, 50 side by side
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    auto grey = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, grey));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, grey));

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = gradient_sky();

    Image img = cam.render(world);
    save_image("images/ch17_diffuse.png", img);

    SaveOptions no_gamma;
    no_gamma.srgb = false;
    save_image("images/ch17_no_gamma.png", img, no_gamma);

    // Shadow acne: rays re-hit the surface they just left because of rounding errors.
    Camera acne = cam;
    acne.t_min = 0.0;
    acne.russian_roulette = false;
    save_image("images/ch17_shadow_acne.png", acne.render(world));

    // How many bounces do we need?
    Camera small = cam;
    small.image_width = 200;
    small.show_progress = false;
    Image strip;
    int depths[4] = {1, 2, 3, 50};
    for (int k = 0; k < 4; k++) {
        small.max_depth = depths[k];
        Image part = small.render(world);
        strip = (k == 0) ? part : post::side_by_side(strip, part, 4);
    }
    save_image("images/ch17_bounces.png", strip);
    return 0;
}
