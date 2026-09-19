# Appendix B — Troubleshooting

[← Contents](README.md)

Graphics bugs are usually **visible**. Learn to read the picture.

## Building

| Message / symptom | Fix |
|-------------------|-----|
| `'g++' is not recognized` | Install GCC (chapter 1) and reopen the terminal |
| `pixel/pixel.h: No such file or directory` | Run from the `render-from-scratch` folder (the build uses `-Iinclude`) |
| `'filesystem' is not a member of 'std'` | Compiler too old, or `-std=c++17` missing |
| `undefined reference to 'pthread_...'` | Add `-pthread` (the scripts do) |
| MSVC: `cl is not recognized` | Use the *x64 Native Tools Command Prompt*; install the C++ workload |
| Warnings about conversions | Harmless in this project; fix them if you like |

## Running

| Symptom | Likely cause |
|---------|--------------|
| No image appears | You ran the program from another folder; look for `images\` next to where you ran it |
| Very slow | Built without `-O2`; scene without a BVH; too many samples |
| Crash immediately | Writing outside an image (`at(x, y)` with a bad index); an empty list given to `BVHNode` |

## Reading broken images

| What you see | What it usually means |
|--------------|----------------------|
| Completely black | No light: black background and no (or wrong-facing) lights; `max_depth` 1; exposure 0 |
| Completely white | Exposure far too high; light values huge with clamp tone mapping |
| Everything too dark and contrasty | Saved without sRGB |
| Everything washed out | sRGB applied twice |
| Fine dark speckle on all surfaces ("acne") | `t_min` = 0; bounce rays re-hit their own surface |
| Isolated bright dots ("fireflies") | Rare strong light paths: light sampling, more samples, `max_sample_value` |
| Grain that won't go away | Small lights without light sampling; volumes; caustics |
| Magenta objects | Texture file not found |
| Objects missing with BVH, fine with list | Wrong bounding box |
| A flat object is invisible | It's seen edge-on, or a one-sided light faces away |
| Image upside down | Viewport v vector sign, or BMP row order |
| Image sheared diagonally | Wrong row length (padding) in a file writer |
| Stretched image | Viewport ratio ≠ image ratio |
| NaN/black pixels in odd places | `sqrt` of a negative number, division by zero, un-normalized vectors |
| Everything identical with 1 or 100 samples | Random offsets not applied, or same seed reused per sample |

## A debugging recipe

1. **Simplify**: one object, one light, few samples, small image.
2. **Visualize** intermediate values: normals (`0.5·(n+1)`), depth, albedo, `front_face`, UVs.
3. **Print** a single pixel's values (pick the pixel in the middle of the problem).
4. Change **one thing** at a time.
5. Compare with a **known-good** chapter program.
