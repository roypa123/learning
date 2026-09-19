// pixel/post.h
// ------------------------------------------------------------
// Post-processing: what happens to the image AFTER rendering.
// This is where a render starts to look like a movie frame.
//   * blur (box & Gaussian)                        docs/35
//   * bloom / glow                                 docs/35
//   * color grading (exposure, contrast, saturation, temperature,
//     lift/gamma/gain)                             docs/34
//   * vignette, film grain, chromatic aberration,
//     letterbox bars                               docs/35
//   * denoising (edge-aware, guided by albedo+normal)  docs/36
//   * resizing                                     docs/35
// All functions work on LINEAR images and return new images.
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "vec3.h"
#include "color.h"
#include "image.h"
#include "random.h"
#include "noise.h"

namespace pixel {
namespace post {

// ---------------- blur -----------------------------------------------------

// 1D Gaussian weights for radius r (sigma = r/2), normalized to sum 1.
inline std::vector<double> gaussian_kernel(int radius) {
    double sigma = std::max(0.5, radius / 2.0);
    std::vector<double> k(2 * radius + 1);
    double sum = 0;
    for (int i = -radius; i <= radius; i++) {
        k[i + radius] = std::exp(-(i * i) / (2 * sigma * sigma));
        sum += k[i + radius];
    }
    for (auto& w : k) w /= sum;
    return k;
}

// Gaussian blur, done as two 1D passes (horizontal then vertical) - much faster.
inline Image gaussian_blur(const Image& src, int radius) {
    if (radius < 1) return src;
    std::vector<double> k = gaussian_kernel(radius);
    Image tmp(src.width, src.height), out(src.width, src.height);
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            Color sum(0, 0, 0);
            for (int i = -radius; i <= radius; i++) sum += k[i + radius] * src.get_clamped(x + i, y);
            tmp.at(x, y) = sum;
        }
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            Color sum(0, 0, 0);
            for (int i = -radius; i <= radius; i++) sum += k[i + radius] * tmp.get_clamped(x, y + i);
            out.at(x, y) = sum;
        }
    return out;
}

// Half-size image (average of 2x2 blocks).
inline Image downsample2(const Image& src) {
    int w = std::max(1, src.width / 2), h = std::max(1, src.height / 2);
    Image out(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            out.at(x, y) = 0.25 * (src.get_clamped(2 * x, 2 * y) + src.get_clamped(2 * x + 1, 2 * y) +
                                   src.get_clamped(2 * x, 2 * y + 1) + src.get_clamped(2 * x + 1, 2 * y + 1));
    return out;
}

// Resize with bilinear filtering (good for enlarging or mild shrinking).
inline Image resize(const Image& src, int new_w, int new_h) {
    Image out(new_w, new_h);
    for (int y = 0; y < new_h; y++)
        for (int x = 0; x < new_w; x++) {
            double u = (x + 0.5) / new_w;
            double v = 1.0 - (y + 0.5) / new_h;
            out.at(x, y) = src.sample_bilinear(u, v);
        }
    return out;
}

// ---------------- bloom ----------------------------------------------------
// Bright parts of the image "bleed" light into their surroundings, like in a
// real camera lens or the human eye.
inline Image bloom(const Image& src, double threshold = 1.0, double strength = 0.15, int levels = 5) {
    // 1. Keep only the part of each pixel brighter than the threshold.
    Image bright(src.width, src.height);
    for (size_t i = 0; i < src.data.size(); i++) {
        Color c = src.data[i];
        double l = luminance(c);
        double factor = l > threshold ? (l - threshold) / std::max(l, 1e-9) : 0.0;
        bright.data[i] = c * factor;
    }
    // 2. Blur it at several sizes and add them all up (wide + soft glow).
    Image out = src;
    Image level = bright;
    for (int n = 0; n < levels; n++) {
        level = gaussian_blur(downsample2(level), 3);
        for (int y = 0; y < out.height; y++)
            for (int x = 0; x < out.width; x++) {
                double u = (x + 0.5) / out.width, v = 1.0 - (y + 0.5) / out.height;
                out.at(x, y) += strength * level.sample_bilinear(u, v);
            }
        if (level.width < 4 || level.height < 4) break;
    }
    return out;
}

// ---------------- color grading --------------------------------------------

inline Image exposure(const Image& src, double stops) {
    Image out = src;
    double m = std::pow(2.0, stops);    // +1 stop = twice as bright
    for (auto& c : out.data) c *= m;
    return out;
}

// Warmer (positive) or cooler (negative) white balance.
inline Image temperature(const Image& src, double amount) {
    Image out = src;
    Color tint(1.0 + 0.1 * amount, 1.0, 1.0 - 0.1 * amount);
    for (auto& c : out.data) c = c * tint;
    return out;
}

// Saturation: 0 = black & white, 1 = unchanged, >1 = more colorful.
inline Image saturation(const Image& src, double amount) {
    Image out = src;
    for (auto& c : out.data) {
        double l = luminance(c);
        c = lerp(Color(l, l, l), c, amount);
        c = vmax(c, Color(0, 0, 0));
    }
    return out;
}

// Contrast around a mid grey (0.18 linear, a photographer's "middle grey").
inline Image contrast(const Image& src, double amount) {
    Image out = src;
    const double mid = 0.18;
    for (auto& c : out.data) {
        for (int k = 0; k < 3; k++) {
            double v = std::max(c[k], 1e-6);
            c[k] = mid * std::pow(v / mid, amount);
        }
    }
    return out;
}

// Lift / gamma / gain: the classic three-way color corrector in film grading.
//   lift  -> shadows, gamma -> midtones, gain -> highlights
// Works on display-referred values (after tone mapping), 0..1.
inline Color lift_gamma_gain(Color c, const Color& lift, const Color& gamma, const Color& gain) {
    for (int k = 0; k < 3; k++) {
        double v = clamp01(c[k]);
        v = gain[k] * (v + lift[k] * (1.0 - v));
        v = std::pow(std::max(v, 0.0), 1.0 / std::max(gamma[k], 1e-3));
        c[k] = v;
    }
    return c;
}

// Apply tone mapping into a new image (values mostly 0..1, still linear).
inline Image tonemap(const Image& src, ToneMapper tm) {
    Image out = src;
    for (auto& c : out.data) c = apply_tonemap(c, tm);
    return out;
}

// "Teal and orange": the famous blockbuster look. Shadows go teal, highlights orange.
// Works on tone mapped images.
inline Image teal_orange(const Image& src, double amount = 0.3) {
    Image out = src;
    Color teal(0.0, 0.5, 0.55), orange(1.0, 0.55, 0.2);
    for (auto& c : out.data) {
        double l = clamp01(luminance(c));
        Color tint = lerp(teal, orange, smoothstep(0.1, 0.7, l));
        Color graded = c * lerp(Color(1, 1, 1), tint * 1.6, amount);
        c = lerp(c, graded, amount);
    }
    return out;
}

// ---------------- lens & film effects --------------------------------------

// Darken the corners.
inline Image vignette(const Image& src, double strength = 0.5, double softness = 0.6) {
    Image out = src;
    double cx = src.width * 0.5, cy = src.height * 0.5;
    double maxd = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double dx = (x + 0.5 - cx) / maxd, dy = (y + 0.5 - cy) / maxd;
            double d = std::sqrt(dx * dx + dy * dy);
            double f = 1.0 - strength * smoothstep(1.0 - softness, 1.0, d);
            out.at(x, y) *= f;
        }
    return out;
}

// Red and blue are bent differently by cheap lenses: colored fringes near the edges.
inline Image chromatic_aberration(const Image& src, double amount_pixels = 1.5) {
    Image out(src.width, src.height);
    double cx = src.width * 0.5, cy = src.height * 0.5;
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double dx = (x + 0.5 - cx) / cx, dy = (y + 0.5 - cy) / cy;
            double ox = dx * amount_pixels, oy = dy * amount_pixels;
            auto sample = [&](double px, double py) {
                return src.sample_bilinear(px / src.width, 1.0 - py / src.height);
            };
            Color r = sample(x + 0.5 + ox, y + 0.5 + oy);
            Color g = src.at(x, y);
            Color b = sample(x + 0.5 - ox, y + 0.5 - oy);
            out.at(x, y) = Color(r.x, g.y, b.z);
        }
    return out;
}

// Film grain: a little random noise, stronger in the shadows.
inline Image film_grain(const Image& src, double strength = 0.03, uint32_t seed = 7) {
    Image out = src;
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++) {
            double n = hash2_01(x + (int)seed * 7919, y) - 0.5;   // -0.5 .. 0.5
            Color& c = out.at(x, y);
            double l = luminance(c);
            double k = strength * (1.0 - 0.5 * clamp01(l));
            c = vmax(c + Color(n, n, n) * k, Color(0, 0, 0));
        }
    return out;
}

// Black bars top and bottom for a wide "cinemascope" frame (2.39:1).
inline Image letterbox(const Image& src, double target_aspect = 2.39) {
    Image out = src;
    int visible_h = (int)(src.width / target_aspect);
    if (visible_h >= src.height) return out;
    int bar = (src.height - visible_h) / 2;
    for (int y = 0; y < src.height; y++)
        if (y < bar || y >= src.height - bar)
            for (int x = 0; x < src.width; x++) out.at(x, y) = Color(0, 0, 0);
    return out;
}

// ---------------- denoising ------------------------------------------------
// A "joint bilateral" filter: average neighbouring pixels, but only those
// that look like the SAME surface (similar normal, similar albedo, similar
// color). Edges and textures survive, noise is smoothed away.
struct DenoiseSettings {
    int radius = 6;              // neighbourhood size in pixels
    double sigma_spatial = 3.0;  // how quickly distance reduces the weight
    double sigma_color = 0.6;    // tolerance for color differences (relative)
    double sigma_normal = 0.2;   // tolerance for normal differences
    double sigma_albedo = 0.1;   // tolerance for albedo differences
    int passes = 1;
};

inline Image denoise(const Image& noisy, const Image& albedo, const Image& normal,
                     const DenoiseSettings& s = DenoiseSettings()) {
    bool has_guides = albedo.width == noisy.width && normal.width == noisy.width &&
                      albedo.height == noisy.height && normal.height == noisy.height;
    Image current = noisy;
    for (int pass = 0; pass < s.passes; pass++) {
        Image out(noisy.width, noisy.height);
        for (int y = 0; y < noisy.height; y++) {
            for (int x = 0; x < noisy.width; x++) {
                Color c0 = current.at(x, y);
                double l0 = luminance(c0);
                Color sum(0, 0, 0);
                double wsum = 0;
                for (int dy = -s.radius; dy <= s.radius; dy++) {
                    for (int dx = -s.radius; dx <= s.radius; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (!current.in_bounds(xx, yy)) continue;
                        Color c = current.at(xx, yy);
                        double d2 = (double)(dx * dx + dy * dy);
                        double w = std::exp(-d2 / (2 * s.sigma_spatial * s.sigma_spatial));
                        // color similarity (relative, so it works for dark and bright areas)
                        double dl = (luminance(c) - l0) / (0.1 + l0);
                        w *= std::exp(-dl * dl / (2 * s.sigma_color * s.sigma_color));
                        if (has_guides) {
                            double dn = (normal.at(xx, yy) - normal.at(x, y)).length_squared();
                            w *= std::exp(-dn / (2 * s.sigma_normal * s.sigma_normal));
                            double da = (albedo.at(xx, yy) - albedo.at(x, y)).length_squared();
                            w *= std::exp(-da / (2 * s.sigma_albedo * s.sigma_albedo));
                        }
                        sum += w * c;
                        wsum += w;
                    }
                }
                out.at(x, y) = wsum > 0 ? sum / wsum : c0;
            }
        }
        current = out;
    }
    return current;
}

// Normals are -1..1; this turns them into a viewable 0..1 image.
inline Image visualize_normals(const Image& normals) {
    Image out = normals;
    for (auto& n : out.data) n = 0.5 * (n + Color(1, 1, 1));
    return out;
}

// Put two images side by side (for before/after comparisons).
inline Image side_by_side(const Image& a, const Image& b, int gap = 4, const Color& gap_color = Color(1, 1, 1)) {
    int h = std::max(a.height, b.height);
    Image out(a.width + gap + b.width, h, gap_color);
    for (int y = 0; y < a.height; y++) for (int x = 0; x < a.width; x++) out.at(x, y) = a.at(x, y);
    for (int y = 0; y < b.height; y++) for (int x = 0; x < b.width; x++) out.at(a.width + gap + x, y) = b.at(x, y);
    return out;
}

// Copy a smaller image into a bigger one at (x0, y0).
inline void paste(Image& dst, const Image& src, int x0, int y0) {
    for (int y = 0; y < src.height; y++)
        for (int x = 0; x < src.width; x++)
            if (dst.in_bounds(x0 + x, y0 + y)) dst.at(x0 + x, y0 + y) = src.at(x, y);
}

} // namespace post
} // namespace pixel
