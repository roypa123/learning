// pixel/sphere.h
// ------------------------------------------------------------
// The sphere: the "hello world" of ray tracing.
// Supports motion (for motion blur) and light sampling.
// Explained in docs/14-hitting-a-sphere.md, docs/22-motion-blur.md,
//              docs/31-light-sampling.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "hittable.h"
#include "pdf.h"

namespace pixel {

class Sphere : public Hittable {
public:
    // A sphere that stays still.
    Sphere(const Point3& center, double radius, std::shared_ptr<Material> mat)
        : center(center, Vec3(0, 0, 0)), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        bbox = AABB(center - rvec, center + rvec);
    }

    // A sphere that moves from center1 (time 0) to center2 (time 1).
    Sphere(const Point3& center1, const Point3& center2, double radius, std::shared_ptr<Material> mat)
        : center(center1, center2 - center1), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        AABB box1(center.at(0) - rvec, center.at(0) + rvec);
        AABB box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = AABB(box1, box2);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Point3 current_center = center.at(r.time());
        Vec3 oc = current_center - r.origin();
        double a = r.direction().length_squared();
        double h = dot(r.direction(), oc);
        double c = oc.length_squared() - radius * radius;

        double discriminant = h * h - a * c;
        if (discriminant < 0) return false;          // the ray misses
        double sqrtd = std::sqrt(discriminant);

        // Find the nearest root inside the acceptable range.
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat.get();
        return true;
    }

    AABB bounding_box() const override { return bbox; }

    // p: a point on the unit sphere centered at the origin.
    // u: angle around the Y axis from X=-1, in [0,1]
    // v: angle from Y=-1 to Y=+1, in [0,1]
    static void get_sphere_uv(const Point3& p, double& u, double& v) {
        double theta = std::acos(clampd(-p.y, -1.0, 1.0));
        double phi = std::atan2(-p.z, p.x) + pi;
        u = phi / (2 * pi);
        v = theta / pi;
    }

    // ----- light sampling: we only sample the cone of directions that hit us.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double dist_squared = (center.at(0) - origin).length_squared();
        if (dist_squared <= radius * radius) return 0.0;
        double cos_theta_max = std::sqrt(1.0 - radius * radius / dist_squared);
        double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);
        return 1.0 / solid_angle;
    }

    Vec3 random(const Point3& origin) const override {
        Vec3 direction = center.at(0) - origin;
        double distance_squared = direction.length_squared();
        if (distance_squared <= radius * radius) return random_unit_vector();
        ONB uvw(direction);
        return uvw.transform(random_to_sphere(radius, distance_squared));
    }

private:
    Ray center;     // center.at(time) = position at that time
    double radius;
    std::shared_ptr<Material> mat;
    AABB bbox;

    static Vec3 random_to_sphere(double radius, double distance_squared) {
        double r1 = random_double();
        double r2 = random_double();
        double z = 1 + r2 * (std::sqrt(1 - radius * radius / distance_squared) - 1);
        double phi = 2 * pi * r1;
        double x = std::cos(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        double y = std::sin(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        return Vec3(x, y, z);
    }
};

} // namespace pixel
