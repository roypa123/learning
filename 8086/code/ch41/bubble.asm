; bubble.asm — bubble sort with an early-exit optimisation
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

        call bubble_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array

        print passmsg
        mov  ax, [passes]
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; bubble_sort — sort the word array at `arr`, ascending, signed.
;
;   Outer loop: up to len-1 passes.
;   Inner loop: compare each adjacent pair, swap if out of order.
;   Early exit: if a pass makes no swaps, the array is sorted.
;
;   Destroys: AX, BX, CX, DX, SI
; ---------------------------------------------------------------
bubble_sort:
        mov  cx, len
        dec  cx                 ; at most len-1 passes
        jcxz .done              ; 0 or 1 elements

.pass:
        push cx                 ; the inner loop needs CX
        inc  word [passes]
        xor  dx, dx             ; DX = "a swap happened this pass" flag
        mov  si, arr            ; SI walks the pairs

.compare:
        mov  ax, [si]
        mov  bx, [si+2]
        cmp  ax, bx
        jle  .in_order          ; SIGNED: ax <= bx is fine
        ; out of order — swap them
        mov  [si], bx
        mov  [si+2], ax
        mov  dx, 1              ; remember that we swapped
.in_order:
        inc  si
        inc  si
        loop .compare

        pop  cx
        or   dx, dx
        jz   .done              ; a clean pass means we are finished
        loop .pass
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
passes:   dw   0
before:   db   'Before: $'
after:    db   'After:  $'
passmsg:  db   'Passes: $'
