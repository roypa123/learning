# Line by line: `ch21_first_masterpiece.cpp`

[← Line-by-line index](README.md) · [Chapter 21 (the theory)](../21-multithreading-final-scene.md) · [camera.h: render](camera.md)

**What the whole program does, in one sentence:** it builds a scene of about 480 small random balls (matte, metal and
glass) plus three big ones, and renders it with a telephoto lens and slight depth of field, using all CPU cores.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. Quality setting from the command line | 12–13 | `final` or preview |
| C. Ground | 15–17 | |
| D. Random generator | 19–20 | a fixed seed and a short helper |
| E. The small balls | 22–40 | a grid of random balls with random materials |
| F. The three big balls | 41–43 | |
| G. BVH | 45–46 | speed-up structure |
| H. Camera and render | 48–62 | |

---

## Block A — Comments, includes (lines 1–10)

Comments (including how to run the `final` version), `<string>`, our library, `using namespace pixel`.

---

## Block B — Quality setting (lines 12–13)

```cpp
int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";
```

* Line 12: this `main` receives the **command-line words**: `argc` = how many words (including the program name), `argv`
  = the words themselves. `run ch21_first_masterpiece final` → argc = 2, argv[1] = "final".
* Line 13: `final_quality` is true only if there is a second word and it's `"final"`. `std::string(...)` turns the C text
  into a string so `==` compares the letters. Note `&&` stops early: if argc is 1, `argv[1]` is never touched.

---

## Block C — Ground (lines 15–17)

```cpp
    HittableList world;
    auto ground_material = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, ground_material));
```

A **huge** grey ball (radius 1000) whose top is at y = 0: a flat-looking floor.

---

## Block D — Random generator (lines 19–20)

```cpp
    Pcg32 rng(2024);   // fixed seed: the same scene every time
    auto rnd = [&]() { return rng.next_double(); };
```

* Line 19: our own random generator with a fixed seed: the scene is the same every run.
* Line 20: `rnd` = a tiny lambda so we can write `rnd()` instead of `rng.next_double()`. `[&]` = it uses `rng` directly.

---

## Block E — The small balls (lines 22–40)

```cpp
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
```

Lines 22–23: a 22 × 22 grid of positions (a and b from −11 to 10).

```cpp
            double choose_mat = rnd();
            Point3 center(a + 0.9 * rnd(), 0.2, b + 0.9 * rnd());
            if ((center - Point3(4, 0.2, 0)).length() <= 0.9) continue;
```

* Line 24: a random number to choose the material.
* Line 25: the ball's center: the grid position moved randomly by up to 0.9, at height 0.2 (the radius, so it sits on
  the floor).
* Line 26: skip balls that would overlap the big metal ball at (4, 1, 0).

```cpp
            std::shared_ptr<Material> mat;
            if (choose_mat < 0.8) {            // 80% matte
                Color albedo(rnd() * rnd(), rnd() * rnd(), rnd() * rnd());
                mat = std::make_shared<Lambertian>(albedo);
```

* Line 28: an empty material pointer, filled below.
* Lines 29–31: 80% chance: matte with a random color. Multiplying two random numbers (`rnd() × rnd()`) makes **darker**
  values more likely, giving deeper, richer colors.

```cpp
            } else if (choose_mat < 0.95) {    // 15% metal
                Color albedo(0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd());
                mat = std::make_shared<Metal>(albedo, 0.5 * rnd());
```

Lines 32–34: 15% chance: metal with a light color (0.5–1.0 per channel) and a random fuzz of 0–0.5.

```cpp
            } else {                           // 5% glass
                mat = std::make_shared<Dielectric>(1.5);
            }
            world.add(std::make_shared<Sphere>(center, 0.2, mat));
        }
    }
```

* Lines 35–37: the remaining 5%: glass.
* Line 38: add the ball (radius 0.2) with its material.

---

## Block F — The three big balls (lines 41–43)

```cpp
    world.add(std::make_shared<Sphere>(Point3(0, 1, 0), 1.0, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(-4, 1, 0), 1.0, std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1))));
    world.add(std::make_shared<Sphere>(Point3(4, 1, 0), 1.0, std::make_shared<Metal>(Color(0.7, 0.6, 0.5), 0.0)));
```

Three radius-1 balls: glass in the middle, matte brown on the left, mirror bronze on the right.

---

## Block G — BVH (lines 45–46)

```cpp
    BVHNode bvh(world);
```

Line 46: build a **BVH** (a tree of boxes, chapter 23) from the world. Testing a ray against the tree is much faster than
testing it against all ~480 balls one by one. The BVH is a Hittable, so the camera can render it like the list.

---

## Block H — Camera and render (lines 48–62)

```cpp
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = final_quality ? 1200 : 600;
    cam.samples_per_pixel = final_quality ? 300 : 50;
    cam.max_depth = 50;
```

Lines 49–52: widescreen; 1200 pixels and 300 samples for `final`, otherwise 600 pixels and 50 samples (about 20 times
less work).

```cpp
    cam.vfov = 20;
    cam.lookfrom = Point3(13, 2, 3);
    cam.lookat = Point3(0, 0, 0);
    cam.vup = Vec3(0, 1, 0);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;
```

* Line 53: a narrow 20° view (telephoto).
* Lines 54–56: the camera is far away at (13, 2, 3), low, looking at the center.
* Lines 57–58: a small lens opening, focused at distance 10 (about where the big balls are).

```cpp
    save_image("images/ch21_random_spheres.png", cam.render(bvh));
    return 0;
}
```

Line 60: render the BVH (using all cores, see [camera.md block D–F](camera.md)) and save.

---

## Check your understanding

1. What is `argv[1]` when you run `run ch21_first_masterpiece final`? *("final")*
2. Why is `rnd() * rnd()` used for matte colors? *(It makes dark values more likely: richer colors.)*
3. Why is line 26 needed? *(To keep small balls out of the big metal ball.)*
4. What does the BVH change in the image? *(Nothing: it only makes rendering faster.)*
