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
