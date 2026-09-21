; matmul.asm — multiply two matrices
;
;   C[i][k] = sum over j of  A[i][j] × B[j][k]
;
;   A is ROWS_A × COLS_A
;   B is COLS_A × COLS_B      (the inner dimensions must match)
;   C is ROWS_A × COLS_B
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS_A  equ  2
COLS_A  equ  3
COLS_B  equ  2

start:
        print amsg
        mov  si, matA
        mov  bx, ROWS_A
        mov  bp, COLS_A
        call show_rect
        print bmsg
        mov  si, matB
        mov  bx, COLS_A
        mov  bp, COLS_B
        call show_rect

        call matmul

        print cmsg
        mov  si, matC
        mov  bx, ROWS_A
        mov  bp, COLS_B
        call show_rect
        exit 0

; ---------------------------------------------------------------
; matmul — C = A × B
;
;   Three nested loops. The index arithmetic is kept in memory
;   variables rather than registers, because the 8086 does not have
;   enough registers for i, j, k, three base pointers and an
;   accumulator all at once.
;
;   Destroys:  AX, BX, CX, DX, SI, DI
; ---------------------------------------------------------------
matmul:
        mov  word [i], 0
.loop_i:
        mov  word [k], 0
.loop_k:
        mov  word [acc], 0
        mov  word [j], 0

.loop_j:
        ; --- AX = A[i][j] ---
        mov  ax, [i]
        mov  dx, COLS_A
        mul  dx                 ; i × COLS_A
        add  ax, [j]
        shl  ax, 1
        mov  si, ax
        mov  ax, [matA + si]

        ; --- BX = B[j][k] ---
        push ax
        mov  ax, [j]
        mov  dx, COLS_B
        mul  dx                 ; j × COLS_B
        add  ax, [k]
        shl  ax, 1
        mov  di, ax
        mov  bx, [matB + di]
        pop  ax

        ; --- acc += A[i][j] × B[j][k] ---
        imul bx                 ; DX:AX = the signed product
        add  [acc], ax          ; we keep only the low 16 bits
        ; (a production version would accumulate DX:AX in 32 bits)

        inc  word [j]
        mov  ax, [j]
        cmp  ax, COLS_A
        jb   .loop_j

        ; --- store C[i][k] ---
        mov  ax, [i]
        mov  dx, COLS_B
        mul  dx
        add  ax, [k]
        shl  ax, 1
        mov  di, ax
        mov  ax, [acc]
        mov  [matC + di], ax

        inc  word [k]
        mov  ax, [k]
        cmp  ax, COLS_B
        jb   .loop_k

        inc  word [i]
        mov  ax, [i]
        cmp  ax, ROWS_A
        jb   .loop_i
        ret

; ... show_rect and print_field ...

matA:   dw   1, 2, 3
        dw   4, 5, 6
matB:   dw   7,  8
        dw   9,  10
        dw   11, 12
matC:   times ROWS_A*COLS_B dw 0
i:      dw   0
j:      dw   0
k:      dw   0
acc:    dw   0
amsg:   db   'A (2x3) =', 0x0D, 0x0A, '$'
bmsg:   db   'B (3x2) =', 0x0D, 0x0A, '$'
cmsg:   db   'A x B (2x2) =', 0x0D, 0x0A, '$'
