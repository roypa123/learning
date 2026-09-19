# Line by line: `include/pixel/vec3.h`

[← Line-by-line index](README.md) · [Chapter 12 (the theory)](../12-vectors.md)

**What this file does, in one sentence:** it creates `Vec3`, a small box of three numbers `(x, y, z)`,
plus the math we need on it. We use it for **positions**, **directions** and **colors**.

This is the most-used file in the whole library. Almost every other file includes it.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments and includes | 1–11 | Notes; load math and printing tools |
| B. Namespace | 13 | Put everything in the family name `pixel` |
| C. Constants and small helpers | 15–26 | π, infinity, and five number helpers |
| D. The `Vec3` struct | 28–58 | The 3-number box and what it can do by itself |
| E. Aliases | 60–62 | `Point3` and `Color` as other names for `Vec3` |
| F. Printing | 64–66 | Let `std::cout << v` print a vector |
| G. Arithmetic operators | 68–74 | `a + b`, `a - b`, `a * b`, `2 * v`, `v / 2` ... |
| H. Vector math | 76–105 | dot, cross, unit vector, min/max, lerp, abs, reflect, refract |
| I. Luminance | 107–108 | How bright a color looks |
| J. End of namespace | 110 | Close `pixel` |

---

## Block A — Comments and includes (lines 1–11)

```cpp
// pixel/vec3.h
// ... (comment lines) ...
#pragma once
#include <cmath>
#include <iostream>
```

* Lines 1–8: comments. They say what the file is for.
* Line 9 `#pragma once`: this is a **header file** (`.h`). Many files include it. `#pragma once` tells the
  compiler: "if this file was already included, skip it the second time". Without it we'd get errors like
  "Vec3 defined twice".
* Line 10 `#include <cmath>`: math functions like `std::sqrt` (square root), `std::fabs` (absolute value),
  `std::fmin`, `std::fmax`.
* Line 11 `#include <iostream>`: printing with `std::cout` and `std::ostream` (used in block F).

---

## Block B — Namespace (line 13)

```cpp
namespace pixel {
```

* A **namespace** is a family name. Everything until the closing `}` on line 110 is called `pixel::something`.
* Why? Another library might also have something called `Vec3` or `dot`. The family name keeps ours separate.
* In chapter programs we write `using namespace pixel;` so we can just write `Vec3` instead of `pixel::Vec3`.

---

## Block C — Constants and small helpers (lines 15–26)

### Lines 15–17: constants

```cpp
// Our own constants (we do not rely on M_PI, which is not standard C++).
constexpr double pi       = 3.1415926535897932385;
constexpr double infinity = 1e300;
```

* Line 15: a comment. Many people use `M_PI` for π, but it doesn't exist on every compiler, so we make our own.
* Line 16: `pi` = 3.14159…
  * `double` = a number with a fraction.
  * `constexpr` = "constant, known when compiling". Like `const`, but even stronger: the compiler can put
    the number directly into the code.
* Line 17: `infinity` = 1e300, meaning 1 followed by 300 zeros. That's so huge that we use it as "no limit",
  for example "a ray can go infinitely far".

### Line 19: degrees to radians

```cpp
inline double degrees_to_radians(double degrees) { return degrees * pi / 180.0; }
```

* A **function**: it takes `degrees` and gives back (`return`) the same angle in **radians**.
* Math functions like `std::sin` use radians. 180° = π radians, so we multiply by π / 180.
* Example: 90 → 90 × 3.14159 / 180 = 1.5708.
* `inline` is needed for functions written inside a header file: the header is included in many places, and
  `inline` tells the compiler that this is fine.

### Line 20: clamp01

```cpp
inline double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
```

* Keeps a number inside the range 0 to 1.
* `condition ? A : B` means "if condition then A, otherwise B".
* Read it as: if x is less than 0, give 0; otherwise, if x is more than 1, give 1; otherwise give x itself.
* Examples: −0.3 → 0, 0.4 → 0.4, 1.7 → 1.

### Line 21: clampd

```cpp
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
```

The same, but you choose the limits `lo` (low) and `hi` (high). `clampd(15, 0, 10)` gives 10.

### Line 22: lerpd (linear interpolation)

```cpp
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
```

* "Go from `a` towards `b` by the fraction `t`".
* t = 0 → a. t = 1 → b. t = 0.5 → exactly in the middle.
* Example: `lerpd(10, 20, 0.25)` = 10 + 10 × 0.25 = 12.5.

### Lines 23–26: smoothstep

```cpp
inline double smoothstep(double e0, double e1, double x) {
    double t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}
```

* A smooth "S-shaped" change from 0 to 1 while x goes from `e0` to `e1`.
* Line 24: `t` = where x is between e0 and e1, as a fraction 0..1 (clamped, so below e0 it's 0, above e1 it's 1).
* Line 25: `t²(3 − 2t)` bends the straight line into a soft S: it starts slowly, goes fast in the middle,
  and ends slowly. This gives smooth edges without a sharp corner.
* Line 26: `}` ends the function.
* Example: `smoothstep(0, 10, 5)` → t = 0.5 → 0.25 × 2 = **0.5**. `smoothstep(0, 10, 2)` → t = 0.2 →
  0.04 × 2.6 = 0.104 (slower than a straight line at the start).

---

## Block D — The `Vec3` struct (lines 28–58)

### Lines 28–29: the data

```cpp
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
```

* Line 28: `struct Vec3 {` begins a new type called `Vec3`. A **struct** groups variables together.
* Line 29: every `Vec3` holds three `double`s named `x`, `y`, `z`. The `= 0.0` means they start at 0 if you
  don't give values.
* For a color, x = red, y = green, z = blue.

### Lines 31–33: constructors (ways to create a Vec3)

```cpp
    Vec3() {}
    Vec3(double v) : x(v), y(v), z(v) {}
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}
```

A **constructor** is a special function with the same name as the struct. It runs when you create one.

* Line 31: `Vec3()`: no inputs, so we get (0, 0, 0). Example: `Vec3 v;`
* Line 32: `Vec3(double v)`: one number, copied into all three. Example: `Vec3(0.5)` = (0.5, 0.5, 0.5),
  middle grey.
  * `: x(v), y(v), z(v)` is the **initializer list**: it sets each member before the body `{}` runs.
* Line 33: `Vec3(double a, double b, double c)`: three numbers. Example: `Vec3(1, 2, 3)` gives x = 1, y = 2, z = 3.

### Lines 35–37: access by number

```cpp
    // Access by index: v[0] == v.x, v[1] == v.y, v[2] == v.z
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    double& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
```

* This lets us write `v[0]`, `v[1]`, `v[2]` instead of `v.x`, `v.y`, `v.z`. It's useful in loops like
  "for each axis 0, 1, 2" (used by bounding boxes in chapter 23).
* `operator[]` is how C++ lets you define what `[ ]` means for your own type.
* Line 36 is the **reading** version: `const` at the end means it doesn't change the vector, and it returns a copy
  of the number.
* Line 37 is the **writing** version: it returns `double&`, a **reference** to the real member, so
  `v[1] = 5;` actually changes `v.y`.

### Line 39: minus sign

```cpp
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
```

Defines `-v`: a new vector pointing the opposite way. `-(1, 2, 3)` = (−1, −2, −3).

### Lines 41–45: "change myself" operators

```cpp
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(double t) { x *= t; y *= t; z *= t; return *this; }
    Vec3& operator*=(const Vec3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
    Vec3& operator/=(double t) { return *this *= 1.0 / t; }
```

These change the vector itself, like `+=` for normal numbers:

| Line | Example | Effect |
|------|---------|--------|
| 41 | `a += b` | add b to a, part by part |
| 42 | `a -= b` | subtract b from a |
| 43 | `a *= 2` | multiply every part by 2 |
| 44 | `a *= b` | multiply part by part (used to **tint** colors) |
| 45 | `a /= 2` | divide every part by 2 (done as "multiply by 1/2") |

* `const Vec3& v`: the input is passed **by reference** (no copy, fast) and `const` (we promise not to change it).
* `return *this;`: `this` is the address of the vector itself; `*this` is the vector. Returning it allows chains
  like `a += b += c` (rarely used, but standard).

### Lines 47–48: length

```cpp
    double length_squared() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(length_squared()); }
```

* Line 47: x² + y² + z². That's the squared length. It's fast, with no square root. Often enough for comparisons.
* Line 48: the real length = the square root of that (Pythagoras in 3D). Example: (3, 4, 0) → √25 = **5**.

### Lines 50–54: near_zero

```cpp
    // True if the vector is very close to zero in every direction.
    bool near_zero() const {
        const double s = 1e-8;
        return std::fabs(x) < s && std::fabs(y) < s && std::fabs(z) < s;
    }
```

* Returns `true` if all three parts are tinier than 0.00000001.
* `std::fabs` = absolute value (makes negative numbers positive).
* `&&` = "and": all three must be true.
* Useful to catch "this direction is basically zero" before dividing by its length.

### Lines 56–57: biggest and smallest part

```cpp
    double max_component() const { return x > y ? (x > z ? x : z) : (y > z ? y : z); }
    double min_component() const { return x < y ? (x < z ? x : z) : (y < z ? y : z); }
```

* Line 56: the largest of x, y, z. Example: (0.2, 0.9, 0.5) → 0.9.
  Read it: if x > y, then the answer is the bigger of x and z; otherwise the bigger of y and z.
* Line 57: the smallest, the same way.

### Line 58

```cpp
};
```

End of the struct. A struct definition ends with `};` (with a semicolon).

---

## Block E — Aliases (lines 60–62)

```cpp
// Aliases: same type, different meaning. Makes code easier to read.
using Point3 = Vec3;
using Color  = Vec3;
```

* `using A = B;` creates another name for a type.
* `Point3` and `Color` are exactly `Vec3`, but the name tells the reader what we mean:
  `Point3 p` is a position, `Color c` is a color.

---

## Block F — Printing (lines 64–66)

```cpp
inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}
```

* This teaches `std::cout << v` how to print a vector.
* `std::ostream& out`: the output stream (for example the screen).
* It writes `(`, x, `, `, y, `, `, z, `)`. So `Vec3(1,2,3)` prints as `(1, 2, 3)`.
* It returns `out` so that you can continue: `std::cout << a << " and " << b;`.

---

## Block G — Arithmetic operators (lines 68–74)

```cpp
inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator*(double t, const Vec3& v) { return Vec3(t * v.x, t * v.y, t * v.z); }
inline Vec3 operator*(const Vec3& v, double t) { return t * v; }
inline Vec3 operator/(const Vec3& v, double t) { return (1.0 / t) * v; }
inline Vec3 operator/(const Vec3& a, const Vec3& b) { return Vec3(a.x / b.x, a.y / b.y, a.z / b.z); }
```

These make vector math look like normal math. Each one builds and returns a **new** vector:

| Line | You write | Result | Example |
|------|-----------|--------|---------|
| 68 | `a + b` | add part by part | (1,2,3) + (4,5,6) = (5,7,9) |
| 69 | `a - b` | subtract part by part | (4,5,6) − (1,2,3) = (3,3,3) |
| 70 | `a * b` | multiply part by part | red light (1,0,0) × grey paint (0.5,0.5,0.5) = (0.5,0,0) |
| 71 | `2.0 * v` | scale | 2 × (1,2,3) = (2,4,6) |
| 72 | `v * 2.0` | same, other order (it just calls line 71) | |
| 73 | `v / 2.0` | divide every part | (2,4,6) / 2 = (1,2,3) |
| 74 | `a / b` | divide part by part | |

Note: these are written **outside** the struct because they take two inputs (left and right side).

---

## Block H — Vector math (lines 76–105)

### Line 76: dot product

```cpp
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
```

* Multiply the matching parts and add them: one single number.
* Meaning (for unit vectors): how much the two directions **agree**. 1 = same direction, 0 = at right angles,
  −1 = opposite. See chapter 12.
* Example: dot((1,2,3), (4,5,6)) = 4 + 10 + 18 = **32**.

### Lines 78–82: cross product

```cpp
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}
```

* Gives a new vector that is **at right angles to both** a and b.
* Each line computes one part (x, y, z) with the standard formula. The statement is split over three lines
  only for readability.
* Example: cross((1,0,0), (0,1,0)) = (0, 0, 1): x-axis "cross" y-axis = z-axis.
* Used for surface normals and for the camera's right/up directions.

### Line 84: unit vector

```cpp
inline Vec3 unit_vector(const Vec3& v) { return v / v.length(); }
```

Divide by the length, so the result has length exactly 1: a pure **direction**.
Example: (3, 4, 0) → (0.6, 0.8, 0).

### Lines 86–91: component-wise min and max

```cpp
inline Vec3 vmin(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z));
}
inline Vec3 vmax(const Vec3& a, const Vec3& b) {
    return Vec3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z));
}
```

* `vmin` takes the smaller value **for each part separately**. vmin((1,5,3), (4,2,6)) = (1, 2, 3).
* `vmax` takes the larger value for each part: (4, 5, 6).
* Used to find the corners of boxes around objects.

### Line 92: lerp for vectors

```cpp
inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }
```

Same as `lerpd` (line 22) but for whole vectors. Very useful for **blending colors**:
`lerp(red, blue, 0.5)` = purple.

### Line 93: absolute value

```cpp
inline Vec3 vabs(const Vec3& v) { return Vec3(std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)); }
```

Makes every part positive. (−1, 2, −3) → (1, 2, 3). Used by the box distance function in chapter 37.

### Lines 95–96: reflect

```cpp
// Mirror reflection of v around the normal n (n must be unit length).
inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - 2.0 * dot(v, n) * n; }
```

* Bounces the direction `v` off a surface whose normal (the "straight out" direction) is `n`, like a ball off a wall.
* `dot(v, n) * n` is the part of v going straight into the surface. Subtracting it **twice** turns it around.
* Example: v = (1, −1, 0) going down-right, floor normal n = (0, 1, 0) → dot = −1 →
  v − 2 × (−1) × (0,1,0) = (1, −1, 0) + (0, 2, 0) = **(1, 1, 0)**: now going up-right.

### Lines 98–105: refract (bending through glass)

```cpp
// Snell's law refraction. uv and n must be unit length.
// etai_over_etat = (index of refraction we come from) / (index we go into)
inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}
```

This computes the new direction of light when it enters glass or water (explained fully in chapter 19).

* Lines 98–99: comments. `etai_over_etat` is the ratio of the two materials' "index of refraction",
  e.g. 1.0 / 1.5 going from air into glass.
* Line 101: `cos_theta` = cosine of the angle between the incoming ray and the normal. `-uv` flips the ray so
  it points away from the surface, like the normal. `std::fmin(..., 1.0)` guards against tiny rounding errors
  giving 1.0000001.
* Line 102: `r_out_perp` = the **sideways** part of the new direction. Snell's law says it is scaled by the ratio.
* Line 103: `r_out_parallel` = the part **along the normal**. Its size is chosen so the total length is 1
  (Pythagoras: `sqrt(1 − sideways²)`), and it points into the surface (minus sign). `std::fabs` guards against a
  tiny negative number from rounding.
* Line 104: the new direction = sideways part + along-normal part.

---

## Block I — Luminance (lines 107–108)

```cpp
// Brightness of a linear color as the human eye perceives it (Rec.709 weights).
inline double luminance(const Color& c) { return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z; }
```

* How bright a color **looks** to a human.
* Our eyes are very sensitive to green (71.5%), less to red (21%), and hardly to blue (7%).
* Example: pure green (0,1,0) → 0.72; pure blue (0,0,1) → only 0.07.

---

## Block J — End of namespace (line 110)

```cpp
} // namespace pixel
```

Closes the `pixel` family started on line 13. The comment just reminds us what this `}` closes.

---

## Check your understanding

1. What is `Vec3(2)`? *(2, 2, 2)*
2. What is `(1,2,3) * (2,2,2)` with line 70? *(2, 4, 6)*
3. Why does `length_squared` exist when we have `length`? *(It's faster: no square root. It's enough for
   comparing distances.)*
4. `dot(unit_vector(a), unit_vector(b))` is 0. What does that tell you? *(The directions are at right angles.)*
5. Why is `Color` the same type as `Point3`? *(All are three numbers with the same math; the different names
   only help humans read the code.)*
