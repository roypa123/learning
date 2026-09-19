# Chapter 21 — Multithreading and your first masterpiece

[← Positionable camera](20-positionable-camera.md) · [Contents](README.md) · [Next: Motion blur →](22-motion-blur.md)

---

## Goal

* Use **all the cores** of your processor to render several times faster.
* Understand the three rules for safe multithreaded code: split the work, don't share mutable data,
  and synchronize the rest.
* Render the famous "random spheres" image that ends *Ray Tracing in One Weekend*: your first
  masterpiece.

---

## 1. Why threads?

A modern CPU has 4–16 **cores**, each able to run code independently. A normal program uses **one**.
Rendering is *embarrassingly parallel*: every pixel is computed independently of the others. So if we
give each core its own pixels, we get almost a linear speedup: 8 cores ≈ 8× faster.

---

## 2. Splitting the work

Several strategies:

| Strategy | Idea | Problem |
|----------|------|---------|
| Fixed halves | Thread 1 renders the top half, thread 2 the bottom half | The sky is fast and the objects are slow, so one thread finishes early and waits |
| Interleaved rows | Thread k renders rows k, k+N, k+2N, ... | Better, but still uneven |
| **Work queue** | Each thread grabs the **next unrendered row** when it's free | Automatically balanced ✓ |

We use a work queue with a single shared counter:

```cpp
std::atomic<int> next_row(0);

auto worker = [&]() {
    while (true) {
        int j = next_row.fetch_add(1);     // take the next row number, and increment the counter
        if (j >= height) break;            // no rows left
        render_row(j);
    }
};
```

`std::atomic<int>` guarantees that `fetch_add` happens as one indivisible step, even if two threads call
it at the same moment. They can never get the same row. That's the whole synchronization we need for
the rendering itself.

### 2.1 Starting the threads

```cpp
int n_threads = std::thread::hardware_concurrency();    // number of cores (logical)
std::vector<std::thread> pool;
for (int t = 0; t < n_threads; t++) pool.emplace_back(worker);
for (auto& th : pool) th.join();                        // wait for all to finish
```

---

## 3. Rules for safe threads

A **data race** happens when two threads access the same memory at the same time and at least one of
them writes. The result is unpredictable. Here's how our camera avoids it:

1. **The scene is read-only during rendering.** All threads read the same spheres and materials. Reading
   together is always safe.
2. **Each thread writes only its own pixels.** Two threads never get the same row, so they never write the
   same pixel.
3. **Each thread has its own random generator** (`thread_local`, chapter 16).
4. **Shared things that do change are protected**: `rows_done` is atomic, and printing the progress
   line uses a `std::mutex` (a lock that only one thread can hold at a time), so progress messages don't
   get mixed up:

```cpp
std::lock_guard<std::mutex> lock(print_mutex);   // wait for the lock; released automatically at "}"
std::printf("\rRendering: %3d%% ...", ...);
```

### 3.1 Reproducible images

If threads grabbed random numbers from a shared sequence, the image would change every run, depending
on timing. Instead, each **row** re-seeds the thread's generator from the row number:

```cpp
seed_thread_rng(seed * 1000003ULL + (uint64_t)j * 7919ULL + 17);
```

Same row → same random numbers → same pixels, no matter which thread renders it. The image is identical
with 1 or 16 threads, and you can change `cam.seed` to get a different noise pattern.

---

## 4. The first masterpiece

The scene:

* a huge grey ground sphere,
* a 22 × 22 grid of small spheres (radius 0.2), each randomly offset, with a random material:
  80% matte (random dark-ish colors: `random × random` makes darker colors more common),
  15% metal (light colors, random fuzz), 5% glass,
* three big spheres: glass (center), matte brown (left), mirror bronze (right),
* a telephoto camera (vfov 20°) looking from far away, with slight depth of field.

It uses a `BVHNode`, a data structure from chapter 23 that makes finding the closest hit among 400+
spheres much faster. Just trust it for now.

**File: `chapters/ch21_first_masterpiece.cpp`**

```cpp
// ch21_first_masterpiece.cpp
// ------------------------------------------------------------
// Chapter 21: Multithreading and the final "one weekend" scene:
// hundreds of random small spheres + three big ones.
//   images/ch21_random_spheres.png
// Run with "final" for a bigger, cleaner image:   run ch21_first_masterpiece final
// ------------------------------------------------------------
#include <string>
#include "pixel/pixel.h"
using namespace pixel;

int main(int argc, char** argv) {
    bool final_quality = argc > 1 && std::string(argv[1]) == "final";

    HittableList world;
    auto ground_material = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    world.add(std::make_shared<Sphere>(Point3(0, -1000, 0), 1000, ground_material));

    Pcg32 rng(2024);   // fixed seed: the same scene every time
    auto rnd = [&]() { return rng.next_double(); };

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            double choose_mat = rnd();
            Point3 center(a + 0.9 * rnd(), 0.2, b + 0.9 * rnd());
            if ((center - Point3(4, 0.2, 0)).length() <= 0.9) continue;

            std::shared_ptr<Material> mat;
            if (choose_mat < 0.8) {            // 80% matte
                Color albedo(rnd() * rnd(), rnd() * rnd(), rnd() * rnd());
                mat = std::make_shared<Lambertian>(albedo);
            } else if (choose_mat < 0.95) {    // 15% metal
                Color albedo(0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd(), 0.5 + 0.5 * rnd());
                mat = std::make_shared<Metal>(albedo, 0.5 * rnd());
            } else {                           // 5% glass
                mat = std::make_shared<Dielectric>(1.5);
            }
            world.add(std::make_shared<Sphere>(center, 0.2, mat));
        }
    }
    world.add(std::make_shared<Sphere>(Point3(0, 1, 0), 1.0, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(Point3(-4, 1, 0), 1.0, std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1))));
    world.add(std::make_shared<Sphere>(Point3(4, 1, 0), 1.0, std::make_shared<Metal>(Color(0.7, 0.6, 0.5), 0.0)));

    // A BVH makes this MUCH faster (explained in chapter 23).
    BVHNode bvh(world);

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = final_quality ? 1200 : 600;
    cam.samples_per_pixel = final_quality ? 300 : 50;
    cam.max_depth = 50;
    cam.vfov = 20;
    cam.lookfrom = Point3(13, 2, 3);
    cam.lookat = Point3(0, 0, 0);
    cam.vup = Vec3(0, 1, 0);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    save_image("images/ch21_random_spheres.png", cam.render(bvh));
    return 0;
}
```

```bat
run ch21_first_masterpiece          :: 600 px wide, 50 samples: about a minute
run ch21_first_masterpiece final    :: 1200 px wide, 300 samples: 10-60 minutes depending on your CPU
```

---

## 5. What you should see

![Random spheres](../images/ch21_random_spheres.png)

> **Image description:** A wide shot of a field of hundreds of small colorful balls scattered over a
> grey plane that stretches to the horizon, under a pale blue sky. Most balls are matte in muted
> colors (dark reds, olive greens, navy, brown). Some are shiny metal reflecting their neighbors, and a few
> are clear glass. In the middle stand three large spheres: on the right a **mirror bronze** ball
> reflecting the field and sky; in the center a large **glass** ball showing an inverted,
> magnified view of the balls behind it; on the left a **matte brown** ball. The big spheres
> are in sharp focus; the small balls closest to and farthest from the camera are slightly blurred.
> Every ball sits in a soft contact shadow.

It's worth rendering the `final` version at least once. It looks like a product photograph.

---

## 6. Measuring the speedup

Try setting `cam.threads = 1` and compare the time printed at the end with the default (all cores).
Typical results on an 8-core/16-thread CPU:

| threads | time | speedup |
|---------|------|---------|
| 1 | 100 s | 1.0× |
| 4 | 26 s | 3.8× |
| 8 | 14 s | 7.1× |
| 16 (hyper-threading) | 11 s | 9× |

Hyper-threading (two "logical" threads per physical core) helps a bit, but less than real cores.

---

## Try it yourself

1. Change the seed of the scene (`Pcg32 rng(2024)`) for a different arrangement.
2. Make 50% of the small balls glass. Rendering gets slower. Why? (Glass rays bounce more times.)
3. Set `cam.seed = 2` and render twice with different seeds. The noise pattern changes but the scene doesn't.
   Average the two images pixel by pixel: that's equal to rendering once with twice the samples!
4. Remove the `BVHNode` (render `world` directly) and time it. Chapter 23 will explain the difference.

## Common problems

| Symptom | Cause |
|---------|-------|
| Crash or garbage when threading your own code | Two threads writing the same variable (e.g. a shared `std::vector::push_back`) |
| No speedup | Built without `-pthread`, or `threads = 1`, or a debug build |
| Image changes between runs | Using a shared or time-seeded random generator |
| Progress output garbled | Printing from several threads without a mutex |

---

## Summary

* Rendering is embarrassingly parallel: give each core its own rows.
* A work queue with an `std::atomic<int>` counter balances the load automatically.
* Avoid data races: shared data read-only, private outputs, per-thread random generators, a mutex for printing.
* Seeding per row makes renders reproducible.

🎉 **You've finished Part 4: you have a working ray tracer.** Everything from here on makes it faster,
richer and more realistic.

Next: [Chapter 22 — Motion blur →](22-motion-blur.md)
