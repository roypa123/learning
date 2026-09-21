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
