# Line by line: `include/pixel/material.h`

[← Line-by-line index](README.md) · Chapters [17](../17-diffuse-materials.md), [18](../18-metal.md), [19](../19-glass.md), [26](../26-lights-cornell-box.md), [28](../28-volumes.md), [32](../32-microfacet-materials.md)

**What this file does, in one sentence:** it defines **materials**, the rules for what happens to light when it hits a
surface: is it absorbed, bounced (in which direction?), bent through it, or does the surface glow?

| Block | Lines | Job | Chapter |
|-------|-------|-----|---------|
| A. Comments, includes | 1–23 | | |
| B. `ScatterRecord` | 25–31 | what a material reports after a hit | 17 |
| C. `Material` base class | 33–55 | the four questions every material answers | 17 |
| D. Schlick (Fresnel) | 57–66 | how reflective a surface is at an angle | 19 |
| E. `Lambertian` (matte) | 68–90 | chalk, paper, clay | 17 |
| F. `Metal` | 92–112 | mirror and brushed metal | 18 |
| G. `Dielectric` (glass) | 114–148 | glass, water, diamond | 19 |
| H. `DiffuseLight` | 150–165 | a glowing surface | 26 |
| I. `Isotropic` | 167–186 | fog and smoke particles | 28 |
| J. GGX helpers | 188–231 | the math of rough surfaces | 32 |
| K. `RoughMetal` | 233–261 | realistic gold, copper, chrome | 32 |
| L. `Plastic` | 263–313 | colored base + clear shiny coat | 32 |
| M. End | 315 | | |

---

## Block A — Comments, includes (lines 1–23)

* Lines 1–12: the list of materials in this file and their chapters.
* Line 13: `#pragma once`.
* Lines 14–15: `<cmath>`, `<memory>` (shared_ptr).
* Lines 16–21: our files: vectors, rays, random numbers, `HitRecord`, textures (surface colors) and PDFs (ways to pick
  random directions).
* Line 23: `namespace pixel`.

---

## Block B — `ScatterRecord` (lines 25–31)

```cpp
struct ScatterRecord {
    Color attenuation;               // how much of each color survives the bounce
    std::shared_ptr<PDF> pdf_ptr;    // for diffuse-like surfaces: how to pick directions
    bool skip_pdf = false;           // for mirror-like surfaces: direction is fixed...
    Ray skip_pdf_ray;                // ...and this is it
};
```

The material fills this in to tell the camera what happens next:

* Line 27: `attenuation` = the color filter of the bounce. (0.8, 0.1, 0.1) keeps 80% of red and 10% of green and blue.
* Line 28: for **matte-like** surfaces: an object that picks random directions (a PDF, chapter 30).
* Line 29: `skip_pdf = true` means "I chose the direction myself" (mirrors, glass)...
* Line 30: ...and this is the new ray.

---

## Block C — `Material` base class (lines 33–55)

```cpp
class Material {
public:
    virtual ~Material() = default;
```

Line 33: the base class of all materials. Line 35: virtual destructor (as in [hittable.md](hittable.md)).

```cpp
    virtual Color emitted(const Ray& /*r_in*/, const HitRecord& /*rec*/,
                          double /*u*/, double /*v*/, const Point3& /*p*/) const {
        return Color(0, 0, 0);
    }
```

Lines 38–41: question 1: **"Do you glow?"** The default answer is black (no light). Only lights change this.

```cpp
    virtual bool scatter(const Ray& /*r_in*/, const HitRecord& /*rec*/, ScatterRecord& /*srec*/) const {
        return false;
    }
```

Lines 44–46: question 2: **"What happens to a ray that hits you?"** Fill `srec` and return true if the light bounces;
return false if it is absorbed. Default: absorbed.

```cpp
    virtual double scattering_pdf(const Ray& /*r_in*/, const HitRecord& /*rec*/, const Ray& /*scattered*/) const {
        return 0.0;
    }
```

Lines 49–51: question 3 (chapter 30): **"How much do you send light into this particular direction?"** Only needed for
matte-like materials.

```cpp
    virtual Color aov_albedo(const HitRecord& /*rec*/) const { return Color(1, 1, 1); }
};
```

Line 54: question 4 (chapter 36): **"What is your plain color here?"** For the denoiser.

(The parameter names are inside `/* */` comments because the default versions don't use them.)

---

## Block D — Schlick (Fresnel) (lines 57–66)

Glass, water and paint reflect **more** when you look at them at a grazing angle. Schlick's formula approximates this:

```cpp
inline double schlick(double cosine, double f0) {
    double m = 1.0 - cosine;
    return f0 + (1.0 - f0) * m * m * m * m * m;
}
```

* `cosine` = cos of the angle between the view and the normal: 1 = looking straight at the surface, 0 = grazing.
* `f0` = the reflectance when looking straight at it (glass: 0.04 = 4%).
* Line 59: `m` = 0 head-on, 1 at grazing.
* Line 60: `f0 + (1 − f0) × m⁵`: head-on it's f0; at grazing it rises to 1 (100% mirror).

```cpp
inline Color schlick(double cosine, const Color& f0) {
    double m = 1.0 - cosine;
    double m5 = m * m * m * m * m;
    return f0 + (Color(1, 1, 1) - f0) * m5;
}
```

Lines 62–66: the same for a **color** f0 (metals like gold reflect colors differently).

---

## Block E — `Lambertian` (matte) (lines 68–90)

```cpp
class Lambertian : public Material {
public:
    Lambertian(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Lambertian(std::shared_ptr<Texture> tex) : tex(tex) {}
```

* Line 71: create from a color: `Lambertian(Color(0.8, 0.1, 0.1))`. Inside, the color is wrapped in a `SolidColor`
  texture.
* Line 72: create from any texture (checkerboard, image...).

```cpp
    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }
```

* Line 75: the color filter = the surface's color at this point (from the texture).
* Line 76: new directions will be picked by a `CosinePDF` around the normal: random, but directions near the normal are
  more likely (that's how matte surfaces behave).
* Line 77: we did **not** fix a direction; the camera picks one.
* Line 78: yes, light bounces.

```cpp
    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }
```

Lines 81–84: how strongly this surface sends light in direction `scattered`: proportional to the cosine of the angle
from the normal (Lambert's law), ÷ π. Below the surface → 0.

```cpp
    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }
private:
    std::shared_ptr<Texture> tex;
};
```

Line 86: plain color for the denoiser. Line 89: the stored texture.

---

## Block F — `Metal` (lines 92–112)

```cpp
    Metal(const Color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}
```

Line 95: a metal color and a **fuzz** amount (0 = perfect mirror, up to 1 = very blurry). Values above 1 are cut to 1.

```cpp
    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        reflected = unit_vector(reflected) + fuzz * random_unit_vector();
```

* Line 98: the perfect mirror direction.
* Line 99: add a small random nudge, scaled by fuzz. The bigger the fuzz, the blurrier the reflections.

```cpp
        srec.attenuation = albedo;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, reflected, r_in.time());
        return dot(reflected, rec.normal) > 0;   // fuzz pushed it below the surface? absorb
    }
```

* Line 100: reflections are tinted by the metal's color.
* Lines 101–103: we chose the direction ourselves: the new ray starts at the hit point.
* Line 104: if the nudge pushed the direction **into** the surface, absorb it (return false).

Lines 107–111: plain color, and the stored color and fuzz.

---

## Block G — `Dielectric` (glass) (lines 114–148)

```cpp
    Dielectric(double ior, const Color& tint = Color(1, 1, 1)) : ior(ior), tint(tint) {}
```

Line 118: `ior` = index of refraction (how much the material bends light). Optional `tint` for colored glass.

```cpp
    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tint;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        double ri = rec.front_face ? (1.0 / ior) : ior;
```

* Line 121: clear glass absorbs nothing (tint white).
* Lines 122–123: glass decides the direction itself.
* Line 124: the ratio for Snell's law: entering the glass (front face) = 1/ior (air → glass); leaving it = ior
  (glass → air).

```cpp
        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
```

* Line 126: the incoming direction, length 1.
* Line 127: cos of the angle between the ray (reversed) and the normal. `fmin(…, 1)` guards rounding.
* Line 128: the sine, from sin² + cos² = 1.

```cpp
        bool cannot_refract = ri * sin_theta > 1.0;     // total internal reflection
        double r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
```

* Line 130: Snell's law says sin θ' = ratio × sin θ. If that's more than 1, there's no way through: the light **must**
  reflect ("total internal reflection", which happens inside glass at steep angles).
* Lines 131–132: `r0` = the head-on reflectance for this pair of materials (about 0.04 for glass).

```cpp
        Vec3 direction;
        if (cannot_refract || schlick(cos_theta, r0) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);
```

* Line 134: reflect if refraction is impossible, **or** at random with chance = the Fresnel reflectance. So about 4% of
  head-on rays and most grazing rays reflect. Over many rays, the right share of light reflects.
* Line 135: mirror direction.
* Line 137: otherwise, bend through (see `refract` in [vec3.md](vec3.md)).

```cpp
        srec.skip_pdf_ray = Ray(rec.p, direction, r_in.time());
        return true;
    }
```

Lines 139–140: the new ray; light always continues (glass doesn't absorb).

Lines 143–147: plain color = tint; stored ior and tint.

---

## Block H — `DiffuseLight` (lines 150–165)

```cpp
    DiffuseLight(std::shared_ptr<Texture> tex, bool two_sided = false) : tex(tex), two_sided(two_sided) {}
    DiffuseLight(const Color& emit, bool two_sided = false)
        : tex(std::make_shared<SolidColor>(emit)), two_sided(two_sided) {}
```

Lines 153–155: a glowing surface, from a texture or a color (like `Color(15, 15, 15)`: light values can be much more
than 1). `two_sided`: glow on both sides (default: only the front).

```cpp
    Color emitted(const Ray&, const HitRecord& rec, double u, double v, const Point3& p) const override {
        if (!rec.front_face && !two_sided) return Color(0, 0, 0);   // lights shine one way
        return tex->value(u, v, p);
    }
```

* Line 157: this material **overrides `emitted`** (the other materials don't).
* Line 158: seen from the back (and not two-sided): no light.
* Line 159: otherwise its light color.

It does **not** override `scatter`, so the default (absorb) is used: a light doesn't reflect anything.

---

## Block I — `Isotropic` (lines 167–186)

Used for fog and smoke (chapter 28): when light hits a particle, it goes off in **any** direction, all equally likely.

```cpp
    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<SpherePDF>();
        srec.skip_pdf = false;
        return true;
    }
    double scattering_pdf(const Ray&, const HitRecord&, const Ray&) const override {
        return 1.0 / (4.0 * pi);
    }
```

* Line 174: the particles' color.
* Line 175: pick directions evenly over the **whole sphere** (`SpherePDF`).
* Lines 179–181: every direction has the same value, 1/(4π) (the whole sphere is 4π in solid angle).

---

## Block J — GGX helpers (lines 188–231), chapter 32

The math for **rough** shiny surfaces. A rough surface is imagined as millions of tiny mirrors ("microfacets") pointing
in slightly different directions. All vectors here are in a **local** coordinate system where the surface normal is +z.

```cpp
namespace ggx {
```

Line 191: an inner namespace, so these are called `ggx::G1`, etc.

### Lines 194–202: masking and shadowing

```cpp
inline double lambda(const Vec3& v, double alpha) {
    if (v.z <= 0.0) return 1e10;
    double tan2 = (v.x * v.x + v.y * v.y) / (v.z * v.z);
    return (-1.0 + std::sqrt(1.0 + alpha * alpha * tan2)) * 0.5;
}
inline double G1(const Vec3& v, double alpha) { return 1.0 / (1.0 + lambda(v, alpha)); }
inline double G2(const Vec3& wi, const Vec3& wo, double alpha) {
    return 1.0 / (1.0 + lambda(wi, alpha) + lambda(wo, alpha));
}
```

On a rough surface, some tiny mirrors are **hidden** behind others (from the viewer or from the light).

* Line 194: `lambda` (Smith's Λ) measures that. Line 195: a direction below the surface → "completely hidden".
* Line 196: tan² of the angle from the normal (grazing angles have large tan).
* Line 197: the Smith formula for GGX: 0 for a smooth surface or head-on view; larger for rough surfaces at grazing angles.
* Line 199: `G1` = the fraction of facets **visible** from one direction (0–1).
* Lines 200–202: `G2` = the fraction visible from **both** the viewer and the light.

### Lines 206–219: pick a random tiny mirror

```cpp
inline Vec3 sample_vndf(const Vec3& ve, double alpha, double u1, double u2) {
```

Picks the direction of one random microfacet **among those the viewer can see** (Heitz 2018). `ve` = the direction to
the viewer; `alpha` = roughness²; `u1, u2` = two random numbers.

* Line 207: **stretch** the view direction by alpha. This turns the rough-surface problem into a simple half-sphere
  problem.
* Lines 208–210: build two axes `t1`, `t2` perpendicular to the stretched direction (`t1` handles the special case of
  looking straight down).
* Lines 211–214: a random point in a disk (radius √u1, angle 2π·u2).
* Lines 215–216: squash the point toward the part of the disk the viewer can actually see (depends on how steeply we
  look: `s`).
* Line 217: lift the point onto the half-sphere → a normal `nh`.
* Line 218: **un-stretch** it back and normalize. That's the microfacet normal.

(It's short but dense. The key idea: facets you can't see are never picked, so no work is wasted.)

### Lines 223–229: bounce off that tiny mirror

```cpp
inline bool sample_reflection(const Vec3& wi_local, double alpha, Vec3& wo_local, Vec3& m_local, double& weight) {
    m_local = sample_vndf(wi_local, alpha, random_double(), random_double());
    wo_local = 2.0 * dot(wi_local, m_local) * m_local - wi_local;
    if (wo_local.z <= 0.0) return false;
    weight = G2(wi_local, wo_local, alpha) / G1(wi_local, alpha);
    return true;
}
```

* Line 223: inputs: the view direction and roughness. Outputs (by reference): the new direction `wo_local`, the facet
  `m_local`, and a `weight`.
* Line 224: pick a visible facet.
* Line 225: mirror the view direction around that facet (the reflect formula, written for a direction pointing away).
* Line 226: if the bounce goes below the surface, fail (the light hits another bump: absorbed).
* Line 227: the weight = G2/G1: how much light survives the shadowing of the outgoing direction. With this sampling
  method, the complicated formulas cancel out to just this.

---

## Block K — `RoughMetal` (lines 233–261)

```cpp
    RoughMetal(const Color& f0, double roughness)
        : f0(f0), alpha(std::fmax(0.001, roughness * roughness)) {}
```

Line 238–239: `f0` = the metal's color when seen head-on (gold ≈ (1.0, 0.77, 0.34)). `alpha` = roughness² (squaring makes
the slider feel more natural), at least 0.001 (a perfect mirror breaks the math).

```cpp
    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        ONB frame(rec.normal);
        Vec3 wi = frame.to_local(-unit_vector(r_in.direction()));   // towards the viewer
        if (wi.z <= 0.0) wi.z = 1e-4;
        wi = unit_vector(wi);
```

* Line 242: a local coordinate system with the normal as +z (see `ONB` in [pdf.md](pdf.md)).
* Line 243: the direction toward the viewer, in local coordinates.
* Lines 244–245: tiny safety fix if it's just below the surface (rounding), then normalize.

```cpp
        Vec3 wo, m;
        double w;
        if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;
        srec.attenuation = schlick(std::fmax(0.0, dot(wi, m)), f0) * w;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
        return true;
    }
```

* Line 248: bounce off a random tiny mirror. If it fails, absorbed.
* Line 249: the color filter = Fresnel (the metal's color, brighter at grazing angles, using the angle to the tiny
  mirror) × the shadowing weight.
* Lines 250–252: we chose the direction; convert it back to world coordinates (`transform`).

---

## Block L — `Plastic` (lines 263–313)

Plastic, paint and varnished wood have **two layers**: a clear shiny coat on top of a colored matte base.

```cpp
    Plastic(std::shared_ptr<Texture> tex, double roughness, double ior = 1.5)
        : tex(tex), alpha(std::fmax(0.001, roughness * roughness)) {
        double r0 = (1.0 - ior) / (1.0 + ior);
        f0 = r0 * r0;   // about 0.04 for ior 1.5
    }
    Plastic(const Color& albedo, double roughness, double ior = 1.5)
        : Plastic(std::make_shared<SolidColor>(albedo), roughness, ior) {}
```

* Lines 268–272: the base color (texture), the coat's roughness, and its `f0` computed from the index of refraction
  (≈ 0.04: a clear coat reflects 4% head-on).
* Lines 273–274: the same from a color. `: Plastic(...)` calls the other constructor (a **delegating constructor**).

```cpp
    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 unit_in = unit_vector(r_in.direction());
        double cos_i = std::fmax(0.0, dot(-unit_in, rec.normal));
        double F = schlick(cos_i, f0);             // how much the coat reflects
        double p_spec = std::fmax(F, 0.2);         // choose the coat more often (less noise)
```

* Lines 277–279: how much the coat reflects at this angle (`F`).
* Line 280: the chance to pick the **coat** layer this time: `F`, but at least 20%. Highlights are small and bright, so
  picking them more often reduces noise; we correct for this below.

```cpp
        if (random_double() < p_spec) {
            ONB frame(rec.normal);
            Vec3 wi = unit_vector(frame.to_local(-unit_in));
            if (wi.z <= 0.0) return false;
            Vec3 wo, m;
            double w;
            if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;
            double Fm = schlick(std::fmax(0.0, dot(wi, m)), f0);
            srec.attenuation = Color(1, 1, 1) * (Fm * w / p_spec);
            srec.pdf_ptr = nullptr;
            srec.skip_pdf = true;
            srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
```

**The coat** (lines 282–293): a GGX reflection like RoughMetal, but:

* the coat is clear, so the reflection is **white** (line 290),
* its strength is `Fm × w`, divided by `p_spec` (the chance we picked this layer). Dividing by the chance keeps the
  average correct.

```cpp
        } else {
            srec.attenuation = tex->value(rec.u, rec.v, rec.p) * ((1.0 - F) / (1.0 - p_spec));
            srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
            srec.skip_pdf = false;
        }
        return true;
    }
```

**The base** (lines 294–298): matte, like Lambertian. The light that wasn't reflected by the coat (`1 − F`) reaches the
colored base; divided by the chance of picking this layer (`1 − p_spec`).

Lines 302–305: the matte `scattering_pdf` (same as Lambertian), used for the base layer.

Lines 307–312: plain color, and stored data.

---

## Block M — End (line 315)

`} // namespace pixel`

---

## Check your understanding

1. Which function makes a material glow? *(`emitted`, only overridden by `DiffuseLight`.)*
2. What's the difference between `skip_pdf = true` and false? *(True: the material chose the direction (mirror, glass).
   False: the camera picks a random direction using `pdf_ptr` (matte).)*
3. Why does glass sometimes reflect, sometimes refract? *(Chosen at random with the Fresnel reflectance as the chance.)*
4. Why divide by `p_spec` in Plastic? *(To correct for choosing the coat more often than its true share.)*
