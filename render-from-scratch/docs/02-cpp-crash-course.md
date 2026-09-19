# Chapter 2 — A C++ crash course for this book

[← Setting up](01-setup.md) · [Contents](README.md) · [Next: Your first image →](03-first-image.md)

---

## Goal

Learn **exactly** the C++ that this book uses, no more, with graphics-flavored examples. If you
already know C++, skim the headings and skip ahead. If you're new, read this chapter slowly and
type in the examples: save them as `chapters/test.cpp` and run `run test`.

We'll cover:

1. The shape of a program
2. Variables and types
3. Math and the traps of integer division
4. Decisions and loops
5. Functions
6. References and `const`
7. Arrays and `std::vector`
8. `struct`: bundling data together
9. Operator overloading (making `a + b` work for colors)
10. Namespaces and header files
11. Classes, inheritance and `virtual`
12. Pointers and `std::shared_ptr`
13. Lambdas and `std::function`
14. Strings, printing and files
15. Threads (a first look)

---

## 1. The shape of a program

```cpp
#include <cstdio>          // lets us use std::printf

int main() {               // every program starts here
    std::printf("Hello, pixels!\n");
    return 0;              // 0 means "everything went fine"
}
```

* `#include <...>` pulls in code from the **standard library** (it comes with the compiler).
  `#include "..."` (with quotes) pulls in **our own** files.
* `main` is the **entry point**: the operating system calls it when the program starts.
* Statements end with `;`. Blocks of code are wrapped in `{ }`.
* `//` starts a comment that runs to the end of the line. `/* ... */` can span several lines.
* `\n` inside a string means "new line".

---

## 2. Variables and types

A **variable** is a named box that holds a value. In C++ every box has a **type** that says what
it can hold.

```cpp
int width = 400;             // whole numbers: ..., -2, -1, 0, 1, 2, ...
double brightness = 0.75;    // real numbers with a fraction (64-bit floating point)
bool is_inside = true;       // true or false
char letter = 'P';           // a single character
uint8_t red = 255;           // an unsigned 8-bit number: 0..255 (exactly one byte!)
```

| Type | Holds | Used in this book for |
|------|-------|-----------------------|
| `int` | whole numbers, about ±2 billion | pixel coordinates, counters |
| `double` | real numbers, ~15 significant digits | all math: positions, colors, light |
| `bool` | `true` / `false` | yes/no answers ("did the ray hit?") |
| `uint8_t` | 0–255 | bytes in image files |
| `uint32_t` | 0 to ~4 billion, 32 bits | checksums, random numbers |
| `uint64_t` | 0 to ~1.8×10¹⁹, 64 bits | random number generator state |
| `size_t` | big unsigned, used for sizes | positions in large arrays |

(`uint8_t` and friends come from `<cstdint>`.)

`auto` lets the compiler figure out the type from the value:

```cpp
auto x = 3.5;        // double
auto n = 10;         // int
```

`const` means "this never changes after it's created":

```cpp
const int image_width = 800;
```

---

## 3. Math, and the traps of integer division

```cpp
double a = 7.0 / 2.0;   // 3.5
int    b = 7 / 2;       // 3    <-- integer division throws away the fraction!
double c = 7 / 2;       // 3.0  <-- STILL 3: the division happened with ints, then got converted
double d = 7.0 / 2;     // 3.5  (one side is a double, so the whole thing is done in doubles)
int    r = 7 % 2;       // 1    (remainder, "modulo")
```

This is **the most common graphics bug** for beginners:

```cpp
int x = 100, width = 400;
double u = x / width;           // WRONG: 100/400 in ints = 0, so u is 0.0
double u = double(x) / width;   // RIGHT: 0.25
```

Converting between types is called **casting**:

```cpp
double v = 0.7;
int byte = (int)(255.999 * v);  // (int) cuts off the fraction: 178
```

Math functions live in `<cmath>`:

```cpp
std::sqrt(16.0)     // 4        square root
std::pow(2.0, 10)   // 1024     power
std::sin(x), std::cos(x), std::tan(x)   // angles in RADIANS
std::atan2(y, x)    // angle of the point (x, y)
std::fabs(-3.2)     // 3.2      absolute value
std::floor(2.7)     // 2.0      round down
std::fmin(a, b), std::fmax(a, b)
std::exp(x), std::log(x)
```

> **Radians**: a full circle is 2π radians = 360°. So 90° = π/2 ≈ 1.5708. We define
> `pixel::pi` and `degrees_to_radians()` in `vec3.h`.

---

## 4. Decisions and loops

```cpp
if (distance < radius) {
    // inside the circle
} else if (distance < radius + 1) {
    // on the edge
} else {
    // outside
}
```

Comparison operators: `<  <=  >  >=  ==  !=`. Combine conditions with `&&` (and), `||` (or),
`!` (not).

The **ternary operator** picks one of two values:

```cpp
double shade = is_inside ? 1.0 : 0.0;
```

The **for loop** is what we use most, to visit every pixel:

```cpp
for (int y = 0; y < height; y++) {         // for each row
    for (int x = 0; x < width; x++) {      // for each column in that row
        // compute pixel (x, y)
    }
}
```

`for (A; B; C)` means: do A once; while B is true, run the body, then do C. `y++` means
"add 1 to y".

**while** loops repeat until a condition becomes false; `break` jumps out of a loop, `continue`
skips to the next round:

```cpp
while (true) {
    Vec3 p = random_point_in_cube();
    if (p.length_squared() < 1) break;   // found a point inside the sphere: stop
}
```

**Range-based for** visits every element of a container:

```cpp
for (const auto& object : objects) { ... }
```

---

## 5. Functions

A function is a named, reusable piece of code with **inputs** (parameters) and an **output**
(return value):

```cpp
double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double a = clamp01(1.7);   // 1.0
```

A function that returns nothing has return type `void`.

Parameters can have **default values**:

```cpp
void fill_circle_aa(double cx, double cy, double radius, Color c, double alpha = 1.0);
fill_circle_aa(10, 10, 5, red);        // alpha is 1.0
fill_circle_aa(10, 10, 5, red, 0.5);   // alpha is 0.5
```

Two functions can share a name if their parameters differ. This is called **overloading**:

```cpp
double linear_to_srgb(double x);          // one number
Color  linear_to_srgb(const Color& c);    // a whole color
```

`inline` in front of a function defined in a header file tells the compiler "this may appear in
several files, and that's fine". It is needed for header-only libraries like ours.

---

## 6. References and `const`

Normally, passing a value to a function **copies** it. For big things (an image with a million
pixels) copying is slow. A **reference** (`&`) passes the original instead:

```cpp
void brighten(Image& img) {          // & = work on the caller's image, not a copy
    for (auto& c : img.data) c *= 2;
}
```

If the function only needs to *read*, use a **const reference**. It's fast (no copy) and safe (no
changes allowed):

```cpp
double average_brightness(const Image& img);
```

You'll see `const Vec3&` everywhere in this book for exactly that reason.

A `const` after a member function's parameter list means "this function doesn't change the
object":

```cpp
double length() const { return std::sqrt(x*x + y*y + z*z); }
```

---

## 7. Arrays and `std::vector`

A fixed-size array:

```cpp
double weights[3] = {0.2126, 0.7152, 0.0722};
weights[0]   // 0.2126   (counting starts at 0!)
```

A `std::vector` is an array that can grow. It's the workhorse of this book:

```cpp
#include <vector>
std::vector<uint8_t> bytes;          // empty
bytes.push_back(137);                // add at the end
bytes.push_back('P');
bytes.size();                        // 2
bytes[0];                            // 137

std::vector<double> zeros(100, 0.0); // 100 elements, all 0.0
```

### Storing a 2D image in a 1D vector

An image is 2D but memory is 1D (a long row of boxes). We store the image **row by row**:

```
 image (width 4, height 3):          memory (a vector of 12 pixels):

  (0,0) (1,0) (2,0) (3,0)            index:  0     1     2     3     4     5    ...  11
  (0,1) (1,1) (2,1) (3,1)            pixel: (0,0) (1,0) (2,0) (3,0) (0,1) (1,1) ... (3,2)
  (0,2) (1,2) (2,2) (3,2)

  index = y * width + x
```

Remember this formula: **`index = y * width + x`**. It appears in almost every chapter.

---

## 8. `struct`: bundling data together

A **struct** groups related variables into one new type:

```cpp
struct Vec3 {
    double x = 0, y = 0, z = 0;       // members, with default values
};

Vec3 p;              // (0, 0, 0)
p.x = 1.5;           // use "." to reach a member
```

Structs can have **constructors** (how to build one) and **member functions**:

```cpp
struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3() {}                                            // default constructor
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}   // "member initializer list"

    double length() const { return std::sqrt(x*x + y*y + z*z); }
};

Vec3 v(3, 4, 0);
double len = v.length();   // 5
```

The part after the `:` (`x(a), y(b), z(c)`) initializes the members. It's the preferred way to set
members in a constructor.

`using` creates another name for a type:

```cpp
using Color = Vec3;   // a Color IS a Vec3, but the name tells the reader what it means
```

(In C++, `struct` and `class` are almost the same thing: members of a `struct` are public by default,
members of a `class` are private by default.)

---

## 9. Operator overloading

We want to write `a + b` for vectors and colors just like for numbers. C++ lets us define what `+`
means for our own types:

```cpp
Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
Vec3 operator*(double t, const Vec3& v) {
    return Vec3(t * v.x, t * v.y, t * v.z);
}

Color sky = 0.5 * Color(1, 1, 1) + 0.5 * Color(0.5, 0.7, 1.0);   // reads like math!
```

Operators that change the left side, like `+=` or `*=`, are written as member functions that
return `*this` (the object itself):

```cpp
Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
```

---

## 10. Namespaces and header files

A **namespace** is a family name for a group of functions and types, which prevents name clashes.
Everything in our library lives in `namespace pixel`:

```cpp
namespace pixel {
    struct Vec3 { ... };
}

pixel::Vec3 v;              // full name
using namespace pixel;      // or: "I'll use pixel names without the prefix"
Vec3 w;
```

Things from the C++ standard library live in `namespace std` (`std::vector`, `std::sqrt`, ...).
Some sub-parts of our library have their own inner namespace, for example `pixel::png`,
`pixel::post` and `pixel::sdf`.

A **header file** (`.h`) contains code to be `#include`d by other files. `#pragma once` at the top
makes sure it's only included once per program, even if several headers include it.

---

## 11. Classes, inheritance and `virtual`

This is the one "advanced" feature that the ray tracer really needs, so let's take it slowly.

We'll have many kinds of objects: spheres, rectangles, triangles, lists of objects. The renderer
shouldn't care which kind it has. It only needs to ask **"does this ray hit you?"**. We express that
with a **base class** that declares the question, and **derived classes** that answer it in their
own way:

```cpp
class Hittable {                                    // the base class: "anything a ray can hit"
public:
    virtual ~Hittable() = default;                  // (always add this to base classes)
    virtual bool hit(const Ray& r, Interval t, HitRecord& rec) const = 0;   // "= 0": must be provided
};

class Sphere : public Hittable {                    // a Sphere IS a Hittable
public:
    bool hit(const Ray& r, Interval t, HitRecord& rec) const override {
        // sphere math here
    }
};

class Quad : public Hittable {
public:
    bool hit(const Ray& r, Interval t, HitRecord& rec) const override {
        // rectangle math here
    }
};
```

* `virtual` means "the real function is chosen **at run time**, depending on the actual object".
* `= 0` makes it **pure virtual**: the base class doesn't implement it and every derived class must.
  You can't create a plain `Hittable`; it's an **interface**.
* `override` asks the compiler to check that we really are replacing a base function (it catches
  typos).

Now code can hold "some Hittable" without knowing what it is:

```cpp
const Hittable& thing = my_sphere;
thing.hit(ray, ...);     // calls Sphere::hit
```

We use exactly the same pattern for `Material` (matte, metal, glass...), `Texture` (solid, checker,
image...) and `PDF`.

`public:`, `private:` and `protected:` control who can use the members: everyone, only the class
itself, or the class and its derived classes.

---

## 12. Pointers and `std::shared_ptr`

A **pointer** holds the *address* of an object instead of the object itself:

```cpp
Sphere s(...);
Sphere* p = &s;       // & in front of a variable = "address of"
p->hit(...);          // -> = use a member through a pointer
```

A pointer can be `nullptr`, meaning "points to nothing".

Our scenes contain many objects of different types that share materials. Who is responsible for
deleting them when we're done? A **smart pointer** handles that automatically.
`std::shared_ptr` counts how many owners an object has and deletes it when the last owner is gone:

```cpp
#include <memory>

auto red = std::make_shared<Lambertian>(Color(0.8, 0.1, 0.1));   // create a material
auto ball = std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, red); // a sphere that uses it
world.add(ball);                                                  // the world shares ownership
```

* `std::make_shared<T>(args...)` creates a `T` with those constructor arguments and returns a
  `std::shared_ptr<T>`.
* A `shared_ptr<Sphere>` automatically converts to a `shared_ptr<Hittable>`, because a Sphere
  *is* a Hittable.
* `.get()` gives the raw pointer (we store raw pointers in hit records for speed).

You will write `std::make_shared<...>(...)` a lot. It's the "new object" command of this book.

---

## 13. Lambdas and `std::function`

A **lambda** is a small function written inline, right where you need it:

```cpp
auto square = [](double x) { return x * x; };
square(3.0);   // 9
```

The `[]` is the **capture list**: which outside variables the lambda may use.

```cpp
double scale = 2.0;
auto scaled = [scale](double x) { return x * scale; };   // copy 'scale' into the lambda
auto counter = [&](int i) { total += i; };               // & = use outside variables by reference
```

`std::function<Result(Args...)>` is a type that can hold **any** callable with that signature,
including lambdas. We use it for things you should be able to customize with a formula:

```cpp
using Background = std::function<Color(const Ray&)>;

Background night = [](const Ray&) { return Color(0.01, 0.01, 0.03); };
```

Our sky, procedural textures, height fields and distance functions all work this way.

---

## 14. Strings, printing and files

```cpp
#include <string>
std::string name = "images/out.png";
std::string full = "images/" + std::string("frame") + ".png";
```

Printing with `printf` (from `<cstdio>`):

```cpp
std::printf("%d rows, %.2f seconds, file %s\n", rows, secs, name.c_str());
//           ^ int      ^ double, 2 decimals   ^ C string
```

| Format | Prints |
|--------|--------|
| `%d` | int |
| `%lld` | long long |
| `%zu` | size_t |
| `%f`, `%.3f` | double (with 3 decimals) |
| `%s` | text (use `.c_str()` on a `std::string`) |
| `%3d%%` | int padded to width 3, then a literal `%` |

Or with streams (`<iostream>`):

```cpp
std::cout << "length = " << v.length() << "\n";
```

Writing a file (`<fstream>`):

```cpp
std::ofstream f("images/test.ppm", std::ios::binary);   // binary = write bytes exactly
f << "P6\n";                                           // text
f.write((const char*)bytes.data(), bytes.size());      // raw bytes
```

---

## 15. Threads (a first look)

Your processor has several **cores**, each of which can run code at the same time. A
`std::thread` runs a function on another core:

```cpp
#include <thread>
std::thread t([] { /* work */ });
t.join();     // wait until it finishes
```

In chapter 21 the camera splits the image into rows and lets every core render rows in parallel.
The only tricky part is making sure two threads never write to the same thing at the same time.
We use `std::atomic<int>` (a counter that's safe to update from several threads) and
`std::mutex` (a lock). Don't worry about this now; it's explained when we need it.

---

## Quick reference card

```cpp
#include "pixel/pixel.h"            // our whole library
using namespace pixel;

int main() {
    Image img(400, 300);                               // a 400x300 image, all black
    img.at(10, 20) = Color(1, 0, 0);                   // set a pixel (linear light)
    save_image("images/test.png", img);                // write it (PNG, BMP or PPM by extension)

    auto mat = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0, 0, -1), 0.5, mat));

    Camera cam;                                        // sensible defaults
    cam.image_width = 400;
    Image result = cam.render(world);
    save_image("images/result.png", result);
    return 0;
}
```

If most of this card makes sense (roughly), you're ready. Everything else will be explained as we
go.

---

## Try it yourself

1. Write a program that prints the numbers 1 to 10 and their squares.
2. Write a function `double lerp(double a, double b, double t)` that returns `a + (b - a) * t`.
   What does it return for `t = 0`, `t = 0.5`, `t = 1`?
3. Make a `std::vector<int>` with 12 elements, and print which `(x, y)` each index corresponds to
   in an image of width 4 (use `x = i % 4`, `y = i / 4`).
4. Explain in one sentence why `double u = 1 / 2;` gives `0`.

## Common problems

| Error message (roughly) | Meaning |
|-------------------------|---------|
| `expected ';' before ...` | You forgot a semicolon on the line *before* |
| `'Vec3' was not declared in this scope` | Missing `#include` or `using namespace pixel;` |
| `no matching function for call to ...` | Wrong number or type of arguments |
| `cannot convert 'X' to 'Y'` | Wrong type; maybe you need a cast or a different constructor |
| `undefined reference to ...` | A function was declared but never defined (or a library is missing) |
| A crash with no message | Often an index outside a vector (`img.at(x, y)` with x ≥ width) |

When the compiler prints many errors, **fix the first one first**. The rest are often caused by it.

---

Next: [Chapter 3 — Your first image →](03-first-image.md)
