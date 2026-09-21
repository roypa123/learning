; dotprod.asm — dot product of two vectors
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, vec1
        mov  di, vec2
        mov  cx, len
        xor  bx, bx             ; BX = the low word of the accumulator
        xor  bp, bp             ; BP = the high word
        jcxz .done

.next:
        mov  ax, [si]
        imul word [di]          ; DX:AX = signed product of the two elements
        add  bx, ax             ; accumulate into BP:BX
        adc  bp, dx             ; with the full 32-bit carry
        inc  si
        inc  si
        inc  di
        inc  di
        loop .next

.done:
        print msg
        mov  dx, bp
        mov  ax, bx
        call print_hex32
        newline

        ; decimal, if it fits in 16 bits
        or   bp, bp
        jz   .small
        cmp  bp, 0xFFFF         ; a negative value sign-extends to FFFF
        jne  .big
.small:
        print dmsg
        mov  ax, bx
        call print_sdec
        newline
.big:
        exit 0

vec1:   dw   1, 2, 3, 4, 5
len     equ  ($ - vec1) / 2
vec2:   dw   10, 20, 30, 40, 50
msg:    db   'Dot product (hex): 0x$'
dmsg:   db   'Dot product (dec): $'
