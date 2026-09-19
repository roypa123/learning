# Line by line: `include/pixel/triangle.h`

[← Line-by-line index](README.md) · [Chapter 25 (the theory)](../25-quads-triangles-meshes.md)

**What this file does, in one sentence:** it defines **triangles** (the building block of all 3D models), **meshes**
(many triangles sharing corners), reading and writing **OBJ** model files, and making meshes from formulas (a donut, a
terrain).

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–24 | tools; the `UV` pair |
| B. Triangle constructors | 26–38 | flat or smooth |
| C. `Triangle::hit` | 40–76 | Möller–Trumbore test + smooth normals + UVs |
| D. Box and light sampling | 78–95 | |
| E. Triangle data and `setup` | 97–115 | edges, normal, area, box |
| F. `Mesh` | 117–159 | arrays of positions/normals/uvs/indices; build a BVH |
| G. `load_obj` | 161–226 | read a model file |
| H. `save_obj` | 228–250 | write a model file |
| I. Procedural meshes | 252–286 | grid from a formula; torus; terrain |
| J. End | 288 | |

---

## Block A — Comments, includes (lines 1–24)

* Lines 1–7: comments, `#pragma once`.
* Lines 8–15: `<cmath>`, `<cstdio>`, `<fstream>` (files), `<sstream>` (reading words from a line of text), `<string>`,
  `<vector>`, `<memory>`, `<functional>`.
* Lines 16–19: our `vec3.h`, `hittable.h`, `bvh.h` (meshes are stored in a BVH), `random.h`.
* Line 24: `struct UV { double u = 0, v = 0; };`: a pair of texture coordinates.

---

## Block B — Triangle constructors (lines 26–38)

```cpp
    Triangle(const Point3& a, const Point3& b, const Point3& c, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), mat(mat) { setup(); }
```

Lines 29–30: a **flat** triangle from 3 corner points. `setup()` computes the helper values (block E).

```cpp
    Triangle(const Point3& a, const Point3& b, const Point3& c,
             const Vec3& na, const Vec3& nb, const Vec3& nc,
             const UV& ta, const UV& tb, const UV& tc,
             bool use_normals, bool use_uvs, std::shared_ptr<Material> mat)
        : v0(a), v1(b), v2(c), n0(na), n1(nb), n2(nc), t0(ta), t1(tb), t2(tc),
          has_normals(use_normals), has_uvs(use_uvs), mat(mat) { setup(); }
```

Lines 33–38: a **smooth** triangle: also a normal and a UV for each corner, plus two flags saying whether to use them.

---

## Block C — `Triangle::hit` (lines 40–76)

This is the **Möller–Trumbore** algorithm: it finds the distance t **and** the barycentric weights (how much of each
corner) in one go.

```cpp
        Vec3 pvec = cross(r.direction(), e2);
        double det = dot(e1, pvec);
        if (std::fabs(det) < 1e-12) return false;        // parallel
        double inv_det = 1.0 / det;
```

* Lines 42–43: `e1`, `e2` are the two edges from corner v0. `det` (the "determinant") measures whether the ray and the
  triangle's plane are parallel.
* Line 44: (almost) 0 → parallel → no hit.
* Line 45: `1 / det`, computed once.

```cpp
        Vec3 tvec = r.origin() - v0;
        double b1 = dot(tvec, pvec) * inv_det;           // barycentric weight of v1
        if (b1 < 0.0 || b1 > 1.0) return false;
```

* Line 47: from corner v0 to the ray's start.
* Line 48: `b1` = how much of corner v1 the hit point has.
* Line 49: outside 0–1 → the hit point is outside the triangle: stop early (this saves time).

```cpp
        Vec3 qvec = cross(tvec, e1);
        double b2 = dot(r.direction(), qvec) * inv_det;  // barycentric weight of v2
        if (b2 < 0.0 || b1 + b2 > 1.0) return false;
```

* Lines 51–52: `b2` = how much of corner v2.
* Line 53: inside the triangle only if b2 ≥ 0 and b1 + b2 ≤ 1 (then the third weight, b0, is ≥ 0 too).

```cpp
        double t = dot(e2, qvec) * inv_det;
        if (!ray_t.surrounds(t)) return false;
```

Lines 55–56: the distance along the ray; it must be in the allowed range.

```cpp
        double b0 = 1.0 - b1 - b2;
        rec.t = t;
        rec.p = r.at(t);
        rec.mat = mat.get();
        rec.set_face_normal(r, geo_normal);
```

* Line 58: the three weights always add up to 1, so `b0 = 1 − b1 − b2`.
* Lines 59–62: fill the hit record, using the triangle's true flat normal (`geo_normal`) to decide front/back.

```cpp
        if (has_normals) {
            Vec3 sn = unit_vector(b0 * n0 + b1 * n1 + b2 * n2);
            if (dot(sn, rec.normal) < 0) sn = -sn;       // keep it on the same side
            rec.normal = sn;
        }
```

Lines 63–67: **smooth shading**: blend the three corner normals by the weights. Across neighboring triangles the normal
changes smoothly, so curved models look smooth instead of faceted. Line 65 keeps it on the same side as the ray-facing
normal.

```cpp
        if (has_uvs) {
            rec.u = b0 * t0.u + b1 * t1.u + b2 * t2.u;
            rec.v = b0 * t0.v + b1 * t1.v + b2 * t2.v;
        } else {
            rec.u = b1;
            rec.v = b2;
        }
        return true;
    }
```

Lines 68–74: texture coordinates: blended from the corners' UVs if we have them; otherwise just use b1, b2.

---

## Block D — Box and light sampling (lines 78–95)

* Line 78: return the bounding box.
* Lines 81–88: the same light-sampling formula as the quad ([quad.md block D](quad.md)), with the triangle's area.

```cpp
    Vec3 random(const Point3& origin) const override {
        double r1 = random_double(), r2 = random_double();
        double s = std::sqrt(r1);
        Point3 p = (1 - s) * v0 + s * (1 - r2) * v1 + s * r2 * v2;   // uniform on triangle
        return p - origin;
    }
```

Lines 90–95: a random point **evenly spread** over the triangle. (The square root is needed; without it, points bunch up
near v0.)

---

## Block E — Triangle data and `setup` (lines 97–115)

Lines 98–105: the stored data: corners, corner normals, corner UVs, flags, material, edges, flat normal, area, box.

```cpp
    void setup() {
        e1 = v1 - v0;
        e2 = v2 - v0;
        Vec3 n = cross(e1, e2);
        area = 0.5 * n.length();
        geo_normal = area > 0 ? unit_vector(n) : Vec3(0, 1, 0);
        bbox = AABB(AABB(v0, v1), AABB(v2, v2));
    }
```

* Lines 108–109: the two edges from v0.
* Line 110: their cross product: perpendicular to the triangle; its length = twice the area.
* Line 111: the area.
* Line 112: the flat normal (a safe default for a degenerate, zero-area triangle).
* Line 113: a box around all three corners.

---

## Block F — `Mesh` (lines 117–159)

```cpp
struct Mesh {
    std::vector<Point3> positions;
    std::vector<Vec3> normals;       // optional, one per position
    std::vector<UV> uvs;             // optional, one per position
    std::vector<int> indices;        // 3 per triangle
```

The way 3D programs store models: a list of corner **positions**, and a list of **indices**, three per triangle,
saying which corners each triangle uses. Corners can be shared by many triangles.

Line 125: `triangle_count` = indices ÷ 3.

### Lines 128–136: smooth normals

```cpp
    void compute_smooth_normals() {
        normals.assign(positions.size(), Vec3(0, 0, 0));
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            Vec3 n = cross(positions[b] - positions[a], positions[c] - positions[a]); // area weighted
            normals[a] += n; normals[b] += n; normals[c] += n;
        }
        for (auto& n : normals) n = n.length() > 0 ? unit_vector(n) : Vec3(0, 1, 0);
    }
```

* Line 129: one zero normal per corner.
* Lines 130–134: for each triangle (3 indices at a time): compute its normal and **add** it to each of its three
  corners. (Not normalized, so bigger triangles count more.)
* Line 135: normalize each corner's sum: the average direction of the surrounding triangles.

### Lines 139–141: move/scale

`p = p × scale + offset` for every corner: resize and move a loaded model.

### Lines 144–158: build a renderable object

```cpp
    std::shared_ptr<Hittable> build(std::shared_ptr<Material> mat) const {
        HittableList list;
        bool use_n = normals.size() == positions.size();
        bool use_uv = uvs.size() == positions.size();
```

Lines 145–147: an empty list; use normals/UVs only if there is one per corner.

```cpp
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            list.add(std::make_shared<Triangle>(
                positions[a], positions[b], positions[c],
                use_n ? normals[a] : Vec3(), use_n ? normals[b] : Vec3(), use_n ? normals[c] : Vec3(),
                use_uv ? uvs[a] : UV(), use_uv ? uvs[b] : UV(), use_uv ? uvs[c] : UV(),
                use_n, use_uv, mat));
        }
```

Lines 148–155: for each triangle, create a smooth `Triangle` from its three corners (with empty normals/UVs if not
available).

```cpp
        if (list.objects.empty()) return std::make_shared<HittableList>();
        return std::make_shared<BVHNode>(list);
    }
```

* Line 156: an empty mesh gives an empty list (a BVH can't be built from nothing).
* Line 157: put all triangles in a **BVH**, so even huge meshes render fast.

---

## Block G — `load_obj` (lines 161–226)

An OBJ file is text. Lines starting with `v` are positions, `vt` texture coordinates, `vn` normals, `f` faces.

```cpp
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
```

* Lines 166–167: open the file, or report an error.
* Lines 168–170: the file's own lists of positions, normals and UVs.
* Line 171: start with an empty mesh.
* Lines 172–174: whether the faces used normals/UVs, and the per-corner normals/UVs we build.

```cpp
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
```

* Line 177: read the file line by line.
* Line 178: a string stream lets us read words from the line with `>>`.
* Line 180: the first word (the tag: `v`, `vt`, `vn`, `f`, or something we ignore).

```cpp
        if (tag == "v") { Point3 p; ss >> p.x >> p.y >> p.z; P.push_back(p); }
        else if (tag == "vn") { Vec3 n; ss >> n.x >> n.y >> n.z; N.push_back(n); }
        else if (tag == "vt") { UV t; ss >> t.u >> t.v; T.push_back(t); }
```

Lines 181–183: read the numbers after the tag and store them.

```cpp
        else if (tag == "f") {
            std::vector<int> face;   // indices into mesh.positions
            std::string corner;
            while (ss >> corner) {
```

A face line looks like `f 1/1/1 2/2/2 3/3/3` (position/uv/normal numbers, counting from 1).

* Line 185: the corners of this face (as indices into our mesh).
* Line 187: read each corner word, like `2/5/7`.

```cpp
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
```

Lines 188–197: split the corner word at the `/` characters into up to 3 numbers. A missing number (as in `2//7`) stays 0.
The loop goes one step past the end (`<=`) so the last number is also stored.

```cpp
                auto fix = [](int i, size_t count) { return i < 0 ? (int)count + i : i - 1; };
                int vi = fix(idx[0], P.size());
                if (vi < 0 || vi >= (int)P.size()) continue;
                mesh.positions.push_back(P[vi]);
```

* Line 198: OBJ numbers count from **1**, and negative numbers count **from the end**. `fix` turns them into normal
  0-based indices.
* Lines 199–200: the position index; skip broken corners.
* Line 201: add a copy of the position as a new mesh corner (simple: every face corner gets its own vertex).

```cpp
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
```

* Lines 202–206: the UV for this corner, if given (checked for safety).
* Lines 207–211: the normal, if given.
* Line 212: remember this corner's index in the face.

```cpp
            for (size_t k = 1; k + 1 < face.size(); k++) {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[k]);
                mesh.indices.push_back(face[k + 1]);
            }
        }
    }
```

Lines 215–219: a face can have more than 3 corners (a square has 4). Split it into triangles like a fan from the first
corner: (0, 1, 2), (0, 2, 3), ...

```cpp
    if (any_normals) mesh.normals = out_n;
    if (any_uvs) mesh.uvs = out_t;
    std::printf("Loaded %s: %zu triangles\n", filename.c_str(), mesh.triangle_count());
    return true;
}
```

Lines 222–225: keep the normals/UVs only if the file had them; print how many triangles were loaded.

---

## Block H — `save_obj` (lines 228–250)

```cpp
inline bool save_obj(const std::string& filename, const Mesh& mesh) {
    std::ofstream f(filename);
    if (!f) return false;
    f << "# written by the pixel library\n";
    for (auto& p : mesh.positions) f << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
```

* Lines 230–232: open the file, write a comment line.
* Line 233: one `v x y z` line per corner.

```cpp
    bool has_uv = mesh.uvs.size() == mesh.positions.size();
    bool has_n = mesh.normals.size() == mesh.positions.size();
    if (has_uv) for (auto& t : mesh.uvs) f << "vt " << t.u << ' ' << t.v << '\n';
    if (has_n) for (auto& n : mesh.normals) f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
```

Lines 234–237: `vt` and `vn` lines, if the mesh has them.

```cpp
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
```

Lines 238–248: one `f` line per triangle. `+ 1` because OBJ counts from 1. Since each corner has its own position, UV and
normal with the same number, the same `id` is written in each slot.

---

## Block I — Procedural meshes (lines 252–286)

### Lines 254–269: a grid shaped by a function

```cpp
inline Mesh make_parametric_mesh(int nu, int nv, const std::function<Point3(double, double)>& fn) {
    Mesh m;
    for (int j = 0; j <= nv; j++)
        for (int i = 0; i <= nu; i++) {
            double u = (double)i / nu, v = (double)j / nv;
            m.positions.push_back(fn(u, v));
            m.uvs.push_back(UV{u, v});
        }
```

* Line 254: `fn` = any function from (u, v) in 0–1 to a 3D point.
* Lines 256–261: a grid of (nu + 1) × (nv + 1) corners: for each, call `fn` to get its 3D position, and store (u, v) as its
  texture coordinate.

```cpp
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            int a = j * (nu + 1) + i, b = a + 1, c = a + (nu + 1), d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, d, a, d, c});
        }
    m.compute_smooth_normals();
    return m;
}
```

* Lines 262–263: each grid cell.
* Line 264: its four corners: a (this one), b (right), c (below), d (below-right).
* Line 265: two triangles per cell: (a, b, d) and (a, d, c).
* Line 267: smooth normals.

### Lines 272–278: a torus (donut)

```cpp
inline Mesh make_torus(double major_radius, double minor_radius, int nu = 64, int nv = 32) {
    return make_parametric_mesh(nu, nv, [=](double u, double v) {
        double a = u * 2 * pi, b = v * 2 * pi;
        double r = major_radius + minor_radius * std::cos(b);
        return Point3(r * std::cos(a), minor_radius * std::sin(b), r * std::sin(a));
    });
}
```

* `a` = the angle around the big ring, `b` = the angle around the tube.
* Line 275: the distance from the center for this point of the tube.
* Line 276: go around the big ring by angle a, at that distance; the height comes from the tube angle.

### Lines 281–286: a terrain (heightfield)

```cpp
inline Mesh make_heightfield(double size, int resolution, const std::function<double(double, double)>& height) {
    return make_parametric_mesh(resolution, resolution, [=](double u, double v) {
        double x = (u - 0.5) * size, z = (v - 0.5) * size;
        return Point3(x, height(x, z), z);
    });
}
```

A flat square grid (from −size/2 to +size/2) where each point's height is `height(x, z)`: any terrain you can write as a
formula.

---

## Block J — End (line 288)

`} // namespace pixel`

---

## Check your understanding

1. What are the three barycentric weights of a hit exactly at corner v1? *(b0 = 0, b1 = 1, b2 = 0.)*
2. How does smooth shading work? *(The three corner normals are blended by the hit's weights.)*
3. How is a 4-corner OBJ face stored? *(As two triangles: (0,1,2) and (0,2,3).)*
4. How many triangles does a 100 × 100 heightfield have? *(100 × 100 × 2 = 20,000.)*
