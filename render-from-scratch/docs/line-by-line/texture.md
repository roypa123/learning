# Line by line: `include/pixel/texture.h`

[← Line-by-line index](README.md) · [Chapter 24 (the theory)](../24-textures.md)

**What this file does, in one sentence:** it defines **textures**: objects that answer "what color is the surface at
this exact spot?", so surfaces can have patterns, pictures and noise instead of one flat color.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–15 | |
| B. `Texture` base class | 17–22 | the one question |
| C. `SolidColor` | 24–31 | the same color everywhere |
| D. `CheckerTexture` | 33–51 | a 3D checkerboard |
| E. `UVCheckerTexture` | 53–65 | a checkerboard that follows the surface |
| F. `ImageTexture` | 67–82 | a picture wrapped on the surface |
| G. `NoiseTexture` | 84–104 | Perlin noise: smooth, turbulent, marble |
| H. `FunctionTexture` | 106–114 | any formula you write |
| I. End | 116 | |

---

## Block A — Comments, includes (lines 1–15)

Comments; `#pragma once`; `<cmath>`, `<memory>`, `<string>` (file names), `<functional>` (`std::function`); our
`vec3.h`, `image.h` (for image textures), `noise.h` (Perlin); `namespace pixel`.

---

## Block B — `Texture` base class (lines 17–22)

```cpp
class Texture {
public:
    virtual ~Texture() = default;
    virtual Color value(double u, double v, const Point3& p) const = 0;
};
```

Every texture answers one question (line 21): the color at a spot, given:

* `u`, `v` = the spot's 2D "address" on the surface (0–1 each, like map coordinates),
* `p` = the spot's 3D position.

Some textures use (u, v), some use p.

---

## Block C — `SolidColor` (lines 24–31)

```cpp
class SolidColor : public Texture {
public:
    SolidColor(const Color& albedo) : albedo(albedo) {}
    SolidColor(double r, double g, double b) : albedo(r, g, b) {}
    Color value(double, double, const Point3&) const override { return albedo; }
private:
    Color albedo;
};
```

* Lines 26–27: create from a Color or from three numbers.
* Line 28: always return the same color (the inputs are ignored, so they have no names).

---

## Block D — `CheckerTexture` (lines 33–51)

```cpp
    CheckerTexture(double scale, std::shared_ptr<Texture> even, std::shared_ptr<Texture> odd)
        : inv_scale(1.0 / scale), even(even), odd(odd) {}
    CheckerTexture(double scale, const Color& c1, const Color& c2)
        : CheckerTexture(scale, std::make_shared<SolidColor>(c1), std::make_shared<SolidColor>(c2)) {}
```

* Lines 36–37: `scale` = the size of each checker cube. We store `1 / scale` (multiplying is faster than dividing).
  `even` and `odd` are the two textures that alternate.
* Lines 38–39: the easy version with two colors; it wraps them in SolidColors and calls the first constructor.

```cpp
    Color value(double u, double v, const Point3& p) const override {
        int x = (int)std::floor(inv_scale * p.x);
        int y = (int)std::floor(inv_scale * p.y);
        int z = (int)std::floor(inv_scale * p.z);
        bool is_even = ((x + y + z) % 2) == 0;
        return is_even ? even->value(u, v, p) : odd->value(u, v, p);
    }
```

* Lines 42–44: which **cube** of space the point is in, along each axis (floor = round down).
* Line 45: if the three cube numbers add up to an even number → one color; odd → the other. Neighbors always differ.
* Line 46: ask the chosen texture for its color.

Because it uses the 3D point, it's like the object was carved from a checkered block.

---

## Block E — `UVCheckerTexture` (lines 53–65)

```cpp
    UVCheckerTexture(int squares_u, int squares_v, const Color& c1, const Color& c2)
        : nu(squares_u), nv(squares_v), c1(c1), c2(c2) {}
    Color value(double u, double v, const Point3&) const override {
        int iu = (int)std::floor(u * nu), iv = (int)std::floor(v * nv);
        return ((iu + iv) % 2 == 0) ? c1 : c2;
    }
```

The same idea in the surface's 2D coordinates: `nu × nv` squares over the whole surface. Line 59: which square (u, v)
falls in; line 60: alternate colors. The squares follow the surface, like a printed pattern.

---

## Block F — `ImageTexture` (lines 67–82)

```cpp
    ImageTexture(const Image& img) : image(img) {}
    ImageTexture(const std::string& ppm_filename) {
        if (!read_ppm(ppm_filename, image))
            std::printf("WARNING: could not load texture '%s'\n", ppm_filename.c_str());
    }
```

* Line 70: from an image already in memory (it's copied).
* Lines 72–75: from a PPM file. If loading fails, print a warning (the image stays empty).

```cpp
    Color value(double u, double v, const Point3&) const override {
        if (image.width == 0) return Color(1, 0, 1);
        return image.sample_bilinear(u, v);
    }
```

* Line 77: no image → **magenta** ("texture missing").
* Line 78: look up the color at (u, v), smoothly blended (see [image.md block D](image.md)).

---

## Block G — `NoiseTexture` (lines 84–104)

```cpp
    enum Style { Smooth, Turbulence, Marble };
    NoiseTexture(double scale, Style style = Marble, const Color& tint = Color(1, 1, 1))
        : scale(scale), style(style), tint(tint) {}
```

* Line 87: three looks to choose from.
* Lines 88–89: `scale` = how fine the pattern is (bigger = smaller details); style; a tint color.

```cpp
    Color value(double, double, const Point3& p) const override {
        switch (style) {
            case Smooth:     return tint * 0.5 * (1.0 + noise.noise(scale * p));
            case Turbulence: return tint * noise.turbulence(scale * p, 7);
            case Marble:
            default:         return tint * 0.5 * (1.0 + std::sin(scale * p.z + 10.0 * noise.turbulence(p, 7)));
        }
    }
```

* Line 93: **smooth**: Perlin noise is −1..1; `0.5 × (1 + noise)` turns it into 0..1 → soft blotches.
* Line 94: **turbulence**: layered absolute noise → dirty, smoky patterns.
* Line 96: **marble**: `sin(scale × z)` alone gives straight stripes; adding `10 × turbulence` to the inside of the
  sine bends the stripes into wavy veins. `0.5 × (1 + sin)` maps −1..1 to 0..1.

Lines 100–103: a Perlin noise generator and the settings.

---

## Block H — `FunctionTexture` (lines 106–114)

```cpp
class FunctionTexture : public Texture {
public:
    using Fn = std::function<Color(double, double, const Point3&)>;
    FunctionTexture(Fn f) : fn(f) {}
    Color value(double u, double v, const Point3& p) const override { return fn(u, v, p); }
private:
    Fn fn;
};
```

* Line 109: `Fn` = "any function that takes (u, v, p) and returns a Color" (a lambda, for example).
* Line 110: store it.
* Line 111: just call it.

Example: color a terrain by height:
`FunctionTexture([](double, double, const Point3& p) { return p.y > 2 ? white : green; })`.

---

## Block I — End (line 116)

`} // namespace pixel`

---

## Check your understanding

1. What's the difference between `CheckerTexture` and `UVCheckerTexture`? *(3D position vs surface coordinates.)*
2. What does a magenta object tell you? *(An image texture failed to load.)*
3. What makes marble wavy instead of striped? *(Turbulence added inside the sine.)*
