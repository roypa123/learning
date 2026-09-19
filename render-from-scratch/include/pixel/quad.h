// pixel/quad.h
// ------------------------------------------------------------
// Quad: a flat parallelogram given by a corner Q and two edge
// vectors u and v.        Q+v ------- Q+u+v
//                          |           |
//                          Q  -------  Q+u
// Also: Disk, and make_box() which builds a box from 6 quads.
// Explained in docs/25-quads-triangles-meshes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "hittable.h"
#include "random.h"

namespace pixel {

class Quad : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, std::shared_ptr<Material> mat)
        : Q(Q), u(u), v(v), mat(mat) {
        Vec3 n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);                 // plane equation: dot(normal, P) = D
        w = n / dot(n, n);                  // helper for computing alpha/beta
        area = n.length();
        AABB diag1(Q, Q + u + v);
        AABB diag2(Q + u, Q + v);
        bbox = AABB(diag1, diag2);
    }

    AABB bounding_box() const override { return bbox; }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        double denom = dot(normal, r.direction());
        if (std::fabs(denom) < 1e-8) return false;         // ray parallel to the plane

        double t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;

        // Where on the plane did we land, in (alpha, beta) coordinates?
        Point3 intersection = r.at(t);
        Vec3 planar_hitpt = intersection - Q;
        double alpha = dot(w, cross(planar_hitpt, v));
        double beta  = dot(w, cross(u, planar_hitpt));
        if (!is_interior(alpha, beta, rec)) return false;

        rec.t = t;
        rec.p = intersection;
        rec.mat = mat.get();
        rec.set_face_normal(r, normal);
        return true;
    }

    // For light sampling: probability density of hitting us from 'origin'.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double distance_squared = rec.t * rec.t * direction.length_squared();
        double cosine = std::fabs(dot(direction, rec.normal) / direction.length());
        if (cosine < 1e-8) return 0.0;
        return distance_squared / (cosine * area);
    }

    Vec3 random(const Point3& origin) const override {
        Point3 p = Q + (random_double() * u) + (random_double() * v);
        return p - origin;
    }

protected:
    // Inside the parallelogram if both coordinates are in [0,1].
    virtual bool is_interior(double a, double b, HitRecord& rec) const {
        Interval unit_interval(0, 1);
        if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }

    Point3 Q;
    Vec3 u, v;
    Vec3 w;
    std::shared_ptr<Material> mat;
    AABB bbox;
    Vec3 normal;
    double D;
    double area;
};

// A disk (circle) inside the parallelogram. Q is the corner, so the center
// is Q + u/2 + v/2. Use square u and v of equal length for a round disk.
class Disk : public Quad {
public:
    Disk(const Point3& center, const Vec3& half_u, const Vec3& half_v, std::shared_ptr<Material> mat)
        : Quad(center - half_u - half_v, 2.0 * half_u, 2.0 * half_v, mat) {
        area = area * pi / 4.0;
    }
    Vec3 random(const Point3& origin) const override {
        Vec3 d = random_in_unit_disk();
        Point3 p = Q + u * (0.5 + 0.5 * d.x) + v * (0.5 + 0.5 * d.y);
        return p - origin;
    }
protected:
    bool is_interior(double a, double b, HitRecord& rec) const override {
        double da = a - 0.5, db = b - 0.5;
        if (da * da + db * db > 0.25) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

// A box made of 6 quads, from two opposite corners a and b.
inline std::shared_ptr<HittableList> make_box(const Point3& a, const Point3& b, std::shared_ptr<Material> mat) {
    auto sides = std::make_shared<HittableList>();
    Point3 min = vmin(a, b);
    Point3 max = vmax(a, b);
    Vec3 dx(max.x - min.x, 0, 0);
    Vec3 dy(0, max.y - min.y, 0);
    Vec3 dz(0, 0, max.z - min.z);
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, max.z),  dx,  dy, mat)); // front
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, max.z), -dz,  dy, mat)); // right
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, min.z), -dx,  dy, mat)); // back
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dz,  dy, mat)); // left
    sides->add(std::make_shared<Quad>(Point3(min.x, max.y, max.z),  dx, -dz, mat)); // top
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dx,  dz, mat)); // bottom
    return sides;
}

} // namespace pixel
