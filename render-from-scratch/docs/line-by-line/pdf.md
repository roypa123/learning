# Line by line: `include/pixel/pdf.h`

[← Line-by-line index](README.md) · Chapters [30](../30-importance-sampling.md), [31](../31-light-sampling.md)

**What this file does, in one sentence:** it gives us a small **local coordinate system** around a surface (`ONB`) and
several ways to **pick random directions**, each able to say how likely a direction is (`PDF`s).

"PDF" = probability density function: "how likely is each direction to be picked?"

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–15 | |
| B. `ONB` | 17–37 | three axes around a normal; convert between local and world |
| C. `PDF` base class | 39–44 | the two questions every PDF answers |
| D. `SpherePDF` | 46–51 | any direction, all equal |
| E. `CosinePDF` | 53–64 | favors directions near the normal (matte surfaces) |
| F. `HittablePDF` | 66–75 | directions toward objects (lights) |
| G. `MixturePDF` | 77–89 | half one way, half another |
| H. End | 91 | |

---

## Block A — Comments, includes (lines 1–15)

Comments; `#pragma once`; `<cmath>`, `<memory>`; our `vec3.h`, `random.h`, `hittable.h`; `namespace pixel`.

---

## Block B — `ONB` (orthonormal basis) (lines 17–37)

Many formulas are easy when the surface normal points straight "up" (+z). An ONB is a set of three axes (u, v, w), at
right angles to each other and all length 1, with **w = the normal**. We do the math in this local system and then
convert back.

```cpp
    explicit ONB(const Vec3& n) {
        axis[2] = unit_vector(n);
        Vec3 a = (std::fabs(axis[2].x) > 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        axis[1] = unit_vector(cross(axis[2], a));
        axis[0] = cross(axis[2], axis[1]);
    }
```

* Line 21: w (stored in `axis[2]`) = the normal, length 1.
* Line 22: pick any helper direction `a` that is **not** parallel to w: normally the x-axis, but if w is almost along x,
  use the y-axis instead (a cross product of parallel vectors is zero, which would break things).
* Line 23: v = w × a: at right angles to w.
* Line 24: u = w × v: at right angles to both. Now we have three perpendicular axes.

```cpp
    const Vec3& u() const { return axis[0]; }
    const Vec3& v() const { return axis[1]; }
    const Vec3& w() const { return axis[2]; }
```

Lines 26–28: read the axes.

```cpp
    Vec3 transform(const Vec3& v) const { return v.x * axis[0] + v.y * axis[1] + v.z * axis[2]; }
    Vec3 to_local(const Vec3& v) const { return Vec3(dot(v, axis[0]), dot(v, axis[1]), dot(v, axis[2])); }
```

* Line 31: **local → world**: "x steps along u, y along v, z along w". So local (0, 0, 1) becomes the normal.
* Line 33: **world → local**: how far the vector goes along each axis (dot products).

Line 36: the three axes are stored in an array.

---

## Block C — `PDF` base class (lines 39–44)

```cpp
class PDF {
public:
    virtual ~PDF() {}
    virtual double value(const Vec3& direction) const = 0;   // density of this direction
    virtual Vec3 generate() const = 0;                        // pick a random direction
};
```

Every PDF must answer two questions:

* Line 42: `value`: "how likely would you pick this direction?"
* Line 43: `generate`: "pick a random direction for me."

The two must agree: directions you often generate must have a high value. The renderer divides by `value` to keep the
result correct (chapter 30).

---

## Block D — `SpherePDF` (lines 46–51)

```cpp
class SpherePDF : public PDF {
public:
    double value(const Vec3&) const override { return 1.0 / (4.0 * pi); }
    Vec3 generate() const override { return random_unit_vector(); }
};
```

All directions equally likely. The whole sphere of directions has size 4π, so each direction's density is 1/(4π).
Used for fog.

---

## Block E — `CosinePDF` (lines 53–64)

```cpp
class CosinePDF : public PDF {
public:
    explicit CosinePDF(const Vec3& w) : uvw(w) {}
```

Line 56: build an ONB around the given normal.

```cpp
    double value(const Vec3& direction) const override {
        double cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0.0, cosine_theta / pi);
    }
```

Lines 57–60: density = cos(angle to the normal) ÷ π. Straight up: 1/π; sideways: 0; below the surface: 0.

```cpp
    Vec3 generate() const override { return uvw.transform(random_cosine_direction()); }
```

Line 61: make a cosine-weighted direction around +z (see [random.md](random.md)), then turn it to point around the
normal with `transform`.

---

## Block F — `HittablePDF` (lines 66–75)

```cpp
class HittablePDF : public PDF {
public:
    HittablePDF(const Hittable& objects, const Point3& origin) : objects(objects), origin(origin) {}
    double value(const Vec3& direction) const override { return objects.pdf_value(origin, direction); }
    Vec3 generate() const override { return objects.random(origin); }
private:
    const Hittable& objects;
    Point3 origin;
};
```

Directions from a point (`origin`) **toward some objects**, usually the lights.

* Line 69: remember the objects (by reference) and the point.
* Line 70: ask the objects how likely the direction is (spheres and quads each know how, chapters 25 and 31).
* Line 71: ask the objects for a random direction toward them.

---

## Block G — `MixturePDF` (lines 77–89)

```cpp
class MixturePDF : public PDF {
public:
    MixturePDF(const PDF* p0, const PDF* p1) { p[0] = p0; p[1] = p1; }
    double value(const Vec3& direction) const override {
        return 0.5 * p[0]->value(direction) + 0.5 * p[1]->value(direction);
    }
    Vec3 generate() const override {
        return random_double() < 0.5 ? p[0]->generate() : p[1]->generate();
    }
private:
    const PDF* p[2];
};
```

Combines two ways of picking directions (for example "toward the light" and "like a matte surface"):

* Line 80: store pointers to the two PDFs.
* Lines 81–83: the chance of a direction = the average of the two chances (each is used half the time).
* Lines 84–86: flip a coin: use the first or the second to pick a direction.

This is how the camera aims half its bounces at the lights (chapter 31).

---

## Block H — End (line 91)

`} // namespace pixel`

---

## Check your understanding

1. In an ONB made from the normal (0, 1, 0), what does `transform((0,0,1))` give? *((0, 1, 0), the normal.)*
2. Why must `value` and `generate` agree? *(The renderer divides by `value`; if they disagree, images get too bright or
   too dark.)*
3. What is the CosinePDF value for a direction exactly along the normal? *(1/π ≈ 0.318.)*
