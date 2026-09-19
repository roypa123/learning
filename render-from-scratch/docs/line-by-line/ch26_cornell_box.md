# Line by line: `ch26_cornell_box.cpp`

[← Line-by-line index](README.md) · [Chapter 26 (the theory)](../26-lights-cornell-box.md) · [material.h: DiffuseLight](material.md)

**What the whole program does, in one sentence:** it renders two scenes lit only by glowing objects (no sky): a marble
ball lit by a glowing sphere and a glowing panel, and the famous empty Cornell box.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. Scene 1: area lights | 13–33 | marble ball + two lights, black background |
| C. Scene 2: Cornell box | 35–61 | five walls + a ceiling light |
| D. End | 62–63 | |

---

## Block A — Comments, includes (lines 1–10)

Comments (these images are **noisy** on purpose; chapter 31 fixes that), our library, `using namespace pixel`.

---

## Block B — Scene 1: area lights (lines 13–33)

```cpp
        auto marble = std::make_shared<NoiseTexture>(4.0, NoiseTexture::Marble);
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(marble)));
        world.add(std::make_shared<Sphere>(Point3(0, 2, 0), 2, std::make_shared<Lambertian>(marble)));
```

* Line 15: a marble texture.
* Lines 17–18: a marble ground and a marble ball (radius 2, resting on the ground).

```cpp
        auto difflight = std::make_shared<DiffuseLight>(Color(4, 4, 4));
        world.add(std::make_shared<Sphere>(Point3(0, 7, 0), 2, difflight));
        world.add(std::make_shared<Quad>(Point3(3, 1, -2), Vec3(2, 0, 0), Vec3(0, 2, 0), difflight));
```

* Line 20: a **light material**: it glows with brightness 4 (4 times brighter than white).
* Line 21: a glowing sphere above the ball.
* Line 22: a glowing 2 × 2 panel to the side. Its front (u × v = +z) faces the ball and the camera.

```cpp
        Camera cam;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));   // no sky: darkness
        cam.vfov = 20;
        cam.lookfrom = Point3(26, 3, 6);
        cam.lookat = Point3(0, 2, 0);
        save_image("images/ch26_light_quad.png", cam.render(world));
```

* Line 25: 200 samples: lights that only rays find by chance need many samples.
* Line 28: a **black background**: rays that escape bring back no light, so all light comes from the two glowing objects.
* Lines 29–31: a telephoto view from far away.
* Line 32: render and save.

---

## Block C — Scene 2: Cornell box (lines 35–61)

```cpp
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));
```

Lines 37–40: the classic Cornell box materials: red, white and green matte walls, and a bright light (15). `.65` is
written without the leading zero (= 0.65).

```cpp
        HittableList world;
        world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
```

* Line 43: the green wall at x = 555 (a 555 × 555 square in y and z).
* Line 44: the red wall at x = 0. (Seen from the camera, green is on the left and red on the right.)

```cpp
        world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
```

Line 46: the ceiling light: a 130 × 105 panel just below the ceiling (y = 554). The comment on line 45 explains the edge
order: u × v points **down**, so the light's front shines into the room (a light only glows on its front side).

```cpp
        world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
        world.add(std::make_shared<Quad>(Point3(555, 555, 555), Vec3(-555, 0, 0), Vec3(0, 0, -555), white));
        world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));
```

Lines 47–49: the white floor (y = 0), ceiling (y = 555) and back wall (z = 555). The front (z = 0) stays open for the camera.

```cpp
        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch26_cornell_empty.png", cam.render(world));
```

* Lines 51–56: square image, 200 samples, black background (the only light is the ceiling panel).
* Lines 57–59: the camera stands in front of the open side (z = −800), at the height of the box's center (278 ≈ 555 ÷ 2),
  looking straight in.
* Line 60: render and save.

---

## Block D — End (lines 62–63)

`return 0;` `}`

---

## Check your understanding

1. What makes the scenes dark except near the lights? *(The black background: no sky light.)*
2. Why does the order of the light quad's edges matter? *(u × v decides its front side, and lights only glow on the front.)*
3. Why are these images noisy? *(Random bounces rarely find the small lights; chapter 31 aims rays at them.)*
