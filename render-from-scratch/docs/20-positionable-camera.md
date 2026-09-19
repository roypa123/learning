# Chapter 20 — A positionable camera and depth of field

[← Glass](19-glass.md) · [Contents](README.md) · [Next: Multithreading & first masterpiece →](21-multithreading-final-scene.md)

---

## Goal

Until now the camera sat at the origin looking down −z. Real cinematography needs to put the camera
**anywhere**, point it **anywhere**, choose a **lens** (wide angle or telephoto), and control **focus**.
You'll learn:

* **field of view** (`vfov`) and how it relates to zoom,
* `lookfrom`, `lookat`, `vup` and building the camera's own coordinate system with cross products,
* the **thin lens model**: aperture, focus distance, and **depth of field** (the blurry background of
  every movie).

---

## 1. Field of view

The **vertical field of view** (vfov) is the angle, from the bottom to the top of the image, that the
camera sees:

```
                 ╱│
               ╱  │  h = tan(vfov / 2)     (half the viewport height, at distance 1)
             ╱ θ/2│
   eye ●───────────┤  distance 1
             ╲ θ/2│
               ╲  │
                 ╲│
```

* **Wide angle** (vfov 90°+): sees a lot, exaggerates perspective (close things look huge). Good for
  interiors and dramatic shots.
* **Normal** (vfov ~40°): about what the human eye feels natural.
* **Telephoto** (vfov 10–20°): sees a narrow slice, flattens perspective (distant things look close to
  each other). Portraits and the "compressed" look of sports photography.

In photography, focal length in millimeters is used instead. For a full-frame camera: 24 mm ≈ vfov 53°,
50 mm ≈ 27°, 200 mm ≈ 7°.

---

## 2. Pointing the camera: `lookfrom`, `lookat`, `vup`

We describe the camera by:

* `lookfrom`: where the camera is,
* `lookat`: a point it looks at,
* `vup`: which way is roughly "up" for the camera (usually world up, (0, 1, 0)).

From these, we build three perpendicular unit vectors, the camera's own **u, v, w** axes:

```
w = unit_vector(lookfrom − lookat)      points BACKWARDS (the camera looks along −w)
u = unit_vector(cross(vup, w))          points to the camera's RIGHT
v = cross(w, u)                         the camera's true UP
```

```
                v (up)
                ▲
                │    ● lookat
                │   ╱
                │  ╱  (looking along −w)
    lookfrom ●──┼──────▶ u (right)
               ╱
              ╱
             w (backwards)
```

Why the cross products? `cross(vup, w)` is perpendicular to both "up" and "backwards", which is
"right". Then `cross(w, u)` gives an "up" that is exactly perpendicular to the viewing direction, even if
the camera is tilted up or down. That's how you "roll" correctly: tilt `vup` and the image rotates.

The viewport is then built exactly as in chapter 13, just using u, v, w instead of the x, y, z axes:

```cpp
Vec3 viewport_u = viewport_width * u;       // across the top edge
Vec3 viewport_v = viewport_height * -v;     // down the left edge
Point3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
```

---

## 3. Depth of field

### 3.1 What real lenses do

A pinhole camera (our camera so far) has everything in perfect focus. A real camera has a **lens with
an opening** (the *aperture*). Light from a point enters through the whole opening, and the lens bends
it back to a single point only if the object is at the **focus distance**. Objects nearer or farther
become small blurry discs: **bokeh**.

```
                       lens (aperture)
   object at focus  ●──────┬┬┬───────● sharp point on the sensor
   distance                │││
   object too near  ●──────┼┼┼──── ○ a blurry disc
                            ┴┴┴
```

A **larger aperture** (a smaller f-number, like f/1.4) gives **more blur**, the dreamy background of
portrait and film shots. A **tiny aperture** (f/16) keeps everything sharp.

### 3.2 The thin lens in a ray tracer

We simulate it with a trick:

1. Put the viewport **at the focus distance** (instead of distance 1). Points there are perfectly
   in focus.
2. Instead of starting every ray exactly at the camera center, start it at a **random point on a
   disk** (the lens) around the center.
3. Aim every ray at the same point on the viewport for that pixel.

```
          lens disk           focus plane (viewport)
            ·  ╲                  │
   center   ●───────────────────▶ ● pixel target: all rays meet HERE -> sharp
            ·  ╱                  │
                                  │
   an object nearer than the focus plane is crossed by rays at DIFFERENT
   points -> it looks blurry. Same for objects farther away.
```

The disk radius is set by `defocus_angle`: the angle of the cone from the pixel target back to the lens
edge:

```cpp
double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
defocus_disk_u = u * defocus_radius;
defocus_disk_v = v * defocus_radius;

Point3 defocus_disk_sample() const {
    Vec3 p = random_in_unit_disk();
    return center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
}
```

Many samples per pixel average over the lens, giving smooth blur.

### 3.3 Lens shift

The camera also has `shift_x`/`shift_y`, which slides the viewport sideways without rotating the camera.
Architectural photographers use this to keep vertical lines vertical. It's rarely needed, but it's one line of
code.

---

## 4. The program

**File: `chapters/ch20_camera.cpp`**

```cpp
// ch20_camera.cpp
// ------------------------------------------------------------
// Chapter 20: A positionable camera with depth of field.
//   images/ch20_fov_wide.png    - vfov 90 from the front
//   images/ch20_fov_tele.png    - vfov 20 from far away (telephoto)
//   images/ch20_defocus.png     - aperture open: blurry foreground/background
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    auto ground = std::make_shared<Lambertian>(Color(0.8, 0.8, 0.0));
    auto center = std::make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto glass  = std::make_shared<Dielectric>(1.50);
    auto bubble = std::make_shared<Dielectric>(1.00 / 1.50);
    auto gold   = std::make_shared<Metal>(Color(0.8, 0.6, 0.2), 1.0);

    HittableList world;
    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, glass));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.4, bubble));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, gold));

    Camera cam;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    // Wide angle, from above-left.
    cam.vfov = 90;
    cam.lookfrom = Point3(-2, 2, 1);
    cam.lookat = Point3(0, 0, -1);
    cam.vup = Vec3(0, 1, 0);
    save_image("images/ch20_fov_wide.png", cam.render(world));

    // Telephoto: narrow field of view from further away.
    cam.vfov = 20;
    save_image("images/ch20_fov_tele.png", cam.render(world));

    // Depth of field: a lens with an aperture.
    cam.defocus_angle = 10.0;
    cam.focus_dist = 3.4;       // distance from lookfrom to the center sphere
    save_image("images/ch20_defocus.png", cam.render(world));
    return 0;
}
```

(The camera code is in `camera.h`, already shown in chapter 17; look at `initialize()` and `get_ray()`.)

```bat
run ch20_camera
```

---

## 5. What you should see

![Wide](../images/ch20_fov_wide.png)

> **Image description:** The five-ball scene (hollow glass, matte blue, rough gold, on yellow ground)
> seen from **above and to the left**, with a wide 90° lens. The balls look small in the middle of a
> large expanse of yellow ground, and perspective is exaggerated: the nearest ball (glass, on the left)
> looks biggest.

![Tele](../images/ch20_fov_tele.png)

> **Image description:** Same viewpoint, telephoto 20°. The three balls **fill the frame**, and look
> close together and similar in size: the "compressed" look of a long lens. The glass ball in front
> shows a clear inverted view.

![Defocus](../images/ch20_defocus.png)

> **Image description:** Same telephoto view with a wide aperture, focused on the blue ball. The blue
> ball is **sharp**; the glass ball in front and the gold ball behind are softly **blurred**, as is the
> ground in the foreground and background. It looks like a photo taken with an expensive camera.

---

## Try it yourself

1. Put the camera **low**, near the ground (`lookfrom = (0, 0.1, 2)`), looking at the blue ball: a
   "worm's-eye view" makes objects look heroic and big.
2. Tilt the camera: `vup = (0.3, 1, 0)`. The horizon rotates ("Dutch angle", used in films to show unease).
3. Focus on the **gold** ball instead: set `focus_dist` to the distance from `lookfrom` to its center.
4. Make the blur extreme: `defocus_angle = 30`. Then very subtle: 1.
5. Animate a "dolly zoom" (the *Vertigo* effect): move the camera back while narrowing the vfov so the blue
   ball stays the same size. (Chapter 39 shows how to render frames.)

## Common problems

| Symptom | Cause |
|---------|-------|
| Image is upside down or mirrored | `vup` parallel to the view direction, or u/v/w cross-product order swapped |
| Black image when looking straight down | `vup = (0,1,0)` is parallel to the view direction; use `vup = (0,0,-1)` |
| Everything blurry | `focus_dist` doesn't match the distance to your subject |
| No blur at all | `defocus_angle` is 0 |

---

## Summary

* **vfov** sets the lens: wide (large) or telephoto (small).
* `lookfrom`, `lookat`, `vup` → the u, v, w axes via cross products.
* **Depth of field**: rays start on a random point of a lens disk and meet at the focus plane.
  Objects off that plane blur.

Next: [Chapter 21 — Multithreading and your first masterpiece →](21-multithreading-final-scene.md)
