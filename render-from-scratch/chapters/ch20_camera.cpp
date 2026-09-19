// ch20_camera.cpp
// ------------------------------------------------------------
// Chapter 20: A positionable camera with depth of field.
//   images/ch20_fov_wide.png    - vfov 90 from the front
//   images/ch20_fov_tele.png    - vfov 20 from far away (telephoto)
//   images/ch20_defocus.png     - aperture open: blurry foreground/background
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 1.0);

    HittableList world;
    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.4, bubble));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));

    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    // Wide angle, from above-left.
    cam.vfov = 90;
    cam.lookfrom = Point3(-2, 2, 1);
    cam.lookat = Point3(0, 0, -1);
    cam.vup = Vec3(0, 1, 0);
    save_image("images/ch20_fov_wide.png", cam.render(world));

    // Telephoto: narrow field of view from further away.
    cam.vfov = 20;
    save_image("images/ch20_fov_tele.png", cam.render(world));

    // Depth of field: a lens with an aperture.
    cam.defocus_angle = 10.0;
    cam.focus_dist = 3.4;       // distance from lookfrom to the center sphere
    save_image("images/ch20_defocus.png", cam.render(world));
    return 0;
}
