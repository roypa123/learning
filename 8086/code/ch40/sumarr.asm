; sumarr.asm — sum an array of 16-bit values
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        cld                     ; forwards
        mov  si, arr            ; DS:SI -> the array
        mov  cx, len            ; element count
        xor  ax, ax             ; AX will hold the running total
        xor  dx, dx             ; DX:AX = a 32-bit accumulator

        jcxz .done              ; guard against an empty array
.next:
        push ax
        lodsw                   ; AX <- [SI], SI += 2
        mov  bx, ax
        pop  ax
        add  ax, bx             ; add to the low word
        adc  dx, 0              ; propagate any carry into the high word
        loop .next
.done:
        print msg
        call print_hex32        ; DX:AX
        newline

        print dmsg
        or   dx, dx
        jnz  .too_big
        call print_udec
        newline
        exit 0
.too_big:
        print bigmsg
        exit 0

arr:    dw   100, 250, 375, 4000, 12000, 65000, 300, 42
len     equ  ($ - arr) / 2
msg:    db   'Sum (hex): 0x$'
dmsg:   db   'Sum (dec): $'
bigmsg: db   '(exceeds 16 bits)', 0x0D, 0x0A, '$'
