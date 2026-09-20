# Line by line: `include/pixel/sdf.h`

[← Line-by-line index](README.md) · [Chapter 37 (the theory)](../37-sdf-raymarching.md)

**What this file does, in one sentence:** it describes shapes by a **distance function** ("how far is this point from my
surface?") and renders them by **sphere tracing**: stepping along the ray by exactly that distance until we touch the
surface.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–18 | two namespaces: `pixel::sdf` |
| B. Primitives | 20–45 | sphere, box, rounded box, torus, plane, capsule |
| C. Combining shapes | 47–56 | union, subtract, intersect, **smooth union** |
| D. Repetition and rotation | 58–67 | infinite copies; turn a point |
| E. Mandelbulb | 69–87 | a 3D fractal |
| F. `SDFObject`: setup | 91–99 | a Hittable from a distance function |
| G. `SDFObject::hit` | 101–125 | sphere tracing |
| H. `normal_at` | 129–137 | the normal from the gradient |
| I. Data and box clipping | 139–160 | |

---

## Block A — Comments, includes (lines 1–18)

Comments explaining the sign convention (positive outside, negative inside, zero on the surface); `#pragma once`;
`<cmath>`, `<functional>`, `<memory>`; our `vec3.h`, `hittable.h`. Lines 17–18 open `pixel` and then `sdf`, so these are
called `sdf::sphere`, `sdf::box`, and so on.

---

## Block B — Primitives (lines 20–45)

```cpp
inline double sphere(const Point3& p, double r) { return p.length() - r; }
```

Line 21: the distance to a sphere at the origin: how far p is from the center, minus the radius. Outside → positive,
inside → negative. **This is the whole shape**: no triangles, no intersection math.

```cpp
inline double box(const Point3& p, const Vec3& half_size) {
    Vec3 q = vabs(p) - half_size;
    return vmax(q, Vec3(0, 0, 0)).length() + std::fmin(q.max_component(), 0.0);
}
```

* Line 24: `vabs(p)` folds the point into the positive corner (a box is symmetric), then subtract the half size. Each
  part of `q` says how far outside the box we are along that axis (negative = inside).
* Line 25: two parts:
  * `vmax(q, 0).length()`: the distance when we're outside (only the positive parts count),
  * `fmin(q.max_component(), 0)`: when we're **inside**, all parts are negative and this gives the distance to the
    nearest face (as a negative number).
  Exactly one of the two is non-zero at a time.

```cpp
inline double round_box(const Point3& p, const Vec3& half_size, double radius) {
    return box(p, half_size - Vec3(radius)) - radius;
}
```

Lines 28–30: a lovely trick: **subtracting a constant from any SDF inflates the shape by that much, with round
corners**. So: shrink the box by `radius`, then inflate it back.

```cpp
inline double torus(const Point3& p, double R, double r) {
    double qx = std::sqrt(p.x * p.x + p.z * p.z) - R;
    return std::sqrt(qx * qx + p.y * p.y) - r;
}
```

* Line 34: the distance from the vertical axis, minus the ring radius: how far we are from the **ring circle** in the
  horizontal plane.
* Line 35: combine that with the height to get the distance to the ring, minus the tube radius.

```cpp
inline double plane_y(const Point3& p, double height) { return p.y - height; }
```

Line 39: an endless floor: the distance is simply the height difference.

```cpp
inline double capsule_y(Point3 p, double h, double r) {
    p.y -= clampd(p.y, 0.0, h);
    return p.length() - r;
}
```

* Line 43: a capsule is "distance to a line segment, minus a radius". Clamping the height to the segment and subtracting
  it moves the point into the segment's frame...
* Line 44: ...so the rest is just the sphere formula. (Note `p` is taken **by value**, so changing it is safe.)

---

## Block C — Combining shapes (lines 47–56)

```cpp
inline double op_union(double a, double b) { return std::fmin(a, b); }
inline double op_subtract(double a, double b) { return std::fmax(a, -b); }   // a minus b
inline double op_intersect(double a, double b) { return std::fmax(a, b); }
```

* Line 48: **union**: the nearest surface of the two → both shapes together.
* Line 49: **subtract**: keep A, but treat the inside of B as outside (flipping B's sign).
* Line 50: **intersect**: only where both are inside.

```cpp
inline double op_smooth_union(double a, double b, double k) {
    double h = clamp01(0.5 + 0.5 * (b - a) / k);
    return lerpd(b, a, h) - k * h * (1.0 - h);
}
```

Lines 53–56: the **smooth union**: instead of a sharp `min`, blend the two distances over a range `k`:

* Line 54: `h` = 0 when b is much smaller, 1 when a is much smaller, and in between near the join.
* Line 55: blend the two distances, then subtract a small bump `k·h(1−h)` (largest in the middle) which **pulls the
  surface outward** right where the shapes meet: they melt together like clay.

---

## Block D — Repetition and rotation (lines 58–67)

```cpp
inline Point3 op_repeat_xz(const Point3& p, double cell) {
    auto rep = [cell](double v) { return v - cell * std::round(v / cell); };
    return Point3(rep(p.x), p.y, rep(p.z));
}
```

* Line 60: `v − cell × round(v / cell)` gives the position **inside the nearest cell**, from −cell/2 to +cell/2.
* Line 61: applied to x and z (not y). Now one shape becomes an endless grid of copies, for free.

```cpp
inline Point3 rotate_y(const Point3& p, double radians) {
    double c = std::cos(radians), s = std::sin(radians);
    return Point3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
```

Lines 64–67: turn a point around the vertical axis (used to rotate a shape: rotate the point the opposite way before
evaluating).

---

## Block E — Mandelbulb (lines 69–87)

```cpp
inline double mandelbulb(const Point3& pos, int iterations = 10, double power = 8.0) {
    Vec3 z = pos;
    double dr = 1.0, r = 0.0;
```

Lines 70–72: the famous 3D fractal. `z` is repeatedly transformed; `dr` tracks how fast nearby points spread apart
(needed for the distance estimate).

```cpp
    for (int i = 0; i < iterations; i++) {
        r = z.length();
        if (r > 2.0) break;
```

Lines 73–75: repeat the transformation. If the point has escaped beyond radius 2, it will fly off to infinity: stop.

```cpp
        double theta = std::acos(clampd(z.z / r, -1.0, 1.0));
        double phi = std::atan2(z.y, z.x);
        dr = std::pow(r, power - 1.0) * power * dr + 1.0;
        double zr = std::pow(r, power);
        theta *= power;
        phi *= power;
        z = zr * Vec3(std::sin(theta) * std::cos(phi), std::sin(phi) * std::sin(theta), std::cos(theta));
        z += pos;
    }
```

* Lines 76–77: write `z` in spherical coordinates (two angles and a radius).
* Line 78: update the spreading rate (the derivative of the transformation).
* Lines 79–82: "raise z to the 8th power": the radius is raised to the power, and both angles are **multiplied** by it.
* Line 83: add the original point (like the Mandelbrot rule `z → z² + c`).

```cpp
    if (r < 1e-12) return 0.0;
    return 0.5 * std::log(r) * r / dr;
}
```

Lines 85–86: the **distance estimate**: how far we probably are from the fractal's surface, based on how fast the point
escaped. It's not an exact distance, which is why the renderer takes slightly careful steps for it.

---

## Block F — `SDFObject`: setup (lines 91–99)

```cpp
class SDFObject : public Hittable {
public:
    using DistanceFn = std::function<double(const Point3&)>;

    SDFObject(DistanceFn fn, const AABB& bounds, std::shared_ptr<Material> mat,
              int max_steps = 256, double epsilon = 1e-4, double step_scale = 1.0)
        : fn(fn), bounds(bounds), mat(mat), max_steps(max_steps), epsilon(epsilon), step_scale(step_scale) {}
```

* Line 95: `DistanceFn` = any function from a point to a distance (usually a lambda).
* Lines 97–99: inputs: the distance function, a **bounding box** (we only search inside it), the material, and three
  tuning numbers:
  * `max_steps`: give up after this many steps,
  * `epsilon`: how close counts as "touching",
  * `step_scale`: take slightly smaller steps for functions that aren't exact distances (fractals).

---

## Block G — `SDFObject::hit` (lines 101–125)

```cpp
        double t0 = ray_t.min, t1 = ray_t.max;
        if (!clip_to_box(r, t0, t1)) return false;
```

Lines 103–104: first cut the ray to the part inside the bounding box. If it misses the box, there's nothing to do (this
also keeps us from marching through empty space forever).

```cpp
        double len = r.direction().length();
        double t = t0;
        double sign = fn(r.at(t)) < 0 ? -1.0 : 1.0;
```

* Line 106: the direction's length (t is measured in units of it).
* Line 107: start at the box entry.
* Line 109: if we start **inside** the shape (for example a ray refracting out of it), flip the sign so the marching
  looks for the way out.

```cpp
        for (int i = 0; i < max_steps && t < t1; i++) {
            Point3 p = r.at(t);
            double d = sign * fn(p);
            if (d < epsilon) {
```

* Line 110: march, at most `max_steps` times and never past the box exit.
* Lines 111–112: the distance to the nearest surface from the current point.
* Line 113: closer than epsilon → we've touched the surface.

```cpp
                if (t <= ray_t.min) { t += 2 * epsilon / len; continue; }
                rec.t = t;
                rec.p = p;
                rec.set_face_normal(r, normal_at(p));
                rec.u = rec.v = 0;
                rec.mat = mat.get();
                return true;
            }
            t += step_scale * d / len;    // distance d along a direction of length 'len'
        }
        return false;
```

* Line 114: if we're touching right at the start (a ray that just bounced off this surface), step a little further and
  keep going, instead of hitting the same point again.
* Lines 115–120: fill the hit record; the normal comes from the gradient (block H).
* Line 122: **the key step**: move forward by the distance `d`. Because nothing is closer than `d`, this can never jump
  through a surface. Steps are long in open space and get shorter near the surface.
* Line 124: too many steps or past the box: no hit.

---

## Block H — `normal_at` (lines 129–137)

```cpp
    Vec3 normal_at(const Point3& p) const {
        const double h = 1e-4;
        Vec3 n(fn(p + Vec3(h, 0, 0)) - fn(p - Vec3(h, 0, 0)),
               fn(p + Vec3(0, h, 0)) - fn(p - Vec3(0, h, 0)),
               fn(p + Vec3(0, 0, h)) - fn(p - Vec3(0, 0, h)));
        double l = n.length();
        return l > 0 ? n / l : Vec3(0, 1, 0);
    }
```

The surface normal is the direction in which the distance **grows fastest** (the *gradient*).

* Lines 132–134: for each axis, sample the distance a tiny step to each side and take the difference (**central
  differences**). If moving in +x increases the distance, the surface faces that way.
* Lines 135–136: normalize (with a safe fallback).

This works for **any** distance function, however complicated: six extra evaluations per hit.

---

## Block I — Data and box clipping (lines 139–160)

Lines 140–145: the stored function, box, material and tuning numbers.

```cpp
    bool clip_to_box(const Ray& r, double& t0, double& t1) const {
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = bounds.axis_interval(axis);
            double inv = 1.0 / r.direction()[axis];
            double a = (ax.min - r.origin()[axis]) * inv;
            double b = (ax.max - r.origin()[axis]) * inv;
            if (a > b) { double tmp = a; a = b; b = tmp; }
            if (a > t0) t0 = a;
            if (b < t1) t1 = b;
            if (t1 <= t0) return false;
        }
        return true;
    }
```

Lines 147–159: the **slab test** from [aabb.md](aabb.md), but here it **returns** the entry and exit distances (`t0`,
`t1` are passed by reference `&`), because we need to know where to start and stop marching.

---

## Check your understanding

1. What does `sphere(p, 1)` return for p = (3, 0, 0)? *(2: the point is 2 units from the surface.)*
2. Why can a sphere-tracing step never jump through a surface? *(The step is the distance to the **nearest** surface, so
   nothing can be closer.)*
3. What does subtracting a constant from an SDF do? *(Inflates the shape with rounded corners.)*
4. Why do fractals need `step_scale` below 1? *(Their distance is only an estimate and can be slightly too large.)*
