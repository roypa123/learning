# Chapter 27 — Instances: moving and rotating things

[← Lights & the Cornell box](26-lights-cornell-box.md) · [Contents](README.md) · [Next: Volumes →](28-volumes.md)

> 📖 **Line by line:** [instance explained line by line](line-by-line/instance.md) · [ch27_instances explained line by line](line-by-line/ch27_instances.md)

---

## Goal

* Move (**translate**) and **rotate** objects without changing their geometry.
* Understand the key trick: **transform the ray instead of the object**.
* Reuse one object many times (**instancing**): a forest of 60 trees from one tree model.
* Render the standard Cornell box with its two rotated boxes.

---

## 1. The trick: move the ray, not the object

Say we have a box built at the origin and we want it 3 units to the right. We could rebuild the box
with new corner points. But for a mesh with a million triangles, or when the same object appears
100 times, that's wasteful. Instead:

> Moving the object **right** by 3 is the same as moving the **ray left** by 3 and testing the
> original object.

```
world space:                         object space:
        ray ──▶     ▢ (moved box)          ray' ──▶  ▢ (box at origin)
                    x = 3                           x = 0
   ray' = ray shifted by −3; if ray' hits the original box at t, the real ray hits the moved box at the same t
```

After the hit, we transform the hit **point** (and **normal**) back into world space.

### 1.1 Translate

```cpp
bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    Ray offset_r(r.origin() - offset, r.direction(), r.time());   // move the ray backwards
    if (!object->hit(offset_r, ray_t, rec)) return false;
    rec.p += offset;                                              // move the hit point forwards
    return true;
}
```

The direction doesn't change, so `t` is the same in both spaces, and normals don't change either.

### 1.2 Rotate around the Y axis

Rotating a point by angle θ around the y axis:

```
x' =  cos θ · x + sin θ · z
y' =  y
z' = −sin θ · x + cos θ · z
```

(Picture looking down from above: a standard 2D rotation in the x-z plane.)

To test a rotated object: rotate the **ray** (origin and direction) by **−θ** into object space, test,
then rotate the hit point and normal by **+θ** back into world space:

```cpp
Ray rotated_r(to_object(r.origin()), to_object(r.direction()), r.time());
if (!object->hit(rotated_r, ray_t, rec)) return false;
rec.p = to_world(rec.p);
rec.normal = to_world(rec.normal);
```

A rotation doesn't change lengths, so `t` is still the same.

### 1.3 The bounding box of a rotated object

The rotated object needs its own bounding box (for the BVH). Rotate all **8 corners** of the original
box and take the box around them:

```
   original box       rotated corners        new AABB (bigger)
   ┌──────┐              ◇                  ┌────────┐
   │      │     ->     ◇   ◇        ->      │   ◇    │
   └──────┘              ◇                  │ ◇   ◇  │
                                            │   ◇    │
                                            └────────┘
```

### 1.4 Order matters

We usually **rotate first, then translate**:

```cpp
std::shared_ptr<Hittable> box = make_box(Point3(0,0,0), Point3(165,330,165), white);
box = std::make_shared<RotateY>(box, 15);            // spin around the origin
box = std::make_shared<Translate>(box, Vec3(265, 0, 295));   // then move into place
```

Translating first and then rotating would swing the box around the world origin, like a ball on a
string.

### 1.5 General transforms

Real renderers use a 4×4 **matrix** for any combination of translation, rotation (around any axis) and
scaling, and its **inverse** to bring rays into object space. Normals are transformed with the
*inverse transpose* of the matrix. Our two classes are the same idea specialized for simplicity.
Adding a general `Transform` class is a great exercise (chapter 40).

### `instance.h`

**File: `include/pixel/instance.h`**

```cpp
// pixel/instance.h
// ------------------------------------------------------------
// Instances: move or rotate an object WITHOUT changing the object.
// Trick: instead of moving the object, move the RAY the opposite way.
// Explained in docs/27-instances.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "hittable.h"

namespace pixel {

class Translate : public Hittable {
public:
    Translate(std::shared_ptr<Hittable> object, const Vec3& offset) : object(object), offset(offset) {
        bbox = object->bounding_box() + offset;
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // Move the ray backwards by the offset.
        Ray offset_r(r.origin() - offset, r.direction(), r.time());
        if (!object->hit(offset_r, ray_t, rec)) return false;
        // Move the hit point forwards by the offset.
        rec.p += offset;
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    Vec3 offset;
    AABB bbox;
};

class RotateY : public Hittable {
public:
    RotateY(std::shared_ptr<Hittable> object, double angle_degrees) : object(object) {
        double radians = degrees_to_radians(angle_degrees);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        AABB b = object->bounding_box();

        // Rotate all 8 corners of the box and take the box around them.
        Point3 min( infinity,  infinity,  infinity);
        Point3 max(-infinity, -infinity, -infinity);
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    double x = i ? b.x.max : b.x.min;
                    double y = j ? b.y.max : b.y.min;
                    double z = k ? b.z.max : b.z.min;
                    Vec3 tester = to_world(Vec3(x, y, z));
                    min = vmin(min, tester);
                    max = vmax(max, tester);
                }
        bbox = AABB(min, max);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        // World space -> object space.
        Ray rotated_r(to_object(r.origin()), to_object(r.direction()), r.time());
        if (!object->hit(rotated_r, ray_t, rec)) return false;
        // Object space -> world space.
        rec.p = to_world(rec.p);
        rec.normal = to_world(rec.normal);
        return true;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> object;
    double sin_theta, cos_theta;
    AABB bbox;

    Vec3 to_world(const Vec3& p) const {
        return Vec3(cos_theta * p.x + sin_theta * p.z, p.y, -sin_theta * p.x + cos_theta * p.z);
    }
    Vec3 to_object(const Vec3& p) const {
        return Vec3(cos_theta * p.x - sin_theta * p.z, p.y, sin_theta * p.x + cos_theta * p.z);
    }
};

} // namespace pixel
```

---

## 2. Instancing: one object, many copies

Because `Translate` and `RotateY` only hold a **pointer** to the object, we can wrap the **same**
object many times:

```cpp
std::shared_ptr<Hittable> tree_bvh = std::make_shared<BVHNode>(*tree);   // built once
for (...) {
    auto t = std::make_shared<RotateY>(tree_bvh, random_angle);           // shares tree_bvh
    world.add(std::make_shared<Translate>(t, position));
}
```

60 trees cost the memory of one tree plus 60 tiny wrappers. Film studios use this for forests,
crowds, city buildings and grass, with millions of instances of a few hundred models.

---

## 3. The program

**File: `chapters/ch27_instances.cpp`**

```cpp
// ch27_instances.cpp
// ------------------------------------------------------------
// Chapter 27: Instances - moving and rotating objects.
// The classic Cornell box with two rotated boxes, plus a "forest"
// of 1 mesh used 60 times.
//   images/ch27_cornell_boxes.png
//   images/ch27_instanced_forest.png
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

// Build the five walls + light of the Cornell box. Reused in later chapters.
static void add_cornell_room(HittableList& world, std::shared_ptr<Material> light) {
    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    world.add(std::make_shared<Quad>(Point3(555, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), green));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(0, 555, 0), Vec3(0, 0, 555), red));
    world.add(std::make_shared<Quad>(Point3(343, 554, 332), Vec3(-130, 0, 0), Vec3(0, 0, -105), light));
    world.add(std::make_shared<Quad>(Point3(0, 0, 0), Vec3(555, 0, 0), Vec3(0, 0, 555), white));
    world.add(std::make_shared<Quad>(Point3(555, 555, 555), Vec3(-555, 0, 0), Vec3(0, 0, -555), white));
    world.add(std::make_shared<Quad>(Point3(0, 0, 555), Vec3(555, 0, 0), Vec3(0, 555, 0), white));
}

int main() {
    // ---------- 1. Cornell box with rotated boxes --------------------------
    {
        HittableList world;
        add_cornell_room(world, std::make_shared<DiffuseLight>(Color(15, 15, 15)));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));

        // Build each box at the origin, rotate it, THEN move it into place.
        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        world.add(box1);

        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 200;
        cam.max_depth = 50;
        cam.background = solid_background(Color(0, 0, 0));
        cam.vfov = 40;
        cam.lookfrom = Point3(278, 278, -800);
        cam.lookat = Point3(278, 278, 0);
        save_image("images/ch27_cornell_boxes.png", cam.render(world));
    }

    // ---------- 2. One tree, many instances --------------------------------
    {
        // A "tree": a cone of triangles (leaves) on a box trunk.
        Mesh cone = make_parametric_mesh(16, 1, [](double u, double v) {
            double a = u * 2 * pi;
            double r = 0.6 * (1 - v);
            return Point3(r * std::cos(a), 0.4 + 1.4 * v, r * std::sin(a));
        });
        auto leaves = std::make_shared<Lambertian>(hex_color(0x2D6A4F));
        auto bark = std::make_shared<Lambertian>(hex_color(0x6B4226));
        auto tree = std::make_shared<HittableList>();
        tree->add(cone.build(leaves));
        tree->add(make_box(Point3(-0.1, 0, -0.1), Point3(0.1, 0.45, 0.1), bark));
        std::shared_ptr<Hittable> tree_bvh = std::make_shared<BVHNode>(*tree);

        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(hex_color(0x95D5B2))));
        Pcg32 rng(8);
        for (int i = 0; i < 60; i++) {
            double x = rng.next_double() * 16 - 8, z = rng.next_double() * 16 - 8;
            if (x * x + z * z < 2) continue;   // leave a clearing
            std::shared_ptr<Hittable> t = std::make_shared<RotateY>(tree_bvh, rng.next_double() * 360);
            world.add(std::make_shared<Translate>(t, Vec3(x, 0, z)));   // the SAME tree, placed again
        }
        world.add(std::make_shared<Sphere>(Point3(0, 0.5, 0), 0.5, std::make_shared<Metal>(Color(0.9, 0.9, 0.9), 0.0)));
        BVHNode bvh(world);

        Camera cam;
        cam.image_width = 600;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 35;
        cam.lookfrom = Point3(10, 6, 10);
        cam.lookat = Point3(0, 0.5, 0);
        save_image("images/ch27_instanced_forest.png", cam.render(bvh));
    }
    return 0;
}
```

```bat
run ch27_instances
```

---

## 4. What you should see

![Cornell boxes](../images/ch27_cornell_boxes.png)

> **Image description:** The classic Cornell box: green left wall, red right wall, white floor,
> ceiling and back wall, and a ceiling light. Inside, a **tall white box** stands at the back left,
> turned slightly, and a **short white box** at the front right, turned the other way. The boxes cast
> soft shadows on the floor. The sides of the boxes facing the walls are tinted green (left) and red
> (right) by bounced light. The image is noisy (no light sampling yet).

![Instanced forest](../images/ch27_instanced_forest.png)

> **Image description:** A light-green meadow seen from above at an angle, with about 55 small
> dark-green cone-shaped trees with brown trunks scattered around, all identical but facing different
> directions. In the center there's a clearing with a small shiny mirror ball reflecting the trees and
> the sky.

---

## Try it yourself

1. Add a uniform `Scale` instance: divide the ray's origin **and** direction by s, test the object,
   then multiply the hit point by s. Does `t` change? (No: work out why on paper. What about the
   normal, and what would change with a *non-uniform* scale?)
2. Make `RotateX` and `RotateZ`. Stand a box on its corner.
3. Replace the cone trees with loaded OBJ models.
4. Give each tree a random size, which needs your `Scale` from exercise 1.

## Common problems

| Symptom | Cause |
|---------|-------|
| Object in the wrong place | Translate and rotate applied in the wrong order |
| Rotated object is cut off | Bounding box not recomputed after rotation |
| Lighting on rotated object is wrong | Normal not rotated back into world space |
| Rotation goes the wrong way | Sign of sin θ swapped between `to_world` and `to_object` |

---

## Summary

* Transform the **ray** into object space, test the original object, transform the hit back.
* Translation shifts the origin; rotation turns origin and direction (and later the normal).
* Instances share geometry: huge scenes for little memory.

Next: [Chapter 28 — Volumes: smoke, fog and clouds →](28-volumes.md)
