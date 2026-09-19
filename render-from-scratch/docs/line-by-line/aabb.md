# Line by line: `include/pixel/aabb.h`

[← Line-by-line index](README.md) · [Chapter 23 (the theory)](../23-bvh.md)

**What this file does, in one sentence:** it defines an **axis-aligned bounding box** (AABB): the simplest box around an
object, with a very fast test "does this ray pass through the box?".

"Axis-aligned" = the box's sides are parallel to the x, y and z axes (never tilted).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. Data and constructors | 13–29 | a box = three intervals |
| C. `axis_interval` | 31 | get the x, y or z range by number |
| D. `hit`: the slab test | 33–48 | does the ray pass through the box? |
| E. Helpers | 50–62 | longest side, center, surface area, empty box |
| F. Padding | 64–73 | never allow zero thickness |
| G. Moving a box | 75–77 | box + offset |
| H. End | 79 | |

---

## Block A — Comments, includes (lines 1–11)

Comments; `#pragma once`; `vec3.h` and `ray.h` (Ray and Interval); `namespace pixel`.

---

## Block B — Data and constructors (lines 13–29)

```cpp
class AABB {
public:
    Interval x, y, z;
```

Line 15: a box is just three ranges: from x.min to x.max, y.min to y.max, z.min to z.max.

```cpp
    AABB() {}   // empty box
```

Line 17: the default box is empty (each Interval starts empty).

```cpp
    AABB(const Interval& ix, const Interval& iy, const Interval& iz) : x(ix), y(iy), z(iz) {
        pad_to_minimums();
    }
```

Lines 18–20: from three ranges. Then make sure no side is too thin (block F).

```cpp
    AABB(const Point3& a, const Point3& b) {
        x = a.x <= b.x ? Interval(a.x, b.x) : Interval(b.x, a.x);
        y = a.y <= b.y ? Interval(a.y, b.y) : Interval(b.y, a.y);
        z = a.z <= b.z ? Interval(a.z, b.z) : Interval(b.z, a.z);
        pad_to_minimums();
    }
```

Lines 22–27: from two opposite corners, in any order. For each axis, put the smaller value first.

```cpp
    AABB(const AABB& a, const AABB& b) : x(a.x, b.x), y(a.y, b.y), z(a.z, b.z) {}
```

Line 29: the box around **two boxes**. Uses the Interval constructor that covers two intervals.

---

## Block C — `axis_interval` (line 31)

```cpp
    const Interval& axis_interval(int n) const { return n == 1 ? y : (n == 2 ? z : x); }
```

Get the range for axis 0 (x), 1 (y) or 2 (z), so loops can go over the three axes.

---

## Block D — `hit`: the slab test (lines 33–48)

A box is where three "slabs" overlap: the space between two planes on x, the same on y, and on z. The ray is inside the
box during the time it's inside **all three** slabs at once.

```cpp
    bool hit(const Ray& r, Interval ray_t) const {
        const Point3& o = r.origin();
        const Vec3& d = r.direction();
```

* Line 34: `ray_t` is passed **by value** (a copy): we'll shrink it inside the function without changing the caller's.
* Lines 35–36: short names for the origin and direction.

```cpp
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / d[axis];
            double t0 = (ax.min - o[axis]) * adinv;
            double t1 = (ax.max - o[axis]) * adinv;
```

* Line 37: for x, y, z.
* Line 38: the box's range on this axis.
* Line 39: 1 ÷ the direction on this axis (computed once, then multiplied twice).
* Lines 40–41: the ray distances `t` where it crosses the two planes of this slab: solve `origin + t × direction = plane`
  → `t = (plane − origin) / direction`.

```cpp
            if (t0 > t1) { double tmp = t0; t0 = t1; t1 = tmp; }
```

Line 42: if the ray goes in the negative direction, it hits the "max" plane first. Swap so that t0 is the entry and t1
the exit.

```cpp
            if (t0 > ray_t.min) ray_t.min = t0;
            if (t1 < ray_t.max) ray_t.max = t1;
            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }
```

* Lines 43–44: shrink the allowed range to the part where the ray is inside **this** slab too (the overlap).
* Line 45: if the range became empty, the ray is never inside all slabs at the same time: it **misses** the box.
* Line 47: survived all three axes: it hits the box.

```
          x-slab
      │         │
  ────┼─────────┼──── y-slab
      │   box   │          ray inside x-slab during [2, 7]
  ────┼─────────┼────      ray inside y-slab during [4, 9]
      │         │          overlap [4, 7] → not empty → hit
```

(If the direction on an axis is exactly 0, `1/0` is infinity; floating-point math handles that correctly.)

---

## Block E — Helpers (lines 50–62)

```cpp
    int longest_axis() const {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }
```

Lines 50–53: which side is longest: 0 = x, 1 = y, 2 = z. The BVH splits along it.

```cpp
    Point3 centroid() const { return Point3((x.min + x.max) * 0.5, (y.min + y.max) * 0.5, (z.min + z.max) * 0.5); }
```

Line 55: the box's center (the middle of each range).

```cpp
    double surface_area() const {
        double a = x.size(), b = y.size(), c = z.size();
        return 2.0 * (a * b + b * c + c * a);
    }
```

Lines 57–60: the total area of the six faces (for smarter BVH building, not used yet).

```cpp
    static AABB empty() { return AABB(Interval::empty(), Interval::empty(), Interval::empty()); }
```

Line 62: an empty box: a good starting point for "grow to include things".

---

## Block F — Padding (lines 64–73)

```cpp
    void pad_to_minimums() {
        const double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
```

A flat object (a floor quad) has a box with zero thickness on one axis, and the slab test can then fail because of
rounding. So any side thinner than 0.0001 is widened to 0.0001.

---

## Block G — Moving a box (lines 75–77)

```cpp
inline AABB operator+(const AABB& b, const Vec3& offset) {
    return AABB(b.x + offset.x, b.y + offset.y, b.z + offset.z);
}
```

`box + offset` = the same box moved. Used when objects are moved with `Translate` (chapter 27).

---

## Block H — End (line 79)

`} // namespace pixel`

---

## Check your understanding

1. Why swap t0 and t1 on line 42? *(A ray going in −x enters through the max plane first.)*
2. When does the test say "miss"? *(When the overlap of the three slab ranges is empty.)*
3. Why pad thin boxes? *(Zero thickness breaks the test because of rounding.)*
