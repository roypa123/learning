# Render From Scratch

### Making images, from single pixels to movie-quality frames, in C++ with no external libraries

This project is a complete, beginner-friendly course that teaches you how computers **make pictures**,
from the first colored pixel up to a physically based, film-style render with sunlight, haze, glass,
metal, depth of field, bloom and color grading.

We build **our own library**, called `pixel`. It uses only the C++17 standard library. No OpenGL,
no image libraries, no zlib. Every byte of every PNG and GIF file is written by code you'll read and
understand.

```
render-from-scratch/
├── README.md            <- you are here
├── docs/                <- THE BOOK: start with docs/README.md
├── include/pixel/       <- our own rendering library (header-only C++)
├── chapters/            <- one complete program per chapter
├── images/              <- created when you run the programs
├── models/              <- 3D model files created by chapter 25
├── build.bat            <- build with g++ (MinGW) on Windows
├── build_msvc.bat       <- build with Visual Studio's cl.exe
├── run.bat              <- build + run one chapter:  run ch14_sphere
└── Makefile             <- for Linux / macOS / MSYS2
```

## Quick start

1. Install a C++ compiler (see [docs/01-setup.md](docs/01-setup.md), about 10 minutes).
2. Open a terminal in this folder.
3. Run your first chapter:

   ```bat
   run ch03_first_image
   ```

4. Open `images\ch03_gradient.bmp`. You just made an image from nothing.

## Read the book

Start here: **[docs/README.md](docs/README.md)**. It has the full table of contents.

| Part | Chapters | You will build |
|------|----------|----------------|
| 0. Getting started | 0–2 | Tools, and just enough C++ |
| 1. Pixels & files | 3–7 | Images, color, and our own PNG encoder |
| 2. 2D graphics | 8–11 | Lines, circles, triangles, anti-aliasing, noise landscapes |
| 3. 3D math | 12–13 | Vectors, rays, a virtual camera |
| 4. The ray tracer | 14–21 | Spheres, matte/metal/glass materials, depth of field, threads |
| 5. Real scenes | 22–28 | Motion blur, BVH, textures, meshes, lights, instancing, smoke |
| 6. Physically based | 29–33 | Monte Carlo, importance sampling, GGX materials, skies |
| 7. Cinema | 34–39 | HDR, tone mapping, bloom, grading, denoising, SDFs, the final shot, animation |

## License

Use it, change it, learn from it. It's yours.
