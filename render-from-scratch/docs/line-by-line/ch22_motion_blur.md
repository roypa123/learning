# Line by line: `ch22_motion_blur.cpp`

[← Line-by-line index](README.md) · [Chapter 22 (the theory)](../22-motion-blur.md) · [sphere.h: moving spheres](sphere.md)

**What the whole program does, in one sentence:** it renders a field of small balls that jump upward while the "shutter
is open", plus a big red ball flying sideways, so they appear blurred like in a photo of fast motion.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. Ground | 11–14 | checkered floor |
| C. Moving small balls | 16–28 | a grid of balls, each with a start and end position |
| D. The big flying ball | 29–31 | |
| E. BVH, camera, render | 33–43 | |

---

## Block A — Comments, includes (lines 1–9)

Comments explaining the idea (each ray has a random time), our library, `using namespace pixel`.

---

## Block B — Ground (lines 11–14)

```cpp
    HittableList world;
    auto checker = std::make_shared<CheckerTexture>(0.5, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(checker)));
```

A huge ground ball with a dark-green and white checker texture (squares of 0.5 units). The pattern makes motion easy to see.

---

## Block C — Moving small balls (lines 16–28)

```cpp
    Pcg32 rng(11);
    for (int a = -6; a < 6; a++) {
        for (int b = -6; b < 6; b++) {
            Point3 center(a + 0.9 * rng.next_double(), 0.2, b + 0.9 * rng.next_double());
```

* Line 16: a random generator with a fixed seed.
* Lines 17–18: a 12 × 12 grid.
* Line 19: a randomly shifted position on the floor (height 0.2 = the radius).

```cpp
            Color albedo(rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double(),
                         rng.next_double() * rng.next_double());
            auto mat = std::make_shared<Lambertian>(albedo);
```

Lines 20–23: a random (mostly dark, rich) matte color, as in chapter 21.

```cpp
            Point3 center2 = center + Vec3(0, 0.5 * rng.next_double(), 0);
            world.add(std::make_shared<Sphere>(center, center2, 0.2, mat));
        }
    }
```

* Line 25: the position at the **end** of the shutter time: moved up by a random amount (0–0.5).
* Line 26: the **moving-sphere** constructor: at time 0 it's at `center`, at time 1 at `center2`. Each ray has a random
  time, so different rays see the ball at different heights, and the average is a vertical smear.

---

## Block D — The big flying ball (lines 29–31)

```cpp
    world.add(std::make_shared<Sphere>(Point3(-0.8, 1, 0), Point3(0.8, 1, 0), 1.0,
                                       std::make_shared<Lambertian>(Color(0.8, 0.2, 0.1))));
```

A red ball of radius 1 moving 1.6 units sideways (from x = −0.8 to x = 0.8) during the shutter: a strong horizontal blur.

---

## Block E — BVH, camera, render (lines 33–43)

```cpp
    BVHNode bvh(world);
    Camera cam;
    cam.image_width = 600;
    cam.samples_per_pixel = 100;
    cam.max_depth = 20;
    cam.vfov = 25;
    cam.lookfrom = Point3(13, 3, 3);
    cam.lookat = Point3(0, 0.5, 0);
    save_image("images/ch22_motion_blur.png", cam.render(bvh));
    return 0;
}
```

* Line 33: a BVH for speed. (The moving spheres' boxes cover their whole path, so nothing gets cut off.)
* Lines 34–40: 600 pixels wide, 100 samples (motion blur needs many samples to look smooth), a 25° view from far away.
* Line 41: render and save.

The camera gives each ray a random time automatically (`get_ray` in [camera.md](camera.md)); nothing else is needed.

---

## Check your understanding

1. What makes a sphere "moving"? *(Two centers; its position depends on the ray's time.)*
2. Why do the small balls blur vertically? *(Their end position is higher than their start position.)*
3. Why use 100 samples? *(Each sample sees one moment in time; many are needed for a smooth blur.)*
