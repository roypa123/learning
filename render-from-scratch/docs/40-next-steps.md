# Chapter 40 — Where to go next

[← Animation](39-animation.md) · [Contents](README.md) · [Appendix A →](appendix-a-library-reference.md)

---

## You did it

Look at what you've built, **from nothing**, with no external libraries:

* image files: PPM, BMP, **PNG** (with your own DEFLATE compressor) and animated **GIF** (with your own LZW),
* 2D graphics: lines, circles, triangles, polygons, anti-aliasing, blending, procedural noise,
* a **multithreaded path tracer** with spheres, quads, disks, triangles, meshes, OBJ files, instances, SDFs and
  fractals, a BVH, textures, volumes (constant and variable density), motion blur and depth of field,
* **physically based** materials: Lambertian, glass with Fresnel, GGX metals and layered plastics,
* **importance sampling**, light sampling with mixture PDFs, Russian roulette,
* skies, sun and environment maps,
* a film pipeline: HDR, ACES, bloom, grading, chromatic aberration, vignette, grain, letterbox, denoising,
* a finished cinematic frame, and an animation.

That's the core of what production renderers do. What separates ours from them is mostly **scale,
speed and polish**. Here's a road map, roughly from easiest to hardest.

---

## 1. Improve the renderer

| Project | Difficulty | Notes |
|---------|------------|-------|
| A general `Transform` (4×4 matrices: rotate any axis, scale) | ★★ | inverse matrix for rays, inverse transpose for normals |
| Henyey–Greenstein phase function for volumes | ★★ | forward scattering = glowing sunbeams |
| SAH BVH construction | ★★ | chapter 23, section 4 |
| PNG **reader** (inflate) | ★★★ | the reverse of chapter 7; then load textures from any PNG |
| Environment map importance sampling | ★★★ | 2D CDF over the image, clean HDRI lighting |
| Light sampling for glossy materials with full MIS | ★★★ | evaluate the GGX BRDF and its PDF; see PBRT |
| Normal maps and bump maps | ★★ | perturb shading normals from a texture |
| Dynamic Huffman codes in the PNG writer | ★★★ | RFC 1951, section 3.2.7 |
| Spectral rendering | ★★★★ | wavelengths instead of RGB: dispersion (rainbows in prisms) |
| Subsurface scattering (skin, wax, marble) | ★★★★ | random walks inside a volume under the surface |
| Bidirectional path tracing / photon mapping | ★★★★ | clean caustics |
| GPU rendering (CUDA, Vulkan, compute shaders) | ★★★★ | 10–100× faster, but new tools |
| Hydra/USD scenes, instancing millions of objects | ★★★★★ | how studios describe shots |

## 2. Make it faster (CPU)

* Use `float` instead of `double` (half the memory, faster SIMD).
* Avoid `shared_ptr` allocations in the inner loop (the PDFs): use a small fixed union instead.
* Trace **packets** of rays with SIMD instructions (SSE/AVX).
* Use tiles instead of rows for better cache behavior.
* Progressive rendering: show the image improving live, and stop when it looks good.

## 3. Books and resources

* **Peter Shirley et al., "Ray Tracing in One Weekend" series** (free online). Part 4 of this book follows its
  spirit closely, and the next two books go deeper into several of our topics.
* **Pharr, Jakob, Humphreys: "Physically Based Rendering: From Theory to Implementation" (PBRT)** (free
  online). *The* reference: everything, rigorously.
* **"Real-Time Rendering"** (Akenine-Möller et al.) for the game-engine side.
* **Inigo Quilez's articles** (iquilezles.org) on SDFs, noise and procedural art.
* **Shadertoy.com**: thousands of tiny renderers in your browser.
* **"Ray Tracing Gems" I & II** (free online).
* Eric Veach's PhD thesis (1997), the source of MIS and much of modern rendering.
* For film look and color: the ACES documentation, and any good cinematography book (lighting, composition).

## 4. Keep making pictures

The best way to learn more is to **make images you care about**. Pick a photo or a film still you love and
try to recreate its mood. You'll quickly find which feature you need next, and you'll know exactly
where in your own code to add it.

Thank you for reading, and happy rendering. ✨

---

Continue to the appendices: [A — Library reference](appendix-a-library-reference.md) ·
[B — Troubleshooting](appendix-b-troubleshooting.md) · [C — Glossary](appendix-c-glossary.md) ·
[D — Math cheat sheet](appendix-d-math-cheatsheet.md) · [E — Full source](appendix-e-full-source.md)
