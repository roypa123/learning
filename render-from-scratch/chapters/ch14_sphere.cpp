// ch14_sphere.cpp
// ------------------------------------------------------------
// Chapter 14: Hitting a sphere.
// We solve a quadratic equation to find if a ray touches a sphere.
//   images/ch14_red_sphere.png     - yes/no hit test: a flat red disc
//   images/ch14_depth.png          - how FAR away each hit is (t value)
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Returns the smallest positive t where the ray hits the sphere, or -1 if it misses.
double hit_sphere(const Point3& center, double radius, const Ray& r) {
    Vec3 oc = center - r.origin();
    double a = dot(r.direction(), r.direction());
    double b = -2.0 * dot(r.direction(), oc);
    double c = dot(oc, oc) - radius * radius;
    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return -1.0;               // no real solution: miss
    return (-b - std::sqrt(discriminant)) / (2.0 * a);  // the nearer of the two solutions
}

Color sky(const Ray& r) {
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    const int W = 400, H = 225;
    const Point3 eye(0, 0, 0);
    const Vec3 du(3.5555 / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(3.5555 / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image flat(W, H), depth(W, H);
    for (int j = 0; j < H; j++) {
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            double t = hit_sphere(Point3(0, 0, -1), 0.5, r);

            // Image 1: red where we hit, sky elsewhere.
            flat.at(i, j) = t > 0 ? Color(1, 0, 0) : sky(r);

            // Image 2: brightness = closeness. Nearest point ~0.5 away, edges ~1.0.
            if (t > 0) {
                double closeness = clamp01((1.1 - t * r.direction().length()) / 0.6);
                depth.at(i, j) = Color(closeness, closeness, closeness);
            } else {
                depth.at(i, j) = Color(0.05, 0.05, 0.1);
            }
        }
    }
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch14_red_sphere.png", flat, raw);
    save_image("images/ch14_depth.png", depth, raw);
    return 0;
}
