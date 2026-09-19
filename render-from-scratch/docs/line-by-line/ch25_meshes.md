# Line by line: `ch25_meshes.cpp`

[← Line-by-line index](README.md) · [Chapter 25 (the theory)](../25-quads-triangles-meshes.md) · [quad.h](quad.md) · [triangle.h](triangle.md)

**What the whole program does, in one sentence:** it renders three scenes: five colored flat panels (quads), two donuts
made of triangles (one flat-shaded, one smooth-shaded, saved to and loaded from an OBJ file), and a terrain of 20,000
triangles.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. Scene 1: quads | 14–38 | five panels like an open box |
| C. Scene 2: donut meshes | 40–70 | make, save, load, render flat vs smooth |
| D. Scene 3: terrain | 72–102 | height function → mesh → colored by height |
| E. End | 103–104 | |

---

## Block A — Comments, includes (lines 1–11)

Comments (including that the program writes `models/torus.obj`), `<cmath>`, our library, `using namespace pixel`.

---

## Block B — Scene 1: quads (lines 14–38)

```cpp
        auto left_red     = std::make_shared<Lambertian>(Color(1.0, 0.2, 0.2));
        auto back_green   = std::make_shared<Lambertian>(Color(0.2, 1.0, 0.2));
        auto right_blue   = std::make_shared<Lambertian>(Color(0.2, 0.2, 1.0));
        auto upper_orange = std::make_shared<Lambertian>(Color(1.0, 0.5, 0.0));
        auto lower_teal   = std::make_shared<Lambertian>(Color(0.2, 0.8, 0.8));
```

Lines 16–20: five matte colors.

```cpp
        HittableList world;
        world.add(std::make_shared<Quad>(Point3(-3, -2, 5), Vec3(0, 0, -4), Vec3(0, 4, 0), left_red));
        world.add(std::make_shared<Quad>(Point3(-2, -2, 0), Vec3(4, 0, 0), Vec3(0, 4, 0), back_green));
        world.add(std::make_shared<Quad>(Point3(3, -2, 1), Vec3(0, 0, 4), Vec3(0, 4, 0), right_blue));
        world.add(std::make_shared<Quad>(Point3(-2, 3, 1), Vec3(4, 0, 0), Vec3(0, 0, 4), upper_orange));
        world.add(std::make_shared<Quad>(Point3(-2, -3, 5), Vec3(4, 0, 0), Vec3(0, 0, -4), lower_teal));
```

Lines 22–27: five quads, each given as (corner, edge u, edge v, material). All are 4 × 4 squares:

| Line | Position | Edges go along |
|------|----------|----------------|
| 23 | left wall at x = −3 | z and y |
| 24 | back wall at z = 0 | x and y |
| 25 | right wall at x = 3 | z and y |
| 26 | ceiling at y = 3 | x and z |
| 27 | floor at y = −3 | x and z |

```cpp
        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 80;
        cam.lookfrom = Point3(0, 0, 9);
        cam.lookat = Point3(0, 0, 0);
        save_image("images/ch25_quads.png", cam.render(world));
```

Lines 29–37: a square image, looking from z = 9 into the "box" with a wide 80° view. Render and save.

---

## Block C — Scene 2: donut meshes (lines 40–70)

```cpp
        Mesh torus = make_torus(1.0, 0.4, 48, 24);
        ensure_parent_folder("models/torus.obj");   // make the "models" folder if needed
        save_obj("models/torus.obj", torus);   // look at this file in a text editor!
```

* Line 42: make a donut mesh: ring radius 1, tube radius 0.4, a 48 × 24 grid (2,304 triangles).
* Line 43: create the `models` folder if needed (otherwise saving fails).
* Line 44: write it as an OBJ text file.

```cpp
        Mesh loaded;
        if (!load_obj("models/torus.obj", loaded)) return 1;
```

Lines 46–47: read it back from the file (to test our OBJ reader). If that fails, stop with error code 1.

```cpp
        Mesh flat = loaded;
        flat.normals.clear();
```

Lines 50–51: a copy **without** per-corner normals → it will be drawn with flat, faceted shading.

```cpp
        HittableList world;
        auto ground = std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(0.5, Color(0.8, 0.8, 0.8), Color(0.3, 0.3, 0.3)));
        world.add(std::make_shared<Quad>(Point3(-20, -0.4, -20), Vec3(40, 0, 0), Vec3(0, 0, 40), ground));
```

Lines 53–55: a big checkered floor quad at y = −0.4 (so the donuts, whose tube radius is 0.4, sit on it).

```cpp
        auto red = std::make_shared<Lambertian>(Color(0.8, 0.15, 0.1));
        auto blue = std::make_shared<Lambertian>(Color(0.1, 0.3, 0.8));
        world.add(std::make_shared<Translate>(flat.build(red), Vec3(-1.5, 0, 0)));
        world.add(std::make_shared<Translate>(loaded.build(blue), Vec3(1.5, 0, 0)));
```

* Lines 57–58: two colors.
* Line 59: `flat.build(red)` turns the mesh into a BVH of triangles; `Translate` moves it 1.5 to the left (chapter 27).
* Line 60: the smooth one, moved 1.5 to the right.

Lines 62–69: a camera above and in front, looking down at the donuts; render and save.

---

## Block D — Scene 3: terrain (lines 72–102)

```cpp
        Perlin noise(3);
        auto height = [&noise](double x, double z) {
            double h = noise.fbm(Point3(x * 0.15, 0, z * 0.15), 6);
            return 3.0 * h + 0.6 * std::exp(-(x * x + z * z) / 20.0) * 3.0;  // + a central mountain
        };
```

* Line 74: a Perlin noise generator.
* Line 75: `height` = a lambda: the ground height at (x, z). `[&noise]` = it uses the noise object.
* Line 76: hills from fBm noise (× 0.15 makes them wide).
* Line 77: plus a **mountain in the middle**: `exp(−distance²/20)` is 1 at the center and falls smoothly to 0 (a bell
  shape), scaled to 1.8 units high.

```cpp
        Mesh terrain = make_heightfield(24, 100, height);   // 100x100 grid = 20,000 triangles
        std::printf("Terrain has %zu triangles\n", terrain.triangle_count());
```

Lines 79–80: build a 24 × 24 area as a 100 × 100 grid using our height function, and print its triangle count.

```cpp
        auto terrain_color = std::make_shared<FunctionTexture>([](double, double, const Point3& p) {
            if (p.y < -0.4) return hex_color(0xC2B280);
            if (p.y < 0.8)  return hex_color(0x4F7942);
            if (p.y < 1.8)  return hex_color(0x7D7461);
            return hex_color(0xF5F5F5);
        });
```

Lines 83–88: a texture from a formula: color by **height**: sand (low), grass, rock, snow (high).

```cpp
        HittableList world;
        world.add(terrain.build(std::make_shared<Lambertian>(terrain_color)));
        auto water = std::make_shared<Metal>(hex_color(0x3A6EA5), 0.05);
        world.add(std::make_shared<Quad>(Point3(-12, -0.8, -12), Vec3(24, 0, 0), Vec3(0, 0, 24), water));
```

* Line 90: the terrain as a BVH of triangles, with a matte material using that texture.
* Lines 91–92: "water": a flat quad at height −0.8 with a blue, slightly fuzzy metal material (it reflects like calm
  water). Everything below −0.8 is under water.

Lines 94–101: a camera high up at a corner, looking at the center; render and save.

---

## Block E — End (lines 103–104)

`return 0;` `}`

---

## Check your understanding

1. Why is the donut saved and then loaded again? *(To show the OBJ writer and reader working.)*
2. What makes the red donut look faceted? *(Its normals were cleared, so each triangle has one flat normal.)*
3. What creates the mountain in the middle? *(The `exp(−(x² + z²)/20)` bell shape.)*
