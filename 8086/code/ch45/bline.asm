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
