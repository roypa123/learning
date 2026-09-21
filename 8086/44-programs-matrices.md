# Chapter 44 — Programs: matrices

[← Number conversion](43-programs-number-conversion.md) · [Contents](README.md) · [Next: Programs: graphics →](45-programs-graphics.md)

---

## Goal

Two-dimensional arrays: how they are laid out, how the index arithmetic is derived, and seven
complete programs — display, addition, transpose, multiplication, row and column sums, the
identity test, and a determinant.

All assume `macros.inc` and `io.inc` from Chapter 39 §0.

---

## 1. Laying out a 2-D array

Memory is one-dimensional. A matrix has to be flattened, and there are two choices:

**Row-major** — rows stored one after another. This is what C, Pascal and this book use.

```
   M = | 1  2  3 |
       | 4  5  6 |

   memory:  1  2  3  4  5  6
            └─row 0─┘└─row 1─┘
```

**Column-major** — columns stored one after another. FORTRAN and MATLAB use it.

```
   memory:  1  4  2  5  3  6
```

### 1.1 The address formula

For a row-major matrix with `COLS` columns and elements of `SIZE` bytes:

```
   address of M[row][col]  =  base  +  (row × COLS + col) × SIZE
```

For a 3×4 word matrix (`COLS = 4`, `SIZE = 2`):

```
   M[0][0] -> base + 0
   M[0][3] -> base + (0×4 + 3)×2 = base + 6
   M[1][0] -> base + (1×4 + 0)×2 = base + 8
   M[2][2] -> base + (2×4 + 2)×2 = base + 20
```

### 1.2 The instruction sequence

```asm
; AX = M[row][col], with row in AL and col in BL
        mov  al, [row]
        xor  ah, ah
        mov  cx, COLS
        mul  cx                 ; AX = row × COLS
        mov  bl, [col]
        xor  bh, bh
        add  ax, bx             ; AX = row × COLS + col
        shl  ax, 1              ; × 2, because elements are words
        mov  si, ax
        mov  ax, [matrix + si]  ; the element
```

**`shl ax, 1` for words, nothing for bytes, `shl` twice for doublewords.** The element size is the
last multiplication and it is always a power of two, so it is always a shift.

### 1.3 Walking a whole matrix

For a full traversal you do not need the formula at all — just advance a pointer:

```asm
        mov  si, matrix
        mov  cx, ROWS * COLS
.next:  mov  ax, [si]
        ; ...
        inc  si
        inc  si
        loop .next
```

**Use the formula only for random access.** Sequential access should walk a pointer, which is 5 EA
clocks instead of a multiply's 118.

---

## Program 44.1 — Display a matrix

```asm
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
```

**Output:**

```
     1     2     3     4
    50    60    70    80
  -900  1000   -11    12
```

### Explanation

**`lodsw` walks the matrix in row-major order**, which is exactly the order we want to print. No
index arithmetic at all.

**Nested loops with `BX` and `CX`.** The inner loop uses `CX` and `LOOP`; the outer uses `BX` and
`DEC`/`JNZ`. Using different registers avoids the `push cx`/`pop cx` of Chapter 27 §6.4.

**`print_field` counts digits by dividing**, then pads. The double save/restore around `print_sdec`
is because `print_field` must preserve everything and `print_sdec` needs `AX`.

---

## Program 44.2 — Matrix addition

```asm
; matadd.asm — add two matrices element by element
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

ROWS    equ  3
COLS    equ  3
COUNT   equ  ROWS * COLS

start:
        print amsg
        mov  si, matA
        call show_matrix
        print bmsg
        mov  si, matB
        call show_matrix

        ; --- the addition ---
        mov  si, matA
        mov  di, matB
        mov  bx, matC
        mov  cx, COUNT
.next:
        mov  ax, [si]
        add  ax, [di]
        mov  [bx], ax
        inc  si
        inc  si
        inc  di
        inc  di
        inc  bx
        inc  bx
        loop .next

        print cmsg
        mov  si, matC
        call show_matrix
        exit 0

; ... show_matrix and print_field from Program 44.1 ...

matA:   dw   1, 2, 3
        dw   4, 5, 6
        dw   7, 8, 9
matB:   dw   10, 20, 30
        dw   40, 50, 60
        dw   70, 80, 90
matC:   times COUNT dw 0
amsg:   db   'A =', 0x0D, 0x0A, '$'
bmsg:   db   'B =', 0x0D, 0x0A, '$'
cmsg:   db   'A + B =', 0x0D, 0x0A, '$'
```

**Output:**

```
A =
     1     2     3
     4     5     6
     7     8     9
B =
    10    20    30
    40    50    60
    70    80    90
A + B =
    11    22    33
    44    55    66
    77    88    99
```

### Explanation

**Addition is element-wise, so the 2-D structure is irrelevant.** The loop treats all nine elements
as one flat array. This is a general principle: any operation that touches each element
independently — addition, subtraction, scaling, negation — needs no index arithmetic.

**Three pointers and one counter.** `SI`, `DI` and `BX` walk the three matrices in step.

---

## Program 44.3 — Transpose

```asm
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
```

**Output:**

```
Original (3x4):
     1     2     3     4
     5     6     7     8
     9    10    11    12
Transpose (4x3):
     1     5     9
     2     6    10
     3     7    11
     4     8    12
```

### Explanation

**Walk the source sequentially, compute the destination.** The alternative — walking the destination
and computing the source — needs exactly the same arithmetic, but this way `SI` advances with two
`INC`s instead of a multiply.

**The multiply is `j × ROWS`, not `j × COLS`.** The destination matrix has `ROWS` columns, because
it is the transpose. Getting this backwards is the classic transpose bug, and it produces a garbled
but plausible-looking result.

**In-place transpose** is possible only for a square matrix, by swapping `M[i][j]` with `M[j][i]` for
`j > i`. Exercise 44.5.

---

## Program 44.4 — Matrix multiplication

```asm
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
```

**Output:**

```
A (2x3) =
     1     2     3
     4     5     6
B (3x2) =
     7     8
     9    10
    11    12
A x B (2x2) =
    58    64
   139   154
```

Check: C[0][0] = 1×7 + 2×9 + 3×11 = 7 + 18 + 33 = 58 ✔
C[0][1] = 1×8 + 2×10 + 3×12 = 8 + 20 + 36 = 64 ✔
C[1][0] = 4×7 + 5×9 + 6×11 = 28 + 45 + 66 = 139 ✔
C[1][1] = 4×8 + 5×10 + 6×12 = 32 + 50 + 72 = 154 ✔

### Explanation

**Loop indices in memory, not registers.** Three loop counters plus three computed addresses plus an
accumulator is seven live values. The 8086 has four usable pointer registers. Memory variables cost
6 EA clocks each and keep the code readable; the alternative is a stack frame (Chapter 28 §5).

**The cost.** Two multiplies per element of the inner loop, each 118–133 clocks, plus the `IMUL`.
For a 2×3 by 3×2 product that is 12 inner iterations × ~400 clocks ≈ 4,800 clocks. For 10×10 by
10×10 it is 1,000 iterations × 400 ≈ 400,000 clocks = 80 ms.

**The optimisation.** The index multiplies are loop-invariant in part: `i × COLS_A` does not change
within the `j` loop. Hoisting it out, and incrementing pointers instead of recomputing addresses,
removes almost all the multiplies. Exercise 44.8.

---

## Program 44.5 — Row and column sums

```asm
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
```

**Output:**

```
Matrix:
     1     2     3     4
     5     6     7     8
     9    10    11    12
Row sums:
    10    26    42
Col sums:
    15    18    21    24
```

### Explanation

**Row sums walk sequentially** — `LODSW` through consecutive elements, because a row *is*
consecutive in row-major layout.

**Column sums stride.** Each step advances by `COLS × 2` bytes — a whole row — to reach the same
column in the next row. `add si, COLS*2` is computed by the assembler as `add si, 8`.

**Rows are cheap, columns are dear.** That asymmetry is fundamental to row-major storage and is why
algorithms are written to traverse rows where possible. On a machine with a cache it matters far
more than it does here.

---

## Program 44.6 — Identity test

```asm
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
```

**Output:**

```
Matrix 1 IS the identity.
Matrix 2 is not the identity.
```

**`cmp bx, cx` / `je`** detects the diagonal without any arithmetic: on the diagonal, row equals
column. Walking sequentially with `LODSW` while tracking the indices is cheaper than computing
`M[i][i]` addresses.

---

## Program 44.7 — Determinant of a 3×3

```asm
; matdet.asm — determinant of a 3×3 matrix
;
;   | a b c |
;   | d e f |  =  a(ei − fh) − b(di − fg) + c(dh − eg)
;   | g h i |
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print mmsg
        mov  si, m
        mov  bx, 3
        mov  bp, 3
        call show_rect

        call det3

        print dmsg
        call print_sdec
        newline
        exit 0

; ---------------------------------------------------------------
; det3 — determinant of the 3×3 matrix at `m`
;
;   Out:       AX = the determinant (16-bit; may overflow for large
;              inputs — see Exercise 44.12)
;   Destroys:  BX, CX, DX
;
;   Element offsets, row-major:
;       a=0  b=2  c=4
;       d=6  e=8  f=10
;       g=12 h=14 i=16
; ---------------------------------------------------------------
det3:
        ; --- term 1: a × (e×i − f×h) ---
        mov  ax, [m+8]          ; e
        imul word [m+16]        ; DX:AX = e × i
        mov  cx, ax             ; keep the low word
        mov  ax, [m+10]         ; f
        imul word [m+14]        ; f × h
        sub  cx, ax             ; CX = ei − fh
        mov  ax, [m+0]          ; a
        imul cx                 ; a × (ei − fh)
        mov  bx, ax             ; BX accumulates the determinant

        ; --- term 2: − b × (d×i − f×g) ---
        mov  ax, [m+6]          ; d
        imul word [m+16]        ; d × i
        mov  cx, ax
        mov  ax, [m+10]         ; f
        imul word [m+12]        ; f × g
        sub  cx, ax             ; CX = di − fg
        mov  ax, [m+2]          ; b
        imul cx
        sub  bx, ax             ; minus

        ; --- term 3: + c × (d×h − e×g) ---
        mov  ax, [m+6]          ; d
        imul word [m+14]        ; d × h
        mov  cx, ax
        mov  ax, [m+8]          ; e
        imul word [m+12]        ; e × g
        sub  cx, ax             ; CX = dh − eg
        mov  ax, [m+4]          ; c
        imul cx
        add  bx, ax             ; plus

        mov  ax, bx
        ret

m:      dw   6, 1, 1
        dw   4, -2, 5
        dw   2, 8, 7
mmsg:   db   'Matrix:', 0x0D, 0x0A, '$'
dmsg:   db   'Determinant = $'
```

**Output:** `Determinant = -306`

Check by hand:
`6 × ((−2)×7 − 5×8) − 1 × (4×7 − 5×2) + 1 × (4×8 − (−2)×2)`
`= 6 × (−14 − 40) − 1 × (28 − 10) + 1 × (32 + 4)`
`= 6 × (−54) − 18 + 36`
`= −324 − 18 + 36 = −306` ✔

### Explanation

**`imul word [m+16]`** — signed multiply by a memory operand. `AX` is the implicit multiplicand and
`DX:AX` receives the product; we keep only `AX`, which is correct as long as no intermediate exceeds
16 bits.

**Nine `IMUL`s at ~130 clocks each** is about 1,200 clocks, plus overhead — around 300 µs. A 4×4
determinant by cofactor expansion needs four 3×3 determinants, so 36 multiplies; 5×5 needs 180. The
factorial growth is why real code uses LU decomposition instead.

---

## Exercises

**44.1** Give the byte offset of `M[2][3]` in a 4×5 matrix of words, row-major.

**44.2** Repeat for column-major layout. Which is used by C?

**44.3** Write the instruction sequence that loads `M[i][j]` into `AX` for a 6-column byte matrix,
with `i` in `BL` and `j` in `CL`.

**44.4** Modify Program 44.2 to compute `A − B` instead.

**44.5** Write an in-place transpose for a square matrix. Why does it not work for a non-square one?

**44.6** Program 44.3 multiplies by `ROWS` to compute the destination index. Explain why, and say
what would go wrong if you used `COLS`.

**44.7** Modify Program 44.4 to accumulate the dot product in 32 bits, so that larger element values
do not overflow.

**44.8** Optimise Program 44.4 by hoisting `i × COLS_A` out of the `j` loop and replacing the
`B[j][k]` index calculation with a pointer that advances by `COLS_B × 2` each iteration. Estimate
the clock saving for a 10×10 multiply.

**44.9** Write a program that multiplies a matrix by a scalar.

**44.10** Write a program that finds the largest element of a matrix and reports its row and column.

**44.11** Program 44.5 shows that column access strides. Write a version of the column sum that
transposes first and then sums rows, and say when that would be worth it.

**44.12** Program 44.7 keeps only the low 16 bits of each product. Give a 3×3 matrix of small
integers for which the answer is wrong, and describe how to fix it.

**44.13** Write a program that tests whether a matrix is symmetric (`M[i][j] = M[j][i]`).

**44.14** Write a program that computes the trace (the sum of the diagonal) of an N×N matrix, using
no multiplication.

Answers in [Appendix H](H-exercise-solutions.md#chapter-44).

---

[← Number conversion](43-programs-number-conversion.md) · [Contents](README.md) · [Next: Programs: graphics →](45-programs-graphics.md)
