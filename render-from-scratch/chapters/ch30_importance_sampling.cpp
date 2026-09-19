// ch30_importance_sampling.cpp
// ------------------------------------------------------------
// Chapter 30: Importance sampling.
//  1. Console: the same integral with a uniform PDF vs a smarter PDF.
//  2. Image: where uniform vs cosine-weighted directions land.
//       images/ch30_directions.png
//  3. Render: a matte scene with UNIFORM hemisphere sampling vs COSINE
//     sampling, same number of samples.
//       images/ch30_uniform_vs_cosine.png
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

// Uniform over the hemisphere around a normal: density 1/(2*pi).
class HemispherePDF : public PDF {
public:
    explicit HemispherePDF(const Vec3& n) : normal(n) {}
    double value(const Vec3& d) const override { return dot(d, normal) > 0 ? 1.0 / (2 * pi) : 0.0; }
    Vec3 generate() const override { return random_on_hemisphere(normal); }
private:
    Vec3 normal;
};

// A Lambertian surface that picks directions uniformly (the "naive" way).
class UniformLambertian : public Material {
public:
    UniformLambertian(const Color& a) : albedo(a) {}
    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = albedo;
        srec.pdf_ptr = std::make_shared<HemispherePDF>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }
    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double c = dot(rec.normal, unit_vector(scattered.direction()));
        return c < 0 ? 0 : c / pi;
    }
private:
    Color albedo;
};

int main() {
    // ---------- 1. Integral of x^2 on [0,2] with three different PDFs -----
    const int N = 1000;
    const int trials = 200;
    auto run = [&](const char* name, auto sample_x, auto pdf) {
        double sum_sq_err = 0;
        for (int t = 0; t < trials; t++) {
            double sum = 0;
            for (int i = 0; i < N; i++) {
                double x = sample_x();
                sum += (x * x) / pdf(x);   // f(x) / p(x)
            }
            double est = sum / N;
            sum_sq_err += (est - 8.0 / 3.0) * (est - 8.0 / 3.0);
        }
        std::printf("%-28s RMS error with %d samples: %.6f\n", name, N, std::sqrt(sum_sq_err / trials));
    };
    run("uniform  p(x)=1/2",
        [] { return random_double(0, 2); }, [](double) { return 0.5; });
    run("linear   p(x)=x/2",
        [] { return std::sqrt(random_double(0, 4)); }, [](double x) { return x / 2; });
    run("perfect  p(x)=3x^2/8",
        [] { return 2.0 * std::pow(random_double(), 1.0 / 3.0); },   // inverse CDF: x = 2 * u^(1/3)
        [](double x) { return 3 * x * x / 8; });

    // ---------- 2. Where do the directions go? (seen from above) ----------
    {
        const int S = 360;
        Image a(S, S, hex_color(0x0F172A)), b(S, S, hex_color(0x0F172A));
        Canvas ca(a), cb(b);
        ca.draw_circle(S / 2, S / 2, S / 2 - 10, hex_color(0x64748B));
        cb.draw_circle(S / 2, S / 2, S / 2 - 10, hex_color(0x64748B));
        Vec3 up(0, 0, 1);
        for (int i = 0; i < 3000; i++) {
            Vec3 u = random_on_hemisphere(up);
            Vec3 c = random_cosine_direction();
            double R = S / 2 - 10;
            ca.fill_circle_aa(S / 2 + u.x * R, S / 2 + u.y * R, 1.4, hex_color(0x38BDF8));
            cb.fill_circle_aa(S / 2 + c.x * R, S / 2 + c.y * R, 1.4, hex_color(0xFBBF24));
        }
        save_image("images/ch30_directions.png", post::side_by_side(a, b, 10));
    }

    // ---------- 3. Same scene, two sampling strategies --------------------
    {
        auto build = [](bool cosine) {
            HittableList world;
            std::shared_ptr<Material> ground, ball;
            if (cosine) {
                ground = std::make_shared<Lambertian>(Color(0.6, 0.6, 0.6));
                ball = std::make_shared<Lambertian>(Color(0.7, 0.3, 0.2));
            } else {
                ground = std::make_shared<UniformLambertian>(Color(0.6, 0.6, 0.6));
                ball = std::make_shared<UniformLambertian>(Color(0.7, 0.3, 0.2));
            }
            world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, ground));
            world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, ball));
            return world;
        };
        Camera cam;
        cam.image_width = 320;
        cam.samples_per_pixel = 8;
        cam.max_depth = 20;
        HittableList uniform_world = build(false), cosine_world = build(true);
        Image img_u = cam.render(uniform_world);
        Image img_c = cam.render(cosine_world);
        save_image("images/ch30_uniform_vs_cosine.png", post::side_by_side(img_u, img_c, 6));
    }
    return 0;
}
