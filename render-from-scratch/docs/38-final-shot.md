# Chapter 38 — The final shot: "The Monolith at Dawn"

[← SDFs & ray marching](37-sdf-raymarching.md) · [Contents](README.md) · [Next: Animation →](39-animation.md)

---

## Goal

Bring **everything** together into one cinematic frame, and think like a film-maker while doing it. This
chapter is less about new code and more about **how to build a shot**:

* story and **composition**,
* **lighting design** (key light, fill, practical lights, atmosphere),
* **materials** chosen for the story,
* **camera** choices (lens, height, aspect ratio, focus),
* a **post-production** pipeline,
* working with **quality presets** (draft → preview → final).

---

## 1. Start with an idea

Every good shot tells a tiny story. Ours: *"At dawn, a lone traveller discovers something impossible in the
desert."* That sentence decides almost everything:

| Story element | Visual decision |
|---------------|-----------------|
| dawn | low, warm sun; long shadows; orange sky fading to blue; haze |
| desert | rolling sand dunes (noise terrain), distant mountains |
| lone traveller | a small human silhouette, for **scale** and emotion |
| something impossible | a perfectly geometric, glossy black slab, **floating** above the ground, with glowing orbs |
| discovery | the person looks at it; the camera sits behind them, so we see what they see |

(Film fans will recognize the homage to *2001: A Space Odyssey*, whose monolith had proportions 1 : 4 : 9.)

---

## 2. Composition

We use a **2.39:1** cinemascope frame, rendered directly at that aspect ratio (no wasted black bars).

```
┌──────────────────────────────────────────────────────────────────┐
│                  sky: blue at top → orange at horizon      ☀     │  sun near the right third,
│  ~~~~~ distant hazy mountains ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ │  just above the mountains
│                        ● orb        █                            │
│                    ●               █ █  ← monolith: center,      │
│                                    █ █    tall and dark           │
│   dunes ~~~~~~~~        ~~~~~~~~~~ █ █ ~~~~~       ~~~~~~        │
│         lake reflects sky ≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈  ▲ person on a rock   │  person on the right third
│  sand, rocks ...                                                 │
└──────────────────────────────────────────────────────────────────┘
```

Principles used:

* **Rule of thirds**: the person and the sun sit near the right third line; the monolith near the center-left.
* **Silhouettes**: the monolith and the person are dark shapes against a bright sky and bright water. The
  eye reads silhouettes instantly.
* **Depth layers**: foreground (sand, rocks), middle ground (lake, person, monolith), background
  (dunes, mountains, sky). Haze makes each layer lighter and bluer, which sells the distance.
* **Leading lines**: the lake edge and dunes lead toward the monolith.
* **Scale**: the tiny person makes the monolith feel huge.

---

## 3. Lighting design

| Light | Role | In code |
|-------|------|---------|
| **Key**: the sun | main light, low and warm, **backlighting** the monolith (it's in front of the camera) | `make_sun(...)` with radiance (9000, 5200, 2300): orange |
| **Fill**: the sky | soft blue light in the shadows | `physical_sky` golden-hour settings |
| **Practicals**: 3 glowing orbs | visible light sources in the scene, warm accents on the monolith | `DiffuseLight(40, 18, 6)` spheres |
| **Atmosphere** | haze scatters sunlight: soft glow, depth, faint light shafts | `ConstantMedium` sphere, density 0.0035 |

Backlighting (sun behind the subject) is a classic cinematography technique: it creates rim light on edges,
long shadows toward the camera, and a glowing atmosphere. It's also the hardest to render cleanly. Luckily
we have light sampling and a denoiser.

All the lights are in the `lights` list, so the renderer aims rays at them.

---

## 4. Materials

| Object | Material | Why |
|--------|----------|-----|
| sand | Lambertian with a `FunctionTexture`: noise-varied dry sand, darker wet sand near the water, rock color up high | believable variation without a texture file |
| lake | `Plastic(dark teal, roughness 0.03, ior 1.33)` | the Fresnel coat reflects the sky strongly at grazing angles, just like real water |
| monolith | `Plastic(near black, roughness 0.12)` | glossy obsidian-like: reflects the sky with soft highlights |
| rocks | Lambertian grey-brown | quiet, not distracting |
| person | Lambertian near-black SDF (capsules + a sphere, smooth unions) | a silhouette; detail doesn't matter at this size |

---

## 5. Camera

* **Low** (1.7 m above the sand, eye height) looking slightly **up** toward the monolith: it makes it feel
  imposing.
* **Lens**: vfov 24° at 2.39:1 is about a 50–60 mm "normal" lens horizontally, so no wide-angle distortion.
* **Depth of field**: very subtle (`defocus_angle = 0.25`), focused on the monolith. Just enough to separate the
  foreground.
* **Position**: behind and to the right of the person, so we share their view.

---

## 6. The terrain

`terrain_height(x, z)` combines several ideas from earlier chapters:

```
height = dunes (two octaves of fBm, chapter 11)
         × flatten near the monolith (smoothstep of the distance)
         − a gaussian dip for the lake basin
         + distant mountains (big, slow fBm, only far away)
```

It's turned into a 300 × 240 grid = **144,000 triangles** with smooth normals (chapter 25), stored in a
BVH (chapter 23). The water is one big quad at `WATER_LEVEL`. Wherever the terrain dips below it, you see
lake.

---

## 7. Post-production

```
HDR render
  → denoise (albedo + normal guides)
  → bloom (threshold 2.0): the sun, the orbs and the sun's reflection in the water glow
  → exposure −0.2
  → ACES tone mapping
  → teal & orange 0.25
  → chromatic aberration (scaled to the image width)
  → vignette
  → film grain
```

The raw ACES image is saved too, so you can compare before and after post.

---

## 8. Quality presets

| Preset | Width × height | Samples | Typical time* |
|--------|----------------|---------|---------------|
| `draft` | 480 × 200 | 16 | ~1 minute |
| `preview` (default) | 960 × 401 | 32 | a few minutes |
| `final` | 1920 × 803 | 256 | a few hours |

\* very dependent on your CPU. The haze and the backlit sun make this our most expensive scene.

**Work like a studio:** iterate on composition and lighting with `draft`, check with `preview`, and only
then run `final` (overnight).

---

## 9. The program

**File: `chapters/ch38_final_shot.cpp`**

```cpp
// ch38_final_shot.cpp
// ------------------------------------------------------------
// Chapter 38: The final shot - "The Monolith at Dawn".
// Everything from the book in one cinematic frame:
//   terrain mesh (144k triangles) + BVH, procedural textures, water with a
//   Fresnel coat, a glossy black monolith, glowing orbs, a human figure made
//   from SDFs, a low golden sun, atmospheric haze (volume), depth of field,
//   light sampling, denoising, bloom, ACES, color grade, vignette, grain.
//
//   run ch38_final_shot draft     ~1 minute   (480 px wide)
//   run ch38_final_shot           a few min.  (960 px wide)  <- default "preview"
//   run ch38_final_shot final     hours       (1920 px wide, 256 samples)
//
// Output: images/ch38_final_<preset>.png  (+ raw and denoise-comparison images)
// ------------------------------------------------------------
#include <cmath>
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

// ---------------- the landscape ------------------------------------------
const double WATER_LEVEL = -0.35;
const Point3 MONOLITH_POS(0, 0, -8);

double terrain_height(double x, double z) {
    // Rolling dunes: two layers of fractal noise.
    double dunes = 3.0 * (fbm_2d(x * 0.02 + 11.0, z * 0.02 + 3.0, 5) - 0.5) * 2.0
                 + 0.8 * (fbm_2d(x * 0.08 + 5.0, z * 0.08, 4) - 0.5);
    // Flat plateau around the monolith.
    double dx = x - MONOLITH_POS.x, dz = z - MONOLITH_POS.z;
    double d = std::sqrt(dx * dx + dz * dz);
    double h = dunes * smoothstep(5.0, 30.0, d);
    // A shallow lake basin between the camera and the monolith.
    h -= 1.4 * std::exp(-((x - 3) * (x - 3) / 60.0 + (z - 1) * (z - 1) / 20.0));
    // Distant mountains far behind.
    double far = smoothstep(70.0, 160.0, -z);
    h += far * 18.0 * fbm_2d(x * 0.012 + 40.0, z * 0.012, 6);
    return h;
}

struct Preset { std::string name; int width; int spp; bool denoise; };

int main(int argc, char** argv) {
    Preset preset = {"preview", 960, 32, true};
    if (argc > 1 && std::string(argv[1]) == "draft") preset = {"draft", 480, 16, true};
    if (argc > 1 && std::string(argv[1]) == "final") preset = {"final", 1920, 256, true};
    std::printf("Preset: %s (%d px wide, %d samples per pixel)\n", preset.name.c_str(), preset.width, preset.spp);

    HittableList world, lights;

    // ---------- terrain -----------------------------------------------------
    std::printf("Building terrain...\n");
    Mesh terrain = make_parametric_mesh(300, 240, [](double u, double v) {
        double x = -150.0 + 300.0 * u;
        double z = 40.0 - 240.0 * v;
        return Point3(x, terrain_height(x, z), z);
    });
    auto sand = std::make_shared<FunctionTexture>([](double, double, const Point3& p) {
        double n = fbm_2d(p.x * 0.3, p.z * 0.3, 4);                    // small color variation
        Color dry = lerp(hex_color(0xC9A27E), hex_color(0xB08463), n);
        Color wet = hex_color(0x6E5641);
        Color rock = hex_color(0x8C8177);
        Color c = lerp(wet, dry, smoothstep(WATER_LEVEL, WATER_LEVEL + 0.5, p.y));   // darker near water
        return lerp(c, rock, smoothstep(6.0, 14.0, p.y));                            // rocky mountains
    });
    world.add(terrain.build(std::make_shared<Lambertian>(sand)));
    std::printf("Terrain: %zu triangles\n", terrain.triangle_count());

    // ---------- water: a dark base under a very smooth clear coat -----------
    world.add(std::make_shared<Quad>(Point3(-150, WATER_LEVEL, 40), Vec3(300, 0, 0), Vec3(0, 0, -240),
                                     std::make_shared<Plastic>(hex_color(0x0B2027), 0.03, 1.33)));

    // ---------- the monolith (floating slightly) ---------------------------
    double ground = terrain_height(MONOLITH_POS.x, MONOLITH_POS.z);
    std::shared_ptr<Hittable> slab = make_box(Point3(-1.0, 0, -0.25), Point3(1.0, 4.5, 0.25),
                                              std::make_shared<Plastic>(hex_color(0x050506), 0.12));
    slab = std::make_shared<RotateY>(slab, 25);
    world.add(std::make_shared<Translate>(slab, Vec3(MONOLITH_POS.x, ground + 0.6, MONOLITH_POS.z)));

    // ---------- glowing orbs -----------------------------------------------
    auto orb_light = std::make_shared<DiffuseLight>(Color(40, 18, 6));
    Point3 orbs[3] = {Point3(-2.2, ground + 3.2, -7.0), Point3(2.0, ground + 2.4, -8.8), Point3(1.2, ground + 4.9, -6.6)};
    for (const Point3& o : orbs) {
        auto s = std::make_shared<Sphere>(o, 0.15, orb_light);
        world.add(s);
        lights.add(s);
    }

    // ---------- rocks --------------------------------------------------------
    auto rock_mat = std::make_shared<Lambertian>(hex_color(0x5B524A));
    HittableList rocks;
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

    // ---------- a person for scale, built from SDFs -------------------------
    // They stand on a flat rock in the shallow lake, looking at the monolith.
    const Point3 person_pos(3.8, WATER_LEVEL + 0.3, 0.5);
    std::shared_ptr<Hittable> pedestal = make_box(Point3(-0.6, -1.5, -0.5), Point3(0.6, 0, 0.5), rock_mat);
    pedestal = std::make_shared<RotateY>(pedestal, 10);
    world.add(std::make_shared<Translate>(pedestal, person_pos));
    auto person = [person_pos](const Point3& p) {
        Point3 q = p - person_pos;
        double body = sdf::capsule_y(q - Vec3(0, 0.25, 0), 1.05, 0.17);
        double head = sdf::sphere(q - Vec3(0, 1.63, 0), 0.12);
        double legs = sdf::op_smooth_union(sdf::capsule_y(q - Vec3(-0.09, 0, 0), 0.8, 0.08),
                                           sdf::capsule_y(q - Vec3(0.09, 0, 0), 0.8, 0.08), 0.05);
        return sdf::op_smooth_union(sdf::op_smooth_union(body, legs, 0.08), head, 0.06);
    };
    world.add(std::make_shared<SDFObject>(person, AABB(person_pos - Vec3(0.5, 0.1, 0.5), person_pos + Vec3(0.5, 1.9, 0.5)),
                                          std::make_shared<Lambertian>(hex_color(0x1F1B18))));

    // ---------- sun & sky -----------------------------------------------------
    Vec3 sun_dir = unit_vector(Vec3(0.18, 0.10, -1.0));   // low, in front of the camera
    auto sun = make_sun(sun_dir, 0.7, Color(9000, 5200, 2300));
    world.add(sun);
    lights.add(sun);

    SkySettings sky;
    sky.sun_direction = sun_dir;
    sky.zenith = Color(0.10, 0.17, 0.40);
    sky.horizon = Color(1.00, 0.62, 0.40);
    sky.ground = Color(0.12, 0.08, 0.06);
    sky.sun_glow = Color(1.0, 0.55, 0.25);
    sky.glow_strength = 1.6;
    sky.intensity = 0.9;

    // ---------- atmosphere: a huge sphere of thin, warm haze ----------------
    auto haze_bounds = std::make_shared<Sphere>(Point3(0, 0, -40), 170, nullptr);
    world.add(std::make_shared<ConstantMedium>(haze_bounds, 0.0035, Color(1.0, 0.92, 0.85)));

    // ---------- camera -------------------------------------------------------
    Camera cam;
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

    Image hdr = cam.render(world, &lights);
    SaveOptions raw_aces;
    raw_aces.tonemap = ToneMapper::Aces;
    save_image("images/ch38_raw_" + preset.name + ".png", hdr, raw_aces);

    // ---------- post-production ---------------------------------------------
    Image img = hdr;
    if (preset.denoise) {
        post::DenoiseSettings ds;
        ds.radius = preset.spp >= 128 ? 3 : 6;
        ds.passes = preset.spp >= 128 ? 1 : 2;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
    }
    img = post::bloom(img, 2.0, 0.12, 6);
    img = post::exposure(img, -0.2);
    img = post::tonemap(img, ToneMapper::Aces);
    img = post::teal_orange(img, 0.25);
    img = post::chromatic_aberration(img, preset.width / 1000.0);
    img = post::vignette(img, 0.45, 0.65);
    img = post::film_grain(img, 0.03);

    SaveOptions plain;
    save_image("images/ch38_final_" + preset.name + ".png", img, plain);
    return 0;
}
```

```bat
run ch38_final_shot draft
run ch38_final_shot
run ch38_final_shot final
```

---

## 10. What you should see

![Raw](../images/ch38_raw_preview.png)

> **Image description (raw, ACES only):** The complete scene, but flatter and slightly noisy: a wide golden
> landscape, the black monolith, the orbs, the lake and the small figure, without glow, grain or grading.

![Final](../images/ch38_final_preview.png)

> **Image description — "The Monolith at Dawn":** A wide cinemascope frame. The sky is deep blue at the top,
> turning to a glowing orange near the horizon, where a bright sun sits just above a line of low, hazy, pale
> mountains, right of center, surrounded by a warm bloom. Rolling sand dunes in shades of tan and ochre stretch
> across the frame, with long shadows falling toward the camera. At the center stands a tall, perfectly
> smooth black slab, hovering a little above the sand, its glossy face catching a soft reflection of the sky
> and warm specks of light from three glowing orange orbs floating around it. In the foreground, a shallow lake
> mirrors the orange sky, and on a dark rock in the water stands a small, dark human silhouette looking toward
> the monolith, which makes it look enormous. Warm haze softens the distance. The shadows lean teal, the
> highlights orange, the corners are gently darkened, and a fine film grain covers everything.

---

## Try it yourself

This is *your* shot now. Some ideas:

1. **Night version**: sun below the horizon, blue-hour sky, stronger orbs (radiance 200), add a moon
   (a small, dim, bluish `make_sun`).
2. **Storm**: grey sky, denser haze (0.01), no sun disk, cool grade.
3. **Different story**: replace the monolith with a crashed glass sphere (`Dielectric`), or a gold
   Mandelbulb (chapter 37).
4. Move the camera **very low** (0.4 m) with a wide lens (vfov 40): dramatic.
5. Put the person in shadow and the monolith in light, or vice versa. How does the mood change?
6. Render `final` and set it as your desktop wallpaper. You made it, from nothing.

## Common problems

| Symptom | Cause |
|---------|-------|
| No sunlight on the ground | The sun is below the mountains: raise its elevation or lower the mountains |
| Very noisy even at preview | Haze plus backlight is hard: more samples, or lower haze density |
| Camera under the sand | Check `lookfrom` against `terrain_height` (the program uses eye height above the sand) |
| Takes forever | Use `draft` while experimenting |

---

## Summary

* Start from a one-sentence story; let it drive light, materials, camera and color.
* Compose with thirds, silhouettes, depth layers and scale.
* Light with a key, a fill, practicals and atmosphere.
* Finish with a disciplined post pipeline, and iterate with quality presets.

Next: [Chapter 39 — Animation →](39-animation.md)
