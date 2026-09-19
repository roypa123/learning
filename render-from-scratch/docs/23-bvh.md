# Chapter 23 — Bounding volume hierarchies (making it fast)

[← Motion blur](22-motion-blur.md) · [Contents](README.md) · [Next: Textures →](24-textures.md)

---

## Goal

Our `HittableList` tests **every** object for **every** ray. With 10 objects that's fine; with a million
triangles (a film character), it's hopeless. In this chapter you'll build the data structure that makes
huge scenes possible:

* **axis-aligned bounding boxes** (AABBs) and the fast **slab test**,
* a **bounding volume hierarchy** (BVH): boxes inside boxes,
* how to build and traverse it,
* measured speedups from 1× to 100×+.

---

## 1. The problem: O(N) per ray

If a scene has N objects and the image needs R rays, the list approach does **N × R** intersection
tests. A 1920×1080 image with 100 samples and 5 bounces is ~1 billion rays. With a million triangles,
that's 10¹⁵ tests. Impossible.

The fix is the same idea as looking up a word in a dictionary: you don't read every page, you jump to
the right section. We want to test only the objects **near the ray**.

---

## 2. Bounding boxes

An **axis-aligned bounding box** (AABB) is the smallest box, with sides parallel to the x, y and z axes,
that contains an object:

```
      ┌─────────────┐
      │    ▄▄▄▄     │     the box around a sphere: center ± radius on each axis
      │  ▄██████▄   │
      │  ████████   │     if a ray misses the box, it certainly misses the object
      │   ▀████▀    │
      └─────────────┘
```

Testing a ray against a box is **much cheaper** than testing against a complex object, and if the ray
misses the box, we can skip whatever is inside.

### 2.1 The slab test

A box is the overlap of three **slabs**: the space between two parallel planes on each axis
(`x_min ≤ x ≤ x_max`, and the same for y and z). For each axis we compute where the ray enters and
leaves that slab:

```
t0 = (x_min − origin.x) / direction.x
t1 = (x_max − origin.x) / direction.x        (swap if t0 > t1, when the ray goes in −x)
```

The ray is inside the **box** while it's inside **all three slabs** at the same time. That's the overlap
of the three `[t0, t1]` intervals:

```
         x-slab                     ray's t-intervals:
       │         │                   x: [2 ─────── 7]
  ─────┼─────────┼──── y-slab        y:     [4 ──────── 9]
       │    ▓▓▓▓▓│                   overlap: [4 ── 7]  -> not empty -> HIT
  ─────┼─────────┼────               (if the overlap is empty, the ray passes beside the box)
       │         │
```

```cpp
for (int axis = 0; axis < 3; axis++) {
    double adinv = 1.0 / d[axis];
    double t0 = (ax.min - o[axis]) * adinv;
    double t1 = (ax.max - o[axis]) * adinv;
    if (t0 > t1) std::swap(t0, t1);
    if (t0 > ray_t.min) ray_t.min = t0;     // shrink the interval to the overlap
    if (t1 < ray_t.max) ray_t.max = t1;
    if (ray_t.max <= ray_t.min) return false;
}
return true;
```

A ray parallel to an axis has `direction = 0` there, so `1/0 = ±infinity`. IEEE floating point handles
that gracefully: the ray is inside that slab forever, or never.

**Padding**: a flat quad has a box with zero thickness, and zero-thickness slabs can fail because of
rounding. `pad_to_minimums()` gives every side at least 0.0001 thickness.

### 2.2 `aabb.h`

**File: `include/pixel/aabb.h`**

```cpp
// pixel/aabb.h
// ------------------------------------------------------------
// AABB = Axis-Aligned Bounding Box: the simplest box around an object.
// Testing a ray against a box is very cheap, so we test boxes first.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include "vec3.h"
#include "ray.h"

namespace pixel {

class AABB {
public:
    Interval x, y, z;

    AABB() {}   // empty box
    AABB(const Interval& ix, const Interval& iy, const Interval& iz) : x(ix), y(iy), z(iz) {
        pad_to_minimums();
    }
    // Box spanned by two corner points (in any order).
    AABB(const Point3& a, const Point3& b) {
        x = a.x <= b.x ? Interval(a.x, b.x) : Interval(b.x, a.x);
        y = a.y <= b.y ? Interval(a.y, b.y) : Interval(b.y, a.y);
        z = a.z <= b.z ? Interval(a.z, b.z) : Interval(b.z, a.z);
        pad_to_minimums();
    }
    // Box around two boxes.
    AABB(const AABB& a, const AABB& b) : x(a.x, b.x), y(a.y, b.y), z(a.z, b.z) {}

    const Interval& axis_interval(int n) const { return n == 1 ? y : (n == 2 ? z : x); }

    // The "slab method": intersect the ray with 3 pairs of parallel planes.
    bool hit(const Ray& r, Interval ray_t) const {
        const Point3& o = r.origin();
        const Vec3& d = r.direction();
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / d[axis];
            double t0 = (ax.min - o[axis]) * adinv;
            double t1 = (ax.max - o[axis]) * adinv;
            if (t0 > t1) { double tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > ray_t.min) ray_t.min = t0;
            if (t1 < ray_t.max) ray_t.max = t1;
            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }

    int longest_axis() const {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }

    Point3 centroid() const { return Point3((x.min + x.max) * 0.5, (y.min + y.max) * 0.5, (z.min + z.max) * 0.5); }

    double surface_area() const {
        double a = x.size(), b = y.size(), c = z.size();
        return 2.0 * (a * b + b * c + c * a);
    }

    static AABB empty() { return AABB(Interval::empty(), Interval::empty(), Interval::empty()); }

private:
    // A flat box (e.g. around a flat quad) has zero thickness, which breaks
    // the math. Give every side at least a tiny thickness.
    void pad_to_minimums() {
        const double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

inline AABB operator+(const AABB& b, const Vec3& offset) {
    return AABB(b.x + offset.x, b.y + offset.y, b.z + offset.z);
}

} // namespace pixel
```

---

## 3. A hierarchy of boxes

One box around everything doesn't help much. The trick: **boxes inside boxes**.

```
                        [ box around ALL ]
                        ╱                ╲
          [ box: left half ]          [ box: right half ]
            ╱          ╲                ╱            ╲
      [box: 2]      [box: 2]      [box: 2]        [box: 2]
       ╱   ╲         ╱   ╲         ╱   ╲           ╱   ╲
      ●     ●       ●     ●       ●     ●         ●     ●     <- objects (leaves)
```

To find what a ray hits:

1. Test the root box. Miss? Done: the ray hits nothing.
2. Hit? Test both child boxes. Only descend into the children the ray hits.
3. At the leaves, test the actual objects.

A ray usually passes through only a few boxes on each level, so the work grows with the **depth** of
the tree, which is about log₂(N): 20 levels for a million objects instead of a million tests.

| objects N | list: tests per ray | BVH: ~levels |
|-----------|---------------------|--------------|
| 10 | 10 | 4 |
| 1,000 | 1,000 | 10 |
| 1,000,000 | 1,000,000 | 20 |

### 3.1 Building the tree

We build it top-down, recursively:

1. Compute the box around all objects in this node.
2. Find its **longest axis** (x, y or z).
3. Sort the objects by the center of their boxes along that axis.
4. Split the sorted list in half: the left half becomes the left child, the right half the right child.
5. Repeat for each child, until a node has 1 or 2 objects.

```
 all objects, longest axis = x          sorted by x, split at the median
  ●    ●  ●     ●●   ●   ●  ●     ->    [ ● ● ● ● ] [ ● ● ● ● ]
```

### 3.2 Traversal: finding the closest hit

```cpp
bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    if (!bbox.hit(r, ray_t)) return false;                       // miss the box: skip everything inside
    bool hit_left = left->hit(r, ray_t, rec);
    bool hit_right = right->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);
    return hit_left || hit_right;
}
```

Notice the same trick as `HittableList`: if the left side found a hit at distance t, the right side
only looks for **closer** hits, and its box test fails immediately when the whole box is farther away.

### 3.3 `bvh.h`

**File: `include/pixel/bvh.h`**

```cpp
// pixel/bvh.h
// ------------------------------------------------------------
// BVH = Bounding Volume Hierarchy. A tree of boxes-inside-boxes.
// Instead of testing the ray against ALL objects (slow), we test
// big boxes first and skip everything inside boxes we miss.
// 1,000,000 objects need only ~20 box tests per ray.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include "hittable.h"
#include "aabb.h"

namespace pixel {

class BVHNode : public Hittable {
public:
    BVHNode(HittableList list) : BVHNode(list.objects, 0, list.objects.size()) {}

    BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
        // 1. Box around everything in this node.
        bbox = AABB::empty();
        for (size_t i = start; i < end; i++) bbox = AABB(bbox, objects[i]->bounding_box());

        // 2. Split along the longest side of that box.
        int axis = bbox.longest_axis();
        size_t span = end - start;

        if (span == 1) {
            left = right = objects[start];
        } else if (span == 2) {
            left = objects[start];
            right = objects[start + 1];
        } else {
            // Sort by the center of each object's box along the axis, split in half.
            auto comparator = [axis](const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b) {
                return a->bounding_box().centroid()[axis] < b->bounding_box().centroid()[axis];
            };
            std::sort(objects.begin() + start, objects.begin() + end, comparator);
            size_t mid = start + span / 2;
            left = std::make_shared<BVHNode>(objects, start, mid);
            right = std::make_shared<BVHNode>(objects, mid, end);
        }
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;           // missed the whole box: skip all
        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);
        return hit_left || hit_right;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> left;
    std::shared_ptr<Hittable> right;
    AABB bbox;
};

} // namespace pixel
```

A `BVHNode` is itself a `Hittable`, so you can use it anywhere a list is used: `BVHNode bvh(world);
cam.render(bvh);`

---

## 4. Better splits (for later)

Splitting at the median is simple and decent. Production renderers use the **Surface Area Heuristic
(SAH)**: try many split positions and pick the one that minimizes *expected cost ≈ (area of left box ×
objects on the left) + (area of right box × objects on the right)*. Big empty boxes are expensive,
because rays often hit them for nothing. `AABB::surface_area()` is ready for when you want to try it.

---

## 5. The program

**File: `chapters/ch23_bvh.cpp`**

```cpp
// ch23_bvh.cpp
// ------------------------------------------------------------
// Chapter 23: Bounding Volume Hierarchies.
// We render the same scene of N spheres twice - once with a plain list
// (test every sphere for every ray) and once with a BVH - and time both.
//   images/ch23_many_spheres.png
//   images/ch23_bvh_boxes.png   - a picture of the BVH boxes themselves
// ------------------------------------------------------------
#include <chrono>
#include <cstdio>
#include "pixel/pixel.h"
using namespace pixel;

// Time how long it takes to render a small image of the world.
static double time_render(const Hittable& world, Image* out) {
    Camera cam;
    cam.image_width = 320;
    cam.samples_per_pixel = 4;
    cam.max_depth = 4;
    cam.vfov = 35;
    cam.lookfrom = Point3(0, 18, 26);
    cam.lookat = Point3(0, 0, 0);
    cam.show_progress = false;
    auto t0 = std::chrono::steady_clock::now();
    Image img = cam.render(world);
    auto t1 = std::chrono::steady_clock::now();
    if (out) *out = img;
    return std::chrono::duration<double>(t1 - t0).count();
}

int main() {
    const int counts[] = {10, 100, 1000, 3000};
    std::printf("%8s  %12s  %12s  %8s\n", "spheres", "list (s)", "BVH (s)", "speedup");
    Image last;
    for (int n : counts) {
        HittableList list;
        list.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5))));
        Pcg32 rng(n);
        for (int i = 0; i < n; i++) {
            Point3 c(rng.next_double() * 30 - 15, 0.3 + rng.next_double() * 3, rng.next_double() * 30 - 15);
            Color col = srgb_to_linear(hsv(rng.next_double() * 360, 0.7, 0.9));
            list.add(std::make_shared<Sphere>(c, 0.3, std::make_shared<Lambertian>(col)));
        }
        BVHNode bvh(list);
        double t_list = time_render(list, nullptr);
        double t_bvh = time_render(bvh, &last);
        std::printf("%8d  %12.2f  %12.2f  %7.1fx\n", n, t_list, t_bvh, t_list / t_bvh);
    }
    save_image("images/ch23_many_spheres.png", last);

    // ----- Visualize the boxes: a top-down view of the BVH split planes.
    // We draw each sphere as a dot and recursively draw the split boxes on a 2D canvas.
    {
        const int S = 500;
        Image img(S, S, hex_color(0x0B1020));
        Canvas cv(img);
        Pcg32 rng(5);
        std::vector<Point3> pts;
        for (int i = 0; i < 64; i++) pts.push_back(Point3(rng.next_double(), 0, rng.next_double()));

        // Recursive median split (same idea as BVHNode), drawn as rectangles.
        std::function<void(std::vector<Point3>, int)> split = [&](std::vector<Point3> p, int depth) {
            double minx = 1, maxx = 0, minz = 1, maxz = 0;
            for (auto& q : p) { minx = std::min(minx, q.x); maxx = std::max(maxx, q.x); minz = std::min(minz, q.z); maxz = std::max(maxz, q.z); }
            Color c = srgb_to_linear(hsv(depth * 50.0, 0.7, 1.0));
            int x0 = (int)(20 + minx * 460) - 6 + depth, z0 = (int)(20 + minz * 460) - 6 + depth;
            int x1 = (int)(20 + maxx * 460) + 6 - depth, z1 = (int)(20 + maxz * 460) + 6 - depth;
            cv.draw_rect(x0, z0, x1 - x0 + 1, z1 - z0 + 1, c);
            if (p.size() <= 2) return;
            int axis = (maxx - minx) > (maxz - minz) ? 0 : 2;
            std::sort(p.begin(), p.end(), [axis](const Point3& a, const Point3& b) { return a[axis] < b[axis]; });
            std::vector<Point3> left(p.begin(), p.begin() + p.size() / 2), right(p.begin() + p.size() / 2, p.end());
            split(left, depth + 1);
            split(right, depth + 1);
        };
        split(pts, 0);
        for (auto& q : pts) cv.fill_circle_aa(20 + q.x * 460, 20 + q.z * 460, 3.5, Color(1, 1, 1));
        save_image("images/ch23_bvh_boxes.png", img);
    }
    return 0;
}
```

It renders a small test image of N random spheres with a plain list and with a BVH, for N = 10, 100,
1000, 3000, and prints the times. It then draws a 2D **top-down picture of the BVH splits** for 64 random points,
using the same median-split idea with nested rectangles.

```bat
run ch23_bvh
```

### What you should see (console)

Numbers depend on your computer, but the shape looks like this:

```
 spheres      list (s)       BVH (s)   speedup
      10          0.05          0.06      0.9x
     100          0.30          0.10      3.0x
    1000          2.90          0.15     19.3x
    3000          8.70          0.17     51.2x
```

With 10 spheres the BVH is no faster (it has overhead). With thousands, it's dramatically faster, and
the BVH time barely grows as N increases. That's the logarithmic magic.

### What you should see (images)

![Many spheres](../images/ch23_many_spheres.png)

> **Image description:** A small render from above of 3000 colorful matte spheres (every hue of the
> rainbow) scattered in the air above a grey ground, like confetti frozen in mid-air. It's noisy because
> it uses only 4 samples per pixel (it's a speed test).

![BVH boxes](../images/ch23_bvh_boxes.png)

> **Image description:** A dark navy square with 64 white dots. Around them, colored rectangles are
> nested inside each other: one big red rectangle around everything, split into two orange-yellow
> rectangles, each split into two green ones, then cyan, then blue and purple ones around pairs of
> dots. Each level splits the longer side of its parent. You can see how the tree divides space.

---

## Try it yourself

1. Add a counter (an `std::atomic<long long>`) that counts sphere intersection tests. Compare list vs BVH.
2. Try splitting on a **random** axis instead of the longest one. How much slower is it?
3. Implement leaf nodes holding up to 4 objects instead of 2. Faster or slower?
4. **Challenge:** implement the SAH split. Test it on the terrain in chapter 25 (20,000 triangles).

## Common problems

| Symptom | Cause |
|---------|-------|
| Objects disappear with the BVH | Wrong bounding box for an object (too small), e.g. a moving sphere's box covering only one position |
| Flat objects (quads) disappear | Zero-thickness box: pad it |
| BVH slower than the list | Scene is tiny, or a debug build |
| Crash building the BVH | Empty list: check before building |

---

## Summary

* AABBs + the slab test give a very fast "might this ray hit anything in here?" check.
* A BVH nests boxes; traversal skips entire branches the ray misses.
* Cost per ray drops from O(N) to about O(log N): thousands of times faster for big scenes.

Next: [Chapter 24 — Textures →](24-textures.md)
