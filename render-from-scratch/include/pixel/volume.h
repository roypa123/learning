// pixel/volume.h
// ------------------------------------------------------------
// Participating media: fog, smoke, mist, clouds.
// A ray inside the volume has a chance to scatter at every step.
// Explained in docs/28-volumes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include <functional>
#include "vec3.h"
#include "hittable.h"
#include "material.h"
#include "random.h"

namespace pixel {

// A volume of constant density, shaped like 'boundary' (a closed object).
class ConstantMedium : public Hittable {
public:
    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, const Color& albedo)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, std::shared_ptr<Texture> tex)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(tex)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        // Where does the ray enter and leave the boundary?
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        // Random distance until the next collision with a particle.
        double hit_distance = neg_inv_density * std::log(1.0 - random_double());
        if (hit_distance > distance_inside_boundary) return false;   // passed through

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);
        rec.normal = Vec3(1, 0, 0);   // arbitrary: particles have no surface
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double neg_inv_density;
    std::shared_ptr<Material> phase_function;
};

// A volume whose density changes in space (e.g. smoke, clouds).
// Uses "delta tracking" (Woodcock tracking): max_density must be >= density(p) everywhere.
class VariableMedium : public Hittable {
public:
    VariableMedium(std::shared_ptr<Hittable> boundary, double max_density,
                   std::function<double(const Point3&)> density, const Color& albedo)
        : boundary(boundary), max_density(max_density), density(density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;
        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double t = rec1.t;
        while (true) {
            // Step as if the medium had max density everywhere...
            t -= std::log(1.0 - random_double()) / (max_density * ray_length);
            if (t >= rec2.t) return false;
            // ...then accept a real collision with probability density/max_density.
            if (random_double() < density(r.at(t)) / max_density) break;
        }
        rec.t = t;
        rec.p = r.at(t);
        rec.normal = Vec3(1, 0, 0);
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double max_density;
    std::function<double(const Point3&)> density;
    std::shared_ptr<Material> phase_function;
};

} // namespace pixel
