# Line by line: `ch20_camera.cpp`

[← Line-by-line index](README.md) · [Chapter 20 (the theory)](../20-positionable-camera.md) · [camera.h: initialize](camera.md)

**What the whole program does, in one sentence:** it renders the same scene three times with different camera settings:
a wide-angle view from above-left, a telephoto (zoomed) view, and a view with depth of field (blurry foreground and
background).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. Materials and world | 11–23 | five spheres |
| C. Camera base settings | 25–28 | |
| D. Wide angle | 30–35 | vfov 90 |
| E. Telephoto | 37–39 | vfov 20 |
| F. Depth of field | 41–44 | lens opening + focus distance |
| G. End | 45–46 | |

---

## Block A — Comments, includes (lines 1–9)

Comments, our library, `using namespace pixel`.

---

## Block B — Materials and world (lines 11–23)

```cpp
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 1.0);
```

Lines 12–16: the same materials as chapter 19, but the gold now has fuzz 1.0 (dull gold).

```cpp
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.4, bubble));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));
```

Lines 18–23: ground, blue ball, hollow glass ball (outer + inner sphere), gold ball.

---

## Block C — Camera base settings (lines 25–28)

```cpp
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
```

One camera object that we modify between renders.

---

## Block D — Wide angle (lines 30–35)

```cpp
    cam.vfov = 90;
    cam.lookfrom = Point3(-2, 2, 1);
    cam.lookat = Point3(0, 0, -1);
    cam.vup = Vec3(0, 1, 0);
    save_image("images/ch20_fov_wide.png", cam.render(world));
```

* Line 31: a **wide** 90° vertical field of view.
* Line 32: the camera stands at (−2, 2, 1): to the left, up high, and in front of the scene.
* Line 33: it looks at (0, 0, −1), the middle of the scene.
* Line 34: "up" is the world's y axis, so the horizon stays level.
* Line 35: render and save. The camera builds its own axes from these settings in `initialize` (see
  [camera.md block J](camera.md)).

---

## Block E — Telephoto (lines 37–39)

```cpp
    cam.vfov = 20;
    save_image("images/ch20_fov_tele.png", cam.render(world));
```

Same position, but only a 20° field of view: it's like zooming in. The balls fill the picture.

---

## Block F — Depth of field (lines 41–44)

```cpp
    cam.defocus_angle = 10.0;
    cam.focus_dist = 3.4;       // distance from lookfrom to the center sphere
    save_image("images/ch20_defocus.png", cam.render(world));
```

* Line 42: open the lens: rays start from random points on a lens disk (a 10° cone). Bigger = blurrier.
* Line 43: the distance that stays **sharp**: 3.4, about the distance from the camera (−2, 2, 1) to the blue ball
  (0, 0, −1.2): √(4 + 4 + 4.84) ≈ 3.58. Things nearer or farther become blurry.
* Line 44: render and save.

---

## Block G — End (lines 45–46)

`return 0;` `}`

---

## Check your understanding

1. What does a smaller `vfov` do? *(Zooms in: a narrower view.)*
2. What does `focus_dist` control? *(Which distance is perfectly sharp.)*
3. What happens with `defocus_angle = 0`? *(No blur: everything is sharp.)*
