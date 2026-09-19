// ch26_cornell_box.cpp
// ------------------------------------------------------------
// Chapter 26: Lights and the Cornell box.
//   images/ch26_light_quad.png    - a glowing rectangle lights a marble scene
//   images/ch26_cornell_empty.png - the classic empty Cornell box
// Note: without light sampling (chapter 31) these images are NOISY.
// That is expected!
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. A simple area light --------------------------------------
    {
        auto marble = std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble);
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(marble)));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 0), 2, std::make_shared<Lambertian>(marble)));

        auto difflight = std::make_shared<DiffuseLight>(Color(4, 4, 4));
        world.add(std::make_shared<Sphere>(Point3(0, 7, 0), 2, difflight));
        world.add(std::make_shared<Quad>(Point3(3, 1, -2), Vec3(2, 0, 0), Vec3(0, 2, 0), difflight));

        Camera cam;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));   // no sky: darkness
        cam.vfov = 20;
        cam.lookfrom = Point3(26, 3, 6);
        cam.lookat = Point3(0, 2, 0);
        save_image("images/ch26_light_quad.png", cam.render(world));
    }

    // ---------- 2. Cornell box ----------------------------------------------
    {
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

        HittableList world;
        world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
        // The light's normal must point DOWN into the room: u x v = (-,0,0)x(0,0,-) points down.
        world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(555, 555, 555), Vec3(-555, 0, 0), Vec3(0, 0, -555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch26_cornell_empty.png", cam.render(world));
    }
    return 0;
}
