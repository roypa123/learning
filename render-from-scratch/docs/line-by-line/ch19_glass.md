# Line by line: `ch19_glass.cpp`

[← Line-by-line index](README.md) · [Chapter 19 (the theory)](../19-glass.md) · [material.h: Dielectric](material.md)

**What the whole program does, in one sentence:** it renders a solid glass ball, then a hollow one (a bubble), then
four balls with different "indices of refraction" (air, water, glass, diamond) to compare how much they bend light.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. `base_camera` | 11–17 | shared camera settings |
| C. Materials | 19–24 | matte, glass, air bubble, gold |
| D. Solid glass, then hollow | 26–38 | two renders |
| E. Four IORs | 40–54 | four glass balls on a checkerboard |
| F. End | 55–56 | |

---

## Block A — Comments, includes (lines 1–9)

Comments listing the three images; our library; `using namespace pixel`.

---

## Block B — `base_camera` (lines 11–17)

```cpp
static Camera base_camera() {
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    return cam;
}
```

A helper that returns a camera with the settings we use for all three images, so we don't repeat them.

---

## Block C — Materials (lines 19–24)

```cpp
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);   // air inside glass
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 0.0);
```

* Line 22: **glass**, index of refraction 1.5.
* Line 23: the **bubble**: air (1.0) inside glass (1.5). Seen from inside the glass, air has a relative index of
  1.0 / 1.5. This inner surface is what makes the ball hollow.
* The others are the matte ground and ball, and a gold mirror.

---

## Block D — Solid glass, then hollow (lines 26–38)

```cpp
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
        world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
        world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
        world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));
        save_image("images/ch19_glass.png", base_camera().render(world));
```

* Lines 28–32: the chapter 18 scene, with a **glass** ball on the left.
* Line 33: make a camera, render, and save, all in one line. `base_camera()` returns a camera, `.render(world)` is
  called on it directly.

```cpp
        world.add(std::make_shared<Sphere>(Point3(-1.0, 0.0, -1.0), 0.4, bubble));
        save_image("images/ch19_hollow.png", base_camera().render(world));
```

* Line 36: add a slightly smaller sphere (radius 0.4) at the **same center**, made of "air in glass". Now the glass is
  only a 0.1-thick shell.
* Line 37: render and save again.

---

## Block E — Four IORs (lines 40–54)

```cpp
        auto checker = std::make_shared<CheckerTexture>(0.25, Color(0.9, 0.9, 0.9), Color(0.15, 0.15, 0.15));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, std::make_shared<Lambertian>(checker)));
```

* Line 42: a black-and-white checker texture (0.25-unit squares). The pattern helps you see how the balls bend the
  view behind them.
* Line 44: a checkered ground.

```cpp
        double iors[4] = {1.0, 1.33, 1.5, 2.4};   // air, water, glass, diamond
        for (int k = 0; k < 4; k++)
            world.add(std::make_shared<Sphere>(Point3(-1.5 + k, 0, -1.6), 0.45,
                                               std::make_shared<Dielectric>(iors[k])));
```

* Line 45: four indices of refraction.
* Lines 46–48: four balls in a row at x = −1.5, −0.5, 0.5, 1.5, each with its own Dielectric material.

```cpp
        Camera cam = base_camera();
        cam.lookfrom = Point3(0, 0.3, 1);
        cam.lookat = Point3(0, 0, -1.6);
        cam.vfov = 60;
        save_image("images/ch19_ior.png", cam.render(world));
```

Lines 49–53: a camera placed a bit higher and farther back, aimed at the row, with a narrower 60° view (chapter 20
explains these settings). Render and save.

---

## Block F — End (lines 55–56)

`return 0;` `}`

---

## Check your understanding

1. Why is the bubble's index 1/1.5 instead of 1.0? *(From inside the glass, light goes from glass (1.5) into air (1.0):
   the ratio is what matters.)*
2. Why do both spheres in the hollow ball have the same center? *(The inner one sits exactly inside the outer one, making a
   shell.)*
3. Which ball bends the view the most? *(Diamond, 2.4.)*
