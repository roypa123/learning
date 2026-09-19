// pixel/instance.h
// ------------------------------------------------------------
// Instances: move or rotate an object WITHOUT changing the object.
// Trick: instead of moving the object, move the RAY the opposite way.
// Explained in docs/27-instances.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "hittable.h"

namespace pixel {

class Translate : public Hittable {
public:
    Translate(std::shared_ptr<Hittable> object, const Vec3& offset) : object(object), offset(offset) {
        bbox = object->bounding_box() + offset;
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // Move the ray backwards by the offset.
        Ray offset_r(r.origin() - offset, r.direction(), r.time());
        if (!object->hit(offset_r, ray_t, rec)) return false;
        // Move the hit point forwards by the offset.
        rec.p += offset;
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    Vec3 offset;
    AABB bbox;
};

class RotateY : public Hittable {
public:
    RotateY(std::shared_ptr<Hittable> object, double angle_degrees) : object(object) {
        double radians = degrees_to_radians(angle_degrees);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        AABB b = object->bounding_box();

        // Rotate all 8 corners of the box and take the box around them.
        Point3 min( infinity,  infinity,  infinity);
        Point3 max(-infinity, -infinity, -infinity);
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    double x = i ? b.x.max : b.x.min;
                    double y = j ? b.y.max : b.y.min;
                    double z = k ? b.z.max : b.z.min;
                    Vec3 tester = to_world(Vec3(x, y, z));
                    min = vmin(min, tester);
                    max = vmax(max, tester);
                }
        bbox = AABB(min, max);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // World space -> object space.
        Ray rotated_r(to_object(r.origin()), to_object(r.direction()), r.time());
        if (!object->hit(rotated_r, ray_t, rec)) return false;
        // Object space -> world space.
        rec.p = to_world(rec.p);
        rec.normal = to_world(rec.normal);
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    double sin_theta, cos_theta;
    AABB bbox;

    Vec3 to_world(const Vec3& p) const {
        return Vec3(cos_theta * p.x + sin_theta * p.z, p.y, -sin_theta * p.x + cos_theta * p.z);
    }
    Vec3 to_object(const Vec3& p) const {
        return Vec3(cos_theta * p.x - sin_theta * p.z, p.y, sin_theta * p.x + cos_theta * p.z);
    }
};

} // namespace pixel
