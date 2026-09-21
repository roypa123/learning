; mul16.asm — 16 × 16 = 32-bit multiply
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        mov  bx, [num2]
        mul  bx                 ; DX:AX = AX × BX.  DESTROYS DX.

        push ax                 ; save the low word
        push dx

        print msg

        pop  dx
        pop  ax
        push ax
        push dx
        call print_hex32        ; print DX:AX as 8 hex digits

        print dmsg

        ; --- also print it in decimal, if it fits in 16 bits ---
        pop  dx
        pop  ax
        or   dx, dx
        jnz  .too_big           ; the high word is non-zero
        call print_udec
        newline
        exit 0

.too_big:
        print bigmsg
        exit 0

num1:   dw   1234
num2:   dw   5678
msg:    db   '1234 * 5678 = 0x$'
dmsg:   db   ' = $'
bigmsg: db   '(too large for 16 bits)', 0x0D, 0x0A, '$'
