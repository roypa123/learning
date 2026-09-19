# Chapter 15 — Surface normals and many objects

[← Hitting a sphere](14-hitting-a-sphere.md) · [Contents](README.md) · [Next: Random numbers & anti-aliasing →](16-random-and-antialiasing.md)

> 📖 **Line by line:** [hittable explained line by line](line-by-line/hittable.md) · [sphere explained line by line](line-by-line/sphere.md) · [ch15_normals explained line by line](line-by-line/ch15_normals.md)

---

## Goal

* Compute the **surface normal** at a hit point, and visualize it as color.
* Collect everything we know about a hit in a `HitRecord`.
* Decide whether a ray hit the **front** or the **back** of a surface.
* Create the `Hittable` interface and a `HittableList` that finds the **closest** hit among many
  objects.
* Put a sphere on the ground.

After this chapter the ray tracer has the same structure as a production renderer: objects behind an
interface, and a world that answers *"what does this ray hit first?"*.

---

## 1. Surface normals

A **normal** is a unit vector perpendicular to a surface at a point: "the direction the surface faces".
For a sphere it's easy. The normal points straight out from the center:

```
normal = (P − C) / r          (dividing by the radius makes it unit length)

          ↖  ↑  ↗
        ←   ( C )   →        every normal points away from the center
          ↙  ↓  ↘
```

### 1.1 Seeing normals

Normals have components between −1 and 1. Map them to colors with `0.5 · (n + 1)`:

| normal | meaning | color |
|--------|---------|-------|
| (1, 0, 0) | facing right | (1, 0.5, 0.5) pinkish red |
| (0, 1, 0) | facing up | (0.5, 1, 0.5) light green |
| (0, 0, 1) | facing the camera | (0.5, 0.5, 1) light blue-violet |

This **normal visualization** is one of the most useful debugging images in graphics.

---

## 2. The HitRecord

When a ray hits something, the renderer will want to know a lot about that spot:

```cpp
struct HitRecord {
    Point3 p;                          // the hit point
    Vec3 normal;                       // unit normal, always facing AGAINST the ray
    const Material* mat = nullptr;     // what the surface is made of (chapter 17)
    double t = 0.0;                    // ray parameter at the hit
    double u = 0.0, v = 0.0;           // texture coordinates (chapter 24)
    bool front_face = true;            // did we hit the outside of the surface?
    void set_face_normal(const Ray& r, const Vec3& outward_normal);
};
```

### 2.1 Front face or back face?

A ray can hit a surface from outside (like the camera looking at a ball) or from inside (a ray
travelling *inside* a glass ball, chapter 19). We need to know which, and we want the stored normal to
always point **against** the ray, towards the side the ray came from. That makes shading math simpler.

```
   OUTSIDE hit (front face)                  INSIDE hit (back face)

        ray ──▶ ●                              ● ──▶ ray          (ray travels inside, towards the wall)
       ◀── normal   (outward normal            ── outward normal ─▶  points the same way as the ray,
                     already faces the ray)    ◀── stored normal      so we flip it)
```

The rule is simple with a dot product:

```cpp
void set_face_normal(const Ray& r, const Vec3& outward_normal) {
    front_face = dot(r.direction(), outward_normal) < 0.0;   // pointing opposite ways = hitting the front
    normal = front_face ? outward_normal : -outward_normal;
}
```

* If the ray and the outward normal point in **opposite** directions (dot < 0), the ray comes from
  outside: **front face**, keep the normal.
* Otherwise the ray is inside going out: **back face**, flip the normal so it points back towards the
  ray.

---

## 3. The Hittable interface

Following chapter 2's plan, everything a ray can hit implements:

```cpp
class Hittable {
public:
    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;
    virtual AABB bounding_box() const = 0;   // chapter 23
    ...
};
```

`hit` answers: *"Does ray r hit you at some t inside the interval ray_t? If so, fill in rec and return
true."*

### 3.1 Why an interval?

`Interval ray_t` (from `ray.h`) is a range `[min, max]` of acceptable `t` values. It solves several
problems at once:

* `min > 0` ignores hits **behind** the ray origin (chapter 14's bug), and `min` slightly above 0
  prevents "shadow acne" (chapter 17).
* `max` lets us **ignore hits farther than one we already found**. That's the key to handling many
  objects.

The sphere tries the nearer root first. If it's outside the interval, it tries the farther root:

```cpp
double root = (h - sqrtd) / a;
if (!ray_t.surrounds(root)) {
    root = (h + sqrtd) / a;
    if (!ray_t.surrounds(root)) return false;
}
```

That fixes the "camera inside a sphere" problem from chapter 14.

---

## 4. Many objects: `HittableList`

A list of objects is also a `Hittable`. Its `hit` function asks each object in turn, but each time it
**shrinks the allowed range** to the closest hit so far:

```cpp
bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    HitRecord temp;
    bool hit_anything = false;
    double closest = ray_t.max;
    for (const auto& object : objects) {
        if (object->hit(r, Interval(ray_t.min, closest), temp)) {
            hit_anything = true;
            closest = temp.t;       // only accept closer hits from now on
            rec = temp;
        }
    }
    return hit_anything;
}
```

```
ray ──▶──────●──────────●──────────●───────▶
             │ sphere A  │ sphere B  │ ground
            t=2         t=5         t=9

check A: hit at 2  -> closest = 2
check B: hit at 5? not in [0.001, 2] -> ignored
check ground: hit at 9? not in range -> ignored
result: A, t = 2
```

Objects are stored as `std::shared_ptr<Hittable>`, so the list can hold spheres, quads, triangles and
even other lists, all mixed together.

---

## 5. The library files

### `hittable.h`

**File: `include/pixel/hittable.h`**

```cpp
// pixel/hittable.h
// ------------------------------------------------------------
// "Hittable" = anything a ray can hit (sphere, quad, triangle,
// a whole list of objects, a BVH tree...).
// Explained in docs/15-normals-and-lists.md
// ------------------------------------------------------------
#pragma once
#include <memory>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "aabb.h"
#include "random.h"

namespace pixel {

class Material;   // defined in material.h

// Everything we want to know about the place where a ray hit something.
struct HitRecord {
    Point3 p;                          // the hit point
    Vec3 normal;                       // unit normal, always facing AGAINST the ray
    const Material* mat = nullptr;     // what the surface is made of
    double t = 0.0;                    // ray parameter: p = origin + t*direction
    double u = 0.0, v = 0.0;           // texture coordinates
    bool front_face = true;            // did we hit the outside of the surface?

    // outward_normal must be unit length.
    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Hittable {
public:
    virtual ~Hittable() = default;

    // Does the ray hit this object with t inside ray_t? If yes, fill rec.
    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

    virtual AABB bounding_box() const = 0;

    // --- Used for light sampling (docs/31-light-sampling.md) ---
    // Probability density of choosing 'direction' from 'origin' towards this object.
    virtual double pdf_value(const Point3& /*origin*/, const Vec3& /*direction*/) const { return 0.0; }
    // A random direction from 'origin' towards this object.
    virtual Vec3 random(const Point3& /*origin*/) const { return Vec3(1, 0, 0); }
};

// A list of hittables that behaves like one hittable.
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;

    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }

    void clear() { objects.clear(); bbox = AABB(); }

    void add(std::shared_ptr<Hittable> object) {
        bbox = objects.empty() ? object->bounding_box() : AABB(bbox, object->bounding_box());
        objects.push_back(object);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord temp;
        bool hit_anything = false;
        double closest = ray_t.max;
        for (const auto& object : objects) {
            if (object->hit(r, Interval(ray_t.min, closest), temp)) {
                hit_anything = true;
                closest = temp.t;       // only accept closer hits from now on
                rec = temp;
            }
        }
        return hit_anything;
    }

    AABB bounding_box() const override { return bbox; }

    // Light sampling over a list: pick one object at random.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        if (objects.empty()) return 0.0;
        double weight = 1.0 / objects.size();
        double sum = 0.0;
        for (const auto& object : objects) sum += weight * object->pdf_value(origin, direction);
        return sum;
    }

    Vec3 random(const Point3& origin) const override {
        if (objects.empty()) return Vec3(1, 0, 0);
        int i = (int)(random_double() * objects.size());
        if (i >= (int)objects.size()) i = (int)objects.size() - 1;
        return objects[i]->random(origin);
    }

private:
    AABB bbox;
};

} // namespace pixel
```

(`pdf_value` and `random` are for light sampling in chapter 31. Ignore them for now.)

### `sphere.h`

**File: `include/pixel/sphere.h`**

```cpp
// pixel/sphere.h
// ------------------------------------------------------------
// The sphere: the "hello world" of ray tracing.
// Supports motion (for motion blur) and light sampling.
// Explained in docs/14-hitting-a-sphere.md, docs/22-motion-blur.md,
//              docs/31-light-sampling.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "ray.h"
#include "hittable.h"
#include "pdf.h"

namespace pixel {

class Sphere : public Hittable {
public:
    // A sphere that stays still.
    Sphere(const Point3& center, double radius, std::shared_ptr<Material> mat)
        : center(center, Vec3(0, 0, 0)), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        bbox = AABB(center - rvec, center + rvec);
    }

    // A sphere that moves from center1 (time 0) to center2 (time 1).
    Sphere(const Point3& center1, const Point3& center2, double radius, std::shared_ptr<Material> mat)
        : center(center1, center2 - center1), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        AABB box1(center.at(0) - rvec, center.at(0) + rvec);
        AABB box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = AABB(box1, box2);
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Point3 current_center = center.at(r.time());
        Vec3 oc = current_center - r.origin();
        double a = r.direction().length_squared();
        double h = dot(r.direction(), oc);
        double c = oc.length_squared() - radius * radius;

        double discriminant = h * h - a * c;
        if (discriminant < 0) return false;          // the ray misses
        double sqrtd = std::sqrt(discriminant);

        // Find the nearest root inside the acceptable range.
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat.get();
        return true;
    }

    AABB bounding_box() const override { return bbox; }

    // p: a point on the unit sphere centered at the origin.
    // u: angle around the Y axis from X=-1, in [0,1]
    // v: angle from Y=-1 to Y=+1, in [0,1]
    static void get_sphere_uv(const Point3& p, double& u, double& v) {
        double theta = std::acos(clampd(-p.y, -1.0, 1.0));
        double phi = std::atan2(-p.z, p.x) + pi;
        u = phi / (2 * pi);
        v = theta / pi;
    }

    // ----- light sampling: we only sample the cone of directions that hit us.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double dist_squared = (center.at(0) - origin).length_squared();
        if (dist_squared <= radius * radius) return 0.0;
        double cos_theta_max = std::sqrt(1.0 - radius * radius / dist_squared);
        double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);
        return 1.0 / solid_angle;
    }

    Vec3 random(const Point3& origin) const override {
        Vec3 direction = center.at(0) - origin;
        double distance_squared = direction.length_squared();
        if (distance_squared <= radius * radius) return random_unit_vector();
        ONB uvw(direction);
        return uvw.transform(random_to_sphere(radius, distance_squared));
    }

private:
    Ray center;     // center.at(time) = position at that time
    double radius;
    std::shared_ptr<Material> mat;
    AABB bbox;

    static Vec3 random_to_sphere(double radius, double distance_squared) {
        double r1 = random_double();
        double r2 = random_double();
        double z = 1 + r2 * (std::sqrt(1 - radius * radius / distance_squared) - 1);
        double phi = 2 * pi * r1;
        double x = std::cos(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        double y = std::sin(phi) * std::sqrt(std::fmax(0.0, 1 - z * z));
        return Vec3(x, y, z);
    }
};

} // namespace pixel
```

The sphere's center is stored as a `Ray` so it can **move** over time (motion blur, chapter 22). For a
still sphere the "direction" is zero, so `center.at(time)` is always the same point. `get_sphere_uv` is
for textures (chapter 24), and `pdf_value`/`random` are for light sampling (chapter 31).

---

## 6. The program

**File: `chapters/ch15_normals.cpp`**

```cpp
// ch15_normals.cpp
// ------------------------------------------------------------
// Chapter 15: Surface normals and many objects.
// Now we use the library classes Sphere, HittableList, HitRecord.
// Each hit point is colored by its normal (x,y,z -> r,g,b).
//   images/ch15_normals.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) {
        // Normal components are in [-1, 1]; map them to [0, 1] colors.
        return 0.5 * (rec.normal + Color(1, 1, 1));
    }
    Vec3 d = unit_vector(r.direction());
    double a = 0.5 * (d.y + 1.0);
    return (1.0 - a) * Color(1.0, 1.0, 1.0) + a * Color(0.5, 0.7, 1.0);
}

int main() {
    // The world: a small sphere, and a huge sphere acting as the ground.
    // (No materials yet - we pass nullptr.)
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, nullptr));
    world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, nullptr));

    const int W = 400, H = 225;
    const double vw = 2.0 * W / H;
    const Point3 eye(0, 0, 0);
    const Vec3 du(vw / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(vw / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);

    Image img(W, H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            img.at(i, j) = ray_color(r, world);
        }
    SaveOptions raw;
    raw.srgb = false;
    save_image("images/ch15_normals.png", img, raw);
    return 0;
}
```

The **ground** is a trick used by many ray tracing tutorials: a *huge* sphere (radius 100) whose top
touches y = −0.5, right under the small sphere. From our small viewpoint its surface looks flat.

We pass `nullptr` as the material because we don't have materials yet. Our `ray_color` only looks at the
normal.

```bat
run ch15_normals
```

---

## 7. What you should see

![Normals](../images/ch15_normals.png)

> **Image description:** The blue-white sky, with a sphere in the center colored like a smooth pastel
> rainbow: light green at the top (normals pointing up), pinkish-red on the right, teal-cyan on the
> left, purple at the bottom, and lavender-blue in the middle (normals facing the camera). Below it, filling the bottom
> half, the "ground" (the giant sphere) is a flat-looking light green that shades slightly, since its normals point
> almost straight up. The small sphere correctly hides the ground behind it: the closest hit wins.

---

## Try it yourself

1. Add a third sphere to the list at `(1, 0, -1.5)`, radius 0.3. Check it hides correctly behind and in
   front of the others.
2. Color by `front_face`: green for front, red for back. Then put the camera inside a big sphere.
   Everything should turn red.
3. Replace the normal coloring with fake lighting (chapter 12): `max(0, dot(rec.normal, light_dir))`.
   Notice there are **no shadows**: the ground is lit even under the sphere. That's because we never ask
   whether the light is blocked. Real shadows appear by themselves in chapter 17.
4. Make the ground sphere radius 1000 and center y = −1000.5. Does it look flatter?

## Common problems

| Symptom | Cause |
|---------|-------|
| Colors on the sphere look "wrong-way round" | Normal not flipped/unflipped consistently, or not unit length |
| Far object drawn over near one | List doesn't shrink `closest` after each hit |
| Weird dark spots on the ground | Using `t_min = 0` together with lighting (see chapter 17) |

---

## Summary

* Sphere normal: `(P − C) / r`. Visualize normals with `0.5 · (n + 1)`.
* `HitRecord` stores the point, normal, t, material, uv and front/back.
* `set_face_normal`: the stored normal always faces the incoming ray.
* `Hittable` is the interface; `HittableList` returns the **closest** hit by shrinking the interval.

Next: [Chapter 16 — Random numbers and anti-aliasing →](16-random-and-antialiasing.md)
