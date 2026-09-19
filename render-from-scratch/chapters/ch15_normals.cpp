// ch15_normals.cpp
// ------------------------------------------------------------
// Chapter 15: Surface normals and many objects.
// Now we use the library classes Sphere, HittableList, HitRecord.
// Each hit point is colored by its normal (x,y,z -> r,g,b).
//   images/ch15_normals.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) {
        // Normal components are in [-1, 1]; map them to [0, 1] colors.
        return 0.5 * (rec.normal + Color(1, 1, 1));
    }
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    // The world: a small sphere, and a huge sphere acting as the ground.
    // (No materials yet - we pass nullptr.)
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));

    const int W = 400, H = 225;
    const double vw = 2.0 * W / H;
    const Point3 eye(0, 0, 0);
    const Vec3 du(vw / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(vw / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            img.at(i, j) = ray_color(r, world);
        }
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch15_normals.png", img, raw);
    return 0;
}
