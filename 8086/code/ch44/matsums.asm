; matsums.asm — sum each row and each column
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS    equ  3
COLS    equ  4

start:
        print mmsg
        mov  si, matrix
        mov  bx, ROWS
        mov  bp, COLS
        call show_rect

        ; --- row sums: sequential, one pass ---
        print rmsg
        mov  si, matrix
        mov  bx, ROWS
.row:
        xor  dx, dx             ; DX = this row's total
        mov  cx, COLS
.rcol:
        lodsw
        add  dx, ax
        loop .rcol
        mov  ax, dx
        call print_field
        dec  bx
        jnz  .row
        newline

        ; --- column sums: strided ---
        print cmsg
        xor  bp, bp             ; BP = the column index
.col:
        mov  si, bp
        shl  si, 1              ; SI = column offset in bytes
        xor  dx, dx
        mov  cx, ROWS
.crow:
        mov  ax, [matrix + si]
        add  dx, ax
        add  si, COLS * 2       ; advance one whole row
        loop .crow
        mov  ax, dx
        call print_field
        inc  bp
        cmp  bp, COLS
        jb   .col
        newline
        exit 0

; ... show_rect and print_field ...

matrix: dw   1,  2,  3,  4
        dw   5,  6,  7,  8
        dw   9, 10, 11, 12
mmsg:   db   'Matrix:', 0x0D, 0x0A, '$'
rmsg:   db   'Row sums:', 0x0D, 0x0A, '$'
cmsg:   db   'Col sums:', 0x0D, 0x0A, '$'
