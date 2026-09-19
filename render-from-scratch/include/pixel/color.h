// pixel/color.h
// ------------------------------------------------------------
// Everything about turning "light numbers" into "screen numbers".
//   * linear  <->  sRGB (gamma)
//   * tone mapping (squeezing bright HDR values into 0..1)
//   * helpers to build colors from hex codes / HSV
// Explained in docs/04-color.md and docs/34-hdr-tonemapping.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"

namespace pixel {

// ---------- Gamma / sRGB ----------------------------------------------------
// Monitors expect "gamma encoded" values. Our renderer works with linear light.

// Exact sRGB transfer function (linear 0..1 -> encoded 0..1)
inline double linear_to_srgb(double x) {
    if (x <= 0.0) return 0.0;
    if (x <= 0.0031308) return 12.92 * x;
    return 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

// Inverse: encoded 0..1 -> linear 0..1
inline double srgb_to_linear(double x) {
    if (x <= 0.04045) return x / 12.92;
    return std::pow((x + 0.055) / 1.055, 2.4);
}

// The simple "gamma 2" approximation used in early chapters.
inline double linear_to_gamma2(double x) { return x > 0.0 ? std::sqrt(x) : 0.0; }

inline Color linear_to_srgb(const Color& c) {
    return Color(linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z));
}
inline Color srgb_to_linear(const Color& c) {
    return Color(srgb_to_linear(c.x), srgb_to_linear(c.y), srgb_to_linear(c.z));
}

// Convert a 0..1 value to a byte 0..255 (with rounding and clamping).
inline uint8_t to_byte(double x) {
    int v = (int)(clamp01(x) * 255.0 + 0.5);
    return (uint8_t)(v > 255 ? 255 : v);
}

// ---------- Handy color constructors -----------------------------------------

// From 0..255 integers, already sRGB encoded (like colors in a paint program).
// Returned color is LINEAR, ready for rendering math.
inline Color rgb255(int r, int g, int b) {
    return srgb_to_linear(Color(r / 255.0, g / 255.0, b / 255.0));
}

// From a hex code like 0xFF8800 (orange). Returned color is LINEAR.
inline Color hex_color(uint32_t hex) {
    return rgb255((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// Hue (0..360), saturation (0..1), value (0..1) -> RGB (0..1, not linearized)
inline Color hsv(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360.0;
    double c = v * s;
    double hp = h / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    Color rgb;
    if      (hp < 1) rgb = Color(c, x, 0);
    else if (hp < 2) rgb = Color(x, c, 0);
    else if (hp < 3) rgb = Color(0, c, x);
    else if (hp < 4) rgb = Color(0, x, c);
    else if (hp < 5) rgb = Color(x, 0, c);
    else             rgb = Color(c, 0, x);
    double m = v - c;
    return rgb + Color(m, m, m);
}

// ---------- Tone mapping ----------------------------------------------------
// Real light can be 1000x brighter than white paper. Screens only show 0..1.
// A tone mapper squeezes 0..infinity into 0..1 in a pleasing way.

inline Color tonemap_clamp(const Color& c) {
    return Color(clamp01(c.x), clamp01(c.y), clamp01(c.z));
}

inline Color tonemap_reinhard(const Color& c) {
    return Color(c.x / (1.0 + c.x), c.y / (1.0 + c.y), c.z / (1.0 + c.z));
}

// ACES filmic curve (Krzysztof Narkowicz's fit). The "movie look".
inline double aces_curve(double x) {
    const double a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}
inline Color tonemap_aces(const Color& c) {
    return Color(aces_curve(c.x), aces_curve(c.y), aces_curve(c.z));
}

// Uncharted 2 "filmic" curve by John Hable.
inline double hable_partial(double x) {
    const double A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
inline Color tonemap_hable(const Color& c) {
    const double exposure_bias = 2.0;
    const double white = 11.2;
    double w = 1.0 / hable_partial(white);
    return Color(hable_partial(c.x * exposure_bias) * w,
                 hable_partial(c.y * exposure_bias) * w,
                 hable_partial(c.z * exposure_bias) * w);
}

enum class ToneMapper { Clamp, Reinhard, Aces, Hable };

inline Color apply_tonemap(const Color& c, ToneMapper tm) {
    switch (tm) {
        case ToneMapper::Clamp:    return tonemap_clamp(c);
        case ToneMapper::Reinhard: return tonemap_reinhard(c);
        case ToneMapper::Aces:     return tonemap_aces(c);
        case ToneMapper::Hable:    return tonemap_hable(c);
    }
    return tonemap_clamp(c);
}

} // namespace pixel
