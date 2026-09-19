# Appendix A — Library reference

[← Contents](README.md)

Everything in the `pixel` library, grouped by header. Include `"pixel/pixel.h"` to get it all. Everything is
in `namespace pixel` (sub-namespaces: `pixel::png`, `pixel::post`, `pixel::sdf`, `pixel::ggx`).

---

## vec3.h — vectors (chapter 12)

| Name | Description |
|------|-------------|
| `pi`, `infinity` | constants |
| `degrees_to_radians(d)` | degrees → radians |
| `clamp01(x)`, `clampd(x, lo, hi)`, `lerpd(a, b, t)`, `smoothstep(e0, e1, x)` | scalar helpers |
| `struct Vec3 { x, y, z }` | 3D vector; `Point3` and `Color` are aliases |
| `Vec3(v)`, `Vec3(x, y, z)` | constructors |
| `v[i]`, `-v`, `+= -= *= /=` | access and arithmetic |
| `length()`, `length_squared()`, `near_zero()`, `max_component()`, `min_component()` | queries |
| `a + b`, `a - b`, `a * b` (component-wise), `t * v`, `v / t`, `a / b` | operators |
| `dot(a, b)`, `cross(a, b)`, `unit_vector(v)` | vector products |
| `vmin`, `vmax`, `vabs`, `lerp(a, b, t)` | component-wise helpers |
| `reflect(v, n)`, `refract(uv, n, ratio)` | optics (chapters 18, 19) |
| `luminance(c)` | perceived brightness of a linear color |

## random.h — randomness (chapter 16)

| Name | Description |
|------|-------------|
| `struct Pcg32` | `Pcg32(seed, seq)`, `next_u32()`, `next_double()`, `seed_with(...)` |
| `thread_rng()`, `seed_thread_rng(seed)` | the per-thread generator |
| `random_double()`, `random_double(a, b)`, `random_int(a, b)` | numbers |
| `random_vec()`, `random_vec(a, b)` | random vectors |
| `random_unit_vector()`, `random_on_hemisphere(n)`, `random_in_unit_disk()`, `random_cosine_direction()` | directions |

## color.h — color and tone mapping (chapters 4, 34)

| Name | Description |
|------|-------------|
| `linear_to_srgb`, `srgb_to_linear` | exact sRGB curve (double or Color) |
| `linear_to_gamma2` | the simple sqrt approximation |
| `to_byte(x)` | 0..1 → 0..255 |
| `rgb255(r, g, b)`, `hex_color(0xRRGGBB)` | designer colors → **linear** |
| `hsv(h, s, v)` | HSV → RGB (display values) |
| `tonemap_clamp/_reinhard/_aces/_hable`, `enum ToneMapper`, `apply_tonemap` | tone mapping |

## png.h — PNG encoder (chapters 6, 7)

`png::crc32_update`, `png::adler32`, `png::BitWriter`, `png::zlib_compress(data, level)`,
`png::filter_rows`, `png::encode(pixels, w, h, channels, level)`, `png::write_file(...)`.

## image.h — images and files (chapter 5)

| Name | Description |
|------|-------------|
| `struct Image { width, height, data }` | linear colors, row by row, top first |
| `Image(w, h, fill)`, `at(x, y)`, `in_bounds`, `get_clamped`, `fill` | basics |
| `sample_bilinear(u, v)`, `sample_nearest(u, v)` | texture lookups (v = 0 is the bottom) |
| `struct SaveOptions { exposure, tonemap, srgb }` | how to encode for display |
| `encode_pixel`, `to_rgb8` | linear → bytes |
| `write_ppm`, `read_ppm`, `write_bmp`, `write_png` | file I/O |
| `save_image(file, img, opt)` | choose format by extension (.png/.bmp/.ppm) and print a message |
| `ensure_parent_folder(file)` | create folders as needed |

## canvas.h — 2D drawing (chapters 8–10)

`Canvas(img)`: `clear`, `set_pixel`, `blend_pixel`, `add_pixel`, `fill_rect`, `draw_rect`, `draw_line`
(Bresenham), `draw_line_aa`, `fill_circle`, `draw_circle`, `fill_circle_aa`, `glow`, `edge` (static),
`fill_triangle` (per-corner colors or one color, with supersampling), `fill_polygon`, `vertical_gradient`,
`radial_gradient`.

## noise.h — procedural noise (chapters 11, 24)

`hash_u32`, `hash2`, `hash3`, `hash2_01`, `value_noise_2d`, `fbm_2d`, and `class Perlin(seed)` with
`noise(p)`, `turbulence(p, depth)`, `fbm(p, octaves, gain)`.

## ray.h — rays and intervals (chapters 13, 15)

`class Ray(origin, direction, time)`: `origin()`, `direction()`, `time()`, `at(t)`.
`struct Interval { min, max }`: `size`, `contains`, `surrounds`, `clamp`, `expand`, `empty()`, `universe()`.

## aabb.h — bounding boxes (chapter 23)

`class AABB`: from intervals, two points, or two boxes; `hit(ray, interval)`, `longest_axis()`,
`centroid()`, `surface_area()`, `axis_interval(n)`, `empty()`; `box + offset`.

## hittable.h — the object interface (chapter 15)

`struct HitRecord { p, normal, mat, t, u, v, front_face; set_face_normal() }`.
`class Hittable`: `hit`, `bounding_box`, `pdf_value`, `random`.
`class HittableList`: `objects`, `add`, `clear`, and the Hittable functions (closest hit; lights average).

## Shapes

| Class | Header | Constructor |
|-------|--------|-------------|
| `Sphere` | sphere.h | `(center, radius, mat)` or moving `(center1, center2, radius, mat)` |
| `Quad` | quad.h | `(Q, u, v, mat)`, front side = `cross(u, v)` |
| `Disk` | quad.h | `(center, half_u, half_v, mat)` |
| `make_box(a, b, mat)` | quad.h | 6 outward-facing quads |
| `Triangle` | triangle.h | flat `(a, b, c, mat)` or smooth with normals and UVs |
| `Mesh` | triangle.h | `positions, normals, uvs, indices`; `compute_smooth_normals()`, `transform(s, offset)`, `build(mat)` |
| `load_obj`, `save_obj` | triangle.h | Wavefront OBJ I/O |
| `make_parametric_mesh`, `make_torus`, `make_heightfield` | triangle.h | procedural meshes |
| `BVHNode` | bvh.h | `(HittableList)` |
| `Translate`, `RotateY` | instance.h | `(object, offset)`, `(object, degrees)` |
| `ConstantMedium` | volume.h | `(boundary, density, albedo or texture)` |
| `VariableMedium` | volume.h | `(boundary, max_density, density_fn, albedo)` |
| `SDFObject` | sdf.h | `(distance_fn, bounds, mat, max_steps, epsilon, step_scale)` |

`sdf::` functions: `sphere`, `box`, `round_box`, `torus`, `plane_y`, `capsule_y`, `op_union`, `op_subtract`,
`op_intersect`, `op_smooth_union`, `op_repeat_xz`, `rotate_y`, `mandelbulb`.

## texture.h — textures (chapter 24)

`SolidColor`, `CheckerTexture(scale, c1, c2)`, `UVCheckerTexture(nu, nv, c1, c2)`, `ImageTexture(image or
"file.ppm")`, `NoiseTexture(scale, Smooth|Turbulence|Marble, tint)`, `FunctionTexture(lambda)`.

## material.h — materials (chapters 17–19, 26, 28, 32)

| Class | Constructor | Behaviour |
|-------|-------------|-----------|
| `Lambertian` | `(color or texture)` | matte, cosine-sampled |
| `Metal` | `(albedo, fuzz)` | mirror + fuzz |
| `Dielectric` | `(ior, tint)` | glass with Schlick Fresnel |
| `DiffuseLight` | `(color or texture, two_sided)` | emits light |
| `Isotropic` | `(color or texture)` | volume scattering |
| `RoughMetal` | `(f0, roughness)` | GGX metal |
| `Plastic` | `(color or texture, roughness, ior)` | diffuse base + GGX clear coat |

Also: `ScatterRecord`, `schlick(cos, f0)`, and `ggx::lambda`, `G1`, `G2`, `sample_vndf`, `sample_reflection`.

## pdf.h — sampling (chapters 30, 31)

`ONB(n)`: `u()`, `v()`, `w()`, `transform`, `to_local`. `PDF`: `value`, `generate`. `SpherePDF`,
`CosinePDF(n)`, `HittablePDF(objects, origin)`, `MixturePDF(&a, &b)`.

## sky.h — backgrounds (chapter 33)

`Background` (= `std::function<Color(const Ray&)>`), `solid_background(c)`, `gradient_sky(horizon, zenith)`,
`SkySettings`, `physical_sky(settings)`, `make_sun(direction, angular_radius_deg, radiance, distance)`,
`environment_map(image, intensity, rotation_deg)`.

## camera.h — the renderer (chapters 17, 20, 21, 31, 36)

| Field | Default | Meaning |
|-------|---------|---------|
| `aspect_ratio` | 16/9 | width / height |
| `image_width` | 400 | pixels |
| `samples_per_pixel` | 10 | rays per pixel |
| `max_depth` | 10 | bounces |
| `vfov` | 90 | vertical field of view (degrees) |
| `lookfrom`, `lookat`, `vup` | (0,0,0), (0,0,−1), (0,1,0) | placement |
| `defocus_angle`, `focus_dist` | 0, 10 | depth of field |
| `shift_x`, `shift_y` | 0 | lens shift |
| `background` | `gradient_sky()` | what rays that miss see |
| `threads` | 0 (all cores) | worker threads |
| `show_progress` | true | print progress |
| `russian_roulette`, `rr_start_depth` | true, 3 | path termination |
| `max_sample_value` | 0 (off) | firefly clamp |
| `seed` | 1 | noise pattern |
| `t_min` | 0.001 | shadow-acne offset |
| `collect_aovs` → `albedo_aov`, `normal_aov` | false | denoiser guides |

Methods: `render(world, lights = nullptr)` → `Image`; `trace(ray, world)`; `initialize()`;
`get_ray(i, j, s)`; `image_height()`.

## post.h — post-processing (chapters 34–36)

`post::gaussian_kernel`, `gaussian_blur`, `downsample2`, `resize`, `bloom`, `exposure`, `temperature`,
`saturation`, `contrast`, `lift_gamma_gain`, `tonemap`, `teal_orange`, `vignette`, `chromatic_aberration`,
`film_grain`, `letterbox`, `DenoiseSettings`, `denoise`, `visualize_normals`, `side_by_side`, `paste`.

## gif.h — animated GIF (chapter 39)

`GifWriter`: `begin(file, w, h, delay_cs, loop_count)`, `add_frame(img, opt)`, `end()`.
