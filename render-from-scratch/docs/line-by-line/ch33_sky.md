# Line by line: `ch33_sky.cpp`

[← Line-by-line index](README.md) · [Chapter 33 (the theory)](../33-sky-and-environment.md) · [sky.h](sky.md)

**What the whole program does, in one sentence:** it renders the same small scene four times under different skies
(noon, golden hour, blue hour, and a painted 360° studio), to show that the sky is the main light source outdoors.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–13 | |
| B. `build_scene` | 15–25 | four objects on a big ground |
| C. `make_camera` | 27–37 | shared camera settings |
| D. `render_with` | 39–59 | place the sun, set the sky, render, save |
| E. Noon | 62–71 | |
| F. Golden hour | 72–82 | |
| G. Blue hour | 83–93 | |
| H. Environment map | 94–117 | paint a 360° image and light with it |
| I. End | 118–119 | |

---

## Block A — Comments, includes (lines 1–13)

Comments listing the four images, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — `build_scene` (lines 15–25)

```cpp
static void build_scene(HittableList& world) {
    auto ground = std::make_shared<Lambertian>(hex_color(0x8A8A80));
    world.add(std::make_shared<Quad>(Point3(-100, 0, -100), Vec3(200, 0, 0), Vec3(0, 0, 200), ground));
```

Lines 16–17: a huge grey matte ground quad (200 × 200), so the horizon is far away.

```cpp
    world.add(std::make_shared<Sphere>(Point3(-1.3, 0.7, 0), 0.7, std::make_shared<Lambertian>(Color(0.8, 0.8, 0.8))));
    world.add(std::make_shared<Sphere>(Point3(0.3, 0.7, -0.6), 0.7, std::make_shared<RoughMetal>(Color(0.95, 0.93, 0.88), 0.15)));
    world.add(std::make_shared<Sphere>(Point3(1.6, 0.5, 0.6), 0.5, std::make_shared<Dielectric>(1.5)));
```

Lines 18–20: three balls, each showing the light differently:

* a **white matte** ball: shows the overall light and the color of the shadows,
* a **polished metal** ball (roughness 0.15): mirrors the sky and the sun,
* a **glass** ball: bends the light and makes a bright spot on the ground.

```cpp
    std::shared_ptr<Hittable> box = make_box(Point3(-0.4, 0, -0.4), Point3(0.4, 1.6, 0.4),
                                             std::make_shared<Plastic>(hex_color(0x2563EB), 0.3));
    box = std::make_shared<RotateY>(box, 30);
    world.add(std::make_shared<Translate>(box, Vec3(-0.2, 0, -2.2)));
```

Lines 21–24: a tall blue plastic block, rotated 30° and moved to the back: it casts a long shadow, which makes the
sun's height easy to see.

---

## Block C — `make_camera` (lines 27–37)

A 640-pixel-wide camera, 128 samples, standing at eye height 1.6 and looking slightly down at the objects.
`max_sample_value = 50` (line 35) keeps a very bright sun reflection from producing white dots.

---

## Block D — `render_with` (lines 39–59)

```cpp
static void render_with(const char* file, SkySettings sky, double sun_elevation_deg, double sun_azimuth_deg,
                        Color sun_radiance, double exposure) {
```

Lines 40–41: one function for all three outdoor images. Inputs: the output file, the sky settings, where the sun is
(**elevation** = how high above the horizon, **azimuth** = the compass direction), how bright it is, and the exposure.

```cpp
    HittableList world, lights;
    build_scene(world);
    double el = degrees_to_radians(sun_elevation_deg), az = degrees_to_radians(sun_azimuth_deg);
    Vec3 sun_dir(std::cos(el) * std::sin(az), std::sin(el), -std::cos(el) * std::cos(az));
    sky.sun_direction = sun_dir;
```

* Lines 42–43: the scene, plus an empty lights list.
* Line 44: angles in radians.
* Line 45: turn the two angles into a **direction**: `sin(elevation)` is the height (y); the rest is spread over x and z
  by the azimuth. At elevation 90° this is straight up (0, 1, 0).
* Line 46: the sky's glow must point the same way as the sun.

```cpp
    if (sun_radiance.max_component() > 0) {     // a sun below the horizon adds nothing
        auto sun = make_sun(sun_dir, 0.8, sun_radiance);
        world.add(sun);
        lights.add(sun);
    }
```

* Line 47: skip the sun entirely if its brightness is zero (used for blue hour).
* Line 48: `make_sun` builds a far-away glowing sphere covering 0.8° (see [sky.md](sky.md)).
* Lines 49–50: add it to the world (so it lights and can be seen) **and** to the lights (so rays aim at it: crisp,
  clean shadows).

```cpp
    Camera cam = make_camera();
    cam.background = physical_sky(sky);
    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = exposure;
    save_image(file, cam.render(world, &lights), opt);
```

Lines 53–58: the camera with this sky, developed with the filmic ACES curve and the given exposure (sun values in the
thousands need tone mapping).

---

## Block E — Noon (lines 62–71)

```cpp
        SkySettings s;
        s.zenith = Color(0.15, 0.35, 0.85);
        s.horizon = Color(0.65, 0.78, 0.95);
        s.sun_glow = Color(1.0, 0.95, 0.85);
        s.glow_strength = 0.3;
        s.intensity = 1.0;
        render_with("images/ch33_noon.png", s, 60, 40, Color(8000, 7700, 7200), 0.6);
```

* Lines 65–66: a deep blue zenith, pale blue horizon: a clear midday sky.
* Lines 67–68: a small, nearly white glow around the sun.
* Line 70: the sun stands **60° high**, almost white and very bright (8000). Short, hard shadows. Exposure 0.6 because the
  scene is bright.

---

## Block F — Golden hour (lines 72–82)

```cpp
        s.zenith = Color(0.12, 0.22, 0.55);
        s.horizon = Color(1.0, 0.62, 0.35);
        s.ground = Color(0.2, 0.12, 0.08);
        s.sun_glow = Color(1.0, 0.45, 0.15);
        s.glow_strength = 1.5;
        s.intensity = 0.8;
        render_with("images/ch33_golden_hour.png", s, 6, 60, Color(6000, 3000, 1100), 0.8);
```

* Line 76: an **orange** horizon.
* Lines 78–79: a strong, warm halo around the sun.
* Line 81: the sun is only **6°** above the horizon and strongly orange (much more red than blue), because low sunlight
  passes through a lot of air. Long shadows, warm light on one side and cool blue sky light on the other.

---

## Block G — Blue hour (lines 83–93)

```cpp
        s.zenith = Color(0.03, 0.05, 0.18);
        s.horizon = Color(0.25, 0.28, 0.55);
        ...
        render_with("images/ch33_blue_hour.png", s, -4, 70, Color(0, 0, 0), 2.5);
```

Line 92: elevation **−4°**: the sun is **below** the horizon, so its brightness is zero (no sun object is created). All
light comes from the dim blue sky, so there are no hard shadows, and the exposure is raised to 2.5 to see anything.

---

## Block H — Environment map (lines 94–117)

```cpp
        auto env = std::make_shared<Image>(512, 256);
        for (int y = 0; y < env->height; y++)
            for (int x = 0; x < env->width; x++) {
                double u = (x + 0.5) / env->width, v = (y + 0.5) / env->height;   // v: 0 = top
                Color c = lerp(Color(0.6, 0.6, 0.65), Color(0.15, 0.13, 0.12), v);   // bright top, dark floor
```

* Line 96: a 512 × 256 image that will hold the whole 360° surroundings ("latitude–longitude" layout).
* Lines 97–99: every pixel; `u` goes around, `v` goes from top (0) to bottom (1).
* Line 100: a simple gradient: brighter above, darker below, like a room with a bright ceiling.

```cpp
                for (int k = 0; k < 3; k++) {
                    double cu = k / 3.0 + 0.1;
                    if (std::fabs(u - cu) < 0.05 && v > 0.25 && v < 0.45) c = Color(8, 7.5, 7);
                }
                env->at(x, y) = c;
            }
```

* Lines 102–105: three bright rectangles at 1/3 intervals around the room, a little above the middle: "windows" or
  studio softboxes, with a value of 8 (an environment map is HDR: values can be far above 1).
* Line 106: store the pixel.

```cpp
        save_image("images/ch33_env_map_texture.png", *env, SaveOptions{0.3, ToneMapper::Aces, true});
```

Line 108: save the map itself so you can look at it. The options are written in one go:
`{exposure 0.3, ACES, sRGB on}` (dimmed, because the windows are very bright).

```cpp
        HittableList world;
        build_scene(world);
        Camera cam = make_camera();
        cam.background = environment_map(env, 1.0, 0.0);
        SaveOptions opt;
        opt.tonemap = ToneMapper::Aces;
        save_image("images/ch33_env_map.png", cam.render(world), opt);
```

* Line 113: use the painted image as the background **and** the light.
* Line 116: render. Note: **no lights list** here, because we don't have a way to aim rays at bright parts of an image
  (a good exercise, mentioned in chapter 33). Shadows come out softer and slightly noisier.

---

## Block I — End (lines 118–119)

`return 0;` `}`

---

## Check your understanding

1. What do elevation and azimuth mean? *(How high the sun is, and which compass direction it is in.)*
2. Why is the sun added to both `world` and `lights`? *(To glow and be visible, and to be aimed at.)*
3. Why is the blue-hour exposure 2.5? *(There is no sun: the scene is dim, so we brighten it when developing.)*
4. Why can values in the environment map be 8? *(It is an HDR image: light can be far brighter than white.)*
