// ch38_final_shot.cpp
// ------------------------------------------------------------
// Chapter 38: The final shot - "The Monolith at Dawn".
// Everything from the book in one cinematic frame:
//   terrain mesh (144k triangles) + BVH, procedural textures, water with a
//   Fresnel coat, a glossy black monolith, glowing orbs, a human figure made
//   from SDFs, a low golden sun, atmospheric haze (volume), depth of field,
//   light sampling, denoising, bloom, ACES, color grade, vignette, grain.
//
//   run ch38_final_shot draft     ~1 minute   (480 px wide)
//   run ch38_final_shot           a few min.  (960 px wide)  <- default "preview"
//   run ch38_final_shot final     hours       (1920 px wide, 256 samples)
//
// Output: images/ch38_final_<preset>.png  (+ raw and denoise-comparison images)
// ------------------------------------------------------------
#include <cmath>
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

// ---------------- the landscape ------------------------------------------
const double WATER_LEVEL = -0.35;
const Point3 MONOLITH_POS(0, 0, -8);

double terrain_height(double x, double z) {
    // Rolling dunes: two layers of fractal noise.
    double dunes = 3.0 * (fbm_2d(x * 0.02 + 11.0, z * 0.02 + 3.0, 5) - 0.5) * 2.0
                 + 0.8 * (fbm_2d(x * 0.08 + 5.0, z * 0.08, 4) - 0.5);
    // Flat plateau around the monolith.
    double dx = x - MONOLITH_POS.x, dz = z - MONOLITH_POS.z;
    double d = std::sqrt(dx * dx + dz * dz);
    double h = dunes * smoothstep(5.0, 30.0, d);
    // A shallow lake basin between the camera and the monolith.
    h -= 1.4 * std::exp(-((x - 3) * (x - 3) / 60.0 + (z - 1) * (z - 1) / 20.0));
    // Distant mountains far behind.
    double far = smoothstep(70.0, 160.0, -z);
    h += far * 18.0 * fbm_2d(x * 0.012 + 40.0, z * 0.012, 6);
    return h;
}

struct Preset { std::string name; int width; int spp; bool denoise; };

int main(int argc, char** argv) {
    Preset preset = {"preview", 960, 32, true};
    if (argc > 1 && std::string(argv[1]) == "draft") preset = {"draft", 480, 16, true};
    if (argc > 1 && std::string(argv[1]) == "final") preset = {"final", 1920, 256, true};
    std::printf("Preset: %s (%d px wide, %d samples per pixel)\n", preset.name.c_str(), preset.width, preset.spp);

    HittableList world, lights;

    // ---------- terrain -----------------------------------------------------
    std::printf("Building terrain...\n");
    Mesh terrain = make_parametric_mesh(300, 240, [](double u, double v) {
        double x = -150.0 + 300.0 * u;
        double z = 40.0 - 240.0 * v;
        return Point3(x, terrain_height(x, z), z);
    });
    auto sand = std::make_shared<FunctionTexture>([](double, double, const Point3& p) {
        double n = fbm_2d(p.x * 0.3, p.z * 0.3, 4);                    // small color variation
        Color dry = lerp(hex_color(0xC9A27E), hex_color(0xB08463), n);
        Color wet = hex_color(0x6E5641);
        Color rock = hex_color(0x8C8177);
        Color c = lerp(wet, dry, smoothstep(WATER_LEVEL, WATER_LEVEL + 0.5, p.y));   // darker near water
        return lerp(c, rock, smoothstep(6.0, 14.0, p.y));                            // rocky mountains
    });
    world.add(terrain.build(std::make_shared<Lambertian>(sand)));
    std::printf("Terrain: %zu triangles\n", terrain.triangle_count());

    // ---------- water: a dark base under a very smooth clear coat -----------
    world.add(std::make_shared<Quad>(Point3(-150, WATER_LEVEL, 40), Vec3(300, 0, 0), Vec3(0, 0, -240),
                                     std::make_shared<Plastic>(hex_color(0x0B2027), 0.03, 1.33)));

    // ---------- the monolith (floating slightly) ---------------------------
    double ground = terrain_height(MONOLITH_POS.x, MONOLITH_POS.z);
    std::shared_ptr<Hittable> slab = make_box(Point3(-1.0, 0, -0.25), Point3(1.0, 4.5, 0.25),
                                              std::make_shared<Plastic>(hex_color(0x050506), 0.12));
    slab = std::make_shared<RotateY>(slab, 25);
    world.add(std::make_shared<Translate>(slab, Vec3(MONOLITH_POS.x, ground + 0.6, MONOLITH_POS.z)));

    // ---------- glowing orbs -----------------------------------------------
    auto orb_light = std::make_shared<DiffuseLight>(Color(40, 18, 6));
    Point3 orbs[3] = {Point3(-2.2, ground + 3.2, -7.0), Point3(2.0, ground + 2.4, -8.8), Point3(1.2, ground + 4.9, -6.6)};
    for (const Point3& o : orbs) {
        auto s = std::make_shared<Sphere>(o, 0.15, orb_light);
        world.add(s);
        lights.add(s);
    }

    // ---------- rocks --------------------------------------------------------
    auto rock_mat = std::make_shared<Lambertian>(hex_color(0x5B524A));
    HittableList rocks;
    Pcg32 rng(77);
    for (int i = 0; i < 80; i++) {
        double x = rng.next_double() * 40 - 20, z = rng.next_double() * 40 - 25;
        double r = 0.15 + 0.6 * std::pow(rng.next_double(), 2);
        double h = terrain_height(x, z);
        if (h < WATER_LEVEL - 0.1) continue;                  // under water
        if ((Point3(x, 0, z) - Point3(5.5, 0, 13.5)).length() < 4) continue;   // too close to camera
        rocks.add(std::make_shared<Sphere>(Point3(x, h + 0.2 * r, z), r, rock_mat));
    }
    world.add(std::make_shared<BVHNode>(rocks));

    // ---------- a person for scale, built from SDFs -------------------------
    // They stand on a flat rock in the shallow lake, looking at the monolith.
    const Point3 person_pos(3.8, WATER_LEVEL + 0.3, 0.5);
    std::shared_ptr<Hittable> pedestal = make_box(Point3(-0.6, -1.5, -0.5), Point3(0.6, 0, 0.5), rock_mat);
    pedestal = std::make_shared<RotateY>(pedestal, 10);
    world.add(std::make_shared<Translate>(pedestal, person_pos));
    auto person = [person_pos](const Point3& p) {
        Point3 q = p - person_pos;
        double body = sdf::capsule_y(q - Vec3(0, 0.25, 0), 1.05, 0.17);
        double head = sdf::sphere(q - Vec3(0, 1.63, 0), 0.12);
        double legs = sdf::op_smooth_union(sdf::capsule_y(q - Vec3(-0.09, 0, 0), 0.8, 0.08),
                                           sdf::capsule_y(q - Vec3(0.09, 0, 0), 0.8, 0.08), 0.05);
        return sdf::op_smooth_union(sdf::op_smooth_union(body, legs, 0.08), head, 0.06);
    };
    world.add(std::make_shared<SDFObject>(person, AABB(person_pos - Vec3(0.5, 0.1, 0.5), person_pos + Vec3(0.5, 1.9, 0.5)),
                                          std::make_shared<Lambertian>(hex_color(0x1F1B18))));

    // ---------- sun & sky -----------------------------------------------------
    Vec3 sun_dir = unit_vector(Vec3(0.18, 0.10, -1.0));   // low, in front of the camera
    auto sun = make_sun(sun_dir, 0.7, Color(9000, 5200, 2300));
    world.add(sun);
    lights.add(sun);

    SkySettings sky;
    sky.sun_direction = sun_dir;
    sky.zenith = Color(0.10, 0.17, 0.40);
    sky.horizon = Color(1.00, 0.62, 0.40);
    sky.ground = Color(0.12, 0.08, 0.06);
    sky.sun_glow = Color(1.0, 0.55, 0.25);
    sky.glow_strength = 1.6;
    sky.intensity = 0.9;

    // ---------- atmosphere: a huge sphere of thin, warm haze ----------------
    auto haze_bounds = std::make_shared<Sphere>(Point3(0, 0, -40), 170, nullptr);
    world.add(std::make_shared<ConstantMedium>(haze_bounds, 0.0035, Color(1.0, 0.92, 0.85)));

    // ---------- camera -------------------------------------------------------
    Camera cam;
    cam.aspect_ratio = 2.39;                 // cinemascope
    cam.image_width = preset.width;
    cam.samples_per_pixel = preset.spp;
    cam.max_depth = 12;
    cam.vfov = 24;
    cam.lookfrom = Point3(5.5, terrain_height(5.5, 13.5) + 1.7, 13.5);   // eye height above the sand
    cam.lookat = Point3(0.5, 2.6, -8.0);
    cam.defocus_angle = 0.25;
    cam.focus_dist = (cam.lookat - cam.lookfrom).length();
    cam.background = physical_sky(sky);
    cam.max_sample_value = 60;
    cam.collect_aovs = preset.denoise;

    Image hdr = cam.render(world, &lights);
    SaveOptions raw_aces;
    raw_aces.tonemap = ToneMapper::Aces;
    save_image("images/ch38_raw_" + preset.name + ".png", hdr, raw_aces);

    // ---------- post-production ---------------------------------------------
    Image img = hdr;
    if (preset.denoise) {
        post::DenoiseSettings ds;
        ds.radius = preset.spp >= 128 ? 3 : 6;
        ds.passes = preset.spp >= 128 ? 1 : 2;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
    }
    img = post::bloom(img, 2.0, 0.12, 6);
    img = post::exposure(img, -0.2);
    img = post::tonemap(img, ToneMapper::Aces);
    img = post::teal_orange(img, 0.25);
    img = post::chromatic_aberration(img, preset.width / 1000.0);
    img = post::vignette(img, 0.45, 0.65);
    img = post::film_grain(img, 0.03);

    SaveOptions plain;
    save_image("images/ch38_final_" + preset.name + ".png", img, plain);
    return 0;
}
