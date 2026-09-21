; buggy.asm
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, arr
        mov  cx, len
        xor  ax, ax
.next:
        add  ax, [si]
        inc  si
        loop .next

        call print_udec
        newline
        exit 0

arr:    dw   10, 20, 30, 40
len     equ  ($ - arr) / 2
