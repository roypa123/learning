# Line by line: `ch12_vectors.cpp`

[← Line-by-line index](README.md) · [Chapter 12 (the theory)](../12-vectors.md) · [vec3.h explained](vec3.md)

**What the whole program does, in one sentence:** it prints vector calculations you can check by hand, then uses
vectors to shade a red ball so it looks 3D, even though it's drawn in a flat 2D image.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. Vector arithmetic | 13–29 | print results of +, −, dot, cross, length, reflect |
| C. Ball setup | 31–35 | image, light direction, view direction |
| D. Per pixel: position and background | 36–45 | is this pixel on the ball? |
| E. Per pixel: the 3D normal | 46–48 | which way the ball's surface faces here |
| F. Per pixel: lighting | 50–55 | diffuse + highlight |
| G. Save and end | 56–60 | |

---

## Block A — Comments, includes (lines 1–11)

Comments; `<cmath>` (sqrt, pow, fmax); `<cstdio>`; our library (which includes `<iostream>` for `std::cout` through
`vec3.h`); `using namespace pixel`.

---

## Block B — Vector arithmetic (lines 13–29)

```cpp
int main() {
    Vec3 a(1, 2, 3);
    Vec3 b(4, 5, 6);
```

Lines 15–16: two vectors to play with.

```cpp
    std::cout << "a           = " << a << "\n";
    std::cout << "b           = " << b << "\n";
```

Lines 17–18: print them. `std::cout <<` prints to the terminal; printing a Vec3 works because `vec3.h` teaches
`<<` how (it prints `(1, 2, 3)`).

```cpp
    std::cout << "a + b       = " << a + b << "   (expected (5, 7, 9))\n";
    std::cout << "b - a       = " << b - a << "   (expected (3, 3, 3))\n";
    std::cout << "2 * a       = " << 2.0 * a << "   (expected (2, 4, 6))\n";
    std::cout << "dot(a, b)   = " << dot(a, b) << "   (expected 32)\n";
    std::cout << "cross(a, b) = " << cross(a, b) << "   (expected (-3, 6, -3))\n";
    std::cout << "|a|         = " << a.length() << "   (expected 3.74166)\n";
    std::cout << "unit(a)     = " << unit_vector(a) << "   (length 1)\n";
```

Each line computes something and prints it next to the expected answer, so you can check the code (and your own
understanding) with pencil and paper:

| Line | Calculation | Working |
|------|-------------|---------|
| 19 | a + b | (1+4, 2+5, 3+6) = (5, 7, 9) |
| 20 | b − a | (4−1, 5−2, 6−3) = (3, 3, 3) |
| 21 | 2 × a | (2, 4, 6) |
| 22 | dot(a, b) | 1×4 + 2×5 + 3×6 = 4 + 10 + 18 = 32 |
| 23 | cross(a, b) | (2×6 − 3×5, 3×4 − 1×6, 1×5 − 2×4) = (−3, 6, −3) |
| 24 | length of a | √(1 + 4 + 9) = √14 ≈ 3.74166 |
| 25 | unit vector of a | a / 3.74166 ≈ (0.267, 0.535, 0.802) |

```cpp
    Vec3 x(1, 0, 0), y(0, 1, 0);
    std::cout << "cross(x, y) = " << cross(x, y) << "   (expected z = (0, 0, 1))\n";
```

* Line 26: the x axis and y axis directions. (Here `x` and `y` are variable names for vectors.)
* Line 27: x-axis "cross" y-axis gives the z-axis: the right-hand rule.

```cpp
    Vec3 down_right(1, -1, 0);
    std::cout << "reflect((1,-1,0), up) = " << reflect(down_right, y) << "   (expected (1, 1, 0))\n";
```

* Line 28: a direction going right and down.
* Line 29: bounce it off a floor whose normal points up (`y`). Result: right and up, (1, 1, 0).

---

## Block C — Ball setup (lines 31–35)

```cpp
    const int S = 400;
    Image img(S, S);
    Vec3 light_dir = unit_vector(Vec3(-1, 1, 1));     // light comes from upper-left-front
    Vec3 view_dir(0, 0, 1);                           // we look along -z, so "towards us" is +z
```

* Lines 32–33: a 400 × 400 image.
* Line 34: the **direction towards the light**: left (−x), up (+y), towards us (+z). Normalized to length 1, because
  the lighting math uses the dot product as a cosine.
* Line 35: the direction **towards the viewer**: straight out of the screen (+z).

---

## Block D — Per pixel: position and background (lines 36–45)

```cpp
    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {
            double sx = (px + 0.5) / S * 2 - 1;
            double sy = 1 - (py + 0.5) / S * 2;
```

* Lines 36–37: every pixel (px, py).
* Line 39: turn the pixel column into `sx` from −1 (left edge) to +1 (right edge): pixel center (px + 0.5) ÷ S gives
  0–1, × 2 gives 0–2, − 1 gives −1..1.
* Line 40: the same for rows, but **flipped** so that `sy` = +1 at the top and −1 at the bottom (in math and 3D,
  y points up).

```cpp
            double r2 = sx * sx + sy * sy;
            if (r2 > 0.8 * 0.8) {                         // background
                img.at(px, py) = lerp(hex_color(0x1E293B), hex_color(0x0F172A), (py + 0.5) / S);
                continue;
            }
```

* Line 41: the squared distance from the center of the image.
* Line 42: the ball has radius 0.8. If we're outside it...
* Line 43: ...paint the background: a dark slate gradient from top to bottom.
* Line 44: `continue` = skip the rest for this pixel.

---

## Block E — Per pixel: the 3D normal (lines 46–48)

```cpp
            double sz = std::sqrt(0.8 * 0.8 - r2);
            Vec3 normal = unit_vector(Vec3(sx, sy, sz));
```

This is the trick that makes it look 3D.

* Line 47: imagine a real ball of radius 0.8 centered at the origin. The point on its **front** surface above
  (sx, sy) has z = √(0.8² − sx² − sy²) (from x² + y² + z² = r²). In the middle z is large (the ball bulges toward
  us); at the rim z is 0.
* Line 48: for a ball centered at the origin, the surface point itself **is** the direction the surface faces: the
  **normal**. Make it length 1.

```
 side view:     viewer →   |      the surface point above (sx, sy)
                           |   ___
                           |  ╱ ↗ normal (sx, sy, sz)
                           | │  ●  │
                           |  ╲___╱
```

---

## Block F — Per pixel: lighting (lines 50–55)

```cpp
            double diffuse = std::fmax(0.0, dot(normal, light_dir));                 // Lambert
```

Line 50: **diffuse** (matte) light. The dot product = cosine of the angle between the surface direction and the
light direction: 1 when facing the light, 0 when sideways, negative when facing away. `fmax(0, ...)` turns negatives
into 0 (no negative light). This is **Lambert's law**.

```cpp
            Vec3 reflected = reflect(-light_dir, normal);
            double specular = std::pow(std::fmax(0.0, dot(reflected, view_dir)), 40); // Phong
```

* Line 51: the light's path: it arrives going **from** the light (`-light_dir`) and bounces off the surface.
* Line 52: **specular** highlight (the shiny spot). If the bounced light goes toward the viewer (dot close to 1), we
  see a highlight. Raising to the power 40 makes the highlight **small and sharp**: 0.99⁴⁰ = 0.67, but 0.9⁴⁰ = 0.015.
  This is the **Phong** model.

```cpp
            Color base = hex_color(0xE11D48);
            Color c = base * (0.08 + 0.92 * diffuse) + Color(1, 1, 1) * 0.6 * specular;
            img.at(px, py) = c;
```

* Line 53: the ball's red color.
* Line 54: the final color:
  * `base × (0.08 + 0.92 × diffuse)`: red, lit by the light. The 0.08 is a little "ambient" light so the dark side
    isn't completely black.
  * `+ white × 0.6 × specular`: add the white highlight on top.
* Line 55: store it.

---

## Block G — Save and end (lines 56–60)

Close the loops, save `images/ch12_fake_sphere.png`, `return 0;`.

---

## Check your understanding

1. Compute cross((0,1,0), (1,0,0)) by hand. *((0, 0, −1): swapping the order flips the result.)*
2. Why is `sy` flipped on line 40? *(Image rows go down; 3D y goes up.)*
3. What is `diffuse` for a point facing directly away from the light? *(0, thanks to `fmax`.)*
4. What happens to the highlight if you change 40 to 5? *(It gets much bigger and softer: a less shiny look.)*
