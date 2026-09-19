# Chapter 25 — Quads, triangles and meshes

[← Textures](24-textures.md) · [Contents](README.md) · [Next: Lights & the Cornell box →](26-lights-cornell-box.md)

> 📖 **Line by line:** [quad explained line by line](line-by-line/quad.md) · [triangle explained line by line](line-by-line/triangle.md) · [ch25_meshes explained line by line](line-by-line/ch25_meshes.md)

---

## Goal

Spheres are great, but the world is made of flat and curved **surfaces** of every shape. You'll add:

* **quads** (parallelograms): walls, floors, boxes, area lights,
* **disks**,
* **triangles** with the fast **Möller–Trumbore** intersection,
* **smooth shading** with per-vertex normals,
* **triangle meshes** and the **OBJ** file format (reading and writing),
* procedural meshes: a torus and a 20,000-triangle **terrain**.

---

## 1. Planes

A plane is all the points P that satisfy:

```
dot(n, P) = D           n = the plane's normal, D = a constant
```

To intersect a ray `P(t) = Q + t·d` with a plane, plug it in and solve for t:

```
dot(n, Q + t·d) = D   →   t = (D − dot(n, Q)) / dot(n, d)
```

If `dot(n, d) ≈ 0`, the ray is parallel to the plane: no hit.

---

## 2. Quads

A **quad** here is a parallelogram given by a corner **Q** and two edge vectors **u** and **v**:

```
      Q+v ───────────── Q+u+v
       ╱               ╱
      ╱               ╱
     Q ───────────── Q+u
```

* normal: `n = unit_vector(cross(u, v))` (so the order of u and v decides which side is the **front**),
* plane constant: `D = dot(n, Q)`.

After finding the hit point P on the plane, we need to know if it's **inside** the parallelogram. Write
`P − Q = α·u + β·v`: P is inside if both α and β are between 0 and 1. The coordinates come from a small
formula with a precomputed helper vector `w = n / dot(n, n)` (with n the non-normalized cross product):

```
α = dot(w, cross(P − Q, v))
β = dot(w, cross(u, P − Q))
```

Bonus: α and β are natural **texture coordinates** (u, v) for the quad.

```cpp
bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    double denom = dot(normal, r.direction());
    if (std::fabs(denom) < 1e-8) return false;         // parallel
    double t = (D - dot(normal, r.origin())) / denom;
    if (!ray_t.contains(t)) return false;
    Point3 intersection = r.at(t);
    Vec3 planar = intersection - Q;
    double alpha = dot(w, cross(planar, v));
    double beta  = dot(w, cross(u, planar));
    if (!is_interior(alpha, beta, rec)) return false;
    ...
}
```

`is_interior` is `virtual`, so shapes built on the same plane math can change the inside test. `Disk`
overrides it with `(α − 0.5)² + (β − 0.5)² ≤ 0.25`: a circle inscribed in the parallelogram.

### 2.1 Boxes

`make_box(a, b, material)` builds a closed box from 6 quads, returned as a `HittableList`. The quads are
oriented so that all normals point **outwards**, which is important for lights and volumes.

### `quad.h`

**File: `include/pixel/quad.h`**

```cpp
// pixel/quad.h
// ------------------------------------------------------------
// Quad: a flat parallelogram given by a corner Q and two edge
// vectors u and v.        Q+v ------- Q+u+v
//                          |           |
//                          Q  -------  Q+u
// Also: Disk, and make_box() which builds a box from 6 quads.
// Explained in docs/25-quads-triangles-meshes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <memory>
#include "vec3.h"
#include "hittable.h"
#include "random.h"

namespace pixel {

class Quad : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, std::shared_ptr<Material> mat)
        : Q(Q), u(u), v(v), mat(mat) {
        Vec3 n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);                 // plane equation: dot(normal, P) = D
        w = n / dot(n, n);                  // helper for computing alpha/beta
        area = n.length();
        AABB diag1(Q, Q + u + v);
        AABB diag2(Q + u, Q + v);
        bbox = AABB(diag1, diag2);
    }

    AABB bounding_box() const override { return bbox; }

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        double denom = dot(normal, r.direction());
        if (std::fabs(denom) < 1e-8) return false;         // ray parallel to the plane

        double t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;

        // Where on the plane did we land, in (alpha, beta) coordinates?
        Point3 intersection = r.at(t);
        Vec3 planar_hitpt = intersection - Q;
        double alpha = dot(w, cross(planar_hitpt, v));
        double beta  = dot(w, cross(u, planar_hitpt));
        if (!is_interior(alpha, beta, rec)) return false;

        rec.t = t;
        rec.p = intersection;
        rec.mat = mat.get();
        rec.set_face_normal(r, normal);
        return true;
    }

    // For light sampling: probability density of hitting us from 'origin'.
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double distance_squared = rec.t * rec.t * direction.length_squared();
        double cosine = std::fabs(dot(direction, rec.normal) / direction.length());
        if (cosine < 1e-8) return 0.0;
        return distance_squared / (cosine * area);
    }

    Vec3 random(const Point3& origin) const override {
        Point3 p = Q + (random_double() * u) + (random_double() * v);
        return p - origin;
    }

protected:
    // Inside the parallelogram if both coordinates are in [0,1].
    virtual bool is_interior(double a, double b, HitRecord& rec) const {
        Interval unit_interval(0, 1);
        if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }

    Point3 Q;
    Vec3 u, v;
    Vec3 w;
    std::shared_ptr<Material> mat;
    AABB bbox;
    Vec3 normal;
    double D;
    double area;
};

// A disk (circle) inside the parallelogram. Q is the corner, so the center
// is Q + u/2 + v/2. Use square u and v of equal length for a round disk.
class Disk : public Quad {
public:
    Disk(const Point3& center, const Vec3& half_u, const Vec3& half_v, std::shared_ptr<Material> mat)
        : Quad(center - half_u - half_v, 2.0 * half_u, 2.0 * half_v, mat) {
        area = area * pi / 4.0;
    }
    Vec3 random(const Point3& origin) const override {
        Vec3 d = random_in_unit_disk();
        Point3 p = Q + u * (0.5 + 0.5 * d.x) + v * (0.5 + 0.5 * d.y);
        return p - origin;
    }
protected:
    bool is_interior(double a, double b, HitRecord& rec) const override {
        double da = a - 0.5, db = b - 0.5;
        if (da * da + db * db > 0.25) return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

// A box made of 6 quads, from two opposite corners a and b.
inline std::shared_ptr<HittableList> make_box(const Point3& a, const Point3& b, std::shared_ptr<Material> mat) {
    auto sides = std::make_shared<HittableList>();
    Point3 min = vmin(a, b);
    Point3 max = vmax(a, b);
    Vec3 dx(max.x - min.x, 0, 0);
    Vec3 dy(0, max.y - min.y, 0);
    Vec3 dz(0, 0, max.z - min.z);
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, max.z),  dx,  dy, mat)); // front
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, max.z), -dz,  dy, mat)); // right
    sides->add(std::make_shared<Quad>(Point3(max.x, min.y, min.z), -dx,  dy, mat)); // back
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dz,  dy, mat)); // left
    sides->add(std::make_shared<Quad>(Point3(min.x, max.y, max.z),  dx, -dz, mat)); // top
    sides->add(std::make_shared<Quad>(Point3(min.x, min.y, min.z),  dx,  dz, mat)); // bottom
    return sides;
}

} // namespace pixel
```

---

## 3. Triangles

### 3.1 The Möller–Trumbore algorithm

We could intersect the triangle's plane and then test barycentric coordinates (chapter 10). In 1997,
Tomas Möller and Ben Trumbore published a faster way that computes the distance t and the barycentric
coordinates (b1, b2) **together**, in one go, without precomputing the plane.

The idea: a point in the triangle is `V0 + b1·(V1 − V0) + b2·(V2 − V0)`. Set it equal to the ray
`O + t·D` and you get a 3×3 linear system in (t, b1, b2). Solve it with **Cramer's rule**, where
determinants become cross and dot products:

```
e1 = V1 − V0,  e2 = V2 − V0
p  = cross(D, e2)
det = dot(e1, p)                   ≈ 0  -> ray parallel to the triangle
T  = O − V0
b1 = dot(T, p) / det               must be in [0, 1]
q  = cross(T, e1)
b2 = dot(D, q) / det               must be ≥ 0 and b1 + b2 ≤ 1
t  = dot(e2, q) / det
```

Each test can exit early, so rays that miss are rejected very quickly.

### 3.2 Smooth shading: vertex normals

A curved object (like a donut) made of flat triangles looks **faceted**: each triangle has one normal, so
it's lit uniformly, and you see the edges. The classic fix (**Phong shading**, 1975): store a normal at
each **vertex** (the average of the surrounding face normals) and **interpolate** it across the triangle
with the barycentric coordinates:

```
shading_normal = normalize(b0·n0 + b1·n1 + b2·n2)          b0 = 1 − b1 − b2
```

```
 flat shading                smooth shading
 ┌──┬──┬──┬──┐               ░░▒▒▒▓▓▓▓███
 │░░│▒▒│▓▓│██│  (bands)      smooth gradient
```

The **geometry** is still flat triangles (the silhouette stays polygonal), but the **lighting** is smooth.
We keep the true geometric normal for the front/back decision, and flip the shading normal to the
same side.

Texture coordinates (UVs) are interpolated the same way.

### 3.3 Meshes

A **mesh** is a list of vertex positions plus a list of triangles that reference them by index:

```
positions: [ (0,0,0), (1,0,0), (1,1,0), (0,1,0) ]
indices:   [ 0,1,2,   0,2,3 ]          -> two triangles forming a square, sharing vertices 0 and 2
```

Sharing vertices saves memory and makes smooth normals easy: `compute_smooth_normals()` adds each
triangle's (area-weighted) normal to its three vertices and normalizes at the end.

`Mesh::build(material)` creates one `Triangle` per face and puts them all in a **BVH** (chapter 23),
so a 100,000-triangle mesh renders quickly.

### 3.4 The OBJ file format

**Wavefront OBJ** (1980s) is the simplest common 3D model format: plain text, and every 3D program can
export it:

```
# a square
v 0 0 0          <- vertex positions
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0           <- texture coordinates (optional)
vn 0 0 1         <- normals (optional)
f 1 2 3          <- faces: vertex indices, starting at 1!
f 1 3 4
```

Faces can also be written `f v/vt/vn`, `f v//vn` or `f v/vt`, and may have more than 3 corners (we
split them into a fan of triangles). Negative indices count from the end. Our `load_obj` handles all
of that, ignoring everything else (materials, groups).

### 3.5 Procedural meshes

`make_parametric_mesh(nu, nv, f)` makes a grid of `(u, v)` samples and connects neighbors into triangles.
With it:

* **torus**: `f(u, v)` = a point on a donut (big circle angle u, small circle angle v),
* **heightfield**: `f(u, v) = (x, height(x, z), z)`, any terrain you can write as a formula.

### `triangle.h`

**File: `include/pixel/triangle.h`**

```cpp
// pixel/triangle.h
// ------------------------------------------------------------
// Triangles and triangle meshes (+ a tiny OBJ file loader/writer).
// Every 3D model in films and games is made of triangles.
// Explained in docs/25-quads-triangles-meshes.md
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "vec3.h"
#include "hittable.h"
#include "bvh.h"
#include "random.h"

namespace pixel {

// Texture coordinate pair.
struct UV { double u = 0, v = 0; };

class Triangle : public Hittable {
public:
    // Flat triangle.
    Triangle(const Point3& a, const Point3& b, const Point3& c, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), mat(mat) { setup(); }

    // Smooth triangle: one normal (and uv) per corner, blended across the face.
    Triangle(const Point3& a, const Point3& b, const Point3& c,
             const Vec3& na, const Vec3& nb, const Vec3& nc,
             const UV& ta, const UV& tb, const UV& tc,
             bool use_normals, bool use_uvs, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), n0(na), n1(nb), n2(nc), t0(ta), t1(tb), t2(tc),
          has_normals(use_normals), has_uvs(use_uvs), mat(mat) { setup(); }

    // Moller-Trumbore ray/triangle intersection.
    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
        Vec3 pvec = cross(r.direction(), e2);
        double det = dot(e1, pvec);
        if (std::fabs(det) < 1e-12) return false;        // parallel
        double inv_det = 1.0 / det;

        Vec3 tvec = r.origin() - v0;
        double b1 = dot(tvec, pvec) * inv_det;           // barycentric weight of v1
        if (b1 < 0.0 || b1 > 1.0) return false;

        Vec3 qvec = cross(tvec, e1);
        double b2 = dot(r.direction(), qvec) * inv_det;  // barycentric weight of v2
        if (b2 < 0.0 || b1 + b2 > 1.0) return false;

        double t = dot(e2, qvec) * inv_det;
        if (!ray_t.surrounds(t)) return false;

        double b0 = 1.0 - b1 - b2;
        rec.t = t;
        rec.p = r.at(t);
        rec.mat = mat.get();
        rec.set_face_normal(r, geo_normal);
        if (has_normals) {
            Vec3 sn = unit_vector(b0 * n0 + b1 * n1 + b2 * n2);
            if (dot(sn, rec.normal) < 0) sn = -sn;       // keep it on the same side
            rec.normal = sn;
        }
        if (has_uvs) {
            rec.u = b0 * t0.u + b1 * t1.u + b2 * t2.u;
            rec.v = b0 * t0.v + b1 * t1.v + b2 * t2.v;
        } else {
            rec.u = b1;
            rec.v = b2;
        }
        return true;
    }

    AABB bounding_box() const override { return bbox; }

    // Light sampling (a triangle can be an area light too).
    double pdf_value(const Point3& origin, const Vec3& direction) const override {
        HitRecord rec;
        if (!this->hit(Ray(origin, direction), Interval(0.001, infinity), rec)) return 0.0;
        double distance_squared = rec.t * rec.t * direction.length_squared();
        double cosine = std::fabs(dot(direction, geo_normal) / direction.length());
        if (cosine < 1e-8) return 0.0;
        return distance_squared / (cosine * area);
    }

    Vec3 random(const Point3& origin) const override {
        double r1 = random_double(), r2 = random_double();
        double s = std::sqrt(r1);
        Point3 p = (1 - s) * v0 + s * (1 - r2) * v1 + s * r2 * v2;   // uniform on triangle
        return p - origin;
    }

private:
    Point3 v0, v1, v2;
    Vec3 n0, n1, n2;
    UV t0, t1, t2;
    bool has_normals = false, has_uvs = false;
    std::shared_ptr<Material> mat;
    Vec3 e1, e2, geo_normal;
    double area = 0;
    AABB bbox;

    void setup() {
        e1 = v1 - v0;
        e2 = v2 - v0;
        Vec3 n = cross(e1, e2);
        area = 0.5 * n.length();
        geo_normal = area > 0 ? unit_vector(n) : Vec3(0, 1, 0);
        bbox = AABB(AABB(v0, v1), AABB(v2, v2));
    }
};

// ---------------------------------------------------------------------------
// A triangle mesh stored as arrays, like in a real 3D program.
struct Mesh {
    std::vector<Point3> positions;
    std::vector<Vec3> normals;       // optional, one per position
    std::vector<UV> uvs;             // optional, one per position
    std::vector<int> indices;        // 3 per triangle

    size_t triangle_count() const { return indices.size() / 3; }

    // Compute smooth normals by averaging the face normals around each vertex.
    void compute_smooth_normals() {
        normals.assign(positions.size(), Vec3(0, 0, 0));
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            Vec3 n = cross(positions[b] - positions[a], positions[c] - positions[a]); // area weighted
            normals[a] += n; normals[b] += n; normals[c] += n;
        }
        for (auto& n : normals) n = n.length() > 0 ? unit_vector(n) : Vec3(0, 1, 0);
    }

    // Move/scale all vertices: p' = p * scale + offset
    void transform(double scale, const Vec3& offset) {
        for (auto& p : positions) p = p * scale + offset;
    }

    // Build one Hittable (a BVH over all triangles) from this mesh.
    std::shared_ptr<Hittable> build(std::shared_ptr<Material> mat) const {
        HittableList list;
        bool use_n = normals.size() == positions.size();
        bool use_uv = uvs.size() == positions.size();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            list.add(std::make_shared<Triangle>(
                positions[a], positions[b], positions[c],
                use_n ? normals[a] : Vec3(), use_n ? normals[b] : Vec3(), use_n ? normals[c] : Vec3(),
                use_uv ? uvs[a] : UV(), use_uv ? uvs[b] : UV(), use_uv ? uvs[c] : UV(),
                use_n, use_uv, mat));
        }
        if (list.objects.empty()) return std::make_shared<HittableList>();
        return std::make_shared<BVHNode>(list);
    }
};

// ---------------------------------------------------------------------------
// Minimal Wavefront .OBJ reader. Supports: v, vt, vn, f (any polygon,
// "v", "v/vt", "v//vn", "v/vt/vn", negative indices). Everything else is ignored.
// Corners are "unwelded": each face corner gets its own vertex (simple & correct).
inline bool load_obj(const std::string& filename, Mesh& mesh) {
    std::ifstream f(filename);
    if (!f) { std::printf("Could not open OBJ '%s'\n", filename.c_str()); return false; }
    std::vector<Point3> P;
    std::vector<Vec3> N;
    std::vector<UV> T;
    mesh = Mesh();
    bool any_normals = false, any_uvs = false;
    std::vector<Vec3> out_n;
    std::vector<UV> out_t;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") { Point3 p; ss >> p.x >> p.y >> p.z; P.push_back(p); }
        else if (tag == "vn") { Vec3 n; ss >> n.x >> n.y >> n.z; N.push_back(n); }
        else if (tag == "vt") { UV t; ss >> t.u >> t.v; T.push_back(t); }
        else if (tag == "f") {
            std::vector<int> face;   // indices into mesh.positions
            std::string corner;
            while (ss >> corner) {
                int idx[3] = {0, 0, 0};   // v, vt, vn (0 = missing)
                int part = 0;
                std::string num;
                for (size_t i = 0; i <= corner.size(); i++) {
                    if (i == corner.size() || corner[i] == '/') {
                        if (!num.empty() && part < 3) idx[part] = std::stoi(num);
                        num.clear();
                        part++;
                    } else num += corner[i];
                }
                auto fix = [](int i, size_t count) { return i < 0 ? (int)count + i : i - 1; };
                int vi = fix(idx[0], P.size());
                if (vi < 0 || vi >= (int)P.size()) continue;
                mesh.positions.push_back(P[vi]);
                if (idx[1] != 0) {
                    int ti = fix(idx[1], T.size());
                    out_t.push_back(ti >= 0 && ti < (int)T.size() ? T[ti] : UV());
                    any_uvs = true;
                } else out_t.push_back(UV());
                if (idx[2] != 0) {
                    int ni = fix(idx[2], N.size());
                    out_n.push_back(ni >= 0 && ni < (int)N.size() ? unit_vector(N[ni]) : Vec3(0, 1, 0));
                    any_normals = true;
                } else out_n.push_back(Vec3(0, 1, 0));
                face.push_back((int)mesh.positions.size() - 1);
            }
            // Triangulate the polygon as a fan: (0,1,2), (0,2,3), ...
            for (size_t k = 1; k + 1 < face.size(); k++) {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[k]);
                mesh.indices.push_back(face[k + 1]);
            }
        }
    }
    if (any_normals) mesh.normals = out_n;
    if (any_uvs) mesh.uvs = out_t;
    std::printf("Loaded %s: %zu triangles\n", filename.c_str(), mesh.triangle_count());
    return true;
}

// Write a mesh as an OBJ file (positions, uvs, normals if present).
inline bool save_obj(const std::string& filename, const Mesh& mesh) {
    std::ofstream f(filename);
    if (!f) return false;
    f << "# written by the pixel library\n";
    for (auto& p : mesh.positions) f << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
    bool has_uv = mesh.uvs.size() == mesh.positions.size();
    bool has_n = mesh.normals.size() == mesh.positions.size();
    if (has_uv) for (auto& t : mesh.uvs) f << "vt " << t.u << ' ' << t.v << '\n';
    if (has_n) for (auto& n : mesh.normals) f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        f << 'f';
        for (int k = 0; k < 3; k++) {
            int id = mesh.indices[i + k] + 1;
            f << ' ' << id;
            if (has_uv && has_n) f << '/' << id << '/' << id;
            else if (has_uv) f << '/' << id;
            else if (has_n) f << "//" << id;
        }
        f << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Procedural meshes: a grid over (u,v) in [0,1]^2 mapped through a function.
inline Mesh make_parametric_mesh(int nu, int nv, const std::function<Point3(double, double)>& fn) {
    Mesh m;
    for (int j = 0; j <= nv; j++)
        for (int i = 0; i <= nu; i++) {
            double u = (double)i / nu, v = (double)j / nv;
            m.positions.push_back(fn(u, v));
            m.uvs.push_back(UV{u, v});
        }
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            int a = j * (nu + 1) + i, b = a + 1, c = a + (nu + 1), d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, d, a, d, c});
        }
    m.compute_smooth_normals();
    return m;
}

// Torus (donut) around the Y axis.
inline Mesh make_torus(double major_radius, double minor_radius, int nu = 64, int nv = 32) {
    return make_parametric_mesh(nu, nv, [=](double u, double v) {
        double a = u * 2 * pi, b = v * 2 * pi;
        double r = major_radius + minor_radius * std::cos(b);
        return Point3(r * std::cos(a), minor_radius * std::sin(b), r * std::sin(a));
    });
}

// Heightfield terrain: y = height(x, z) over a square of side 'size'.
inline Mesh make_heightfield(double size, int resolution, const std::function<double(double, double)>& height) {
    return make_parametric_mesh(resolution, resolution, [=](double u, double v) {
        double x = (u - 0.5) * size, z = (v - 0.5) * size;
        return Point3(x, height(x, z), z);
    });
}

} // namespace pixel
```

---

## 4. The program

**File: `chapters/ch25_meshes.cpp`**

```cpp
// ch25_meshes.cpp
// ------------------------------------------------------------
// Chapter 25: Quads, triangles and meshes.
//   images/ch25_quads.png      - five colored quads (a broken box)
//   images/ch25_triangles.png  - flat vs smooth shaded triangle meshes
//   images/ch25_terrain.png    - a heightfield terrain made of 20,000 triangles
// Also writes models/torus.obj, then loads it back with our OBJ reader.
// ------------------------------------------------------------
#include <cmath>
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    // ---------- 1. Quads ---------------------------------------------------
    {
        auto left_red     = std::make_shared<Lambertian>(Color(1.0, 0.2, 0.2));
        auto back_green   = std::make_shared<Lambertian>(Color(0.2, 1.0, 0.2));
        auto right_blue   = std::make_shared<Lambertian>(Color(0.2, 0.2, 1.0));
        auto upper_orange = std::make_shared<Lambertian>(Color(1.0, 0.5, 0.0));
        auto lower_teal   = std::make_shared<Lambertian>(Color(0.2, 0.8, 0.8));

        HittableList world;
        world.add(std::make_shared<Quad>(Point3(-3, -2, 5), Vec3(0, 0, -4), Vec3(0, 4, 0), left_red));
        world.add(std::make_shared<Quad>(Point3(-2, -2, 0), Vec3(4, 0, 0), Vec3(0, 4, 0), back_green));
        world.add(std::make_shared<Quad>(Point3(3, -2, 1), Vec3(0, 0, 4), Vec3(0, 4, 0), right_blue));
        world.add(std::make_shared<Quad>(Point3(-2, 3, 1), Vec3(4, 0, 0), Vec3(0, 0, 4), upper_orange));
        world.add(std::make_shared<Quad>(Point3(-2, -3, 5), Vec3(4, 0, 0), Vec3(0, 0, -4), lower_teal));

        Camera cam;
        cam.aspect_ratio = 1.0;
        cam.image_width = 400;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 80;
        cam.lookfrom = Point3(0, 0, 9);
        cam.lookat = Point3(0, 0, 0);
        save_image("images/ch25_quads.png", cam.render(world));
    }

    // ---------- 2. Triangle meshes: write an OBJ, read it back -------------
    {
        Mesh torus = make_torus(1.0, 0.4, 48, 24);
        ensure_parent_folder("models/torus.obj");   // make the "models" folder if needed
        save_obj("models/torus.obj", torus);   // look at this file in a text editor!

        Mesh loaded;
        if (!load_obj("models/torus.obj", loaded)) return 1;

        // Flat version: same triangles but WITHOUT per-vertex normals.
        Mesh flat = loaded;
        flat.normals.clear();

        HittableList world;
        auto ground = std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(0.5, Color(0.8, 0.8, 0.8), Color(0.3, 0.3, 0.3)));
        world.add(std::make_shared<Quad>(Point3(-20, -0.4, -20), Vec3(40, 0, 0), Vec3(0, 0, 40), ground));

        auto red = std::make_shared<Lambertian>(Color(0.8, 0.15, 0.1));
        auto blue = std::make_shared<Lambertian>(Color(0.1, 0.3, 0.8));
        world.add(std::make_shared<Translate>(flat.build(red), Vec3(-1.5, 0, 0)));
        world.add(std::make_shared<Translate>(loaded.build(blue), Vec3(1.5, 0, 0)));

        Camera cam;
        cam.image_width = 500;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 35;
        cam.lookfrom = Point3(0, 4, 6);
        cam.lookat = Point3(0, 0, 0);
        save_image("images/ch25_triangles.png", cam.render(world));
    }

    // ---------- 3. Terrain from a height function --------------------------
    {
        Perlin noise(3);
        auto height = [&noise](double x, double z) {
            double h = noise.fbm(Point3(x * 0.15, 0, z * 0.15), 6);
            return 3.0 * h + 0.6 * std::exp(-(x * x + z * z) / 20.0) * 3.0;  // + a central mountain
        };
        Mesh terrain = make_heightfield(24, 100, height);   // 100x100 grid = 20,000 triangles
        std::printf("Terrain has %zu triangles\n", terrain.triangle_count());

        // Color by height: sand, grass, rock, snow.
        auto terrain_color = std::make_shared<FunctionTexture>([](double, double, const Point3& p) {
            if (p.y < -0.4) return hex_color(0xC2B280);
            if (p.y < 0.8)  return hex_color(0x4F7942);
            if (p.y < 1.8)  return hex_color(0x7D7461);
            return hex_color(0xF5F5F5);
        });
        HittableList world;
        world.add(terrain.build(std::make_shared<Lambertian>(terrain_color)));
        auto water = std::make_shared<Metal>(hex_color(0x3A6EA5), 0.05);
        world.add(std::make_shared<Quad>(Point3(-12, -0.8, -12), Vec3(24, 0, 0), Vec3(0, 0, 24), water));

        Camera cam;
        cam.image_width = 640;
        cam.samples_per_pixel = 64;
        cam.max_depth = 20;
        cam.vfov = 40;
        cam.lookfrom = Point3(14, 9, 14);
        cam.lookat = Point3(0, 0, 0);
        save_image("images/ch25_terrain.png", cam.render(world));
    }
    return 0;
}
```

Three scenes:

1. **Quads**: five colored quads facing the camera, like an open box.
2. **Triangles**: a torus is written to `models/torus.obj`, loaded back, and rendered twice: without
   normals (flat, red, left) and with smooth normals (blue, right). (`Translate`, from chapter 27, moves each
   copy sideways.)
3. **Terrain**: a 100×100 heightfield built from Perlin fBm plus a central mountain, colored by
   height with a `FunctionTexture`, with a reflective "water" plane at y = −0.8.

```bat
run ch25_meshes
```

Open `models/torus.obj` in a text editor, or in any 3D program (Blender, Windows 3D Viewer): it's a real
model file your code wrote.

---

## 5. What you should see

![Quads](../images/ch25_quads.png)

> **Image description:** A square image showing five flat colored panels arranged like the inside of a
> box seen from the open front: a red panel on the left, green at the back, blue on the right, orange on
> top and teal at the bottom, with the blue-white sky visible through the gaps.

![Triangles](../images/ch25_triangles.png)

> **Image description:** Two donuts lying on a grey checkered floor, seen from above at an angle.
> The **left, red** donut is visibly **faceted**, made of small flat rectangular panels with visible
> edges. The **right, blue** donut has the same shape but looks **smooth**, with continuous
> shading. Both have faceted silhouettes if you look very closely at the outline.

![Terrain](../images/ch25_terrain.png)

> **Image description:** An aerial view of a small island landscape: a green rolling terrain with a
> central mountain, brown-grey rocky slopes and white snowy peaks, sandy beaches at the lowest edges,
> surrounded by a blue reflective water plane that mirrors the sky and the hills. It looks like a model
> landscape on a table.

---

## Try it yourself

1. Make a `Disk` light (chapter 26) or a disk-shaped table top.
2. Change the terrain's `height` lambda: sharper mountains (ridged noise, chapter 11), or a volcano
   crater (subtract a small bump at the peak).
3. Export a model from Blender as OBJ (triangulated) and load it. Use `mesh.transform(scale, offset)`
   to fit it into the scene.
4. Generate a **sphere mesh** with `make_parametric_mesh` (u = longitude, v = latitude) and compare it to
   the analytic `Sphere`. How many triangles until you can't tell the difference?
5. Count the triangles in `models/torus.obj` (`f` lines) and compare with `48 × 24 × 2`.

## Common problems

| Symptom | Cause |
|---------|-------|
| Quad invisible from one side (as a light) | Normal points the wrong way: swap u and v |
| Mesh has holes or dark triangles | Inconsistent winding with flat shading, or bad normals in the file |
| Nothing loads from OBJ | Wrong path; or the file uses quads with separate texture indices (supported), or is binary (not OBJ) |
| Faceted look although normals exist | The file had no `vn` lines: call `compute_smooth_normals()` |
| Rendering a mesh is slow | Built without a BVH: always use `mesh.build()` |

---

## Summary

* Quads: plane intersection + (α, β) coordinates; boxes are 6 quads.
* Triangles: Möller–Trumbore gives t and barycentric coordinates in one go.
* Vertex normals interpolated across triangles give smooth shading.
* Meshes = positions + indices; OBJ is the simplest file format for them; BVHs make them fast.

Next: [Chapter 26 — Lights and the Cornell box →](26-lights-cornell-box.md)
