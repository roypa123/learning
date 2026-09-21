; sortsearch.asm — sort an array, then binary search it
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

        call insertion_sort     ; from Program 41.3

        print after
        mov  bx, arr
        mov  cx, len
        call show_array

.again:
        print prompt
        call read_udec
        jc   .quit              ; Enter with no digits -> quit
        newline
        mov  [target], ax

        call binary_search      ; from Program 41.5
        jc   .nf
        print foundmsg
        mov  ax, [index]
        call print_udec
        newline
        jmp  .again
.nf:
        print nfmsg
        jmp  .again

.quit:
        newline
        exit 0

; ... insertion_sort and binary_search go here, unchanged ...

arr:      dw   64, 34, 25, 12, 22, 11, 90, 5
len       equ  ($ - arr) / 2
target:   dw   0
index:    dw   0
steps:    dw   0
before:   db   'Before: $'
after:    db   'Sorted: $'
prompt:   db   'Search (Enter to quit): $'
foundmsg: db   'Found at index $'
nfmsg:    db   'Not found.', 0x0D, 0x0A, '$'
