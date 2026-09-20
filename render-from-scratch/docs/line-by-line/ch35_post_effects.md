# Line by line: `ch35_post_effects.cpp`

[← Line-by-line index](README.md) · [Chapter 35 (the theory)](../35-post-processing.md) · [post.h](post.md)

**What the whole program does, in one sentence:** it renders a neon-lit night scene once, then saves four versions of it
showing the post-processing built up step by step: tone mapping, bloom, color grade, and lens/film effects.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–12 | |
| B. Quality setting | 14–16 | `final` or preview |
| C. Street and wall | 18–23 | wet road, dark back wall |
| D. Neon lights | 25–38 | a ring of 40 dots and two bars |
| E. Hero objects | 40–43 | chrome, glass and yellow balls |
| F. Camera | 45–58 | night settings, slight depth of field |
| G. Step 1: tone mapping | 59–63 | |
| H. Step 2: bloom | 65–68 | |
| I. Step 3: grade | 70–73 | |
| J. Step 4: lens and film | 75–80 | |
| K. End | 81–82 | |

---

## Block A — Comments, includes (lines 1–12)

Comments listing the four step images, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — Quality setting (lines 14–16)

```cpp
int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    HittableList world, lights;
```

Line 15: `true` if you run `run ch35_post_effects final` (see [ch21](ch21_first_masterpiece.md)).

---

## Block C — Street and wall (lines 18–23)

```cpp
    world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60),
                                     std::make_shared<Plastic>(hex_color(0x0A0A0C), 0.08)));
```

Lines 19–20: the street: a 60 × 60 quad with a **plastic** material whose base color is almost black and whose coat is
very smooth (roughness 0.08). The black base absorbs nearly everything, but the clear coat reflects: exactly how **wet
asphalt** looks at night.

```cpp
    world.add(std::make_shared<Quad>(Point3(-30, 0, -4), Vec3(60, 0, 0), Vec3(0, 20, 0),
                                     std::make_shared<Lambertian>(hex_color(0x2A2320))));
```

Lines 22–23: a dark brown wall standing at z = −4, 20 units tall, so the neon has something to light up.

---

## Block D — Neon lights (lines 25–38)

```cpp
    auto pink = std::make_shared<DiffuseLight>(Color(12, 1.5, 6));
    for (int i = 0; i < 40; i++) {
        double a = 2 * pi * i / 40;
        auto s = std::make_shared<Sphere>(Point3(1.8 * std::cos(a), 2.4 + 1.8 * std::sin(a), -3.6), 0.09, pink);
        world.add(s);
        lights.add(s);
    }
```

* Line 26: a hot pink light material (much more red than green).
* Line 27: 40 small spheres...
* Line 28: ...at equally spaced angles around a circle,
* Line 29: on a circle of radius 1.8, centered at height 2.4, just in front of the wall (z = −3.6). Each sphere is tiny
  (radius 0.09), so together they look like a glowing neon ring.
* Lines 30–31: each one is added to the world **and** to the lights list (so the renderer aims rays at them).

```cpp
    auto cyan = std::make_shared<DiffuseLight>(Color(1, 8, 12));
    auto bar1 = make_box(Point3(-3.6, 0.3, -3.7), Point3(-3.45, 3.8, -3.55), cyan);
    auto bar2 = make_box(Point3(3.45, 0.3, -3.7), Point3(3.6, 3.8, -3.55), cyan);
    world.add(bar1); world.add(bar2);
    lights.add(bar1); lights.add(bar2);
```

Lines 34–38: two tall, thin glowing **boxes** (0.15 wide, 3.5 tall) at the left and right: cyan neon tubes. A box is a
list of six quads, and a list can act as a light too.

---

## Block E — Hero objects (lines 40–43)

```cpp
    world.add(std::make_shared<Sphere>(Point3(-1.1, 0.8, -0.8), 0.8, std::make_shared<RoughMetal>(Color(0.95, 0.95, 0.95), 0.05)));
    world.add(std::make_shared<Sphere>(Point3(1.2, 0.6, -0.2), 0.6, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(0.2, 0.3, 1.0), 0.3, std::make_shared<Plastic>(hex_color(0xF5C518), 0.25)));
```

Lines 41–43: a near-mirror chrome ball (it reflects the neon), a glass ball (it bends the ring into a distorted shape),
and a small glossy yellow ball in front.

---

## Block F — Camera (lines 45–58)

```cpp
    cam.image_width = final_quality ? 1280 : 640;
    cam.samples_per_pixel = final_quality ? 512 : 128;
    ...
    cam.defocus_angle = 0.6;
    cam.focus_dist = 8.0;
    cam.background = solid_background(Color(0.002, 0.003, 0.008));
    cam.max_sample_value = 40;

    Image hdr = cam.render(world, &lights);
```

* Lines 47–48: two quality levels.
* Lines 53–54: a slightly open lens, focused 8 units away (about the neon wall): a gentle background blur.
* Line 55: an almost black background: night.
* Line 56: clamp extreme samples (bright neon reflected in the near-mirror ball).
* Line 58: render once. Everything below works on this **one** HDR image.

---

## Block G — Step 1: tone mapping (lines 59–63)

```cpp
    SaveOptions plain;   // images below are already tone mapped

    Image step1 = post::tonemap(hdr, ToneMapper::Aces);
    save_image("images/ch35_step1_raw.png", step1, plain);
```

* Line 59: default save options (no extra exposure or tone mapping): the images we save below are already developed, and
  only need the sRGB step.
* Lines 62–63: only the filmic ACES curve. The lights look flat, like stickers.

---

## Block H — Step 2: bloom (lines 65–68)

```cpp
    Image bloomed = post::bloom(hdr, 1.0, 0.25, 6);
    Image step2 = post::tonemap(bloomed, ToneMapper::Aces);
    save_image("images/ch35_step2_bloom.png", step2, plain);
```

* Line 66: bloom on the **HDR** image (not on step1!): anything brighter than 1 glows, with strength 0.25 and 6 blur
  sizes.
* Line 67: then tone map.

The neon now bleeds light into the air around it, which instantly feels more real.

---

## Block I — Step 3: grade (lines 70–73)

```cpp
    Image step3 = post::teal_orange(step2, 0.35);
    step3 = post::saturation(step3, 1.1);
    save_image("images/ch35_step3_grade.png", step3, plain);
```

Lines 71–72: push shadows toward teal and highlights toward orange (35%), and add 10% saturation. These work on the
**tone-mapped** image (0–1 values), as grading does in film.

---

## Block J — Step 4: lens and film (lines 75–80)

```cpp
    Image step4 = post::chromatic_aberration(step3, 1.5);
    step4 = post::vignette(step4, 0.55, 0.7);
    step4 = post::film_grain(step4, 0.04);
    step4 = post::letterbox(step4, 2.39);
    save_image("images/ch35_step4_final.png", step4, plain);
```

* Line 76: colored fringes near the edges (1.5 pixels at the corners).
* Line 77: darken the corners by 55%, with a soft falloff.
* Line 78: a little film grain.
* Line 79: black bars for a 2.39:1 cinema frame.

Each step takes the previous result, so the effects stack up in the right order (see chapter 35, section 5).

---

## Block K — End (lines 81–82)

`return 0;` `}`

---

## Check your understanding

1. Why is bloom applied to `hdr` and not to `step1`? *(After tone mapping, a value of 400 and a value of 1 both become
   ~1, so the renderer can't tell what was truly bright.)*
2. Why is `plain` used for saving? *(These images are already tone mapped; only the sRGB curve is still needed.)*
3. What makes the street look wet? *(A near-black base under a smooth clear coat: strong Fresnel reflections.)*
