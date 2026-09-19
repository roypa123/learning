# Line by line: `include/pixel/hittable.h`

[← Line-by-line index](README.md) · [Chapter 15 (the theory)](../15-normals-and-lists.md)

**What this file does, in one sentence:** it defines what "something a ray can hit" means (`Hittable`), what we
remember about a hit (`HitRecord`), and a list of objects that finds the **closest** hit (`HittableList`).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–15 | |
| B. Forward declaration | 17 | "Material exists, details later" |
| C. `HitRecord` | 19–33 | everything about one hit |
| D. `Hittable` (the interface) | 35–49 | the questions every object must answer |
| E. `HittableList`: storage | 51–64 | a list of objects |
| F. `HittableList::hit` | 66–78 | find the closest hit |
| G. `HittableList`: box and light sampling | 80–100 | used in later chapters |
| H. End | 102 | |

---

## Block A — Comments, includes (lines 1–15)

* Lines 1–6: comments.
* Line 7: `#pragma once`.
* Line 8 `<memory>`: `std::shared_ptr` (smart pointers).
* Line 9 `<vector>`: lists.
* Lines 10–13: our `vec3.h`, `ray.h` (Ray, Interval), `aabb.h` (bounding boxes, chapter 23) and `random.h`.
* Line 15: `namespace pixel`.

---

## Block B — Forward declaration (line 17)

```cpp
class Material;   // defined in material.h
```

A hit record needs to **point to** a material, but `material.h` needs this file first. This line says "a class called
Material exists" so we can use a pointer to it before its full definition. It's called a **forward declaration**.

---

## Block C — `HitRecord` (lines 19–33)

When a ray hits something, the object fills in one of these:

```cpp
struct HitRecord {
    Point3 p;                          // the hit point
    Vec3 normal;                       // unit normal, always facing AGAINST the ray
    const Material* mat = nullptr;     // what the surface is made of
    double t = 0.0;                    // ray parameter: p = origin + t*direction
    double u = 0.0, v = 0.0;           // texture coordinates
    bool front_face = true;            // did we hit the outside of the surface?
```

| Line | Member | Meaning |
|------|--------|---------|
| 21 | `p` | where the hit is, in 3D |
| 22 | `normal` | the direction the surface faces, length 1, turned to face the incoming ray |
| 23 | `mat` | a pointer to the surface's material (matte, metal, glass...). `nullptr` = none |
| 24 | `t` | how far along the ray the hit is |
| 25 | `u`, `v` | the position on the surface for textures (chapter 24) |
| 26 | `front_face` | true if the ray hit the **outside** of the surface |

```cpp
    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};
```

* Line 29: the object gives its **outward** normal (pointing out of the object).
* Line 30: if the ray and the outward normal point in **opposite** directions (dot < 0), the ray comes from outside:
  front face.
* Line 31: store the normal so it always faces the ray: keep it for front hits, flip it for back hits (a ray inside a
  glass ball hitting the wall from inside).

---

## Block D — `Hittable`, the interface (lines 35–49)

```cpp
class Hittable {
public:
    virtual ~Hittable() = default;
```

* Line 35: the **base class** for all objects: spheres, quads, triangles, lists...
* Line 37: a **virtual destructor**. `~Hittable` runs when an object is destroyed. Making it `virtual` makes sure the
  right cleanup runs for a Sphere even if we only hold a `Hittable` pointer. `= default` = the usual behavior.
  (Rule: every base class with virtual functions should have this.)

```cpp
    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;
```

Line 40: **the** question: "Does ray `r` hit you at a distance inside `ray_t`? If yes, fill `rec` and return true."

* `virtual` = each kind of object answers in its own way; the right version is chosen at run time.
* `= 0` = **pure virtual**: the base class has no answer; every derived class **must** provide one.
* `HitRecord& rec` = passed by reference so the function can fill it in for the caller.
* `const` at the end = testing a ray doesn't change the object.

```cpp
    virtual AABB bounding_box() const = 0;
```

Line 42: every object must also give a **box** around itself (used to speed things up in chapter 23).

```cpp
    virtual double pdf_value(const Point3& /*origin*/, const Vec3& /*direction*/) const { return 0.0; }
    virtual Vec3 random(const Point3& /*origin*/) const { return Vec3(1, 0, 0); }
};
```

Lines 46–48: two functions used for **light sampling** (chapter 31). They're virtual but **not** pure: they have a
simple default, so objects that never act as lights don't have to write them. The parameter names are in comments
(`/*origin*/`) because the default versions don't use them (this avoids "unused parameter" warnings).

---

## Block E — `HittableList`: storage (lines 51–64)

```cpp
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;
```

* Line 52: a list of objects is **also** a Hittable (`: public Hittable`). So a list can be used wherever one object is
  expected, and lists can even contain other lists.
* Line 54: the objects, stored as **shared pointers**. A `shared_ptr` deletes the object automatically when nobody uses it
  anymore. Using pointers to the base class lets one list hold spheres, quads, triangles... together.

```cpp
    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }
    void clear() { objects.clear(); bbox = AABB(); }
```

* Line 56: an empty list.
* Line 57: a list that starts with one object.
* Line 59: remove everything and reset the box.

```cpp
    void add(std::shared_ptr<Hittable> object) {
        bbox = objects.empty() ? object->bounding_box() : AABB(bbox, object->bounding_box());
        objects.push_back(object);
    }
```

* Line 62: grow the list's bounding box to include the new object (for the first object, just take its box).
  `->` calls a function through a pointer.
* Line 63: add the object at the end of the list.

---

## Block F — `HittableList::hit` (lines 66–78)

This is how we find **what the ray hits first** among many objects.

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        HitRecord temp;
        bool hit_anything = false;
        double closest = ray_t.max;
```

* Line 66: `override` = "this replaces the base class's `hit`" (the compiler checks the spelling for us).
* Line 67: a scratch record for each test.
* Line 68: nothing hit yet.
* Line 69: `closest` = the farthest distance we still accept. It starts at the interval's maximum.

```cpp
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

* Line 70: test each object in turn.
* Line 71: ask the object: "do you get hit between `ray_t.min` and `closest`?"
* Lines 72–74: if yes, remember it: it's the nearest so far. Then **shrink** `closest` to this hit's distance, so the
  following objects only count if they are **nearer**. Copy the record into `rec`.
* Line 77: tell the caller whether anything was hit. `rec` holds the nearest hit.

```
ray ──▶──●──────────●──────────●──▶
         A (t=2)    B (t=5)    C (t=9)
test A → hit at 2, closest = 2
test B → 5 is not below 2 → ignored
test C → ignored
result: A
```

---

## Block G — `HittableList`: box and light sampling (lines 80–100)

```cpp
    AABB bounding_box() const override { return bbox; }
```

Line 80: the box around all objects (kept up to date by `add`).

```cpp
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        if (objects.empty()) return 0.0;
        double weight = 1.0 / objects.size();
        double sum = 0.0;
        for (const auto& object : objects) sum += weight * object->pdf_value(origin, direction);
        return sum;
    }
```

Lines 83–89 (chapter 31): when a list is used as a list of **lights**, we pick one light at random (each with chance
1/N). The chance of a direction is then the **average** of each light's chance.

```cpp
    Vec3 random(const Point3& origin) const override {
        if (objects.empty()) return Vec3(1, 0, 0);
        int i = (int)(random_double() * objects.size());
        if (i >= (int)objects.size()) i = (int)objects.size() - 1;
        return objects[i]->random(origin);
    }
```

Lines 91–96: pick a random object index (0 to N−1, with a safety check), and ask that object for a random direction
towards it.

```cpp
private:
    AABB bbox;
};
```

Lines 98–100: the stored box, private.

---

## Block H — End (line 102)

`} // namespace pixel`

---

## Check your understanding

1. Why does `HittableList` inherit from `Hittable`? *(So a list can be used anywhere an object can, even inside another
   list.)*
2. What does `= 0` mean on line 40? *(Pure virtual: every object type must write its own `hit`.)*
3. Why shrink `closest` after each hit? *(So only nearer objects can replace the current hit.)*
4. When is `front_face` false? *(When the ray hits a surface from inside, e.g. inside glass.)*
