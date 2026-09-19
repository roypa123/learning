// ch33_sky.cpp
// ------------------------------------------------------------
// Chapter 33: Skies and environment lighting.
// One scene lit by four different skies. The sky is not just a
// background: it is the main light of every outdoor shot.
//   images/ch33_noon.png
//   images/ch33_golden_hour.png
//   images/ch33_blue_hour.png
//   images/ch33_env_map.png    - lit by a 360-degree image we paint ourselves
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

static void build_scene(HittableList& world) {
    auto ground = std::make_shared<Lambertian>(hex_color(0x8A8A80));
    world.add(std::make_shared<Quad>(Point3(-100, 0, -100), Vec3(200, 0, 0), Vec3(0, 0, 200), ground));
    world.add(std::make_shared<Sphere>(Point3(-1.3, 0.7, 0), 0.7, std::make_shared<Lambertian>(Color(0.8, 0.8, 0.8))));
    world.add(std::make_shared<Sphere>(Point3(0.3, 0.7, -0.6), 0.7, std::make_shared<RoughMetal>(Color(0.95, 0.93, 0.88), 0.15)));
    world.add(std::make_shared<Sphere>(Point3(1.6, 0.5, 0.6), 0.5, std::make_shared<Dielectric>(1.5)));
    std::shared_ptr<Hittable> box = make_box(Point3(-0.4, 0, -0.4), Point3(0.4, 1.6, 0.4),
                                             std::make_shared<Plastic>(hex_color(0x2563EB), 0.3));
    box = std::make_shared<RotateY>(box, 30);
    world.add(std::make_shared<Translate>(box, Vec3(-0.2, 0, -2.2)));
}

static Camera make_camera() {
    Camera cam;
    cam.image_width = 640;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 35;
    cam.lookfrom = Point3(0, 1.6, 7);
    cam.lookat = Point3(0, 0.6, 0);
    cam.max_sample_value = 50;
    return cam;
}

// Render the scene lit by a sky + a sun (the sun is a real, samplable light).
static void render_with(const char* file, SkySettings sky, double sun_elevation_deg, double sun_azimuth_deg,
                        Color sun_radiance, double exposure) {
    HittableList world, lights;
    build_scene(world);
    double el = degrees_to_radians(sun_elevation_deg), az = degrees_to_radians(sun_azimuth_deg);
    Vec3 sun_dir(std::cos(el) * std::sin(az), std::sin(el), -std::cos(el) * std::cos(az));
    sky.sun_direction = sun_dir;
    if (sun_radiance.max_component() > 0) {     // a sun below the horizon adds nothing
        auto sun = make_sun(sun_dir, 0.8, sun_radiance);
        world.add(sun);
        lights.add(sun);
    }

    Camera cam = make_camera();
    cam.background = physical_sky(sky);
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = exposure;
    save_image(file, cam.render(world, &lights), opt);
}

int main() {
    // ---------- Noon: high white sun, deep blue sky -------------------------
    {
        SkySettings s;
        s.zenith = Color(0.15, 0.35, 0.85);
        s.horizon = Color(0.65, 0.78, 0.95);
        s.sun_glow = Color(1.0, 0.95, 0.85);
        s.glow_strength = 0.3;
        s.intensity = 1.0;
        render_with("images/ch33_noon.png", s, 60, 40, Color(8000, 7700, 7200), 0.6);
    }
    // ---------- Golden hour: low orange sun, long shadows -------------------
    {
        SkySettings s;
        s.zenith = Color(0.12, 0.22, 0.55);
        s.horizon = Color(1.0, 0.62, 0.35);
        s.ground = Color(0.2, 0.12, 0.08);
        s.sun_glow = Color(1.0, 0.45, 0.15);
        s.glow_strength = 1.5;
        s.intensity = 0.8;
        render_with("images/ch33_golden_hour.png", s, 6, 60, Color(6000, 3000, 1100), 0.8);
    }
    // ---------- Blue hour: sun just below the horizon ----------------------
    {
        SkySettings s;
        s.zenith = Color(0.03, 0.05, 0.18);
        s.horizon = Color(0.25, 0.28, 0.55);
        s.ground = Color(0.02, 0.02, 0.04);
        s.sun_glow = Color(0.8, 0.35, 0.3);
        s.glow_strength = 0.8;
        s.intensity = 0.6;
        render_with("images/ch33_blue_hour.png", s, -4, 70, Color(0, 0, 0), 2.5);
    }
    // ---------- Environment map: a painted 360 degree "studio" -------------
    {
        auto env = std::make_shared<Image>(512, 256);
        for (int y = 0; y < env->height; y++)
            for (int x = 0; x < env->width; x++) {
                double u = (x + 0.5) / env->width, v = (y + 0.5) / env->height;   // v: 0 = top
                Color c = lerp(Color(0.6, 0.6, 0.65), Color(0.15, 0.13, 0.12), v);   // bright top, dark floor
                // Three bright rectangular "windows"/softboxes around the horizon.
                for (int k = 0; k < 3; k++) {
                    double cu = k / 3.0 + 0.1;
                    if (std::fabs(u - cu) < 0.05 && v > 0.25 && v < 0.45) c = Color(8, 7.5, 7);
                }
                env->at(x, y) = c;
            }
        save_image("images/ch33_env_map_texture.png", *env, SaveOptions{0.3, ToneMapper::Aces, true});

        HittableList world;
        build_scene(world);
        Camera cam = make_camera();
        cam.background = environment_map(env, 1.0, 0.0);
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        save_image("images/ch33_env_map.png", cam.render(world), opt);
    }
    return 0;
}
