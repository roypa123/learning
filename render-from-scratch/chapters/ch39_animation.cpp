// ch39_animation.cpp
// ------------------------------------------------------------
// Chapter 39: Animation - many images in a row.
// A camera orbits a small scene while a ball bounces (with motion blur).
//   images/ch39_frames/frame_000.png ... frame_047.png   (every frame)
//   images/ch39_turntable.gif                            (our own GIF encoder!)
//
//   run ch39_animation          48 frames, 320x180
//   run ch39_animation final    96 frames, 640x360, more samples
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

// Height of the bouncing ball at time t (seconds): a bounce every second.
static double ball_height(double t) {
    double phase = t - std::floor(t);                // 0..1 within the current bounce
    return 0.35 + 1.6 * 4.0 * phase * (1.0 - phase);   // parabola: like real gravity
}

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    const int frames = final_quality ? 96 : 48;
    const int width = final_quality ? 640 : 320;
    const int spp = final_quality ? 64 : 16;
    const double fps = 24.0;
    const double shutter = 0.5;   // "180 degree shutter": open for half of each frame

    // ---------- static part of the scene -----------------------------------
    auto floor_tex = std::make_shared<CheckerTexture>(0.5, Color(0.8, 0.8, 0.8), Color(0.2, 0.2, 0.22));
    auto floor_mat = std::make_shared<Lambertian>(floor_tex);
    auto torus_mat = std::make_shared<RoughMetal>(Color(1.0, 0.77, 0.34), 0.2);
    auto glass = std::make_shared<Dielectric>(1.5);
    auto blue = std::make_shared<Plastic>(hex_color(0x1D4ED8), 0.25);
    auto ball_mat = std::make_shared<Plastic>(hex_color(0xDC2626), 0.3);
    auto panel = std::make_shared<DiffuseLight>(Color(5, 5, 5));

    Mesh torus_mesh = make_torus(0.8, 0.25, 48, 24);
    std::shared_ptr<Hittable> torus = torus_mesh.build(torus_mat);
    torus = std::make_shared<Translate>(torus, Vec3(0, 0.25, 0));

    auto light_quad = std::make_shared<Quad>(Point3(-2, 5, -2), Vec3(4, 0, 0), Vec3(0, 0, 4), panel);   // u x v points down

    GifWriter gif;
    int height = (int)(width / (16.0 / 9.0));
    gif.begin("images/ch39_turntable.gif", width, height, (int)std::round(100.0 / fps));

    for (int f = 0; f < frames; f++) {
        double t = f / fps;                       // time of this frame in seconds
        double angle = 2 * pi * f / frames;       // camera goes around once

        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60), floor_mat));
        world.add(torus);
        world.add(std::make_shared<Sphere>(Point3(0, 0.6, 0), 0.6, glass));
        world.add(std::make_shared<Sphere>(Point3(-1.8, 0.45, -1.2), 0.45, blue));
        world.add(light_quad);
        lights.add(light_quad);

        // The ball moves DURING the shutter: from its position at t to t + shutter/fps.
        Point3 p0(1.9, ball_height(t), 0.8);
        Point3 p1(1.9, ball_height(t + shutter / fps), 0.8);
        world.add(std::make_shared<Sphere>(p0, p1, 0.35, ball_mat));

        Camera cam;
        cam.image_width = width;
        cam.samples_per_pixel = spp;
        cam.max_depth = 12;
        cam.vfov = 35;
        cam.lookfrom = Point3(7 * std::sin(angle), 2.8, 7 * std::cos(angle));
        cam.lookat = Point3(0, 0.6, 0);
        cam.background = gradient_sky(Color(0.25, 0.25, 0.3), Color(0.5, 0.6, 0.8));
        cam.show_progress = false;
        cam.collect_aovs = true;
        cam.max_sample_value = 20;
        cam.seed = 1 + f;                         // different noise each frame (looks like film grain)

        Image img = cam.render(world, &lights);
        post::DenoiseSettings ds;
        ds.radius = 4;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
        img = post::tonemap(img, ToneMapper::Aces);

        char name[128];
        std::snprintf(name, sizeof(name), "images/ch39_frames/frame_%03d.png", f);
        write_png(name, img);
        gif.add_frame(img);
        std::printf("frame %d/%d done\n", f + 1, frames);
    }
    gif.end();
    std::printf("Saved images/ch39_turntable.gif\n");
    return 0;
}
