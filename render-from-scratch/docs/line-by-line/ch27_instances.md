# Line by line: `ch27_instances.cpp`

[← Line-by-line index](README.md) · [Chapter 27 (the theory)](../27-instances.md) · [instance.h](instance.md)

**What the whole program does, in one sentence:** it renders the Cornell box with two rotated white boxes, and a small
forest where **one** tree model is placed 60 times with different positions and rotations.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–11 | |
| B. `add_cornell_room` | 13–24 | the five walls and the light |
| C. Scene 1: Cornell box with two boxes | 27–54 | rotate, then move |
| D. Scene 2: making one tree | 56–69 | a cone and a trunk in one BVH |
| E. Scene 2: the forest | 71–90 | place the same tree 60 times |
| F. End | 92–93 | |

---

## Block A — Comments, includes (lines 1–11)

Comments, `<cmath>`, our library, `using namespace pixel`.

---

## Block B — `add_cornell_room` (lines 13–24)

```cpp
static void add_cornell_room(HittableList& world, std::shared_ptr<Material> light) {
```

Line 14: a helper that adds the room to a world passed **by reference** (`&`), so it adds to the caller's world. The light
material is passed in.

Lines 15–23: the same walls and light as chapter 26 ([ch26 block C](ch26_cornell_box.md)).

---

## Block C — Scene 1: Cornell box with two boxes (lines 27–54)

```cpp
        HittableList world;
        add_cornell_room(world, std::make_shared<DiffuseLight>(Color(15, 15, 15)));
        auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
```

Lines 29–31: the room with a light of strength 15, and a white material for the boxes.

```cpp
        std::shared_ptr<Hittable> box1 = make_box(Point3(0, 0, 0), Point3(165, 330, 165), white);
        box1 = std::make_shared<RotateY>(box1, 15);
        box1 = std::make_shared<Translate>(box1, Vec3(265, 0, 295));
        world.add(box1);
```

* Line 34: a tall box (165 × 330 × 165) with one corner at the origin. Its type is written as `std::shared_ptr<Hittable>`
  so we can put different wrappers in the same variable.
* Line 35: wrap it: rotate 15° around the vertical axis. (The rotation happens around the origin, which is the box's corner.)
* Line 36: wrap again: move it to (265, 0, 295).
* Line 37: add the final wrapped object.

The order matters: **rotate first, then move**. If we moved first, the rotation would swing the box around the origin
of the world.

```cpp
        std::shared_ptr<Hittable> box2 = make_box(Point3(0, 0, 0), Point3(165, 165, 165), white);
        box2 = std::make_shared<RotateY>(box2, -18);
        box2 = std::make_shared<Translate>(box2, Vec3(130, 0, 65));
        world.add(box2);
```

Lines 39–42: a short cube, rotated −18° (the other way) and moved toward the front.

Lines 44–53: the Cornell camera (as in chapter 26), render, save.

---

## Block D — Scene 2: making one tree (lines 56–69)

```cpp
        Mesh cone = make_parametric_mesh(16, 1, [](double u, double v) {
            double a = u * 2 * pi;
            double r = 0.6 * (1 - v);
            return Point3(r * std::cos(a), 0.4 + 1.4 * v, r * std::sin(a));
        });
```

A **cone** from a formula (a 16 × 1 grid):

* Line 60: `a` = the angle around (u goes once around).
* Line 61: `r` = the radius: 0.6 at the bottom (v = 0), 0 at the top (v = 1).
* Line 62: the point: a circle of radius r, at height 0.4 (bottom) to 1.8 (tip).

```cpp
        auto leaves = std::make_shared<Lambertian>(hex_color(0x2D6A4F));
        auto bark = std::make_shared<Lambertian>(hex_color(0x6B4226));
        auto tree = std::make_shared<HittableList>();
        tree->add(cone.build(leaves));
        tree->add(make_box(Point3(-0.1, 0, -0.1), Point3(0.1, 0.45, 0.1), bark));
        std::shared_ptr<Hittable> tree_bvh = std::make_shared<BVHNode>(*tree);
```

* Lines 64–65: dark green leaves, brown bark.
* Line 66: a list for the tree's parts (a shared pointer to a list, so we use `->`).
* Line 67: the cone as triangles.
* Line 68: a thin box as the trunk.
* Line 69: one BVH for the whole tree. `*tree` = the list the pointer points to.

---

## Block E — Scene 2: the forest (lines 71–90)

```cpp
        HittableList world;
        world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, std::make_shared<Lambertian>(hex_color(0x95D5B2))));
        Pcg32 rng(8);
```

Lines 71–73: a light-green ground and a random generator.

```cpp
        for (int i = 0; i < 60; i++) {
            double x = rng.next_double() * 16 - 8, z = rng.next_double() * 16 - 8;
            if (x * x + z * z < 2) continue;   // leave a clearing
            std::shared_ptr<Hittable> t = std::make_shared<RotateY>(tree_bvh, rng.next_double() * 360);
            world.add(std::make_shared<Translate>(t, Vec3(x, 0, z)));   // the SAME tree, placed again
        }
```

* Line 74: 60 tries.
* Line 75: a random spot in a 16 × 16 area.
* Line 76: skip spots near the center (keep a clearing for the ball).
* Line 77: rotate the **same** tree (`tree_bvh`) by a random angle, so they don't all look identical.
* Line 78: move it to the spot. Each instance is just a tiny wrapper pointing to the one tree: 60 trees for the memory of
  one.

```cpp
        world.add(std::make_shared<Sphere>(Point3(0, 0.5, 0), 0.5, std::make_shared<Metal>(Color(0.9, 0.9, 0.9), 0.0)));
        BVHNode bvh(world);
```

* Line 80: a mirror ball in the clearing.
* Line 81: a BVH over everything.

Lines 83–90: a camera looking down at the forest; render and save.

---

## Block F — End (lines 92–93)

`return 0;` `}`

---

## Check your understanding

1. Why rotate before translating? *(Rotation is around the origin; moving first would swing the object around.)*
2. How much memory do 60 instances of the tree use? *(About one tree, plus 120 tiny wrappers.)*
3. What does line 76 do? *(Skips trees too close to the center, leaving a clearing.)*
