; revarr.asm — reverse an array in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        call show               ; before

        mov  si, arr            ; SI -> first element
        mov  di, arr + (len-1)*2 ; DI -> last element
        mov  cx, len
        shr  cx, 1              ; swap only half the elements
        jcxz .done              ; 0 or 1 elements -> nothing to do

.swap:
        mov  ax, [si]
        mov  bx, [di]
        mov  [si], bx
        mov  [di], ax
        inc  si
        inc  si
        dec  di
        dec  di
        loop .swap
.done:
        print arrow
        call show               ; after
        exit 0

; ---------------------------------------------------------------
; show — print the array as  10 20 30 ...
; ---------------------------------------------------------------
show:
        push ax
        push bx
        push cx
        mov  bx, arr
        mov  cx, len
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

arr:    dw   10, 20, 30, 40, 50, 60, 70
len     equ  ($ - arr) / 2
arrow:  db   '     becomes', 0x0D, 0x0A, '$'
