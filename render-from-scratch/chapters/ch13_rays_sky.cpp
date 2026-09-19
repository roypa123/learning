// ch13_rays_sky.cpp
// ------------------------------------------------------------
// Chapter 13: Rays and a simple camera.
// One ray per pixel, shot from the eye through the pixel.
// A ray that hits nothing shows the sky: blue up, white down.
//   images/ch13_sky.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

// The color a ray "sees". For now there is nothing in the world but sky.
Color ray_color(const Ray& r) {
    Vec3 unit_direction = unit_vector(r.direction());
    double a = 0.5 * (unit_direction.y + 1.0);     // y in [-1,1]  ->  a in [0,1]
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    // ---------- image ----------
    const double aspect_ratio = 16.0 / 9.0;
    const int image_width = 400;
    const int image_height = (int)(image_width / aspect_ratio);   // 225

    // ---------- camera ----------
    const double focal_length = 1.0;               // distance eye -> viewport
    const double viewport_height = 2.0;
    const double viewport_width = viewport_height * (double(image_width) / image_height);
    const Point3 camera_center(0, 0, 0);

    // Vectors along the viewport edges (x to the right, y DOWN the image).
    const Vec3 viewport_u(viewport_width, 0, 0);
    const Vec3 viewport_v(0, -viewport_height, 0);
    // Distance between neighbouring pixel centers.
    const Vec3 pixel_delta_u = viewport_u / image_width;
    const Vec3 pixel_delta_v = viewport_v / image_height;
    // Location of the upper-left pixel's center.
    const Point3 viewport_upper_left = camera_center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    const Point3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // ---------- render ----------
    Image img(image_width, image_height);
    for (int j = 0; j < image_height; j++) {
        for (int i = 0; i < image_width; i++) {
            Point3 pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
            Vec3 ray_direction = pixel_center - camera_center;
            Ray r(camera_center, ray_direction);
            img.at(i, j) = ray_color(r);
        }
    }
    // These sky colors were chosen as display values, so save WITHOUT the sRGB curve.
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch13_sky.png", img, raw);
    return 0;
}
