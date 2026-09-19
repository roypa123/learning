// ch35_post_effects.cpp
// ------------------------------------------------------------
// Chapter 35: The film look - post-processing.
// A neon-lit night scene, developed step by step:
//   images/ch35_step1_raw.png        ACES tone mapping only
//   images/ch35_step2_bloom.png      + bloom (glow around bright lights)
//   images/ch35_step3_grade.png      + teal & orange color grade
//   images/ch35_step4_final.png      + vignette, chromatic aberration, grain, letterbox
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    HittableList world, lights;

    // Wet street: almost black base with a clear glossy coat -> reflections.
    world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60),
                                     std::make_shared<Plastic>(hex_color(0x0A0A0C), 0.08)));
    // Back wall.
    world.add(std::make_shared<Quad>(Point3(-30, 0, -4), Vec3(60, 0, 0), Vec3(0, 20, 0),
                                     std::make_shared<Lambertian>(hex_color(0x2A2320))));

    // Neon ring: 40 small glowing spheres.
    auto pink = std::make_shared<DiffuseLight>(Color(12, 1.5, 6));
    for (int i = 0; i < 40; i++) {
        double a = 2 * pi * i / 40;
        auto s = std::make_shared<Sphere>(Point3(1.8 * std::cos(a), 2.4 + 1.8 * std::sin(a), -3.6), 0.09, pink);
        world.add(s);
        lights.add(s);
    }
    // Two neon bars (glowing boxes).
    auto cyan = std::make_shared<DiffuseLight>(Color(1, 8, 12));
    auto bar1 = make_box(Point3(-3.6, 0.3, -3.7), Point3(-3.45, 3.8, -3.55), cyan);
    auto bar2 = make_box(Point3(3.45, 0.3, -3.7), Point3(3.6, 3.8, -3.55), cyan);
    world.add(bar1); world.add(bar2);
    lights.add(bar1); lights.add(bar2);

    // Hero objects.
    world.add(std::make_shared<Sphere>(Point3(-1.1, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.95, 0.95, 0.95), 0.05)));
    world.add(std::make_shared<Sphere>(Point3(1.2, 0.6, -0.2), 0.6, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(0.2, 0.3, 1.0), 0.3, std::make_shared<Plastic>(hex_color(0xF5C518), 0.25)));

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = final_quality ? 1280 : 640;
    cam.samples_per_pixel = final_quality ? 512 : 128;
    cam.max_depth = 20;
    cam.vfov = 38;
    cam.lookfrom = Point3(0, 1.3, 7.5);
    cam.lookat = Point3(0, 1.4, -1);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 8.0;
    cam.background = solid_background(Color(0.002, 0.003, 0.008));
    cam.max_sample_value = 40;

    Image hdr = cam.render(world, &lights);
    SaveOptions plain;   // images below are already tone mapped

    // Step 1: just tone mapping.
    Image step1 = post::tonemap(hdr, ToneMapper::Aces);
    save_image("images/ch35_step1_raw.png", step1, plain);

    // Step 2: bloom happens in HDR, BEFORE tone mapping.
    Image bloomed = post::bloom(hdr, 1.0, 0.25, 6);
    Image step2 = post::tonemap(bloomed, ToneMapper::Aces);
    save_image("images/ch35_step2_bloom.png", step2, plain);

    // Step 3: color grade.
    Image step3 = post::teal_orange(step2, 0.35);
    step3 = post::saturation(step3, 1.1);
    save_image("images/ch35_step3_grade.png", step3, plain);

    // Step 4: lens and film effects.
    Image step4 = post::chromatic_aberration(step3, 1.5);
    step4 = post::vignette(step4, 0.55, 0.7);
    step4 = post::film_grain(step4, 0.04);
    step4 = post::letterbox(step4, 2.39);
    save_image("images/ch35_step4_final.png", step4, plain);
    return 0;
}
