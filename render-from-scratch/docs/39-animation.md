# Chapter 39 — Animation

[← The final shot](38-final-shot.md) · [Contents](README.md) · [Next: Where to go next →](40-next-steps.md)

---

## Goal

A film is a sequence of images. In this final technical chapter you'll:

* render a sequence of **frames** where things change over time,
* use a **camera orbit** ("turntable") and a physically plausible **bouncing ball**,
* get correct **motion blur** per frame with a proper shutter,
* understand why each frame should have **different noise**,
* write an **animated GIF** with our own encoder: palette, dithering and **LZW** compression,
* (optionally) turn the PNG frames into a video.

---

## 1. Frames and time

Movies play at **24 frames per second**. For frame number `f`, the time is `t = f / 24` seconds. Everything
that moves is a function of `t`:

```cpp
double t = f / fps;                            // time of this frame
double angle = 2 * pi * f / frames;            // camera: one full circle over the animation
cam.lookfrom = Point3(7 * sin(angle), 2.8, 7 * cos(angle));   // orbit at radius 7
```

### 1.1 A bouncing ball

A thrown ball follows a **parabola** under gravity. For a bounce every second:

```
phase = t − floor(t)                   0..1 within the current bounce
height = 0.35 + 1.6 · 4 · phase · (1 − phase)
```

`4 · phase · (1 − phase)` is 0 at the start and end of each bounce and 1 in the middle: a parabola, just like
real physics, with the ball moving fast near the ground and slowly at the top.

### 1.2 Motion blur per frame

Film cameras use a **180° shutter**: the shutter is open for half of each frame's duration (1/48 s at 24 fps).
During that time the ball moves, so we give it a start and end position:

```cpp
Point3 p0(1.9, ball_height(t), 0.8);
Point3 p1(1.9, ball_height(t + shutter / fps), 0.8);
world.add(std::make_shared<Sphere>(p0, p1, 0.35, ball_mat));   // moving sphere, chapter 22
```

Near the ground the ball moves fast and blurs a lot; at the top it's almost sharp. That's exactly what real
footage looks like, and it's what makes animation feel smooth rather than jittery.

### 1.3 Noise between frames

If every frame used the same random numbers, the noise pattern would stay stuck to the screen while the scene
moves behind it, which looks like a dirty lens. Changing the seed per frame (`cam.seed = 1 + f`) makes
the noise **change** every frame, so it reads as film grain. Since we also denoise each frame, some
"flicker" can remain. Production renderers use temporal denoisers that look at neighboring frames.

---

## 2. Writing an animated GIF

GIF (1987) is ancient, but it's the one animated image format that opens everywhere, and it's
fun to write. It has two big limitations: **256 colors per frame**, and **LZW** compression.

### 2.1 A fixed palette

We use one palette for all frames: 6 levels of red × 7 of green × 6 of blue = **252 colors** (green gets
more levels because the eye is most sensitive to it, chapter 4). A color `(r, g, b)` in 0..255 maps to
palette index `r6 · 42 + g7 · 6 + b6`.

### 2.2 Dithering

With only 6–7 levels per channel, smooth gradients would show ugly **bands**. **Ordered dithering** adds a
small, fixed pattern of offsets (a 4×4 **Bayer matrix**) before rounding:

```
 Bayer 4×4 (÷16):       without dithering:     with dithering:
  0  8  2 10            ████████▓▓▓▓▓▓▓▓       ███▓█▓█▓▓▓▓▒▓▒▓▒
 12  4 14  6            (hard band)            (mixed pixels: looks like
  3 11  1  9                                     the in-between shade from
 15  7 13  5                                     a distance)
```

Neighboring pixels round differently, and from a normal viewing distance the eye averages them into the
in-between color.

### 2.3 LZW compression

GIF uses **LZW** (Lempel–Ziv–Welch, 1984), a cousin of the LZ77 from chapter 7 with a different twist.
Instead of "copy from back there", it builds a **dictionary of sequences** as it goes:

1. Start with a dictionary of all 256 single colors (codes 0–255), plus two special codes: **clear** (256) and
   **end** (257).
2. Read colors, extending the current sequence as long as `sequence + next color` is already in the
   dictionary.
3. When it isn't: output the code for the current sequence, add `sequence + next color` as a **new**
   dictionary entry, and start a new sequence with the next color.

```
input:   A A A A A A A A
step:    "A" known, "AA"? no  -> output A,  add AA=258
         "A" known, "AA" known, "AAA"? no -> output 258 (AA), add AAA=259
         "A","AA","AAA" known, "AAAA"? no -> output 259 (AAA), add AAAA=260
         ...
output:  A, 258, 259, ... : long runs become very few codes
```

The decoder rebuilds exactly the same dictionary from the codes, so the dictionary is never stored in
the file. That's the elegant part.

Details handled by `gif.h`:

* Codes start at **9 bits** and grow (up to 12 bits) as the dictionary grows past 512, 1024 and 2048 entries.
* When the dictionary is full (4096 entries), we emit a **clear** code and start over.
* Bits are packed LSB-first (like DEFLATE) into **sub-blocks** of at most 255 bytes.
* The file has a header, the global palette, a **NETSCAPE2.0** extension (to make it loop), and for each
  frame a delay and an image block.

### `gif.h`

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

## 3. The program

**File: `chapters/ch39_animation.cpp`**

```cpp
// ch39_animation.cpp
// ------------------------------------------------------------
// Chapter 39: Animation - many images in a row.
// A camera orbits a small scene while a ball bounces (with motion blur).
//   images/ch39_frames/frame_000.png ... frame_047.png   (every frame)
//   images/ch39_turntable.gif                            (our own GIF encoder!)
//
//   run ch39_animation          48 frames, 320x180
//   run ch39_animation final    96 frames, 640x360, more samples
// ------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

// Height of the bouncing ball at time t (seconds): a bounce every second.
static double ball_height(double t) {
    double phase = t - std::floor(t);                // 0..1 within the current bounce
    return 0.35 + 1.6 * 4.0 * phase * (1.0 - phase);   // parabola: like real gravity
}

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    const int frames = final_quality ? 96 : 48;
    const int width = final_quality ? 640 : 320;
    const int spp = final_quality ? 64 : 16;
    const double fps = 24.0;
    const double shutter = 0.5;   // "180 degree shutter": open for half of each frame

    // ---------- static part of the scene -----------------------------------
    auto floor_tex = std::make_shared<CheckerTexture>(0.5, Color(0.8, 0.8, 0.8), Color(0.2, 0.2, 0.22));
    auto floor_mat = std::make_shared<Lambertian>(floor_tex);
    auto torus_mat = std::make_shared<RoughMetal>(Color(1.0, 0.77, 0.34), 0.2);
    auto glass = std::make_shared<Dielectric>(1.5);
    auto blue = std::make_shared<Plastic>(hex_color(0x1D4ED8), 0.25);
    auto ball_mat = std::make_shared<Plastic>(hex_color(0xDC2626), 0.3);
    auto panel = std::make_shared<DiffuseLight>(Color(5, 5, 5));

    Mesh torus_mesh = make_torus(0.8, 0.25, 48, 24);
    std::shared_ptr<Hittable> torus = torus_mesh.build(torus_mat);
    torus = std::make_shared<Translate>(torus, Vec3(0, 0.25, 0));

    auto light_quad = std::make_shared<Quad>(Point3(-2, 5, -2), Vec3(4, 0, 0), Vec3(0, 0, 4), panel);   // u x v points down

    GifWriter gif;
    int height = (int)(width / (16.0 / 9.0));
    gif.begin("images/ch39_turntable.gif", width, height, (int)std::round(100.0 / fps));

    for (int f = 0; f < frames; f++) {
        double t = f / fps;                       // time of this frame in seconds
        double angle = 2 * pi * f / frames;       // camera goes around once

        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60), floor_mat));
        world.add(torus);
        world.add(std::make_shared<Sphere>(Point3(0, 0.6, 0), 0.6, glass));
        world.add(std::make_shared<Sphere>(Point3(-1.8, 0.45, -1.2), 0.45, blue));
        world.add(light_quad);
        lights.add(light_quad);

        // The ball moves DURING the shutter: from its position at t to t + shutter/fps.
        Point3 p0(1.9, ball_height(t), 0.8);
        Point3 p1(1.9, ball_height(t + shutter / fps), 0.8);
        world.add(std::make_shared<Sphere>(p0, p1, 0.35, ball_mat));

        Camera cam;
        cam.image_width = width;
        cam.samples_per_pixel = spp;
        cam.max_depth = 12;
        cam.vfov = 35;
        cam.lookfrom = Point3(7 * std::sin(angle), 2.8, 7 * std::cos(angle));
        cam.lookat = Point3(0, 0.6, 0);
        cam.background = gradient_sky(Color(0.25, 0.25, 0.3), Color(0.5, 0.6, 0.8));
        cam.show_progress = false;
        cam.collect_aovs = true;
        cam.max_sample_value = 20;
        cam.seed = 1 + f;                         // different noise each frame (looks like film grain)

        Image img = cam.render(world, &lights);
        post::DenoiseSettings ds;
        ds.radius = 4;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
        img = post::tonemap(img, ToneMapper::Aces);

        char name[128];
        std::snprintf(name, sizeof(name), "images/ch39_frames/frame_%03d.png", f);
        write_png(name, img);
        gif.add_frame(img);
        std::printf("frame %d/%d done\n", f + 1, frames);
    }
    gif.end();
    std::printf("Saved images/ch39_turntable.gif\n");
    return 0;
}
```

```bat
run ch39_animation           :: 48 frames at 320x180: a few minutes
run ch39_animation final     :: 96 frames at 640x360: much longer
```

### 3.1 Making a video (optional)

Our code writes every frame as a PNG in `images/ch39_frames/`. To turn them into an MP4 video, you need a video
encoder, which is far outside the scope of this book. The free tool **ffmpeg** does it in one line:

```bat
ffmpeg -framerate 24 -i images\ch39_frames\frame_%03d.png -pix_fmt yuv420p images\ch39_turntable.mp4
```

(This is the only external program mentioned in the book, and it's optional. The GIF is made entirely
by our own code.)

---

## 4. What you should see

![Turntable](../images/ch39_turntable.gif)

> **Image description (animated):** A small looping animation. The camera circles slowly all the way
> around a little scene on a grey checkered floor: a clear glass ball sitting inside a golden ring (torus), a
> glossy blue ball and a glossy red ball bouncing up and down. The red ball is sharp at the top of each
> bounce and streaked vertically (motion blur) when it's near the floor. The glass ball refracts the
> checkerboard, and the gold ring reflects the moving scene. Colors are slightly speckled from the GIF
> dithering.

Individual frames, like `images/ch39_frames/frame_000.png`, are full-color PNGs without dithering.

---

## Try it yourself

1. Animate the **sun** from chapter 33 over a day: a time-lapse!
2. Animate the chapter 11 **clouds** by adding time to the noise coordinates.
3. Make a **dolly zoom**: move the camera back while narrowing vfov so the subject stays the same size.
4. Animate the **Mandelbulb power** from 2 to 8.
5. Improve the GIF encoder with a per-frame **median-cut palette** (much better colors) or
   **Floyd–Steinberg** error diffusion.

## Common problems

| Symptom | Cause |
|---------|-------|
| GIF won't open | Wrong LZW code-size switch timing or missing trailer (`end()` not called) |
| Colors banded | Dithering disabled or palette too small |
| Animation flickers strongly | Too few samples per frame; add samples or increase denoise strength |
| Motion looks jerky | No motion blur (shutter 0), or too few frames per second |

---

## Summary

* An animation is a loop over frames; everything that moves is a function of time.
* Use a 180° shutter for natural motion blur; change the noise seed per frame.
* GIF = palette + dithering + LZW (a dictionary built on the fly) + a looping extension.

Next: [Chapter 40 — Where to go next →](40-next-steps.md)
