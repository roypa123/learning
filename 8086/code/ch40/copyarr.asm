; copyarr.asm — copy an array, and time the two methods
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- method 1: REP MOVSW ---
        cld
        mov  si, src
        mov  di, dst
        mov  cx, len
        rep  movsw              ; 9 + len × 17 clocks

        print msg1
        mov  bx, dst
        mov  cx, len
        call show

        ; --- method 2: a hand-written loop, for comparison ---
        mov  si, src
        mov  di, dst2
        mov  cx, len
.next:
        mov  ax, [si]           ; 8 + 5  = 13
        mov  [di], ax           ; 9 + 5  = 14
        inc  si                 ; 2
        inc  si                 ; 2
        inc  di                 ; 2
        inc  di                 ; 2
        loop .next              ; 17
                                ;         = 52 clocks per element

        print msg2
        mov  bx, dst2
        mov  cx, len
        call show
        exit 0

show:
        push ax
        push bx
        push cx
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

src:    dw   11, 22, 33, 44, 55, 66
len     equ  ($ - src) / 2
dst:    times len dw 0
dst2:   times len dw 0
msg1:   db   'REP MOVSW:   $'
msg2:   db   'Manual loop: $'
