# Line by line: `ch18_metal.cpp`

[← Line-by-line index](README.md) · [Chapter 18 (the theory)](../18-metal.md) · [material.h: Metal](material.md)

**What the whole program does, in one sentence:** it renders three balls (matte blue, silver metal, gold metal) twice:
once as perfect mirrors, once with "fuzz" for brushed-metal reflections.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–8 | |
| B. `render_scene` | 10–27 | build the scene with two fuzz values, render it |
| C. `main` | 29–33 | render and save both versions |

---

## Block A — Comments, includes (lines 1–8)

Comments, our library, `using namespace pixel`.

---

## Block B — `render_scene` (lines 10–27)

```cpp
static Image render_scene(double fuzz_left, double fuzz_right) {
```

Line 10: a helper that builds and renders the scene. Its inputs are the fuzz of the left and right metal balls, so we
can reuse it with different values. It returns the image.

```cpp
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto left   = std::make_shared<Metal>(Color(0.8, 0.8, 0.8), fuzz_left);
    auto right  = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), fuzz_right);
```

Lines 11–14: four materials:

* ground: matte yellow-green,
* center: matte dark blue,
* left: **silver metal** (reflects 80% of every color),
* right: **gold metal** (more red and green than blue).

```cpp
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, left));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, right));
```

Lines 16–20: the ground and three balls in a row: silver at x = −1 (left), blue in the middle (a bit farther back,
z = −1.2), gold at x = +1 (right). The extra spaces just line the numbers up for readability.

```cpp
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    return cam.render(world);
}
```

Lines 22–26: a default camera (at the origin, looking at −z, 90° view), 100 samples, 50 bounces. Render and return the
image directly.

---

## Block C — `main` (lines 29–33)

```cpp
int main() {
    save_image("images/ch18_metal_mirror.png", render_scene(0.0, 0.0));
    save_image("images/ch18_metal_fuzzy.png", render_scene(0.3, 1.0));
    return 0;
}
```

* Line 30: fuzz 0 for both metals: **perfect mirrors**.
* Line 31: fuzz 0.3 (left: brushed silver) and 1.0 (right: dull gold).

Each call renders the whole scene and passes the resulting image straight to `save_image`.

---

## Check your understanding

1. Why is `render_scene` a function with parameters? *(To render the same scene twice with different fuzz.)*
2. Which ball tints its reflections? *(Both metals: silver slightly (0.8), gold strongly (less blue).)*
3. What happens to the right ball's reflections with fuzz 1.0? *(They blur almost completely.)*
