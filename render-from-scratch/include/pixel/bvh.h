// pixel/bvh.h
// ------------------------------------------------------------
// BVH = Bounding Volume Hierarchy. A tree of boxes-inside-boxes.
// Instead of testing the ray against ALL objects (slow), we test
// big boxes first and skip everything inside boxes we miss.
// 1,000,000 objects need only ~20 box tests per ray.
// Explained in docs/23-bvh.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include "hittable.h"
#include "aabb.h"

namespace pixel {

class BVHNode : public Hittable {
public:
    BVHNode(HittableList list) : BVHNode(list.objects, 0, list.objects.size()) {}

    BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
        // 1. Box around everything in this node.
        bbox = AABB::empty();
        for (size_t i = start; i < end; i++) bbox = AABB(bbox, objects[i]->bounding_box());

        // 2. Split along the longest side of that box.
        int axis = bbox.longest_axis();
        size_t span = end - start;

        if (span == 1) {
            left = right = objects[start];
        } else if (span == 2) {
            left = objects[start];
            right = objects[start + 1];
        } else {
            // Sort by the center of each object's box along the axis, split in half.
            auto comparator = [axis](const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b) {
                return a->bounding_box().centroid()[axis] < b->bounding_box().centroid()[axis];
            };
            std::sort(objects.begin() + start, objects.begin() + end, comparator);
            size_t mid = start + span / 2;
            left = std::make_shared<BVHNode>(objects, start, mid);
            right = std::make_shared<BVHNode>(objects, mid, end);
        }
    }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;           // missed the whole box: skip all
        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);
        return hit_left || hit_right;
    }

    AABB bounding_box() const override { return bbox; }

private:
    std::shared_ptr<Hittable> left;
    std::shared_ptr<Hittable> right;
    AABB bbox;
};

} // namespace pixel
