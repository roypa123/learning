# Line by line: `ch24_textures.cpp`

[← Line-by-line index](README.md) · [Chapter 24 (the theory)](../24-textures.md) · [texture.h](texture.md)

**What the whole program does, in one sentence:** it renders three texture demos: checkered spheres, a planet (with a
world map that the program paints itself, saves, and loads back), and Perlin-noise textures (smooth, turbulent, marble).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. `make_camera` | 13–22 | a camera from position, target and field of view |
| C. `paint_planet_map` | 24–45 | paint a world map with noise |
| D. Checkered spheres | 48–55 | |
| E. The planet | 57–72 | paint, save, load back, render |
| F. Perlin textures | 74–84 | |
| G. End | 85–86 | |

---

## Block A — Comments, includes (lines 1–11)

Comments listing the images; `<cmath>`; our library; `using namespace pixel`.

---

## Block B — `make_camera` (lines 13–22)

```cpp
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
```

A helper: a camera with our usual settings, placed at `from`, looking at `at`, with field of view `vfov`.

---

## Block C — `paint_planet_map` (lines 24–45)

Paints a flat "world map" image (twice as wide as tall), like a map of the Earth, using noise.

```cpp
static Image paint_planet_map(int W, int H) {
    Image map(W, H);
    Perlin noise(99);
```

Lines 25–27: the map image and a Perlin noise generator (seed 99).

```cpp
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double lon = (double)x / W * 2 * pi;
            double lat = ((double)y / H - 0.5) * pi;
            Point3 p(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
```

* Lines 28–29: every map pixel.
* Line 31: **longitude**: the map's x goes once around the planet: 0 to 2π.
* Line 32: **latitude**: the map's y goes from −π/2 to +π/2 (from one pole to the other).
* Line 33: turn (longitude, latitude) into a point **on a ball** of radius 1 (the standard sphere formula). We sample
  the noise on the ball, not on the flat map, so the texture joins up seamlessly at the left/right edges and doesn't get
  stretched at the poles.

```cpp
            double h = noise.fbm(p * 2.2, 6);                  // -1 .. 1 "height"
            Color c;
```

Line 34: a "height" for this spot from 6-layer Perlin fBm (2.2 sets the size of the continents).

```cpp
            if (h < 0.0)       c = lerp(hex_color(0x0B3D91), hex_color(0x1E6FD9), clamp01(1 + h * 3));   // ocean
            else if (h < 0.05) c = hex_color(0xD8C690);                                                  // beach
            else if (h < 0.3)  c = lerp(hex_color(0x3B7D2A), hex_color(0x2A5A1E), h / 0.3);             // forest
            else               c = lerp(hex_color(0x7A6A58), hex_color(0xEEEEEE), clamp01((h - 0.3) * 3)); // mountains
```

Lines 36–39: a color by height, like a real map:

* below 0: **ocean**: deep blue for low values, lighter blue near the coast,
* 0 to 0.05: **beach** sand,
* 0.05 to 0.3: **forest** green, getting darker higher up,
* above 0.3: **mountains**: from brown-grey rock to white snow.

```cpp
            double polar = std::fabs(std::sin(lat));
            if (polar + 0.1 * h > 0.85) c = hex_color(0xF4F8FF);                                         // ice caps
            map.at(x, y) = c;
        }
    return map;
}
```

* Line 40: `polar` = 0 at the equator, 1 at the poles.
* Line 41: near the poles → white ice. Adding a bit of the height makes the ice edge irregular instead of a straight line.
* Line 42: store the pixel. Line 44: return the finished map.

---

## Block D — Checkered spheres (lines 48–55)

```cpp
        auto checker = std::make_shared<CheckerTexture>(0.32, Color(0.2, 0.3, 0.1), Color(0.9, 0.9, 0.9));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -10, 0), 10, std::make_shared<Lambertian>(checker)));
        world.add(std::make_shared<Sphere>(Point3(0, 10, 0), 10, std::make_shared<Lambertian>(checker)));
        save_image("images/ch24_checker.png", make_camera(Point3(13, 2, 3), Point3(0, 0, 0), 20).render(world));
```

* Line 50: a 3D checker texture (cubes of 0.32 units, dark green and white).
* Lines 52–53: two big spheres (radius 10) touching at the origin, one below and one above, both with the checker.
* Line 54: make a camera, render and save in one line.

---

## Block E — The planet (lines 57–72)

```cpp
        Image map = paint_planet_map(1024, 512);
        save_image("images/ch24_planet_map.png", map);
        save_image("images/ch24_planet_map.ppm", map);          // also as PPM...
        auto tex = std::make_shared<ImageTexture>("images/ch24_planet_map.ppm");   // ...and load it back
```

* Line 59: paint a 1024 × 512 map.
* Line 60: save it as PNG so you can look at it.
* Line 61: also save it as PPM, the format our loader can read.
* Line 62: load the PPM back as an **image texture**. (We could use `map` directly, but this shows loading a texture
  from a file.)

```cpp
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, 0, 0), 2, std::make_shared<Lambertian>(tex)));
        Camera cam = make_camera(Point3(0, 1.5, 12), Point3(0, 0, 0), 20);
        cam.background = solid_background(Color(0.01, 0.01, 0.02));
```

* Line 65: the planet: a sphere of radius 2 wearing the image texture (the sphere's u, v coordinates choose where on
  the map each point is).
* Lines 66–67: a camera in front, and an almost black background (space).

```cpp
        world.add(std::make_shared<Sphere>(Point3(-30, 10, 20), 8, std::make_shared<DiffuseLight>(Color(12, 11, 10))));
        cam.samples_per_pixel = 200;
        save_image("images/ch24_planet.png", cam.render(world));
```

* Line 69: the "sun": a big glowing sphere off to the left, up and toward the camera. It lights one side of the planet.
* Line 70: more samples (the only light is small, which makes more noise).
* Line 71: render and save.

---

## Block F — Perlin textures (lines 74–84)

```cpp
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, -2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(2.0, NoiseTexture::Smooth))));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 2.3), 2,
                  std::make_shared<Lambertian>(std::make_shared<NoiseTexture>(3.0, NoiseTexture::Turbulence))));
        save_image("images/ch24_perlin.png", make_camera(Point3(13, 3, 3), Point3(0, 1.5, 0), 30).render(world));
```

* Lines 77–78: a marble ground.
* Lines 79–80: a sphere with smooth noise.
* Lines 81–82: a sphere with turbulence.
* Line 83: render from the side and save.

Each line nests three `make_shared` calls: a texture, inside a material, inside a sphere.

---

## Block G — End (lines 85–86)

`return 0;` `}`

---

## Check your understanding

1. Why is the noise sampled on a ball instead of the flat map? *(So the map has no seam and no stretching at the poles.)*
2. What decides whether a map pixel is ocean or land? *(Whether the noise "height" is below or above 0.)*
3. Why is the planet map saved as PPM too? *(Our image texture loader reads PPM files.)*
