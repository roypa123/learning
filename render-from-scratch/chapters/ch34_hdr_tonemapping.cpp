// ch34_hdr_tonemapping.cpp
// ------------------------------------------------------------
// Chapter 34: HDR, exposure and tone mapping.
// We render ONE high-dynamic-range image (a dusk scene with a very
// bright lamp) and develop it in different ways, like a photographer.
//   images/ch34_clamp.png, ch34_reinhard.png, ch34_aces.png, ch34_hable.png
//   images/ch34_exposure_bracket.png  - exposure -2 .. +2 stops (ACES)
//   images/ch34_false_color.png       - brightness as a heat map (log scale)
//   images/ch34_graded.png            - ACES + color grading
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Blue (dark) -> green -> yellow -> red (bright), on a log scale.
static Color heat(double lum) {
    double t = clamp01((std::log10(std::max(lum, 1e-4)) + 3.0) / 5.0);   // 0.001 .. 100
    Color stops[5] = {Color(0, 0, 0.5), Color(0, 0.6, 1), Color(0, 1, 0), Color(1, 1, 0), Color(1, 0, 0)};
    double f = t * 4;
    int i = std::min(3, (int)f);
    return lerp(stops[i], stops[i + 1], f - i);
}

int main() {
    HittableList world, lights;
    world.add(std::make_shared<Quad>(Point3(-50, 0, -50), Vec3(100, 0, 0), Vec3(0, 0, 100),
                                     std::make_shared<Lambertian>(hex_color(0x6B6B6B))));
    world.add(std::make_shared<Sphere>(Point3(-1.4, 0.6, 0.3), 0.6, std::make_shared<Lambertian>(hex_color(0xD9D9D9))));
    world.add(std::make_shared<Sphere>(Point3(1.3, 0.6, 0.2), 0.6, std::make_shared<Plastic>(hex_color(0xB91C1C), 0.2)));
    world.add(std::make_shared<Sphere>(Point3(0.0, 0.45, 1.3), 0.45, std::make_shared<Dielectric>(1.5)));

    // A street lamp: a thin pole and a VERY bright bulb (radiance 400!).
    auto pole = make_box(Point3(-0.05, 0, -0.05), Point3(0.05, 2.6, 0.05), std::make_shared<RoughMetal>(Color(0.3, 0.3, 0.3), 0.4));
    world.add(std::make_shared<Translate>(pole, Vec3(0, 0, -1.2)));
    auto bulb = std::make_shared<Sphere>(Point3(0, 2.75, -1.2), 0.15, std::make_shared<DiffuseLight>(Color(400, 300, 180)));
    world.add(bulb);
    lights.add(bulb);

    Camera cam;
    cam.image_width = 480;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 40;
    cam.lookfrom = Point3(0, 1.5, 6.5);
    cam.lookat = Point3(0, 1.0, 0);
    SkySettings dusk;
    dusk.zenith = Color(0.01, 0.02, 0.06);
    dusk.horizon = Color(0.10, 0.07, 0.12);
    dusk.ground = Color(0.01, 0.01, 0.01);
    dusk.glow_strength = 0.0;
    cam.background = physical_sky(dusk);

    Image hdr = cam.render(world, &lights);   // values from ~0.001 up to 400!

    SaveOptions o;
    o.tonemap = ToneMapper::Clamp;    save_image("images/ch34_clamp.png", hdr, o);
    o.tonemap = ToneMapper::Reinhard; save_image("images/ch34_reinhard.png", hdr, o);
    o.tonemap = ToneMapper::Aces;     save_image("images/ch34_aces.png", hdr, o);
    o.tonemap = ToneMapper::Hable;    save_image("images/ch34_hable.png", hdr, o);

    // Exposure bracket: -2, -1, 0, +1, +2 stops.
    Image small = post::resize(hdr, 240, 135);
    Image strip;
    for (int stop = -2; stop <= 2; stop++) {
        Image developed = post::tonemap(post::exposure(small, stop), ToneMapper::Aces);
        strip = stop == -2 ? developed : post::side_by_side(strip, developed, 4);
    }
    SaveOptions plain;   // already tone mapped: just apply sRGB
    save_image("images/ch34_exposure_bracket.png", strip, plain);

    // False color: see the real range of light values.
    Image fc(hdr.width, hdr.height);
    for (size_t i = 0; i < hdr.data.size(); i++) fc.data[i] = heat(luminance(hdr.data[i]));
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch34_false_color.png", fc, raw);

    // A graded version: warm, slightly more contrast and saturation.
    Image graded = post::contrast(hdr, 1.1);
    graded = post::temperature(graded, 1.0);
    graded = post::saturation(graded, 1.15);
    graded = post::tonemap(post::exposure(graded, 0.3), ToneMapper::Aces);
    for (auto& c : graded.data)
        c = post::lift_gamma_gain(c, Color(0.02, 0.01, 0.04), Color(1.0, 1.0, 1.05), Color(1.0, 0.97, 0.92));
    save_image("images/ch34_graded.png", graded, plain);
    return 0;
}
