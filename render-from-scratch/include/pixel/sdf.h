// pixel/sdf.h
// ------------------------------------------------------------
// Signed Distance Functions (SDFs) and ray marching.
// An SDF answers: "how far is point p from the surface?"
//   > 0 outside, < 0 inside, = 0 exactly on the surface.
// With SDFs you can blend shapes like clay, repeat them forever,
// and build fractals - things that are hard with triangles.
// Explained in docs/37-sdf-raymarching.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include "vec3.h"
#include "hittable.h"

namespace pixel {
namespace sdf {

// ---------- primitives (all centered at the origin) ------------------------
inline double sphere(const Point3& p, double r) { return p.length() - r; }

inline double box(const Point3& p, const Vec3& half_size) {
    Vec3 q = vabs(p) - half_size;
    return vmax(q, Vec3(0, 0, 0)).length() + std::fmin(q.max_component(), 0.0);
}

inline double round_box(const Point3& p, const Vec3& half_size, double radius) {
    return box(p, half_size - Vec3(radius)) - radius;
}

// Torus lying in the XZ plane. R = ring radius, r = tube radius.
inline double torus(const Point3& p, double R, double r) {
    double qx = std::sqrt(p.x * p.x + p.z * p.z) - R;
    return std::sqrt(qx * qx + p.y * p.y) - r;
}

// Infinite plane y = height.
inline double plane_y(const Point3& p, double height) { return p.y - height; }

// Vertical capsule from y=0 to y=h.
inline double capsule_y(Point3 p, double h, double r) {
    p.y -= clampd(p.y, 0.0, h);
    return p.length() - r;
}

// ---------- combining shapes -----------------------------------------------
inline double op_union(double a, double b) { return std::fmin(a, b); }
inline double op_subtract(double a, double b) { return std::fmax(a, -b); }   // a minus b
inline double op_intersect(double a, double b) { return std::fmax(a, b); }

// Smooth union: melts two shapes together. k = blend size.
inline double op_smooth_union(double a, double b, double k) {
    double h = clamp01(0.5 + 0.5 * (b - a) / k);
    return lerpd(b, a, h) - k * h * (1.0 - h);
}

// Infinite repetition every 'cell' units in x and z.
inline Point3 op_repeat_xz(const Point3& p, double cell) {
    auto rep = [cell](double v) { return v - cell * std::round(v / cell); };
    return Point3(rep(p.x), p.y, rep(p.z));
}

inline Point3 rotate_y(const Point3& p, double radians) {
    double c = std::cos(radians), s = std::sin(radians);
    return Point3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

// The Mandelbulb fractal (power 8). Returns a distance estimate.
inline double mandelbulb(const Point3& pos, int iterations = 10, double power = 8.0) {
    Vec3 z = pos;
    double dr = 1.0, r = 0.0;
    for (int i = 0; i < iterations; i++) {
        r = z.length();
        if (r > 2.0) break;
        double theta = std::acos(clampd(z.z / r, -1.0, 1.0));
        double phi = std::atan2(z.y, z.x);
        dr = std::pow(r, power - 1.0) * power * dr + 1.0;
        double zr = std::pow(r, power);
        theta *= power;
        phi *= power;
        z = zr * Vec3(std::sin(theta) * std::cos(phi), std::sin(phi) * std::sin(theta), std::cos(theta));
        z += pos;
    }
    if (r < 1e-12) return 0.0;
    return 0.5 * std::log(r) * r / dr;
}

} // namespace sdf

// A Hittable defined by a distance function, found by "sphere tracing":
// step forward by the distance to the nearest surface until we touch it.
class SDFObject : public Hittable {
public:
    using DistanceFn = std::function<double(const Point3&)>;

    SDFObject(DistanceFn fn, const AABB& bounds, std::shared_ptr<Material> mat,
              int max_steps = 256, double epsilon = 1e-4, double step_scale = 1.0)
        : fn(fn), bounds(bounds), mat(mat), max_steps(max_steps), epsilon(epsilon), step_scale(step_scale) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // Only march inside the bounding box.
        double t0 = ray_t.min, t1 = ray_t.max;
        if (!clip_to_box(r, t0, t1)) return false;

        double len = r.direction().length();
        double t = t0;
        // If we start inside the surface (e.g. a bounce from it), we look for the way out.
        double sign = fn(r.at(t)) < 0 ? -1.0 : 1.0;
        for (int i = 0; i < max_steps && t < t1; i++) {
            Point3 p = r.at(t);
            double d = sign * fn(p);
            if (d < epsilon) {
                if (t <= ray_t.min) { t += 2 * epsilon / len; continue; }
                rec.t = t;
                rec.p = p;
                rec.set_face_normal(r, normal_at(p));
                rec.u = rec.v = 0;
                rec.mat = mat.get();
                return true;
            }
            t += step_scale * d / len;    // distance d along a direction of length 'len'
        }
        return false;
    }

    AABB bounding_box() const override { return bounds; }

    // The gradient of the distance field points away from the surface.
    Vec3 normal_at(const Point3& p) const {
        const double h = 1e-4;
        Vec3 n(fn(p + Vec3(h, 0, 0)) - fn(p - Vec3(h, 0, 0)),
               fn(p + Vec3(0, h, 0)) - fn(p - Vec3(0, h, 0)),
               fn(p + Vec3(0, 0, h)) - fn(p - Vec3(0, 0, h)));
        double l = n.length();
        return l > 0 ? n / l : Vec3(0, 1, 0);
    }

private:
    DistanceFn fn;
    AABB bounds;
    std::shared_ptr<Material> mat;
    int max_steps;
    double epsilon;
    double step_scale;

    bool clip_to_box(const Ray& r, double& t0, double& t1) const {
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = bounds.axis_interval(axis);
            double inv = 1.0 / r.direction()[axis];
            double a = (ax.min - r.origin()[axis]) * inv;
            double b = (ax.max - r.origin()[axis]) * inv;
            if (a > b) { double tmp = a; a = b; b = tmp; }
            if (a > t0) t0 = a;
            if (b < t1) t1 = b;
            if (t1 <= t0) return false;
        }
        return true;
    }
};

} // namespace pixel
