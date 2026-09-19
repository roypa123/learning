# Line by line: `ch15_normals.cpp`

[← Line-by-line index](README.md) · [Chapter 15 (the theory)](../15-normals-and-lists.md) · [hittable.h](hittable.md) · [sphere.h](sphere.md)

**What the whole program does, in one sentence:** it puts two spheres (a small one and a huge "ground" one) into a list,
shoots a ray per pixel, and colors each hit by the direction its surface faces (its normal).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–9 | |
| B. `ray_color` | 11–20 | hit → normal color; miss → sky |
| C. The world | 22–27 | two spheres in a list |
| D. Camera | 29–33 | as in chapter 14 |
| E. Render, save, end | 35–45 | |

---

## Block A — Comments, includes (lines 1–9)

Comments, our library, `using namespace pixel`.

---

## Block B — `ray_color` (lines 11–20)

```cpp
Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) {
        return 0.5 * (rec.normal + Color(1, 1, 1));
    }
```

* Line 11: the color seen by a ray. `world` is any Hittable (here: a list of spheres). Passing `const Hittable&` lets
  this function work with **any** kind of world.
* Line 12: an empty hit record to be filled in.
* Line 13: ask the world: is anything hit at a distance between 0 and infinity? If yes, `rec` now describes the
  **closest** hit.
* Line 15: color by the normal. Each part of the normal is −1..1; adding 1 gives 0..2 and halving gives 0..1: a valid
  color. Facing up → green, right → red, toward us → blue.

```cpp
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}
```

Lines 17–19: nothing hit: the sky (as in chapter 13).

---

## Block C — The world (lines 22–27)

```cpp
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));
```

* Line 25: an empty list of objects.
* Line 26: a small sphere, radius 0.5, 1 unit in front of us.
  * `std::make_shared<Sphere>(...)` creates a Sphere and wraps it in a shared pointer (see chapter 2).
  * `nullptr` = no material yet (we don't need one: we only color by normals).
* Line 27: a **huge** sphere (radius 100) whose top is at y = −100.5 + 100 = **−0.5**, just under the small sphere.
  Seen from close up, its top looks like flat ground.

---

## Block D — Camera (lines 29–33)

```cpp
    const int W = 400, H = 225;
    const double vw = 2.0 * W / H;
    ...
```

The same compact camera as chapter 14, but the viewport width `vw` is computed (2 × 400 ÷ 225 ≈ 3.56) instead of
typed in.

---

## Block E — Render, save, end (lines 35–45)

```cpp
    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            img.at(i, j) = ray_color(r, world);
        }
```

For each pixel: build the ray through its center and store the color from `ray_color`. Then save without sRGB (as in
chapters 13–14) and return 0.

---

## Check your understanding

1. What color is a surface facing straight up? *(0.5 × ((0,1,0) + (1,1,1)) = (0.5, 1, 0.5): light green.)*
2. Why does the small sphere hide the ground behind it? *(The list keeps only the closest hit.)*
3. Where is the top of the ground sphere? *(y = −0.5.)*
