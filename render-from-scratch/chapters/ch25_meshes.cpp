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
