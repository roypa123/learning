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
