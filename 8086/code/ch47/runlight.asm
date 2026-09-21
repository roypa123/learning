; runlight.asm — a single lit LED running back and forth
        cpu  8086
        org  0x100
%include "macros.inc"

PORTA   equ  0x00
CTRL    equ  0x06

start:
        mov  al, 0x80           ; all ports output
        out  CTRL, al

        mov  al, 0x01           ; start with PA0 lit
.right:
        out  PORTA, al
        call delay
        shl  al, 1              ; move the bit left
        jnc  .right             ; CF = 1 when the bit falls off the top

        mov  al, 0x80           ; start again from PA7
.left:
        out  PORTA, al
        call delay
        shr  al, 1
        jnc  .left

        ; --- quit on a key, otherwise repeat ---
        mov  ah, 0x0B
        int  0x21
        or   al, al
        jnz  .done
        mov  al, 0x01
        jmp  .right

.done:
        xor  al, al
        out  PORTA, al
        exit 0

; ---------------------------------------------------------------
; delay — a crude software delay of roughly 0.1 s at 5 MHz.
;   Nested loops: 200 × 65536 iterations × ~4 clocks.
;   Chapter 48 does this properly with a hardware timer.
; ---------------------------------------------------------------
delay:
        push ax
        push cx
        push dx
        mov  dx, 8
.outer:
        xor  cx, cx             ; 65536 iterations
.inner:
        loop .inner
        dec  dx
        jnz  .outer
        pop  dx
        pop  cx
        pop  ax
        ret
