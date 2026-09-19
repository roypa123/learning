# Chapter 24 — Textures

[← BVH](23-bvh.md) · [Contents](README.md) · [Next: Quads, triangles & meshes →](25-quads-triangles-meshes.md)

> 📖 **Line by line:** [texture explained line by line](line-by-line/texture.md)

---

## Goal

So far every object has a single color. Real surfaces vary: wood grain, marble veins, printed labels,
planets. A **texture** is a function that answers *"what color is the surface at this exact point?"*.
You'll learn:

* the `Texture` interface, and the **solid** and **checker** textures,
* **UV coordinates** and how to wrap a flat image around a sphere,
* **image textures** (including painting a planet map procedurally and loading it back),
* **Perlin noise textures**: smooth, turbulent and marble.

---

## 1. The Texture interface

```cpp
class Texture {
public:
    virtual Color value(double u, double v, const Point3& p) const = 0;
};
```

A texture gets two kinds of location information:

* `(u, v)`: **surface coordinates**, a 2D "address" on the surface, each 0..1 (like latitude and
  longitude on a globe),
* `p`: the **3D point** in space.

Some textures use one, some the other. `Lambertian` (and later `Plastic`) takes a texture instead of a
fixed color and asks it for the albedo at every hit: `tex->value(rec.u, rec.v, rec.p)`.

---

## 2. Solid and checker textures

`SolidColor` returns the same color everywhere. It's what `Lambertian(Color(...))` uses internally.

`CheckerTexture` is a **3D** checkerboard: space is divided into cubes of size `scale`, and cubes
alternate between two textures:

```cpp
int x = floor(p.x / scale), y = floor(p.y / scale), z = floor(p.z / scale);
bool is_even = (x + y + z) % 2 == 0;
return is_even ? even->value(u, v, p) : odd->value(u, v, p);
```

Because it uses the 3D point, it's like the object was carved out of a block of checkered material (a
**solid texture**). On a sphere, the pattern gets distorted where the sphere's surface cuts the cubes at
an angle. There's also `UVCheckerTexture`, which uses (u, v) instead, so the squares follow the surface.

---

## 3. UV coordinates on a sphere

To wrap a flat picture around a sphere, like a world map around a globe, we need to map each surface
point to `(u, v)`. We use **spherical coordinates**:

```
θ (theta) = angle from the bottom pole (−y) up to the top (+y):   0 .. π
φ (phi)   = angle around the y axis:                               0 .. 2π

u = φ / (2π)        v = θ / π
```

For a point on the unit sphere (x, y, z):

```
θ = acos(−y)                    (y = −1 → 0, y = +1 → π)
φ = atan2(−z, x) + π            (atan2 gives −π..π; adding π gives 0..2π)
```

```
        v = 1  (north pole)
         ┌──────────────────────┐
         │   the image is       │        u goes around the equator
         │   wrapped around     │        v goes from pole to pole
         │   like a label       │
         └──────────────────────┘
        v = 0  (south pole)
        u = 0 ─────────────▶ u = 1
```

That's `Sphere::get_sphere_uv`. The poles squeeze a whole row of the image into a single point (look
at the ice caps in the planet render), which is the same distortion as on world maps.

---

## 4. Image textures

`ImageTexture` holds an `Image` and looks colors up with bilinear filtering (chapter 5):

```cpp
Color value(double u, double v, const Point3&) const override {
    return image.sample_bilinear(u, v);
}
```

It can be created from an `Image` in memory or loaded from a **PPM** file. If loading fails it returns
**magenta**, the traditional "missing texture" color in graphics. When you see bright magenta, a file
path is wrong.

> **Why only PPM?** Reading PNG requires *decompressing* DEFLATE (the reverse of chapter 7), which is
> a nice project (see chapter 40) but beyond what we need. Convert your images to PPM with any image
> program ("Export as... PPM/PNM"), or generate them in code as we do here.

### 4.1 Painting a planet

The chapter program *paints* a 1024×512 planet map procedurally and saves it as PNG (to look at) and
PPM (to load back as a texture). For every map pixel:

1. Convert the pixel to longitude/latitude, then to a 3D point on a unit sphere. We sample the noise on
   the sphere, not on the flat map, so the texture has no seam at the edges and no stretching at the poles.
2. Take Perlin fBm at that point as a "height".
3. Pick a color by height: deep ocean → shallow ocean → beach → forest → rock → snow.
4. Add ice caps near the poles.

This is how many games and films build planets and terrain.

---

## 5. Noise textures

Perlin noise (chapter 11) in 3D makes excellent solid textures. `NoiseTexture` has three styles:

| Style | Formula | Looks like |
|-------|---------|-----------|
| `Smooth` | `0.5 · (1 + noise(scale · p))` | soft blotches, clouds |
| `Turbulence` | `turbulence(scale · p)` | smoke, fire, dirty stone |
| `Marble` | `0.5 · (1 + sin(scale · p.z + 10 · turbulence(p)))` | marble veins |

The marble trick is lovely: `sin(scale · z)` alone makes regular parallel stripes. Adding turbulence to the
*phase* of the sine wiggles the stripes into veins, just like layers of sediment deformed over millions
of years.

```
sin(z):            ||||||||||||      regular stripes
sin(z + 10·turb):  )\\|((//|)\\(     wiggly veins
```

`FunctionTexture` wraps any lambda: `color = f(u, v, p)`. It's great for quick experiments (chapter 25
colors a terrain by height with it).

### `texture.h`

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

## 6. The program

**File: `chapters/ch24_textures.cpp`**

```cpp
// ch24_textures.cpp
// ------------------------------------------------------------
// Chapter 24: Textures.
//   images/ch24_checker.png     - 3D checker texture on two big spheres
//   images/ch24_planet.png      - an image texture (a planet map we paint ourselves)
//   images/ch24_perlin.png      - Perlin noise: smooth, turbulent, marble
//   images/ch24_planet_map.png  - the flat 2:1 "world map" we generated
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

static Camera make_camera(Point3 from, Point3 at, double vfov) {
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 64;
    cam.max_depth = 20;
    cam.vfov = vfov;
    cam.lookfrom = from;
    cam.lookat = at;
    return cam;
}

// Paint a fantasy planet map procedurally: oceans, land, ice caps.
static Image paint_planet_map(int W, int H) {
    Image map(W, H);
    Perlin noise(99);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            // Turn map coordinates into a point on a sphere so the noise wraps seamlessly.
            double lon = (double)x / W * 2 * pi;
            double lat = ((double)y / H - 0.5) * pi;
            Point3 p(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
            double h = noise.fbm(p * 2.2, 6);                  // -1 .. 1 "height"
            Color c;
            if (h < 0.0)       c = lerp(hex_color(0x0B3D91), hex_color(0x1E6FD9), clamp01(1 + h * 3));   // ocean
            else if (h < 0.05) c = hex_color(0xD8C690);                                                  // beach
            else if (h < 0.3)  c = lerp(hex_color(0x3B7D2A), hex_color(0x2A5A1E), h / 0.3);             // forest
            else               c = lerp(hex_color(0x7A6A58), hex_color(0xEEEEEE), clamp01((h - 0.3) * 3)); // mountains
            double polar = std::fabs(std::sin(lat));
            if (polar + 0.1 * h > 0.85) c = hex_color(0xF4F8FF);                                         // ice caps
            map.at(x, y) = c;
        }
    return map;
}

int main() {
    // ---------- 1. Checkered spheres -------------------------------------
    {
        auto checker = std::make_shared<CheckerTexture>(0.32, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -10, 0), 10, std::make_shared<Lambertian>(checker)));
        world.add(std::make_shared<Sphere>(Point3(0, 10, 0), 10, std::make_shared<Lambertian>(checker)));
        save_image("images/ch24_checker.png", make_camera(Point3(13, 2, 3), Point3(0, 0, 0), 20).render(world));
    }

    // ---------- 2. Image texture -----------------------------------------
    {
        Image map = paint_planet_map(1024, 512);
        save_image("images/ch24_planet_map.png", map);
        save_image("images/ch24_planet_map.ppm", map);          // also as PPM...
        auto tex = std::make_shared<ImageTexture>("images/ch24_planet_map.ppm");   // ...and load it back

        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, 0, 0), 2, std::make_shared<Lambertian>(tex)));
        Camera cam = make_camera(Point3(0, 1.5, 12), Point3(0, 0, 0), 20);
        cam.background = solid_background(Color(0.01, 0.01, 0.02));
        // The sun: a very bright sphere to the side. (Lights are explained in chapter 26.)
        world.add(std::make_shared<Sphere>(Point3(-30, 10, 20), 8, std::make_shared<DiffuseLight>(Color(12, 11, 10))));
        cam.samples_per_pixel = 200;
        save_image("images/ch24_planet.png", cam.render(world));
    }

    // ---------- 3. Perlin textures ---------------------------------------
    {
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, -2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(2.0, NoiseTexture::Smooth))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(3.0, NoiseTexture::Turbulence))));
        save_image("images/ch24_perlin.png", make_camera(Point3(13, 3, 3), Point3(0, 1.5, 0), 30).render(world));
    }
    return 0;
}
```

Note the planet scene uses a black background and a big **glowing sphere** as the sun. Glowing materials
(`DiffuseLight`) are explained in chapter 26; here we just use one.

```bat
run ch24_textures
```

---

## 7. What you should see

![Checker](../images/ch24_checker.png)

> **Image description:** Two huge spheres, one above and one below, touching at the center of the image,
> both covered with a dark-green-and-white checker pattern. The checkers are distorted into curved,
> wedge-like shapes where the spheres curve away, because the pattern is 3D and the sphere surfaces cut
> through it.

![Planet map](../images/ch24_planet_map.png)

> **Image description:** A flat 2:1 fantasy world map: irregular continents with green forests,
> brown-grey mountains with snowy peaks and sandy coastlines, surrounded by deep and shallow blue oceans,
> with white ice bands across the top and bottom edges.

![Planet](../images/ch24_planet.png)

> **Image description:** The map wrapped around a sphere floating in black space: a small planet,
> lit from the upper left by a distant sun, with a clear day side and a soft transition into the dark
> night side. Continents, oceans and a polar ice cap are visible.

![Perlin](../images/ch24_perlin.png)

> **Image description:** Two spheres on a marble ground plane. The ground has flowing grey-white
> **marble veins**. The left sphere (from our viewpoint) shows darker, crinkly **turbulence** like
> dirty stone, and the right sphere shows soft grey **smooth noise** blotches.

---

## Try it yourself

1. Make the checker squares smaller (`scale 0.1`) and bigger (`scale 2`).
2. Color the marble: multiply by a tint like `hex_color(0xE8D5B7)` (cream) or make green jade.
3. Make **wood**: `rings = fract(10 · sqrt(p.x² + p.z²) + 2·turbulence(p))`, then lerp between two
   browns. Use a `FunctionTexture`.
4. Change the planet's colors to make a **desert** or **lava** world.
5. Use an image of your own: save a photo as PPM (binary P6, 8-bit) and put it on a sphere.

## Common problems

| Symptom | Cause |
|---------|-------|
| Object is bright magenta | Texture file not found (check the path and run folder) |
| Image texture is upside down | v = 0 must be the **bottom** row |
| Texture looks too dark / washed out | sRGB not converted to linear when loading (or converted twice) |
| Visible seam on a sphere | Noise sampled on the flat map instead of on the sphere |

---

## Summary

* A texture maps a surface location (u, v and/or p) to a color.
* Solid (3D) textures use `p`; image textures use `(u, v)`.
* Sphere UVs come from spherical angles: `u = φ/2π`, `v = θ/π`.
* Perlin-based textures give clouds, smoke and marble for free.

Next: [Chapter 25 — Quads, triangles and meshes →](25-quads-triangles-meshes.md)
