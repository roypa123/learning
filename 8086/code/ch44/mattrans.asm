; mattrans.asm — transpose a matrix
;
;   T[j][i] = M[i][j]
;
;   Note that a non-square transpose has different dimensions, so it
;   cannot be done in place. This version writes to a second matrix.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS    equ  3
COLS    equ  4

start:
        print omsg
        mov  si, matrix
        mov  bx, ROWS
        mov  bp, COLS
        call show_rect

        call transpose

        print tmsg
        mov  si, result
        mov  bx, COLS           ; the transpose is COLS × ROWS
        mov  bp, ROWS
        call show_rect
        exit 0

; ---------------------------------------------------------------
; transpose — result[j][i] = matrix[i][j]
;
;   Walk the SOURCE sequentially and compute the destination index.
;   That way the expensive index arithmetic happens once per element
;   rather than twice.
;
;   Registers:
;     SI = source pointer, walking row-major
;     DI = destination offset, computed per element
;     BX = current row i
;     CX = current column j
; ---------------------------------------------------------------
transpose:
        mov  si, matrix
        xor  bx, bx             ; i = 0
.row:
        xor  cx, cx             ; j = 0
.col:
        ; destination index = j × ROWS + i
        mov  ax, cx
        mov  dx, ROWS
        mul  dx                 ; AX = j × ROWS
        add  ax, bx             ; + i
        shl  ax, 1              ; × 2 for words
        mov  di, ax

        mov  ax, [si]           ; the source element
        mov  [result + di], ax

        inc  si
        inc  si
        inc  cx
        cmp  cx, COLS
        jb   .col

        inc  bx
        cmp  bx, ROWS
        jb   .row
        ret

; ---------------------------------------------------------------
; show_rect — print a BX × BP word matrix at DS:SI
; ---------------------------------------------------------------
show_rect:
        push bx
.row:
        mov  cx, bp
.col:
        lodsw
        call print_field
        loop .col
        newline
        dec  bx
        jnz  .row
        pop  bx
        ret

; ... print_field from Program 44.1 ...

matrix: dw   1,  2,  3,  4
        dw   5,  6,  7,  8
        dw   9, 10, 11, 12
result: times ROWS*COLS dw 0
omsg:   db   'Original (3x4):', 0x0D, 0x0A, '$'
tmsg:   db   'Transpose (4x3):', 0x0D, 0x0A, '$'
