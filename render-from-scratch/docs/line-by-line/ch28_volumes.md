# Line by line: `ch28_volumes.cpp`

[← Line-by-line index](README.md) · [Chapter 28 (the theory)](../28-volumes.md) · [volume.h](volume.md)

**What the whole program does, in one sentence:** it turns the Cornell box's two boxes into black smoke and white fog,
then renders a fluffy cloud whose thickness comes from noise, under a daylight sky.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. Scene 1: the room | 12–25 | walls and a bigger light |
| C. Scene 1: smoke and fog | 27–35 | two volumes shaped like the boxes |
| D. Scene 1: camera | 37–46 | |
| E. Scene 2: the cloud's density | 49–58 | a squashed ball + noise |
| F. Scene 2: world, camera, sky | 59–74 | |
| G. End | 76–77 | |

---

## Block A — Comments, includes (lines 1–9)

Comments, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — Scene 1: the room (lines 12–25)

```cpp
        auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
        auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
        auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));
```

Lines 14–17: the Cornell materials, but a dimmer light (7)...

```cpp
        world.add(std::make_shared<Quad>(Point3(113, 554, 127), Vec3(330, 0, 0), Vec3(0, 0, 305), light));
```

Line 22: ...that is **much bigger** (330 × 305 instead of 130 × 105). A big light makes less noise, which helps volumes
(they are noisy by nature). u × v points down, into the room.

Lines 20–21 and 23–25: the green, red and three white walls.

---

## Block C — Scene 1: smoke and fog (lines 27–35)

```cpp
        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
```

Lines 27–32: the same two rotated boxes as chapter 27. But they're **not added** to the world as solid boxes...

```cpp
        world.add(std::make_shared<ConstantMedium>(box1, 0.01, Color(0, 0, 0)));   // black smoke
        world.add(std::make_shared<ConstantMedium>(box2, 0.01, Color(1, 1, 1)));   // white fog
```

...instead, they're used as the **shapes** of two volumes:

* Line 34: black smoke (particles absorb all light) in the tall box's shape. Density 0.01 → a ray travels about 100
  units on average before hitting a particle (the box is 165 wide, so it's semi-transparent).
* Line 35: white fog (particles scatter all light) in the short box's shape.

---

## Block D — Scene 1: camera (lines 37–46)

The Cornell camera (square image, 200 samples, black background, looking into the open front). Render and save.

---

## Block E — Scene 2: the cloud's density (lines 49–58)

```cpp
        auto noise = std::make_shared<Perlin>(17);
```

Line 51: a Perlin noise generator, held by a shared pointer so the lambda below can keep it alive.

```cpp
        auto density = [noise](const Point3& p) {
            Vec3 q(p.x / 2.2, p.y / 1.1, p.z / 1.6);
            double shape = 1.0 - q.length();                         // 1 at center, 0 at the edge
            double n = noise->fbm(p * 1.3, 5);                       // -1 .. 1
            return std::fmax(0.0, shape + 0.6 * n) * 4.0;            // clamp to >= 0
        };
```

`density` = a function: how thick the cloud is at point p.

* Line 54: squash space: dividing by different numbers makes the cloud wide (2.2), flat (1.1) and medium deep (1.6).
* Line 55: `shape` = 1 in the middle, falling to 0 at the edge of the squashed ball (and negative outside).
* Line 56: noise, so the edges become irregular and puffy.
* Line 57: shape plus some noise, never below 0, times 4 (to make it thick). Maximum ≈ (1 + 0.6) × 4 = 6.4.

```cpp
        auto bounds = make_box(Point3(-2.6, -1.4, -2.0), Point3(2.6, 1.4, 2.0), nullptr);
```

Line 59: a box around the whole cloud (its boundary). No material needed (it's only a shape), so `nullptr`.

---

## Block F — Scene 2: world, camera, sky (lines 60–74)

```cpp
        HittableList world;
        world.add(std::make_shared<VariableMedium>(bounds, 7.0, density, Color(0.95, 0.95, 0.95)));
        world.add(std::make_shared<Sphere>(Point3(0, -1003, 0), 1000, std::make_shared<Lambertian>(hex_color(0x5B7F5A))));
```

* Line 61: the cloud: a variable-density volume in the box, with maximum density 7.0 (a little above the real maximum
  6.4, as delta tracking requires), and nearly white particles.
* Line 62: green ground far below (its top is at y = −3).

```cpp
        Camera cam;
        cam.image_width = 480;
        cam.samples_per_pixel = 128;
        cam.max_depth = 30;
        cam.vfov = 40;
        cam.lookfrom = Point3(0, 1, 9);
        cam.lookat = Point3(0, 0.3, 0);
        SkySettings sky;
        sky.sun_direction = unit_vector(Vec3(0.5, 0.6, 0.3));
        cam.background = physical_sky(sky);
        save_image("images/ch28_cloud.png", cam.render(world));
```

* Lines 64–70: a camera in front of the cloud, 128 samples, up to 30 bounces (light bounces many times inside a cloud).
* Lines 71–73: a daylight sky ([sky.md](sky.md)) with the sun's glow up and to the right.
* Line 74: render and save.

---

## Block G — End (lines 76–77)

`return 0;` `}`

---

## Check your understanding

1. Why aren't the boxes added to the world as solid boxes? *(They're only used as the shapes of the volumes.)*
2. What's the difference between black smoke and white fog here? *(Only the particle color: black absorbs, white scatters.)*
3. Why must `max_density` (7.0) be at least the real maximum? *(Delta tracking accepts steps with chance density/max;
   that chance must never be above 1.)*
