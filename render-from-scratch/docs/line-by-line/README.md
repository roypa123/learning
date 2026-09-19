# Line by line: every file explained

[← Back to the book](../README.md)

These pages explain the code **line by line**, in simple words. For each file:

1. a one-sentence summary of what the file does,
2. a table of its **blocks** (groups of lines that do one job),
3. every block shown again, with an explanation of **each line**,
4. a picture of the whole flow, and a few check questions.

Read a chapter of the book first (it explains the *ideas*), then read its line-by-line page (it
explains the *code*), with the source file open next to it.

## Part 1 — Pixels and files

| Code file | Line-by-line page | Book chapter |
|-----------|-------------------|--------------|
| `chapters/ch03_first_image.cpp` | [ch03_first_image.md](ch03_first_image.md) | [3](../03-first-image.md) |
| `include/pixel/vec3.h` | [vec3.md](vec3.md) | [12](../12-vectors.md) (used from ch. 4) |
| `include/pixel/color.h` | [color.md](color.md) | [4](../04-color.md) |
| `chapters/ch04_color.cpp` | [ch04_color.md](ch04_color.md) | [4](../04-color.md) |
| `include/pixel/image.h` | [image.md](image.md) | [5](../05-image-class.md) |
| `chapters/ch05_image_class.cpp` | [ch05_image_class.md](ch05_image_class.md) | [5](../05-image-class.md) |
| `chapters/ch06_png_stored.cpp` | [ch06_png_stored.md](ch06_png_stored.md) | [6](../06-png-part1.md) |
| `include/pixel/png.h` | [png.md](png.md) | [7](../07-png-part2.md) |
| `chapters/ch07_png_compressed.cpp` | [ch07_png_compressed.md](ch07_png_compressed.md) | [7](../07-png-part2.md) |

## Part 2 — 2D graphics

| Code file | Line-by-line page | Book chapter |
|-----------|-------------------|--------------|
| `include/pixel/random.h` | [random.md](random.md) | [16](../16-random-and-antialiasing.md) (used from ch. 7) |
| `include/pixel/canvas.h` | [canvas.md](canvas.md) | [8](../08-drawing-basics.md), [9](../09-circles-antialiasing.md), [10](../10-triangles-gradients.md) |
| `chapters/ch08_drawing.cpp` | [ch08_drawing.md](ch08_drawing.md) | [8](../08-drawing-basics.md) |
| `chapters/ch09_circles_antialiasing.cpp` | [ch09_circles_antialiasing.md](ch09_circles_antialiasing.md) | [9](../09-circles-antialiasing.md) |
| `chapters/ch10_triangles.cpp` | [ch10_triangles.md](ch10_triangles.md) | [10](../10-triangles-gradients.md) |
| `include/pixel/noise.h` | [noise.md](noise.md) | [11](../11-procedural-noise.md) |
| `chapters/ch11_procedural_noise.cpp` | [ch11_procedural_noise.md](ch11_procedural_noise.md) | [11](../11-procedural-noise.md) |
| `chapters/ch12_vectors.cpp` | [ch12_vectors.md](ch12_vectors.md) | [12](../12-vectors.md) |

## Parts 3 and 4 — Into 3D, and the ray tracer

| Code file | Line-by-line page | Book chapter |
|-----------|-------------------|--------------|
| `include/pixel/ray.h` | [ray.md](ray.md) | [13](../13-rays-and-camera.md), [15](../15-normals-and-lists.md) |
| `chapters/ch13_rays_sky.cpp` | [ch13_rays_sky.md](ch13_rays_sky.md) | [13](../13-rays-and-camera.md) |
| `chapters/ch14_sphere.cpp` | [ch14_sphere.md](ch14_sphere.md) | [14](../14-hitting-a-sphere.md) |
| `include/pixel/hittable.h` | [hittable.md](hittable.md) | [15](../15-normals-and-lists.md) |
| `include/pixel/sphere.h` | [sphere.md](sphere.md) | [14](../14-hitting-a-sphere.md), [15](../15-normals-and-lists.md) |
| `chapters/ch15_normals.cpp` | [ch15_normals.md](ch15_normals.md) | [15](../15-normals-and-lists.md) |
| `chapters/ch16_antialiasing.cpp` | [ch16_antialiasing.md](ch16_antialiasing.md) | [16](../16-random-and-antialiasing.md) |
| `include/pixel/camera.h` | [camera.md](camera.md) | [17](../17-diffuse-materials.md), [20](../20-positionable-camera.md), [21](../21-multithreading-final-scene.md) |
| `include/pixel/material.h` | [material.md](material.md) | [17](../17-diffuse-materials.md)–[19](../19-glass.md), [32](../32-microfacet-materials.md) |
| `include/pixel/pdf.h` | [pdf.md](pdf.md) | [30](../30-importance-sampling.md), [31](../31-light-sampling.md) |
| `include/pixel/texture.h` | [texture.md](texture.md) | [24](../24-textures.md) |
| `include/pixel/sky.h` | [sky.md](sky.md) | [33](../33-sky-and-environment.md) |
| `chapters/ch17_diffuse.cpp` | [ch17_diffuse.md](ch17_diffuse.md) | [17](../17-diffuse-materials.md) |
| `chapters/ch18_metal.cpp` | [ch18_metal.md](ch18_metal.md) | [18](../18-metal.md) |
| `chapters/ch19_glass.cpp` | [ch19_glass.md](ch19_glass.md) | [19](../19-glass.md) |
| `chapters/ch20_camera.cpp` | [ch20_camera.md](ch20_camera.md) | [20](../20-positionable-camera.md) |
| `chapters/ch21_first_masterpiece.cpp` | [ch21_first_masterpiece.md](ch21_first_masterpiece.md) | [21](../21-multithreading-final-scene.md) |

More parts will be added in the same style.
