# Line by line: `include/pixel/sky.h`

[← Line-by-line index](README.md) · [Chapter 33 (the theory)](../33-sky-and-environment.md)

**What this file does, in one sentence:** it provides **backgrounds** (what a ray sees when it hits nothing): a single
color, a simple gradient, a sunset-style sky, a 360° image, plus a helper that creates a **sun** you can light scenes
with.

The background also **lights** the scene: rays that bounce off objects and escape bring back the sky's color.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–18 | |
| B. `Background` type | 20 | "a function from a ray to a color" |
| C. `solid_background` | 22–25 | one color everywhere |
| D. `gradient_sky` | 27–35 | white at the horizon, blue above |
| E. `SkySettings` | 37–49 | all knobs of the nicer sky |
| F. `physical_sky` | 51–76 | zenith, horizon, ground, sun glow |
| G. `make_sun` | 78–85 | a far-away glowing sphere |
| H. `environment_map` | 87–98 | a 360° picture as the sky |
| I. End | 100 | |

---

## Block A — Comments, includes (lines 1–18)

Comments; `#pragma once`; `<cmath>`, `<functional>`, `<memory>`; our `vec3.h`, `ray.h`, `image.h` (for 360° images),
`material.h` (`DiffuseLight` for the sun) and `sphere.h`; `namespace pixel`.

---

## Block B — `Background` type (line 20)

```cpp
using Background = std::function<Color(const Ray&)>;
```

A new name for "any function that takes a Ray and returns a Color". The camera stores one and calls it for rays that
miss everything. The functions below **return** such functions (lambdas).

---

## Block C — `solid_background` (lines 22–25)

```cpp
inline Background solid_background(const Color& c) {
    return [c](const Ray&) { return c; };
}
```

Returns a lambda that ignores the ray and always gives color `c`. `[c]` = the lambda keeps its own copy of `c`.
`solid_background(Color(0,0,0))` = a completely dark world (only lamps give light).

---

## Block D — `gradient_sky` (lines 27–35)

```cpp
inline Background gradient_sky(const Color& horizon = Color(1.0, 1.0, 1.0),
                               const Color& zenith = Color(0.5, 0.7, 1.0)) {
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double a = 0.5 * (d.y + 1.0);
        return (1.0 - a) * horizon + a * zenith;
    };
}
```

The chapter 13 sky, with the two colors as options. `[=]` = the lambda copies `horizon` and `zenith`. Line 32: 0 looking
down, 1 looking up; line 33: blend.

---

## Block E — `SkySettings` (lines 37–49)

A bundle of settings for the nicer sky (each has a default):

| Line | Setting | Meaning |
|------|---------|---------|
| 39 | `sun_direction` | direction **towards** the sun |
| 40 | `zenith` | the color straight up |
| 41 | `horizon` | the color at the horizon |
| 42 | `ground` | the color below the horizon |
| 43 | `sun_glow` | the color of the halo around the sun |
| 44 | `glow_strength` | how strong the halo is |
| 45 | `intensity` | multiply everything (overall brightness) |
| 46 | `draw_sun_disk` | draw the sun itself in the sky? (usually no: use `make_sun`) |
| 47 | `sun_radiance` | brightness of that disk |
| 48 | `sun_angular_radius_deg` | size of that disk |

---

## Block F — `physical_sky` (lines 51–76)

```cpp
inline Background physical_sky(const SkySettings& s) {
    return [s](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        Vec3 sun = unit_vector(s.sun_direction);
        double cos_sun = dot(d, sun);
        Color c;
```

* Line 52: the returned lambda keeps a copy of the settings.
* Line 53: the ray direction, length 1.
* Line 54: the sun direction, length 1.
* Line 55: how close the ray is to the sun's direction (1 = looking straight at it).

```cpp
        if (d.y < 0.0) {
            double t = smoothstep(0.0, 0.08, -d.y);
            c = lerp(s.horizon, s.ground, t);
        } else {
            double t = std::pow(d.y, 0.45);
            c = lerp(s.horizon, s.zenith, t);
        }
```

* Lines 57–60: looking **down**: fade quickly from the horizon color to the ground color (within a small angle).
* Lines 61–64: looking **up**: from horizon to zenith. `pow(d.y, 0.45)` rises quickly, so the pale horizon band is thin
  and most of the sky has the zenith color, like a real sky.

```cpp
        double g = std::pow(std::fmax(0.0, cos_sun), 8.0) * s.glow_strength;
        double g2 = std::pow(std::fmax(0.0, cos_sun), 64.0) * s.glow_strength * 2.0;
        c += s.sun_glow * (g + g2);
```

* Line 67: a **wide** soft glow around the sun (power 8 falls off slowly).
* Line 68: a **tight**, stronger glow (power 64 falls off fast).
* Line 69: add both, in the glow color.

```cpp
        if (s.draw_sun_disk) {
            double cos_r = std::cos(degrees_to_radians(s.sun_angular_radius_deg));
            if (cos_sun > cos_r) c += s.sun_radiance;
        }
        return c * s.intensity;
    };
}
```

* Lines 70–73: optionally, if the ray is within the sun's angular radius, add the bright sun disk.
* Line 74: scale by the overall intensity.

---

## Block G — `make_sun` (lines 78–85)

```cpp
inline std::shared_ptr<Sphere> make_sun(const Vec3& direction_to_sun, double angular_radius_deg,
                                        const Color& radiance, double distance = 10000.0) {
    double radius = distance * std::tan(degrees_to_radians(angular_radius_deg));
    Point3 center = unit_vector(direction_to_sun) * distance;
    return std::make_shared<Sphere>(center, radius, std::make_shared<DiffuseLight>(radiance));
}
```

The real sun is tiny in the sky but extremely bright. We model it as a **glowing sphere very far away**:

* Line 82: how big it must be to cover `angular_radius_deg` degrees when seen from 10,000 units away
  (`radius = distance × tan(angle)`).
* Line 83: its position: 10,000 units in the sun's direction.
* Line 84: a sphere with a `DiffuseLight` material (`radiance` = its brightness, usually thousands).

Add it to both the world and the lights list, so the renderer can aim rays at it (chapters 31, 33).

---

## Block H — `environment_map` (lines 87–98)

```cpp
inline Background environment_map(std::shared_ptr<Image> env, double intensity = 1.0, double rotation_deg = 0.0) {
    double rot = degrees_to_radians(rotation_deg);
    return [=](const Ray& r) {
        Vec3 d = unit_vector(r.direction());
        double phi = std::atan2(d.z, d.x) + rot;
        double theta = std::acos(clampd(d.y, -1.0, 1.0));
        double u = phi / (2 * pi) + 0.5;
        double v = 1.0 - theta / pi;
        return env->sample_bilinear(u, v) * intensity;
    };
}
```

A 360° photo (an "equirectangular" image, like a world map of the whole surroundings) as the sky:

* Line 89: the optional rotation, in radians.
* Line 92: `phi` = the angle **around** (left/right), plus the rotation.
* Line 93: `theta` = the angle **down from straight up** (0 = up, π = down).
* Line 94: `u` = phi turned into 0–1 across the image.
* Line 95: `v` = 1 at the top of the image (straight up), 0 at the bottom.
* Line 96: look up the color in the image, times the intensity.

---

## Block I — End (line 100)

`} // namespace pixel`

---

## Check your understanding

1. What does `solid_background(Color(0,0,0))` mean for lighting? *(No light from the sky: only lamps light the scene.)*
2. Why is the sun a sphere and not just a bright spot in the sky? *(So the renderer can aim rays at it: clean
   shadows, less noise.)*
3. How big is a sun sphere 10,000 units away with an angular radius of 1°? *(10,000 × tan(1°) ≈ 175 units.)*
