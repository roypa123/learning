; matident.asm — is this matrix the identity?
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

N       equ  4                  ; square, N × N

start:
        mov  si, mat1
        call is_identity
        jc   .no1
        print y1
        jmp  .second
.no1:   print n1

.second:
        mov  si, mat2
        call is_identity
        jc   .no2
        print y2
        exit 0
.no2:   print n2
        exit 0

; ---------------------------------------------------------------
; is_identity — test the N×N word matrix at DS:SI
;
;   Out:  CF = 0 if it is the identity, CF = 1 otherwise
;   Destroys: AX, BX, CX, SI
; ---------------------------------------------------------------
is_identity:
        xor  bx, bx             ; BX = the row index
.row:
        mov  cx, 0              ; CX = the column index
.col:
        lodsw                   ; AX = M[bx][cx], SI advances
        cmp  bx, cx
        je   .diagonal
        ; off-diagonal: must be 0
        or   ax, ax
        jnz  .fail
        jmp  .next
.diagonal:
        ; on the diagonal: must be 1
        cmp  ax, 1
        jne  .fail
.next:
        inc  cx
        cmp  cx, N
        jb   .col
        inc  bx
        cmp  bx, N
        jb   .row
        clc
        ret
.fail:
        stc
        ret

mat1:   dw   1, 0, 0, 0
        dw   0, 1, 0, 0
        dw   0, 0, 1, 0
        dw   0, 0, 0, 1
mat2:   dw   1, 0, 0, 0
        dw   0, 1, 0, 0
        dw   0, 0, 2, 0
        dw   0, 0, 0, 1
y1:     db   'Matrix 1 IS the identity.', 0x0D, 0x0A, '$'
n1:     db   'Matrix 1 is not the identity.', 0x0D, 0x0A, '$'
y2:     db   'Matrix 2 IS the identity.', 0x0D, 0x0A, '$'
n2:     db   'Matrix 2 is not the identity.', 0x0D, 0x0A, '$'
