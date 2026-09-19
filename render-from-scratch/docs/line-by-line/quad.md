# Line by line: `include/pixel/quad.h`

[← Line-by-line index](README.md) · [Chapter 25 (the theory)](../25-quads-triangles-meshes.md)

**What this file does, in one sentence:** it defines flat shapes: the **Quad** (a parallelogram, e.g. a wall, floor or
lamp panel), the **Disk**, and `make_box` (a box built from six quads).

A quad is given by one corner **Q** and two edge vectors **u** and **v**:

```
   Q+v ────────── Q+u+v
    │               │
    Q  ──────────  Q+u
```

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–17 | |
| B. Constructor | 19–31 | precompute the plane, helper vector, area, box |
| C. `hit` | 33–54 | hit the plane, then check if inside the parallelogram |
| D. Light sampling | 56–69 | for quad-shaped lights |
| E. `is_interior` and data | 71–89 | the inside test |
| F. `Disk` | 91–112 | a round version |
| G. `make_box` | 114–129 | six quads |
| H. End | 131 | |

---

## Block A — Comments, includes (lines 1–17)

Comments with the diagram; `#pragma once`; `<cmath>`, `<memory>`; our `vec3.h`, `hittable.h`, `random.h`;
`namespace pixel`.

---

## Block B — Constructor (lines 19–31)

```cpp
class Quad : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, std::shared_ptr<Material> mat)
        : Q(Q), u(u), v(v), mat(mat) {
```

Lines 21–22: store the corner, the two edges and the material.

```cpp
        Vec3 n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);                 // plane equation: dot(normal, P) = D
        w = n / dot(n, n);                  // helper for computing alpha/beta
        area = n.length();
```

* Line 23: `n` = u × v: perpendicular to the quad. Its direction decides the quad's **front** side (so the order of u
  and v matters, for example for one-sided lights).
* Line 24: the unit normal.
* Line 25: every point P on the quad's plane satisfies `dot(normal, P) = D`. We compute D from the corner Q.
* Line 26: `w` = a helper vector used in `hit` to find where on the quad a point is.
* Line 27: the area of a parallelogram = the length of u × v.

```cpp
        AABB diag1(Q, Q + u + v);
        AABB diag2(Q + u, Q + v);
        bbox = AABB(diag1, diag2);
    }
```

Lines 28–30: the bounding box: a box around both diagonals covers all four corners.

---

## Block C — `hit` (lines 33–54)

```cpp
        double denom = dot(normal, r.direction());
        if (std::fabs(denom) < 1e-8) return false;         // ray parallel to the plane
```

* Line 36: how much the ray moves toward or away from the plane per unit of t.
* Line 37: almost 0 → the ray runs parallel to the plane and never hits it.

```cpp
        double t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;
```

* Line 39: the distance at which the ray reaches the plane (from `dot(normal, origin + t × dir) = D`, solved for t).
* Line 40: outside the allowed range → no hit.

```cpp
        Point3 intersection = r.at(t);
        Vec3 planar_hitpt = intersection - Q;
        double alpha = dot(w, cross(planar_hitpt, v));
        double beta  = dot(w, cross(u, planar_hitpt));
        if (!is_interior(alpha, beta, rec)) return false;
```

* Line 43: the point where the ray meets the plane.
* Line 44: that point relative to the corner Q.
* Lines 45–46: write it as `alpha × u + beta × v`. `alpha` = how far along u (0 at Q, 1 at the far edge), `beta` = how
  far along v. These two formulas compute exactly that, using the helper `w`.
* Line 47: inside the shape? (block E). If not, no hit.

```cpp
        rec.t = t;
        rec.p = intersection;
        rec.mat = mat.get();
        rec.set_face_normal(r, normal);
        return true;
    }
```

Lines 49–53: fill in the hit record and report a hit.

---

## Block D — Light sampling (lines 56–69), chapter 31

```cpp
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double distance_squared = rec.t * rec.t * direction.length_squared();
        double cosine = std::fabs(dot(direction, rec.normal) / direction.length());
        if (cosine < 1e-8) return 0.0;
        return distance_squared / (cosine * area);
    }
```

How likely is a direction, if we pick random **points on the quad** and aim at them?

* Line 59: the direction must hit the quad.
* Line 60: the squared distance to the hit point.
* Line 61: the cosine of the angle between the direction and the quad's normal (a quad seen from the side looks
  smaller).
* Line 62: seen exactly edge-on: 0.
* Line 63: the formula: `distance² ÷ (cosine × area)`. Far away or tilted → each direction is more likely (the quad
  looks smaller, so the same points fit into fewer directions).

```cpp
    Vec3 random(const Point3& origin) const override {
        Point3 p = Q + (random_double() * u) + (random_double() * v);
        return p - origin;
    }
```

Lines 66–69: a random point on the quad (random amounts of u and v), and the direction to it.

---

## Block E — `is_interior` and data (lines 71–89)

```cpp
protected:
    virtual bool is_interior(double a, double b, HitRecord& rec) const {
        Interval unit_interval(0, 1);
        if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
```

* Line 71: `protected:` = usable by this class and classes built on it (like Disk), but not from outside.
* Line 73: `virtual`, so Disk can replace it with a round test.
* Line 75: inside the parallelogram if both alpha and beta are between 0 and 1.
* Lines 76–77: they are also perfect texture coordinates.

Lines 81–88: the stored data: corner, edges, helper, material, box, normal, plane constant, area.

---

## Block F — `Disk` (lines 91–112)

```cpp
class Disk : public Quad {
public:
    Disk(const Point3& center, const Vec3& half_u, const Vec3& half_v, std::shared_ptr<Material> mat)
        : Quad(center - half_u - half_v, 2.0 * half_u, 2.0 * half_v, mat) {
        area = area * pi / 4.0;
    }
```

* Line 93: a Disk **is a** Quad with a different inside test.
* Lines 95–96: given a center and two half-edges, build the square around the disk (corner = center − half_u − half_v).
* Line 97: a circle inside a square covers π/4 of its area.

```cpp
    Vec3 random(const Point3& origin) const override {
        Vec3 d = random_in_unit_disk();
        Point3 p = Q + u * (0.5 + 0.5 * d.x) + v * (0.5 + 0.5 * d.y);
        return p - origin;
    }
```

Lines 99–103: a random point in the disk (a random point in a unit disk, moved into the square's 0–1 coordinates).

```cpp
    bool is_interior(double a, double b, HitRecord& rec) const override {
        double da = a - 0.5, db = b - 0.5;
        if (da * da + db * db > 0.25) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
```

Lines 105–111: inside if the distance from the square's center (0.5, 0.5) is at most 0.5 (0.25 = 0.5², comparing
squares).

---

## Block G — `make_box` (lines 114–129)

```cpp
inline std::shared_ptr<HittableList> make_box(const Point3& a, const Point3& b, std::shared_ptr<Material> mat) {
    auto sides = std::make_shared<HittableList>();
    Point3 min = vmin(a, b);
    Point3 max = vmax(a, b);
    Vec3 dx(max.x - min.x, 0, 0);
    Vec3 dy(0, max.y - min.y, 0);
    Vec3 dz(0, 0, max.z - min.z);
```

* Line 115: a box from two opposite corners, returned as a list of six quads.
* Lines 117–118: the lowest and highest corner (so the input order doesn't matter).
* Lines 119–121: the box's edge vectors along x, y, z.

```cpp
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, max.z),  dx,  dy, mat)); // front
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, max.z), -dz,  dy, mat)); // right
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, min.z), -dx,  dy, mat)); // back
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dz,  dy, mat)); // left
    sides->add(std::make_shared<Quad>(Point3(min.x, max.y, max.z),  dx, -dz, mat)); // top
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dx,  dz, mat)); // bottom
    return sides;
}
```

Lines 122–127: the six faces. Each has a corner and two edges, chosen (including the minus signs) so that every face's
normal points **outward**. That matters for lights and fog.

---

## Block H — End (line 131)

`} // namespace pixel`

---

## Check your understanding

1. What are alpha and beta at the corner Q + u? *(alpha = 1, beta = 0.)*
2. Why does the order of u and v matter? *(u × v decides which side is the front.)*
3. How does a Disk differ from a Quad? *(Only in the inside test, the area and the random point.)*
