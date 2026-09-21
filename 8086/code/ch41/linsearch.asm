; linsearch.asm — find a value in an unsorted array
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print prompt
        call read_udec
        newline
        mov  [target], ax

        call linear_search
        jc   .not_found

        print foundmsg
        mov  ax, [index]
        call print_udec
        newline
        exit 0

.not_found:
        print nfmsg
        exit 1

; ---------------------------------------------------------------
; linear_search — find [target] in `arr`.
;
;   Out:  CF = 0 and [index] = the position, or CF = 1 if absent
; ---------------------------------------------------------------
linear_search:
        mov  bx, arr
        mov  cx, len
        mov  ax, [target]
        xor  si, si             ; SI = the current index
        jcxz .fail
.next:
        cmp  ax, [bx]
        je   .found
        inc  bx
        inc  bx
        inc  si
        loop .next
.fail:
        stc
        ret
.found:
        mov  [index], si
        clc
        ret

arr:      dw   45, 12, 78, 3, 99, 23, 67, 5
len       equ  ($ - arr) / 2
target:   dw   0
index:    dw   0
prompt:   db   'Search for: $'
foundmsg: db   'Found at index $'
nfmsg:    db   'Not found.', 0x0D, 0x0A, '$'
