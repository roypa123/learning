; insert.asm — insertion sort
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"
%include "show.inc"

start:
        print before
        mov  bx, arr
        mov  cx, len
        call show_array

        call insertion_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array
        exit 0

; ---------------------------------------------------------------
; insertion_sort — sort `arr` ascending, signed.
;
;   for i = 1 to n-1:
;       key = arr[i]
;       j = i - 1
;       while j >= 0 and arr[j] > key:
;           arr[j+1] = arr[j]
;           j = j - 1
;       arr[j+1] = key
;
;   Registers:
;     SI = address of arr[i]
;     DI = address of arr[j]
;     AX = the key being inserted
;     CX = outer counter
; ---------------------------------------------------------------
insertion_sort:
        mov  cx, len
        dec  cx
        jcxz .done

        mov  si, arr + 2        ; start at i = 1
.outer:
        mov  ax, [si]           ; AX = key
        mov  di, si
        sub  di, 2              ; DI -> arr[i-1]

.shift:
        cmp  di, arr
        jb   .place             ; ran off the front of the array
        mov  bx, [di]
        cmp  bx, ax
        jle  .place             ; SIGNED: found the insertion point
        mov  [di+2], bx         ; shift this element right
        sub  di, 2
        jmp  .shift

.place:
        mov  [di+2], ax         ; drop the key into the gap
        inc  si
        inc  si
        loop .outer
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
before:   db   'Before: $'
after:    db   'After:  $'
