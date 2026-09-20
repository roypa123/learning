# Line by line: `ch39_animation.cpp`

[← Line-by-line index](README.md) · [Chapter 39 (the theory)](../39-animation.md) · [gif.h](gif.md)

**What the whole program does, in one sentence:** it renders many frames of a camera circling a small scene while a ball
bounces, saves each frame as a PNG, and writes them all into one animated GIF.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–15 | |
| B. `ball_height` | 17–21 | the bouncing motion |
| C. Settings | 23–29 | frames, size, samples, frame rate, shutter |
| D. Things that never change | 31–44 | materials, the torus, the light |
| E. Start the GIF | 46–48 | |
| F. The frame loop | 50–91 | build, render, denoise, save, add to GIF |
| G. Finish | 92–95 | |

---

## Block A — Comments, includes (lines 1–15)

Comments (the outputs and the two quality levels), `<cmath>`, `<cstdio>`, `<string>`, our library,
`using namespace pixel`.

---

## Block B — `ball_height` (lines 17–21)

```cpp
static double ball_height(double t) {
    double phase = t - std::floor(t);                // 0..1 within the current bounce
    return 0.35 + 1.6 * 4.0 * phase * (1.0 - phase);   // parabola: like real gravity
}
```

* Line 19: `t − floor(t)` keeps only the fraction of the time: 0 at the start of each second, close to 1 at the end.
  So the ball bounces once per second, forever.
* Line 20: `4 · phase · (1 − phase)` is a **parabola**: 0 at phase 0 and 1, and exactly 1 in the middle. Times 1.6 =
  the bounce height, plus 0.35 (the ball's radius, so it touches the floor instead of sinking into it).

That shape is what a real thrown ball does under gravity: fast near the ground, slow at the top.

---

## Block C — Settings (lines 23–29)

```cpp
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
    const int frames = final_quality ? 96 : 48;
    const int width = final_quality ? 640 : 320;
    const int spp = final_quality ? 64 : 16;
    const double fps = 24.0;
    const double shutter = 0.5;   // "180 degree shutter": open for half of each frame
```

* Lines 25–27: two quality levels (48 small frames, or 96 bigger ones).
* Line 28: 24 frames per second, like cinema.
* Line 29: the shutter is open for **half** of each frame's time. Film cameras have used this "180° shutter" for a
  century, and it's why film motion looks natural.

---

## Block D — Things that never change (lines 31–44)

```cpp
    auto floor_tex = std::make_shared<CheckerTexture>(0.5, Color(0.8, 0.8, 0.8), Color(0.2, 0.2, 0.22));
    auto floor_mat = std::make_shared<Lambertian>(floor_tex);
    auto torus_mat = std::make_shared<RoughMetal>(Color(1.0, 0.77, 0.34), 0.2);
    auto glass = std::make_shared<Dielectric>(1.5);
    auto blue = std::make_shared<Plastic>(hex_color(0x1D4ED8), 0.25);
    auto ball_mat = std::make_shared<Plastic>(hex_color(0xDC2626), 0.3);
    auto panel = std::make_shared<DiffuseLight>(Color(5, 5, 5));
```

Lines 32–38: all the materials, created **once** outside the loop: a checkered floor, gold, glass, blue plastic, red
plastic, and a light panel.

```cpp
    Mesh torus_mesh = make_torus(0.8, 0.25, 48, 24);
    std::shared_ptr<Hittable> torus = torus_mesh.build(torus_mat);
    torus = std::make_shared<Translate>(torus, Vec3(0, 0.25, 0));

    auto light_quad = std::make_shared<Quad>(Point3(-2, 5, -2), Vec3(4, 0, 0), Vec3(0, 0, 4), panel);   // u x v points down
```

* Lines 40–42: the gold donut: built once (2,304 triangles in a BVH) and lifted so it rests on the floor. Building it
  once and reusing it in every frame saves a lot of time.
* Line 44: a 4 × 4 glowing panel above the scene, facing **down**.

---

## Block E — Start the GIF (lines 46–48)

```cpp
    GifWriter gif;
    int height = (int)(width / (16.0 / 9.0));
    gif.begin("images/ch39_turntable.gif", width, height, (int)std::round(100.0 / fps));
```

* Line 47: the height that matches a 16:9 shape (the same rule the camera uses, so the GIF matches the frames).
* Line 48: open the GIF. The delay is in hundredths of a second: `100 / 24 ≈ 4`.

---

## Block F — The frame loop (lines 50–91)

```cpp
    for (int f = 0; f < frames; f++) {
        double t = f / fps;                       // time of this frame in seconds
        double angle = 2 * pi * f / frames;       // camera goes around once
```

* Line 50: one round per frame.
* Line 51: the time of this frame.
* Line 52: the camera's angle: over all the frames it makes exactly one full circle, so the animation loops perfectly.

```cpp
        HittableList world, lights;
        world.add(std::make_shared<Quad>(Point3(-30, 0, -30), Vec3(60, 0, 0), Vec3(0, 0, 60), floor_mat));
        world.add(torus);
        world.add(std::make_shared<Sphere>(Point3(0, 0.6, 0), 0.6, glass));
        world.add(std::make_shared<Sphere>(Point3(-1.8, 0.45, -1.2), 0.45, blue));
        world.add(light_quad);
        lights.add(light_quad);
```

Lines 54–60: the scene is rebuilt each frame (cheap: everything expensive is reused). A floor, the donut, a glass ball
in its middle, a blue ball, and the light.

```cpp
        Point3 p0(1.9, ball_height(t), 0.8);
        Point3 p1(1.9, ball_height(t + shutter / fps), 0.8);
        world.add(std::make_shared<Sphere>(p0, p1, 0.35, ball_mat));
```

* Lines 63–64: the ball's height **at the start** of the shutter and **at the end** (half a frame later).
* Line 65: a **moving sphere** between those two positions (chapter 22). The camera gives each ray a random time within
  the shutter, so the ball blurs exactly as much as it moves: strong blur near the floor, sharp at the top.

```cpp
        cam.lookfrom = Point3(7 * std::sin(angle), 2.8, 7 * std::cos(angle));
        cam.lookat = Point3(0, 0.6, 0);
```

Lines 72–73: the camera stands on a circle of radius 7 (sin and cos of the angle give the position), 2.8 high, always
looking at the middle: a **turntable** shot.

```cpp
        cam.show_progress = false;
        cam.collect_aovs = true;
        cam.max_sample_value = 20;
        cam.seed = 1 + f;                         // different noise each frame (looks like film grain)
```

* Line 75: no per-frame progress bar (we print our own line below).
* Line 76: collect the denoiser's guides.
* Line 78: a **different random seed per frame**. If every frame had the same noise, the grain would sit still while the
  image moved, which looks like a dirty lens.

```cpp
        Image img = cam.render(world, &lights);
        post::DenoiseSettings ds;
        ds.radius = 4;
        img = post::denoise(img, cam.albedo_aov, cam.normal_aov, ds);
        img = post::tonemap(img, ToneMapper::Aces);
```

Lines 80–84: render with few samples, denoise (that's what makes a 16-sample animation usable), and tone map with ACES.

```cpp
        char name[128];
        std::snprintf(name, sizeof(name), "images/ch39_frames/frame_%03d.png", f);
        write_png(name, img);
        gif.add_frame(img);
        std::printf("frame %d/%d done\n", f + 1, frames);
```

* Lines 86–87: build the file name. `%03d` writes the frame number with **leading zeros** (frame_000, frame_001, ...), so
  the files sort correctly and video tools can read the sequence.
* Line 88: save the frame as a PNG (the folder is created automatically).
* Line 89: add the same image to the GIF (it gets quantized and dithered there).
* Line 90: a progress line.

---

## Block G — Finish (lines 92–95)

```cpp
    gif.end();
    std::printf("Saved images/ch39_turntable.gif\n");
    return 0;
```

Line 92: write the GIF's final byte and close the file. (The destructor would do it too, but being explicit is clearer.)

---

## Check your understanding

1. Why is the torus built outside the loop? *(Building a mesh and its BVH is slow; reusing it saves that work 48 times.)*
2. What makes the animation loop smoothly? *(The camera angle goes exactly once around over all frames.)*
3. Why does each frame use a different seed? *(So the noise changes every frame and reads as film grain.)*
4. Why `%03d` in the file name? *(Leading zeros keep the frames in order.)*
