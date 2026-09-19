// pixel/camera.h
// ------------------------------------------------------------
// The Camera: builds rays for every pixel, traces them through the
// world, and averages the results. This is the heart of the renderer.
//   * positionable camera (lookfrom / lookat / vup / vfov)   docs/20
//   * depth of field (defocus blur)                          docs/20
//   * anti-aliasing with stratified samples                  docs/16
//   * multithreading                                         docs/21
//   * motion blur (random ray time)                          docs/22
//   * light sampling with mixture PDFs                       docs/31
//   * Russian roulette + firefly clamping                    docs/31
//   * AOV buffers (albedo / normal) for the denoiser         docs/36
// ------------------------------------------------------------
#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "random.h"
#include "hittable.h"
#include "material.h"
#include "pdf.h"
#include "image.h"
#include "sky.h"

namespace pixel {

class Camera {
public:
    // ---------------- image settings ----------------
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 400;
    int samples_per_pixel = 10;      // rays per pixel: more = less noise, slower
    int max_depth = 10;              // maximum number of bounces

    // ---------------- lens & position ----------------
    double vfov = 90;                        // vertical field of view in degrees
    Point3 lookfrom = Point3(0, 0, 0);       // where the camera is
    Point3 lookat = Point3(0, 0, -1);        // what it looks at
    Vec3 vup = Vec3(0, 1, 0);                // which way is "up"
    double defocus_angle = 0;                // 0 = everything sharp
    double focus_dist = 10;                  // distance that is perfectly sharp
    double shift_x = 0, shift_y = 0;         // lens shift (moves the image window)

    // ---------------- lighting ----------------
    Background background = gradient_sky();

    // ---------------- quality & speed ----------------
    int threads = 0;                    // 0 = use all CPU cores
    bool show_progress = true;
    bool russian_roulette = true;       // randomly stop weak paths (faster, still unbiased)
    int rr_start_depth = 3;
    double max_sample_value = 0.0;      // > 0: clamp very bright samples (removes fireflies)
    uint64_t seed = 1;
    double t_min = 0.001;               // ignore hits closer than this (avoids "shadow acne")

    // ---------------- extra outputs ----------------
    bool collect_aovs = false;          // also fill albedo_aov and normal_aov
    Image albedo_aov;
    Image normal_aov;

    // Extra per-sample information for the denoiser (first surface hit).
    struct AovSlot { Color albedo; Vec3 normal; };

    int image_height() const { int h = (int)(image_width / aspect_ratio); return h < 1 ? 1 : h; }

    // Render the whole image. 'lights' may be nullptr (then no light sampling).
    Image render(const Hittable& world, const Hittable* lights = nullptr) {
        initialize();
        Image img(image_width, height);
        if (collect_aovs) {
            albedo_aov = Image(image_width, height);
            normal_aov = Image(image_width, height);
        }
        light_list = lights;
        if (auto list = dynamic_cast<const HittableList*>(lights))
            if (list->objects.empty()) light_list = nullptr;

        int n_threads = threads > 0 ? threads : (int)std::thread::hardware_concurrency();
        if (n_threads < 1) n_threads = 1;

        std::atomic<int> next_row(0);
        std::atomic<int> rows_done(0);
        std::mutex print_mutex;
        auto start_time = std::chrono::steady_clock::now();

        auto worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1);
                if (j >= height) break;
                // Seed per row: the image is identical no matter how many threads we use.
                seed_thread_rng(seed * 1000003ULL + (uint64_t)j * 7919ULL + 17);
                for (int i = 0; i < image_width; i++) {
                    Color pixel_color(0, 0, 0);
                    Color albedo_sum(0, 0, 0);
                    Vec3 normal_sum(0, 0, 0);
                    for (int s = 0; s < samples_per_pixel; s++) {
                        Ray r = get_ray(i, j, s);
                        AovSlot slot;
                        pixel_color += trace(r, world, collect_aovs ? &slot : nullptr);
                        albedo_sum += slot.albedo;
                        normal_sum += slot.normal;
                    }
                    img.at(i, j) = pixel_color / samples_per_pixel;
                    if (collect_aovs) {
                        albedo_aov.at(i, j) = albedo_sum / samples_per_pixel;
                        normal_aov.at(i, j) = normal_sum / samples_per_pixel;
                    }
                }
                int done = ++rows_done;
                if (show_progress) {
                    std::lock_guard<std::mutex> lock(print_mutex);
                    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                    std::printf("\rRendering: %3d%%  (%d/%d rows, %.1fs)   ", 100 * done / height, done, height, secs);
                    std::fflush(stdout);
                }
            }
        };

        std::vector<std::thread> pool;
        for (int t = 0; t < n_threads; t++) pool.emplace_back(worker);
        for (auto& th : pool) th.join();

        if (show_progress) {
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
            std::printf("\rRendering: done in %.1f seconds using %d threads.            \n", secs, n_threads);
        }
        return img;
    }

    // Trace one ray and return the light (radiance) coming back along it.
    // This is the "path tracer": an iterative loop over bounces.
    Color trace(const Ray& start_ray, const Hittable& world, AovSlot* aov = nullptr) const {
        Color radiance(0, 0, 0);
        Color throughput(1, 1, 1);     // how much light survives the bounces so far
        Ray ray = start_ray;

        for (int depth = 0; depth < max_depth; depth++) {
            HitRecord rec;
            if (!world.hit(ray, Interval(t_min, infinity), rec)) {
                Color bg = background(ray);
                radiance += throughput * bg;
                if (aov && depth == 0) { aov->albedo = bg; aov->normal = Vec3(0, 0, 0); }
                break;
            }
            if (!rec.mat) break;   // an object without a material: treat as black
            if (aov && depth == 0) { aov->albedo = rec.mat->aov_albedo(rec); aov->normal = rec.normal; }

            // 1. Light emitted by the surface itself.
            radiance += throughput * rec.mat->emitted(ray, rec, rec.u, rec.v, rec.p);

            // 2. Does the light bounce?
            ScatterRecord srec;
            if (!rec.mat->scatter(ray, rec, srec)) break;

            if (srec.skip_pdf) {
                // Mirror-like: the direction was decided by the material.
                throughput *= srec.attenuation;
                ray = srec.skip_pdf_ray;
            } else {
                // Diffuse-like: choose a direction, half towards lights, half by the material.
                Vec3 dir;
                double pdf_val;
                if (light_list) {
                    HittablePDF light_pdf(*light_list, rec.p);
                    MixturePDF mix(&light_pdf, srec.pdf_ptr.get());
                    dir = mix.generate();
                    pdf_val = mix.value(dir);
                } else {
                    dir = srec.pdf_ptr->generate();
                    pdf_val = srec.pdf_ptr->value(dir);
                }
                if (pdf_val <= 1e-12) break;
                Ray scattered(rec.p, dir, ray.time());
                double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);
                throughput *= srec.attenuation * (scattering_pdf / pdf_val);
                ray = scattered;
            }

            // 3. Russian roulette: weak paths are stopped at random; survivors get stronger.
            if (russian_roulette && depth >= rr_start_depth) {
                double p = clampd(throughput.max_component(), 0.05, 0.95);
                if (random_double() > p) break;
                throughput /= p;
            }
        }

        if (max_sample_value > 0.0) {
            double m = radiance.max_component();
            if (m > max_sample_value) radiance *= max_sample_value / m;
        }
        if (radiance.x != radiance.x || radiance.y != radiance.y || radiance.z != radiance.z)
            return Color(0, 0, 0);   // drop NaNs
        return radiance;
    }

    // Build the camera coordinate system. Called automatically by render().
    void initialize() {
        height = image_height();
        center = lookfrom;

        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        double viewport_height = 2 * h * focus_dist;
        double viewport_width = viewport_height * ((double)image_width / height);

        // u = right, v = up, w = backwards (camera looks along -w)
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        Vec3 viewport_u = viewport_width * u;      // across the top edge
        Vec3 viewport_v = viewport_height * -v;    // down the left edge
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / height;

        Point3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2
                                   + shift_x * viewport_u - shift_y * viewport_v;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;

        sqrt_spp = (int)std::sqrt((double)samples_per_pixel);
        if (sqrt_spp < 1) sqrt_spp = 1;
    }

    // Ray for pixel (i, j), sample number s.
    Ray get_ray(int i, int j, int s) const {
        double ox, oy;
        if (s < sqrt_spp * sqrt_spp) {
            // Stratified: split the pixel into a grid, one random point per cell.
            int si = s % sqrt_spp, sj = s / sqrt_spp;
            ox = (si + random_double()) / sqrt_spp - 0.5;
            oy = (sj + random_double()) / sqrt_spp - 0.5;
        } else {
            ox = random_double() - 0.5;
            oy = random_double() - 0.5;
        }
        Point3 pixel_sample = pixel00_loc + ((i + ox) * pixel_delta_u) + ((j + oy) * pixel_delta_v);
        Point3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        Vec3 ray_direction = pixel_sample - ray_origin;
        double ray_time = random_double();       // for motion blur: shutter open during [0,1)
        return Ray(ray_origin, ray_direction, ray_time);
    }

private:
    int height = 1;
    Point3 center;
    Point3 pixel00_loc;
    Vec3 pixel_delta_u, pixel_delta_v;
    Vec3 u, v, w;
    Vec3 defocus_disk_u, defocus_disk_v;
    int sqrt_spp = 1;
    const Hittable* light_list = nullptr;

    Point3 defocus_disk_sample() const {
        Vec3 p = random_in_unit_disk();
        return center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
    }
};

} // namespace pixel
