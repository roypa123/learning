// ch16_antialiasing.cpp
// ------------------------------------------------------------
// Chapter 16: Random numbers and anti-aliasing.
// Same scene as chapter 15, rendered with 1 and with 64 samples per pixel.
// We zoom into the edge of the sphere to see the difference.
//   images/ch16_aa_compare.png   left: 1 sample, right: 64 samples (zoomed 6x)
//   images/ch16_random_pixels.png  (a picture of our random number generator)
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) return 0.5 * (rec.normal + Color(1, 1, 1));
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

Image render(const Hittable& world, int samples_per_pixel) {
    const int W = 400, H = 225;
    const double vw = 2.0 * W / H;
    const Point3 eye(0, 0, 0);
    const Vec3 du(vw / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(vw / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Color sum(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; s++) {
                // A random point inside the pixel square instead of its exact center.
                double ox = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
                double oy = samples_per_pixel == 1 ? 0.0 : random_double() - 0.5;
                Point3 target = pixel00 + (i + ox) * du + (j + oy) * dv;
                sum += ray_color(Ray(eye, target - eye), world);
            }
            img.at(i, j) = sum / samples_per_pixel;   // the average of all samples
        }
    return img;
}

// Cut out a rectangle and enlarge it with square pixels.
Image crop_zoom(const Image& src, int x0, int y0, int w, int h, int factor) {
    Image out(w * factor, h * factor);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.at(x, y) = src.at(x0 + x / factor, y0 + y / factor);
    return out;
}

int main() {
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));

    Image one = render(world, 1);
    Image many = render(world, 64);

    // The sphere's upper-right edge lives around pixel (250, 70).
    Image left = crop_zoom(one, 230, 50, 50, 50, 6);
    Image right = crop_zoom(many, 230, 50, 50, 50, 6);
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch16_aa_compare.png", post::side_by_side(left, right, 10), raw);
    save_image("images/ch16_full_64spp.png", many, raw);

    // What do random numbers look like? Each pixel is a random grey.
    Image rnd(256, 128);
    for (auto& c : rnd.data) { double v = random_double(); c = Color(v, v, v); }
    save_image("images/ch16_random_pixels.png", rnd, raw);
    return 0;
}
