# Line by line: `include/pixel/volume.h`

[← Line-by-line index](README.md) · [Chapter 28 (the theory)](../28-volumes.md)

**What this file does, in one sentence:** it defines **volumes** (fog, smoke, clouds): shapes filled with tiny particles
where light can scatter anywhere inside, not only at a surface.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–16 | |
| B. `ConstantMedium`: constructors | 18–27 | same density everywhere |
| C. `ConstantMedium::hit` | 29–53 | where (if at all) the ray hits a particle |
| D. `ConstantMedium`: box and data | 55–61 | |
| E. `VariableMedium` | 63–106 | density that changes from place to place |
| F. End | 108 | |

---

## Block A — Comments, includes (lines 1–16)

Comments; `#pragma once`; `<cmath>` (log), `<memory>`, `<functional>` (density functions); our `vec3.h`,
`hittable.h`, `material.h` (the `Isotropic` material), `random.h`; `namespace pixel`.

---

## Block B — `ConstantMedium`: constructors (lines 18–27)

```cpp
class ConstantMedium : public Hittable {
public:
    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, const Color& albedo)
        : boundary(boundary), neg_inv_density(-1.0 / density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}
```

* Line 21: inputs:
  * `boundary` = the shape of the fog (a box or a sphere),
  * `density` = how thick it is (the chance of hitting a particle per unit of distance),
  * `albedo` = the particles' color (white = white fog, black = dark smoke).
* Line 22: store `−1 / density` (used in the distance formula below).
* Line 23: the material for particles: `Isotropic` (scatter in any direction).
* Lines 25–27: the same with a texture instead of a color.

---

## Block C — `ConstantMedium::hit` (lines 29–53)

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord rec1, rec2;
        if (!boundary->hit(r, Interval::universe(), rec1)) return false;
        if (!boundary->hit(r, Interval(rec1.t + 0.0001, infinity), rec2)) return false;
```

* Line 30: two records: where the ray **enters** and **leaves** the shape.
* Line 32: find the first hit with the boundary anywhere along the whole line (even behind the ray's start, so that rays
  starting **inside** the fog also work).
* Line 33: find the next hit after that: the exit.

```cpp
        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;
        if (rec1.t >= rec2.t) return false;
        if (rec1.t < 0) rec1.t = 0;
```

* Lines 35–36: keep only the part inside the allowed range.
* Line 37: nothing left → no hit.
* Line 38: never start behind the ray.

```cpp
        double ray_length = r.direction().length();
        double distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        double hit_distance = neg_inv_density * std::log(1.0 - random_double());
        if (hit_distance > distance_inside_boundary) return false;   // passed through
```

* Line 40: the direction's length (t isn't a real distance unless the direction has length 1).
* Line 41: how far the ray travels inside the fog.
* Line 43: a **random distance to the next particle**. In a uniform fog, this distance follows the exponential
  distribution: `−(1/density) × ln(random)`. Thick fog → short distances.
* Line 44: if the particle would be beyond the exit, the ray goes straight through: no hit.

```cpp
        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);
        rec.normal = Vec3(1, 0, 0);   // arbitrary: particles have no surface
        rec.front_face = true;
        rec.u = rec.v = 0;
        rec.mat = phase_function.get();
        return true;
    }
```

* Lines 46–47: the particle's position.
* Line 48: particles have no real surface, so any normal is fine (the Isotropic material ignores it).
* Line 51: the particle's material: scatter in a random direction.

---

## Block D — `ConstantMedium`: box and data (lines 55–61)

The bounding box is the boundary's box. The stored data: the boundary, `−1/density`, and the particle material.

---

## Block E — `VariableMedium` (lines 63–106)

For fog whose density changes (clouds: thick in the middle, thin at the edges). It uses **delta tracking**.

```cpp
    VariableMedium(std::shared_ptr<Hittable> boundary, double max_density,
                   std::function<double(const Point3&)> density, const Color& albedo)
        : boundary(boundary), max_density(max_density), density(density),
          phase_function(std::make_shared<Isotropic>(albedo)) {}
```

Lines 67–70: the shape, the **highest** density anywhere, a **function** giving the density at any point, and the color.

Lines 72–79: find the entry and exit, exactly as in block C.

```cpp
        double ray_length = r.direction().length();
        double t = rec1.t;
        while (true) {
            t -= std::log(1.0 - random_double()) / (max_density * ray_length);
            if (t >= rec2.t) return false;
            if (random_double() < density(r.at(t)) / max_density) break;
        }
```

* Line 82: start at the entry.
* Line 83: repeat:
  * Line 85: take a random step **as if** the whole fog had the maximum density.
  * Line 86: passed the exit? No hit.
  * Line 88: accept this point as a **real** particle with chance `density here ÷ max density`. In thin areas, most
    points are rejected (the ray keeps going); in thick areas, most are accepted.
* This trick gives exactly the right result, as long as `max_density` really is the highest density.

Lines 90–96: fill the hit record as in block C. Lines 99–105: the box and stored data.

---

## Block F — End (line 108)

`} // namespace pixel`

---

## Check your understanding

1. What does a higher density do? *(Particles are closer together: rays scatter sooner: thicker fog.)*
2. Why search the boundary with `Interval::universe()`? *(So rays that start inside the fog still find where they entered.)*
3. In delta tracking, why accept only some steps? *(To follow the real density: thin places reject most steps.)*
