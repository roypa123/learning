# Line by line: `ch30_importance_sampling.cpp`

[← Line-by-line index](README.md) · [Chapter 30 (the theory)](../30-importance-sampling.md) · [pdf.h](pdf.md)

**What the whole program does, in one sentence:** it shows that choosing random samples "where it matters" reduces
error: first with a simple integral, then with a picture of random directions, then with two renders of the same scene.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–14 | |
| B. `HemispherePDF` | 16–24 | directions spread evenly over a half-sphere |
| C. `UniformLambertian` | 26–42 | a matte material using that (naive) PDF |
| D. The integral test | 45–67 | three PDFs, three error sizes |
| E. Picture of directions | 69–85 | even vs cosine-weighted, seen from above |
| F. Two renders | 87–111 | uniform vs cosine sampling |
| G. End | 112–113 | |

---

## Block A — Comments, includes (lines 1–14)

Comments, `<cmath>`, `<cstdio>`, our library, `using namespace pixel`.

---

## Block B — `HemispherePDF` (lines 16–24)

```cpp
class HemispherePDF : public PDF {
public:
    explicit HemispherePDF(const Vec3& n) : normal(n) {}
    double value(const Vec3& d) const override { return dot(d, normal) > 0 ? 1.0 / (2 * pi) : 0.0; }
    Vec3 generate() const override { return random_on_hemisphere(normal); }
private:
    Vec3 normal;
};
```

A PDF (see [pdf.md](pdf.md)) that picks directions **evenly** over the half-sphere above a surface:

* Line 20: every direction above the surface has the same chance, 1/(2π) (a half-sphere has size 2π); below: 0.
* Line 21: pick a random direction on that side.

---

## Block C — `UniformLambertian` (lines 26–42)

```cpp
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
```

The same matte behavior as `Lambertian` (same `scattering_pdf`, lines 36–39), but it **picks** directions evenly
(line 32) instead of cosine-weighted. The image comes out the same on average, but noisier: it spends effort on grazing
directions that contribute little.

---

## Block D — The integral test (lines 45–67)

```cpp
    const int N = 1000;
    const int trials = 200;
```

Lines 46–47: each estimate uses 1000 samples; we repeat it 200 times to measure the typical error.

```cpp
    auto run = [&](const char* name, auto sample_x, auto pdf) {
```

Line 48: a helper lambda. Inputs: a name, a function that **picks** a random x (`sample_x`), and a function that says
how likely each x is (`pdf`). `auto` parameters make it a "generic lambda": it accepts any kind of function.

```cpp
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
```

* Lines 50–51: 200 trials.
* Lines 52–56: one estimate: for 1000 random x, add `f(x) / p(x)` (the general Monte Carlo formula: divide by how likely
  x was). f(x) = x².
* Line 57: the estimate = the average.
* Line 58: add the squared error (difference from the true 8/3).
* Line 60: print the **RMS error** (the square root of the average squared error: the "typical" error).

```cpp
    run("uniform  p(x)=1/2",
        [] { return random_double(0, 2); }, [](double) { return 0.5; });
```

Lines 61–62: **uniform**: x evenly between 0 and 2; every x has density 1/2.

```cpp
    run("linear   p(x)=x/2",
        [] { return std::sqrt(random_double(0, 4)); }, [](double x) { return x / 2; });
```

Lines 63–64: **linear**: bigger x more likely (density x/2). To pick x this way: `sqrt(random 0..4)` (the "inverse CDF"
method, chapter 30). Smaller error, because x² is also bigger for bigger x.

```cpp
    run("perfect  p(x)=3x^2/8",
        [] { return 2.0 * std::pow(random_double(), 1.0 / 3.0); },   // inverse CDF: x = 2 * u^(1/3)
        [](double x) { return 3 * x * x / 8; });
```

Lines 65–67: **perfect**: density exactly proportional to x². Then `f(x) / p(x) = 8/3` for every sample: zero error.

---

## Block E — Picture of directions (lines 69–85)

```cpp
        const int S = 360;
        Image a(S, S, hex_color(0x0F172A)), b(S, S, hex_color(0x0F172A));
        Canvas ca(a), cb(b);
        ca.draw_circle(S / 2, S / 2, S / 2 - 10, hex_color(0x64748B));
        cb.draw_circle(S / 2, S / 2, S / 2 - 10, hex_color(0x64748B));
        Vec3 up(0, 0, 1);
```

Lines 71–76: two dark images, each with a grey circle (the half-sphere seen from above). "Up" is +z.

```cpp
        for (int i = 0; i < 3000; i++) {
            Vec3 u = random_on_hemisphere(up);
            Vec3 c = random_cosine_direction();
            double R = S / 2 - 10;
            ca.fill_circle_aa(S / 2 + u.x * R, S / 2 + u.y * R, 1.4, hex_color(0x38BDF8));
            cb.fill_circle_aa(S / 2 + c.x * R, S / 2 + c.y * R, 1.4, hex_color(0xFBBF24));
        }
        save_image("images/ch30_directions.png", post::side_by_side(a, b, 10));
```

* Lines 77–79: 3000 directions of each kind: evenly spread (`u`) and cosine-weighted (`c`).
* Line 80: the circle's radius in pixels.
* Lines 81–82: draw each direction as a dot at its (x, y), seen from above (z is ignored). Even directions pile up near
  the rim (many are nearly horizontal); cosine directions fill the disk evenly (more near the top of the dome).
* Line 84: save both side by side.

---

## Block F — Two renders (lines 87–111)

```cpp
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
```

Lines 89–102: a lambda that builds the same scene (grey ground, terracotta ball) with either cosine sampling
(`Lambertian`) or even sampling (`UniformLambertian`).

```cpp
        Camera cam;
        cam.image_width = 320;
        cam.samples_per_pixel = 8;
        cam.max_depth = 20;
        HittableList uniform_world = build(false), cosine_world = build(true);
        Image img_u = cam.render(uniform_world);
        Image img_c = cam.render(cosine_world);
        save_image("images/ch30_uniform_vs_cosine.png", post::side_by_side(img_u, img_c, 6));
```

* Lines 103–106: only 8 samples, so the noise difference is easy to see.
* Line 107: build both worlds.
* Lines 108–109: render both with the same camera.
* Line 110: save side by side: left (uniform) is grainier than right (cosine).

---

## Block G — End (lines 112–113)

`return 0;` `}`

---

## Check your understanding

1. What is `f(x) / p(x)` when p is exactly proportional to f? *(The same constant for every sample: zero error.)*
2. Why is uniform hemisphere sampling noisier for matte surfaces? *(It often picks grazing directions that contribute
   very little.)*
3. Do both renders converge to the same picture? *(Yes, with enough samples: only the noise differs.)*
