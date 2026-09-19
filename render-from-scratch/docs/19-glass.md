# Chapter 19 — Glass and other dielectrics

[← Metal](18-metal.md) · [Contents](README.md) · [Next: Positionable camera →](20-positionable-camera.md)

---

## Goal

Make **transparent** materials (glass, water, diamond) that bend light. You'll learn:

* **refraction** and **Snell's law**,
* **total internal reflection**,
* **Fresnel reflection** (why glass reflects more at grazing angles) and **Schlick's approximation**,
* the "hollow glass sphere" trick,
* why a glass ball shows the world **upside down**.

Transparent materials that don't conduct electricity are called **dielectrics**, hence the class name
`Dielectric`.

---

## 1. Refraction: light bends

Light travels more slowly in glass than in air. When a ray crosses the boundary at an angle, it
**bends**. You've seen it: a straw in a glass of water looks broken.

Each material has an **index of refraction** (IOR), usually written η ("eta"): how many times slower
light travels in it than in vacuum.

| Material | η |
|----------|---|
| vacuum / air | 1.0 (air is 1.0003) |
| water | 1.33 |
| glass | 1.5 – 1.7 |
| diamond | 2.42 |

### 1.1 Snell's law

```
η · sin θ = η′ · sin θ′

                 normal
                   │
     incoming ╲ θ  │               air   (η = 1.0)
               ╲   │
   ─────────────╲──┼────────────  surface
                 ╲ │
                  ╲│ θ′            glass (η′ = 1.5)
                   ╲               bends TOWARDS the normal
                   │╲
```

Going from air into glass (1.0 → 1.5), the ray bends **toward** the normal (θ′ < θ). Going from glass
out to air, it bends **away**.

### 1.2 Computing the refracted direction

Split the refracted ray `R′` into a part perpendicular to the normal and a part parallel to it. With
unit vectors `R` (incoming) and `n` (normal, facing the incoming ray):

```
R′⊥ = (η/η′) · (R + cos θ · n)            where cos θ = dot(−R, n)
R′∥ = −sqrt(1 − |R′⊥|²) · n
R′  = R′⊥ + R′∥
```

The first line is Snell's law in vector form (the sideways part scales by η/η′); the second makes the
total length 1. That's `refract()` in `vec3.h`:

```cpp
inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    double cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}
```

### 1.3 Which way are we going?

Remember `front_face` from chapter 15: we know whether the ray is entering (front face) or leaving
(back face) the object. So the ratio is:

```cpp
double ri = rec.front_face ? (1.0 / ior) : ior;
```

---

## 2. Total internal reflection

From inside glass, at a steep enough angle, Snell's law has **no solution**: `sin θ′` would need to be
greater than 1. Then light can't get out at all and **reflects completely**. That's what makes diamonds
sparkle and optical fibers work.

```
cannot_refract = ri · sin θ > 1.0       (then: reflect instead)
```

You can see this looking up from underwater: beyond a certain angle, the surface looks like a mirror.

---

## 3. Fresnel: glass is also a mirror

Look at a window from straight on: you mostly see *through* it. Look at it at a grazing angle: it
becomes a **mirror**. The share of light reflected vs refracted depends on the angle. It's described by
the **Fresnel equations** (Augustin-Jean Fresnel, 1820s).

The exact equations are long. Christophe **Schlick**'s 1994 approximation is simple and accurate
enough for rendering:

```
R(θ) = R₀ + (1 − R₀) · (1 − cos θ)⁵          R₀ = ((1 − η)/(1 + η))²

   for glass (η = 1.5): R₀ = 0.04    ->  4% reflected head-on, rising to 100% at grazing angles
```

```
 reflectance
 1.0 ┤                                      ╭
     │                                     ╱
     │                                   ╱
 0.5 ┤                                ╭╯
     │                          __,--'
 0.04┤──────────────────────────                    head-on: 4%
     └──────────────────────────────────────┬───
     0°                                    90°  angle from normal
```

### 3.1 Choosing randomly between reflection and refraction

A ray can't split in two (that would double the work at every bounce). Instead, we **choose one at
random** with probability equal to the reflectance:

```cpp
if (cannot_refract || schlick(cos_theta, r0) > random_double())
    direction = reflect(unit_direction, rec.normal);
else
    direction = refract(unit_direction, rec.normal, ri);
```

Averaged over many samples, exactly R of the light is reflected and 1 − R refracted. This "pick one
path at random, in proportion" trick is used all over path tracing.

The attenuation is white (1, 1, 1) for clear glass: glass absorbs almost nothing. Our `Dielectric` takes
an optional **tint** for colored glass.

---

## 4. The hollow glass sphere

A soap bubble or a glass Christmas ornament is a thin shell of glass. Model it as a glass sphere with
a slightly smaller sphere of **air** inside. For the inner sphere, use the ratio `1/1.5`. From its
point of view, it's "air inside glass":

```cpp
world.add(std::make_shared<Sphere>(center, 0.5, glass));                                 // outer surface
world.add(std::make_shared<Sphere>(center, 0.4, std::make_shared<Dielectric>(1.0 / 1.5)));  // inner surface
```

The result looks like a thin glass bubble, and the view through it is barely distorted.

---

## 5. The program

**File: `chapters/ch19_glass.cpp`**

```cpp
// ch19_glass.cpp
// ------------------------------------------------------------
// Chapter 19: Dielectrics - glass, water and bubbles.
//   images/ch19_glass.png        - solid glass ball (left), matte (middle), gold (right)
//   images/ch19_hollow.png       - hollow glass ball: a bubble inside glass
//   images/ch19_ior.png          - index of refraction 1.0, 1.33, 1.5, 2.4
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

static Camera base_camera() {
    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    return cam;
}

int main() {
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);   // air inside glass
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 0.0);

    // ---------- 1. Solid glass ball --------------------------------------
    {
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
        world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
        world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
        world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));
        save_image("images/ch19_glass.png", base_camera().render(world));

        // ---------- 2. Add a bubble inside the glass: hollow sphere -------
        world.add(std::make_shared<Sphere>(Point3(-1.0, 0.0, -1.0), 0.4, bubble));
        save_image("images/ch19_hollow.png", base_camera().render(world));
    }

    // ---------- 3. Different materials, same shape ------------------------
    {
        auto checker = std::make_shared<CheckerTexture>(0.25, Color(0.9, 0.9, 0.9), Color(0.15, 0.15, 0.15));
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -100.5, -1), 100, std::make_shared<Lambertian>(checker)));
        double iors[4] = {1.0, 1.33, 1.5, 2.4};   // air, water, glass, diamond
        for (int k = 0; k < 4; k++)
            world.add(std::make_shared<Sphere>(Point3(-1.5 + k, 0, -1.6), 0.45,
                                               std::make_shared<Dielectric>(iors[k])));
        Camera cam = base_camera();
        cam.lookfrom = Point3(0, 0.3, 1);
        cam.lookat = Point3(0, 0, -1.6);
        cam.vfov = 60;
        save_image("images/ch19_ior.png", cam.render(world));
    }
    return 0;
}
```

```bat
run ch19_glass
```

---

## 6. What you should see

![Glass](../images/ch19_glass.png)

> **Image description:** The chapter 18 scene with the left ball now made of clear **solid glass**. It
> looks like a crystal ball: you see through it, but the world behind it is **upside down and flipped**, and
> compressed: the yellow ground appears at the *top* of the ball and the sky at the bottom. Around its
> rim there's a thin bright, mirror-like band (Fresnel reflection). On the ground below it there's a
> **bright spot** where the ball focuses light (a caustic), noisy for now. The middle ball is matte blue and
> the right ball is mirror gold.

![Hollow](../images/ch19_hollow.png)

> **Image description:** Same scene, but the glass ball is now **hollow**, like a soap bubble or a
> glass ornament. The world behind it is no longer upside down: the view is almost undistorted, with
> just a bit of bending near the rim, where a thin bright outline shows the glass shell.

![IOR](../images/ch19_ior.png)

> **Image description:** Four glass balls in a row on a black-and-white checkerboard ground, with
> indices of refraction 1.0, 1.33, 1.5 and 2.4 from left to right. **Left (1.0):** almost invisible,
> just a faint outline: light doesn't bend at all. **Second (water):** the checkerboard behind is
> magnified and a bit distorted. **Third (glass):** it's flipped upside down, like a crystal ball.
> **Right (diamond):** strongly distorted and shrunk, with a wide reflective dark ring around the edge
> from total internal reflection, and more sparkle.

### Why upside down?

A solid glass ball acts like a very strong lens. Rays from the top of the scene cross over inside the
ball and come out at the bottom, like in a camera or your eye. A hollow ball is a very weak lens, so
there's no flip.

---

## Try it yourself

1. Make green bottle glass: `Dielectric(1.5, Color(0.7, 0.95, 0.75))`.
2. Put a small red ball *inside* a big glass ball.
3. Look at the rim of the solid glass ball with more samples (1000 spp). The Fresnel reflections
   become clearer.
4. Make a "water surface": a huge glass sphere as ground, with a checker sphere below it. Try IOR 1.33.
5. Why does IOR 1.0 still show a faint outline? (Hint: Schlick with R₀ = 0 still reflects at grazing
   angles. Real air–air boundaries don't exist, so this is a small inaccuracy of the approximation.)

## Common problems

| Symptom | Cause |
|---------|-------|
| Glass ball is black | Refraction ratio upside down, or `sqrt` of a negative number (NaN): check total internal reflection |
| Glass looks like a mirror everywhere | Always choosing reflection: Schlick or `random_double` comparison reversed |
| Hollow sphere looks solid | Inner sphere IOR must be `1/1.5`, and its radius smaller than the outer one |
| Black ring inside glass | `max_depth` too small: rays bounce many times inside glass |

---

## Summary

* **Snell's law**: `η sin θ = η′ sin θ′`; the refracted direction splits into perpendicular and parallel parts.
* **Total internal reflection** happens when refraction is impossible.
* **Fresnel** (Schlick): reflectance rises from R₀ at head-on angles to 100% at grazing angles. Choose reflect vs refract
  randomly by that probability.
* A hollow sphere = glass sphere + slightly smaller sphere with IOR `1/η`.

Next: [Chapter 20 — A positionable camera and depth of field →](20-positionable-camera.md)
