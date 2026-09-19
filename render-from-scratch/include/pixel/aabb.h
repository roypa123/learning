// pixel/aabb.h
// ------------------------------------------------------------
// AABB = Axis-Aligned Bounding Box: the simplest box around an object.
// Testing a ray against a box is very cheap, so we test boxes first.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"
#include "ray.h"

namespace pixel {

class AABB {
public:
    Interval x, y, z;

    AABB() {}   // empty box
    AABB(const Interval& ix, const Interval& iy, const Interval& iz) : x(ix), y(iy), z(iz) {
        pad_to_minimums();
    }
    // Box spanned by two corner points (in any order).
    AABB(const Point3& a, const Point3& b) {
        x = a.x <= b.x ? Interval(a.x, b.x) : Interval(b.x, a.x);
        y = a.y <= b.y ? Interval(a.y, b.y) : Interval(b.y, a.y);
        z = a.z <= b.z ? Interval(a.z, b.z) : Interval(b.z, a.z);
        pad_to_minimums();
    }
    // Box around two boxes.
    AABB(const AABB& a, const AABB& b) : x(a.x, b.x), y(a.y, b.y), z(a.z, b.z) {}

    const Interval& axis_interval(int n) const { return n == 1 ? y : (n == 2 ? z : x); }

    // The "slab method": intersect the ray with 3 pairs of parallel planes.
    bool hit(const Ray& r, Interval ray_t) const {
        const Point3& o = r.origin();
        const Vec3& d = r.direction();
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / d[axis];
            double t0 = (ax.min - o[axis]) * adinv;
            double t1 = (ax.max - o[axis]) * adinv;
            if (t0 > t1) { double tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > ray_t.min) ray_t.min = t0;
            if (t1 < ray_t.max) ray_t.max = t1;
            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }

    int longest_axis() const {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }

    Point3 centroid() const { return Point3((x.min + x.max) * 0.5, (y.min + y.max) * 0.5, (z.min + z.max) * 0.5); }

    double surface_area() const {
        double a = x.size(), b = y.size(), c = z.size();
        return 2.0 * (a * b + b * c + c * a);
    }

    static AABB empty() { return AABB(Interval::empty(), Interval::empty(), Interval::empty()); }

private:
    // A flat box (e.g. around a flat quad) has zero thickness, which breaks
    // the math. Give every side at least a tiny thickness.
    void pad_to_minimums() {
        const double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

inline AABB operator+(const AABB& b, const Vec3& offset) {
    return AABB(b.x + offset.x, b.y + offset.y, b.z + offset.z);
}

} // namespace pixel
