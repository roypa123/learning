; minmax.asm — find the smallest and largest elements
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  cx, len
        jcxz .empty

        mov  bx, arr
        mov  ax, [bx]           ; start with element 0 as BOTH min and max
        mov  [min], ax
        mov  [max], ax
        dec  cx                 ; we have already examined one element
        jcxz .done
        inc  bx
        inc  bx

.next:
        mov  ax, [bx]
        cmp  ax, [min]
        jge  .not_smaller       ; SIGNED comparison — the data may be negative
        mov  [min], ax
.not_smaller:
        cmp  ax, [max]
        jle  .not_bigger
        mov  [max], ax
.not_bigger:
        inc  bx
        inc  bx
        loop .next

.done:
        print minmsg
        mov  ax, [min]
        call print_sdec
        newline
        print maxmsg
        mov  ax, [max]
        call print_sdec
        newline
        exit 0

.empty:
        print emptymsg
        exit 1

arr:      dw   45, -12, 300, 7, -250, 99, 1000, 0
len       equ  ($ - arr) / 2
min:      dw   0
max:      dw   0
minmsg:   db   'Minimum: $'
maxmsg:   db   'Maximum: $'
emptymsg: db   'Empty array.', 0x0D, 0x0A, '$'
