# Line by line: `ch31_light_sampling.cpp`

[← Line-by-line index](README.md) · [Chapter 31 (the theory)](../31-light-sampling.md) · [pdf.h](pdf.md) · [camera.h: trace](camera.md)

**What the whole program does, in one sentence:** it renders the Cornell box twice with the same number of samples,
once letting rays wander randomly and once aiming half of them **at the light**, then renders a final version with a
glass ball.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. `build_cornell` | 13–45 | the room, the boxes/ball, and the **lights list** |
| C. `cornell_camera` | 47–58 | shared camera settings |
| D. The comparison | 60–68 | same scene, with and without light sampling |
| E. The final image | 69–74 | mirror box + glass ball, 400 samples |
| F. End | 75–76 | |

---

## Block A — Comments, includes (lines 1–11)

Comments explaining the comparison (left: no light sampling, right: with), our library, `using namespace pixel`.

---

## Block B — `build_cornell` (lines 13–45)

```cpp
static void build_cornell(HittableList& world, HittableList& lights, bool glass_ball) {
```

Line 13: builds the scene. It fills **two** lists (both passed by reference `&`):

* `world` = everything the rays can hit,
* `lights` = the shapes worth **aiming at**.

`glass_ball` chooses between the two versions of the scene.

```cpp
    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));
```

Lines 14–17: the Cornell materials (as in chapter 26).

```cpp
    world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
    world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
    world.add(std::make_shared<Quad>(Point3(0, 555, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));
```

Lines 19–24: the six walls: green, red, the ceiling light panel, ceiling, floor, back wall.

```cpp
    auto aluminum = std::make_shared<Metal>(Color(0.8, 0.85, 0.88), 0.0);
    std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), glass_ball ? aluminum : white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
    world.add(box1);
```

* Line 26: a mirror-like aluminium material.
* Line 27: the tall box: **aluminium** in the final version, plain white in the comparison version
  (`condition ? a : b`).
* Lines 28–30: rotate 15°, move into place, add.

```cpp
    if (glass_ball) {
        world.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, std::make_shared<Dielectric>(1.5)));
    } else {
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);
    }
```

Lines 32–39: the second object: a **glass ball** in the final version, or the small white box in the comparison.

```cpp
    lights.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), nullptr));
    if (glass_ball)   // glass focuses light (caustics): aiming at it helps too
        lights.add(std::make_shared<Sphere>(Point3(190, 90, 190), 90, nullptr));
```

* Line 42: a **copy of the light's shape** goes into the lights list. Note `nullptr` as the material: this copy is never
  drawn, it only tells the renderer "important directions are over here". The real, glowing light is the one in `world`.
* Lines 43–44: for the glass version, also aim at the glass ball. Glass focuses light into a bright spot (a **caustic**),
  which random bounces almost never find.

---

## Block C — `cornell_camera` (lines 47–58)

```cpp
static Camera cornell_camera(int spp) {
    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = spp;
    cam.max_depth = 50;
    cam.background = solid_background(Color(0, 0, 0));
    cam.vfov = 40;
    cam.lookfrom = Point3(278, 278, -800);
    cam.lookat = Point3(278, 278, 0);
    return cam;
}
```

The standard Cornell camera, with the number of samples as an input so we can reuse it.

---

## Block D — The comparison (lines 60–68)

```cpp
        HittableList world, lights;
        build_cornell(world, lights, false);
        Camera cam = cornell_camera(64);
        Image without = cam.render(world, nullptr);
        Image with = cam.render(world, &lights);
        save_image("images/ch31_compare.png", post::side_by_side(without, with, 8));
```

* Lines 62–63: build the two-box scene and its lights list.
* Line 64: a camera with only 64 samples.
* Line 65: render **without** light sampling (`nullptr` = no lights list): rays bounce randomly and must find the small
  ceiling light by luck → very noisy.
* Line 66: render the **same** scene with the lights list: half of every diffuse bounce is aimed at the light → clean.
* Line 67: save both side by side.

Both images are correct; only the noise differs.

---

## Block E — The final image (lines 69–74)

```cpp
        HittableList world, lights;
        build_cornell(world, lights, true);
        Camera cam = cornell_camera(400);
        save_image("images/ch31_cornell_final.png", cam.render(world, &lights));
```

The glass version with 400 samples: the showpiece of the book's middle part (mirror box, glass ball and its caustic).

---

## Block F — End (lines 75–76)

`return 0;` `}`

---

## Check your understanding

1. Why do the light shapes in `lights` have `nullptr` materials? *(They're only used to choose directions; the glowing
   copy lives in `world`.)*
2. What is the only difference between the two renders in block D? *(Whether the lights list is passed to `render`.)*
3. Why add the glass ball to the lights list? *(To find the caustic it focuses, which random bounces rarely hit.)*
