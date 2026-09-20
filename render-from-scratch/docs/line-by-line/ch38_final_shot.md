# Line by line: `ch38_final_shot.cpp`

[← Line-by-line index](README.md) · [Chapter 38 (the theory)](../38-final-shot.md)

**What the whole program does, in one sentence:** it builds and renders the book's final cinematic frame, "The Monolith
at Dawn", using almost every feature of the library, and finishes it with a full post-production pipeline.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–19 | |
| B. Landscape constants and `terrain_height` | 21–39 | the shape of the land |
| C. Quality presets | 41–47 | draft / preview / final |
| D. The terrain mesh and its texture | 49–67 | 144,000 triangles |
| E. Water | 69–71 | |
| F. The monolith | 73–78 | |
| G. Glowing orbs | 80–87 | |
| H. Rocks | 89–101 | |
| I. The person (SDF) | 103–118 | |
| J. Sun and sky | 120–133 | |
| K. Atmosphere (haze) | 135–137 | |
| L. Camera and render | 139–157 | |
| M. Post-production | 159–176 | |
| N. End | 177–178 | |

---

## Block A — Comments, includes (lines 1–19)

Comments listing everything that goes into the shot and how to run the three quality presets; `<cmath>`, `<string>`,
our library, `using namespace pixel`.

---

## Block B — Landscape constants and `terrain_height` (lines 21–39)

```cpp
const double WATER_LEVEL = -0.35;
const Point3 MONOLITH_POS(0, 0, -8);
```

Lines 22–23: two constants used by several parts of the program: the height of the lake surface and where the monolith
stands.

```cpp
double terrain_height(double x, double z) {
    double dunes = 3.0 * (fbm_2d(x * 0.02 + 11.0, z * 0.02 + 3.0, 5) - 0.5) * 2.0
                 + 0.8 * (fbm_2d(x * 0.08 + 5.0, z * 0.08, 4) - 0.5);
```

* Line 25: the ground height at any (x, z). Everything about the landscape comes from this one function.
* Lines 27–28: **dunes**: big slow waves (× 0.02) plus smaller ones (× 0.08). `fbm_2d` returns 0–1, so `− 0.5` centers
  it around zero. The `+ 11.0` and `+ 5.0` just pick a different part of the noise.

```cpp
    double dx = x - MONOLITH_POS.x, dz = z - MONOLITH_POS.z;
    double d = std::sqrt(dx * dx + dz * dz);
    double h = dunes * smoothstep(5.0, 30.0, d);
```

Lines 30–32: the distance to the monolith, and a **flat plateau** around it: within 5 units the dunes are multiplied by
0 (perfectly flat), and beyond 30 units by 1 (full dunes), with a smooth change between.

```cpp
    h -= 1.4 * std::exp(-((x - 3) * (x - 3) / 60.0 + (z - 1) * (z - 1) / 20.0));
```

Line 34: dig the **lake basin**: a smooth bell-shaped dip, 1.4 deep, centered at (3, 1), wider in x than in z.

```cpp
    double far = smoothstep(70.0, 160.0, -z);
    h += far * 18.0 * fbm_2d(x * 0.012 + 40.0, z * 0.012, 6);
    return h;
```

Lines 36–38: far away (starting 70 units behind, fully at 160), add tall, slow noise: the **distant mountains**.

---

## Block C — Quality presets (lines 41–47)

```cpp
struct Preset { std::string name; int width; int spp; bool denoise; };

int main(int argc, char** argv) {
    Preset preset = {"preview", 960, 32, true};
    if (argc > 1 && std::string(argv[1]) == "draft") preset = {"draft", 480, 16, true};
    if (argc > 1 && std::string(argv[1]) == "final") preset = {"final", 1920, 256, true};
    std::printf("Preset: %s (%d px wide, %d samples per pixel)\n", preset.name.c_str(), preset.width, preset.spp);
```

* Line 41: a small struct holding one quality setting.
* Lines 44–46: the default is "preview"; the command-line word can switch to "draft" or "final".
* Line 47: print which one is running.

---

## Block D — The terrain mesh and its texture (lines 49–67)

```cpp
    std::printf("Building terrain...\n");
    Mesh terrain = make_parametric_mesh(300, 240, [](double u, double v) {
        double x = -150.0 + 300.0 * u;
        double z = 40.0 - 240.0 * v;
        return Point3(x, terrain_height(x, z), z);
    });
```

Lines 52–57: a 300 × 240 grid (= **144,000 triangles**) covering x from −150 to 150 and z from 40 (behind the camera)
to −200 (the far mountains). Each grid point's height comes from `terrain_height`.

```cpp
    auto sand = std::make_shared<FunctionTexture>([](double, double, const Point3& p) {
        double n = fbm_2d(p.x * 0.3, p.z * 0.3, 4);                    // small color variation
        Color dry = lerp(hex_color(0xC9A27E), hex_color(0xB08463), n);
        Color wet = hex_color(0x6E5641);
        Color rock = hex_color(0x8C8177);
        Color c = lerp(wet, dry, smoothstep(WATER_LEVEL, WATER_LEVEL + 0.5, p.y));   // darker near water
        return lerp(c, rock, smoothstep(6.0, 14.0, p.y));                            // rocky mountains
    });
    world.add(terrain.build(std::make_shared<Lambertian>(sand)));
```

The sand color depends on the **height** of each point:

* Line 59–60: fine noise mixes two sand tones, so the ground isn't flat-colored.
* Line 63: below and just above the water line, blend toward **wet** (darker) sand.
* Line 64: above 6–14 units, blend toward grey **rock** (the mountains).
* Line 66: build the mesh into a BVH with this texture.

---

## Block E — Water (lines 69–71)

```cpp
    world.add(std::make_shared<Quad>(Point3(-150, WATER_LEVEL, 40), Vec3(300, 0, 0), Vec3(0, 0, -240),
                                     std::make_shared<Plastic>(hex_color(0x0B2027), 0.03, 1.33)));
```

A huge flat quad at the water level, with a **Plastic** material: a very dark teal base under a very smooth coat with
the index of refraction of water (1.33). The coat's Fresnel reflection makes it mirror the sky at grazing angles: exactly
how a calm lake looks.

---

## Block F — The monolith (lines 73–78)

```cpp
    double ground = terrain_height(MONOLITH_POS.x, MONOLITH_POS.z);
    std::shared_ptr<Hittable> slab = make_box(Point3(-1.0, 0, -0.25), Point3(1.0, 4.5, 0.25),
                                              std::make_shared<Plastic>(hex_color(0x050506), 0.12));
    slab = std::make_shared<RotateY>(slab, 25);
    world.add(std::make_shared<Translate>(slab, Vec3(MONOLITH_POS.x, ground + 0.6, MONOLITH_POS.z)));
```

* Line 74: the ground height where it stands.
* Lines 75–76: a slab 2 wide, 4.5 tall and 0.5 deep, in near-black glossy plastic (like polished obsidian).
* Line 77: turned 25°.
* Line 78: moved into place, **floating 0.6 above the ground**, which makes it look impossible.

---

## Block G — Glowing orbs (lines 80–87)

```cpp
    auto orb_light = std::make_shared<DiffuseLight>(Color(40, 18, 6));
    Point3 orbs[3] = {Point3(-2.2, ground + 3.2, -7.0), Point3(2.0, ground + 2.4, -8.8), Point3(1.2, ground + 4.9, -6.6)};
    for (const Point3& o : orbs) {
        auto s = std::make_shared<Sphere>(o, 0.15, orb_light);
        world.add(s);
        lights.add(s);
    }
```

Three small warm orange lights floating around the monolith, each added to the world and the lights list. They give
warm accents on the slab and something for the eye to follow.

---

## Block H — Rocks (lines 89–101)

```cpp
    Pcg32 rng(77);
    for (int i = 0; i < 80; i++) {
        double x = rng.next_double() * 40 - 20, z = rng.next_double() * 40 - 25;
        double r = 0.15 + 0.6 * std::pow(rng.next_double(), 2);
        double h = terrain_height(x, z);
        if (h < WATER_LEVEL - 0.1) continue;                  // under water
        if ((Point3(x, 0, z) - Point3(5.5, 0, 13.5)).length() < 4) continue;   // too close to camera
        rocks.add(std::make_shared<Sphere>(Point3(x, h + 0.2 * r, z), r, rock_mat));
    }
    world.add(std::make_shared<BVHNode>(rocks));
```

* Line 94: a random spot in the area in front of the monolith.
* Line 95: a random size; squaring the random number makes **small rocks much more common**.
* Lines 96–97: skip spots under water.
* Line 98: skip spots right in front of the camera (they would block the view).
* Line 99: place the rock slightly **sunk into** the ground (`h + 0.2r`), so it looks half-buried.
* Line 101: all rocks into one BVH.

---

## Block I — The person (SDF) (lines 103–118)

```cpp
    const Point3 person_pos(3.8, WATER_LEVEL + 0.3, 0.5);
    std::shared_ptr<Hittable> pedestal = make_box(Point3(-0.6, -1.5, -0.5), Point3(0.6, 0, 0.5), rock_mat);
    pedestal = std::make_shared<RotateY>(pedestal, 10);
    world.add(std::make_shared<Translate>(pedestal, person_pos));
```

Lines 105–108: the figure stands on a flat rock in the shallow water: a box whose top is exactly at the person's feet.

```cpp
    auto person = [person_pos](const Point3& p) {
        Point3 q = p - person_pos;
        double body = sdf::capsule_y(q - Vec3(0, 0.25, 0), 1.05, 0.17);
        double head = sdf::sphere(q - Vec3(0, 1.63, 0), 0.12);
        double legs = sdf::op_smooth_union(sdf::capsule_y(q - Vec3(-0.09, 0, 0), 0.8, 0.08),
                                           sdf::capsule_y(q - Vec3(0.09, 0, 0), 0.8, 0.08), 0.05);
        return sdf::op_smooth_union(sdf::op_smooth_union(body, legs, 0.08), head, 0.06);
    };
```

A human silhouette from four simple shapes (chapter 37):

* Line 111: the **body**: a capsule from 0.25 to 1.30 high, 0.17 thick.
* Line 112: the **head**: a small sphere at 1.63.
* Lines 113–114: two thin **legs**, smoothly joined.
* Line 115: melt body + legs + head together, so the joins look natural.

```cpp
    world.add(std::make_shared<SDFObject>(person, AABB(person_pos - Vec3(0.5, 0.1, 0.5), person_pos + Vec3(0.5, 1.9, 0.5)),
                                          std::make_shared<Lambertian>(hex_color(0x1F1B18))));
```

Lines 117–118: wrap it with a tight bounding box and a very dark matte material. At this distance the figure reads as a
**silhouette**, which is all we need: it gives the monolith its scale.

---

## Block J — Sun and sky (lines 120–133)

```cpp
    Vec3 sun_dir = unit_vector(Vec3(0.18, 0.10, -1.0));   // low, in front of the camera
    auto sun = make_sun(sun_dir, 0.7, Color(9000, 5200, 2300));
    world.add(sun);
    lights.add(sun);
```

Lines 121–124: the sun sits **low** (y = 0.10 against z = −1: about 6° above the horizon) and slightly right, in front
of the camera. Its color is strongly orange (9000 red vs 2300 blue) and it's sampled as a light.

```cpp
    SkySettings sky;
    sky.sun_direction = sun_dir;
    sky.zenith = Color(0.10, 0.17, 0.40);
    sky.horizon = Color(1.00, 0.62, 0.40);
    sky.ground = Color(0.12, 0.08, 0.06);
    sky.sun_glow = Color(1.0, 0.55, 0.25);
    sky.glow_strength = 1.6;
    sky.intensity = 0.9;
```

Lines 126–133: a dawn sky: blue above, glowing orange at the horizon, with a strong warm halo around the sun.

---

## Block K — Atmosphere (lines 135–137)

```cpp
    auto haze_bounds = std::make_shared<Sphere>(Point3(0, 0, -40), 170, nullptr);
    world.add(std::make_shared<ConstantMedium>(haze_bounds, 0.0035, Color(1.0, 0.92, 0.85)));
```

A **huge** sphere (radius 170) filled with very thin warm haze (density 0.0035: a ray travels ~285 units on average
before scattering). It makes distant things fade, catches the low sunlight, and gives the image depth. The camera is
inside it, which the volume code handles.

---

## Block L — Camera and render (lines 139–157)

```cpp
    cam.aspect_ratio = 2.39;                 // cinemascope
    cam.image_width = preset.width;
    cam.samples_per_pixel = preset.spp;
    cam.max_depth = 12;
    cam.vfov = 24;
    cam.lookfrom = Point3(5.5, terrain_height(5.5, 13.5) + 1.7, 13.5);   // eye height above the sand
    cam.lookat = Point3(0.5, 2.6, -8.0);
    cam.defocus_angle = 0.25;
    cam.focus_dist = (cam.lookat - cam.lookfrom).length();
    cam.background = physical_sky(sky);
    cam.max_sample_value = 60;
    cam.collect_aovs = preset.denoise;
```

* Line 141: a wide **2.39:1** cinema frame.
* Line 145: a 24° lens: a normal, undistorted look.
* Line 146: the camera stands on the ground, 1.7 m above it (eye height). Using `terrain_height` means it can never end
  up under the sand.
* Line 147: it looks at the monolith, a little above its middle.
* Lines 148–149: a slightly open lens, focused **exactly** on the monolith (the distance between the two points).
* Lines 151–152: clamp extreme samples, and collect the denoiser's guide images.

```cpp
    Image hdr = cam.render(world, &lights);
    SaveOptions raw_aces;
    raw_aces.tonemap = ToneMapper::Aces;
    save_image("images/ch38_raw_" + preset.name + ".png", hdr, raw_aces);
```

Lines 154–157: render, then save a "raw" version (only tone mapped) so you can see what post-production adds. The file
name includes the preset name.

---

## Block M — Post-production (lines 159–176)

```cpp
    Image img = hdr;
    if (preset.denoise) {
        post::DenoiseSettings ds;
        ds.radius = preset.spp >= 128 ? 3 : 6;
        ds.passes = preset.spp >= 128 ? 1 : 2;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
    }
```

Lines 160–166: denoise first, on the **linear** image. With many samples (final) the image is already fairly clean, so
we filter gently (radius 3, one pass); with few samples we filter harder.

```cpp
    img = post::bloom(img, 2.0, 0.12, 6);
    img = post::exposure(img, -0.2);
    img = post::tonemap(img, ToneMapper::Aces);
    img = post::teal_orange(img, 0.25);
    img = post::chromatic_aberration(img, preset.width / 1000.0);
    img = post::vignette(img, 0.45, 0.65);
    img = post::film_grain(img, 0.03);
```

The pipeline in the correct order (chapter 35):

* Line 167: bloom in HDR: only things brighter than 2 glow (the sun, the orbs, the sun's reflection in the water).
* Line 168: darken slightly (−0.2 stops).
* Line 169: ACES tone mapping → values are now 0–1.
* Line 170: a gentle teal & orange grade.
* Line 171: chromatic aberration scaled to the image size, so `draft` and `final` look the same.
* Lines 172–173: vignette and film grain.

```cpp
    SaveOptions plain;
    save_image("images/ch38_final_" + preset.name + ".png", img, plain);
```

Lines 175–176: save the finished frame (already developed, so only the sRGB step is applied).

---

## Block N — End (lines 177–178)

`return 0;` `}`

---

## Check your understanding

1. Which single function defines the whole landscape? *(`terrain_height`.)*
2. Why is the camera's height computed from `terrain_height`? *(So it always stands on the ground, whatever the noise
   does.)*
3. Why is `focus_dist` computed instead of typed in? *(So the monolith is always perfectly sharp, even if you move the
   camera.)*
4. Why does chromatic aberration use `preset.width / 1000.0`? *(So the effect looks the same at every image size.)*
