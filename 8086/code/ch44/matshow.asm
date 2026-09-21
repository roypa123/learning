; matshow.asm — print a matrix as a grid
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS    equ  3
COLS    equ  4

start:
        mov  si, matrix
        call show_matrix
        exit 0

; ---------------------------------------------------------------
; show_matrix — print the ROWS × COLS word matrix at DS:SI
;
;   Prints each element right-aligned in a 6-column field.
;   Destroys:  AX, BX, CX, DX, SI
; ---------------------------------------------------------------
show_matrix:
        mov  bx, ROWS           ; BX = rows remaining
.row:
        mov  cx, COLS           ; CX = columns remaining in this row
.col:
        lodsw                   ; AX = the element, SI advances by 2
        call print_field
        loop .col

        newline
        dec  bx
        jnz  .row
        ret

; ---------------------------------------------------------------
; print_field — print AX right-aligned in six columns.
;   Destroys:  nothing
; ---------------------------------------------------------------
print_field:
        push ax
        push bx
        push cx
        push dx

        ; --- count the digits (and the sign) ---
        mov  bx, ax
        xor  cx, cx             ; CX = width needed
        or   bx, bx
        jns  .positive
        inc  cx                 ; one column for the '-'
        neg  bx
.positive:
        mov  ax, bx
        mov  bx, 10
.count:
        xor  dx, dx
        div  bx
        inc  cx
        or   ax, ax
        jnz  .count

        ; --- emit leading spaces ---
        mov  ax, 6
        sub  ax, cx
        jbe  .no_pad
        mov  cx, ax
.pad:
        putc ' '
        loop .pad
.no_pad:
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        push ax
        push bx
        push cx
        push dx
        call print_sdec

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

matrix: dw   1,    2,    3,    4
        dw   50,   60,   70,   80
        dw   -900, 1000, -11,  12
