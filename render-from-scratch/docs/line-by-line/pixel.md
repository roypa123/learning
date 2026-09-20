# Line by line: `include/pixel/pixel.h`

[← Line-by-line index](README.md)

**What this file does, in one sentence:** it includes every other file of the library, so one line
(`#include "pixel/pixel.h"`) gives you everything.

```cpp
// pixel/pixel.h
// ------------------------------------------------------------
// "pixel" - our own from-scratch rendering library.
// Include this ONE file to get everything.
// ...
#pragma once
#include "vec3.h"
#include "random.h"
#include "color.h"
#include "png.h"
#include "image.h"
#include "canvas.h"
#include "noise.h"
#include "ray.h"
#include "aabb.h"
#include "hittable.h"
#include "pdf.h"
#include "texture.h"
#include "material.h"
#include "sphere.h"
#include "quad.h"
#include "bvh.h"
#include "triangle.h"
#include "instance.h"
#include "volume.h"
#include "sdf.h"
#include "sky.h"
#include "camera.h"
#include "post.h"
#include "gif.h"
```

* The comment block lists the files by topic, so you can find things quickly.
* `#pragma once`: even though this file is included by every chapter program, its contents are only read once.
* The order matters a little: each file also includes what **it** needs, and `#pragma once` in every header makes sure
  nothing is defined twice. So the order here is really just "most basic first", for readability.

**Why an umbrella header?** Chapter programs stay short:

```cpp
#include "pixel/pixel.h"
using namespace pixel;
```

instead of ten include lines. The cost is a slightly longer compile time, which for this book's small programs is a
fraction of a second.

If you build a bigger project, include only the parts you need (for example just `image.h` and `canvas.h` for a 2D
tool).

---

## The dependency map

```
vec3.h ─────────────┬─────────────────────────────────────────────┐
  random.h ─────────┤                                             │
  color.h ──────────┤                                             │
    png.h ──────────┴─▶ image.h ─┬─▶ canvas.h                     │
                                 ├─▶ texture.h ◀── noise.h        │
ray.h ──▶ aabb.h ──▶ hittable.h ─┼─▶ pdf.h ──▶ material.h ◀───────┘
                                 ├─▶ sphere.h, quad.h, triangle.h, bvh.h,
                                 │   instance.h, volume.h, sdf.h
                                 └─▶ sky.h ──▶ camera.h ──▶ (your program)
                                     post.h, gif.h
```

Read it as "the arrow points to the file that needs the one before it".
