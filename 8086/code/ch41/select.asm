; select.asm — selection sort
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

        call selection_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array
        exit 0

; ---------------------------------------------------------------
; selection_sort — sort `arr` ascending, signed.
;
;   for i = 0 to n-2:
;       find the index m of the smallest element in arr[i..n-1]
;       swap arr[i] and arr[m]
;
;   Registers:
;     SI = address of arr[i]      (the outer position)
;     DI = address of arr[j]      (the scan position)
;     BX = address of the smallest found so far
;     CX = outer loop counter
;     DX = inner loop counter
; ---------------------------------------------------------------
selection_sort:
        mov  cx, len
        dec  cx
        jcxz .done

        mov  si, arr
.outer:
        mov  bx, si             ; assume arr[i] is the smallest
        mov  di, si
        add  di, 2              ; start scanning at i+1
        mov  dx, cx             ; this many elements remain to scan

.inner:
        mov  ax, [di]
        cmp  ax, [bx]
        jge  .not_smaller       ; SIGNED comparison
        mov  bx, di             ; a new minimum
.not_smaller:
        inc  di
        inc  di
        dec  dx
        jnz  .inner

        ; --- swap arr[i] with the minimum, if they differ ---
        cmp  bx, si
        je   .no_swap
        mov  ax, [si]
        mov  dx, [bx]
        mov  [si], dx
        mov  [bx], ax
.no_swap:
        inc  si
        inc  si
        loop .outer
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
before:   db   'Before: $'
after:    db   'After:  $'
