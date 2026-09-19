# Chapter 0 — Introduction: how pictures are made

[← Contents](README.md) · [Next: Setting up →](01-setup.md)

---

## Goal

By the end of this chapter you will know:

* what a digital image really is,
* the two big families of techniques that computers use to draw 3D worlds,
* why movies use a technique called **path tracing**, and
* the plan for the rest of the book.

There's no code in this chapter, just ideas. Get a cup of tea.

---

## 1. An image is a grid of numbers

Zoom far enough into any photo on your screen and you'll find tiny squares, each a single color.
These squares are **pixels** (short for *picture elements*).

```
   A 6 x 4 pixel image, zoomed in:

   ┌───┬───┬───┬───┬───┬───┐
   │ ░ │ ░ │ ▒ │ ▒ │ ▓ │ █ │   row 0
   ├───┼───┼───┼───┼───┼───┤
   │ ░ │ ▒ │ ▒ │ ▓ │ █ │ █ │   row 1
   ├───┼───┼───┼───┼───┼───┤
   │ ▒ │ ▒ │ ▓ │ █ │ █ │ █ │   row 2
   ├───┼───┼───┼───┼───┼───┤
   │ ▒ │ ▓ │ █ │ █ │ █ │ █ │   row 3
   └───┴───┴───┴───┴───┴───┘
    col0 col1 ...       col5
```

Each pixel's color is stored as **three numbers**: how much **red**, **green** and **blue** light
the screen should emit at that spot. Usually each number is between 0 and 255:

| Color | Red | Green | Blue |
|-------|-----|-------|------|
| black | 0 | 0 | 0 |
| white | 255 | 255 | 255 |
| pure red | 255 | 0 | 0 |
| yellow (red + green light) | 255 | 255 | 0 |
| a dark teal | 0 | 90 | 100 |

So a 1920 × 1080 "Full HD" image is 1920 × 1080 × 3 ≈ **6.2 million numbers**. A 4K movie frame
has about 25 million. A movie at 24 frames per second shows 600 million numbers every second.

**Making an image = deciding each of those numbers.** That's the whole job, and the rest of this
book is about good ways to decide them.

---

## 2. Two ways to decide the numbers

### 2.1 "Paint it": 2D graphics

The simplest approach is to describe shapes directly on the grid: "fill this rectangle
with blue", "draw a circle here". This is how paint programs, fonts, charts and user interfaces
work. You'll do this in **Part 2** (chapters 8–11), and it teaches a lot of useful ideas:
anti-aliasing, blending, gradients, noise.

### 2.2 "Simulate it": 3D rendering

For 3D worlds we describe a *scene* (objects, materials, lights, a camera) and let the computer work
out what the camera sees. There are two main strategies:

**Rasterization** (used by video games): take each triangle of each object, work out which pixels it
covers on screen, and color them. It's extremely fast, and graphics cards are built for it. But light
bouncing between objects, soft shadows, reflections and glass all need clever tricks.

**Ray tracing** (used by movies): for each pixel, shoot a *ray* (a line) from the camera through that
pixel into the scene and ask *"what does this ray hit, and how much light comes back along it?"*

```
            camera                  image plane (the pixels)               scene
              ●  ─────────────────────▶ ▪ ─────────────────────────────▶  ◯  sphere
              │ \                       ▪                                   ╲
              │   \                     ▪                                    ╲  light bounces
              │     \ ──────────────────▶ ▪ ───────────────▶  ▭  box           ╲  toward the sun ☀
              │                         ▪
```

Ray tracing copies how light really works (only backwards: we follow light from the eye back to
its source, which saves wasting effort on light that never reaches the camera). Because it is a
*simulation*, effects like shadows, reflections, refraction and soft indirect light happen by
themselves once the basic physics is right.

### 2.3 Path tracing: what the movies use

A ray tracer that bounces rays **randomly** many times, the way real light scatters off rough
surfaces, and averages the results is called a **path tracer**. Since roughly 2010 almost every
animated feature film and visual-effects shot has been rendered with one (Pixar's RenderMan,
Disney's Hyperion, Weta's Manuka, Arnold, Cycles, V-Ray...).

Path tracing is surprisingly simple to write. The core loop fits on one screen (you'll see ours in
chapter 17). What makes film renderers huge is everything around the core: speed, huge scenes, complex
materials, artistic control. We'll build a compact version of each of those.

---

## 3. What "movie quality" really means

When people say an image looks "cinematic" they usually mean a combination of:

1. **Physically plausible light**: light bounces, colors bleed from one surface onto another, shadows
   are soft where the light source is big.
2. **Believable materials**: metals reflect their surroundings, glass bends light, rough things look
   rough.
3. **Photographic camera effects**: depth of field (blurry background), motion blur, lens glow
   (bloom), darker corners (vignette), film grain.
4. **High dynamic range and tone mapping**: the sun is thousands of times brighter than a shadow,
   and a filmic curve squeezes that range onto a screen gracefully.
5. **Color grading**: an intentional palette (the famous "teal and orange" of blockbusters).
6. **Composition**: where you place the camera, the objects and the light.

We will build **every one** of these, in that order of difficulty. Here's the kind of image you'll
make in [chapter 38](38-final-shot.md):

![The final shot](../images/ch38_final_preview.png)

> **Image description — "The Monolith at Dawn":** A wide cinemascope frame. A low golden sun
> sits just above distant hazy mountains, casting long shadows across rolling sand dunes. In the
> middle stands a tall, glossy black slab, floating slightly above the ground, its surface reflecting
> the orange sky. Three small glowing orbs float around it. In the foreground, a shallow lake mirrors
> the sky, and a lone human silhouette stands on a rock in the water, looking at the monolith. The air
> is filled with warm haze; bright areas glow softly; the corners are slightly darkened and a fine
> film grain covers the frame.

---

## 4. The plan

| Part | What you build | Key ideas |
|------|----------------|-----------|
| **1. Pixels & files** | Images in memory; BMP, PPM and PNG writers (with our own compression) | pixels, RGB, gamma, bytes, checksums, LZ77, Huffman codes |
| **2. 2D graphics** | A drawing canvas; a poster and a landscape made only with code | lines, circles, triangles, anti-aliasing, alpha blending, noise |
| **3. 3D math** | A vector class; a virtual camera | vectors, dot/cross products, rays |
| **4. Ray tracer** | Spheres with matte, metal and glass materials; a movable camera with a lens | intersections, normals, random sampling, reflection, refraction |
| **5. Real scenes** | Fast rendering of thousands of objects; textures; triangle meshes; lights; smoke | BVH, UV mapping, OBJ files, instancing, volumes |
| **6. Physically based** | A noise-free Cornell box; realistic metals and plastics; skies | Monte Carlo, importance sampling, microfacets, Fresnel |
| **7. Cinema** | A post-production pipeline, a denoiser, fractals, a finished film frame, an animation | HDR, ACES, bloom, grading, SDFs, GIF encoding |

### The library we build: `pixel`

Everything reusable goes into a folder of header files, `include/pixel/`. By the end it contains:

```
vec3.h      random.h     color.h      png.h       image.h     canvas.h    noise.h
ray.h       aabb.h       hittable.h   sphere.h    quad.h      triangle.h  bvh.h
instance.h  volume.h     sdf.h        texture.h   material.h  pdf.h       sky.h
camera.h    post.h       gif.h        pixel.h (includes everything)
```

Each chapter explains the parts it introduces. [Appendix A](appendix-a-library-reference.md) is a
reference for all of it, and [Appendix E](appendix-e-full-source.md) prints every line.

### What you need to know already

* How to use a computer and a text editor.
* A little programming: variables, `if`, loops, functions. If you have never programmed at all,
  [chapter 2](02-cpp-crash-course.md) is a crash course with everything this book uses.
* School math: + − × ÷, squares and square roots, and what sine and cosine are (roughly). Everything
  else is explained when we need it.

### How long will it take?

Reading plus experimenting, about **one chapter per evening**. Parts 1–4 are a great weekend
project; parts 5–7 are a few more weeks. Rendering times go from a fraction of a second (chapter 3)
up to hours (the "final" setting of chapter 38) — but every chapter also has a quick setting.

---

## A word of encouragement

Graphics programming is one of the most rewarding kinds of programming, because **every result is
a picture**. When something goes wrong you can *see* it: a black image, a pink sphere, a shadow
full of dots. Those broken images aren't failures, they're clues, and some of the chapters show you
broken images on purpose so you learn to read them.

Let's start. First, [set up your tools →](01-setup.md)
