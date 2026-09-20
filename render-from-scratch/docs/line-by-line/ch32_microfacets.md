# Line by line: `ch32_microfacets.cpp`

[← Line-by-line index](README.md) · [Chapter 32 (the theory)](../32-microfacet-materials.md) · [material.h: GGX](material.md)

**What the whole program does, in one sentence:** it builds a "material chart": three rows of six spheres (gold, copper
and red plastic) going from perfectly smooth to very rough, lit like a photo studio.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. Studio floor | 13–18 | a dark checkered floor |
| C. The 18 spheres | 20–30 | three rows, roughness 0 → 1 |
| D. Two softbox lights | 32–39 | key light and rim light |
| E. Camera | 41–50 | |
| F. Save with ACES | 52–56 | |

---

## Block A — Comments, includes (lines 1–11)

Comments describing the three rows, our library, `using namespace pixel`.

---

## Block B — Studio floor (lines 13–18)

```cpp
    HittableList world, lights;

    auto floor_mat = std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(0.5, Color(0.35, 0.35, 0.35), Color(0.25, 0.25, 0.25)));
    world.add(std::make_shared<Quad>(Point3(-50, -0.5, -50), Vec3(100, 0, 0), Vec3(0, 0, 100), floor_mat));
```

* Line 14: two lists: the scene, and the lights to aim at.
* Line 17: a matte floor with a subtle dark checker (two close greys), so reflections have something to show.
* Line 18: a big 100 × 100 floor quad at y = −0.5 (the spheres have radius 0.5, so the bottom row sits on it).

---

## Block C — The 18 spheres (lines 20–30)

```cpp
    const Color gold(1.000, 0.766, 0.336);
    const Color copper(0.955, 0.638, 0.538);
    const int N = 6;
```

* Lines 21–22: the **measured** head-on reflectance (F0) of real gold and copper, in linear RGB. Metals reflect colors
  differently: that's where their color comes from.
* Line 23: six spheres per row.

```cpp
    for (int i = 0; i < N; i++) {
        double roughness = (double)i / (N - 1);
        double x = (i - (N - 1) / 2.0) * 1.15;
```

* Line 25: `roughness` goes 0, 0.2, 0.4, 0.6, 0.8, 1.0 (i ÷ 5).
* Line 26: the x position: `i − 2.5` times 1.15, so the six spheres are centered around x = 0 with 1.15 spacing.

```cpp
        world.add(std::make_shared<Sphere>(Point3(x, 2.3, 0), 0.5, std::make_shared<RoughMetal>(gold, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 1.15, 0), 0.5, std::make_shared<RoughMetal>(copper, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 0.0, 0), 0.5, std::make_shared<Plastic>(Color(0.6, 0.05, 0.05), roughness)));
    }
```

Lines 27–29: three spheres in this column:

* top row (y = 2.3): **gold**, physically based GGX metal,
* middle row (y = 1.15): **copper**,
* bottom row (y = 0): **red plastic** (a matte red base under a clear glossy coat).

All 18 share the same radius; only the material differs.

---

## Block D — Two softbox lights (lines 32–39)

```cpp
    auto softbox = std::make_shared<DiffuseLight>(Color(6, 6, 6));
    auto key = std::make_shared<Quad>(Point3(-6, 6, 4), Vec3(4, 0, 0), Vec3(0, -2, 2), softbox);   // faces down-back
    auto rim = std::make_shared<Quad>(Point3(4, 5, -6), Vec3(3, 0, 0), Vec3(0, 2, 2), softbox);
    world.add(key);
    world.add(rim);
    lights.add(key);
    lights.add(rim);
```

* Line 33: one light material (brightness 6) shared by both panels.
* Line 34: the **key light**: a big panel up and to the left, in front; its edges are chosen so its front faces down and
  back, toward the spheres. It creates the main highlights.
* Line 35: the **rim light**: behind and to the right, facing down and forward. It puts bright edges on the spheres,
  which separates them from the background (a classic studio trick).
* Lines 36–39: each panel is added **twice**: to `world` (so it glows and can be seen) and to `lights` (so the renderer
  aims rays at it).

---

## Block E — Camera (lines 41–50)

```cpp
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 800;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 32;
    cam.lookfrom = Point3(0, 1.6, 10);
    cam.lookat = Point3(0, 1.1, 0);
    cam.background = gradient_sky(Color(0.05, 0.05, 0.06), Color(0.2, 0.22, 0.25));   // dim studio walls
    cam.max_sample_value = 30;   // tame rare fireflies from tiny highlights
```

* Lines 42–46: a wide image, 128 samples, a 32° lens.
* Lines 47–48: the camera stands in front at the height of the middle row.
* Line 49: instead of a bright sky, a very dim grey gradient: like the walls of a dark studio. Almost all light comes
  from the two panels.
* Line 50: clamp very bright single samples. A tiny, sharp highlight reflected in a near-mirror sphere can produce
  extreme values ("fireflies"); this keeps them from making white dots.

---

## Block F — Save with ACES (lines 52–56)

```cpp
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = 1.4;
    save_image("images/ch32_material_chart.png", cam.render(world, &lights), opt);
    return 0;
```

* Lines 52–54: the highlights are far brighter than 1, so we develop the image with the filmic **ACES** curve
  (chapter 34) and brighten it a little (exposure 1.4).
* Line 55: render **with** the lights list, and save.

---

## Check your understanding

1. What does roughness 0 look like, and 1? *(A perfect mirror; a soft, satin surface with a broad highlight.)*
2. Why are gold and copper given colored F0 values? *(A metal's color comes from reflecting some wavelengths more.)*
3. Why is each light added to both lists? *(`world` makes it glow and visible; `lights` lets rays aim at it.)*
4. Why is `max_sample_value` used here? *(Small bright lights reflected in smooth spheres cause fireflies.)*
