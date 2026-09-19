# Appendix E — Full source code of the `pixel` library

[← Contents](README.md)

Every line of the library, in dependency order. The same files live in `include/pixel/`. The chapter
programs are printed in their chapters and live in `chapters/`.

---

## `vec3.h`

**File: `include/pixel/vec3.h`**

```cpp
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
```

---

## `random.h`

**File: `include/pixel/random.h`**

```cpp
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
```

---

## `color.h`

**File: `include/pixel/color.h`**

```cpp
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
```

---

## `png.h`

**File: `include/pixel/png.h`**

```cpp
// pixel/png.h
// ------------------------------------------------------------
// Our own PNG encoder, written from the specification.
//   * CRC-32 checksum        (for every PNG chunk)
//   * Adler-32 checksum      (for the zlib stream)
//   * DEFLATE compression    (LZ77 + fixed Huffman codes)
//   * PNG row filters        (None, Sub, Up, Average, Paeth)
// Explained in docs/06-png-part1.md and docs/07-png-part2.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace pixel {
namespace png {

// ======================= Checksums ==========================================

// CRC-32 (polynomial 0xEDB88320), as used by PNG and ZIP.
inline uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        ready = true;
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Adler-32, as used at the end of a zlib stream.
inline uint32_t adler32(const uint8_t* data, size_t len) {
    const uint32_t MOD = 65521;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

// ======================= Bit writer =========================================
// DEFLATE packs values bit by bit, starting from the LEAST significant bit.

struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buffer = 0;   // bits waiting to be written
    int count = 0;         // how many bits are waiting

    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}

    // Write 'nbits' bits of 'value', least significant bit first.
    void put_bits(uint32_t value, int nbits) {
        buffer |= value << count;
        count += nbits;
        while (count >= 8) {
            out.push_back((uint8_t)(buffer & 0xFF));
            buffer >>= 8;
            count -= 8;
        }
    }

    // Huffman codes are defined most-significant-bit first,
    // so we reverse them before writing.
    void put_huffman(uint32_t code, int length) {
        uint32_t reversed = 0;
        for (int i = 0; i < length; i++)
            reversed = (reversed << 1) | ((code >> i) & 1u);
        put_bits(reversed, length);
    }

    void flush() {
        if (count > 0) out.push_back((uint8_t)(buffer & 0xFF));
        buffer = 0;
        count = 0;
    }
};

// ======================= DEFLATE (fixed Huffman) ============================

// Write a literal byte or the end-of-block symbol (0..287) with the fixed code.
inline void write_literal_symbol(BitWriter& bw, int sym) {
    if (sym <= 143)      bw.put_huffman(0x30 + sym, 8);
    else if (sym <= 255) bw.put_huffman(0x190 + (sym - 144), 9);
    else if (sym <= 279) bw.put_huffman(sym - 256, 7);
    else                 bw.put_huffman(0xC0 + (sym - 280), 8);
}

inline void write_length(BitWriter& bw, int length) {       // 3..258
    static const int base[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                  35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
                                  3,3,3,3,4,4,4,4,5,5,5,5,0};
    int i = 28;
    while (base[i] > length) i--;
    write_literal_symbol(bw, 257 + i);
    if (extra[i] > 0) bw.put_bits((uint32_t)(length - base[i]), extra[i]);
}

inline void write_distance(BitWriter& bw, int dist) {       // 1..32768
    static const int base[30]  = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                  257,385,513,769,1025,1537,2049,3073,4097,6145,
                                  8193,12289,16385,24577};
    static const int extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
                                  7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int i = 29;
    while (base[i] > dist) i--;
    bw.put_huffman((uint32_t)i, 5);                 // fixed distance codes are 5 bits
    if (extra[i] > 0) bw.put_bits((uint32_t)(dist - base[i]), extra[i]);
}

// Compress 'data' into a zlib stream (header + deflate + adler32).
// level 0 = store only (no compression), anything else = LZ77 + fixed Huffman.
inline std::vector<uint8_t> zlib_compress(const std::vector<uint8_t>& data, int level = 1) {
    std::vector<uint8_t> out;
    out.push_back(0x78);    // CMF: deflate, 32K window
    out.push_back(0x01);    // FLG: chosen so (CMF*256 + FLG) % 31 == 0

    const size_t n = data.size();

    if (level == 0) {
        // "Stored" blocks: up to 65535 raw bytes each.
        size_t pos = 0;
        do {
            size_t len = n - pos;
            if (len > 65535) len = 65535;
            bool final = (pos + len == n);
            out.push_back(final ? 1 : 0);       // BFINAL bit + BTYPE=00, byte aligned
            out.push_back((uint8_t)(len & 0xFF));
            out.push_back((uint8_t)(len >> 8));
            out.push_back((uint8_t)(~len & 0xFF));
            out.push_back((uint8_t)((~len >> 8) & 0xFF));
            out.insert(out.end(), data.begin() + pos, data.begin() + pos + len);
            pos += len;
        } while (pos < n);
    } else {
        BitWriter bw(out);
        bw.put_bits(1, 1);   // BFINAL = 1 (this is the last block)
        bw.put_bits(1, 2);   // BTYPE  = 01 (fixed Huffman codes)

        const int WINDOW = 32768;
        const int HASH_SIZE = 1 << 15;
        const int MAX_CHAIN = 48;
        std::vector<int> head(HASH_SIZE, -1);   // most recent position for each hash
        std::vector<int> prev(n > 0 ? n : 1, -1); // previous position with same hash

        auto hash3 = [&](size_t i) -> int {
            uint32_t h = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 | data[i + 2];
            return (int)((h * 2654435761u) >> 17) & (HASH_SIZE - 1);
        };
        auto insert = [&](size_t i) {
            if (i + 2 < n) {
                int h = hash3(i);
                prev[i] = head[h];
                head[h] = (int)i;
            }
        };

        size_t i = 0;
        while (i < n) {
            int best_len = 0, best_dist = 0;
            if (i + 2 < n) {
                int candidate = head[hash3(i)];
                int chain = 0;
                while (candidate >= 0 && (int)i - candidate <= WINDOW && chain < MAX_CHAIN) {
                    int len = 0;
                    int max_len = (int)((n - i) < 258 ? (n - i) : 258);
                    while (len < max_len && data[candidate + len] == data[i + len]) len++;
                    if (len > best_len) {
                        best_len = len;
                        best_dist = (int)i - candidate;
                        if (len == max_len) break;
                    }
                    candidate = prev[candidate];
                    chain++;
                }
            }
            if (best_len >= 3) {
                write_length(bw, best_len);
                write_distance(bw, best_dist);
                for (int k = 0; k < best_len; k++) insert(i + k);
                i += best_len;
            } else {
                write_literal_symbol(bw, data[i]);
                insert(i);
                i++;
            }
        }
        write_literal_symbol(bw, 256);   // end of block
        bw.flush();
    }

    uint32_t ad = adler32(data.data(), data.size());
    out.push_back((uint8_t)(ad >> 24));
    out.push_back((uint8_t)(ad >> 16));
    out.push_back((uint8_t)(ad >> 8));
    out.push_back((uint8_t)(ad));
    return out;
}

// ======================= PNG filters ========================================

inline uint8_t paeth_predictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return (uint8_t)a;
    if (pb <= pc) return (uint8_t)b;
    return (uint8_t)c;
}

// Apply the best of the 5 PNG filters to each row.
// 'rgb' is width*height*channels bytes. Output has one extra filter byte per row.
inline std::vector<uint8_t> filter_rows(const uint8_t* rgb, int width, int height, int channels) {
    const int stride = width * channels;
    std::vector<uint8_t> out;
    out.reserve((size_t)(stride + 1) * height);
    std::vector<uint8_t> candidate(stride);
    std::vector<uint8_t> best(stride);

    for (int y = 0; y < height; y++) {
        const uint8_t* row = rgb + (size_t)y * stride;
        const uint8_t* up  = y > 0 ? rgb + (size_t)(y - 1) * stride : nullptr;
        long best_score = -1;
        int best_type = 0;

        for (int type = 0; type < 5; type++) {
            long score = 0;
            for (int i = 0; i < stride; i++) {
                int a = i >= channels ? row[i - channels] : 0;          // left
                int b = up ? up[i] : 0;                                 // above
                int c = (up && i >= channels) ? up[i - channels] : 0;   // above-left
                int predicted = 0;
                switch (type) {
                    case 0: predicted = 0; break;
                    case 1: predicted = a; break;
                    case 2: predicted = b; break;
                    case 3: predicted = (a + b) / 2; break;
                    case 4: predicted = paeth_predictor(a, b, c); break;
                }
                uint8_t v = (uint8_t)(row[i] - predicted);
                candidate[i] = v;
                score += (v < 128) ? v : 256 - v;   // small values compress better
            }
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_type = type;
                best = candidate;
            }
        }
        out.push_back((uint8_t)best_type);
        out.insert(out.end(), best.begin(), best.end());
    }
    return out;
}

// ======================= PNG file ===========================================

inline void put_u32_be(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x));
}

// A chunk is: length (4 bytes), type (4 letters), data, CRC of type+data.
inline void write_chunk(std::vector<uint8_t>& file, const char type[4], const std::vector<uint8_t>& data) {
    put_u32_be(file, (uint32_t)data.size());
    std::vector<uint8_t> type_and_data(type, type + 4);
    type_and_data.insert(type_and_data.end(), data.begin(), data.end());
    file.insert(file.end(), type_and_data.begin(), type_and_data.end());
    put_u32_be(file, crc32_update(0, type_and_data.data(), type_and_data.size()));
}

// Encode 8-bit RGB (channels=3) or RGBA (channels=4) pixels as a PNG file in memory.
inline std::vector<uint8_t> encode(const uint8_t* pixels, int width, int height,
                                   int channels = 3, int level = 1) {
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr;
    put_u32_be(ihdr, (uint32_t)width);
    put_u32_be(ihdr, (uint32_t)height);
    ihdr.push_back(8);                          // bit depth: 8 bits per channel
    ihdr.push_back(channels == 4 ? 6 : 2);      // color type: 2 = RGB, 6 = RGBA
    ihdr.push_back(0);                          // compression method (always 0)
    ihdr.push_back(0);                          // filter method (always 0)
    ihdr.push_back(0);                          // no interlace
    write_chunk(file, "IHDR", ihdr);

    std::vector<uint8_t> raw = level == 0
        ? std::vector<uint8_t>()
        : filter_rows(pixels, width, height, channels);
    if (level == 0) {
        // No filters: filter byte 0 in front of each row.
        const int stride = width * channels;
        for (int y = 0; y < height; y++) {
            raw.push_back(0);
            raw.insert(raw.end(), pixels + (size_t)y * stride, pixels + (size_t)(y + 1) * stride);
        }
    }
    write_chunk(file, "IDAT", zlib_compress(raw, level));
    write_chunk(file, "IEND", std::vector<uint8_t>());
    return file;
}

inline bool write_file(const std::string& filename, const uint8_t* pixels,
                       int width, int height, int channels = 3, int level = 1) {
    std::vector<uint8_t> bytes = encode(pixels, width, height, channels, level);
    FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return written == bytes.size();
}

} // namespace png
} // namespace pixel
```

---

## `image.h`

**File: `include/pixel/image.h`**

```cpp
// pixel/image.h
// ------------------------------------------------------------
// The Image class: a grid of linear-light colors (doubles),
// plus saving (PPM / BMP / PNG) and loading (PPM).
// Explained in docs/05-image-class.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include "vec3.h"
#include "color.h"
#include "png.h"

namespace pixel {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> data;   // row by row, top row first

    Image() {}
    Image(int w, int h, const Color& fill_color = Color(0, 0, 0))
        : width(w), height(h), data((size_t)w * h, fill_color) {}

    bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }

    Color& at(int x, int y) { return data[(size_t)y * width + x]; }
    const Color& at(int x, int y) const { return data[(size_t)y * width + x]; }

    // Like at(), but coordinates outside the image are clamped to the edge.
    Color get_clamped(int x, int y) const {
        x = x < 0 ? 0 : (x >= width ? width - 1 : x);
        y = y < 0 ? 0 : (y >= height ? height - 1 : y);
        return at(x, y);
    }

    void fill(const Color& c) { for (auto& p : data) p = c; }

    // Look up a color with texture coordinates u,v in [0,1].
    // v = 0 is the BOTTOM of the image (the usual convention for textures).
    // Bilinear filtering blends the 4 nearest pixels for a smooth result.
    Color sample_bilinear(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);   // magenta = "missing"
        u = u - std::floor(u);          // wrap around (tiling)
        v = v - std::floor(v);
        double fx = u * width - 0.5;
        double fy = (1.0 - v) * height - 0.5;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        double tx = fx - x0, ty = fy - y0;
        Color c00 = get_clamped(x0, y0),     c10 = get_clamped(x0 + 1, y0);
        Color c01 = get_clamped(x0, y0 + 1), c11 = get_clamped(x0 + 1, y0 + 1);
        return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
    }

    Color sample_nearest(double u, double v) const {
        if (width == 0 || height == 0) return Color(1, 0, 1);
        u = u - std::floor(u);
        v = v - std::floor(v);
        int x = (int)(u * width), y = (int)((1.0 - v) * height);
        return get_clamped(x, y);
    }
};

// How to turn linear HDR colors into 8-bit screen colors.
struct SaveOptions {
    double exposure = 1.0;                  // multiply light before tone mapping
    ToneMapper tonemap = ToneMapper::Clamp;
    bool srgb = true;                       // apply the sRGB gamma curve
};

inline Color encode_pixel(const Color& linear, const SaveOptions& opt) {
    Color c = linear * opt.exposure;
    // NaN protection: a NaN is never equal to itself.
    if (c.x != c.x) c.x = 0;
    if (c.y != c.y) c.y = 0;
    if (c.z != c.z) c.z = 0;
    c = apply_tonemap(c, opt.tonemap);
    if (opt.srgb) c = linear_to_srgb(c);
    return c;
}

inline std::vector<uint8_t> to_rgb8(const Image& img, const SaveOptions& opt = SaveOptions()) {
    std::vector<uint8_t> bytes((size_t)img.width * img.height * 3);
    for (size_t i = 0; i < img.data.size(); i++) {
        Color c = encode_pixel(img.data[i], opt);
        bytes[i * 3 + 0] = to_byte(c.x);
        bytes[i * 3 + 1] = to_byte(c.y);
        bytes[i * 3 + 2] = to_byte(c.z);
    }
    return bytes;
}

// Make sure the folder for 'filename' exists (e.g. "images/").
inline void ensure_parent_folder(const std::string& filename) {
    std::filesystem::path p(filename);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
}

// ---------------- PPM (the simplest image format) --------------------------

inline bool write_ppm(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::ofstream f(filename, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << img.width << ' ' << img.height << "\n255\n";
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

// Reads P3 (text) and P6 (binary) PPM files. Result is converted to LINEAR color.
inline bool read_ppm(const std::string& filename, Image& out, bool file_is_srgb = true) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;

    // Read the next header token, skipping whitespace and # comments.
    auto next_token = [&](std::string& tok) -> bool {
        tok.clear();
        char ch;
        while (f.get(ch)) {
            if (ch == '#') { std::string line; std::getline(f, line); continue; }
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                if (!tok.empty()) return true;
                continue;
            }
            tok += ch;
        }
        return !tok.empty();
    };

    std::string magic, sw, sh, smax;
    if (!next_token(magic) || !next_token(sw) || !next_token(sh) || !next_token(smax)) return false;
    if (magic != "P6" && magic != "P3") return false;
    int w = std::stoi(sw), h = std::stoi(sh), maxval = std::stoi(smax);
    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 255) return false;

    out = Image(w, h);
    for (int i = 0; i < w * h; i++) {
        int rgb[3];
        for (int k = 0; k < 3; k++) {
            if (magic == "P6") {
                char ch;
                if (!f.get(ch)) return false;
                rgb[k] = (unsigned char)ch;
            } else {
                std::string tok;
                if (!next_token(tok)) return false;
                rgb[k] = std::stoi(tok);
            }
        }
        Color c(rgb[0] / (double)maxval, rgb[1] / (double)maxval, rgb[2] / (double)maxval);
        out.data[i] = file_is_srgb ? srgb_to_linear(c) : c;
    }
    return true;
}

// ---------------- BMP (opens in every Windows program) ----------------------

// Little-endian helpers: BMP stores numbers lowest byte first.
inline void put_u16_le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF)); v.push_back((uint8_t)((x >> 8) & 0xFF));
}
inline void put_u32_le(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)((x >> (8 * i)) & 0xFF));
}

inline bool write_bmp(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> rgb = to_rgb8(img, opt);
    const int row_size = (img.width * 3 + 3) & ~3;          // rows padded to 4 bytes
    const uint32_t pixel_bytes = (uint32_t)row_size * img.height;
    std::vector<uint8_t> f;
    // --- file header (14 bytes)
    f.push_back('B'); f.push_back('M');
    put_u32_le(f, 54 + pixel_bytes);   // total file size
    put_u32_le(f, 0);                  // reserved
    put_u32_le(f, 54);                 // where the pixels start
    // --- info header (40 bytes)
    put_u32_le(f, 40);
    put_u32_le(f, (uint32_t)img.width);
    put_u32_le(f, (uint32_t)img.height); // positive height = rows stored bottom-up
    put_u16_le(f, 1);                  // planes
    put_u16_le(f, 24);                 // bits per pixel
    put_u32_le(f, 0);                  // no compression
    put_u32_le(f, pixel_bytes);
    put_u32_le(f, 2835); put_u32_le(f, 2835);   // 72 DPI
    put_u32_le(f, 0); put_u32_le(f, 0);
    // --- pixels: bottom row first, in B,G,R order
    for (int y = img.height - 1; y >= 0; y--) {
        for (int x = 0; x < img.width; x++) {
            size_t i = ((size_t)y * img.width + x) * 3;
            f.push_back(rgb[i + 2]); f.push_back(rgb[i + 1]); f.push_back(rgb[i + 0]);
        }
        for (int pad = img.width * 3; pad < row_size; pad++) f.push_back(0);
    }
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out.write((const char*)f.data(), (std::streamsize)f.size());
    return (bool)out;
}

// ---------------- PNG (uses our own encoder in png.h) -----------------------

inline bool write_png(const std::string& filename, const Image& img,
                      const SaveOptions& opt = SaveOptions()) {
    ensure_parent_folder(filename);
    std::vector<uint8_t> bytes = to_rgb8(img, opt);
    return png::write_file(filename, bytes.data(), img.width, img.height, 3, 1);
}

// Save as .png, .bmp or .ppm depending on the file extension. Prints a message.
inline bool save_image(const std::string& filename, const Image& img,
                       const SaveOptions& opt = SaveOptions()) {
    bool ok;
    std::string ext = std::filesystem::path(filename).extension().string();
    if (ext == ".ppm")      ok = write_ppm(filename, img, opt);
    else if (ext == ".bmp") ok = write_bmp(filename, img, opt);
    else                    ok = write_png(filename, img, opt);
    std::printf("%s %s (%dx%d)\n", ok ? "Saved" : "FAILED to save", filename.c_str(),
                img.width, img.height);
    return ok;
}

} // namespace pixel
```

---

## `canvas.h`

**File: `include/pixel/canvas.h`**

```cpp
// pixel/canvas.h
// ------------------------------------------------------------
// 2D drawing on an Image: pixels, lines, rectangles, circles,
// triangles, gradients, alpha blending and anti-aliasing.
// Explained in docs/08-drawing-basics.md, docs/09-circles-antialiasing.md,
//              docs/10-triangles-gradients.md
// All colors are LINEAR (use hex_color()/rgb255() to make them).
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <utility>
#include "vec3.h"
#include "image.h"

namespace pixel {

struct Canvas {
    Image& img;
    explicit Canvas(Image& target) : img(target) {}

    int width() const { return img.width; }
    int height() const { return img.height; }

    void clear(const Color& c) { img.fill(c); }

    // ---------- single pixels ----------------------------------------------

    void set_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) = c;
    }

    // Mix a color over the existing pixel. alpha = 0 (invisible) .. 1 (solid)
    void blend_pixel(int x, int y, const Color& c, double alpha) {
        if (!img.in_bounds(x, y) || alpha <= 0.0) return;
        if (alpha >= 1.0) { img.at(x, y) = c; return; }
        Color& dst = img.at(x, y);
        dst = dst * (1.0 - alpha) + c * alpha;
    }

    // Add light to a pixel (for glows and stars).
    void add_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) += c;
    }

    // ---------- rectangles -------------------------------------------------

    void fill_rect(int x0, int y0, int w, int h, const Color& c, double alpha = 1.0) {
        for (int y = y0; y < y0 + h; y++)
            for (int x = x0; x < x0 + w; x++)
                blend_pixel(x, y, c, alpha);
    }

    void draw_rect(int x0, int y0, int w, int h, const Color& c) {
        draw_line(x0, y0, x0 + w - 1, y0, c);
        draw_line(x0, y0 + h - 1, x0 + w - 1, y0 + h - 1, c);
        draw_line(x0, y0, x0, y0 + h - 1, c);
        draw_line(x0 + w - 1, y0, x0 + w - 1, y0 + h - 1, c);
    }

    // ---------- lines ------------------------------------------------------

    // Bresenham's line algorithm: only integer math, no gaps, no anti-aliasing.
    void draw_line(int x0, int y0, int x1, int y1, const Color& c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set_pixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Smooth (anti-aliased) thick line using distance to a line segment.
    void draw_line_aa(double x0, double y0, double x1, double y1, double thickness, const Color& c) {
        double r = thickness * 0.5;
        int minx = (int)std::floor(std::min(x0, x1) - r - 1), maxx = (int)std::ceil(std::max(x0, x1) + r + 1);
        int miny = (int)std::floor(std::min(y0, y1) - r - 1), maxy = (int)std::ceil(std::max(y0, y1) + r + 1);
        minx = std::max(minx, 0); miny = std::max(miny, 0);
        maxx = std::min(maxx, width() - 1); maxy = std::min(maxy, height() - 1);
        double vx = x1 - x0, vy = y1 - y0;
        double len2 = vx * vx + vy * vy;
        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                double px = x + 0.5 - x0, py = y + 0.5 - y0;
                double t = len2 > 0 ? clamp01((px * vx + py * vy) / len2) : 0.0;
                double dx = px - t * vx, dy = py - t * vy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(r - d + 0.5);   // 1 inside, fades over 1 pixel
                blend_pixel(x, y, c, coverage);
            }
        }
    }

    // ---------- circles ----------------------------------------------------

    // Hard-edged filled circle: a pixel is in or out.
    void fill_circle(int cx, int cy, int radius, const Color& c) {
        for (int y = cy - radius; y <= cy + radius; y++)
            for (int x = cx - radius; x <= cx + radius; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set_pixel(x, y, c);
            }
    }

    // Midpoint circle algorithm: outline only, integer math.
    void draw_circle(int cx, int cy, int radius, const Color& c) {
        int x = radius, y = 0, err = 1 - radius;
        while (x >= y) {
            set_pixel(cx + x, cy + y, c); set_pixel(cx + y, cy + x, c);
            set_pixel(cx - y, cy + x, c); set_pixel(cx - x, cy + y, c);
            set_pixel(cx - x, cy - y, c); set_pixel(cx - y, cy - x, c);
            set_pixel(cx + y, cy - x, c); set_pixel(cx + x, cy - y, c);
            y++;
            if (err < 0) err += 2 * y + 1;
            else { x--; err += 2 * (y - x) + 1; }
        }
    }

    // Smooth-edged circle. Edge pixels get partial coverage -> no "staircase".
    void fill_circle_aa(double cx, double cy, double radius, const Color& c, double alpha = 1.0) {
        int minx = std::max(0, (int)std::floor(cx - radius - 1));
        int maxx = std::min(width() - 1, (int)std::ceil(cx + radius + 1));
        int miny = std::max(0, (int)std::floor(cy - radius - 1));
        int maxy = std::min(height() - 1, (int)std::ceil(cy + radius + 1));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(radius - d + 0.5);
                blend_pixel(x, y, c, coverage * alpha);
            }
    }

    // A soft glowing disc: brightness falls off smoothly from the center.
    void glow(double cx, double cy, double radius, const Color& c) {
        int minx = std::max(0, (int)(cx - radius)), maxx = std::min(width() - 1, (int)(cx + radius));
        int miny = std::max(0, (int)(cy - radius)), maxy = std::min(height() - 1, (int)(cy + radius));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy) / radius;
                if (d < 1.0) {
                    double k = (1.0 - d) * (1.0 - d);
                    add_pixel(x, y, c * k);
                }
            }
    }

    // ---------- triangles --------------------------------------------------

    // "Edge function": tells on which side of the line a->b the point p lies.
    // It is also twice the signed area of triangle (a, b, p).
    static double edge(double ax, double ay, double bx, double by, double px, double py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }

    // Filled triangle with a color at each corner, smoothly blended
    // using barycentric coordinates. ss = samples per pixel side (anti-aliasing).
    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c0, const Color& c1, const Color& c2, int ss = 4) {
        double area = edge(x0, y0, x1, y1, x2, y2);
        if (std::fabs(area) < 1e-12) return;   // degenerate (flat) triangle
        int minx = std::max(0, (int)std::floor(std::min({x0, x1, x2})));
        int maxx = std::min(width() - 1, (int)std::ceil(std::max({x0, x1, x2})));
        int miny = std::max(0, (int)std::floor(std::min({y0, y1, y2})));
        int maxy = std::min(height() - 1, (int)std::ceil(std::max({y0, y1, y2})));

        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                int inside = 0;
                Color sum(0, 0, 0);
                for (int sy = 0; sy < ss; sy++) {
                    for (int sx = 0; sx < ss; sx++) {
                        double px = x + (sx + 0.5) / ss;
                        double py = y + (sy + 0.5) / ss;
                        double w0 = edge(x1, y1, x2, y2, px, py) / area;
                        double w1 = edge(x2, y2, x0, y0, px, py) / area;
                        double w2 = edge(x0, y0, x1, y1, px, py) / area;
                        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                            inside++;
                            sum += w0 * c0 + w1 * c1 + w2 * c2;
                        }
                    }
                }
                if (inside > 0) blend_pixel(x, y, sum / inside, (double)inside / (ss * ss));
            }
        }
    }

    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c, int ss = 4) {
        fill_triangle(x0, y0, x1, y1, x2, y2, c, c, c, ss);
    }

    // Filled polygon (any shape, given as a list of points) using the
    // even-odd scanline rule. Points are (x,y) pairs.
    void fill_polygon(const std::vector<std::pair<double, double>>& pts, const Color& c, int ss = 4) {
        if (pts.size() < 3) return;
        double miny = pts[0].second, maxy = pts[0].second;
        for (auto& p : pts) { miny = std::min(miny, p.second); maxy = std::max(maxy, p.second); }
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(height() - 1, (int)std::ceil(maxy));
        std::vector<double> coverage(width());
        for (int y = y0; y <= y1; y++) {
            std::fill(coverage.begin(), coverage.end(), 0.0);
            for (int s = 0; s < ss; s++) {
                double sy = y + (s + 0.5) / ss;
                std::vector<double> xs;
                for (size_t i = 0; i < pts.size(); i++) {
                    auto a = pts[i], b = pts[(i + 1) % pts.size()];
                    if ((a.second <= sy && b.second > sy) || (b.second <= sy && a.second > sy)) {
                        double t = (sy - a.second) / (b.second - a.second);
                        xs.push_back(a.first + t * (b.first - a.first));
                    }
                }
                std::sort(xs.begin(), xs.end());
                for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                    // add horizontal coverage of span [xs[i], xs[i+1]]
                    double xa = xs[i], xb = xs[i + 1];
                    int ia = std::max(0, (int)std::floor(xa)), ib = std::min(width() - 1, (int)std::floor(xb));
                    for (int x = ia; x <= ib; x++) {
                        double left = std::max(xa, (double)x), right = std::min(xb, (double)x + 1);
                        if (right > left) coverage[x] += (right - left) / ss;
                    }
                }
            }
            for (int x = 0; x < width(); x++)
                if (coverage[x] > 0) blend_pixel(x, y, c, std::min(1.0, coverage[x]));
        }
    }

    // ---------- gradients --------------------------------------------------

    // Vertical gradient from 'top' color to 'bottom' color over rows [y0, y1).
    void vertical_gradient(int y0, int y1, const Color& top, const Color& bottom) {
        for (int y = std::max(0, y0); y < std::min(height(), y1); y++) {
            double t = (y1 - y0) > 1 ? (double)(y - y0) / (y1 - y0 - 1) : 0.0;
            Color c = lerp(top, bottom, t);
            for (int x = 0; x < width(); x++) img.at(x, y) = c;
        }
    }

    // Radial gradient: 'inner' at the center fading to 'outer' at 'radius'.
    void radial_gradient(double cx, double cy, double radius, const Color& inner, const Color& outer) {
        for (int y = 0; y < height(); y++)
            for (int x = 0; x < width(); x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double t = clamp01(std::sqrt(dx * dx + dy * dy) / radius);
                img.at(x, y) = lerp(inner, outer, t);
            }
    }
};

} // namespace pixel
```

---

## `noise.h`

**File: `include/pixel/noise.h`**

```cpp
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
```

---

## `ray.h`

**File: `include/pixel/ray.h`**

```cpp
// pixel/ray.h
// ------------------------------------------------------------
// Ray: a half-line  P(t) = origin + t * direction
// Interval: a range of numbers [min, max]
// Explained in docs/13-rays-and-camera.md and docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"

namespace pixel {

class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction, double time = 0.0)
        : orig(origin), dir(direction), tm(time) {}

    const Point3& origin() const { return orig; }
    const Vec3& direction() const { return dir; }
    double time() const { return tm; }   // moment inside the camera shutter (motion blur)

    Point3 at(double t) const { return orig + t * dir; }

private:
    Point3 orig;
    Vec3 dir;
    double tm = 0.0;
};

struct Interval {
    double min = +infinity;   // default interval is empty
    double max = -infinity;

    Interval() {}
    Interval(double mn, double mx) : min(mn), max(mx) {}
    // The smallest interval containing both a and b.
    Interval(const Interval& a, const Interval& b)
        : min(a.min <= b.min ? a.min : b.min), max(a.max >= b.max ? a.max : b.max) {}

    double size() const { return max - min; }
    bool contains(double x) const { return min <= x && x <= max; }
    bool surrounds(double x) const { return min < x && x < max; }
    double clamp(double x) const { return x < min ? min : (x > max ? max : x); }
    Interval expand(double delta) const { double p = delta / 2; return Interval(min - p, max + p); }

    static Interval empty() { return Interval(+infinity, -infinity); }
    static Interval universe() { return Interval(-infinity, +infinity); }
};

inline Interval operator+(const Interval& ival, double displacement) {
    return Interval(ival.min + displacement, ival.max + displacement);
}

} // namespace pixel
```

---

## `aabb.h`

**File: `include/pixel/aabb.h`**

```cpp
// pixel/aabb.h
// ------------------------------------------------------------
// AABB = Axis-Aligned Bounding Box: the simplest box around an object.
// Testing a ray against a box is very cheap, so we test boxes first.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"
#include "ray.h"

namespace pixel {

class AABB {
public:
    Interval x, y, z;

    AABB() {}   // empty box
    AABB(const Interval& ix, const Interval& iy, const Interval& iz) : x(ix), y(iy), z(iz) {
        pad_to_minimums();
    }
    // Box spanned by two corner points (in any order).
    AABB(const Point3& a, const Point3& b) {
        x = a.x <= b.x ? Interval(a.x, b.x) : Interval(b.x, a.x);
        y = a.y <= b.y ? Interval(a.y, b.y) : Interval(b.y, a.y);
        z = a.z <= b.z ? Interval(a.z, b.z) : Interval(b.z, a.z);
        pad_to_minimums();
    }
    // Box around two boxes.
    AABB(const AABB& a, const AABB& b) : x(a.x, b.x), y(a.y, b.y), z(a.z, b.z) {}

    const Interval& axis_interval(int n) const { return n == 1 ? y : (n == 2 ? z : x); }

    // The "slab method": intersect the ray with 3 pairs of parallel planes.
    bool hit(const Ray& r, Interval ray_t) const {
        const Point3& o = r.origin();
        const Vec3& d = r.direction();
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / d[axis];
            double t0 = (ax.min - o[axis]) * adinv;
            double t1 = (ax.max - o[axis]) * adinv;
            if (t0 > t1) { double tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > ray_t.min) ray_t.min = t0;
            if (t1 < ray_t.max) ray_t.max = t1;
            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }

    int longest_axis() const {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }

    Point3 centroid() const { return Point3((x.min + x.max) * 0.5, (y.min + y.max) * 0.5, (z.min + z.max) * 0.5); }

    double surface_area() const {
        double a = x.size(), b = y.size(), c = z.size();
        return 2.0 * (a * b + b * c + c * a);
    }

    static AABB empty() { return AABB(Interval::empty(), Interval::empty(), Interval::empty()); }

private:
    // A flat box (e.g. around a flat quad) has zero thickness, which breaks
    // the math. Give every side at least a tiny thickness.
    void pad_to_minimums() {
        const double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

inline AABB operator+(const AABB& b, const Vec3& offset) {
    return AABB(b.x + offset.x, b.y + offset.y, b.z + offset.z);
}

} // namespace pixel
```

---

## `hittable.h`

**File: `include/pixel/hittable.h`**

```cpp
// pixel/hittable.h
// ------------------------------------------------------------
// "Hittable" = anything a ray can hit (sphere, quad, triangle,
// a whole list of objects, a BVH tree...).
// Explained in docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include <memory>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "aabb.h"
#include "random.h"

namespace pixel {

class Material;   // defined in material.h

// Everything we want to know about the place where a ray hit something.
struct HitRecord {
    Point3 p;                          // the hit point
    Vec3 normal;                       // unit normal, always facing AGAINST the ray
    const Material* mat = nullptr;     // what the surface is made of
    double t = 0.0;                    // ray parameter: p = origin + t*direction
    double u = 0.0, v = 0.0;           // texture coordinates
    bool front_face = true;            // did we hit the outside of the surface?

    // outward_normal must be unit length.
    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Hittable {
public:
    virtual ~Hittable() = default;

    // Does the ray hit this object with t inside ray_t? If yes, fill rec.
    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

    virtual AABB bounding_box() const = 0;

    // --- Used for light sampling (docs/31-light-sampling.md) ---
    // Probability density of choosing 'direction' from 'origin' towards this object.
    virtual double pdf_value(const Point3& /*origin*/, const Vec3& /*direction*/) const { return 0.0; }
    // A random direction from 'origin' towards this object.
    virtual Vec3 random(const Point3& /*origin*/) const { return Vec3(1, 0, 0); }
};

// A list of hittables that behaves like one hittable.
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;

    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }

    void clear() { objects.clear(); bbox = AABB(); }

    void add(std::shared_ptr<Hittable> object) {
        bbox = objects.empty() ? object->bounding_box() : AABB(bbox, object->bounding_box());
        objects.push_back(object);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord temp;
        bool hit_anything = false;
        double closest = ray_t.max;
        for (const auto& object : objects) {
            if (object->hit(r, Interval(ray_t.min, closest), temp)) {
                hit_anything = true;
                closest = temp.t;       // only accept closer hits from now on
                rec = temp;
            }
        }
        return hit_anything;
    }

    AABB bounding_box() const override { return bbox; }

    // Light sampling over a list: pick one object at random.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        if (objects.empty()) return 0.0;
        double weight = 1.0 / objects.size();
        double sum = 0.0;
        for (const auto& object : objects) sum += weight * object->pdf_value(origin, direction);
        return sum;
    }

    Vec3 random(const Point3& origin) const override {
        if (objects.empty()) return Vec3(1, 0, 0);
        int i = (int)(random_double() * objects.size());
        if (i >= (int)objects.size()) i = (int)objects.size() - 1;
        return objects[i]->random(origin);
    }

private:
    AABB bbox;
};

} // namespace pixel
```

---

## `pdf.h`

**File: `include/pixel/pdf.h`**

```cpp
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
```

---

## `texture.h`

**File: `include/pixel/texture.h`**

```cpp
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
```

---

## `material.h`

**File: `include/pixel/material.h`**

```cpp
// pixel/material.h
// ------------------------------------------------------------
// Materials decide what happens to light when it touches a surface:
// absorbed? bounced? bent? emitted?
//   Lambertian    - matte, like chalk or paper       (docs/17)
//   Metal         - mirror / brushed metal           (docs/18)
//   Dielectric    - glass, water, diamond            (docs/19)
//   DiffuseLight  - a glowing surface (a lamp)       (docs/26)
//   Isotropic     - scattering inside fog / smoke    (docs/28)
//   RoughMetal    - physically based GGX metal       (docs/32)
//   Plastic       - colored base + shiny clear coat  (docs/32)
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "random.h"
#include "hittable.h"
#include "texture.h"
#include "pdf.h"

namespace pixel {

// What a material tells the renderer after a hit.
struct ScatterRecord {
    Color attenuation;               // how much of each color survives the bounce
    std::shared_ptr<PDF> pdf_ptr;    // for diffuse-like surfaces: how to pick directions
    bool skip_pdf = false;           // for mirror-like surfaces: direction is fixed...
    Ray skip_pdf_ray;                // ...and this is it
};

class Material {
public:
    virtual ~Material() = default;

    // Light produced by the surface itself (only lights return non-zero).
    virtual Color emitted(const Ray& /*r_in*/, const HitRecord& /*rec*/,
                          double /*u*/, double /*v*/, const Point3& /*p*/) const {
        return Color(0, 0, 0);
    }

    // Returns false if the ray is absorbed.
    virtual bool scatter(const Ray& /*r_in*/, const HitRecord& /*rec*/, ScatterRecord& /*srec*/) const {
        return false;
    }

    // The material's own density for bouncing towards 'scattered'.
    virtual double scattering_pdf(const Ray& /*r_in*/, const HitRecord& /*rec*/, const Ray& /*scattered*/) const {
        return 0.0;
    }

    // Surface color used by the denoiser (docs/36-denoising.md).
    virtual Color aov_albedo(const HitRecord& /*rec*/) const { return Color(1, 1, 1); }
};

// Schlick's approximation of Fresnel reflectance.
inline double schlick(double cosine, double f0) {
    double m = 1.0 - cosine;
    return f0 + (1.0 - f0) * m * m * m * m * m;
}
inline Color schlick(double cosine, const Color& f0) {
    double m = 1.0 - cosine;
    double m5 = m * m * m * m * m;
    return f0 + (Color(1, 1, 1) - f0) * m5;
}

// ---------------------------------------------------------------------------
class Lambertian : public Material {
public:
    Lambertian(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Lambertian(std::shared_ptr<Texture> tex) : tex(tex) {}

    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
};

// ---------------------------------------------------------------------------
class Metal : public Material {
public:
    Metal(const Color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        reflected = unit_vector(reflected) + fuzz * random_unit_vector();
        srec.attenuation = albedo;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, reflected, r_in.time());
        return dot(reflected, rec.normal) > 0;   // fuzz pushed it below the surface? absorb
    }

    Color aov_albedo(const HitRecord&) const override { return albedo; }

private:
    Color albedo;
    double fuzz;
};

// ---------------------------------------------------------------------------
class Dielectric : public Material {
public:
    // ior = index of refraction: air 1.0, water 1.33, glass 1.5, diamond 2.4
    Dielectric(double ior, const Color& tint = Color(1, 1, 1)) : ior(ior), tint(tint) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tint;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        double ri = rec.front_face ? (1.0 / ior) : ior;

        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;     // total internal reflection
        double r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
        Vec3 direction;
        if (cannot_refract || schlick(cos_theta, r0) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        srec.skip_pdf_ray = Ray(rec.p, direction, r_in.time());
        return true;
    }

    Color aov_albedo(const HitRecord&) const override { return tint; }

private:
    double ior;
    Color tint;
};

// ---------------------------------------------------------------------------
class DiffuseLight : public Material {
public:
    DiffuseLight(std::shared_ptr<Texture> tex, bool two_sided = false) : tex(tex), two_sided(two_sided) {}
    DiffuseLight(const Color& emit, bool two_sided = false)
        : tex(std::make_shared<SolidColor>(emit)), two_sided(two_sided) {}

    Color emitted(const Ray&, const HitRecord& rec, double u, double v, const Point3& p) const override {
        if (!rec.front_face && !two_sided) return Color(0, 0, 0);   // lights shine one way
        return tex->value(u, v, p);
    }

private:
    std::shared_ptr<Texture> tex;
    bool two_sided;
};

// ---------------------------------------------------------------------------
class Isotropic : public Material {
public:
    Isotropic(const Color& albedo) : tex(std::make_shared<SolidColor>(albedo)) {}
    Isotropic(std::shared_ptr<Texture> tex) : tex(tex) {}

    bool scatter(const Ray&, const HitRecord& rec, ScatterRecord& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = std::make_shared<SpherePDF>();
        srec.skip_pdf = false;
        return true;
    }
    double scattering_pdf(const Ray&, const HitRecord&, const Ray&) const override {
        return 1.0 / (4.0 * pi);
    }
    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
};

// ===================== Microfacet (GGX) helpers ============================
// Explained in docs/32-microfacet-materials.md. All vectors are in the local
// frame where the surface normal is +z.
namespace ggx {

// Smith Lambda function for GGX.
inline double lambda(const Vec3& v, double alpha) {
    if (v.z <= 0.0) return 1e10;
    double tan2 = (v.x * v.x + v.y * v.y) / (v.z * v.z);
    return (-1.0 + std::sqrt(1.0 + alpha * alpha * tan2)) * 0.5;
}
inline double G1(const Vec3& v, double alpha) { return 1.0 / (1.0 + lambda(v, alpha)); }
inline double G2(const Vec3& wi, const Vec3& wo, double alpha) {
    return 1.0 / (1.0 + lambda(wi, alpha) + lambda(wo, alpha));
}

// Sample a visible micro-normal (Heitz 2018, "Sampling the GGX Distribution
// of Visible Normals"). ve = direction towards the viewer, local frame.
inline Vec3 sample_vndf(const Vec3& ve, double alpha, double u1, double u2) {
    Vec3 vh = unit_vector(Vec3(alpha * ve.x, alpha * ve.y, ve.z));
    double lensq = vh.x * vh.x + vh.y * vh.y;
    Vec3 t1 = lensq > 0 ? Vec3(-vh.y, vh.x, 0) / std::sqrt(lensq) : Vec3(1, 0, 0);
    Vec3 t2 = cross(vh, t1);
    double r = std::sqrt(u1);
    double phi = 2.0 * pi * u2;
    double p1 = r * std::cos(phi);
    double p2 = r * std::sin(phi);
    double s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * std::sqrt(std::fmax(0.0, 1.0 - p1 * p1)) + s * p2;
    Vec3 nh = p1 * t1 + p2 * t2 + std::sqrt(std::fmax(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return unit_vector(Vec3(alpha * nh.x, alpha * nh.y, std::fmax(1e-6, nh.z)));
}

// Sample a reflected direction off a rough surface. Returns false if the
// sample goes below the surface. 'weight' receives G2/G1 (Fresnel not included).
inline bool sample_reflection(const Vec3& wi_local, double alpha, Vec3& wo_local, Vec3& m_local, double& weight) {
    m_local = sample_vndf(wi_local, alpha, random_double(), random_double());
    wo_local = 2.0 * dot(wi_local, m_local) * m_local - wi_local;
    if (wo_local.z <= 0.0) return false;
    weight = G2(wi_local, wo_local, alpha) / G1(wi_local, alpha);
    return true;
}

} // namespace ggx

// ---------------------------------------------------------------------------
// Physically based metal: gold, copper, aluminium, chrome...
// roughness 0 = perfect mirror, 1 = very dull.
class RoughMetal : public Material {
public:
    RoughMetal(const Color& f0, double roughness)
        : f0(f0), alpha(std::fmax(0.001, roughness * roughness)) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        ONB frame(rec.normal);
        Vec3 wi = frame.to_local(-unit_vector(r_in.direction()));   // towards the viewer
        if (wi.z <= 0.0) wi.z = 1e-4;
        wi = unit_vector(wi);
        Vec3 wo, m;
        double w;
        if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;
        srec.attenuation = schlick(std::fmax(0.0, dot(wi, m)), f0) * w;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
        return true;
    }

    Color aov_albedo(const HitRecord&) const override { return f0; }

private:
    Color f0;
    double alpha;
};

// ---------------------------------------------------------------------------
// Plastic / painted / varnished surfaces: a diffuse colored base under a
// clear glossy coat. We randomly pick one of the two layers per bounce.
class Plastic : public Material {
public:
    Plastic(std::shared_ptr<Texture> tex, double roughness, double ior = 1.5)
        : tex(tex), alpha(std::fmax(0.001, roughness * roughness)) {
        double r0 = (1.0 - ior) / (1.0 + ior);
        f0 = r0 * r0;   // about 0.04 for ior 1.5
    }
    Plastic(const Color& albedo, double roughness, double ior = 1.5)
        : Plastic(std::make_shared<SolidColor>(albedo), roughness, ior) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, ScatterRecord& srec) const override {
        Vec3 unit_in = unit_vector(r_in.direction());
        double cos_i = std::fmax(0.0, dot(-unit_in, rec.normal));
        double F = schlick(cos_i, f0);             // how much the coat reflects
        double p_spec = std::fmax(F, 0.2);         // choose the coat more often (less noise)

        if (random_double() < p_spec) {
            ONB frame(rec.normal);
            Vec3 wi = unit_vector(frame.to_local(-unit_in));
            if (wi.z <= 0.0) return false;
            Vec3 wo, m;
            double w;
            if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;
            double Fm = schlick(std::fmax(0.0, dot(wi, m)), f0);
            srec.attenuation = Color(1, 1, 1) * (Fm * w / p_spec);
            srec.pdf_ptr = nullptr;
            srec.skip_pdf = true;
            srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
        } else {
            srec.attenuation = tex->value(rec.u, rec.v, rec.p) * ((1.0 - F) / (1.0 - p_spec));
            srec.pdf_ptr = std::make_shared<CosinePDF>(rec.normal);
            srec.skip_pdf = false;
        }
        return true;
    }

    double scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const override {
        double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    Color aov_albedo(const HitRecord& rec) const override { return tex->value(rec.u, rec.v, rec.p); }

private:
    std::shared_ptr<Texture> tex;
    double alpha;
    double f0;
};

} // namespace pixel
```

---

## `sphere.h`

**File: `include/pixel/sphere.h`**

```cpp
// pixel/sphere.h
// ------------------------------------------------------------
// The sphere: the "hello world" of ray tracing.
// Supports motion (for motion blur) and light sampling.
// Explained in docs/14-hitting-a-sphere.md, docs/22-motion-blur.md,
//              docs/31-light-sampling.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "hittable.h"
#include "pdf.h"

namespace pixel {

class Sphere : public Hittable {
public:
    // A sphere that stays still.
    Sphere(const Point3& center, double radius, std::shared_ptr<Material> mat)
        : center(center, Vec3(0, 0, 0)), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        bbox = AABB(center - rvec, center + rvec);
    }

    // A sphere that moves from center1 (time 0) to center2 (time 1).
    Sphere(const Point3& center1, const Point3& center2, double radius, std::shared_ptr<Material> mat)
        : center(center1, center2 - center1), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        AABB box1(center.at(0) - rvec, center.at(0) + rvec);
        AABB box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = AABB(box1, box2);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Point3 current_center = center.at(r.time());
        Vec3 oc = current_center - r.origin();
        double a = r.direction().length_squared();
        double h = dot(r.direction(), oc);
        double c = oc.length_squared() - radius * radius;

        double discriminant = h * h - a * c;
        if (discriminant < 0) return false;          // the ray misses
        double sqrtd = std::sqrt(discriminant);

        // Find the nearest root inside the acceptable range.
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat.get();
        return true;
    }

    AABB bounding_box() const override { return bbox; }

    // p: a point on the unit sphere centered at the origin.
    // u: angle around the Y axis from X=-1, in [0,1]
    // v: angle from Y=-1 to Y=+1, in [0,1]
    static void get_sphere_uv(const Point3& p, double& u, double& v) {
        double theta = std::acos(clampd(-p.y, -1.0, 1.0));
        double phi = std::atan2(-p.z, p.x) + pi;
        u = phi / (2 * pi);
        v = theta / pi;
    }

    // ----- light sampling: we only sample the cone of directions that hit us.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double dist_squared = (center.at(0) - origin).length_squared();
        if (dist_squared <= radius * radius) return 0.0;
        double cos_theta_max = std::sqrt(1.0 - radius * radius / dist_squared);
        double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);
        return 1.0 / solid_angle;
    }

    Vec3 random(const Point3& origin) const override {
        Vec3 direction = center.at(0) - origin;
        double distance_squared = direction.length_squared();
        if (distance_squared <= radius * radius) return random_unit_vector();
        ONB uvw(direction);
        return uvw.transform(random_to_sphere(radius, distance_squared));
    }

private:
    Ray center;     // center.at(time) = position at that time
    double radius;
    std::shared_ptr<Material> mat;
    AABB bbox;

    static Vec3 random_to_sphere(double radius, double distance_squared) {
        double r1 = random_double();
        double r2 = random_double();
        double z = 1 + r2 * (std::sqrt(1 - radius * radius / distance_squared) - 1);
        double phi = 2 * pi * r1;
        double x = std::cos(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        double y = std::sin(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        return Vec3(x, y, z);
    }
};

} // namespace pixel
```

---

## `quad.h`

**File: `include/pixel/quad.h`**

```cpp
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
```

---

## `bvh.h`

**File: `include/pixel/bvh.h`**

```cpp
// pixel/bvh.h
// ------------------------------------------------------------
// BVH = Bounding Volume Hierarchy. A tree of boxes-inside-boxes.
// Instead of testing the ray against ALL objects (slow), we test
// big boxes first and skip everything inside boxes we miss.
// 1,000,000 objects need only ~20 box tests per ray.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include "hittable.h"
#include "aabb.h"

namespace pixel {

class BVHNode : public Hittable {
public:
    BVHNode(HittableList list) : BVHNode(list.objects, 0, list.objects.size()) {}

    BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
        // 1. Box around everything in this node.
        bbox = AABB::empty();
        for (size_t i = start; i < end; i++) bbox = AABB(bbox, objects[i]->bounding_box());

        // 2. Split along the longest side of that box.
        int axis = bbox.longest_axis();
        size_t span = end - start;

        if (span == 1) {
            left = right = objects[start];
        } else if (span == 2) {
            left = objects[start];
            right = objects[start + 1];
        } else {
            // Sort by the center of each object's box along the axis, split in half.
            auto comparator = [axis](const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b) {
                return a->bounding_box().centroid()[axis] < b->bounding_box().centroid()[axis];
            };
            std::sort(objects.begin() + start, objects.begin() + end, comparator);
            size_t mid = start + span / 2;
            left = std::make_shared<BVHNode>(objects, start, mid);
            right = std::make_shared<BVHNode>(objects, mid, end);
        }
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;           // missed the whole box: skip all
        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);
        return hit_left || hit_right;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> left;
    std::shared_ptr<Hittable> right;
    AABB bbox;
};

} // namespace pixel
```

---

## `triangle.h`

**File: `include/pixel/triangle.h`**

```cpp
// pixel/triangle.h
// ------------------------------------------------------------
// Triangles and triangle meshes (+ a tiny OBJ file loader/writer).
// Every 3D model in films and games is made of triangles.
// Explained in docs/25-quads-triangles-meshes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "vec3.h"
#include "hittable.h"
#include "bvh.h"
#include "random.h"

namespace pixel {

// Texture coordinate pair.
struct UV { double u = 0, v = 0; };

class Triangle : public Hittable {
public:
    // Flat triangle.
    Triangle(const Point3& a, const Point3& b, const Point3& c, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), mat(mat) { setup(); }

    // Smooth triangle: one normal (and uv) per corner, blended across the face.
    Triangle(const Point3& a, const Point3& b, const Point3& c,
             const Vec3& na, const Vec3& nb, const Vec3& nc,
             const UV& ta, const UV& tb, const UV& tc,
             bool use_normals, bool use_uvs, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), n0(na), n1(nb), n2(nc), t0(ta), t1(tb), t2(tc),
          has_normals(use_normals), has_uvs(use_uvs), mat(mat) { setup(); }

    // Moller-Trumbore ray/triangle intersection.
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Vec3 pvec = cross(r.direction(), e2);
        double det = dot(e1, pvec);
        if (std::fabs(det) < 1e-12) return false;        // parallel
        double inv_det = 1.0 / det;

        Vec3 tvec = r.origin() - v0;
        double b1 = dot(tvec, pvec) * inv_det;           // barycentric weight of v1
        if (b1 < 0.0 || b1 > 1.0) return false;

        Vec3 qvec = cross(tvec, e1);
        double b2 = dot(r.direction(), qvec) * inv_det;  // barycentric weight of v2
        if (b2 < 0.0 || b1 + b2 > 1.0) return false;

        double t = dot(e2, qvec) * inv_det;
        if (!ray_t.surrounds(t)) return false;

        double b0 = 1.0 - b1 - b2;
        rec.t = t;
        rec.p = r.at(t);
        rec.mat = mat.get();
        rec.set_face_normal(r, geo_normal);
        if (has_normals) {
            Vec3 sn = unit_vector(b0 * n0 + b1 * n1 + b2 * n2);
            if (dot(sn, rec.normal) < 0) sn = -sn;       // keep it on the same side
            rec.normal = sn;
        }
        if (has_uvs) {
            rec.u = b0 * t0.u + b1 * t1.u + b2 * t2.u;
            rec.v = b0 * t0.v + b1 * t1.v + b2 * t2.v;
        } else {
            rec.u = b1;
            rec.v = b2;
        }
        return true;
    }

    AABB bounding_box() const override { return bbox; }

    // Light sampling (a triangle can be an area light too).
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double distance_squared = rec.t * rec.t * direction.length_squared();
        double cosine = std::fabs(dot(direction, geo_normal) / direction.length());
        if (cosine < 1e-8) return 0.0;
        return distance_squared / (cosine * area);
    }

    Vec3 random(const Point3& origin) const override {
        double r1 = random_double(), r2 = random_double();
        double s = std::sqrt(r1);
        Point3 p = (1 - s) * v0 + s * (1 - r2) * v1 + s * r2 * v2;   // uniform on triangle
        return p - origin;
    }

private:
    Point3 v0, v1, v2;
    Vec3 n0, n1, n2;
    UV t0, t1, t2;
    bool has_normals = false, has_uvs = false;
    std::shared_ptr<Material> mat;
    Vec3 e1, e2, geo_normal;
    double area = 0;
    AABB bbox;

    void setup() {
        e1 = v1 - v0;
        e2 = v2 - v0;
        Vec3 n = cross(e1, e2);
        area = 0.5 * n.length();
        geo_normal = area > 0 ? unit_vector(n) : Vec3(0, 1, 0);
        bbox = AABB(AABB(v0, v1), AABB(v2, v2));
    }
};

// ---------------------------------------------------------------------------
// A triangle mesh stored as arrays, like in a real 3D program.
struct Mesh {
    std::vector<Point3> positions;
    std::vector<Vec3> normals;       // optional, one per position
    std::vector<UV> uvs;             // optional, one per position
    std::vector<int> indices;        // 3 per triangle

    size_t triangle_count() const { return indices.size() / 3; }

    // Compute smooth normals by averaging the face normals around each vertex.
    void compute_smooth_normals() {
        normals.assign(positions.size(), Vec3(0, 0, 0));
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            Vec3 n = cross(positions[b] - positions[a], positions[c] - positions[a]); // area weighted
            normals[a] += n; normals[b] += n; normals[c] += n;
        }
        for (auto& n : normals) n = n.length() > 0 ? unit_vector(n) : Vec3(0, 1, 0);
    }

    // Move/scale all vertices: p' = p * scale + offset
    void transform(double scale, const Vec3& offset) {
        for (auto& p : positions) p = p * scale + offset;
    }

    // Build one Hittable (a BVH over all triangles) from this mesh.
    std::shared_ptr<Hittable> build(std::shared_ptr<Material> mat) const {
        HittableList list;
        bool use_n = normals.size() == positions.size();
        bool use_uv = uvs.size() == positions.size();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            list.add(std::make_shared<Triangle>(
                positions[a], positions[b], positions[c],
                use_n ? normals[a] : Vec3(), use_n ? normals[b] : Vec3(), use_n ? normals[c] : Vec3(),
                use_uv ? uvs[a] : UV(), use_uv ? uvs[b] : UV(), use_uv ? uvs[c] : UV(),
                use_n, use_uv, mat));
        }
        if (list.objects.empty()) return std::make_shared<HittableList>();
        return std::make_shared<BVHNode>(list);
    }
};

// ---------------------------------------------------------------------------
// Minimal Wavefront .OBJ reader. Supports: v, vt, vn, f (any polygon,
// "v", "v/vt", "v//vn", "v/vt/vn", negative indices). Everything else is ignored.
// Corners are "unwelded": each face corner gets its own vertex (simple & correct).
inline bool load_obj(const std::string& filename, Mesh& mesh) {
    std::ifstream f(filename);
    if (!f) { std::printf("Could not open OBJ '%s'\n", filename.c_str()); return false; }
    std::vector<Point3> P;
    std::vector<Vec3> N;
    std::vector<UV> T;
    mesh = Mesh();
    bool any_normals = false, any_uvs = false;
    std::vector<Vec3> out_n;
    std::vector<UV> out_t;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") { Point3 p; ss >> p.x >> p.y >> p.z; P.push_back(p); }
        else if (tag == "vn") { Vec3 n; ss >> n.x >> n.y >> n.z; N.push_back(n); }
        else if (tag == "vt") { UV t; ss >> t.u >> t.v; T.push_back(t); }
        else if (tag == "f") {
            std::vector<int> face;   // indices into mesh.positions
            std::string corner;
            while (ss >> corner) {
                int idx[3] = {0, 0, 0};   // v, vt, vn (0 = missing)
                int part = 0;
                std::string num;
                for (size_t i = 0; i <= corner.size(); i++) {
                    if (i == corner.size() || corner[i] == '/') {
                        if (!num.empty() && part < 3) idx[part] = std::stoi(num);
                        num.clear();
                        part++;
                    } else num += corner[i];
                }
                auto fix = [](int i, size_t count) { return i < 0 ? (int)count + i : i - 1; };
                int vi = fix(idx[0], P.size());
                if (vi < 0 || vi >= (int)P.size()) continue;
                mesh.positions.push_back(P[vi]);
                if (idx[1] != 0) {
                    int ti = fix(idx[1], T.size());
                    out_t.push_back(ti >= 0 && ti < (int)T.size() ? T[ti] : UV());
                    any_uvs = true;
                } else out_t.push_back(UV());
                if (idx[2] != 0) {
                    int ni = fix(idx[2], N.size());
                    out_n.push_back(ni >= 0 && ni < (int)N.size() ? unit_vector(N[ni]) : Vec3(0, 1, 0));
                    any_normals = true;
                } else out_n.push_back(Vec3(0, 1, 0));
                face.push_back((int)mesh.positions.size() - 1);
            }
            // Triangulate the polygon as a fan: (0,1,2), (0,2,3), ...
            for (size_t k = 1; k + 1 < face.size(); k++) {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[k]);
                mesh.indices.push_back(face[k + 1]);
            }
        }
    }
    if (any_normals) mesh.normals = out_n;
    if (any_uvs) mesh.uvs = out_t;
    std::printf("Loaded %s: %zu triangles\n", filename.c_str(), mesh.triangle_count());
    return true;
}

// Write a mesh as an OBJ file (positions, uvs, normals if present).
inline bool save_obj(const std::string& filename, const Mesh& mesh) {
    std::ofstream f(filename);
    if (!f) return false;
    f << "# written by the pixel library\n";
    for (auto& p : mesh.positions) f << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
    bool has_uv = mesh.uvs.size() == mesh.positions.size();
    bool has_n = mesh.normals.size() == mesh.positions.size();
    if (has_uv) for (auto& t : mesh.uvs) f << "vt " << t.u << ' ' << t.v << '\n';
    if (has_n) for (auto& n : mesh.normals) f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        f << 'f';
        for (int k = 0; k < 3; k++) {
            int id = mesh.indices[i + k] + 1;
            f << ' ' << id;
            if (has_uv && has_n) f << '/' << id << '/' << id;
            else if (has_uv) f << '/' << id;
            else if (has_n) f << "//" << id;
        }
        f << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Procedural meshes: a grid over (u,v) in [0,1]^2 mapped through a function.
inline Mesh make_parametric_mesh(int nu, int nv, const std::function<Point3(double, double)>& fn) {
    Mesh m;
    for (int j = 0; j <= nv; j++)
        for (int i = 0; i <= nu; i++) {
            double u = (double)i / nu, v = (double)j / nv;
            m.positions.push_back(fn(u, v));
            m.uvs.push_back(UV{u, v});
        }
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            int a = j * (nu + 1) + i, b = a + 1, c = a + (nu + 1), d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, d, a, d, c});
        }
    m.compute_smooth_normals();
    return m;
}

// Torus (donut) around the Y axis.
inline Mesh make_torus(double major_radius, double minor_radius, int nu = 64, int nv = 32) {
    return make_parametric_mesh(nu, nv, [=](double u, double v) {
        double a = u * 2 * pi, b = v * 2 * pi;
        double r = major_radius + minor_radius * std::cos(b);
        return Point3(r * std::cos(a), minor_radius * std::sin(b), r * std::sin(a));
    });
}

// Heightfield terrain: y = height(x, z) over a square of side 'size'.
inline Mesh make_heightfield(double size, int resolution, const std::function<double(double, double)>& height) {
    return make_parametric_mesh(resolution, resolution, [=](double u, double v) {
        double x = (u - 0.5) * size, z = (v - 0.5) * size;
        return Point3(x, height(x, z), z);
    });
}

} // namespace pixel
```

---

## `instance.h`

**File: `include/pixel/instance.h`**

```cpp
// pixel/instance.h
// ------------------------------------------------------------
// Instances: move or rotate an object WITHOUT changing the object.
// Trick: instead of moving the object, move the RAY the opposite way.
// Explained in docs/27-instances.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "hittable.h"

namespace pixel {

class Translate : public Hittable {
public:
    Translate(std::shared_ptr<Hittable> object, const Vec3& offset) : object(object), offset(offset) {
        bbox = object->bounding_box() + offset;
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // Move the ray backwards by the offset.
        Ray offset_r(r.origin() - offset, r.direction(), r.time());
        if (!object->hit(offset_r, ray_t, rec)) return false;
        // Move the hit point forwards by the offset.
        rec.p += offset;
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    Vec3 offset;
    AABB bbox;
};

class RotateY : public Hittable {
public:
    RotateY(std::shared_ptr<Hittable> object, double angle_degrees) : object(object) {
        double radians = degrees_to_radians(angle_degrees);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        AABB b = object->bounding_box();

        // Rotate all 8 corners of the box and take the box around them.
        Point3 min( infinity,  infinity,  infinity);
        Point3 max(-infinity, -infinity, -infinity);
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    double x = i ? b.x.max : b.x.min;
                    double y = j ? b.y.max : b.y.min;
                    double z = k ? b.z.max : b.z.min;
                    Vec3 tester = to_world(Vec3(x, y, z));
                    min = vmin(min, tester);
                    max = vmax(max, tester);
                }
        bbox = AABB(min, max);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // World space -> object space.
        Ray rotated_r(to_object(r.origin()), to_object(r.direction()), r.time());
        if (!object->hit(rotated_r, ray_t, rec)) return false;
        // Object space -> world space.
        rec.p = to_world(rec.p);
        rec.normal = to_world(rec.normal);
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    double sin_theta, cos_theta;
    AABB bbox;

    Vec3 to_world(const Vec3& p) const {
        return Vec3(cos_theta * p.x + sin_theta * p.z, p.y, -sin_theta * p.x + cos_theta * p.z);
    }
    Vec3 to_object(const Vec3& p) const {
        return Vec3(cos_theta * p.x - sin_theta * p.z, p.y, sin_theta * p.x + cos_theta * p.z);
    }
};

} // namespace pixel
```

---

## `volume.h`

**File: `include/pixel/volume.h`**

```cpp
// pixel/volume.h
// ------------------------------------------------------------
// Participating media: fog, smoke, mist, clouds.
// A ray inside the volume has a chance to scatter at every step.
// Explained in docs/28-volumes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include <functional>
#include "vec3.h"
#include "hittable.h"
#include "material.h"
#include "random.h"

namespace pixel {

// A volume of constant density, shaped like 'boundary' (a closed object).
class ConstantMedium : public Hittable {
public:
    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, const Color& albedo)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, std::shared_ptr<Texture> tex)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(tex)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        // Where does the ray enter and leave the boundary?
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        // Random distance until the next collision with a particle.
        double hit_distance = neg_inv_density * std::log(1.0 - random_double());
        if (hit_distance > distance_inside_boundary) return false;   // passed through

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);
        rec.normal = Vec3(1, 0, 0);   // arbitrary: particles have no surface
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double neg_inv_density;
    std::shared_ptr<Material> phase_function;
};

// A volume whose density changes in space (e.g. smoke, clouds).
// Uses "delta tracking" (Woodcock tracking): max_density must be >= density(p) everywhere.
class VariableMedium : public Hittable {
public:
    VariableMedium(std::shared_ptr<Hittable> boundary, double max_density,
                   std::function<double(const Point3&)> density, const Color& albedo)
        : boundary(boundary), max_density(max_density), density(density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;
        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;

        double ray_length = r.direction().length();
        double t = rec1.t;
        while (true) {
            // Step as if the medium had max density everywhere...
            t -= std::log(1.0 - random_double()) / (max_density * ray_length);
            if (t >= rec2.t) return false;
            // ...then accept a real collision with probability density/max_density.
            if (random_double() < density(r.at(t)) / max_density) break;
        }
        rec.t = t;
        rec.p = r.at(t);
        rec.normal = Vec3(1, 0, 0);
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }

    AABB bounding_box() const override { return boundary->bounding_box(); }

private:
    std::shared_ptr<Hittable> boundary;
    double max_density;
    std::function<double(const Point3&)> density;
    std::shared_ptr<Material> phase_function;
};

} // namespace pixel
```

---

## `sdf.h`

**File: `include/pixel/sdf.h`**

```cpp
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
```

---

## `sky.h`

**File: `include/pixel/sky.h`**

```cpp
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
```

---

## `camera.h`

**File: `include/pixel/camera.h`**

```cpp
// pixel/camera.h
// ------------------------------------------------------------
// The Camera: builds rays for every pixel, traces them through the
// world, and averages the results. This is the heart of the renderer.
//   * positionable camera (lookfrom / lookat / vup / vfov)   docs/20
//   * depth of field (defocus blur)                          docs/20
//   * anti-aliasing with stratified samples                  docs/16
//   * multithreading                                         docs/21
//   * motion blur (random ray time)                          docs/22
//   * light sampling with mixture PDFs                       docs/31
//   * Russian roulette + firefly clamping                    docs/31
//   * AOV buffers (albedo / normal) for the denoiser         docs/36
// ------------------------------------------------------------
#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "random.h"
#include "hittable.h"
#include "material.h"
#include "pdf.h"
#include "image.h"
#include "sky.h"

namespace pixel {

class Camera {
public:
    // ---------------- image settings ----------------
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 400;
    int samples_per_pixel = 10;      // rays per pixel: more = less noise, slower
    int max_depth = 10;              // maximum number of bounces

    // ---------------- lens & position ----------------
    double vfov = 90;                        // vertical field of view in degrees
    Point3 lookfrom = Point3(0, 0, 0);       // where the camera is
    Point3 lookat = Point3(0, 0, -1);        // what it looks at
    Vec3 vup = Vec3(0, 1, 0);                // which way is "up"
    double defocus_angle = 0;                // 0 = everything sharp
    double focus_dist = 10;                  // distance that is perfectly sharp
    double shift_x = 0, shift_y = 0;         // lens shift (moves the image window)

    // ---------------- lighting ----------------
    Background background = gradient_sky();

    // ---------------- quality & speed ----------------
    int threads = 0;                    // 0 = use all CPU cores
    bool show_progress = true;
    bool russian_roulette = true;       // randomly stop weak paths (faster, still unbiased)
    int rr_start_depth = 3;
    double max_sample_value = 0.0;      // > 0: clamp very bright samples (removes fireflies)
    uint64_t seed = 1;
    double t_min = 0.001;               // ignore hits closer than this (avoids "shadow acne")

    // ---------------- extra outputs ----------------
    bool collect_aovs = false;          // also fill albedo_aov and normal_aov
    Image albedo_aov;
    Image normal_aov;

    // Extra per-sample information for the denoiser (first surface hit).
    struct AovSlot { Color albedo; Vec3 normal; };

    int image_height() const { int h = (int)(image_width / aspect_ratio); return h < 1 ? 1 : h; }

    // Render the whole image. 'lights' may be nullptr (then no light sampling).
    Image render(const Hittable& world, const Hittable* lights = nullptr) {
        initialize();
        Image img(image_width, height);
        if (collect_aovs) {
            albedo_aov = Image(image_width, height);
            normal_aov = Image(image_width, height);
        }
        light_list = lights;
        if (auto list = dynamic_cast<const HittableList*>(lights))
            if (list->objects.empty()) light_list = nullptr;

        int n_threads = threads > 0 ? threads : (int)std::thread::hardware_concurrency();
        if (n_threads < 1) n_threads = 1;

        std::atomic<int> next_row(0);
        std::atomic<int> rows_done(0);
        std::mutex print_mutex;
        auto start_time = std::chrono::steady_clock::now();

        auto worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1);
                if (j >= height) break;
                // Seed per row: the image is identical no matter how many threads we use.
                seed_thread_rng(seed * 1000003ULL + (uint64_t)j * 7919ULL + 17);
                for (int i = 0; i < image_width; i++) {
                    Color pixel_color(0, 0, 0);
                    Color albedo_sum(0, 0, 0);
                    Vec3 normal_sum(0, 0, 0);
                    for (int s = 0; s < samples_per_pixel; s++) {
                        Ray r = get_ray(i, j, s);
                        AovSlot slot;
                        pixel_color += trace(r, world, collect_aovs ? &slot : nullptr);
                        albedo_sum += slot.albedo;
                        normal_sum += slot.normal;
                    }
                    img.at(i, j) = pixel_color / samples_per_pixel;
                    if (collect_aovs) {
                        albedo_aov.at(i, j) = albedo_sum / samples_per_pixel;
                        normal_aov.at(i, j) = normal_sum / samples_per_pixel;
                    }
                }
                int done = ++rows_done;
                if (show_progress) {
                    std::lock_guard<std::mutex> lock(print_mutex);
                    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                    std::printf("\rRendering: %3d%%  (%d/%d rows, %.1fs)   ", 100 * done / height, done, height, secs);
                    std::fflush(stdout);
                }
            }
        };

        std::vector<std::thread> pool;
        for (int t = 0; t < n_threads; t++) pool.emplace_back(worker);
        for (auto& th : pool) th.join();

        if (show_progress) {
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
            std::printf("\rRendering: done in %.1f seconds using %d threads.            \n", secs, n_threads);
        }
        return img;
    }

    // Trace one ray and return the light (radiance) coming back along it.
    // This is the "path tracer": an iterative loop over bounces.
    Color trace(const Ray& start_ray, const Hittable& world, AovSlot* aov = nullptr) const {
        Color radiance(0, 0, 0);
        Color throughput(1, 1, 1);     // how much light survives the bounces so far
        Ray ray = start_ray;

        for (int depth = 0; depth < max_depth; depth++) {
            HitRecord rec;
            if (!world.hit(ray, Interval(t_min, infinity), rec)) {
                Color bg = background(ray);
                radiance += throughput * bg;
                if (aov && depth == 0) { aov->albedo = bg; aov->normal = Vec3(0, 0, 0); }
                break;
            }
            if (!rec.mat) break;   // an object without a material: treat as black
            if (aov && depth == 0) { aov->albedo = rec.mat->aov_albedo(rec); aov->normal = rec.normal; }

            // 1. Light emitted by the surface itself.
            radiance += throughput * rec.mat->emitted(ray, rec, rec.u, rec.v, rec.p);

            // 2. Does the light bounce?
            ScatterRecord srec;
            if (!rec.mat->scatter(ray, rec, srec)) break;

            if (srec.skip_pdf) {
                // Mirror-like: the direction was decided by the material.
                throughput *= srec.attenuation;
                ray = srec.skip_pdf_ray;
            } else {
                // Diffuse-like: choose a direction, half towards lights, half by the material.
                Vec3 dir;
                double pdf_val;
                if (light_list) {
                    HittablePDF light_pdf(*light_list, rec.p);
                    MixturePDF mix(&light_pdf, srec.pdf_ptr.get());
                    dir = mix.generate();
                    pdf_val = mix.value(dir);
                } else {
                    dir = srec.pdf_ptr->generate();
                    pdf_val = srec.pdf_ptr->value(dir);
                }
                if (pdf_val <= 1e-12) break;
                Ray scattered(rec.p, dir, ray.time());
                double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);
                throughput *= srec.attenuation * (scattering_pdf / pdf_val);
                ray = scattered;
            }

            // 3. Russian roulette: weak paths are stopped at random; survivors get stronger.
            if (russian_roulette && depth >= rr_start_depth) {
                double p = clampd(throughput.max_component(), 0.05, 0.95);
                if (random_double() > p) break;
                throughput /= p;
            }
        }

        if (max_sample_value > 0.0) {
            double m = radiance.max_component();
            if (m > max_sample_value) radiance *= max_sample_value / m;
        }
        if (radiance.x != radiance.x || radiance.y != radiance.y || radiance.z != radiance.z)
            return Color(0, 0, 0);   // drop NaNs
        return radiance;
    }

    // Build the camera coordinate system. Called automatically by render().
    void initialize() {
        height = image_height();
        center = lookfrom;

        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        double viewport_height = 2 * h * focus_dist;
        double viewport_width = viewport_height * ((double)image_width / height);

        // u = right, v = up, w = backwards (camera looks along -w)
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        Vec3 viewport_u = viewport_width * u;      // across the top edge
        Vec3 viewport_v = viewport_height * -v;    // down the left edge
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / height;

        Point3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2
                                   + shift_x * viewport_u - shift_y * viewport_v;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;

        sqrt_spp = (int)std::sqrt((double)samples_per_pixel);
        if (sqrt_spp < 1) sqrt_spp = 1;
    }

    // Ray for pixel (i, j), sample number s.
    Ray get_ray(int i, int j, int s) const {
        double ox, oy;
        if (s < sqrt_spp * sqrt_spp) {
            // Stratified: split the pixel into a grid, one random point per cell.
            int si = s % sqrt_spp, sj = s / sqrt_spp;
            ox = (si + random_double()) / sqrt_spp - 0.5;
            oy = (sj + random_double()) / sqrt_spp - 0.5;
        } else {
            ox = random_double() - 0.5;
            oy = random_double() - 0.5;
        }
        Point3 pixel_sample = pixel00_loc + ((i + ox) * pixel_delta_u) + ((j + oy) * pixel_delta_v);
        Point3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        Vec3 ray_direction = pixel_sample - ray_origin;
        double ray_time = random_double();       // for motion blur: shutter open during [0,1)
        return Ray(ray_origin, ray_direction, ray_time);
    }

private:
    int height = 1;
    Point3 center;
    Point3 pixel00_loc;
    Vec3 pixel_delta_u, pixel_delta_v;
    Vec3 u, v, w;
    Vec3 defocus_disk_u, defocus_disk_v;
    int sqrt_spp = 1;
    const Hittable* light_list = nullptr;

    Point3 defocus_disk_sample() const {
        Vec3 p = random_in_unit_disk();
        return center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
    }
};

} // namespace pixel
```

---

## `post.h`

**File: `include/pixel/post.h`**

```cpp
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
```

---

## `gif.h`

**File: `include/pixel/gif.h`**

```cpp
// pixel/gif.h
// ------------------------------------------------------------
// Our own animated GIF encoder.
//   * fixed 252-color palette (6 red x 7 green x 6 blue levels)
//   * ordered (Bayer) dithering to hide the limited palette
//   * LZW compression (the algorithm GIF is built on)
// Explained in docs/39-animation.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "image.h"

namespace pixel {

class GifWriter {
public:
    // delay_cs = time per frame in 1/100 seconds (4 = 25 frames per second)
    bool begin(const std::string& filename, int width, int height, int delay_cs = 4, int loop_count = 0) {
        ensure_parent_folder(filename);
        f = std::fopen(filename.c_str(), "wb");
        if (!f) return false;
        w = width; h = height; delay = delay_cs;

        put_str("GIF89a");
        put_u16(w); put_u16(h);
        put_byte(0xF7);          // global color table, 8 bits color resolution, 256 entries
        put_byte(0);             // background color index
        put_byte(0);             // pixel aspect ratio (unused)
        // Global color table: our fixed palette.
        for (int i = 0; i < 256; i++) {
            int r = 0, g = 0, b = 0;
            if (i < 252) {
                r = (i / 42) * 255 / 5;
                g = ((i / 6) % 7) * 255 / 6;
                b = (i % 6) * 255 / 5;
            }
            put_byte(r); put_byte(g); put_byte(b);
        }
        // "NETSCAPE2.0" extension: makes the animation loop.
        put_byte(0x21); put_byte(0xFF); put_byte(0x0B);
        put_str("NETSCAPE2.0");
        put_byte(0x03); put_byte(0x01); put_u16(loop_count); put_byte(0x00);
        return true;
    }

    void add_frame(const Image& img, const SaveOptions& opt = SaveOptions()) {
        if (!f) return;
        std::vector<uint8_t> rgb = to_rgb8(img, opt);
        std::vector<uint8_t> indices((size_t)w * h);
        static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int sx = x < img.width ? x : img.width - 1;
                int sy = y < img.height ? y : img.height - 1;
                size_t i = ((size_t)sy * img.width + sx) * 3;
                double d = (bayer[y & 3][x & 3] + 0.5) / 16.0 - 0.5;   // -0.5 .. 0.5
                int r = quantize(rgb[i + 0], 5, d);
                int g = quantize(rgb[i + 1], 6, d);
                int b = quantize(rgb[i + 2], 5, d);
                indices[(size_t)y * w + x] = (uint8_t)(r * 42 + g * 6 + b);
            }

        // Graphic Control Extension: frame delay.
        put_byte(0x21); put_byte(0xF9); put_byte(0x04);
        put_byte(0x04);          // disposal: leave frame in place
        put_u16(delay);
        put_byte(0);             // transparent color index (unused)
        put_byte(0);
        // Image descriptor.
        put_byte(0x2C);
        put_u16(0); put_u16(0); put_u16(w); put_u16(h);
        put_byte(0);             // no local color table, not interlaced
        lzw_encode(indices);
    }

    void end() {
        if (!f) return;
        put_byte(0x3B);          // trailer
        std::fclose(f);
        f = nullptr;
    }

    ~GifWriter() { end(); }

private:
    FILE* f = nullptr;
    int w = 0, h = 0, delay = 4;

    // Map 0..255 to 0..levels with a dither offset d (in units of one level).
    static int quantize(int value, int levels, double d) {
        double v = value / 255.0 * levels + d;
        int q = (int)(v + 0.5);
        return q < 0 ? 0 : (q > levels ? levels : q);
    }

    void put_byte(int b) { std::fputc(b & 0xFF, f); }
    void put_u16(int v) { put_byte(v & 0xFF); put_byte((v >> 8) & 0xFF); }
    void put_str(const char* s) { while (*s) put_byte(*s++); }

    // ---------------- LZW compression ------------------------------------
    // The dictionary starts with all 256 single colors. Whenever we see a
    // sequence we have not seen before, we give it a new code number.
    void lzw_encode(const std::vector<uint8_t>& data) {
        const int min_code_size = 8;
        const int clear_code = 1 << min_code_size;   // 256
        const int end_code = clear_code + 1;         // 257
        put_byte(min_code_size);

        std::vector<uint8_t> block;                  // bytes waiting for a sub-block
        uint32_t bit_buffer = 0;
        int bit_count = 0;
        int code_size = min_code_size + 1;

        auto flush_block = [&]() {
            if (block.empty()) return;
            put_byte((int)block.size());
            std::fwrite(block.data(), 1, block.size(), f);
            block.clear();
        };
        auto emit = [&](int code) {
            bit_buffer |= (uint32_t)code << bit_count;
            bit_count += code_size;
            while (bit_count >= 8) {
                block.push_back((uint8_t)(bit_buffer & 0xFF));
                bit_buffer >>= 8;
                bit_count -= 8;
                if (block.size() == 255) flush_block();
            }
        };

        // dictionary[code * 256 + next_color] = code for (sequence + next_color), or -1
        std::vector<int16_t> dict(4096 * 256, -1);
        int next_code = end_code + 1;

        emit(clear_code);
        if (data.empty()) {
            emit(end_code);
        } else {
            int prefix = data[0];
            for (size_t i = 1; i < data.size(); i++) {
                int c = data[i];
                int key = prefix * 256 + c;
                if (dict[key] >= 0) {
                    prefix = dict[key];            // known sequence: keep growing it
                    continue;
                }
                emit(prefix);                      // output the longest known sequence
                int assigned = next_code++;
                dict[key] = (int16_t)assigned;     // remember the new, longer sequence
                if (assigned >= (1 << code_size)) code_size++;
                if (assigned == 4095) {            // dictionary full: start over
                    emit(clear_code);
                    std::fill(dict.begin(), dict.end(), (int16_t)-1);
                    next_code = end_code + 1;
                    code_size = min_code_size + 1;
                }
                prefix = c;
            }
            emit(prefix);
            emit(end_code);
        }
        if (bit_count > 0) {
            block.push_back((uint8_t)(bit_buffer & 0xFF));
            if (block.size() == 255) flush_block();
        }
        flush_block();
        put_byte(0);   // block terminator
    }
};

} // namespace pixel
```

---

## `pixel.h`

**File: `include/pixel/pixel.h`**

```cpp
// pixel/pixel.h
// ------------------------------------------------------------
// "pixel" - our own from-scratch rendering library.
// Include this ONE file to get everything.
//
//   Pixels & files : vec3.h color.h image.h png.h
//   2D drawing     : canvas.h noise.h
//   Ray tracing    : ray.h aabb.h hittable.h sphere.h quad.h triangle.h
//                    bvh.h instance.h volume.h sdf.h
//   Shading        : texture.h material.h pdf.h sky.h
//   Rendering      : camera.h
//   Film look      : post.h
//   Animation      : gif.h
//
// No external libraries: only the C++17 standard library.
// ------------------------------------------------------------
#pragma once
#include "vec3.h"
#include "random.h"
#include "color.h"
#include "png.h"
#include "image.h"
#include "canvas.h"
#include "noise.h"
#include "ray.h"
#include "aabb.h"
#include "hittable.h"
#include "pdf.h"
#include "texture.h"
#include "material.h"
#include "sphere.h"
#include "quad.h"
#include "bvh.h"
#include "triangle.h"
#include "instance.h"
#include "volume.h"
#include "sdf.h"
#include "sky.h"
#include "camera.h"
#include "post.h"
#include "gif.h"
```
