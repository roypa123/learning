// pixel/ray.h
// ------------------------------------------------------------
// Ray: a half-line  P(t) = origin + t * direction
// Interval: a range of numbers [min, max]
// Explained in docs/13-rays-and-camera.md and docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"

namespace pixel {

class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction, double time = 0.0)
        : orig(origin), dir(direction), tm(time) {}

    const Point3& origin() const { return orig; }
    const Vec3& direction() const { return dir; }
    double time() const { return tm; }   // moment inside the camera shutter (motion blur)

    Point3 at(double t) const { return orig + t * dir; }

private:
    Point3 orig;
    Vec3 dir;
    double tm = 0.0;
};

struct Interval {
    double min = +infinity;   // default interval is empty
    double max = -infinity;

    Interval() {}
    Interval(double mn, double mx) : min(mn), max(mx) {}
    // The smallest interval containing both a and b.
    Interval(const Interval& a, const Interval& b)
        : min(a.min <= b.min ? a.min : b.min), max(a.max >= b.max ? a.max : b.max) {}

    double size() const { return max - min; }
    bool contains(double x) const { return min <= x && x <= max; }
    bool surrounds(double x) const { return min < x && x < max; }
    double clamp(double x) const { return x < min ? min : (x > max ? max : x); }
    Interval expand(double delta) const { double p = delta / 2; return Interval(min - p, max + p); }

    static Interval empty() { return Interval(+infinity, -infinity); }
    static Interval universe() { return Interval(-infinity, +infinity); }
};

inline Interval operator+(const Interval& ival, double displacement) {
    return Interval(ival.min + displacement, ival.max + displacement);
}

} // namespace pixel
