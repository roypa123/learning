// pixel/pdf.h
// ------------------------------------------------------------
// ONB  = OrthoNormal Basis: a little coordinate system around a normal.
// PDF  = Probability Density Function: "how likely is each direction?"
// These let us send rays where the light is, instead of randomly.
// Explained in docs/30-importance-sampling.md and docs/31-light-sampling.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "random.h"
#include "hittable.h"

namespace pixel {

class ONB {
public:
    // Build three perpendicular unit axes; w points along n.
    explicit ONB(const Vec3& n) {
        axis[2] = unit_vector(n);
        Vec3 a = (std::fabs(axis[2].x) > 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        axis[1] = unit_vector(cross(axis[2], a));
        axis[0] = cross(axis[2], axis[1]);
    }
    const Vec3& u() const { return axis[0]; }
    const Vec3& v() const { return axis[1]; }
    const Vec3& w() const { return axis[2]; }

    // From local (x,y,z) coordinates to world coordinates.
    Vec3 transform(const Vec3& v) const { return v.x * axis[0] + v.y * axis[1] + v.z * axis[2]; }
    // From world coordinates to local coordinates.
    Vec3 to_local(const Vec3& v) const { return Vec3(dot(v, axis[0]), dot(v, axis[1]), dot(v, axis[2])); }

private:
    Vec3 axis[3];
};

class PDF {
public:
    virtual ~PDF() {}
    virtual double value(const Vec3& direction) const = 0;   // density of this direction
    virtual Vec3 generate() const = 0;                        // pick a random direction
};

// Every direction equally likely.
class SpherePDF : public PDF {
public:
    double value(const Vec3&) const override { return 1.0 / (4.0 * pi); }
    Vec3 generate() const override { return random_unit_vector(); }
};

// Directions near the normal are more likely: density = cos(theta)/pi.
class CosinePDF : public PDF {
public:
    explicit CosinePDF(const Vec3& w) : uvw(w) {}
    double value(const Vec3& direction) const override {
        double cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0.0, cosine_theta / pi);
    }
    Vec3 generate() const override { return uvw.transform(random_cosine_direction()); }
private:
    ONB uvw;
};

// Directions towards a (list of) object(s), usually the lights.
class HittablePDF : public PDF {
public:
    HittablePDF(const Hittable& objects, const Point3& origin) : objects(objects), origin(origin) {}
    double value(const Vec3& direction) const override { return objects.pdf_value(origin, direction); }
    Vec3 generate() const override { return objects.random(origin); }
private:
    const Hittable& objects;
    Point3 origin;
};

// 50/50 mix of two PDFs.
class MixturePDF : public PDF {
public:
    MixturePDF(const PDF* p0, const PDF* p1) { p[0] = p0; p[1] = p1; }
    double value(const Vec3& direction) const override {
        return 0.5 * p[0]->value(direction) + 0.5 * p[1]->value(direction);
    }
    Vec3 generate() const override {
        return random_double() < 0.5 ? p[0]->generate() : p[1]->generate();
    }
private:
    const PDF* p[2];
};

} // namespace pixel
