; merge.asm — merge two sorted arrays into a third
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, arr1           ; SI -> current element of arr1
        mov  di, arr2           ; DI -> current element of arr2
        mov  bx, result         ; BX -> where the next output goes
        mov  cx, len1           ; how many remain in arr1
        mov  dx, len2           ; how many remain in arr2

.next:
        or   cx, cx
        jz   .drain2            ; arr1 exhausted
        or   dx, dx
        jz   .drain1            ; arr2 exhausted

        mov  ax, [si]
        cmp  ax, [di]
        jg   .take2             ; signed: arr2's element is smaller
        ; take from arr1
        mov  [bx], ax
        inc  si
        inc  si
        dec  cx
        jmp  .advance
.take2:
        mov  ax, [di]
        mov  [bx], ax
        inc  di
        inc  di
        dec  dx
.advance:
        inc  bx
        inc  bx
        jmp  .next

.drain1:                        ; copy what is left of arr1
        or   cx, cx
        jz   .done
        mov  ax, [si]
        mov  [bx], ax
        inc  si
        inc  si
        inc  bx
        inc  bx
        dec  cx
        jmp  .drain1

.drain2:                        ; copy what is left of arr2
        or   dx, dx
        jz   .done
        mov  ax, [di]
        mov  [bx], ax
        inc  di
        inc  di
        inc  bx
        inc  bx
        dec  dx
        jmp  .drain2

.done:
        print msg
        mov  bx, result
        mov  cx, len1 + len2
.show:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .show
        newline
        exit 0

arr1:   dw   1, 5, 9, 12, 20
len1    equ  ($ - arr1) / 2
arr2:   dw   2, 3, 11, 15
len2    equ  ($ - arr2) / 2
result: times (len1 + len2) dw 0
msg:    db   'Merged: $'
