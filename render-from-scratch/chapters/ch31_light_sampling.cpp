// ch31_light_sampling.cpp
// ------------------------------------------------------------
// Chapter 31: Light sampling with mixture PDFs.
// Same Cornell box, same number of samples (64):
//   left  = only BSDF sampling (rays bounce randomly, hoping to find the light)
//   right = half the rays aimed AT the light, half by the material
//   images/ch31_compare.png
//   images/ch31_cornell_final.png   - the finished box with a glass ball (more samples)
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

static void build_cornell(HittableList& world, HittableList& lights, bool glass_ball) {
    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

    world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
    world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
    world.add(std::make_shared<Quad>(Point3(0, 555, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));

    auto aluminum = std::make_shared<Metal>(Color(0.8, 0.85, 0.88), 0.0);
    std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), glass_ball ? aluminum : white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
    world.add(box1);

    if (glass_ball) {
        world.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, std::make_shared<Dielectric>(1.5)));
    } else {
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);
    }

    // The list of things worth aiming at. Materials don't matter here: only the shape.
    lights.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), nullptr));
    if (glass_ball)   // glass focuses light (caustics): aiming at it helps too
        lights.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, nullptr));
}

static Camera cornell_camera(int spp) {
    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = spp;
    cam.max_depth = 50;
    cam.background = solid_background(Color(0, 0, 0));
    cam.vfov = 40;
    cam.lookfrom = Point3(278, 278, -800);
    cam.lookat = Point3(278, 278, 0);
    return cam;
}

int main() {
    {
        HittableList world, lights;
        build_cornell(world, lights, false);
        Camera cam = cornell_camera(64);
        Image without = cam.render(world, nullptr);
        Image with = cam.render(world, &lights);
        save_image("images/ch31_compare.png", post::side_by_side(without, with, 8));
    }
    {
        HittableList world, lights;
        build_cornell(world, lights, true);
        Camera cam = cornell_camera(400);
        save_image("images/ch31_cornell_final.png", cam.render(world, &lights));
    }
    return 0;
}
