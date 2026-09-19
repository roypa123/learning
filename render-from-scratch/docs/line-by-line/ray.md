# Line by line: `include/pixel/ray.h`

[← Line-by-line index](README.md) · Chapters [13](../13-rays-and-camera.md), [15](../15-normals-and-lists.md)

**What this file does, in one sentence:** it defines a **Ray** (a starting point plus a direction) and an
**Interval** (a range of numbers from `min` to `max`).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. `class Ray` | 12–28 | origin, direction, time; the point at distance t |
| C. `struct Interval` | 30–48 | a range [min, max] and useful tests |
| D. Shifting an interval | 50–52 | interval + number |
| E. End | 54 | |

---

## Block A — Comments, includes (lines 1–10)

* Lines 1–6: comments. Line 3 gives the ray formula: **P(t) = origin + t × direction**.
* Line 7: `#pragma once`. Line 8: we need `Vec3`/`Point3`. Line 10: `namespace pixel`.

---

## Block B — `class Ray` (lines 12–28)

A ray is like a laser beam: it starts somewhere and goes on forever in one direction.

```
 origin                                      t = 2
   ●───────────────●───────────────●──────────▶
  t = 0          t = 1
```

```cpp
class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction, double time = 0.0)
        : orig(origin), dir(direction), tm(time) {}
```

* Line 12: a **class** called Ray. Line 13: `public:` = the following can be used by anyone.
* Line 14: an empty ray (origin and direction all zero). Needed so you can create a Ray variable and fill it later.
* Lines 15–16: the normal constructor: `Ray r(start, direction);`. `time` is optional (default 0); it's used for motion
  blur in chapter 22. The initializer list copies the inputs into the private members.

```cpp
    const Point3& origin() const { return orig; }
    const Vec3& direction() const { return dir; }
    double time() const { return tm; }   // moment inside the camera shutter (motion blur)
```

Lines 18–20: **getter** functions: they let others **read** the members, but not change them (the members are
private, and the returned references are `const`). This protects the ray from accidental changes.

```cpp
    Point3 at(double t) const { return orig + t * dir; }
```

Line 22: the point at distance `t` along the ray: start + t × direction. This is the most important function of the
class. Example: origin (0,0,0), direction (0,0,−1): `at(2)` = (0, 0, −2).

```cpp
private:
    Point3 orig;
    Vec3 dir;
    double tm = 0.0;
};
```

* Line 24: `private:` = only the class itself can use these.
* Lines 25–27: the stored data: origin, direction, time.
* Line 28: end of class.

---

## Block C — `struct Interval` (lines 30–48)

An Interval is a range of numbers, like "from 0.001 to infinity". Ray tracing uses it to say "only accept hits at
distances in this range".

```cpp
struct Interval {
    double min = +infinity;   // default interval is empty
    double max = -infinity;
```

* Lines 31–32: the lowest and highest value. The default (min = +∞, max = −∞) is an **empty** interval: no number can
  be both ≥ +∞ and ≤ −∞. That's a useful starting value when building bigger intervals.

```cpp
    Interval() {}
    Interval(double mn, double mx) : min(mn), max(mx) {}
    Interval(const Interval& a, const Interval& b)
        : min(a.min <= b.min ? a.min : b.min), max(a.max >= b.max ? a.max : b.max) {}
```

* Line 34: an empty interval.
* Line 35: `Interval(0.001, 100)` = from 0.001 to 100.
* Lines 37–38: the smallest interval that covers **both** a and b: the smaller of the two mins and the larger of the
  two maxes. Example: [1, 3] and [2, 5] → [1, 5]. (Used by bounding boxes.)

```cpp
    double size() const { return max - min; }
    bool contains(double x) const { return min <= x && x <= max; }
    bool surrounds(double x) const { return min < x && x < max; }
    double clamp(double x) const { return x < min ? min : (x > max ? max : x); }
    Interval expand(double delta) const { double p = delta / 2; return Interval(min - p, max + p); }
```

| Line | Function | Meaning | Example with [2, 5] |
|------|----------|---------|---------------------|
| 40 | `size()` | length of the range | 3 |
| 41 | `contains(x)` | is x inside, **edges included**? | contains(5) = true |
| 42 | `surrounds(x)` | is x inside, **edges excluded**? | surrounds(5) = false |
| 43 | `clamp(x)` | move x into the range | clamp(9) = 5 |
| 44 | `expand(d)` | a bigger range, d/2 more on each side | expand(2) = [1, 6] |

```cpp
    static Interval empty() { return Interval(+infinity, -infinity); }
    static Interval universe() { return Interval(-infinity, +infinity); }
};
```

* Line 46: the empty interval, written clearly: `Interval::empty()`.
* Line 47: the interval containing **everything**, from −∞ to +∞.
* `static` = you call them on the type itself (`Interval::universe()`), not on a particular interval.

---

## Block D — Shifting an interval (lines 50–52)

```cpp
inline Interval operator+(const Interval& ival, double displacement) {
    return Interval(ival.min + displacement, ival.max + displacement);
}
```

`[2, 5] + 10` = `[12, 15]`: move the whole range. Used when moving bounding boxes (chapter 27).

---

## Block E — End (line 54)

`} // namespace pixel`

---

## Check your understanding

1. A ray starts at (1, 0, 0) with direction (0, 2, 0). What is `at(3)`? *((1, 6, 0))*
2. Is 0 inside `Interval(0, 10)` with `contains`? With `surrounds`? *(Yes; no.)*
3. Why is the default interval empty? *(min = +∞ and max = −∞, so nothing fits. It's a good start for "grow to cover
   things".)*
