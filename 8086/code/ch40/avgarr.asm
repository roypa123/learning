; avgarr.asm — mean of an array, with the remainder
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  bx, arr
        mov  cx, len
        xor  ax, ax
        xor  dx, dx
        jcxz .empty
.next:
        add  ax, [bx]
        adc  dx, 0
        inc  bx
        inc  bx
        loop .next

        ; DX:AX = the total.  Divide by the count.
        mov  bx, len
        div  bx                 ; AX = mean, DX = remainder
                                ; safe: DX < BX before the divide, because
                                ; total / count <= 65535 for these values
        mov  [mean], ax
        mov  [rem], dx

        print msg
        mov  ax, [mean]
        call print_udec
        print rmsg
        mov  ax, [rem]
        call print_udec
        print dmsg
        mov  ax, len
        call print_udec
        newline
        exit 0

.empty:
        print emptymsg
        exit 1

arr:      dw   10, 20, 30, 40, 55
len       equ  ($ - arr) / 2
mean:     dw   0
rem:      dw   0
msg:      db   'Mean = $'
rmsg:     db   ' remainder $'
dmsg:     db   '/$'
emptymsg: db   'Empty array.', 0x0D, 0x0A, '$'
