# Line by line: `include/pixel/camera.h`

[← Line-by-line index](README.md) · Chapters [17](../17-diffuse-materials.md), [20](../20-positionable-camera.md), [21](../21-multithreading-final-scene.md), [31](../31-light-sampling.md), [36](../36-denoising.md)

**What this file does, in one sentence:** the `Camera` makes a ray for every sample of every pixel, follows each ray as
it bounces around the scene (the **path tracer**), averages the results, and does all of this on every CPU core at once.

This is **the heart of the renderer**. It's long, so take it one block at a time. Features marked with a chapter
number are explained in that chapter; you can skip them on first reading.

| Block | Lines | Job | Chapter |
|-------|-------|-----|---------|
| A. Comments, includes | 1–31 | | |
| B. Settings (public fields) | 33–68 | everything you can change | 17, 20, 21, 31, 36 |
| C. `image_height` | 70 | | 13 |
| D. `render`: preparation | 72–90 | set up image, lights, threads | 21 |
| E. `render`: the worker | 92–123 | what each thread does: rows of pixels | 21 |
| F. `render`: start threads, finish | 125–134 | | 21 |
| G. `trace`: start | 136–150 | the path tracer; a ray that hits nothing | 17 |
| H. `trace`: emission and scattering | 151–183 | what happens at a surface | 17, 31 |
| I. `trace`: Russian roulette and clean-up | 185–200 | | 31 |
| J. `initialize` | 202–232 | build the camera's coordinate system and viewport | 13, 20 |
| K. `get_ray` | 234–251 | the ray for one sample of one pixel | 16, 20, 22 |
| L. Private data | 253–267 | | |
| M. End | 269 | | |

---

## Block A — Comments, includes (lines 1–31)

* Lines 1–13: a list of everything the camera does, with the chapter that explains each part.
* Line 14: `#pragma once`.
* Lines 15–21: standard tools:

| Line | Include | Used for |
|------|---------|----------|
| 15 | `<atomic>` | a counter many threads can safely increase |
| 16 | `<chrono>` | measuring time |
| 17 | `<cmath>` | tan, sqrt |
| 18 | `<cstdio>` | printf |
| 19 | `<mutex>` | a lock so only one thread prints at a time |
| 20 | `<thread>` | running code on several cores |
| 21 | `<vector>` | the list of threads |

* Lines 22–29: our files: vectors, rays, random numbers, hittables, materials, PDFs, images, skies.
* Line 31: `namespace pixel`.

---

## Block B — Settings (lines 33–68)

```cpp
class Camera {
public:
```

Line 33: the Camera class. Everything under `public:` can be set by your program, like `cam.image_width = 800;`. Each
setting has a sensible default value.

### Image settings (lines 35–39)

| Line | Setting | Default | Meaning |
|------|---------|---------|---------|
| 36 | `aspect_ratio` | 16/9 | width ÷ height |
| 37 | `image_width` | 400 | pixels across |
| 38 | `samples_per_pixel` | 10 | rays per pixel: more = less noise, but slower |
| 39 | `max_depth` | 10 | maximum number of bounces per ray |

### Lens and position (lines 41–48), chapter 20

| Line | Setting | Meaning |
|------|---------|---------|
| 42 | `vfov` | vertical field of view in degrees: big = wide angle, small = zoom |
| 43 | `lookfrom` | where the camera is |
| 44 | `lookat` | the point it looks at |
| 45 | `vup` | which way is "up" (to keep the camera level) |
| 46 | `defocus_angle` | lens opening for background blur; 0 = everything sharp |
| 47 | `focus_dist` | the distance that is perfectly sharp |
| 48 | `shift_x`, `shift_y` | slide the image window (rarely used) |

### Lighting (line 51)

`background` = what a ray sees when it hits nothing. It's a **function** (`Background`, from `sky.h`) that takes a ray
and returns a color. Default: the blue-white gradient sky.

### Quality and speed (lines 53–60)

| Line | Setting | Meaning | Chapter |
|------|---------|---------|---------|
| 54 | `threads` | how many threads; 0 = one per CPU core | 21 |
| 55 | `show_progress` | print "Rendering: 45%" while working | 21 |
| 56 | `russian_roulette` | stop weak rays early, at random, to save time | 31 |
| 57 | `rr_start_depth` | only after this many bounces | 31 |
| 58 | `max_sample_value` | if > 0, limit extremely bright samples (removes "fireflies") | 31 |
| 59 | `seed` | random seed: change it for a different noise pattern | 21 |
| 60 | `t_min` | ignore hits closer than this (prevents "shadow acne") | 17 |

### Extra outputs (lines 62–68), chapter 36

* Line 63: `collect_aovs`: if true, also make two helper images for the denoiser.
* Lines 64–65: those images: the surface **color** (albedo) and **normal** seen by each pixel.
* Line 68: `AovSlot`: a small struct to carry one sample's albedo and normal.

---

## Block C — `image_height` (line 70)

```cpp
    int image_height() const { int h = (int)(image_width / aspect_ratio); return h < 1 ? 1 : h; }
```

Height = width ÷ aspect ratio, but at least 1 pixel.

---

## Block D — `render`: preparation (lines 72–90)

```cpp
    Image render(const Hittable& world, const Hittable* lights = nullptr) {
        initialize();
        Image img(image_width, height);
```

* Line 73: the main function you call: `Image img = cam.render(world);`. `lights` is an optional pointer to a list of light
  shapes (chapter 31); `nullptr` = none.
* Line 74: compute the viewport and all helper values from the settings (block J).
* Line 75: the output image.

```cpp
        if (collect_aovs) {
            albedo_aov = Image(image_width, height);
            normal_aov = Image(image_width, height);
        }
```

Lines 76–79: create the helper images only if asked.

```cpp
        light_list = lights;
        if (auto list = dynamic_cast<const HittableList*>(lights))
            if (list->objects.empty()) light_list = nullptr;
```

* Line 80: remember the lights.
* Lines 81–82: if the lights are a `HittableList` that is **empty**, treat it as "no lights". `dynamic_cast` checks at
  run time whether the object really is a HittableList (it gives `nullptr` if not). Written inside the `if`, it
  creates `list` and tests it in one step.

```cpp
        int n_threads = threads > 0 ? threads : (int)std::thread::hardware_concurrency();
        if (n_threads < 1) n_threads = 1;
```

* Line 84: how many threads: your setting, or the number of CPU cores (`hardware_concurrency`).
* Line 85: at least 1 (it can return 0 if unknown).

```cpp
        std::atomic<int> next_row(0);
        std::atomic<int> rows_done(0);
        std::mutex print_mutex;
        auto start_time = std::chrono::steady_clock::now();
```

* Line 87: `next_row` = the next row that still needs rendering. **atomic** = safe to change from many threads at the same
  time.
* Line 88: how many rows are finished (for the progress display).
* Line 89: a lock for printing.
* Line 90: the start time.

---

## Block E — `render`: the worker (lines 92–123)

Each thread runs this function. It keeps taking the next free row until there are none left.

```cpp
        auto worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1);
                if (j >= height) break;
```

* Line 92: `worker` is a lambda (a small function). `[&]` = it can use all the variables above (image, counters...).
* Line 93: repeat.
* Line 94: `fetch_add(1)` = "give me the current value and increase it by 1", as **one** unbreakable step. So two
  threads can never get the same row number.
* Line 95: all rows taken? This thread is done.

```cpp
                seed_thread_rng(seed * 1000003ULL + (uint64_t)j * 7919ULL + 17);
```

Line 97: re-seed this thread's random generator **from the row number** (mixed with big primes). The same row always
gets the same random numbers, so the picture is identical however many threads you use.

```cpp
                for (int i = 0; i < image_width; i++) {
                    Color pixel_color(0, 0, 0);
                    Color albedo_sum(0, 0, 0);
                    Vec3 normal_sum(0, 0, 0);
```

* Line 98: every pixel in this row.
* Lines 99–101: sums for the pixel's color and the helper images.

```cpp
                    for (int s = 0; s < samples_per_pixel; s++) {
                        Ray r = get_ray(i, j, s);
                        AovSlot slot;
                        pixel_color += trace(r, world, collect_aovs ? &slot : nullptr);
                        albedo_sum += slot.albedo;
                        normal_sum += slot.normal;
                    }
```

* Line 102: for each sample.
* Line 103: make the ray for pixel (i, j), sample s (block K).
* Line 104: an empty slot for this sample's albedo and normal.
* Line 105: **trace** the ray: follow it through the scene and get the light it brings back (block G). Add it up. If
  helper images are wanted, pass the slot's address (`&slot`) so `trace` can fill it; otherwise `nullptr`.
* Lines 106–107: add the slot's values (zero if not used).

```cpp
                    img.at(i, j) = pixel_color / samples_per_pixel;
                    if (collect_aovs) {
                        albedo_aov.at(i, j) = albedo_sum / samples_per_pixel;
                        normal_aov.at(i, j) = normal_sum / samples_per_pixel;
                    }
                }
```

Lines 109–113: store the **averages**. Each thread writes only its own rows, so threads never write the same pixel.

```cpp
                int done = ++rows_done;
                if (show_progress) {
                    std::lock_guard<std::mutex> lock(print_mutex);
                    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                    std::printf("\rRendering: %3d%%  (%d/%d rows, %.1fs)   ", 100 * done / height, done, height, secs);
                    std::fflush(stdout);
                }
            }
        };
```

* Line 115: one more row finished (`++` on an atomic is also safe).
* Line 117: take the print lock. `lock_guard` locks now and **unlocks automatically** at the closing `}`. Other threads
  wait here, so two messages never get mixed.
* Line 118: seconds since the start.
* Line 119: print the progress. `\r` = go back to the start of the line, so the same line is updated instead of
  printing hundreds of lines.
* Line 120: `fflush` = show it now (don't wait).
* Line 123: end of the worker lambda.

---

## Block F — `render`: start threads, finish (lines 125–134)

```cpp
        std::vector<std::thread> pool;
        for (int t = 0; t < n_threads; t++) pool.emplace_back(worker);
        for (auto& th : pool) th.join();
```

* Line 125: a list of threads.
* Line 126: start `n_threads` threads, each running `worker`. `emplace_back` creates the thread directly in the list.
  From this moment they all work at the same time.
* Line 127: `join` = wait until each thread has finished.

```cpp
        if (show_progress) {
            double secs = ...;
            std::printf("\rRendering: done in %.1f seconds using %d threads.            \n", secs, n_threads);
        }
        return img;
    }
```

Lines 129–133: print the total time and return the finished image.

---

## Block G — `trace`: start (lines 136–150)

`trace` follows **one** ray through the scene, bounce after bounce, and returns the light that comes back along it.

```cpp
    Color trace(const Ray& start_ray, const Hittable& world, AovSlot* aov = nullptr) const {
        Color radiance(0, 0, 0);
        Color throughput(1, 1, 1);     // how much light survives the bounces so far
        Ray ray = start_ray;
```

* Line 139: `radiance` = the light collected so far (starts at 0).
* Line 140: `throughput` = how much of any light found **from now on** will reach the camera. It starts at 100% (1, 1, 1)
  and shrinks at every bounce (a red surface keeps mostly red, and so on).
* Line 141: the current ray (it changes at each bounce).

```cpp
        for (int depth = 0; depth < max_depth; depth++) {
            HitRecord rec;
            if (!world.hit(ray, Interval(t_min, infinity), rec)) {
                Color bg = background(ray);
                radiance += throughput * bg;
                if (aov && depth == 0) { aov->albedo = bg; aov->normal = Vec3(0, 0, 0); }
                break;
            }
```

* Line 143: one round per bounce, at most `max_depth`.
* Line 145: find the nearest hit farther than `t_min`.
* If **nothing** is hit, the ray escapes to the sky:
  * Line 146: the sky's color in this direction.
  * Line 147: add it, **filtered by the throughput** (the surfaces it bounced off earlier).
  * Line 148: for the first ray, record the sky as the helper values.
  * Line 149: `break` = stop bouncing.

---

## Block H — `trace`: emission and scattering (lines 151–183)

```cpp
            if (!rec.mat) break;   // an object without a material: treat as black
            if (aov && depth == 0) { aov->albedo = rec.mat->aov_albedo(rec); aov->normal = rec.normal; }
```

* Line 151: safety: an object with no material stops the ray.
* Line 152: first hit: record the surface color and normal for the denoiser.

```cpp
            radiance += throughput * rec.mat->emitted(ray, rec, rec.u, rec.v, rec.p);
```

Line 155: **1. Emission.** If the surface glows (a lamp), add its light. Normal surfaces return black here.

```cpp
            ScatterRecord srec;
            if (!rec.mat->scatter(ray, rec, srec)) break;
```

Lines 158–159: **2. Scattering.** Ask the material what happens to light here (see [material.md](material.md)). It
fills `srec`. If it returns false, the light is **absorbed**: stop.

```cpp
            if (srec.skip_pdf) {
                throughput *= srec.attenuation;
                ray = srec.skip_pdf_ray;
```

Lines 161–164: **mirror-like** materials (metal, glass) decide the new direction themselves:

* Line 163: multiply the throughput by the surface's color (the `attenuation`).
* Line 164: continue with the new ray the material gave us.

```cpp
            } else {
                Vec3 dir;
                double pdf_val;
                if (light_list) {
                    HittablePDF light_pdf(*light_list, rec.p);
                    MixturePDF mix(&light_pdf, srec.pdf_ptr.get());
                    dir = mix.generate();
                    pdf_val = mix.value(dir);
                } else {
                    dir = srec.pdf_ptr->generate();
                    pdf_val = srec.pdf_ptr->value(dir);
                }
```

Lines 165–177: **matte-like** materials: we choose a random new direction ourselves.

* Line 169: if we have a lights list (chapter 31)...
  * Line 170: a way to pick directions **towards the lights**,
  * Line 171: mixed 50/50 with the material's own way of picking directions,
  * Line 172: pick one direction,
  * Line 173: and ask how likely that direction was (`pdf_val`).
* Lines 174–177: otherwise, just use the material's own way (for matte: cosine-weighted directions).

```cpp
                if (pdf_val <= 1e-12) break;
                Ray scattered(rec.p, dir, ray.time());
                double scattering_pdf = rec.mat->scattering_pdf(ray, rec, scattered);
                throughput *= srec.attenuation * (scattering_pdf / pdf_val);
                ray = scattered;
            }
```

* Line 178: if the chance is (almost) 0, we can't divide by it: stop.
* Line 179: the new ray: from the hit point, in the chosen direction, at the same time.
* Line 180: how much the **material** would send light in that direction.
* Line 181: update the throughput: surface color × (material's value ÷ how likely we picked this direction). This is the
  Monte Carlo weight (chapter 30). For a plain matte surface without lights, the two numbers are equal and the ratio is
  1, so this is just `throughput *= color`.
* Line 182: continue with the new ray.

---

## Block I — `trace`: Russian roulette and clean-up (lines 185–200)

```cpp
            if (russian_roulette && depth >= rr_start_depth) {
                double p = clampd(throughput.max_component(), 0.05, 0.95);
                if (random_double() > p) break;
                throughput /= p;
            }
        }
```

**Russian roulette** (chapter 31): after a few bounces, rays carry little light. Instead of tracing them to the end:

* Line 187: the survival chance `p` = how strong the ray still is (between 5% and 95%).
* Line 188: stop the ray at random with chance 1 − p.
* Line 189: rays that survive count **more** (÷ p). On average, the result stays exactly the same, but we save lots of work.
* Line 191: end of the bounce loop.

```cpp
        if (max_sample_value > 0.0) {
            double m = radiance.max_component();
            if (m > max_sample_value) radiance *= max_sample_value / m;
        }
```

Lines 193–196: firefly clamp: if enabled and this sample is extremely bright, scale it down (keeping its color).

```cpp
        if (radiance.x != radiance.x || radiance.y != radiance.y || radiance.z != radiance.z)
            return Color(0, 0, 0);   // drop NaNs
        return radiance;
    }
```

* Lines 197–198: if any part is NaN (the result of invalid math), throw the sample away, so one bad sample can't spoil a
  pixel.
* Line 199: return the collected light.

---

## Block J — `initialize` (lines 202–232)

Builds the virtual camera from the settings (the full explanation is in chapter 20).

```cpp
    void initialize() {
        height = image_height();
        center = lookfrom;
```

Lines 204–205: the image height, and the camera's position.

```cpp
        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        double viewport_height = 2 * h * focus_dist;
        double viewport_width = viewport_height * ((double)image_width / height);
```

* Line 207: the field of view in radians.
* Line 208: `tan(half the angle)` = half the window's height at distance 1.
* Line 209: the window sits at the **focus distance**, so its height is `2 × h × focus_dist`.
* Line 210: its width matches the image shape.

```cpp
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);
```

The camera's own three axes:

* Line 213: `w` points **backwards** (from what we look at, to the camera).
* Line 214: `u` = right: perpendicular to "up" and "backwards" (a cross product).
* Line 215: `v` = the camera's true up: perpendicular to both.

```cpp
        Vec3 viewport_u = viewport_width * u;      // across the top edge
        Vec3 viewport_v = viewport_height * -v;    // down the left edge
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / height;
```

Lines 217–220: the window's edges and the step between pixels, as in [chapter 13](ch13_rays_sky.md), but using the
camera's axes u and v.

```cpp
        Point3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2
                                   + shift_x * viewport_u - shift_y * viewport_v;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
```

* Lines 222–223: the window's upper-left corner: go forward (−w) by the focus distance, then left and up by half the
  window. Plus the optional shift.
* Line 224: the center of the first pixel.

```cpp
        double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
```

Lines 226–228: the size of the **lens** (for depth of field): its radius, and two vectors spanning the lens disk.

```cpp
        sqrt_spp = (int)std::sqrt((double)samples_per_pixel);
        if (sqrt_spp < 1) sqrt_spp = 1;
    }
```

Lines 230–231: for stratified sampling (block K): the grid size per pixel. For 64 samples: 8 × 8.

---

## Block K — `get_ray` (lines 234–251)

```cpp
    Ray get_ray(int i, int j, int s) const {
        double ox, oy;
        if (s < sqrt_spp * sqrt_spp) {
            int si = s % sqrt_spp, sj = s / sqrt_spp;
            ox = (si + random_double()) / sqrt_spp - 0.5;
            oy = (sj + random_double()) / sqrt_spp - 0.5;
        } else {
            ox = random_double() - 0.5;
            oy = random_double() - 0.5;
        }
```

The random position inside the pixel for sample `s`:

* Line 237: while `s` fits in the grid (e.g. the first 64 of 64 samples)...
* Line 239: this sample's grid cell: column `s % n` and row `s / n`.
* Lines 240–241: a random point **inside that cell**, as an offset −0.5..0.5 from the pixel center. So samples are spread
  evenly over the pixel (**stratified sampling**, chapter 16), with no clumps.
* Lines 243–244: extra samples (if the count isn't a perfect square) just go anywhere in the pixel.

```cpp
        Point3 pixel_sample = pixel00_loc + ((i + ox) * pixel_delta_u) + ((j + oy) * pixel_delta_v);
        Point3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        Vec3 ray_direction = pixel_sample - ray_origin;
        double ray_time = random_double();       // for motion blur: shutter open during [0,1)
        return Ray(ray_origin, ray_direction, ray_time);
    }
```

* Line 246: the 3D point on the window for this sample.
* Line 247: where the ray starts: the camera center, or a random point on the lens (for depth of field).
* Line 248: aim from there at the window point.
* Line 249: a random moment during the shutter time (for motion blur, chapter 22).
* Line 250: return the ray.

---

## Block L — Private data (lines 253–267)

```cpp
private:
    int height = 1;
    Point3 center;
    Point3 pixel00_loc;
    Vec3 pixel_delta_u, pixel_delta_v;
    Vec3 u, v, w;
    Vec3 defocus_disk_u, defocus_disk_v;
    int sqrt_spp = 1;
    const Hittable* light_list = nullptr;
```

Values computed by `initialize` and `render`, used internally.

```cpp
    Point3 defocus_disk_sample() const {
        Vec3 p = random_in_unit_disk();
        return center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
    }
};
```

Lines 263–266: a random point on the lens: a random point in a unit disk, scaled by the lens vectors and placed around
the camera center.

---

## Block M — End (line 269)

`} // namespace pixel`

---

## The whole flow

```
render(world)
  initialize()                      camera axes, window, lens
  start N threads, each:
     take next row j
       seed random numbers from j
       for each pixel i:
          for each sample s:
             ray = get_ray(i, j, s)          stratified point + lens + time
             color += trace(ray)
                loop over bounces:
                   miss   → add throughput × sky, stop
                   hit    → add throughput × emission
                            scatter? no → stop
                            mirror  → throughput *= color, follow its ray
                            matte   → pick a direction (maybe toward lights),
                                      throughput *= color × pdf ratio
                            roulette may stop weak rays
          pixel = color / samples
  wait for all threads, return image
```

## Check your understanding

1. Why can't two threads render the same row? *(`fetch_add` on an atomic counter hands out each number once.)*
2. What is `throughput` at the start of `trace`? *((1, 1, 1): nothing has been absorbed yet.)*
3. When does `trace` stop? *(The ray misses everything, the material absorbs it, `max_depth` is reached, or Russian
   roulette stops it.)*
4. What makes the background blurry when `defocus_angle` > 0? *(Rays start from random points on the lens.)*
