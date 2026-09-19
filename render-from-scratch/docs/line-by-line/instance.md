# Line by line: `include/pixel/instance.h`

[← Line-by-line index](README.md) · [Chapter 27 (the theory)](../27-instances.md)

**What this file does, in one sentence:** it lets you **move** (`Translate`) and **rotate** (`RotateY`) any object
without changing the object itself, by moving the **ray** the opposite way instead.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–13 | |
| B. `Translate` | 15–36 | move an object |
| C. `RotateY`: constructor | 38–60 | angle, and a new bounding box |
| D. `RotateY::hit` | 62–72 | rotate the ray in, rotate the result out |
| E. `RotateY`: data and helpers | 74–85 | the rotation formulas |
| F. End | 87 | |

---

## Block A — Comments, includes (lines 1–13)

Comments explaining the trick; `#pragma once`; `<cmath>`, `<memory>`; our `vec3.h`, `hittable.h`; `namespace pixel`.

---

## Block B — `Translate` (lines 15–36)

```cpp
class Translate : public Hittable {
public:
    Translate(std::shared_ptr<Hittable> object, const Vec3& offset) : object(object), offset(offset) {
        bbox = object->bounding_box() + offset;
    }
```

* Line 15: a Translate **is a** Hittable that wraps another object.
* Line 17: store the wrapped object (a shared pointer, so the same object can be wrapped many times) and the offset.
* Line 18: its bounding box = the object's box, moved by the offset.

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Ray offset_r(r.origin() - offset, r.direction(), r.time());
        if (!object->hit(offset_r, ray_t, rec)) return false;
        rec.p += offset;
        return true;
    }
```

* Line 23: instead of moving the object forward by `offset`, move the **ray** backward by `offset`. The direction doesn't
  change.
* Line 24: test the original, unmoved object with the moved ray. The distance `t` is the same in both situations.
* Line 26: the hit point was found in the object's own place, so move it forward to the real position. (The normal
  doesn't change when moving.)

Lines 30–35: the box, and the stored data.

---

## Block C — `RotateY`: constructor (lines 38–60)

Rotates an object around the vertical (y) axis.

```cpp
    RotateY(std::shared_ptr<Hittable> object, double angle_degrees) : object(object) {
        double radians = degrees_to_radians(angle_degrees);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        AABB b = object->bounding_box();
```

* Lines 41–43: compute sin and cos of the angle **once** (they're used for every ray).
* Line 44: the object's original box.

```cpp
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
```

A rotated box is tilted, and our boxes can't tilt, so we need a new, bigger box around it:

* Lines 47–48: start min at +∞ and max at −∞ (anything will be smaller/larger).
* Lines 49–51: three loops of 2 → the box's **8 corners** (each coordinate is either its min or its max).
* Lines 52–54: pick this corner's coordinates (`i ? a : b` = if i is 1 then a else b).
* Line 55: rotate the corner.
* Lines 56–57: grow min and max to include it.
* Line 59: the new box around all rotated corners.

---

## Block D — `RotateY::hit` (lines 62–72)

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Ray rotated_r(to_object(r.origin()), to_object(r.direction()), r.time());
        if (!object->hit(rotated_r, ray_t, rec)) return false;
        rec.p = to_world(rec.p);
        rec.normal = to_world(rec.normal);
        return true;
    }
```

* Line 64: rotate the ray's start **and** direction the **opposite** way, into the object's own space.
* Line 65: test the original object.
* Lines 67–68: rotate the hit point and the normal **back** into the world. (Rotations don't change lengths, so `t` stays
  the same, and the normal stays length 1.)

---

## Block E — `RotateY`: data and helpers (lines 74–85)

```cpp
    Vec3 to_world(const Vec3& p) const {
        return Vec3(cos_theta * p.x + sin_theta * p.z, p.y, -sin_theta * p.x + cos_theta * p.z);
    }
    Vec3 to_object(const Vec3& p) const {
        return Vec3(cos_theta * p.x - sin_theta * p.z, p.y, sin_theta * p.x + cos_theta * p.z);
    }
```

* Lines 79–81: rotate a point by +θ around the y axis: y stays the same; x and z turn like a 2D rotation.
* Lines 82–84: rotate by −θ: the same formula with the signs of the sin terms flipped. Doing one then the other gives back
  the original point.

---

## Block F — End (line 87)

`} // namespace pixel`

---

## Check your understanding

1. To move an object right by 3, which way does `Translate` move the ray? *(Left by 3.)*
2. Why does RotateY compute a new box from 8 corners? *(A rotated box needs a bigger axis-aligned box around it.)*
3. Why must the normal be rotated back but not changed by Translate? *(Rotation turns directions; moving doesn't.)*
