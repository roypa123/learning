# Line by line: `include/pixel/sphere.h`

[← Line-by-line index](README.md) · Chapters [14](../14-hitting-a-sphere.md), [15](../15-normals-and-lists.md), [22](../22-motion-blur.md), [24](../24-textures.md), [31](../31-light-sampling.md)

**What this file does, in one sentence:** it defines the `Sphere` object: how a ray hits it, its normal, its texture
coordinates, its bounding box, how it can move (motion blur), and how to aim rays at it when it's a light.

Some parts are used only in later chapters; they're marked.

| Block | Lines | Job | Chapter |
|-------|-------|-----|---------|
| A. Comments, includes | 1–16 | | |
| B. Constructors | 18–34 | a still sphere and a moving sphere | 15, 22 |
| C. `hit` | 36–61 | the ray–sphere test, filling the hit record | 14, 15 |
| D. `bounding_box` | 63 | | 23 |
| E. `get_sphere_uv` | 65–73 | where on the sphere, for textures | 24 |
| F. Light sampling | 75–92 | chance of a direction; random direction toward the sphere | 31 |
| G. Private data and helper | 94–109 | | |
| H. End | 111 | | |

---

## Block A — Comments, includes (lines 1–16)

Comments; `#pragma once`; `<cmath>` (sqrt, acos, atan2, cos, sin), `<memory>` (shared_ptr); our `vec3.h`, `ray.h`,
`hittable.h` (the base class) and `pdf.h` (the `ONB` helper, chapter 30). Line 16: `namespace pixel`.

---

## Block B — Constructors (lines 18–34)

```cpp
class Sphere : public Hittable {
public:
```

Line 18: a Sphere **is a** Hittable, so it must provide `hit` and `bounding_box`.

### Lines 21–25: a sphere that stays still

```cpp
    Sphere(const Point3& center, double radius, std::shared_ptr<Material> mat)
        : center(center, Vec3(0, 0, 0)), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        bbox = AABB(center - rvec, center + rvec);
    }
```

* Line 21: inputs: center, radius, material (shared, because several objects can use the same material).
* Line 22, the initializer list:
  * `center(center, Vec3(0, 0, 0))`: the member `center` is stored as a **Ray** (start = the center, direction = zero).
    Why a Ray? So a moving sphere can use the same code: `center.at(time)`. With a zero direction it never moves.
  * `radius(std::fmax(0.0, radius))`: store the radius, never negative.
  * `mat(mat)`: store the material.
* Line 23: a vector (r, r, r).
* Line 24: the bounding box: from center − (r, r, r) to center + (r, r, r), the smallest box around the ball.

### Lines 28–34: a moving sphere

```cpp
    Sphere(const Point3& center1, const Point3& center2, double radius, std::shared_ptr<Material> mat)
        : center(center1, center2 - center1), radius(std::fmax(0.0, radius)), mat(mat) {
        Vec3 rvec(radius, radius, radius);
        AABB box1(center.at(0) - rvec, center.at(0) + rvec);
        AABB box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = AABB(box1, box2);
    }
```

* Line 28: two centers: where it is at time 0 and at time 1.
* Line 29: the center "ray" starts at `center1` with direction `center2 − center1`, so `at(0)` = center1 and
  `at(1)` = center2, and times in between are in between.
* Lines 31–33: a box at the start, a box at the end, and one box around both, so the box covers the whole path.

---

## Block C — `hit` (lines 36–61)

The same quadratic idea as [ch14's hit_sphere](ch14_sphere.md), in a slightly simplified form, plus filling the hit
record.

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Point3 current_center = center.at(r.time());
```

* Line 36: the Hittable question.
* Line 37: where the center is at the ray's time (always the same for a still sphere).

```cpp
        Vec3 oc = current_center - r.origin();
        double a = r.direction().length_squared();
        double h = dot(r.direction(), oc);
        double c = oc.length_squared() - radius * radius;
```

Lines 38–41: the quadratic's numbers, using a shortcut: in ch14 we had `b = −2 × dot(d, oc)`. Here we use `h = dot(d, oc)`
instead (so b = −2h). The formula then becomes simpler: `t = (h ± √(h² − a·c)) / a`. Same result, fewer calculations.
`length_squared()` is the same as a dot product of a vector with itself.

```cpp
        double discriminant = h * h - a * c;
        if (discriminant < 0) return false;          // the ray misses
        double sqrtd = std::sqrt(discriminant);
```

* Line 43: the (simplified) discriminant.
* Line 44: negative → no hit.
* Line 45: its square root, computed once and used twice below.

```cpp
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }
```

* Line 48: first try the **nearer** hit (where the ray enters).
* Line 49: is it inside the allowed range? (`surrounds` = strictly between min and max.) If not...
* Line 50: ...try the **farther** hit (where the ray leaves). This handles rays that start **inside** the sphere.
* Line 51: if that's not allowed either, no hit.

```cpp
        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat.get();
        return true;
    }
```

* Line 54: the distance.
* Line 55: the hit point.
* Line 56: the outward normal: from the center to the hit point, divided by the radius (so length 1).
* Line 57: store it facing the ray, and remember front/back (see [hittable.md](hittable.md)).
* Line 58: compute the texture coordinates u, v.
* Line 59: `mat.get()` = the plain pointer inside the shared_ptr.
* Line 60: yes, hit.

---

## Block D — `bounding_box` (line 63)

Returns the box made in the constructor.

---

## Block E — `get_sphere_uv` (lines 65–73)

Where on the sphere is a point, as two numbers 0–1, like longitude and latitude on a globe?

```cpp
    static void get_sphere_uv(const Point3& p, double& u, double& v) {
        double theta = std::acos(clampd(-p.y, -1.0, 1.0));
        double phi = std::atan2(-p.z, p.x) + pi;
        u = phi / (2 * pi);
        v = theta / pi;
    }
```

* Line 68: `p` is a point on a unit sphere (our outward normal). `u` and `v` are **outputs** (passed by reference `&`).
  `static` = doesn't need a particular sphere.
* Line 69: `theta` = the angle from the **bottom** pole: `acos(−y)` gives 0 at y = −1 and π at y = +1. The clamp
  protects `acos` from tiny rounding errors outside −1..1.
* Line 70: `phi` = the angle **around** the vertical axis, 0 to 2π. `atan2` gives −π..π; adding π shifts it to 0..2π.
* Lines 71–72: divide to get 0–1.

---

## Block F — Light sampling (lines 75–92), chapter 31

When a sphere is a light, the renderer wants to **aim rays at it**. From a point outside, the sphere covers a **cone** of
directions.

```cpp
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
```

* Line 76: "how likely is this direction, if we pick directions evenly inside the cone?"
* Line 78: if the direction misses the sphere, its chance is 0.

```cpp
        double dist_squared = (center.at(0) - origin).length_squared();
        if (dist_squared <= radius * radius) return 0.0;
        double cos_theta_max = std::sqrt(1.0 - radius * radius / dist_squared);
        double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);
        return 1.0 / solid_angle;
    }
```

* Line 79: squared distance to the center.
* Line 80: if the point is inside the sphere, there's no cone (return 0).
* Line 81: the cone's half-angle, as a cosine: `√(1 − r²/d²)` (from the right triangle formed by the center, the point
  and the edge of the sphere).
* Line 82: the cone's size in **solid angle** (the 3D version of an angle): `2π(1 − cos θmax)`.
* Line 83: picking evenly inside it, each direction's chance is 1 ÷ its size.

```cpp
    Vec3 random(const Point3& origin) const override {
        Vec3 direction = center.at(0) - origin;
        double distance_squared = direction.length_squared();
        if (distance_squared <= radius * radius) return random_unit_vector();
        ONB uvw(direction);
        return uvw.transform(random_to_sphere(radius, distance_squared));
    }
```

* Line 87: the direction from the point to the center.
* Line 89: inside the sphere: any direction.
* Line 90: build a small coordinate system (`ONB`, chapter 30) whose "up" axis points at the sphere.
* Line 91: pick a random direction in the cone (around "up"), then turn it into world directions.

---

## Block G — Private data and helper (lines 94–109)

```cpp
private:
    Ray center;     // center.at(time) = position at that time
    double radius;
    std::shared_ptr<Material> mat;
    AABB bbox;
```

Lines 95–98: the stored data.

```cpp
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
```

A random direction inside the cone around +z:

* Line 103: `z` (the cosine of the angle from the axis) is picked evenly between 1 (straight at the center) and
  `cos θmax` (the edge of the cone). Picking the cosine evenly spreads directions evenly over the cone.
* Line 104: a random angle around the axis.
* Lines 105–106: x and y so that the total length is 1 (`x² + y² = 1 − z²`). `fmax(0, ...)` protects against tiny
  negative rounding.

---

## Block H — End (line 111)

`} // namespace pixel`

---

## Check your understanding

1. Why is the center stored as a Ray? *(So moving and still spheres share one `hit` function: `center.at(time)`.)*
2. When is the second root (`h + sqrtd`) used? *(When the first is outside the allowed range, e.g. the ray starts inside.)*
3. What is the normal at the top of a sphere? *((0, 1, 0).)*
4. What are u and v at the north pole? *(v = 1; u can be anything.)*
