// pixel/noise.h
// ------------------------------------------------------------
// Procedural noise: value noise, Perlin gradient noise, fBm and
// turbulence. Used for clouds, marble, terrain, film grain...
// Explained in docs/11-procedural-noise.md and docs/24-textures.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"
#include "random.h"

namespace pixel {

// Hash an integer into a pseudo-random 32 bit value (no state needed).
inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline uint32_t hash2(int x, int y) { return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u)); }
inline uint32_t hash3(int x, int y, int z) {
    return hash_u32((uint32_t)x * 73856093u ^ hash_u32((uint32_t)y * 19349663u ^ hash_u32((uint32_t)z * 83492791u)));
}
// Random double in [0,1) from integer coordinates.
inline double hash2_01(int x, int y) { return hash2(x, y) * (1.0 / 4294967296.0); }

// ---------------- 2D value noise --------------------------------------------
// Random values at integer grid points, smoothly interpolated in between.
inline double value_noise_2d(double x, double y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    double tx = x - xi, ty = y - yi;
    double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);  // smoothstep
    double v00 = hash2_01(xi, yi),     v10 = hash2_01(xi + 1, yi);
    double v01 = hash2_01(xi, yi + 1), v11 = hash2_01(xi + 1, yi + 1);
    return lerpd(lerpd(v00, v10, sx), lerpd(v01, v11, sx), sy);   // 0..1
}

// Fractal Brownian motion: add several octaves of noise, each smaller and weaker.
inline double fbm_2d(double x, double y, int octaves = 5, double lacunarity = 2.0, double gain = 0.5) {
    double sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * value_noise_2d(x, y);
        norm += amp;
        x *= lacunarity; y *= lacunarity;
        amp *= gain;
    }
    return sum / norm;   // 0..1
}

// ---------------- 3D Perlin (gradient) noise --------------------------------
class Perlin {
public:
    explicit Perlin(uint64_t seed = 42) {
        Pcg32 rng(seed);
        for (int i = 0; i < point_count; i++) {
            // random unit vector
            Vec3 v;
            do {
                v = Vec3(rng.next_double() * 2 - 1, rng.next_double() * 2 - 1, rng.next_double() * 2 - 1);
            } while (v.length_squared() > 1.0 || v.length_squared() < 1e-6);
            randvec[i] = unit_vector(v);
        }
        generate_perm(perm_x, rng);
        generate_perm(perm_y, rng);
        generate_perm(perm_z, rng);
    }

    // Smooth noise in roughly [-1, 1].
    double noise(const Point3& p) const {
        double u = p.x - std::floor(p.x);
        double v = p.y - std::floor(p.y);
        double w = p.z - std::floor(p.z);
        int i = (int)std::floor(p.x);
        int j = (int)std::floor(p.y);
        int k = (int)std::floor(p.z);
        Vec3 c[2][2][2];
        for (int di = 0; di < 2; di++)
            for (int dj = 0; dj < 2; dj++)
                for (int dk = 0; dk < 2; dk++)
                    c[di][dj][dk] = randvec[perm_x[(i + di) & 255] ^
                                            perm_y[(j + dj) & 255] ^
                                            perm_z[(k + dk) & 255]];
        return interpolate(c, u, v, w);
    }

    // Sum of absolute noise at several scales: looks like turbulent smoke.
    double turbulence(const Point3& p, int depth = 7) const {
        double accum = 0.0, weight = 1.0;
        Point3 temp = p;
        for (int i = 0; i < depth; i++) {
            accum += weight * noise(temp);
            weight *= 0.5;
            temp *= 2.0;
        }
        return std::fabs(accum);
    }

    // fBm with Perlin noise, result roughly in [-1, 1].
    double fbm(const Point3& p, int octaves = 6, double gain = 0.5) const {
        double sum = 0.0, amp = 1.0, norm = 0.0;
        Point3 q = p;
        for (int i = 0; i < octaves; i++) {
            sum += amp * noise(q);
            norm += amp;
            amp *= gain;
            q = q * 2.03;   // not exactly 2, avoids visible repetition
        }
        return sum / norm;
    }

private:
    static const int point_count = 256;
    Vec3 randvec[point_count];
    int perm_x[point_count];
    int perm_y[point_count];
    int perm_z[point_count];

    static void generate_perm(int* p, Pcg32& rng) {
        for (int i = 0; i < point_count; i++) p[i] = i;
        for (int i = point_count - 1; i > 0; i--) {     // Fisher-Yates shuffle
            int target = (int)(rng.next_double() * (i + 1));
            int tmp = p[i]; p[i] = p[target]; p[target] = tmp;
        }
    }

    static double interpolate(const Vec3 c[2][2][2], double u, double v, double w) {
        double uu = u * u * (3 - 2 * u);
        double vv = v * v * (3 - 2 * v);
        double ww = w * w * (3 - 2 * w);
        double accum = 0.0;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    Vec3 weight_v(u - i, v - j, w - k);
                    accum += (i * uu + (1 - i) * (1 - uu)) *
                             (j * vv + (1 - j) * (1 - vv)) *
                             (k * ww + (1 - k) * (1 - ww)) *
                             dot(c[i][j][k], weight_v);
                }
        return accum;
    }
};

} // namespace pixel
