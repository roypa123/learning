// ch36_denoise.cpp
// ------------------------------------------------------------
// Chapter 36: Denoising.
// Render with FEW samples, then clean the noise with an edge-aware
// filter guided by extra images (albedo & normals).
//   images/ch36_noisy.png        16 samples per pixel
//   images/ch36_albedo.png       surface color at the first hit (AOV)
//   images/ch36_normals.png      surface normals at the first hit (AOV)
//   images/ch36_blur.png         a plain Gaussian blur (for comparison: BAD)
//   images/ch36_denoised.png     our joint bilateral denoiser
//   images/ch36_reference.png    512 samples per pixel (the "truth")
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world, lights;
    auto checker = std::make_shared<CheckerTexture>(0.6, Color(0.7, 0.7, 0.7), Color(0.15, 0.15, 0.15));
    world.add(std::make_shared<Quad>(Point3(-20, 0, -20), Vec3(40, 0, 0), Vec3(0, 0, 40), std::make_shared<Lambertian>(checker)));
    world.add(std::make_shared<Sphere>(Point3(-1.2, 0.7, 0), 0.7, std::make_shared<Lambertian>(hex_color(0x2F6DB5))));
    world.add(std::make_shared<Sphere>(Point3(0.4, 0.5, 0.8), 0.5, std::make_shared<Plastic>(hex_color(0xD97706), 0.3)));
    world.add(std::make_shared<Sphere>(Point3(1.5, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.9, 0.9, 0.9), 0.25)));
    auto lamp = std::make_shared<Quad>(Point3(-2, 4, 2), Vec3(0, 0, -2), Vec3(2, 0, 0), std::make_shared<DiffuseLight>(Color(10, 9, 8)));
    world.add(lamp);
    lights.add(lamp);

    Camera cam;
    cam.image_width = 480;
    cam.max_depth = 20;
    cam.vfov = 40;
    cam.lookfrom = Point3(0, 2.2, 6);
    cam.lookat = Point3(0, 0.6, 0);
    cam.background = gradient_sky(Color(0.15, 0.15, 0.18), Color(0.25, 0.3, 0.4));
    cam.max_sample_value = 20;
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;

    // Noisy render + guide images.
    cam.samples_per_pixel = 16;
    cam.collect_aovs = true;
    Image noisy = cam.render(world, &lights);
    save_image("images/ch36_noisy.png", noisy, opt);
    save_image("images/ch36_albedo.png", cam.albedo_aov);
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch36_normals.png", post::visualize_normals(cam.normal_aov), raw);

    // A plain blur: removes noise AND all detail.
    save_image("images/ch36_blur.png", post::gaussian_blur(noisy, 4), opt);

    // Our denoiser.
    post::DenoiseSettings ds;
    ds.radius = 7;
    ds.passes = 2;
    Image clean = post::denoise(noisy, cam.albedo_aov, cam.normal_aov, ds);
    save_image("images/ch36_denoised.png", clean, opt);

    // The reference: many more samples (this takes a while).
    cam.samples_per_pixel = 512;
    cam.collect_aovs = false;
    save_image("images/ch36_reference.png", cam.render(world, &lights), opt);
    return 0;
}
