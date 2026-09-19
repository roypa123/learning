// pixel/hittable.h
// ------------------------------------------------------------
// "Hittable" = anything a ray can hit (sphere, quad, triangle,
// a whole list of objects, a BVH tree...).
// Explained in docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include <memory>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "aabb.h"
#include "random.h"

namespace pixel {

class Material;   // defined in material.h

// Everything we want to know about the place where a ray hit something.
struct HitRecord {
    Point3 p;                          // the hit point
    Vec3 normal;                       // unit normal, always facing AGAINST the ray
    const Material* mat = nullptr;     // what the surface is made of
    double t = 0.0;                    // ray parameter: p = origin + t*direction
    double u = 0.0, v = 0.0;           // texture coordinates
    bool front_face = true;            // did we hit the outside of the surface?

    // outward_normal must be unit length.
    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Hittable {
public:
    virtual ~Hittable() = default;

    // Does the ray hit this object with t inside ray_t? If yes, fill rec.
    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

    virtual AABB bounding_box() const = 0;

    // --- Used for light sampling (docs/31-light-sampling.md) ---
    // Probability density of choosing 'direction' from 'origin' towards this object.
    virtual double pdf_value(const Point3& /*origin*/, const Vec3& /*direction*/) const { return 0.0; }
    // A random direction from 'origin' towards this object.
    virtual Vec3 random(const Point3& /*origin*/) const { return Vec3(1, 0, 0); }
};

// A list of hittables that behaves like one hittable.
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;

    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }

    void clear() { objects.clear(); bbox = AABB(); }

    void add(std::shared_ptr<Hittable> object) {
        bbox = objects.empty() ? object->bounding_box() : AABB(bbox, object->bounding_box());
        objects.push_back(object);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord temp;
        bool hit_anything = false;
        double closest = ray_t.max;
        for (const auto& object : objects) {
            if (object->hit(r, Interval(ray_t.min, closest), temp)) {
                hit_anything = true;
                closest = temp.t;       // only accept closer hits from now on
                rec = temp;
            }
        }
        return hit_anything;
    }

    AABB bounding_box() const override { return bbox; }

    // Light sampling over a list: pick one object at random.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        if (objects.empty()) return 0.0;
        double weight = 1.0 / objects.size();
        double sum = 0.0;
        for (const auto& object : objects) sum += weight * object->pdf_value(origin, direction);
        return sum;
    }

    Vec3 random(const Point3& origin) const override {
        if (objects.empty()) return Vec3(1, 0, 0);
        int i = (int)(random_double() * objects.size());
        if (i >= (int)objects.size()) i = (int)objects.size() - 1;
        return objects[i]->random(origin);
    }

private:
    AABB bbox;
};

} // namespace pixel
