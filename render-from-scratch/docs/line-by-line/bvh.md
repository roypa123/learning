# Line by line: `include/pixel/bvh.h`

[← Line-by-line index](README.md) · [Chapter 23 (the theory)](../23-bvh.md) · [aabb.h](aabb.md)

**What this file does, in one sentence:** it builds a **tree of boxes** (a Bounding Volume Hierarchy) around all objects,
so a ray only tests the objects near its path instead of every object in the scene.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–16 | |
| B. Constructor from a list | 18–20 | the easy way to make a BVH |
| C. Building a node | 22–46 | box around objects, split in two halves |
| D. `hit` | 48–53 | skip the whole branch if its box is missed |
| E. Box and data | 55–61 | |
| F. End | 63 | |

---

## Block A — Comments, includes (lines 1–16)

* Lines 1–8: comments (a million objects need only ~20 box tests per ray).
* Line 9: `#pragma once`.
* Lines 10–14: `<algorithm>` (`std::sort`), `<memory>`, `<vector>`, our `hittable.h` and `aabb.h`.
* Line 16: `namespace pixel`.

---

## Block B — Constructor from a list (lines 18–20)

```cpp
class BVHNode : public Hittable {
public:
    BVHNode(HittableList list) : BVHNode(list.objects, 0, list.objects.size()) {}
```

* Line 18: a BVH node **is a** Hittable (so the camera can render it like any object).
* Line 20: `BVHNode bvh(world);` takes a list (as a **copy**, because building sorts the objects) and calls the main
  constructor with all objects, from index 0 to the end.

---

## Block C — Building a node (lines 22–46)

```cpp
    BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
```

Line 22: builds a node for the objects from index `start` up to (not including) `end`.

```cpp
        bbox = AABB::empty();
        for (size_t i = start; i < end; i++) bbox = AABB(bbox, objects[i]->bounding_box());
```

Lines 24–25: **step 1**: start with an empty box and grow it to include each object's box. Now `bbox` surrounds
all objects of this node.

```cpp
        int axis = bbox.longest_axis();
        size_t span = end - start;
```

* Line 28: **step 2**: the longest side of that box. Splitting along it gives the most useful halves.
* Line 29: how many objects this node has.

```cpp
        if (span == 1) {
            left = right = objects[start];
        } else if (span == 2) {
            left = objects[start];
            right = objects[start + 1];
```

* Lines 31–32: one object: put it on both sides (simpler than handling "no right child").
* Lines 33–35: two objects: one on each side. These are the **leaves** of the tree.

```cpp
        } else {
            auto comparator = [axis](const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b) {
                return a->bounding_box().centroid()[axis] < b->bounding_box().centroid()[axis];
            };
            std::sort(objects.begin() + start, objects.begin() + end, comparator);
```

More than two objects:

* Lines 38–40: a **comparator**: a small function that says whether object a should come before object b: compare the
  centers of their boxes along the chosen axis.
* Line 41: sort this node's objects from low to high along that axis. `std::sort` uses our comparator.

```cpp
            size_t mid = start + span / 2;
            left = std::make_shared<BVHNode>(objects, start, mid);
            right = std::make_shared<BVHNode>(objects, mid, end);
        }
    }
```

* Line 42: the middle index.
* Line 43: the **first half** becomes a new node (which builds itself the same way: this is **recursion**, a function
  that uses itself on a smaller part).
* Line 44: the **second half** too.

The recursion stops when a node has 1 or 2 objects. The result is a tree:

```
            [all]
          /       \
     [left half] [right half]
      /    \        /    \
    ...    ...    ...    ...
```

---

## Block D — `hit` (lines 48–53)

```cpp
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;           // missed the whole box: skip all
```

Line 49: first test the node's box. If the ray misses it, it misses **everything inside**, so we skip the whole branch
at once. That's where the speed comes from.

```cpp
        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);
        return hit_left || hit_right;
    }
```

* Line 50: test the left child (a node or an object).
* Line 51: test the right child, but if the left side already hit something at distance `rec.t`, only accept **closer**
  hits on the right (the same trick as `HittableList`). If the right hits closer, it overwrites `rec`.
* Line 52: hit if either side hit.

---

## Block E — Box and data (lines 55–61)

```cpp
    AABB bounding_box() const override { return bbox; }
private:
    std::shared_ptr<Hittable> left;
    std::shared_ptr<Hittable> right;
    AABB bbox;
};
```

The node's box, its two children, and the class end.

---

## Block F — End (line 63)

`} // namespace pixel`

---

## Check your understanding

1. Why sort along the longest axis? *(Splitting the longest side separates objects best, so boxes overlap less.)*
2. What happens when the ray misses a node's box? *(All objects inside are skipped.)*
3. What is recursion here? *(A node builds its children with the same constructor, until only 1–2 objects are left.)*
4. With 1,000,000 objects, about how many levels deep is the tree? *(About 20, because 2²⁰ ≈ 1,000,000.)*
