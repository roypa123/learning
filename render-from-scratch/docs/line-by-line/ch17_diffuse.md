# Line by line: `ch17_diffuse.cpp`

[← Line-by-line index](README.md) · [Chapter 17 (the theory)](../17-diffuse-materials.md) · [camera.h](camera.md) · [material.h](material.md)

**What the whole program does, in one sentence:** it renders a grey matte ball on grey ground with the library's
`Camera` (real light bounces for the first time), and saves four comparison images: correct, no gamma, shadow acne,
and different numbers of bounces.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. The world | 13–17 | a material and two spheres |
| C. Camera settings | 19–24 | size, samples, bounces, sky |
| D. Normal render + no-gamma version | 26–31 | |
| E. Shadow acne demo | 33–37 | a copy of the camera with `t_min = 0` |
| F. Bounces comparison | 39–50 | four small renders side by side |
| G. End | 51–52 | |

---

## Block A — Comments, includes (lines 1–11)

Comments listing the four output images; our library; `using namespace pixel`.

---

## Block B — The world (lines 13–17)

```cpp
    auto grey = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, grey));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, grey));
```

* Line 14: one **matte grey material** (reflects 50% of each color). `auto` lets the compiler figure out the type
  (`std::shared_ptr<Lambertian>`).
* Line 15: an empty world.
* Lines 16–17: the small ball and the big "ground" ball, both using the **same** material (it's shared).

---

## Block C — Camera settings (lines 19–24)

```cpp
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = gradient_sky();
```

* Line 19: a camera with all default settings (at the origin, looking down −z).
* Lines 20–24: change what we need: widescreen, 400 pixels wide, **100 rays per pixel** (for smooth results), up to 50
  bounces, and the blue-white sky (which is the only light in this scene).

---

## Block D — Normal render + no-gamma version (lines 26–31)

```cpp
    Image img = cam.render(world);
    save_image("images/ch17_diffuse.png", img);
```

* Line 26: **render!** The camera traces 400 × 225 × 100 = 9 million rays, on all CPU cores, printing progress.
* Line 27: save normally (with the sRGB curve).

```cpp
    SaveOptions no_gamma;
    no_gamma.srgb = false;
    save_image("images/ch17_no_gamma.png", img, no_gamma);
```

Lines 29–31: save the **same** image without the sRGB curve. It looks too dark: a reminder of why gamma matters.

---

## Block E — Shadow acne demo (lines 33–37)

```cpp
    Camera acne = cam;
    acne.t_min = 0.0;
    acne.russian_roulette = false;
    save_image("images/ch17_shadow_acne.png", acne.render(world));
```

* Line 34: `acne` = a **copy** of the camera with all its settings.
* Line 35: accept hits even at distance 0. Because of rounding errors, bounced rays often hit the surface they just
  left, which makes dark speckles ("shadow acne").
* Line 36: switch off Russian roulette, so that only one thing changes compared with the normal image.
* Line 37: render and save in one line.

---

## Block F — Bounces comparison (lines 39–50)

```cpp
    Camera small = cam;
    small.image_width = 200;
    small.show_progress = false;
    Image strip;
    int depths[4] = {1, 2, 3, 50};
```

* Lines 40–42: another copy, half the size, without progress output.
* Line 43: `strip` will hold the images side by side (it starts empty).
* Line 44: the four `max_depth` values to try.

```cpp
    for (int k = 0; k < 4; k++) {
        small.max_depth = depths[k];
        Image part = small.render(world);
        strip = (k == 0) ? part : post::side_by_side(strip, part, 4);
    }
    save_image("images/ch17_bounces.png", strip);
```

* Line 45: 4 rounds.
* Line 46: set the bounce limit.
* Line 47: render a small image.
* Line 48: the first image starts the strip; each later one is attached on the right (4-pixel gap).
* Line 50: save the strip.

With 1 bounce, objects are black (a ray that hits a surface can't continue to the sky); with more bounces they get
lighter.

---

## Block G — End (lines 51–52)

`return 0;` `}`

---

## Check your understanding

1. How many rays does line 26 trace? *(400 × 225 × 100 = 9,000,000 camera rays, plus their bounces.)*
2. Why does `acne` switch off Russian roulette too? *(So only `t_min` differs, and the comparison is fair.)*
3. Why are objects black with `max_depth = 1`? *(The ray stops at the first hit and never reaches the sky, the only light.)*
