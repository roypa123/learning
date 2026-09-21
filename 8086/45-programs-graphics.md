# Chapter 45 — Programs: graphics

[← Programs: matrices](44-programs-matrices.md) · [Contents](README.md) · [Next: Debugging →](46-debugging.md)

---

## Goal

Mode 13h graphics: 320×200 pixels, 256 colours, one byte per pixel, memory-mapped at `0xA0000`.
Eight programs — set a pixel, fill the screen, fast horizontal and vertical lines, Bresenham's line
algorithm, filled rectangles, Bresenham circles, the 256-colour palette, and a bouncing ball
synchronised to the vertical retrace.

This is the chapter where writing directly to hardware stops being an abstraction.

All programs assume `macros.inc` from Chapter 38 §8.

---

## 1. Mode 13h

```asm
        mov  ax, 0x0013         ; AH = 0 (set mode), AL = 13h
        int  0x10
```

| Property | Value |
|----------|-------|
| Resolution | 320 × 200 |
| Colours | 256, from a palette of 262,144 |
| Bytes per pixel | **1** |
| Buffer address | **`0xA0000`** |
| Buffer size | 320 × 200 = 64,000 bytes |

**One byte per pixel, and the whole screen fits in one 64 KiB segment.** Those two facts are why
mode 13h is the one everybody used: no bit masking, no plane switching, no segment arithmetic.

### 1.1 The address formula

```
   offset of pixel (x, y)  =  y × 320 + x
```

and the byte at `0xA000:offset` *is* that pixel's colour.

```asm
        mov  ax, 0xA000
        mov  es, ax             ; ES -> the video buffer
        mov  di, offset
        mov  byte [es:di], colour
```

### 1.2 Computing `y × 320` without a multiply

`MUL` costs 118–133 clocks. `320 = 256 + 64 = 2⁸ + 2⁶`, so:

```asm
; DI = y × 320, with y in AX
        mov  di, ax
        mov  cl, 6
        shl  di, cl             ; DI = y × 64
        mov  cl, 8
        shl  ax, cl             ; AX = y × 256
        add  di, ax             ; DI = y × 320
```

Clocks: 4 + 8+24 + 4 + 8+32 + 3 = about **83**. Better, but not dramatically.

**The fast version uses a lookup table**, built once at start-up:

```asm
rowtab: times 200 dw 0          ; rowtab[y] = y × 320

build_rowtab:
        mov  di, rowtab
        xor  ax, ax             ; AX = the running offset
        mov  cx, 200
.next:
        mov  [di], ax
        add  ax, 320
        inc  di
        inc  di
        loop .next
        ret
```

Then a pixel address is:

```asm
        mov  si, y
        shl  si, 1              ; ×2, word table
        mov  di, [rowtab + si]  ; y × 320
        add  di, x              ; + x
```

About **20 clocks**, and 400 bytes of table. Every graphics program below uses this.

---

## Program 45.1 — Set a pixel

```asm
; pixel.asm — plot some pixels
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10               ; enter mode 13h

        mov  ax, 0xA000
        mov  es, ax             ; ES -> the video buffer
        call build_rowtab

        ; --- plot a diagonal line of pixels ---
        xor  cx, cx             ; CX = the loop counter
.next:
        mov  ax, cx             ; x = cx
        mov  bx, cx             ; y = cx
        mov  dl, 15             ; colour: white
        call putpixel
        inc  cx
        cmp  cx, 200
        jb   .next

        ; --- wait for a key, then restore text mode ---
        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; putpixel — plot a pixel.
;
;   In:        AX = x (0-319), BX = y (0-199), DL = colour
;              ES must already point at 0xA000
;   Destroys:  nothing
;
;   No bounds checking — see Exercise 45.1.
; ---------------------------------------------------------------
putpixel:
        push ax
        push bx
        push di

        shl  bx, 1              ; ×2 for the word table
        mov  di, [rowtab + bx]  ; y × 320
        add  di, ax             ; + x
        mov  [es:di], dl        ; the pixel IS this byte

        pop  di
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; build_rowtab — fill rowtab[y] with y × 320
; ---------------------------------------------------------------
build_rowtab:
        push ax
        push cx
        push di
        mov  di, rowtab
        xor  ax, ax
        mov  cx, 200
.next:
        mov  [di], ax
        add  ax, 320
        inc  di
        inc  di
        loop .next
        pop  di
        pop  cx
        pop  ax
        ret

rowtab: times 200 dw 0
```

**Output:** a white diagonal line from the top-left corner.

### Explanation

**`mov [es:di], dl` is the entire pixel write.** One instruction, about 14 clocks. BIOS function 0Ch
does the same thing in around 500 clocks — a factor of 35.

**`ES` is set once**, outside the loop. Reloading a segment register per pixel would be pure waste.

**`rowtab` is in the program's data segment**, addressed through `DS`, while the pixel is addressed
through `ES`. That is exactly what `ES` is for (Chapter 7 §4).

---

## Program 45.2 — Fill the screen

```asm
; fill.asm — fill the screen with a colour, three ways
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax

        ; --- method 1: REP STOSB, one byte at a time ---
        cld
        xor  di, di
        mov  al, 1              ; blue
        mov  cx, 64000
        rep  stosb              ; 9 + 64000 × 10 = 640,009 clocks
        call pause

        ; --- method 2: REP STOSW, two pixels at a time ---
        xor  di, di
        mov  ax, 0x0404         ; red in both bytes
        mov  cx, 32000
        rep  stosw              ; 9 + 32000 × 10 = 320,009 clocks — HALF
        call pause

        ; --- method 3: horizontal bands ---
        xor  di, di
        xor  bl, bl             ; BL = the colour
        mov  dx, 200            ; 200 rows
.row:
        mov  al, bl
        mov  ah, bl
        mov  cx, 160            ; 320 bytes = 160 words
        rep  stosw
        inc  bl                 ; a different colour each row
        dec  dx
        jnz  .row
        call pause

        mov  ax, 0x0003
        int  0x10
        exit 0

pause:
        push ax
        xor  ah, ah
        int  0x16
        pop  ax
        ret
```

### The timing

| Method | Clocks | At 5 MHz |
|--------|--------|----------|
| `REP STOSB` | 640,009 | 128 ms |
| `REP STOSW` | **320,009** | **64 ms** |
| A `MOV`/`INC`/`LOOP` loop | ~2,240,000 | 448 ms |
| BIOS pixel calls | ~32,000,000 | **6.4 seconds** |

**`REP STOSW` is twice as fast as `REP STOSB`** on an 8086, because each transfer moves two bytes for
the same 10 clocks (Chapter 29 §3.3). On an 8088 they are identical.

---

## Program 45.3 — Horizontal and vertical lines

```asm
; lines.asm — fast horizontal and vertical lines
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

        ; --- a border ---
        mov  ax, 0              ; x1
        mov  bx, 319            ; x2
        mov  cx, 0              ; y
        mov  dl, 15
        call hline
        mov  cx, 199
        call hline
        mov  ax, 0              ; x
        mov  bx, 0              ; y1
        mov  cx, 199            ; y2
        call vline
        mov  ax, 319
        call vline

        ; --- some coloured bars ---
        mov  bp, 20             ; y
        mov  dl, 1
.bar:
        mov  ax, 40
        mov  bx, 280
        mov  cx, bp
        call hline
        add  bp, 10
        inc  dl
        cmp  bp, 190
        jb   .bar

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; hline — horizontal line from (AX, CX) to (BX, CX) in colour DL
;   Uses REP STOSB, so it is as fast as a fill.
;   Destroys: nothing
; ---------------------------------------------------------------
hline:
        push ax
        push bx
        push cx
        push di

        cmp  ax, bx             ; make sure AX <= BX
        jbe  .ordered
        xchg ax, bx
.ordered:
        push ax
        mov  si, cx
        shl  si, 1
        mov  di, [rowtab + si]  ; y × 320
        pop  ax
        add  di, ax             ; + x1

        mov  cx, bx
        sub  cx, ax
        inc  cx                 ; CX = the pixel count

        mov  al, dl             ; STOSB stores AL
        cld
        rep  stosb

        pop  di
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; vline — vertical line from (AX, BX) to (AX, CX) in colour DL
;   Strides by 320 bytes per row, so no REP is possible.
;   Destroys: nothing
; ---------------------------------------------------------------
vline:
        push ax
        push bx
        push cx
        push di

        cmp  bx, cx
        jbe  .ordered
        xchg bx, cx
.ordered:
        push ax
        mov  si, bx
        shl  si, 1
        mov  di, [rowtab + si]
        pop  ax
        add  di, ax

        mov  ax, cx
        sub  ax, bx
        inc  ax
        mov  cx, ax             ; CX = the pixel count
.next:
        mov  [es:di], dl
        add  di, 320            ; down one row
        loop .next

        pop  di
        pop  cx
        pop  bx
        pop  ax
        ret

; ... build_rowtab and rowtab from Program 45.1 ...
```

### Explanation

**Horizontal lines use `REP STOSB`** — consecutive pixels are consecutive bytes, so one instruction
draws the whole line at 10 clocks per pixel.

**Vertical lines cannot.** Consecutive pixels are 320 bytes apart, so each one needs its own store
and pointer bump: about 14 + 4 + 17 = 35 clocks per pixel. **Vertical lines are three and a half
times as expensive as horizontal ones**, which is the same row-major asymmetry as Chapter 44 §5.

---

## Program 45.4 — Bresenham's line algorithm

The general case: a line at any angle, using only integer addition.

```asm
; bline.asm — Bresenham's line algorithm
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

        ; --- a fan of lines from the centre ---
        mov  bp, 0              ; the endpoint x
        mov  dl, 1
.fan:
        mov  word [x0], 160
        mov  word [y0], 100
        mov  [x1], bp
        mov  word [y1], 0
        call bresenham
        add  bp, 20
        inc  dl
        cmp  bp, 320
        jb   .fan

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; bresenham — draw a line from (x0,y0) to (x1,y1) in colour DL.
;
;   dx = |x1 - x0|,  sx = x0 < x1 ? 1 : -1
;   dy = -|y1 - y0|, sy = y0 < y1 ? 1 : -1
;   err = dx + dy
;   loop:
;       plot(x0, y0)
;       if x0 == x1 and y0 == y1: stop
;       e2 = 2 * err
;       if e2 >= dy: err += dy; x0 += sx
;       if e2 <= dx: err += dx; y0 += sy
;
;   All variables are in memory, because there are more of them than
;   the 8086 has registers.
;
;   Destroys:  AX, BX, CX, SI, DI
; ---------------------------------------------------------------
bresenham:
        ; --- dx = |x1 - x0|, sx ---
        mov  ax, [x1]
        sub  ax, [x0]
        mov  word [sx], 1
        or   ax, ax
        jns  .dx_ok
        neg  ax
        mov  word [sx], -1
.dx_ok:
        mov  [dx_], ax

        ; --- dy = -|y1 - y0|, sy ---
        mov  ax, [y1]
        sub  ax, [y0]
        mov  word [sy], 1
        or   ax, ax
        jns  .dy_ok
        neg  ax
        mov  word [sy], -1
.dy_ok:
        neg  ax                 ; dy is kept NEGATIVE, as the algorithm wants
        mov  [dy_], ax

        ; --- err = dx + dy ---
        mov  ax, [dx_]
        add  ax, [dy_]
        mov  [err], ax

.loop:
        ; --- plot ---
        mov  ax, [x0]
        mov  bx, [y0]
        call putpixel

        ; --- finished? ---
        mov  ax, [x0]
        cmp  ax, [x1]
        jne  .step
        mov  ax, [y0]
        cmp  ax, [y1]
        je   .done

.step:
        mov  ax, [err]
        shl  ax, 1              ; e2 = 2 × err
        mov  [e2], ax

        ; --- if e2 >= dy: err += dy; x0 += sx ---
        cmp  ax, [dy_]
        jl   .skip_x            ; SIGNED comparison — err and dy can be negative
        mov  ax, [err]
        add  ax, [dy_]
        mov  [err], ax
        mov  ax, [x0]
        add  ax, [sx]
        mov  [x0], ax
.skip_x:

        ; --- if e2 <= dx: err += dx; y0 += sy ---
        mov  ax, [e2]
        cmp  ax, [dx_]
        jg   .skip_y
        mov  ax, [err]
        add  ax, [dx_]
        mov  [err], ax
        mov  ax, [y0]
        add  ax, [sy]
        mov  [y0], ax
.skip_y:
        jmp  .loop
.done:
        ret

; ... putpixel, build_rowtab and rowtab from Program 45.1 ...

x0:     dw   0
y0:     dw   0
x1:     dw   0
y1:     dw   0
dx_:    dw   0
dy_:    dw   0
sx:     dw   0
sy:     dw   0
err:    dw   0
e2:     dw   0
rowtab: times 200 dw 0
```

### Explanation

**Why Bresenham rather than `y = mx + c`.** The slope-intercept form needs division and fractional
arithmetic. Bresenham uses only integer addition, subtraction and comparison — no `MUL`, no `DIV`,
no rounding. On an 8086 that is the difference between 30 clocks per pixel and 300.

**The error term.** `err` tracks how far the true line has drifted from the pixels drawn. When it
exceeds a threshold, the algorithm steps in the other axis. All of it is integer arithmetic scaled
by a factor of 2, which is why `e2 = 2 × err`.

**`jl` and `jg`, not `jb` and `ja`.** `err`, `dy` and `e2` are all genuinely signed. This is the most
common place to get Bresenham wrong.

**Ten memory variables.** As with matrix multiplication (Chapter 44 §4), this algorithm needs more
live values than the 8086 has registers. Memory variables are the honest solution.

---

## Program 45.5 — Rectangles

```asm
; rects.asm — outlined and filled rectangles
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

        ; --- filled rectangles in a grid ---
        mov  bp, 0              ; colour
        mov  word [ry], 10
.row:
        mov  word [rx], 10
.col:
        mov  ax, [rx]
        mov  bx, [ry]
        mov  cx, 28             ; width
        mov  dx, 28             ; height
        mov  [w], cx
        mov  [h], dx
        mov  dl, bl             ; colour = y, for variety
        add  dl, byte [rx]
        call fillrect

        add  word [rx], 32
        cmp  word [rx], 300
        jb   .col
        add  word [ry], 32
        cmp  word [ry], 190
        jb   .row

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; fillrect — fill a rectangle.
;
;   In:  AX = x, BX = y, [w] = width, [h] = height, DL = colour
;   Destroys: nothing
;
;   Each row is a REP STOSB; the pointer advances by 320 between rows.
; ---------------------------------------------------------------
fillrect:
        push ax
        push bx
        push cx
        push dx
        push di

        mov  si, bx
        shl  si, 1
        mov  di, [rowtab + si]
        add  di, ax             ; DI = the top-left corner

        mov  ax, 320
        sub  ax, [w]
        mov  [stride], ax       ; how far to advance after each row

        mov  al, dl             ; STOSB stores AL
        mov  bx, [h]
        cld
.row:
        mov  cx, [w]
        rep  stosb
        add  di, [stride]       ; move to the start of the next row
        dec  bx
        jnz  .row

        pop  di
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ... build_rowtab and rowtab ...

rx:      dw  0
ry:      dw  0
w:       dw  0
h:       dw  0
stride:  dw  0
rowtab:  times 200 dw 0
```

**`stride = 320 − width`.** After `REP STOSB` has drawn one row, `DI` points just past the row's last
pixel. Adding `320 − width` lands it on the first pixel of the next row. Computing this once outside
the loop, rather than recomputing the row address each time, saves 20 clocks per row.

---

## Program 45.6 — Circles

Bresenham's circle algorithm — again, integer arithmetic only.

```asm
; circle.asm — midpoint circle algorithm
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

        ; --- concentric circles ---
        mov  bp, 5              ; radius
        mov  dl, 1
.next:
        mov  word [cx_], 160
        mov  word [cy_], 100
        mov  [r], bp
        call circle
        add  bp, 6
        inc  dl
        cmp  bp, 100
        jb   .next

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; circle — draw a circle centred at ([cx_], [cy_]) with radius [r].
;
;   Midpoint algorithm:
;       x = 0;  y = r;  d = 3 - 2r
;       while x <= y:
;           plot the 8 symmetric points
;           if d < 0:  d += 4x + 6
;           else:      d += 4(x - y) + 10;  y--
;           x++
;
;   Destroys:  AX, BX, CX, SI, DI
; ---------------------------------------------------------------
circle:
        mov  word [px], 0
        mov  ax, [r]
        mov  [py], ax
        shl  ax, 1
        neg  ax
        add  ax, 3              ; d = 3 - 2r
        mov  [d], ax

.loop:
        mov  ax, [px]
        cmp  ax, [py]
        jg   .done              ; SIGNED

        call plot8

        mov  ax, [d]
        or   ax, ax
        js   .d_negative

        ; d >= 0:  d += 4(x - y) + 10;  y--
        mov  ax, [px]
        sub  ax, [py]
        shl  ax, 1
        shl  ax, 1              ; 4(x - y)
        add  ax, 10
        add  [d], ax
        dec  word [py]
        jmp  .advance

.d_negative:
        ; d < 0:  d += 4x + 6
        mov  ax, [px]
        shl  ax, 1
        shl  ax, 1              ; 4x
        add  ax, 6
        add  [d], ax

.advance:
        inc  word [px]
        jmp  .loop
.done:
        ret

; ---------------------------------------------------------------
; plot8 — plot the eight symmetric points of the current octant.
;   Uses [cx_], [cy_], [px], [py] and DL.
; ---------------------------------------------------------------
plot8:
        push ax
        push bx

        mov  ax, [cx_]
        add  ax, [px]
        mov  bx, [cy_]
        add  bx, [py]
        call putpixel_safe
        mov  ax, [cx_]
        sub  ax, [px]
        call putpixel_safe
        mov  bx, [cy_]
        sub  bx, [py]
        call putpixel_safe
        mov  ax, [cx_]
        add  ax, [px]
        call putpixel_safe

        mov  ax, [cx_]
        add  ax, [py]
        mov  bx, [cy_]
        add  bx, [px]
        call putpixel_safe
        mov  ax, [cx_]
        sub  ax, [py]
        call putpixel_safe
        mov  bx, [cy_]
        sub  bx, [px]
        call putpixel_safe
        mov  ax, [cx_]
        add  ax, [py]
        call putpixel_safe

        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; putpixel_safe — like putpixel, but silently ignores anything off
;   the screen. Essential for circles that reach the edge.
; ---------------------------------------------------------------
putpixel_safe:
        push ax
        push bx
        push di
        cmp  ax, 320
        jae  .out               ; unsigned: catches negative x too
        cmp  bx, 200
        jae  .out
        shl  bx, 1
        mov  di, [rowtab + bx]
        add  di, ax
        mov  [es:di], dl
.out:
        pop  di
        pop  bx
        pop  ax
        ret

; ... build_rowtab and rowtab ...

cx_:    dw   0
cy_:    dw   0
r:      dw   0
px:     dw   0
py:     dw   0
d:      dw   0
rowtab: times 200 dw 0
```

### Explanation

**Eight-fold symmetry.** A circle is symmetric about both axes and both diagonals, so computing one
octant (from 0° to 45°) gives the other seven for free. That is why `plot8` exists and why the loop
runs only while `x ≤ y`.

**`cmp ax, 320` / `jae` catches negative coordinates too.** A negative `AX` is a large unsigned
number, so the single unsigned comparison rejects both `x < 0` and `x ≥ 320`. One test instead of
two — the same trick as Chapter 27 §9.1.

**No `MUL`, no `DIV`, no square roots.** The decision variable `d` is updated by additions and
shifts only.

---

## Program 45.7 — The 256-colour palette

```asm
; palette.asm — display all 256 colours
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax

        ; --- a 16 × 16 grid of 18 × 11 pixel blocks ---
        cld
        xor  bl, bl             ; BL = the colour
        xor  bp, bp             ; BP = the grid row
.gridrow:
        mov  si, 0              ; SI = the grid column
.gridcol:
        ; top-left of this block
        mov  ax, bp
        mov  dx, 11
        mul  dx                 ; y = row × 11
        mov  dx, 320
        mul  dx                 ; AX = y × 320  (DX:AX, but it fits)
        mov  di, ax
        mov  ax, si
        mov  dx, 18
        mul  dx
        add  di, ax             ; + column × 18

        ; draw an 18 × 11 block
        mov  al, bl
        mov  cx, 11
.blockrow:
        push cx
        push di
        mov  cx, 18
        rep  stosb
        pop  di
        add  di, 320
        pop  cx
        loop .blockrow

        inc  bl
        inc  si
        cmp  si, 16
        jb   .gridcol
        inc  bp
        cmp  bp, 16
        jb   .gridrow

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0
```

**The default mode 13h palette:**

| Range | Contents |
|-------|----------|
| 0–15 | the standard EGA/CGA 16 colours |
| 16–31 | 16 shades of grey |
| 32–55 | a hue ramp at full saturation and brightness |
| 56–247 | further hue/saturation/brightness combinations |
| 248–255 | black |

### 7.1 Changing the palette

Each colour is three 6-bit values — red, green, blue, each 0–63 — set through ports `0x3C8` and
`0x3C9`:

```asm
; ---------------------------------------------------------------
; setcolour — set palette entry AL to (DH, CH, CL) = (R, G, B), each 0-63
; ---------------------------------------------------------------
setcolour:
        push dx
        mov  dx, 0x3C8
        out  dx, al             ; which entry
        inc  dx                 ; port 0x3C9
        mov  al, dh
        out  dx, al             ; red
        mov  al, ch
        out  dx, al             ; green
        mov  al, cl
        out  dx, al             ; blue
        pop  dx
        ret
```

The three components must be written in order to consecutive `OUT`s to the *same* port — the VGA
has an internal counter that advances after each write. Writing them out of order, or with another
`OUT` in between, corrupts the entry.

A grey ramp:

```asm
        xor  al, al
.next:
        mov  dh, al
        shr  dh, 1
        shr  dh, 1              ; scale 0-255 down to 0-63
        mov  ch, dh
        mov  cl, dh
        call setcolour
        inc  al
        jnz  .next              ; wraps to 0 after 255
```

---

## Program 45.8 — A bouncing ball

```asm
; ball.asm — a ball bouncing around the screen
        cpu  8086
        org  0x100
%include "macros.inc"

RADIUS  equ  8

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

.frame:
        ; --- erase the ball at its old position ---
        mov  dl, 0              ; black
        call draw_ball

        ; --- move ---
        mov  ax, [bx_]
        add  ax, [vx]
        mov  [bx_], ax
        cmp  ax, RADIUS
        jle  .bounce_x
        cmp  ax, 320 - RADIUS
        jge  .bounce_x
        jmp  .move_y
.bounce_x:
        neg  word [vx]
.move_y:
        mov  ax, [by_]
        add  ax, [vy]
        mov  [by_], ax
        cmp  ax, RADIUS
        jle  .bounce_y
        cmp  ax, 200 - RADIUS
        jge  .bounce_y
        jmp  .draw
.bounce_y:
        neg  word [vy]

.draw:
        mov  dl, 14             ; yellow
        call draw_ball

        call vsync              ; wait for the vertical retrace

        ; --- quit on a key ---
        mov  ah, 0x01
        int  0x16
        jz   .frame             ; ZF = 1 -> no key waiting

        xor  ah, ah
        int  0x16               ; consume it
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; draw_ball — draw a filled circle of radius RADIUS at ([bx_],[by_])
;   in colour DL. Crude but adequate: test every pixel in the
;   bounding box against x² + y² <= r².
; ---------------------------------------------------------------
draw_ball:
        push ax
        push bx
        push cx
        push si

        mov  si, -RADIUS        ; SI = dy
.row:
        mov  cx, -RADIUS        ; CX = dx
.col:
        ; --- is dx² + dy² <= RADIUS²? ---
        mov  ax, cx
        imul cx                 ; AX = dx²
        mov  bx, ax
        mov  ax, si
        imul si                 ; AX = dy²
        add  ax, bx
        cmp  ax, RADIUS*RADIUS
        jg   .skip

        mov  ax, [bx_]
        add  ax, cx
        mov  bx, [by_]
        add  bx, si
        call putpixel_safe
.skip:
        inc  cx
        cmp  cx, RADIUS
        jle  .col
        inc  si
        cmp  si, RADIUS
        jle  .row

        pop  si
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; vsync — wait for the start of the vertical retrace.
;
;   Port 0x3DA bit 3 is 1 during the retrace. Waiting for a fresh
;   retrace makes the animation smooth and caps it at 70 frames a
;   second in mode 13h.
; ---------------------------------------------------------------
vsync:
        push ax
        push dx
        mov  dx, 0x3DA
.wait_end:
        in   al, dx
        test al, 8
        jnz  .wait_end          ; wait for any current retrace to finish
.wait_start:
        in   al, dx
        test al, 8
        jz   .wait_start        ; now wait for the next one to begin
        pop  dx
        pop  ax
        ret

; ... putpixel_safe, build_rowtab and rowtab ...

bx_:    dw   160                ; ball x
by_:    dw   100                ; ball y
vx:     dw   3                  ; x velocity
vy:     dw   2                  ; y velocity
rowtab: times 200 dw 0
```

### Explanation

**Erase, move, draw.** The simplest animation loop. It flickers slightly because the erase and the
draw are separated in time; a double-buffered version draws into a 64,000-byte off-screen buffer and
copies it with `REP MOVSW` during the retrace. Exercise 45.12.

**`vsync` is what makes it smooth.** Without it, the ball is drawn while the screen is being scanned
out, and you see it torn in half. Bit 3 of port `0x3DA` is the vertical retrace status; waiting for
a *fresh* retrace (not one already in progress) gives a consistent frame rate.

**The velocity reversal.** `neg word [vx]` flips the direction. Note the bounds test uses `RADIUS`
so the ball's edge, not its centre, hits the wall.

**`imul cx` with `CX` negative** works correctly because `IMUL` is the signed multiply. With `MUL`,
`dx = −3` would be treated as 65,533 and the test would never pass.

---

## Exercises

**45.1** `putpixel` has no bounds checking. Add it, using a single unsigned comparison per axis, and
say what the unchecked version does when `x = 400`.

**45.2** Compute the byte offset of pixel (200, 150) in mode 13h.

**45.3** Why is `REP STOSW` twice as fast as `REP STOSB` for a screen fill on an 8086 but not on an
8088?

**45.4** Write a routine that fills the screen with a vertical colour gradient — each column a
different colour. Why is it much slower than the horizontal version?

**45.5** Modify `hline` to clip to the screen edges rather than assuming valid coordinates.

**45.6** The row table costs 400 bytes and saves about 60 clocks per pixel. For how many pixel
writes does it pay for itself, assuming the table takes 200 × 25 clocks to build?

**45.7** Write a routine that draws a rectangle outline using four calls to `hline` and `vline`.

**45.8** In Bresenham's algorithm, why must `jl` and `jg` be used rather than `jb` and `ja`?

**45.9** Modify the circle routine to draw a *filled* circle by replacing the eight `putpixel` calls
with four `hline` calls.

**45.10** Write a routine that draws an ellipse.

**45.11** Set the palette so that colours 0–63 form a red ramp, 64–127 a green ramp and 128–191 a
blue ramp, then display them.

**45.12** Rewrite the bouncing ball with double buffering: draw into a 64,000-byte buffer in a second
segment, then `REP MOVSW` it to `0xA0000` during the retrace. How many clocks does the copy take, and
does it fit in one retrace period?

**45.13** Add a second ball with different velocities, and detect when the two collide.

**45.14** `draw_ball` tests 289 pixels to draw about 200. Rewrite it using `hline` for each row, with
the row width computed from the circle equation, and estimate the speed-up.

Answers in [Appendix H](H-exercise-solutions.md#chapter-45).

---

[← Programs: matrices](44-programs-matrices.md) · [Contents](README.md) · [Next: Debugging →](46-debugging.md)
