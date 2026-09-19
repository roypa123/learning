# Line by line: `ch08_drawing.cpp`

[← Line-by-line index](README.md) · [Chapter 8 (the theory)](../08-drawing-basics.md) · [canvas.h explained](canvas.md)

**What the whole program does, in one sentence:** it draws three pictures with the Canvas: a pixel-art heart, a
starburst of colored lines, and a painting made only of rectangles.

| Block | Lines | Job |
|-------|-------|-----|
| A. Comments, includes | 1–10 | notes and our library |
| B. Picture 1: pixel-art heart | 12–37 | a heart from text, drawn as big squares with a grid |
| C. Picture 2: starburst | 39–57 | 48 lines from the center, rainbow colors |
| D. Picture 3: Mondrian | 59–77 | colored rectangles and black bars |
| E. End | 78–79 | |

---

## Block A — Comments, includes (lines 1–10)

* Lines 1–7: comments: the three images.
* Line 8: `<cmath>` for `std::cos`, `std::sin`.
* Line 9: our library. Line 10: use `pixel` names directly.

---

## Block B — Picture 1: pixel-art heart (lines 12–37)

```cpp
int main() {
    // ---------- 1. Pixel art: every pixel set by hand -----------------------
    {
```

* Line 12: the program starts.
* Line 14: a block, so names like `img` can be used again later.

```cpp
        const char* heart[] = {
            "..XX...XX..",
            ".XXXX.XXXX.",
            "XXXXXXXXXXX",
            "XXXXXXXXXXX",
            ".XXXXXXXXX.",
            "..XXXXXXX..",
            "...XXXXX...",
            "....XXX....",
            ".....X.....",
        };
```

* Line 15: `heart` is an **array of text lines**. `const char*` = a piece of fixed text. `[]` = let the compiler
  count how many (9).
* Lines 16–24: the picture as text: `X` = colored pixel, `.` = empty. Each line has 11 characters. It's an easy way
  to "draw" small pictures directly in code.
* Line 25: `};` ends the list.

```cpp
        const int rows = 9, cols = 11, zoom = 24;
        Image img(cols * zoom, rows * zoom, hex_color(0xFDF6E3));
        Canvas cv(img);
```

* Line 26: 9 rows, 11 columns, and each "art pixel" will be drawn as a 24 × 24 square (`zoom`).
* Line 27: the image is 11 × 24 = 264 wide and 9 × 24 = 216 tall, filled with cream (`#FDF6E3`).
* Line 28: a canvas to draw on it.

```cpp
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (heart[r][c] == 'X')
                    cv.fill_rect(c * zoom, r * zoom, zoom, zoom, hex_color(0xE63946));
```

* Lines 29–30: for every row `r` and column `c` of the text picture.
* Line 31: `heart[r][c]` = the character at row r, column c. `'X'` with single quotes is one character.
* Line 32: if it's an X, fill a 24 × 24 red square at position (c × 24, r × 24).

```cpp
        // grid lines so you can see individual "pixels"
        for (int c = 0; c <= cols; c++) cv.draw_line(c * zoom, 0, c * zoom, rows * zoom - 1, hex_color(0xCCCCCC));
        for (int r = 0; r <= rows; r++) cv.draw_line(0, r * zoom, cols * zoom - 1, r * zoom, hex_color(0xCCCCCC));
        save_image("images/ch08_pixels.png", img);
    }
```

* Line 34: 12 vertical grey lines (`<= cols`, so one more line than columns, to close the right side), every 24 pixels,
  from top to bottom.
* Line 35: 10 horizontal grey lines.
* Line 36: save as PNG.
* Line 37: end of block.

---

## Block C — Picture 2: starburst (lines 39–57)

```cpp
        Image img(400, 400, hex_color(0x0B132B));
        Canvas cv(img);
```

Lines 41–42: a 400 × 400 dark navy image and a canvas.

```cpp
        for (int i = 0; i < 400; i += 20) {       // background grid
            cv.draw_line(i, 0, i, 399, hex_color(0x1C2541));
            cv.draw_line(0, i, 399, i, hex_color(0x1C2541));
        }
```

* Line 43: `i` = 0, 20, 40, ... 380 (`i += 20` adds 20 each round).
* Line 44: a vertical line at x = i.
* Line 45: a horizontal line at y = i. Together: a faint grid.

```cpp
        const int N = 48;
        for (int k = 0; k < N; k++) {             // starburst
            double a = 2 * pi * k / N;
```

* Line 47: 48 lines.
* Line 48: for each line number k.
* Line 49: its angle: a full circle (2π) divided into 48 equal parts. k = 0 → 0 (pointing right), k = 12 → π/2
  (pointing down, since y goes down in images), and so on.

```cpp
            int x1 = (int)(200 + 180 * std::cos(a));
            int y1 = (int)(200 + 180 * std::sin(a));
```

Lines 50–51: the end point: start at the center (200, 200) and go 180 pixels in direction `a`. `cos` gives the x part
of a direction and `sin` gives the y part (the basic circle formula).

```cpp
            Color c = srgb_to_linear(hsv(360.0 * k / N, 0.8, 1.0));
            cv.draw_line(200, 200, x1, y1, c);
        }
```

* Line 52: a color from the color wheel: hue goes 0 → 360 as k goes 0 → 48, so the colors go all around the rainbow.
  Converted to linear for our image.
* Line 53: draw the line from the center to the end point (Bresenham, sharp).

```cpp
        cv.draw_rect(20, 20, 360, 360, hex_color(0xFFFFFF));
        save_image("images/ch08_lines.png", img);
    }
```

* Line 55: a white square outline, 20 pixels in from each edge.
* Line 56: save.

---

## Block D — Picture 3: Mondrian (lines 59–77)

A painting in the style of Piet Mondrian, made only with `fill_rect(x, y, width, height, color)`.

```cpp
        Image img(420, 420, hex_color(0xF5F1E6));
        Canvas cv(img);
        cv.fill_rect(0, 0, 250, 250, hex_color(0xD62828));      // big red
        cv.fill_rect(330, 0, 90, 140, hex_color(0x003F88));     // blue
        cv.fill_rect(0, 330, 120, 90, hex_color(0xFFD60A));     // yellow
        cv.fill_rect(330, 360, 90, 60, hex_color(0x111111));    // black
```

* Line 61: a 420 × 420 off-white canvas.
* Line 63: a big red square in the top-left corner (250 × 250).
* Line 64: a blue rectangle in the top-right.
* Line 65: a yellow rectangle in the bottom-left.
* Line 66: a small black block in the bottom-right.

```cpp
        const Color black = hex_color(0x111111);
        const int t = 10;                                       // line thickness
```

Lines 67–68: a named black color and the bar thickness (10 pixels), so they're easy to change in one place.

```cpp
        cv.fill_rect(250, 0, t, 420, black);
        cv.fill_rect(0, 250, 420, t, black);
        cv.fill_rect(320, 0, t, 420, black);
        cv.fill_rect(120, 250, t, 170, black);
        cv.fill_rect(320, 140, 100, t, black);
        cv.fill_rect(320, 350, 100, t, black);
        cv.fill_rect(0, 320, 120, t, black);
        save_image("images/ch08_mondrian.png", img);
    }
```

* Lines 69–75: the thick black bars that separate the color areas. A tall thin rectangle (width t) is a vertical bar;
  a wide flat one (height t) is a horizontal bar:
  * 69: vertical bar at x = 250, full height.
  * 70: horizontal bar at y = 250, full width.
  * 71: vertical bar at x = 320, full height.
  * 72: vertical bar at x = 120, from y = 250 down to the bottom.
  * 73–74: two short horizontal bars on the right side, at y = 140 and y = 350.
  * 75: a short horizontal bar at y = 320 on the left.
* The bars are drawn **after** the colored areas so they sit on top (painters draw back to front).
* Line 76: save.

---

## Block E — End (lines 78–79)

`return 0;` and `}`.

---

## Check your understanding

1. Why is the heart image 264 pixels wide? *(11 columns × 24 zoom.)*
2. What does `heart[2][0]` contain? *('X')*
3. Why does line 34 use `<= cols`? *(11 columns need 12 lines to have a line on both sides.)*
4. How would you make the Mondrian bars thicker? *(Change `t` on line 68.)*
