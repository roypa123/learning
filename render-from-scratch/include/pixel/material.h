// pixel/material.h
// ------------------------------------------------------------
// Materials decide what happens to light when it touches a surface:
// absorbed? bounced? bent? emitted?
//   Lambertian    - matte, like chalk or paper       (docs/17)
//   Metal         - mirror / brushed metal           (docs/18)
//   Dielectric    - glass, water, diamond            (docs/19)
//   DiffuseLight  - a glowing surface (a lamp)       (docs/26)
//   Isotropic     - scattering inside fog / smoke    (docs/28)
//   RoughMetal    - physically based GGX metal       (docs/32)
//   Plastic       - colored base + shiny clear coat  (docs/32)
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "random.h"
#include "hittable.h"
#include "texture.h"
#include "pdf.h"

namespace pixel {

// What a material tells the renderer after a hit.
struct ScatterRecord {
    Color attenuation;               // how much of each color survives the bounce
    std::shared_ptr<PDF> pdf_ptr;    // for diffuse-like surfaces: how to pick directions
    bool skip_pdf = false;           // for mirror-like surfaces: direction is fixed...
    Ray skip_pdf_ray;                // ...and this is it
};

class Material {
public:
    virtual ~Material() = default;

    // Light produced by the surface itself (only lights return non-zero).
    virtual Color emitted(const Ray& /*r_in*/, const HitRecord& /*rec*/,
                          double /*u*/, double /*v*/, const Point3& /*p*/) const {
        return Color(0, 0, 0);
    }

    // Returns false if the ray is absorbed.
    virtual bool scatter(const Ray& /*r_in*/, const HitRecord& /*rec*/, ScatterRecord& /*srec*/) const {
        return false;
    }

    // The material's own density for bouncing towards 'scattered'.
    virtual double scattering_pdf(const Ray& /*r_in*/, const HitRecord& /*rec*/, const Ray& /*scattered*/) const {
        return 0.0;
    }

    // Surface color used by the denoiser (docs/36-denoising.md).
    virtual Color aov_albedo(const HitRecord& /*rec*/) const { return Color(1, 1, 1); }
};

// Schlick's approximation of Fresnel reflectance.
inline double schlick(double cosine, double f0) {
    double m = 1.0 - cosine;
    return f0 + (1.0 - f0) * m * m * m * m * m;
}
inline Color schlick(double cosine, const Color& f0) {
    double m = 1.0 - cosine;
    double m5 = m * m * m * m * m;
    return f0 + (Color(1, 1, 1) - f0) * m5;
}

// ---------------------------------------------------------------------------
class Lambertian : public Material {
public:
    Lambertian(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Lambertian(std::shared_ptr<Texture> tex) : tex(tex) {}

    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
};

// ---------------------------------------------------------------------------
class Metal : public Material {
public:
    Metal(const Color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        reflected = unit_vector(reflected) + fuzz * random_unit_vector();
        srec.attenuation = albedo;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, reflected, r_in.time());
        return dot(reflected, rec.normal) > 0;   // fuzz pushed it below the surface? absorb
    }

    Color aov_albedo(const HitRecord&) const override { return albedo; }

private:
    Color albedo;
    double fuzz;
};

// ---------------------------------------------------------------------------
class Dielectric : public Material {
public:
    // ior = index of refraction: air 1.0, water 1.33, glass 1.5, diamond 2.4
    Dielectric(double ior, const Color& tint = Color(1, 1, 1)) : ior(ior), tint(tint) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tint;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        double ri = rec.front_face ? (1.0 / ior) : ior;

        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;     // total internal reflection
        double r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
        Vec3 direction;
        if (cannot_refract || schlick(cos_theta, r0) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        srec.skip_pdf_ray = Ray(rec.p, direction, r_in.time());
        return true;
    }

    Color aov_albedo(const HitRecord&) const override { return tint; }

private:
    double ior;
    Color tint;
};

// ---------------------------------------------------------------------------
class DiffuseLight : public Material {
public:
    DiffuseLight(std::shared_ptr<Texture> tex, bool two_sided = false) : tex(tex), two_sided(two_sided) {}
    DiffuseLight(const Color& emit, bool two_sided = false)
        : tex(std::make_shared<SolidColor>(emit)), two_sided(two_sided) {}

    Color emitted(const Ray&, const HitRecord& rec, double u, double v, const Point3& p) const override {
        if (!rec.front_face && !two_sided) return Color(0, 0, 0);   // lights shine one way
        return tex->value(u, v, p);
    }

private:
    std::shared_ptr<Texture> tex;
    bool two_sided;
};

// ---------------------------------------------------------------------------
class Isotropic : public Material {
public:
    Isotropic(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Isotropic(std::shared_ptr<Texture> tex) : tex(tex) {}

    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<SpherePDF>();
        srec.skip_pdf = false;
        return true;
    }
    double scattering_pdf(const Ray&, const HitRecord&, const Ray&) const override {
        return 1.0 / (4.0 * pi);
    }
    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
};

// ===================== Microfacet (GGX) helpers ============================
// Explained in docs/32-microfacet-materials.md. All vectors are in the local
// frame where the surface normal is +z.
namespace ggx {

// Smith Lambda function for GGX.
inline double lambda(const Vec3& v, double alpha) {
    if (v.z <= 0.0) return 1e10;
    double tan2 = (v.x * v.x + v.y * v.y) / (v.z * v.z);
    return (-1.0 + std::sqrt(1.0 + alpha * alpha * tan2)) * 0.5;
}
inline double G1(const Vec3& v, double alpha) { return 1.0 / (1.0 + lambda(v, alpha)); }
inline double G2(const Vec3& wi, const Vec3& wo, double alpha) {
    return 1.0 / (1.0 + lambda(wi, alpha) + lambda(wo, alpha));
}

// Sample a visible micro-normal (Heitz 2018, "Sampling the GGX Distribution
// of Visible Normals"). ve = direction towards the viewer, local frame.
inline Vec3 sample_vndf(const Vec3& ve, double alpha, double u1, double u2) {
    Vec3 vh = unit_vector(Vec3(alpha * ve.x, alpha * ve.y, ve.z));
    double lensq = vh.x * vh.x + vh.y * vh.y;
    Vec3 t1 = lensq > 0 ? Vec3(-vh.y, vh.x, 0) / std::sqrt(lensq) : Vec3(1, 0, 0);
    Vec3 t2 = cross(vh, t1);
    double r = std::sqrt(u1);
    double phi = 2.0 * pi * u2;
    double p1 = r * std::cos(phi);
    double p2 = r * std::sin(phi);
    double s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * std::sqrt(std::fmax(0.0, 1.0 - p1 * p1)) + s * p2;
    Vec3 nh = p1 * t1 + p2 * t2 + std::sqrt(std::fmax(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return unit_vector(Vec3(alpha * nh.x, alpha * nh.y, std::fmax(1e-6, nh.z)));
}

// Sample a reflected direction off a rough surface. Returns false if the
// sample goes below the surface. 'weight' receives G2/G1 (Fresnel not included).
inline bool sample_reflection(const Vec3& wi_local, double alpha, Vec3& wo_local, Vec3& m_local, double& weight) {
    m_local = sample_vndf(wi_local, alpha, random_double(), random_double());
    wo_local = 2.0 * dot(wi_local, m_local) * m_local - wi_local;
    if (wo_local.z <= 0.0) return false;
    weight = G2(wi_local, wo_local, alpha) / G1(wi_local, alpha);
    return true;
}

} // namespace ggx

// ---------------------------------------------------------------------------
// Physically based metal: gold, copper, aluminium, chrome...
// roughness 0 = perfect mirror, 1 = very dull.
class RoughMetal : public Material {
public:
    RoughMetal(const Color& f0, double roughness)
        : f0(f0), alpha(std::fmax(0.001, roughness * roughness)) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        ONB frame(rec.normal);
        Vec3 wi = frame.to_local(-unit_vector(r_in.direction()));   // towards the viewer
        if (wi.z <= 0.0) wi.z = 1e-4;
        wi = unit_vector(wi);
        Vec3 wo, m;
        double w;
        if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;
        srec.attenuation = schlick(std::fmax(0.0, dot(wi, m)), f0) * w;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
        return true;
    }

    Color aov_albedo(const HitRecord&) const override { return f0; }

private:
    Color f0;
    double alpha;
};

// ---------------------------------------------------------------------------
// Plastic / painted / varnished surfaces: a diffuse colored base under a
// clear glossy coat. We randomly pick one of the two layers per bounce.
class Plastic : public Material {
public:
    Plastic(std::shared_ptr<Texture> tex, double roughness, double ior = 1.5)
        : tex(tex), alpha(std::fmax(0.001, roughness * roughness)) {
        double r0 = (1.0 - ior) / (1.0 + ior);
        f0 = r0 * r0;   // about 0.04 for ior 1.5
    }
    Plastic(const Color& albedo, double roughness, double ior = 1.5)
        : Plastic(std::make_shared<SolidColor>(albedo), roughness, ior) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 unit_in = unit_vector(r_in.direction());
        double cos_i = std::fmax(0.0, dot(-unit_in, rec.normal));
        double F = schlick(cos_i, f0);             // how much the coat reflects
        double p_spec = std::fmax(F, 0.2);         // choose the coat more often (less noise)

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
        } else {
            srec.attenuation = tex->value(rec.u, rec.v, rec.p) * ((1.0 - F) / (1.0 - p_spec));
            srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
            srec.skip_pdf = false;
        }
        return true;
    }

    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
    double alpha;
    double f0;
};

} // namespace pixel
