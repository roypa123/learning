# Line by line: `ch14_sphere.cpp`

[← Line-by-line index](README.md) · [Chapter 14 (the theory)](../14-hitting-a-sphere.md)

**What the whole program does, in one sentence:** it puts one sphere in front of the camera, checks for every pixel's
ray whether it hits the sphere (by solving a quadratic equation), and makes two images: a flat red disc and a
"distance" picture.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | |
| B. `hit_sphere` | 12–21 | does the ray hit the sphere? how far away? |
| C. `sky` | 23–27 | the background color |
| D. Camera (short version) | 29–33 | same camera as chapter 13, in 3 lines |
| E. Render loop | 35–52 | per pixel: shoot a ray, color both images |
| F. Save and end | 53–58 | |

---

## Block A — Comments, includes (lines 1–10)

Comments, `<cmath>` (sqrt), our library, `using namespace pixel`.

---

## Block B — `hit_sphere` (lines 12–21)

**The math in short:** a point P is on the sphere when its distance to the center C equals the radius r. The ray gives
points `P(t) = origin + t × direction`. Putting the ray into the sphere equation gives a **quadratic equation**
`a·t² + b·t + c = 0`. Solving it gives the distances `t` where the ray touches the sphere. (Full derivation in chapter 14.)

```cpp
double hit_sphere(const Point3& center, double radius, const Ray& r) {
```

Line 13: inputs: the sphere's center and radius, and the ray. Output: the distance `t` to the hit, or −1 for "missed".

```cpp
    Vec3 oc = center - r.origin();
```

Line 14: `oc` = the vector from the ray's start to the sphere's center.

```cpp
    double a = dot(r.direction(), r.direction());
    double b = -2.0 * dot(r.direction(), oc);
    double c = dot(oc, oc) - radius * radius;
```

Lines 15–17: the three numbers of the quadratic equation:

* `a` = direction · direction (the squared length of the direction).
* `b` = −2 × (direction · oc).
* `c` = (oc · oc) − r² (squared distance to the center, minus squared radius).

```cpp
    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return -1.0;               // no real solution: miss
```

* Line 18: the **discriminant** `b² − 4ac` (the part under the square root in the quadratic formula).
* Line 19: if it's negative there's no solution: the ray **misses** the sphere. Return −1.
  (0 would mean the ray just touches the edge; positive means it goes in and out.)

```cpp
    return (-b - std::sqrt(discriminant)) / (2.0 * a);  // the nearer of the two solutions
}
```

Line 20: the quadratic formula `t = (−b ± √D) / 2a`. We use the **minus** version, which gives the **smaller** t: the
point where the ray **enters** the sphere (the side facing us).

---

## Block C — `sky` (lines 23–27)

The same blue-to-white sky as [ch13's ray_color](ch13_rays_sky.md), with a different name.

---

## Block D — Camera, short version (lines 29–33)

```cpp
int main() {
    const int W = 400, H = 225;
    const Point3 eye(0, 0, 0);
    const Vec3 du(3.5555 / W, 0, 0), dv(0, -2.0 / H, 0);
    const Point3 pixel00 = eye - Vec3(0, 0, 1) - Vec3(3.5555 / 2, 0, 0) + Vec3(0, 1, 0) + 0.5 * (du + dv);
```

The chapter 13 camera, written compactly:

* Line 30: image 400 × 225.
* Line 31: the eye at the origin.
* Line 32: `du` = one pixel step to the right (viewport width 3.5555 ÷ 400); `dv` = one pixel step down (height 2 ÷ 225,
  negative because y goes up in 3D).
* Line 33: the center of the top-left pixel: from the eye, 1 forward (−z), half the width left, half the height (1) up,
  then half a pixel right and down.

---

## Block E — Render loop (lines 35–52)

```cpp
    Image flat(W, H), depth(W, H);
    for (int j = 0; j < H; j++) {
        for (int i = 0; i < W; i++) {
            Ray r(eye, pixel00 + i * du + j * dv - eye);
            double t = hit_sphere(Point3(0, 0, -1), 0.5, r);
```

* Line 35: two images at once.
* Lines 36–37: every pixel.
* Line 38: the ray from the eye through this pixel's center (`pixel center − eye` = direction).
* Line 39: test it against a sphere at (0, 0, −1) (1 unit in front of the camera) with radius 0.5.

```cpp
            flat.at(i, j) = t > 0 ? Color(1, 0, 0) : sky(r);
```

Line 42: image 1: if we hit the sphere in front of us (t > 0), red; otherwise the sky.

```cpp
            if (t > 0) {
                double closeness = clamp01((1.1 - t * r.direction().length()) / 0.6);
                depth.at(i, j) = Color(closeness, closeness, closeness);
            } else {
                depth.at(i, j) = Color(0.05, 0.05, 0.1);
            }
```

* Line 45: only for hits.
* Line 46: the real distance = `t × length of direction` (because t counts in units of the direction's length). The
  sphere's nearest point is ~0.5 away, its edges ~0.87. `(1.1 − distance) / 0.6` turns that into brightness: near →
  1 (white), far → darker. `clamp01` keeps it in 0–1.
* Line 47: a grey of that brightness.
* Line 49: misses get a dark navy background.

---

## Block F — Save and end (lines 53–58)

Save both images without the sRGB curve (like chapter 13), `return 0`.

---

## Check your understanding

1. What does a negative discriminant mean? *(The ray misses the sphere.)*
2. Why take `−√D` instead of `+√D`? *(It's the smaller t: the side of the sphere facing us.)*
3. Why is the red disc flat-looking? *(We only know "hit or miss", not which way the surface faces. Normals come next
   chapter.)*
4. Why multiply `t` by the direction's length on line 46? *(The direction isn't length 1, so t isn't a real distance
   by itself.)*
