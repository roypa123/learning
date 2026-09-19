// ch28_volumes.cpp
// ------------------------------------------------------------
// Chapter 28: Volumes - smoke, fog and clouds.
//   images/ch28_cornell_smoke.png  - the two boxes turned into smoke and fog
//   images/ch28_cloud.png          - a noise-shaped cloud (variable density)
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Constant density smoke in the Cornell box ---------------
    {
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));

        HittableList world;
        world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
        world.add(std::make_shared<Quad>(Point3(113, 554, 127), Vec3(330, 0, 0), Vec3(0, 0, 305), light));
        world.add(std::make_shared<Quad>(Point3(0, 555, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));

        world.add(std::make_shared<ConstantMedium>(box1, 0.01, Color(0, 0, 0)));   // black smoke
        world.add(std::make_shared<ConstantMedium>(box2, 0.01, Color(1, 1, 1)));   // white fog

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch28_cornell_smoke.png", cam.render(world));
    }

    // ---------- 2. A cloud with density from noise -------------------------
    {
        auto noise = std::make_shared<Perlin>(17);
        // Density: a squashed ball, eroded by noise at the edges.
        auto density = [noise](const Point3& p) {
            Vec3 q(p.x / 2.2, p.y / 1.1, p.z / 1.6);
            double shape = 1.0 - q.length();                         // 1 at center, 0 at the edge
            double n = noise->fbm(p * 1.3, 5);                       // -1 .. 1
            return std::fmax(0.0, shape + 0.6 * n) * 4.0;            // clamp to >= 0
        };
        auto bounds = make_box(Point3(-2.6, -1.4, -2.0), Point3(2.6, 1.4, 2.0), nullptr);
        HittableList world;
        world.add(std::make_shared<VariableMedium>(bounds, 7.0, density, Color(0.95, 0.95, 0.95)));
        world.add(std::make_shared<Sphere>(Point3(0, -1003, 0), 1000, std::make_shared<Lambertian>(hex_color(0x5B7F5A))));

        Camera cam;
        cam.image_width = 480;
        cam.samples_per_pixel = 128;
        cam.max_depth = 30;
        cam.vfov = 40;
        cam.lookfrom = Point3(0, 1, 9);
        cam.lookat = Point3(0, 0.3, 0);
        SkySettings sky;
        sky.sun_direction = unit_vector(Vec3(0.5, 0.6, 0.3));
        cam.background = physical_sky(sky);
        save_image("images/ch28_cloud.png", cam.render(world));
    }
    return 0;
}
