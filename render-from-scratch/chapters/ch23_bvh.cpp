// ch23_bvh.cpp
// ------------------------------------------------------------
// Chapter 23: Bounding Volume Hierarchies.
// We render the same scene of N spheres twice - once with a plain list
// (test every sphere for every ray) and once with a BVH - and time both.
//   images/ch23_many_spheres.png
//   images/ch23_bvh_boxes.png   - a picture of the BVH boxes themselves
// ------------------------------------------------------------
#include <chrono>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

// Time how long it takes to render a small image of the world.
static double time_render(const Hittable& world, Image* out) {
    Camera cam;
    cam.image_width = 320;
    cam.samples_per_pixel = 4;
    cam.max_depth = 4;
    cam.vfov = 35;
    cam.lookfrom = Point3(0, 18, 26);
    cam.lookat = Point3(0, 0, 0);
    cam.show_progress = false;
    auto t0 = std::chrono::steady_clock::now();
    Image img = cam.render(world);
    auto t1 = std::chrono::steady_clock::now();
    if (out) *out = img;
    return std::chrono::duration<double>(t1 - t0).count();
}

int main() {
    const int counts[] = {10, 100, 1000, 3000};
    std::printf("%8s  %12s  %12s  %8s\n", "spheres", "list (s)", "BVH (s)", "speedup");
    Image last;
    for (int n : counts) {
        HittableList list;
        list.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5))));
        Pcg32 rng(n);
        for (int i = 0; i < n; i++) {
            Point3 c(rng.next_double() * 30 - 15, 0.3 + rng.next_double() * 3, rng.next_double() * 30 - 15);
            Color col = srgb_to_linear(hsv(rng.next_double() * 360, 0.7, 0.9));
            list.add(std::make_shared<Sphere>(c, 0.3, std::make_shared<Lambertian>(col)));
        }
        BVHNode bvh(list);
        double t_list = time_render(list, nullptr);
        double t_bvh = time_render(bvh, &last);
        std::printf("%8d  %12.2f  %12.2f  %7.1fx\n", n, t_list, t_bvh, t_list / t_bvh);
    }
    save_image("images/ch23_many_spheres.png", last);

    // ----- Visualize the boxes: a top-down view of the BVH split planes.
    // We draw each sphere as a dot and recursively draw the split boxes on a 2D canvas.
    {
        const int S = 500;
        Image img(S, S, hex_color(0x0B1020));
        Canvas cv(img);
        Pcg32 rng(5);
        std::vector<Point3> pts;
        for (int i = 0; i < 64; i++) pts.push_back(Point3(rng.next_double(), 0, rng.next_double()));

        // Recursive median split (same idea as BVHNode), drawn as rectangles.
        std::function<void(std::vector<Point3>, int)> split = [&](std::vector<Point3> p, int depth) {
            double minx = 1, maxx = 0, minz = 1, maxz = 0;
            for (auto& q : p) { minx = std::min(minx, q.x); maxx = std::max(maxx, q.x); minz = std::min(minz, q.z); maxz = std::max(maxz, q.z); }
            Color c = srgb_to_linear(hsv(depth * 50.0, 0.7, 1.0));
            int x0 = (int)(20 + minx * 460) - 6 + depth, z0 = (int)(20 + minz * 460) - 6 + depth;
            int x1 = (int)(20 + maxx * 460) + 6 - depth, z1 = (int)(20 + maxz * 460) + 6 - depth;
            cv.draw_rect(x0, z0, x1 - x0 + 1, z1 - z0 + 1, c);
            if (p.size() <= 2) return;
            int axis = (maxx - minx) > (maxz - minz) ? 0 : 2;
            std::sort(p.begin(), p.end(), [axis](const Point3& a, const Point3& b) { return a[axis] < b[axis]; });
            std::vector<Point3> left(p.begin(), p.begin() + p.size() / 2), right(p.begin() + p.size() / 2, p.end());
            split(left, depth + 1);
            split(right, depth + 1);
        };
        split(pts, 0);
        for (auto& q : pts) cv.fill_circle_aa(20 + q.x * 460, 20 + q.z * 460, 3.5, Color(1, 1, 1));
        save_image("images/ch23_bvh_boxes.png", img);
    }
    return 0;
}
