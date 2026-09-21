; rotarr.asm — rotate an array left by one position
;   [10,20,30,40,50]  ->  [20,30,40,50,10]
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        call show

        mov  cx, len
        cmp  cx, 2
        jb   .done              ; 0 or 1 elements — nothing to do

        mov  ax, [arr]          ; save element 0
        push ax

        ; --- shift everything down one place ---
        cld
        mov  si, arr + 2        ; source: element 1
        mov  di, arr            ; destination: element 0
        mov  cx, len - 1
        rep  movsw              ; the only memory-to-memory move there is

        pop  ax
        mov  [arr + (len-1)*2], ax   ; the saved element goes to the end

.done:
        print arrow
        call show
        exit 0

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

arr:    dw   10, 20, 30, 40, 50
len     equ  ($ - arr) / 2
arrow:  db   '  rotate left ->', 0x0D, 0x0A, '$'
