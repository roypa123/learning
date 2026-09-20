# Line by line: `ch37_sdf.cpp`

[← Line-by-line index](README.md) · [Chapter 37 (the theory)](../37-sdf-raymarching.md) · [sdf.h](sdf.md)

**What the whole program does, in one sentence:** it renders four shapes that are hard to make from triangles (a melted
blob, a cube with a bite taken out, a tilted ring, and an endless field of pillars), then renders the Mandelbulb
fractal.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–14 | |
| B. Floor | 17–21 | |
| C. (a) The blob | 23–32 | three spheres melted together |
| D. (b) The bitten cube | 34–42 | subtraction |
| E. (c) The tilted ring | 44–51 | a rotated torus |
| F. (d) Endless pillars | 53–59 | repetition |
| G. Sun, camera, save | 61–81 | |
| H. The Mandelbulb | 83–106 | |
| I. End | 107–108 | |

---

## Block A — Comments, includes (lines 1–14)

Comments listing the shapes, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — Floor (lines 17–21)

```cpp
        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-40, 0, -40), Vec3(80, 0, 0), Vec3(0, 0, 80),
                                         std::make_shared<Lambertian>(hex_color(0xB8B2A7))));
```

A big stone-colored floor quad. (An ordinary quad: SDFs and normal objects mix freely in one scene.)

---

## Block C — (a) The blob (lines 23–32)

```cpp
        auto blob = [](const Point3& p) {
            Point3 q = p - Point3(-2.2, 0.9, 0);
            double d1 = sdf::sphere(q - Vec3(0, 0, 0), 0.7);
            double d2 = sdf::sphere(q - Vec3(0.6, 0.5, 0.2), 0.5);
            double d3 = sdf::sphere(q - Vec3(-0.4, 0.6, -0.3), 0.45);
            return sdf::op_smooth_union(sdf::op_smooth_union(d1, d2, 0.35), d3, 0.35);
        };
```

* Line 24: a lambda: the distance from any point `p` to this shape.
* Line 25: move the whole shape: subtracting the center from `p` means "measure as if the shape were at the origin".
* Lines 26–28: three spheres of different sizes at slightly different places (each one is just "distance − radius").
* Line 29: melt them together in two steps with a blend size of 0.35. Where two spheres come close, the surface bulges
  out and joins smoothly, like drops of water merging.

```cpp
        world.add(std::make_shared<SDFObject>(blob, AABB(Point3(-3.4, 0, -1.2), Point3(-1.0, 2.2, 1.2)),
                                              std::make_shared<Plastic>(hex_color(0xE11D48), 0.2)));
```

Lines 31–32: wrap the function in an `SDFObject` with a **bounding box** around it (the renderer only marches inside
that box) and a glossy red plastic material.

---

## Block D — (b) The bitten cube (lines 34–42)

```cpp
        auto bitten = [](const Point3& p) {
            Point3 q = sdf::rotate_y(p - Point3(0, 0.8, 0), 0.6);
            double cube = sdf::round_box(q, Vec3(0.7, 0.7, 0.7), 0.12);
            double bite = sdf::sphere(q - Vec3(0.55, 0.55, 0.55), 0.6);
            return sdf::op_subtract(cube, bite);
        };
```

* Line 36: move **and rotate** the point (0.6 radians ≈ 34°). Rotating the point the opposite way rotates the shape.
* Line 37: a cube of half-size 0.7 with corners rounded by 0.12.
* Line 38: a sphere sitting at one corner.
* Line 39: **subtract** it: the cube with a smooth spherical bite. With triangles this would need real work; here it's
  one `max`.

---

## Block E — (c) The tilted ring (lines 44–51)

```cpp
        auto ring = [](const Point3& p) {
            Point3 q = p - Point3(2.3, 0.75, 0);
            Point3 tilted(q.x, q.y * std::cos(1.2) - q.z * std::sin(1.2), q.y * std::sin(1.2) + q.z * std::cos(1.2));
            return sdf::torus(tilted, 0.55, 0.18);
        };
```

* Line 46: move to the ring's position.
* Line 47: rotate around the **x** axis by 1.2 radians (≈ 69°), written out by hand (the library only has `rotate_y`),
  so the donut stands up instead of lying flat.
* Line 48: the torus: ring radius 0.55, tube radius 0.18.

---

## Block F — (d) Endless pillars (lines 53–59)

```cpp
        auto pillars = [](const Point3& p) {
            Point3 q = sdf::op_repeat_xz(p, 1.5);
            return sdf::capsule_y(q, 3.0, 0.2);
        };
        world.add(std::make_shared<SDFObject>(pillars, AABB(Point3(-20, 0, -20), Point3(20, 3.3, -3.0)),
                                              std::make_shared<Lambertian>(hex_color(0xE7E5E4))));
```

* Line 55: fold the point into a 1.5 × 1.5 cell.
* Line 56: **one** capsule, 3 units tall and 0.2 thick.
* Together: an infinite grid of pillars. The bounding box on line 58 limits where we actually draw them (a wide strip
  behind the other shapes).

---

## Block G — Sun, camera, save (lines 61–81)

```cpp
        Vec3 sun_dir = unit_vector(Vec3(-0.6, 0.7, 0.5));
        auto sun = make_sun(sun_dir, 1.0, Color(3000, 2800, 2500));
        world.add(sun);
        lights.add(sun);
        SkySettings sky;
        sky.sun_direction = sun_dir;
```

Lines 61–66: a sun up, to the left and slightly behind the camera, added to the world and the lights, with a matching
sky.

```cpp
        cam.max_sample_value = 50;
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        opt.exposure = 0.7;
        save_image("images/ch37_sdf_scene.png", cam.render(world, &lights), opt);
```

Lines 76–80: the usual outdoor setup: clamp fireflies, develop with ACES at a slightly lower exposure, render and save.

---

## Block H — The Mandelbulb (lines 83–106)

```cpp
        auto bulb = [](const Point3& p) { return sdf::mandelbulb(p, 12, 8.0); };
        world.add(std::make_shared<SDFObject>(bulb, AABB(Point3(-1.25, -1.25, -1.25), Point3(1.25, 1.25, 1.25)),
                                              std::make_shared<Plastic>(hex_color(0xF59E0B), 0.35),
                                              400, 2e-4, 0.9));
```

* Line 86: the fractal with 12 iterations and power 8.
* Lines 87–89: an `SDFObject` with a box of ±1.25 (the whole fractal fits inside), orange plastic, and **special
  settings**: up to 400 steps, a smaller epsilon (2e-4), and `step_scale = 0.9` (take 90% steps, because the fractal's
  distance is only an estimate and full steps could overshoot through thin parts).

```cpp
        auto key = std::make_shared<Sphere>(Point3(4, 5, 4), 1.2, std::make_shared<DiffuseLight>(Color(18, 16, 14)));
        world.add(key);
        lights.add(key);
```

Lines 90–92: a single glowing sphere as the light, up and to the front-right.

```cpp
        cam.aspect_ratio = 1.0;
        ...
        cam.background = gradient_sky(Color(0.05, 0.03, 0.08), Color(0.12, 0.15, 0.3));
```

Lines 95–102: a square image, a 30° lens looking at the fractal from slightly above, and a dark blue-purple background.

Line 105: render and save. This is the slowest image in the book: every marching step evaluates the fractal, which
itself loops 12 times.

---

## Block I — End (lines 107–108)

`return 0;` `}`

---

## Check your understanding

1. How do you move an SDF shape? *(Subtract the position from the point before measuring.)*
2. Why does each SDFObject need a bounding box? *(To know where to start and stop marching; outside it, the object is
   skipped.)*
3. What single line creates the endless pillars? *(`op_repeat_xz(p, 1.5)`.)*
4. Why does the Mandelbulb use `step_scale = 0.9`? *(Its distance is an estimate; smaller steps avoid stepping through
   the surface.)*
