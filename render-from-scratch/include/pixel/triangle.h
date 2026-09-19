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
