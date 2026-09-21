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
