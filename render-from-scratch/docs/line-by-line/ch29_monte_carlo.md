# Line by line: `ch29_monte_carlo.cpp`

[← Line-by-line index](README.md) · [Chapter 29 (the theory)](../29-monte-carlo.md)

**What the whole program does, in one sentence:** it estimates π by throwing random "darts" at a square (plain and
evenly-spread versions), estimates the area under x², and draws a picture of the darts.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–13 | |
| B. π with random darts | 15–28 | the error shrinks as we throw more darts |
| C. π with stratified darts | 30–42 | one dart per grid cell: more accurate |
| D. An integral | 44–54 | area under x² from 0 to 2 |
| E. Picture of the darts | 56–71 | |

---

## Block A — Comments, includes (lines 1–13)

Comments (the four experiments), `<cmath>`, `<cstdio>`, our library, `using namespace pixel`.

---

## Block B — π with random darts (lines 15–28)

The idea: a circle of radius 1 inside a 2 × 2 square. The circle's area is π, the square's area is 4. So the fraction of
random darts that land inside the circle ≈ π / 4, and π ≈ 4 × fraction.

```cpp
    std::printf("Estimating pi with random darts in the square [-1,1]^2:\n");
    std::printf("%12s %14s %12s\n", "darts", "estimate", "error");
    long long inside = 0, total = 0;
```

* Lines 17–18: a title and column headers.
* Line 19: counters. `long long` = a very large whole-number type (we'll count up to 10 million).

```cpp
    for (long long n = 10; n <= 10000000; n *= 10) {
        while (total < n) {
            double x = random_double(-1, 1), y = random_double(-1, 1);
            if (x * x + y * y < 1) inside++;
            total++;
        }
```

* Line 20: check the estimate after 10, 100, 1000, ... 10,000,000 darts (`n *= 10` multiplies by 10 each time).
* Line 21: throw more darts until we have `n` in total (we keep the earlier darts).
* Line 22: a random point in the square.
* Line 23: inside the circle? (x² + y² < 1.) Count it.
* Line 24: count every dart.

```cpp
        double estimate = 4.0 * inside / total;
        std::printf("%12lld %14.8f %12.8f\n", total, estimate, std::fabs(estimate - pi));
    }
```

* Line 26: π ≈ 4 × inside / total.
* Line 27: print the dart count (`%lld` = long long), the estimate, and the error (the distance from the true π).

---

## Block C — π with stratified darts (lines 30–42)

Instead of fully random darts, split the square into a grid and put **one random dart in each cell**. The darts are
spread evenly (no clumps, no gaps), and the result is more accurate.

```cpp
    for (int grid = 10; grid <= 3000; grid *= 3) {
        long long in = 0;
```

Line 32: grids of 10 × 10, 30 × 30, 90 × 90, ... up to 2430 × 2430.

```cpp
        for (int i = 0; i < grid; i++)
            for (int j = 0; j < grid; j++) {
                double x = 2 * ((i + random_double()) / grid) - 1;
                double y = 2 * ((j + random_double()) / grid) - 1;
                if (x * x + y * y < 1) in++;
            }
```

* Lines 34–35: every cell (i, j).
* Lines 36–37: a random point **inside cell (i, j)**: `(i + random) / grid` is between i/grid and (i+1)/grid (0–1 overall);
  `× 2 − 1` maps it to −1..1.
* Line 38: count darts inside the circle.

```cpp
        double estimate = 4.0 * in / ((double)grid * grid);
        std::printf("%12lld %14.8f %12.8f\n", (long long)grid * grid, estimate, std::fabs(estimate - pi));
    }
```

Lines 40–41: the estimate and its error. Compare with block B for the same number of darts.

---

## Block D — An integral (lines 44–54)

The area under the curve y = x² from x = 0 to 2 (exactly 8/3 = 2.6667). Monte Carlo: average the height at random x, times
the width.

```cpp
    for (int n = 10; n <= 1000000; n *= 10) {
        double sum = 0;
        for (int i = 0; i < n; i++) {
            double x = random_double(0, 2);
            sum += x * x;
        }
        std::printf("%12d %14.8f\n", n, 2.0 * sum / n);
    }
```

* Line 47: 10, 100, ... 1,000,000 samples.
* Lines 49–52: add up x² at `n` random points between 0 and 2.
* Line 53: the average height (`sum / n`) × the width (2). It gets closer to 2.6667 as n grows.

---

## Block E — Picture of the darts (lines 56–71)

```cpp
    const int S = 500;
    Image img(S, S, hex_color(0x0F172A));
    Canvas cv(img);
    cv.draw_circle(S / 2, S / 2, S / 2 - 20, hex_color(0x94A3B8));
    cv.draw_rect(20, 20, S - 40, S - 40, hex_color(0x94A3B8));
```

Lines 57–61: a dark image with a grey circle outline inside a grey square outline (20-pixel margin).

```cpp
    Pcg32 rng(3);
    for (int i = 0; i < 2000; i++) {
        double x = rng.next_double() * 2 - 1, y = rng.next_double() * 2 - 1;
        bool in = x * x + y * y < 1;
        double px = 20 + (x + 1) * 0.5 * (S - 40), py = 20 + (y + 1) * 0.5 * (S - 40);
        cv.fill_circle_aa(px, py, 2.2, in ? hex_color(0xF43F5E) : hex_color(0x38BDF8));
    }
    save_image("images/ch29_darts.png", img);
```

* Line 62: a random generator (its own, so the picture is always the same).
* Lines 63–64: 2000 random darts in −1..1.
* Line 65: inside the circle?
* Line 66: convert −1..1 to pixel positions (20 to 480).
* Line 67: a small dot: pink-red if inside, light blue if outside.
* Line 69: save.

---

## Check your understanding

1. Why multiply the fraction by 4? *(The circle is π/4 of the square's area.)*
2. What makes stratified darts better? *(They cover the square evenly: no clumps or gaps.)*
3. In block D, why multiply the average by 2? *(Area = average height × width, and the width is 2.)*
