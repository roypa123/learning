// pixel/vec3.h
// ------------------------------------------------------------
// A tiny 3D vector class. We use it for:
//   * positions in space      (Point3)
//   * directions              (Vec3)
//   * colors (red,green,blue) (Color)
// Explained in docs/12-vectors.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <iostream>

namespace pixel {

// Our own constants (we do not rely on M_PI, which is not standard C++).
constexpr double pi       = 3.1415926535897932385;
constexpr double infinity = 1e300;

inline double degrees_to_radians(double degrees) { return degrees * pi / 180.0; }
inline double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double e0, double e1, double x) {
    double t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() {}
    Vec3(double v) : x(v), y(v), z(v) {}
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}

    // Access by index: v[0] == v.x, v[1] == v.y, v[2] == v.z
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    double& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }

    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(double t) { x *= t; y *= t; z *= t; return *this; }
    Vec3& operator*=(const Vec3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
    Vec3& operator/=(double t) { return *this *= 1.0 / t; }

    double length_squared() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(length_squared()); }

    // True if the vector is very close to zero in every direction.
    bool near_zero() const {
        const double s = 1e-8;
        return std::fabs(x) < s && std::fabs(y) < s && std::fabs(z) < s;
    }

    double max_component() const { return x > y ? (x > z ? x : z) : (y > z ? y : z); }
    double min_component() const { return x < y ? (x < z ? x : z) : (y < z ? y : z); }
};

// Aliases: same type, different meaning. Makes code easier to read.
using Point3 = Vec3;
using Color  = Vec3;

inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator*(double t, const Vec3& v) { return Vec3(t * v.x, t * v.y, t * v.z); }
inline Vec3 operator*(const Vec3& v, double t) { return t * v; }
inline Vec3 operator/(const Vec3& v, double t) { return (1.0 / t) * v; }
inline Vec3 operator/(const Vec3& a, const Vec3& b) { return Vec3(a.x / b.x, a.y / b.y, a.z / b.z); }

inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

inline Vec3 unit_vector(const Vec3& v) { return v / v.length(); }

inline Vec3 vmin(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z));
}
inline Vec3 vmax(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z));
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }
inline Vec3 vabs(const Vec3& v) { return Vec3(std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)); }

// Mirror reflection of v around the normal n (n must be unit length).
inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - 2.0 * dot(v, n) * n; }

// Snell's law refraction. uv and n must be unit length.
// etai_over_etat = (index of refraction we come from) / (index we go into)
inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

// Brightness of a linear color as the human eye perceives it (Rec.709 weights).
inline double luminance(const Color& c) { return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z; }

} // namespace pixel
