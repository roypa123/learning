# Chapter 30 — Importance sampling

[← Monte Carlo](29-monte-carlo.md) · [Contents](README.md) · [Next: Light sampling →](31-light-sampling.md)

> 📖 **Line by line:** [pdf explained line by line](line-by-line/pdf.md)

---

## Goal

Monte Carlo works with **any** way of choosing random samples, as long as we weight them correctly.
Choosing samples **where the function is big** reduces noise dramatically. This is **importance
sampling**. You'll learn:

* **probability density functions** (PDFs) and the general Monte Carlo estimator `f(x) / p(x)`,
* **inverse transform sampling**: turning uniform random numbers into any distribution,
* **directions and solid angles**,
* why Lambertian surfaces sample directions with a **cosine** distribution,
* orthonormal bases (**ONB**) and the `PDF` classes in our library,
* exactly what `Camera::trace` computes at each diffuse bounce.

---

## 1. Not all samples are equally useful

Look at `f(x) = x²` on [0, 2]. Most of its area is on the right: `f(0.5) = 0.25` but `f(1.9) = 3.61`.
Uniform samples spend half their effort on the left half, which contributes only 1/8 of the total. What if
we sampled the right side **more often**?

We can, if we **correct** for it: samples taken where we look often must count less, and samples from
places we rarely look must count more.

### 1.1 The PDF

A **probability density function** `p(x)` describes how likely each x is to be chosen. It's ≥ 0
everywhere and its total area is 1. Uniform on [0, 2]: `p(x) = 1/2`.

### 1.2 The general estimator

```
∫ f(x) dx  ≈  (1/N) · Σ f(xᵢ) / p(xᵢ)          (xᵢ chosen with density p)
```

With `p = 1/(b−a)` (uniform) this is exactly chapter 29's formula. It's correct for **any** p that is
non-zero wherever f is non-zero.

### 1.3 The best PDF

If `p` has **the same shape as f** (p ∝ f), then `f(x)/p(x)` is the **same constant** for every sample, and
**every sample gives the exact answer**: zero variance! We can't do that in general (normalizing p would
require knowing the integral we're trying to compute), but the closer p's shape is to f, the less noise.

The chapter program measures this for `∫₀² x² dx`:

| PDF | shape | RMS error with 1000 samples |
|-----|-------|-----------------------------|
| uniform `1/2` | flat | ~0.075 |
| linear `x/2` | ramps up, roughly like x² | ~0.030 |
| perfect `3x²/8` | exactly like x² | **0** |

---

## 2. Generating samples with a given PDF

Our random generator gives uniform numbers ξ in [0, 1). How do we get x with density p(x)?

**Inverse transform sampling**:

1. Compute the **CDF** (cumulative distribution): `P(x) = ∫₀ˣ p(s) ds`, the probability that the sample is ≤ x.
   It goes from 0 to 1.
2. Solve `P(x) = ξ` for x.

```
 P(x)
 1 ┤                ╭──       pick ξ on the vertical axis,
 ξ ┤- - - - - - - ╭╯          go across to the curve,
   │            ╭╯│           and down: that's x
   │        ╭──╯  │
 0 ┼───────╯──────┴──
                  x
```

Example: `p(x) = 3x²/8` on [0, 2] → `P(x) = x³/8` → `x = 2 · ξ^(1/3)`.
Example: `p(x) = x/2` → `P(x) = x²/4` → `x = sqrt(4ξ)`.

We already used this for fog distances in chapter 28: `d = −ln(1 − ξ)/σ` is the inverse CDF of the
exponential distribution.

---

## 3. Directions and solid angle

Light at a surface point comes from all directions of the **hemisphere** above it. So we need to
integrate over **directions**, and that needs a notion of the "size" of a set of directions: the
**solid angle**.

A solid angle is the area a set of directions covers on a sphere of radius 1. It's measured in
**steradians** (sr). The whole sphere is 4π sr, a hemisphere is 2π sr.

```
          ╭────╮
       ╭──┤ ▓▓ ├──╮     the patch ▓▓ on the unit sphere = the solid angle
      │   ╰────╯   │    of the cone of directions that points to it
      │     ●      │
       ╲          ╱
        ╰────────╯
```

A **uniform** PDF over the hemisphere is `1/(2π)`; over the sphere, `1/(4π)`.

---

## 4. Sampling a Lambertian surface

### 4.1 What we're integrating

For a Lambertian surface, the rendering equation (chapter 29) says the reflected light is:

```
L_out = ∫ (albedo/π) · L_in(ω) · cos θ  dω          over the hemisphere
```

The integrand contains `cos θ`. Directions near the normal count more; grazing directions count almost
nothing.

### 4.2 Uniform sampling (the naive way)

Pick ω uniformly on the hemisphere (`p = 1/(2π)`), and the estimate is

```
(albedo/π) · L_in · cos θ / (1/(2π))  =  2 · albedo · L_in · cos θ
```

Grazing samples (cos θ ≈ 0) contribute almost nothing: wasted work.

### 4.3 Cosine sampling (the smart way)

Choose directions with `p(ω) = cos θ / π`, the same shape as the cos θ in the integrand. Then:

```
(albedo/π) · L_in · cos θ / (cos θ / π)  =  albedo · L_in
```

The cosines cancel. Every sample is just `albedo × incoming light`. That's exactly the
`throughput *= albedo` from chapter 17, and it's why "normal + random unit vector" was the right choice there:
it produces precisely this cosine distribution.

### 4.4 Generating cosine-weighted directions

A classic trick (Malley's method): pick a **uniform point on the unit disk**, then **project it up** onto
the hemisphere. Areas near the center of the disk map to directions near the top, and the projection
naturally creates the cos θ density. In formulas, with two uniform numbers r₁, r₂:

```
φ = 2π · r₁
x = cos(φ) · sqrt(r₂)
y = sin(φ) · sqrt(r₂)          (x, y) is uniform on the disk
z = sqrt(1 − r₂)               lift it onto the hemisphere around +z
```

That's `random_cosine_direction()` in `random.h`. It produces directions around the **+z** axis. To use it
around a surface normal we need a coordinate system aligned with the normal.

### 4.5 Orthonormal bases (ONB)

An **ONB** is three perpendicular unit vectors **u, v, w**. We build one with w = the normal:

```cpp
axis[2] = unit_vector(n);                                     // w = normal
Vec3 a = (std::fabs(axis[2].x) > 0.9) ? Vec3(0,1,0) : Vec3(1,0,0);   // any vector not parallel to w
axis[1] = unit_vector(cross(axis[2], a));                     // v ⟂ w
axis[0] = cross(axis[2], axis[1]);                            // u ⟂ both
```

Then a local direction `(x, y, z)` becomes `x·u + y·v + z·w` in world space (`transform`), and
`to_local` does the opposite with three dot products. The GGX materials (chapter 32) work in this local frame.

---

## 5. The PDF classes

A PDF object can do two things:

```cpp
class PDF {
public:
    virtual double value(const Vec3& direction) const = 0;   // how likely is this direction?
    virtual Vec3 generate() const = 0;                        // give me a random direction
};
```

| Class | Distribution | Used for |
|-------|--------------|----------|
| `SpherePDF` | uniform over the sphere, `1/(4π)` | fog (isotropic scattering) |
| `CosinePDF(normal)` | `cos θ / π` | Lambertian and plastic surfaces |
| `HittablePDF(objects, point)` | directions towards some objects | lights (chapter 31) |
| `MixturePDF(a, b)` | 50% a, 50% b | combining the above (chapter 31) |

**File: `include/pixel/pdf.h`**

```cpp
// pixel/pdf.h
// ------------------------------------------------------------
// ONB  = OrthoNormal Basis: a little coordinate system around a normal.
// PDF  = Probability Density Function: "how likely is each direction?"
// These let us send rays where the light is, instead of randomly.
// Explained in docs/30-importance-sampling.md and docs/31-light-sampling.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "random.h"
#include "hittable.h"

namespace pixel {

class ONB {
public:
    // Build three perpendicular unit axes; w points along n.
    explicit ONB(const Vec3& n) {
        axis[2] = unit_vector(n);
        Vec3 a = (std::fabs(axis[2].x) > 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        axis[1] = unit_vector(cross(axis[2], a));
        axis[0] = cross(axis[2], axis[1]);
    }
    const Vec3& u() const { return axis[0]; }
    const Vec3& v() const { return axis[1]; }
    const Vec3& w() const { return axis[2]; }

    // From local (x,y,z) coordinates to world coordinates.
    Vec3 transform(const Vec3& v) const { return v.x * axis[0] + v.y * axis[1] + v.z * axis[2]; }
    // From world coordinates to local coordinates.
    Vec3 to_local(const Vec3& v) const { return Vec3(dot(v, axis[0]), dot(v, axis[1]), dot(v, axis[2])); }

private:
    Vec3 axis[3];
};

class PDF {
public:
    virtual ~PDF() {}
    virtual double value(const Vec3& direction) const = 0;   // density of this direction
    virtual Vec3 generate() const = 0;                        // pick a random direction
};

// Every direction equally likely.
class SpherePDF : public PDF {
public:
    double value(const Vec3&) const override { return 1.0 / (4.0 * pi); }
    Vec3 generate() const override { return random_unit_vector(); }
};

// Directions near the normal are more likely: density = cos(theta)/pi.
class CosinePDF : public PDF {
public:
    explicit CosinePDF(const Vec3& w) : uvw(w) {}
    double value(const Vec3& direction) const override {
        double cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0.0, cosine_theta / pi);
    }
    Vec3 generate() const override { return uvw.transform(random_cosine_direction()); }
private:
    ONB uvw;
};

// Directions towards a (list of) object(s), usually the lights.
class HittablePDF : public PDF {
public:
    HittablePDF(const Hittable& objects, const Point3& origin) : objects(objects), origin(origin) {}
    double value(const Vec3& direction) const override { return objects.pdf_value(origin, direction); }
    Vec3 generate() const override { return objects.random(origin); }
private:
    const Hittable& objects;
    Point3 origin;
};

// 50/50 mix of two PDFs.
class MixturePDF : public PDF {
public:
    MixturePDF(const PDF* p0, const PDF* p1) { p[0] = p0; p[1] = p1; }
    double value(const Vec3& direction) const override {
        return 0.5 * p[0]->value(direction) + 0.5 * p[1]->value(direction);
    }
    Vec3 generate() const override {
        return random_double() < 0.5 ? p[0]->generate() : p[1]->generate();
    }
private:
    const PDF* p[2];
};

} // namespace pixel
```

---

## 6. What `Camera::trace` really computes

Now we can read the diffuse branch of the path tracer completely:

```cpp
dir = srec.pdf_ptr->generate();                               // sample a direction with density p
pdf_val = srec.pdf_ptr->value(dir);                           // p(dir)
double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);   // the material's cos θ / π
throughput *= srec.attenuation * (scattering_pdf / pdf_val);
```

The Monte Carlo weight is `f · cos θ / p`. We split the material's `f · cos θ` into
`attenuation × scattering_pdf`: for Lambertian, `albedo × (cos θ / π)`. When we sample with the
material's own distribution, `scattering_pdf / pdf_val = 1`. But when we sample with a **different** PDF
(for example, towards a light), the ratio is no longer 1, and it automatically gives the correct
weight. That's the flexibility we need for the next chapter.

---

## 7. The program

**File: `chapters/ch30_importance_sampling.cpp`**

```cpp
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
```

```bat
run ch30_importance_sampling
```

### Console

The printed RMS errors show the effect of the PDF shape (the exact numbers vary run to run):

```
uniform  p(x)=1/2            RMS error with 1000 samples: 0.07...
linear   p(x)=x/2            RMS error with 1000 samples: 0.03...
perfect  p(x)=3x^2/8         RMS error with 1000 samples: 0.000000
```

### Images

![Directions](../images/ch30_directions.png)

> **Image description:** Two dark squares, each with a grey circle, seen as if looking straight down onto a
> hemisphere. **Left (uniform):** 3000 light-blue dots, **denser toward the rim** of the circle (uniform
> directions include many grazing ones, which project near the edge). **Right (cosine):** 3000 yellow dots
> spread **evenly** over the disk: directions near the top are favored.

![Uniform vs cosine](../images/ch30_uniform_vs_cosine.png)

> **Image description:** Two renders side by side of a terracotta-colored ball on a grey ground under the
> sky, both with only 8 samples per pixel. **Left (uniform hemisphere sampling):** clearly grainier,
> especially in the shadow under the ball and on the ball's lower half. **Right (cosine sampling):**
> the same brightness and colors but visibly less noise. Both converge to the same image with enough samples:
> importance sampling changes the noise, not the answer.

---

## Try it yourself

1. Add a fourth PDF to the console test: `p(x) = 3x²/8` but sampled with the **wrong** inverse (e.g.
   uniform samples divided by this p). What happens? (The estimate becomes wrong. PDF and sampler must match!)
2. Render the uniform vs cosine comparison with 64 samples. Is the difference still visible?
3. Prove that `z = sqrt(1 − r₂)` gives `p = cos θ / π` (hint: the disk area within radius r is πr²).
4. Implement a `UniformHemispherePDF` in the library and use it for a "UniformLambertian" material.

## Common problems

| Symptom | Cause |
|---------|-------|
| Image too bright or too dark after changing PDFs | `value()` doesn't match `generate()` |
| Black speckles | PDF value is zero for a generated direction → division by zero (guarded in `trace`) |
| Directions all bunch around one axis | ONB not built from the normal, or `transform` skipped |

---

## Summary

* General Monte Carlo: `average of f(x)/p(x)` with x drawn from p. It's correct for any valid p.
* The closer p is to f's shape, the less noise (importance sampling).
* Inverse transform sampling turns uniform numbers into any distribution.
* Lambertian: sample `cos θ / π`, and the cosines cancel. Directions are built in an ONB around the normal.
* `trace` multiplies by `attenuation × scattering_pdf / pdf_value`, which is correct for any sampling PDF.

Next: [Chapter 31 — Light sampling and mixture PDFs →](31-light-sampling.md)
