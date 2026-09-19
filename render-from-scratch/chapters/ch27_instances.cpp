// ch27_instances.cpp
// ------------------------------------------------------------
// Chapter 27: Instances - moving and rotating objects.
// The classic Cornell box with two rotated boxes, plus a "forest"
// of 1 mesh used 60 times.
//   images/ch27_cornell_boxes.png
//   images/ch27_instanced_forest.png
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Build the five walls + light of the Cornell box. Reused in later chapters.
static void add_cornell_room(HittableList& world, std::shared_ptr<Material> light) {
    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
    world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(555, 555, 555), Vec3(-555, 0, 0), Vec3(0, 0, -555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));
}

int main() {
    // ---------- 1. Cornell box with rotated boxes --------------------------
    {
        HittableList world;
        add_cornell_room(world, std::make_shared<DiffuseLight>(Color(15, 15, 15)));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));

        // Build each box at the origin, rotate it, THEN move it into place.
        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        world.add(box1);

        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch27_cornell_boxes.png", cam.render(world));
    }

    // ---------- 2. One tree, many instances --------------------------------
    {
        // A "tree": a cone of triangles (leaves) on a box trunk.
        Mesh cone = make_parametric_mesh(16, 1, [](double u, double v) {
            double a = u * 2 * pi;
            double r = 0.6 * (1 - v);
            return Point3(r * std::cos(a), 0.4 + 1.4 * v, r * std::sin(a));
        });
        auto leaves = std::make_shared<Lambertian>(hex_color(0x2D6A4F));
        auto bark = std::make_shared<Lambertian>(hex_color(0x6B4226));
        auto tree = std::make_shared<HittableList>();
        tree->add(cone.build(leaves));
        tree->add(make_box(Point3(-0.1, 0, -0.1), Point3(0.1, 0.45, 0.1), bark));
        std::shared_ptr<Hittable> tree_bvh = std::make_shared<BVHNode>(*tree);

        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(hex_color(0x95D5B2))));
        Pcg32 rng(8);
        for (int i = 0; i < 60; i++) {
            double x = rng.next_double() * 16 - 8, z = rng.next_double() * 16 - 8;
            if (x * x + z * z < 2) continue;   // leave a clearing
            std::shared_ptr<Hittable> t = std::make_shared<RotateY>(tree_bvh, rng.next_double() * 360);
            world.add(std::make_shared<Translate>(t, Vec3(x, 0, z)));   // the SAME tree, placed again
        }
        world.add(std::make_shared<Sphere>(Point3(0, 0.5, 0), 0.5, std::make_shared<Metal>(Color(0.9, 0.9, 0.9), 0.0)));
        BVHNode bvh(world);

        Camera cam;
        cam.image_width = 600;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 35;
        cam.lookfrom = Point3(10, 6, 10);
        cam.lookat = Point3(0, 0.5, 0);
        save_image("images/ch27_instanced_forest.png", cam.render(bvh));
    }
    return 0;
}
