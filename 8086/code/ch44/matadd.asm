; matadd.asm — add two matrices element by element
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS    equ  3
COLS    equ  3
COUNT   equ  ROWS * COLS

start:
        print amsg
        mov  si, matA
        call show_matrix
        print bmsg
        mov  si, matB
        call show_matrix

        ; --- the addition ---
        mov  si, matA
        mov  di, matB
        mov  bx, matC
        mov  cx, COUNT
.next:
        mov  ax, [si]
        add  ax, [di]
        mov  [bx], ax
        inc  si
        inc  si
        inc  di
        inc  di
        inc  bx
        inc  bx
        loop .next

        print cmsg
        mov  si, matC
        call show_matrix
        exit 0

; ... show_matrix and print_field from Program 44.1 ...

matA:   dw   1, 2, 3
        dw   4, 5, 6
        dw   7, 8, 9
matB:   dw   10, 20, 30
        dw   40, 50, 60
        dw   70, 80, 90
matC:   times COUNT dw 0
amsg:   db   'A =', 0x0D, 0x0A, '$'
bmsg:   db   'B =', 0x0D, 0x0A, '$'
cmsg:   db   'A + B =', 0x0D, 0x0A, '$'
