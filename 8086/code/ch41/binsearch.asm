; binsearch.asm — binary search of a sorted array
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print prompt
        call read_udec
        newline
        mov  [target], ax

        call binary_search
        jc   .not_found

        print foundmsg
        mov  ax, [index]
        call print_udec
        print stepmsg
        mov  ax, [steps]
        call print_udec
        newline
        exit 0

.not_found:
        print nfmsg
        mov  ax, [steps]
        call print_udec
        newline
        exit 1

; ---------------------------------------------------------------
; binary_search — find [target] in the sorted array `arr`.
;
;   lo = 0;  hi = n-1
;   while lo <= hi:
;       mid = (lo + hi) / 2
;       if arr[mid] == target:  found
;       if arr[mid] <  target:  lo = mid + 1
;       else:                   hi = mid - 1
;
;   Registers:
;     SI = lo (an index)
;     DI = hi (an index)
;     BX = mid (an index), then mid × 2 (a byte offset)
;
;   Out:  CF = 0 and [index] = the position, or CF = 1
; ---------------------------------------------------------------
binary_search:
        xor  si, si             ; lo = 0
        mov  di, len - 1        ; hi = n-1
        mov  word [steps], 0

.loop:
        cmp  si, di
        ja   .fail              ; lo > hi -> not present
        inc  word [steps]

        ; mid = (lo + hi) / 2
        mov  bx, si
        add  bx, di
        shr  bx, 1              ; BX = mid, as an index
        mov  dx, bx             ; keep the index
        shl  bx, 1              ; BX = mid × 2, a byte offset

        mov  ax, [arr + bx]
        cmp  ax, [target]
        je   .found
        jb   .go_right          ; UNSIGNED — the data here is unsigned

        ; arr[mid] > target -> search the left half
        mov  di, dx
        or   dx, dx
        jz   .fail              ; mid = 0 and we need to go left -> absent
        dec  di                 ; hi = mid - 1
        jmp  .loop

.go_right:
        mov  si, dx
        inc  si                 ; lo = mid + 1
        jmp  .loop

.found:
        mov  [index], dx
        clc
        ret
.fail:
        stc
        ret

arr:      dw   3, 7, 12, 19, 23, 34, 45, 56, 67, 78, 89, 91, 95, 99
len       equ  ($ - arr) / 2
target:   dw   0
index:    dw   0
steps:    dw   0
prompt:   db   'Search for: $'
foundmsg: db   'Found at index $'
stepmsg:  db   ' in $'
nfmsg:    db   'Not found. Steps: $'
