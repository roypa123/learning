// pixel/sky.h
// ------------------------------------------------------------
// Backgrounds: what a ray sees when it hits nothing.
// Because our renderer is physically based, the sky is also a
// giant light source that lights the whole scene.
// Explained in docs/33-sky-and-environment.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "image.h"
#include "material.h"
#include "sphere.h"

namespace pixel {

using Background = std::function<Color(const Ray&)>;

// One color in every direction. Color(0,0,0) = a dark room.
inline Background solid_background(const Color& c) {
    return [c](const Ray&) { return c; };
}

// The classic blue-white gradient from "Ray Tracing in One Weekend".
inline Background gradient_sky(const Color& horizon = Color(1.0, 1.0, 1.0),
                               const Color& zenith = Color(0.5, 0.7, 1.0)) {
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double a = 0.5 * (d.y + 1.0);
        return (1.0 - a) * horizon + a * zenith;
    };
}

// A simple artistic "physical-looking" daylight / sunset sky.
struct SkySettings {
    Vec3 sun_direction = unit_vector(Vec3(0.3, 0.5, -1.0));  // points TOWARDS the sun
    Color zenith = Color(0.10, 0.25, 0.65);     // straight up
    Color horizon = Color(0.70, 0.80, 0.95);    // at the horizon
    Color ground = Color(0.18, 0.16, 0.14);     // below the horizon
    Color sun_glow = Color(1.0, 0.7, 0.4);      // halo around the sun
    double glow_strength = 0.6;
    double intensity = 1.0;                     // overall multiplier
    bool draw_sun_disk = false;                 // usually the sun is a real light (make_sun)
    Color sun_radiance = Color(50, 45, 40);
    double sun_angular_radius_deg = 1.5;
};

inline Background physical_sky(const SkySettings& s) {
    return [s](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        Vec3 sun = unit_vector(s.sun_direction);
        double cos_sun = dot(d, sun);
        Color c;
        if (d.y < 0.0) {
            // Ground: fade from horizon color to ground color quickly.
            double t = smoothstep(0.0, 0.08, -d.y);
            c = lerp(s.horizon, s.ground, t);
        } else {
            // Sky: horizon -> zenith. pow makes the horizon band thin.
            double t = std::pow(d.y, 0.45);
            c = lerp(s.horizon, s.zenith, t);
        }
        // Glow around the sun (Mie scattering look), stronger near the horizon.
        double g = std::pow(std::fmax(0.0, cos_sun), 8.0) * s.glow_strength;
        double g2 = std::pow(std::fmax(0.0, cos_sun), 64.0) * s.glow_strength * 2.0;
        c += s.sun_glow * (g + g2);
        if (s.draw_sun_disk) {
            double cos_r = std::cos(degrees_to_radians(s.sun_angular_radius_deg));
            if (cos_sun > cos_r) c += s.sun_radiance;
        }
        return c * s.intensity;
    };
}

// A far-away glowing sphere that acts as the sun. Add it to the world AND to
// the lights list so the renderer can aim shadow rays at it.
inline std::shared_ptr<Sphere> make_sun(const Vec3& direction_to_sun, double angular_radius_deg,
                                        const Color& radiance, double distance = 10000.0) {
    double radius = distance * std::tan(degrees_to_radians(angular_radius_deg));
    Point3 center = unit_vector(direction_to_sun) * distance;
    return std::make_shared<Sphere>(center, radius, std::make_shared<DiffuseLight>(radiance));
}

// A 360 degree photo (equirectangular / "latitude-longitude") as the sky.
inline Background environment_map(std::shared_ptr<Image> env, double intensity = 1.0, double rotation_deg = 0.0) {
    double rot = degrees_to_radians(rotation_deg);
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double phi = std::atan2(d.z, d.x) + rot;
        double theta = std::acos(clampd(d.y, -1.0, 1.0));
        double u = phi / (2 * pi) + 0.5;
        double v = 1.0 - theta / pi;
        return env->sample_bilinear(u, v) * intensity;
    };
}

} // namespace pixel
