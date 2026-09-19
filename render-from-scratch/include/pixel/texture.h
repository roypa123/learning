// pixel/texture.h
// ------------------------------------------------------------
// Textures answer one question: "what color is the surface HERE?"
// Explained in docs/24-textures.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <functional>
#include "vec3.h"
#include "image.h"
#include "noise.h"

namespace pixel {

class Texture {
public:
    virtual ~Texture() = default;
    // u,v = texture coordinates, p = the 3D hit point
    virtual Color value(double u, double v, const Point3& p) const = 0;
};

class SolidColor : public Texture {
public:
    SolidColor(const Color& albedo) : albedo(albedo) {}
    SolidColor(double r, double g, double b) : albedo(r, g, b) {}
    Color value(double, double, const Point3&) const override { return albedo; }
private:
    Color albedo;
};

// 3D checkerboard: alternates between two textures in space.
class CheckerTexture : public Texture {
public:
    CheckerTexture(double scale, std::shared_ptr<Texture> even, std::shared_ptr<Texture> odd)
        : inv_scale(1.0 / scale), even(even), odd(odd) {}
    CheckerTexture(double scale, const Color& c1, const Color& c2)
        : CheckerTexture(scale, std::make_shared<SolidColor>(c1), std::make_shared<SolidColor>(c2)) {}

    Color value(double u, double v, const Point3& p) const override {
        int x = (int)std::floor(inv_scale * p.x);
        int y = (int)std::floor(inv_scale * p.y);
        int z = (int)std::floor(inv_scale * p.z);
        bool is_even = ((x + y + z) % 2) == 0;
        return is_even ? even->value(u, v, p) : odd->value(u, v, p);
    }
private:
    double inv_scale;
    std::shared_ptr<Texture> even, odd;
};

// Checkerboard in texture (u,v) space - follows the surface.
class UVCheckerTexture : public Texture {
public:
    UVCheckerTexture(int squares_u, int squares_v, const Color& c1, const Color& c2)
        : nu(squares_u), nv(squares_v), c1(c1), c2(c2) {}
    Color value(double u, double v, const Point3&) const override {
        int iu = (int)std::floor(u * nu), iv = (int)std::floor(v * nv);
        return ((iu + iv) % 2 == 0) ? c1 : c2;
    }
private:
    int nu, nv;
    Color c1, c2;
};

// A picture wrapped onto the surface.
class ImageTexture : public Texture {
public:
    ImageTexture(const Image& img) : image(img) {}
    // Loads a PPM file. If loading fails the texture shows magenta.
    ImageTexture(const std::string& ppm_filename) {
        if (!read_ppm(ppm_filename, image))
            std::printf("WARNING: could not load texture '%s'\n", ppm_filename.c_str());
    }
    Color value(double u, double v, const Point3&) const override {
        if (image.width == 0) return Color(1, 0, 1);
        return image.sample_bilinear(u, v);
    }
private:
    Image image;
};

// Perlin noise based textures.
class NoiseTexture : public Texture {
public:
    enum Style { Smooth, Turbulence, Marble };
    NoiseTexture(double scale, Style style = Marble, const Color& tint = Color(1, 1, 1))
        : scale(scale), style(style), tint(tint) {}

    Color value(double, double, const Point3& p) const override {
        switch (style) {
            case Smooth:     return tint * 0.5 * (1.0 + noise.noise(scale * p));
            case Turbulence: return tint * noise.turbulence(scale * p, 7);
            case Marble:
            default:         return tint * 0.5 * (1.0 + std::sin(scale * p.z + 10.0 * noise.turbulence(p, 7)));
        }
    }
private:
    Perlin noise;
    double scale;
    Style style;
    Color tint;
};

// Any function you like: color = f(u, v, p). Great for experiments.
class FunctionTexture : public Texture {
public:
    using Fn = std::function<Color(double, double, const Point3&)>;
    FunctionTexture(Fn f) : fn(f) {}
    Color value(double u, double v, const Point3& p) const override { return fn(u, v, p); }
private:
    Fn fn;
};

} // namespace pixel
