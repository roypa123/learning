# Line by line: `ch34_hdr_tonemapping.cpp`

[← Line-by-line index](README.md) · [Chapter 34 (the theory)](../34-hdr-tonemapping.md) · [color.h: tone mapping](color.md)

**What the whole program does, in one sentence:** it renders **one** night scene with a very bright lamp, then
"develops" that single image in many ways: four tone-mapping curves, five exposures, a false-color map, and a graded
version.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–13 | |
| B. `heat` | 15–22 | brightness → heat-map color |
| C. The scene | 24–37 | ground, three balls, a street lamp |
| D. Camera and dusk sky | 39–51 | |
| E. One render, four tone mappers | 53–59 | |
| F. Exposure bracket | 61–69 | −2 to +2 stops |
| G. False color | 71–76 | |
| H. Color grading | 78–85 | |
| I. End | 86–87 | |

---

## Block A — Comments, includes (lines 1–13)

Comments listing the outputs, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — `heat` (lines 15–22)

Turns a brightness into a "heat map" color, so we can *see* how much light each part of the image has.

```cpp
static Color heat(double lum) {
    double t = clamp01((std::log10(std::max(lum, 1e-4)) + 3.0) / 5.0);   // 0.001 .. 100
```

* Line 17: brightness values span a huge range, so we use a **logarithm**: `log10(0.001) = −3`, `log10(100) = +2`.
  Adding 3 and dividing by 5 maps 0.001 → 0 and 100 → 1. `max(lum, 1e-4)` avoids `log10(0)`, and `clamp01` keeps it in
  range.

```cpp
    Color stops[5] = {Color(0, 0, 0.5), Color(0, 0.6, 1), Color(0, 1, 0), Color(1, 1, 0), Color(1, 0, 0)};
    double f = t * 4;
    int i = std::min(3, (int)f);
    return lerp(stops[i], stops[i + 1], f - i);
}
```

* Line 18: five colors: dark blue → cyan → green → yellow → red.
* Line 19: `t` (0–1) stretched to 0–4: which pair of colors we are between.
* Line 20: the lower color's index (at most 3, so `i + 1` is always valid).
* Line 21: blend between the two by the fractional part.

---

## Block C — The scene (lines 24–37)

```cpp
    world.add(std::make_shared<Quad>(Point3(-50, 0, -50), Vec3(100, 0, 0), Vec3(0, 0, 100),
                                     std::make_shared<Lambertian>(hex_color(0x6B6B6B))));
    world.add(std::make_shared<Sphere>(Point3(-1.4, 0.6, 0.3), 0.6, std::make_shared<Lambertian>(hex_color(0xD9D9D9))));
    world.add(std::make_shared<Sphere>(Point3(1.3, 0.6, 0.2), 0.6, std::make_shared<Plastic>(hex_color(0xB91C1C), 0.2)));
    world.add(std::make_shared<Sphere>(Point3(0.0, 0.45, 1.3), 0.45, std::make_shared<Dielectric>(1.5)));
```

Lines 26–30: a grey ground, a white matte ball, a glossy red plastic ball, and a glass ball in front.

```cpp
    auto pole = make_box(Point3(-0.05, 0, -0.05), Point3(0.05, 2.6, 0.05), std::make_shared<RoughMetal>(Color(0.3, 0.3, 0.3), 0.4));
    world.add(std::make_shared<Translate>(pole, Vec3(0, 0, -1.2)));
    auto bulb = std::make_shared<Sphere>(Point3(0, 2.75, -1.2), 0.15, std::make_shared<DiffuseLight>(Color(400, 300, 180)));
    world.add(bulb);
    lights.add(bulb);
```

* Line 33: a thin (0.1 × 2.6 × 0.1) dark metal box: the lamp post.
* Line 34: moved back by 1.2.
* Line 35: the **bulb**: a small glowing sphere with radiance **(400, 300, 180)**: 400 times brighter than white, and
  warm in color. This is what makes the image truly HDR.
* Lines 36–37: added to the world and to the lights (so rays aim at it).

---

## Block D — Camera and dusk sky (lines 39–51)

```cpp
    SkySettings dusk;
    dusk.zenith = Color(0.01, 0.02, 0.06);
    dusk.horizon = Color(0.10, 0.07, 0.12);
    dusk.ground = Color(0.01, 0.01, 0.01);
    dusk.glow_strength = 0.0;
    cam.background = physical_sky(dusk);
```

Lines 46–51: a **very dim** dusk sky (values around 0.01–0.1) with no sun glow. Together with the bulb's 400, the scene
spans a range of about 1 : 40,000.

---

## Block E — One render, four tone mappers (lines 53–59)

```cpp
    Image hdr = cam.render(world, &lights);   // values from ~0.001 up to 400!

    SaveOptions o;
    o.tonemap = ToneMapper::Clamp;    save_image("images/ch34_clamp.png", hdr, o);
    o.tonemap = ToneMapper::Reinhard; save_image("images/ch34_reinhard.png", hdr, o);
    o.tonemap = ToneMapper::Aces;     save_image("images/ch34_aces.png", hdr, o);
    o.tonemap = ToneMapper::Hable;    save_image("images/ch34_hable.png", hdr, o);
```

* Line 53: render **once** and keep the HDR image in memory.
* Lines 56–59: save it four times with different curves. The render is not repeated: only the "development" changes,
  exactly like developing one photo negative in four ways.

---

## Block F — Exposure bracket (lines 61–69)

```cpp
    Image small = post::resize(hdr, 240, 135);
    Image strip;
    for (int stop = -2; stop <= 2; stop++) {
        Image developed = post::tonemap(post::exposure(small, stop), ToneMapper::Aces);
        strip = stop == -2 ? developed : post::side_by_side(strip, developed, 4);
    }
    SaveOptions plain;   // already tone mapped: just apply sRGB
    save_image("images/ch34_exposure_bracket.png", strip, plain);
```

* Line 62: a small copy, so five versions fit side by side.
* Line 64: exposures −2, −1, 0, +1, +2 **stops** (each stop doubles the light).
* Line 65: brighten by that many stops, then tone map with ACES.
* Line 66: build the strip.
* Lines 68–69: save with default options: these images are **already** tone mapped, so we only want the sRGB step.

---

## Block G — False color (lines 71–76)

```cpp
    Image fc(hdr.width, hdr.height);
    for (size_t i = 0; i < hdr.data.size(); i++) fc.data[i] = heat(luminance(hdr.data[i]));
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch34_false_color.png", fc, raw);
```

* Line 73: for every pixel: compute its brightness (`luminance`) and convert it to a heat color.
* Lines 74–76: save **without** the sRGB curve, because these colors are already chosen as screen colors.

Cinematographers use false color on set to see instantly which parts are too dark or blown out.

---

## Block H — Color grading (lines 78–85)

```cpp
    Image graded = post::contrast(hdr, 1.1);
    graded = post::temperature(graded, 1.0);
    graded = post::saturation(graded, 1.15);
    graded = post::tonemap(post::exposure(graded, 0.3), ToneMapper::Aces);
```

* Line 79: slightly more contrast, **before** tone mapping (on the HDR values).
* Line 80: warmer (more red, less blue).
* Line 81: 15% more colorful.
* Line 82: brighten by 0.3 stops and tone map.

```cpp
    for (auto& c : graded.data)
        c = post::lift_gamma_gain(c, Color(0.02, 0.01, 0.04), Color(1.0, 1.0, 1.05), Color(1.0, 0.97, 0.92));
    save_image("images/ch34_graded.png", graded, plain);
```

Lines 83–85: the classic three-way color corrector, applied per pixel **after** tone mapping:

* **lift** (0.02, 0.01, 0.04): raises the blacks slightly, with a purple-blue tint (moody shadows),
* **gamma** (1, 1, 1.05): a touch more blue in the midtones,
* **gain** (1, 0.97, 0.92): warm highlights.

---

## Block I — End (lines 86–87)

`return 0;` `}`

---

## Check your understanding

1. Why is the scene rendered only once? *(Tone mapping and grading are "development" steps on the finished HDR image.)*
2. What does one "stop" mean? *(Twice as much light.)*
3. Why does the false-color image use a logarithm? *(Light spans a huge range; a log scale shows both dark and bright.)*
4. Why is `plain` used when saving already tone-mapped images? *(They're already 0–1: only the sRGB step is needed.)*
