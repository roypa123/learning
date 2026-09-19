# Render From Scratch: The Book

> *"Every picture you have ever seen on a screen is just a grid of numbers.
> This book teaches you how to choose those numbers."*

Welcome! This book takes you from **zero** (never having written graphics code, maybe barely
knowing C++) to rendering **movie-style images**: physically based light, soft shadows, glass,
gold, fog, sunsets, depth of field and a film-like look. All of it comes from code you write and
understand.

Nothing is hidden. We don't use any image library or graphics API. We write everything ourselves:
the PNG file format, the compression, the math, the renderer, the post-processing and even an
animated GIF encoder.

---

## How to use this book

* **Read in order.** Each chapter builds on the previous one. The early chapters are gentle; the later
  ones are more demanding, but by then you'll be ready.
* **Run every program.** Each chapter has a complete program in `chapters/`. Build and run it with
  `run <name>`, look at the image it produces, and compare it with the description in the chapter.
* **Do the "Try it yourself" exercises.** Changing a number and watching what happens teaches you more
  than any explanation.
* **Don't worry about math.** Every formula is explained in plain words, with a picture where it helps.
  If you can add, multiply and remember what a triangle is, you can follow along.

Each chapter follows the same pattern:

| Section | What it gives you |
|---------|-------------------|
| **Goal** | What you'll be able to do by the end |
| **The idea** | The concept in plain language, with diagrams |
| **The math** | Only what is needed, explained step by step |
| **The code** | Walk-through, then the complete listing |
| **Run it** | The command to type |
| **What you should see** | A description of the image, so you know it worked |
| **Try it yourself** | Experiments |
| **Common problems** | What usually goes wrong and how to fix it |

About the images: the pictures in this book are created **by you**, when you run the programs.
Until you do, the image links will show as broken, but every image also has a written description,
so you can read the book anywhere. After running the chapters, open these files in VS Code's Markdown
preview (Ctrl+Shift+V) and the images will appear in place.

---

## Table of contents

### Part 0 — Getting started
* [Chapter 0 — Introduction: how pictures are made](00-introduction.md)
* [Chapter 1 — Setting up: compiler, editor, building](01-setup.md)
* [Chapter 2 — A C++ crash course for this book](02-cpp-crash-course.md)

### Part 1 — Pixels and files
* [Chapter 3 — Your first image](03-first-image.md)
* [Chapter 4 — Color for programmers](04-color.md)
* [Chapter 5 — The Image class](05-image-class.md)
* [Chapter 6 — Writing PNG files, part 1: the container](06-png-part1.md)
* [Chapter 7 — Writing PNG files, part 2: compression](07-png-part2.md)

### Part 2 — 2D graphics
* [Chapter 8 — Drawing: pixels, rectangles, lines](08-drawing-basics.md)
* [Chapter 9 — Circles, anti-aliasing and transparency](09-circles-antialiasing.md)
* [Chapter 10 — Triangles and gradients](10-triangles-gradients.md)
* [Chapter 11 — Procedural noise: nature from math](11-procedural-noise.md)

### Part 3 — Into 3D
* [Chapter 12 — Vectors: the language of 3D](12-vectors.md)
* [Chapter 13 — Rays and a virtual camera](13-rays-and-camera.md)

### Part 4 — The ray tracer
* [Chapter 14 — Hitting a sphere](14-hitting-a-sphere.md)
* [Chapter 15 — Surface normals and many objects](15-normals-and-lists.md)
* [Chapter 16 — Random numbers and anti-aliasing](16-random-and-antialiasing.md)
* [Chapter 17 — Diffuse (matte) materials](17-diffuse-materials.md)
* [Chapter 18 — Metal](18-metal.md)
* [Chapter 19 — Glass and other dielectrics](19-glass.md)
* [Chapter 20 — A positionable camera and depth of field](20-positionable-camera.md)
* [Chapter 21 — Multithreading and your first masterpiece](21-multithreading-final-scene.md)

### Part 5 — Building real scenes
* [Chapter 22 — Motion blur](22-motion-blur.md)
* [Chapter 23 — Bounding volume hierarchies (making it fast)](23-bvh.md)
* [Chapter 24 — Textures](24-textures.md)
* [Chapter 25 — Quads, triangles and meshes](25-quads-triangles-meshes.md)
* [Chapter 26 — Lights and the Cornell box](26-lights-cornell-box.md)
* [Chapter 27 — Instances: moving and rotating things](27-instances.md)
* [Chapter 28 — Volumes: smoke, fog and clouds](28-volumes.md)

### Part 6 — Physically based rendering
* [Chapter 29 — Monte Carlo integration](29-monte-carlo.md)
* [Chapter 30 — Importance sampling](30-importance-sampling.md)
* [Chapter 31 — Light sampling and mixture PDFs](31-light-sampling.md)
* [Chapter 32 — Microfacet materials (GGX)](32-microfacet-materials.md)
* [Chapter 33 — Skies and environment lighting](33-sky-and-environment.md)

### Part 7 — Cinema
* [Chapter 34 — HDR, exposure and tone mapping](34-hdr-tonemapping.md)
* [Chapter 35 — Post-processing: the film look](35-post-processing.md)
* [Chapter 36 — Denoising](36-denoising.md)
* [Chapter 37 — Signed distance functions and ray marching](37-sdf-raymarching.md)
* [Chapter 38 — The final shot: "The Monolith at Dawn"](38-final-shot.md)
* [Chapter 39 — Animation](39-animation.md)
* [Chapter 40 — Where to go next](40-next-steps.md)

### Appendices
* [Appendix A — Library reference (every class and function)](appendix-a-library-reference.md)
* [Appendix B — Troubleshooting](appendix-b-troubleshooting.md)
* [Appendix C — Glossary](appendix-c-glossary.md)
* [Appendix D — Math cheat sheet](appendix-d-math-cheatsheet.md)
* [Appendix E — Full source code of the `pixel` library](appendix-e-full-source.md)

---

## The road ahead

```
 Part 1        Part 2          Part 3/4                 Part 5/6                    Part 7
 ┌──────┐     ┌────────┐     ┌──────────────┐     ┌────────────────────┐     ┌──────────────────┐
 │pixels│ ──▶ │ shapes │ ──▶ │ rays, spheres│ ──▶ │ meshes, lights,    │ ──▶ │ HDR, bloom, grade│
 │files │     │ noise  │     │ glass, metal │     │ physically based   │     │ denoise, film    │
 └──────┘     └────────┘     └──────────────┘     └────────────────────┘     └──────────────────┘
  gradient     poster          RTOW cover           Cornell box, GGX            "The Monolith
  BMP/PNG      landscape       random spheres       sunset skies                 at Dawn"
```

Ready? Turn the page to [Chapter 0](00-introduction.md).
