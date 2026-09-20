# Chapter 32 — Microfacet materials (GGX)

[← Light sampling](31-light-sampling.md) · [Contents](README.md) · [Next: Skies →](33-sky-and-environment.md)

> 📖 **Line by line:** [ch32_microfacets explained line by line](line-by-line/ch32_microfacets.md)

---

## Goal

Replace our "fuzzy metal" trick with the **physically based** material model used by nearly every film
studio and game engine today (Disney, Pixar, Unreal, Unity, Blender...). You'll learn:

* **microfacet theory**: rough surfaces as billions of tiny mirrors,
* the **GGX** (Trowbridge–Reitz) distribution and what **roughness** really means,
* **Fresnel** reflectance for metals (colored F0) and non-metals (F0 ≈ 0.04),
* **masking-shadowing** (Smith G), which keeps rough surfaces from gaining or losing energy,
* **sampling visible normals** (VNDF) for low noise,
* a two-layer **plastic** material: colored diffuse base + clear glossy coat,
* how to make a **material chart**, the reference image every look-development artist uses.

This chapter has more math than most. Skim the formulas if you like; the ideas and the pictures are what
matter.

---

## 1. Microfacet theory

Zoom into a "rough" metal surface with a microscope and you'll see a landscape of tiny, perfectly smooth
facets tilted in random directions:

```
 smooth (roughness 0)          rough (roughness 0.5)
 ─────────────────────         ╱╲╱‾╲_╱╲╱‾‾╲╱╲_╱╲
   all facets face up           facets tilted every which way
   -> a sharp mirror image      -> reflections spread into a blurry highlight
```

Each microfacet is a perfect mirror. The **roughness** controls how widely the facet normals are spread.
A rough surface's appearance is the **statistical average** of its microfacets. Three pieces describe
that average:

| Piece | Name | Answers |
|-------|------|---------|
| **D** | normal distribution function (NDF) | how many facets point in each direction? |
| **F** | Fresnel | how much light does one facet reflect at this angle? |
| **G** | masking-shadowing | how many facets are hidden by other facets? |

Put together, the **Cook–Torrance** microfacet BRDF is:

```
            D(h) · F(v·h) · G(l, v)
f(l, v) = ───────────────────────────        h = the "half vector" between l and v
              4 · (n·l) · (n·v)
```

Don't worry about memorizing it. We'll never evaluate it directly, because sampling does the work for us
(section 5).

---

## 2. D: the GGX distribution

The GGX distribution (Walter et al. 2007, originally Trowbridge & Reitz 1975) is:

```
D(m) = α² / ( π · ((n·m)² · (α² − 1) + 1)² )
```

with **α = roughness²** (squaring the artist's roughness slider makes it feel perceptually linear: 0.5
looks like "half rough").

```
 D
 │█                   α small (0.05): a sharp spike, almost all facets face up -> mirror
 │█
 │██
 │███▄▄▄___           α large (0.5): wide and low, facets in many directions -> blurry
 └────────────── angle from the normal
```

GGX's special feature is its **long tail**: even at sharp-looking settings, a few facets point far off,
so highlights have a soft glow around a bright core. Real materials look like that, and older models
(Phong, Beckmann) didn't.

---

## 3. F: Fresnel

Chapter 19 introduced Fresnel for glass. For opaque materials it's the same idea: reflectance is **F0**
head-on and rises to 100% at grazing angles:

```
F(θ) = F0 + (1 − F0)(1 − cos θ)⁵          (Schlick)
```

The key insight of physically based rendering: **metals and non-metals have very different F0**.

| Material | F0 (linear RGB) | Notes |
|----------|-----------------|-------|
| water | 0.02 | |
| plastic, glass, paint, skin | 0.04 | almost all non-metals are 0.02–0.05, colorless |
| diamond | 0.17 | |
| iron | (0.56, 0.57, 0.58) | |
| aluminium | (0.91, 0.92, 0.92) | |
| silver | (0.95, 0.93, 0.88) | |
| gold | (1.00, 0.77, 0.34) | colored! |
| copper | (0.95, 0.64, 0.54) | colored! |

* **Metals**: high, *colored* F0, and no diffuse part at all (light that isn't reflected is absorbed).
  The color of gold *is* its reflection.
* **Non-metals** ("dielectrics"): a weak, *white* reflection (4%) on top of a colored diffuse body. The
  red of a red plastic ball comes from the diffuse part; its highlight is white.

This is the famous "metalness" workflow of modern game engines.

---

## 4. G: masking and shadowing

On a rough surface, some microfacets are **hidden** from the viewer by other facets (masking), and some
are **in the shadow** of other facets relative to the light (shadowing):

```
  viewer ↘            light ↙
       ╱╲  ╱╲  ╱╲             some facet slopes are blocked by neighboring bumps
      ╱  ╲╱  ╲╱  ╲
```

The **Smith** model computes this from a function Λ (lambda):

```
Λ(v) = ( −1 + sqrt(1 + α² · tan²θ_v) ) / 2
G1(v) = 1 / (1 + Λ(v))                 masking for one direction
G2(l, v) = 1 / (1 + Λ(l) + Λ(v))       masking + shadowing together
```

G matters most for **rough** surfaces at **grazing** angles. Without it, rough metals glow too brightly
at the edges.

---

## 5. Sampling: the visible normal distribution (VNDF)

How do we pick a bounce direction for a rough metal? The best strategy (Heitz, 2018) is:

1. Pick a random **microfacet normal m** among the facets **visible from the viewer** (with distribution
   `G1(v) · max(0, v·m) · D(m) / cos θ_v`).
2. **Reflect** the view direction around that facet: `l = 2(v·m)m − v`.

The algorithm does this geometrically: it stretches the view direction by α (turning the rough
ellipsoid of facets into a hemisphere), samples a point on the visible half of a disk, and un-stretches.
The code is `ggx::sample_vndf`. It's short but dense, so read the comments alongside the paper
("Sampling the GGX Distribution of Visible Normals", JCGT 2018) if you want every detail.

The beautiful part: with this sampling, the Monte Carlo weight `f · cos θ_l / p` simplifies to

```
weight = F(v·m) · G2(l, v) / G1(v)
```

No D, no denominators. Just Fresnel times a masking ratio between 0 and 1. That's `ggx::sample_reflection`,
and `RoughMetal::scatter`:

```cpp
ONB frame(rec.normal);
Vec3 wi = frame.to_local(-unit_vector(r_in.direction()));   // towards the viewer, in the local frame
Vec3 wo, m;
double w;
if (!ggx::sample_reflection(wi, alpha, wo, m, w)) return false;   // went below the surface: absorbed
srec.attenuation = schlick(dot(wi, m), f0) * w;
srec.skip_pdf = true;                                             // direction fully decided here
srec.skip_pdf_ray = Ray(rec.p, frame.transform(wo), r_in.time());
```

Because the material samples itself so well, we mark it `skip_pdf` like the mirror metal. (A full
production renderer would *also* use light sampling for glossy surfaces with MIS weights. That's a natural
next step for our renderer, see chapter 40.)

### 5.1 Energy loss at high roughness

Single-scattering microfacet models lose some energy on very rough surfaces: light that bounces twice
between facets is ignored. So rough metals look slightly too dark. Film renderers add a "multiple
scattering" compensation term. Our version keeps it simple.

---

## 6. Plastic: two layers

A plastic, painted or varnished object has two layers:

```
        ╲ ↗ white glossy reflection (clear coat, F0 = 0.04, GGX)
   ═════════════════════════════  coat
        ↘ light that enters the coat...
   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  colored diffuse base (Lambertian)
        ...scatters in the colored base and comes back out
```

The `Plastic` material picks **one layer at random** per bounce, as with glass in chapter 19:

1. Compute the coat reflectance `F = schlick(cos θ, 0.04)`.
2. Choose the coat with probability `p_spec = max(F, 0.2)`. (We choose it a bit more often than F, because
   highlights are small and bright, and sampling them more often reduces noise. Then we correct the weight.)
3. **Coat**: sample GGX as above, weight `F_m · G2/G1 / p_spec`, white.
4. **Base**: Lambertian with albedo `(1 − F) / (1 − p_spec)` × color, sampled with the cosine PDF (so light
   sampling works for it).

Dividing by the choice probability keeps the estimate unbiased, exactly like Russian roulette.

---

## 7. The program: a material chart

**File: `chapters/ch32_microfacets.cpp`**

```cpp
// ch32_microfacets.cpp
// ------------------------------------------------------------
// Chapter 32: Physically based materials with microfacets (GGX).
// A "material chart" like the ones used by film and game studios:
//   row 1: gold      RoughMetal, roughness 0.0 -> 1.0
//   row 2: copper    RoughMetal
//   row 3: red plastic (diffuse base + clear coat), roughness 0.0 -> 1.0
//   images/ch32_material_chart.png
// ------------------------------------------------------------
#include "pixel/pixel.h"
using namespace pixel;

int main() {
    HittableList world, lights;

    // Studio floor.
    auto floor_mat = std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(0.5, Color(0.35, 0.35, 0.35), Color(0.25, 0.25, 0.25)));
    world.add(std::make_shared<Quad>(Point3(-50, -0.5, -50), Vec3(100, 0, 0), Vec3(0, 0, 100), floor_mat));

    // Measured "base reflectivity" (F0) of real metals, in linear RGB.
    const Color gold(1.000, 0.766, 0.336);
    const Color copper(0.955, 0.638, 0.538);
    const int N = 6;
    for (int i = 0; i < N; i++) {
        double roughness = (double)i / (N - 1);
        double x = (i - (N - 1) / 2.0) * 1.15;
        world.add(std::make_shared<Sphere>(Point3(x, 2.3, 0), 0.5, std::make_shared<RoughMetal>(gold, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 1.15, 0), 0.5, std::make_shared<RoughMetal>(copper, roughness)));
        world.add(std::make_shared<Sphere>(Point3(x, 0.0, 0), 0.5, std::make_shared<Plastic>(Color(0.6, 0.05, 0.05), roughness)));
    }

    // Two big soft "softbox" lights, like a photo studio.
    auto softbox = std::make_shared<DiffuseLight>(Color(6, 6, 6));
    auto key = std::make_shared<Quad>(Point3(-6, 6, 4), Vec3(4, 0, 0), Vec3(0, -2, 2), softbox);   // faces down-back
    auto rim = std::make_shared<Quad>(Point3(4, 5, -6), Vec3(3, 0, 0), Vec3(0, 2, 2), softbox);
    world.add(key);
    world.add(rim);
    lights.add(key);
    lights.add(rim);

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 800;
    cam.samples_per_pixel = 128;
    cam.max_depth = 20;
    cam.vfov = 32;
    cam.lookfrom = Point3(0, 1.6, 10);
    cam.lookat = Point3(0, 1.1, 0);
    cam.background = gradient_sky(Color(0.05, 0.05, 0.06), Color(0.2, 0.22, 0.25));   // dim studio walls
    cam.max_sample_value = 30;   // tame rare fireflies from tiny highlights

    SaveOptions opt;
    opt.tonemap = ToneMapper::Aces;
    opt.exposure = 1.4;
    save_image("images/ch32_material_chart.png", cam.render(world, &lights), opt);
    return 0;
}
```

The scene is a studio: a dark checkered floor, a dim grey "wall" background, and two large softbox
lights (a **key** light upper left and a **rim** light behind right). Three rows of six spheres go from
roughness 0 (left) to 1 (right):

* top: **gold**,
* middle: **copper**,
* bottom: **red plastic**.

The image is saved with **ACES tone mapping** (chapter 34) because the highlights are much brighter than 1.

```bat
run ch32_microfacets
```

---

## 8. What you should see

![Material chart](../images/ch32_material_chart.png)

> **Image description:** A dark studio with 18 spheres in three rows on a dark checkered floor.
> **Top row, gold:** the leftmost sphere is a perfect gold mirror showing sharp reflections of the two
> rectangular softbox lights and the checkered floor. Moving right, the reflections blur more and more: the
> softbox reflections become soft glows, and the rightmost sphere is a smooth, satin-like gold with a broad
> soft highlight. **Middle row, copper:** the same progression in a pinkish copper color. **Bottom row, red
> plastic:** all six have the same red body color; the leftmost has sharp white reflections of the
> softboxes on top of the red, and the reflections become broader and fainter to the right, until the last
> one looks like matte red rubber with a soft sheen. Near the edges of every plastic sphere, the
> reflections get stronger (Fresnel).

This kind of chart is how material artists check that a renderer's materials behave well across the
whole range.

---

## Try it yourself

1. Add a row of **aluminium** and one of **iron** (F0 values in the table above).
2. Make a **blue car paint**: `Plastic(hex_color(0x1E3A8A), 0.05)`. Try a very low roughness.
3. Set F0 of the plastic coat to 0.08 (by changing the IOR to ~1.8). How does it look?
4. Compare the old `Metal(gold, fuzz)` with `RoughMetal(gold, roughness)` side by side. Which looks more
   convincing at the edges?
5. Put the chart outdoors with the sky from the next chapter.

## Common problems

| Symptom | Cause |
|---------|-------|
| Black spheres | Light directions in the wrong frame (forgot `to_local`/`transform`), or `wi.z <= 0` |
| Rough metal looks too bright at the edges | Missing the G2/G1 masking term |
| Plastic has colored highlights | Coat weight multiplied by the base color (the coat must be white) |
| Fireflies on smooth surfaces | Tiny bright lights reflected in near-mirrors: use `max_sample_value` |

---

## Summary

* Rough surfaces = many tiny mirrors: **D** (GGX, α = roughness²), **F** (Schlick with F0), **G** (Smith).
* Metals: colored F0, no diffuse. Non-metals: white F0 ≈ 0.04 + colored diffuse.
* VNDF sampling makes the weight just `F · G2/G1`.
* Plastic = choose coat or base at random, and correct by the choice probability.

Next: [Chapter 33 — Skies and environment lighting →](33-sky-and-environment.md)
