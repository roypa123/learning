# Line by line: `ch23_bvh.cpp`

[← Line-by-line index](README.md) · [Chapter 23 (the theory)](../23-bvh.md) · [bvh.h](bvh.md) · [aabb.h](aabb.md)

**What the whole program does, in one sentence:** it measures how much faster a BVH is than a plain list for 10, 100,
1000 and 3000 spheres, and draws a picture of how a BVH splits space into boxes.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–12 | |
| B. `time_render` | 14–29 | render a small image and measure the time |
| C. The speed test | 31–49 | list vs BVH for different numbers of spheres |
| D. Picture of the boxes | 51–79 | a 2D drawing of the splits |
| E. End | 80–81 | |

---

## Block A — Comments, includes (lines 1–12)

Comments; `<chrono>` (time measurement); `<cstdio>` (printf); our library; `using namespace pixel`.

---

## Block B — `time_render` (lines 14–29)

```cpp
static double time_render(const Hittable& world, Image* out) {
```

Line 15: renders `world` and returns the time in seconds. `out` = a **pointer** to an image where the result can be
stored, or `nullptr` if we don't need the image.

```cpp
    Camera cam;
    cam.image_width = 320;
    cam.samples_per_pixel = 4;
    cam.max_depth = 4;
    cam.vfov = 35;
    cam.lookfrom = Point3(0, 18, 26);
    cam.lookat = Point3(0, 0, 0);
    cam.show_progress = false;
```

Lines 16–23: a small, fast test render (only 4 samples, 4 bounces), looking down from above, no progress messages.

```cpp
    auto t0 = std::chrono::steady_clock::now();
    Image img = cam.render(world);
    auto t1 = std::chrono::steady_clock::now();
```

Lines 24–26: note the time, render, note the time again. `steady_clock` is a clock meant for measuring durations.

```cpp
    if (out) *out = img;
    return std::chrono::duration<double>(t1 - t0).count();
}
```

* Line 27: if a pointer was given, copy the image there. `*out` = "the image that `out` points to".
* Line 28: the difference between the two times, in seconds as a double.

---

## Block C — The speed test (lines 31–49)

```cpp
int main() {
    const int counts[] = {10, 100, 1000, 3000};
    std::printf("%8s  %12s  %12s  %8s\n", "spheres", "list (s)", "BVH (s)", "speedup");
    Image last;
```

* Line 32: the four scene sizes to test.
* Line 33: print a table header. `%8s` = text right-aligned in 8 spaces (so the columns line up).
* Line 34: will hold the last BVH image.

```cpp
    for (int n : counts) {
        HittableList list;
        list.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5))));
```

* Line 35: for each size `n`.
* Lines 36–37: a new list with a ground sphere.

```cpp
        Pcg32 rng(n);
        for (int i = 0; i < n; i++) {
            Point3 c(rng.next_double() * 30 - 15, 0.3 + rng.next_double() * 3, rng.next_double() * 30 - 15);
            Color col = srgb_to_linear(hsv(rng.next_double() * 360, 0.7, 0.9));
            list.add(std::make_shared<Sphere>(c, 0.3, std::make_shared<Lambertian>(col)));
        }
```

* Line 38: random numbers, seeded with n.
* Line 39: `n` spheres.
* Line 40: a random position in a 30 × 30 area (−15..15), floating 0.3–3.3 units high.
* Line 41: a random bright color (random hue).
* Line 42: a small sphere (radius 0.3).

```cpp
        BVHNode bvh(list);
        double t_list = time_render(list, nullptr);
        double t_bvh = time_render(bvh, &last);
        std::printf("%8d  %12.2f  %12.2f  %7.1fx\n", n, t_list, t_bvh, t_list / t_bvh);
    }
    save_image("images/ch23_many_spheres.png", last);
```

* Line 44: build a BVH from the same list.
* Line 45: time the plain list (don't keep the image).
* Line 46: time the BVH, and keep its image in `last` (`&last` = the address of `last`).
* Line 47: print a row: the size, both times, and the **speedup** (list time ÷ BVH time).
* Line 49: save the image from the largest test.

---

## Block D — Picture of the boxes (lines 51–79)

A 2D drawing of how a BVH divides 64 random points, seen from above.

```cpp
        const int S = 500;
        Image img(S, S, hex_color(0x0B1020));
        Canvas cv(img);
        Pcg32 rng(5);
        std::vector<Point3> pts;
        for (int i = 0; i < 64; i++) pts.push_back(Point3(rng.next_double(), 0, rng.next_double()));
```

* Lines 54–56: a dark 500 × 500 image and a canvas.
* Lines 57–59: 64 random points with x and z between 0 and 1 (y is unused: we look from above).

```cpp
        std::function<void(std::vector<Point3>, int)> split = [&](std::vector<Point3> p, int depth) {
```

Line 62: a **recursive lambda** (it calls itself). It must be stored in a `std::function` (with its type written out) so
that it can refer to itself by name. Inputs: a group of points and how deep in the tree we are.

```cpp
            double minx = 1, maxx = 0, minz = 1, maxz = 0;
            for (auto& q : p) { minx = std::min(minx, q.x); maxx = std::max(maxx, q.x); minz = std::min(minz, q.z); maxz = std::max(maxz, q.z); }
```

Lines 63–64: find the bounding box of the group (the smallest and largest x and z).

```cpp
            Color c = srgb_to_linear(hsv(depth * 50.0, 0.7, 1.0));
            int x0 = (int)(20 + minx * 460) - 6 + depth, z0 = (int)(20 + minz * 460) - 6 + depth;
            int x1 = (int)(20 + maxx * 460) + 6 - depth, z1 = (int)(20 + maxz * 460) + 6 - depth;
            cv.draw_rect(x0, z0, x1 - x0 + 1, z1 - z0 + 1, c);
```

* Line 65: a different color for each depth level (the hue changes by 50° per level).
* Lines 66–67: convert the box to pixels (0–1 → 20–480, leaving a margin). The box is made a little bigger than the
  points (`± 6`), and a little smaller at each deeper level (`± depth`), so nested boxes don't draw on top of each other.
* Line 68: draw the box outline.

```cpp
            if (p.size() <= 2) return;
            int axis = (maxx - minx) > (maxz - minz) ? 0 : 2;
            std::sort(p.begin(), p.end(), [axis](const Point3& a, const Point3& b) { return a[axis] < b[axis]; });
            std::vector<Point3> left(p.begin(), p.begin() + p.size() / 2), right(p.begin() + p.size() / 2, p.end());
            split(left, depth + 1);
            split(right, depth + 1);
        };
```

The same steps as `BVHNode`:

* Line 69: 2 points or fewer: stop (a leaf).
* Line 70: the longer side: x (0) or z (2).
* Line 71: sort the points along that axis.
* Line 72: the first half and the second half, as two new lists.
* Lines 73–74: do the same for each half, one level deeper.

```cpp
        split(pts, 0);
        for (auto& q : pts) cv.fill_circle_aa(20 + q.x * 460, 20 + q.z * 460, 3.5, Color(1, 1, 1));
        save_image("images/ch23_bvh_boxes.png", img);
    }
```

* Line 76: start with all points at depth 0.
* Line 77: draw each point as a small white dot.
* Line 78: save.

---

## Block E — End (lines 80–81)

`return 0;` `}`

---

## Check your understanding

1. Why pass `nullptr` for the list's image? *(We only need its time, not its picture.)*
2. What does the speedup column mean? *(How many times faster the BVH is.)*
3. Why does the `split` lambda need `std::function`? *(A lambda can only call itself if it's stored in a named variable
   with a known type.)*
