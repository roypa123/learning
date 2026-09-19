// pixel/random.h
// ------------------------------------------------------------
// Our own random number generator (PCG32) and helpers that
// produce random points/directions. No <random> needed.
// Explained in docs/16-random-and-antialiasing.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cmath>
#include "vec3.h"

namespace pixel {

// PCG32: a small, fast, high quality generator by Melissa O'Neill.
// State is 64 bits, output is 32 bits.
struct Pcg32 {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    Pcg32() {}
    Pcg32(uint64_t seed, uint64_t sequence = 1) { seed_with(seed, sequence); }

    void seed_with(uint64_t seed, uint64_t sequence = 1) {
        state = 0;
        inc = (sequence << 1u) | 1u;   // must be odd
        next_u32();
        state += seed;
        next_u32();
    }

    uint32_t next_u32() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    // Uniform double in [0, 1)
    double next_double() { return next_u32() * (1.0 / 4294967296.0); }
};

// Each thread gets its own generator, so threads never fight over it.
inline Pcg32& thread_rng() {
    static thread_local Pcg32 rng;
    return rng;
}

inline void seed_thread_rng(uint64_t seed) { thread_rng().seed_with(seed, seed * 2 + 1); }

// Random real number in [0,1).
inline double random_double() { return thread_rng().next_double(); }

// Random real number in [min,max).
inline double random_double(double min, double max) { return min + (max - min) * random_double(); }

// Random integer in [min,max] (both included).
inline int random_int(int min, int max) {
    int v = (int)random_double(min, max + 1.0);
    return v > max ? max : v;
}

inline Vec3 random_vec() { return Vec3(random_double(), random_double(), random_double()); }
inline Vec3 random_vec(double min, double max) {
    return Vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

// Uniform random direction (a point on the unit sphere).
inline Vec3 random_unit_vector() {
    while (true) {
        Vec3 p = random_vec(-1, 1);
        double lensq = p.length_squared();
        if (1e-160 < lensq && lensq <= 1.0) return p / std::sqrt(lensq);
    }
}

// Random direction in the hemisphere around the normal.
inline Vec3 random_on_hemisphere(const Vec3& normal) {
    Vec3 on_unit_sphere = random_unit_vector();
    return dot(on_unit_sphere, normal) > 0.0 ? on_unit_sphere : -on_unit_sphere;
}

// Random point inside a disk of radius 1 in the xy plane (for camera lenses).
inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() < 1.0) return p;
    }
}

// Cosine-weighted random direction around +z (see docs/30-importance-sampling.md).
inline Vec3 random_cosine_direction() {
    double r1 = random_double();
    double r2 = random_double();
    double phi = 2.0 * pi * r1;
    double x = std::cos(phi) * std::sqrt(r2);
    double y = std::sin(phi) * std::sqrt(r2);
    double z = std::sqrt(1.0 - r2);
    return Vec3(x, y, z);
}

} // namespace pixel
