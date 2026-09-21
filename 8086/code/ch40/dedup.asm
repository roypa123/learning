; dedup.asm — remove adjacent duplicates from a sorted array, in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  cx, len
        call show

        cmp  cx, 2
        jb   .done              ; 0 or 1 elements — nothing to do

        mov  si, arr + 2        ; SI -> the element being examined (index 1)
        mov  di, arr + 2        ; DI -> where the next kept element goes
        mov  bx, [arr]          ; BX = the last value we kept
        mov  cx, len - 1
        mov  dx, 1              ; DX counts the elements kept

.next:
        mov  ax, [si]
        cmp  ax, bx
        je   .duplicate         ; same as the previous — skip it
        mov  [di], ax           ; keep it
        mov  bx, ax
        inc  di
        inc  di
        inc  dx
.duplicate:
        inc  si
        inc  si
        loop .next

        mov  [newlen], dx

.done:
        print arrow
        mov  cx, [newlen]
        call show
        print cntmsg
        mov  ax, [newlen]
        call print_udec
        newline
        exit 0

; show — print CX elements starting at arr
show:
        push ax
        push bx
        push cx
        mov  bx, arr
        jcxz .out
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
.out:
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

arr:     dw   1, 1, 2, 3, 3, 3, 5, 8, 8, 13
len      equ  ($ - arr) / 2
newlen:  dw   len
arrow:   db   '  dedup ->', 0x0D, 0x0A, '$'
cntmsg:  db   'Elements remaining: $'
