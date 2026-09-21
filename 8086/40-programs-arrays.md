# Chapter 40 — Programs: arrays

[← Programs: arithmetic](39-programs-arithmetic.md) · [Contents](README.md) · [Next: Sorting and searching →](41-programs-sorting-searching.md)

---

## Goal

Ten complete array programs: sum, average, minimum, maximum, reverse, rotate, merge, frequency
count, remove duplicates, and a two-array dot product. Each shows a different addressing pattern.

All programs assume `macros.inc` and `io.inc` from Chapter 39 §0.

---

## 1. How an array is laid out

```asm
arr:    dw   10, 20, 30, 40, 50
len     equ  ($ - arr) / 2      ; 5 — computed by the assembler
```

In memory, at offset `arr`:

```
   offset:   arr+0  arr+2  arr+4  arr+6  arr+8
   bytes:    0A 00  14 00  1E 00  28 00  32 00
   value:      10     20     30     40     50
   index:       0      1      2      3      4
```

**Element *i* is at `arr + i × 2`** for a word array, `arr + i` for a byte array.

`len equ ($ - arr) / 2` computes the count automatically: `$` is the address just past the last
element, so `$ - arr` is the byte length, and dividing by 2 gives the element count. **Never
hard-code the length** — it will drift out of step with the data.

### 1.1 The three ways to walk an array

```asm
; --- indexed: the array address is constant, SI is the offset ---
        xor  si, si
.next:  mov  ax, [arr+si]       ; EA = 9 clocks (index + displacement)
        add  si, 2
        loop .next

; --- pointer: BX walks through memory ---
        mov  bx, arr
.next:  mov  ax, [bx]           ; EA = 5 clocks
        inc  bx
        inc  bx
        loop .next

; --- string instruction: SI advances automatically ---
        cld
        mov  si, arr
.next:  lodsw                   ; 12 clocks total, no EA, no increment
        loop .next
```

The third is fastest and shortest. Use it whenever you are walking forwards through a whole array.

---

## Program 40.1 — Sum an array

```asm
; sumarr.asm — sum an array of 16-bit values
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        cld                     ; forwards
        mov  si, arr            ; DS:SI -> the array
        mov  cx, len            ; element count
        xor  ax, ax             ; AX will hold the running total
        xor  dx, dx             ; DX:AX = a 32-bit accumulator

        jcxz .done              ; guard against an empty array
.next:
        push ax
        lodsw                   ; AX <- [SI], SI += 2
        mov  bx, ax
        pop  ax
        add  ax, bx             ; add to the low word
        adc  dx, 0              ; propagate any carry into the high word
        loop .next
.done:
        print msg
        call print_hex32        ; DX:AX
        newline

        print dmsg
        or   dx, dx
        jnz  .too_big
        call print_udec
        newline
        exit 0
.too_big:
        print bigmsg
        exit 0

arr:    dw   100, 250, 375, 4000, 12000, 65000, 300, 42
len     equ  ($ - arr) / 2
msg:    db   'Sum (hex): 0x$'
dmsg:   db   'Sum (dec): $'
bigmsg: db   '(exceeds 16 bits)', 0x0D, 0x0A, '$'
```

**Output:**

```
Sum (hex): 0x00014093
Sum (dec): (exceeds 16 bits)
```

Check: 100 + 250 + 375 + 4000 + 12000 + 65000 + 300 + 42 = 82,067 = `0x14093`. That is 17 bits, so
`DX` = 1 and the decimal path correctly refuses.

### Explanation

**The 32-bit accumulator matters.** Eight 16-bit values can sum to 524,280, which needs 20 bits. With
a 16-bit accumulator the answer would wrap silently. `adc dx, 0` after each addition catches the
carry — it adds 0 + 0 + `CF`, so `DX` counts the number of times the low word wrapped.

**The `push ax` / `pop ax` around `LODSW`** is because `LODSW` loads into `AX`, which is where our
running total lives. An alternative that avoids the stack traffic:

```asm
        mov  bx, arr
.next:  add  ax, [bx]
        adc  dx, 0
        inc  bx
        inc  bx
        loop .next
```

14 + 5 = 19 clocks for the `ADD`, plus 2+2+17 — about 40 per element, against the `LODSW` version's
11 + 12 + 2 + 3 + 2 + 17 ≈ 47. The pointer version wins here precisely *because* the accumulator is
in `AX`.

---

## Program 40.2 — Average

```asm
; avgarr.asm — mean of an array, with the remainder
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  bx, arr
        mov  cx, len
        xor  ax, ax
        xor  dx, dx
        jcxz .empty
.next:
        add  ax, [bx]
        adc  dx, 0
        inc  bx
        inc  bx
        loop .next

        ; DX:AX = the total.  Divide by the count.
        mov  bx, len
        div  bx                 ; AX = mean, DX = remainder
                                ; safe: DX < BX before the divide, because
                                ; total / count <= 65535 for these values
        mov  [mean], ax
        mov  [rem], dx

        print msg
        mov  ax, [mean]
        call print_udec
        print rmsg
        mov  ax, [rem]
        call print_udec
        print dmsg
        mov  ax, len
        call print_udec
        newline
        exit 0

.empty:
        print emptymsg
        exit 1

arr:      dw   10, 20, 30, 40, 55
len       equ  ($ - arr) / 2
mean:     dw   0
rem:      dw   0
msg:      db   'Mean = $'
rmsg:     db   ' remainder $'
dmsg:     db   '/$'
emptymsg: db   'Empty array.', 0x0D, 0x0A, '$'
```

**Output:** `Mean = 31 remainder 0/5`

Check: 10+20+30+40+55 = 155; 155 ÷ 5 = 31 exactly.

### Explanation

**`div bx` with a 32-bit dividend can overflow.** The quotient must fit in `AX`, i.e. be under
65,536. Here the total is at most `len × 65535` and we divide by `len`, so the quotient is at most
65,535. Safe. For a general routine, check `DX < BX` before dividing:

```asm
        cmp  dx, bx
        jae  .would_overflow
        div  bx
```

---

## Program 40.3 — Minimum and maximum

```asm
; minmax.asm — find the smallest and largest elements
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  cx, len
        jcxz .empty

        mov  bx, arr
        mov  ax, [bx]           ; start with element 0 as BOTH min and max
        mov  [min], ax
        mov  [max], ax
        dec  cx                 ; we have already examined one element
        jcxz .done
        inc  bx
        inc  bx

.next:
        mov  ax, [bx]
        cmp  ax, [min]
        jge  .not_smaller       ; SIGNED comparison — the data may be negative
        mov  [min], ax
.not_smaller:
        cmp  ax, [max]
        jle  .not_bigger
        mov  [max], ax
.not_bigger:
        inc  bx
        inc  bx
        loop .next

.done:
        print minmsg
        mov  ax, [min]
        call print_sdec
        newline
        print maxmsg
        mov  ax, [max]
        call print_sdec
        newline
        exit 0

.empty:
        print emptymsg
        exit 1

arr:      dw   45, -12, 300, 7, -250, 99, 1000, 0
len       equ  ($ - arr) / 2
min:      dw   0
max:      dw   0
minmsg:   db   'Minimum: $'
maxmsg:   db   'Maximum: $'
emptymsg: db   'Empty array.', 0x0D, 0x0A, '$'
```

**Output:**

```
Minimum: -250
Maximum: 1000
```

### Explanation

**`jge` and `jle`, not `jae` and `jbe`.** The array contains negative values, so signed comparisons
are required. With `jae`, −250 would be treated as 65,286 and reported as the maximum. This is
Chapter 27 §4.4 in practice, and it is the single most common array bug.

**Initialising both to element 0** rather than to `0x7FFF` and `0x8000` is more robust: it works
whatever the data, and it handles a one-element array correctly.

**`dec cx` before the loop** accounts for the element already consumed. Without it the last element
is examined twice — harmless here, but wrong in general.

---

## Program 40.4 — Reverse an array in place

```asm
; revarr.asm — reverse an array in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        call show               ; before

        mov  si, arr            ; SI -> first element
        mov  di, arr + (len-1)*2 ; DI -> last element
        mov  cx, len
        shr  cx, 1              ; swap only half the elements
        jcxz .done              ; 0 or 1 elements -> nothing to do

.swap:
        mov  ax, [si]
        mov  bx, [di]
        mov  [si], bx
        mov  [di], ax
        inc  si
        inc  si
        dec  di
        dec  di
        loop .swap
.done:
        print arrow
        call show               ; after
        exit 0

; ---------------------------------------------------------------
; show — print the array as  10 20 30 ...
; ---------------------------------------------------------------
show:
        push ax
        push bx
        push cx
        mov  bx, arr
        mov  cx, len
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

arr:    dw   10, 20, 30, 40, 50, 60, 70
len     equ  ($ - arr) / 2
arrow:  db   '     becomes', 0x0D, 0x0A, '$'
```

**Output:**

```
10 20 30 40 50 60 70
     becomes
70 60 50 40 30 20 10
```

### Explanation

**`shr cx, 1` halves the count.** With 7 elements, `CX` becomes 3 — the middle element stays put,
which is correct. With 8 elements, `CX` becomes 4 and every element moves.

**Two pointers converging** is the standard in-place reversal. The loop runs exactly `len/2` times,
and `SI` and `DI` cross in the middle.

**`inc si` twice and `dec di` twice** rather than `add si, 2` / `sub di, 2` — identical here (both
4 clocks), and the `INC` form preserves `CF` in case you later fold this into a carry chain.

---

## Program 40.5 — Rotate left by one

```asm
; rotarr.asm — rotate an array left by one position
;   [10,20,30,40,50]  ->  [20,30,40,50,10]
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        call show

        mov  cx, len
        cmp  cx, 2
        jb   .done              ; 0 or 1 elements — nothing to do

        mov  ax, [arr]          ; save element 0
        push ax

        ; --- shift everything down one place ---
        cld
        mov  si, arr + 2        ; source: element 1
        mov  di, arr            ; destination: element 0
        mov  cx, len - 1
        rep  movsw              ; the only memory-to-memory move there is

        pop  ax
        mov  [arr + (len-1)*2], ax   ; the saved element goes to the end

.done:
        print arrow
        call show
        exit 0

show:
        push ax
        push bx
        push cx
        mov  bx, arr
        mov  cx, len
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

arr:    dw   10, 20, 30, 40, 50
len     equ  ($ - arr) / 2
arrow:  db   '  rotate left ->', 0x0D, 0x0A, '$'
```

**Output:**

```
10 20 30 40 50
  rotate left ->
20 30 40 50 10
```

### Explanation

**`rep movsw` moves overlapping data safely here** because the destination (`arr`) is *below* the
source (`arr+2`), and we copy forwards (Chapter 29 §6.2). Reverse the direction of the rotation and
you must copy backwards with `STD`.

**`ES` must equal `DS`.** In a `.COM` program DOS has already arranged that, which is why no setup
appears. In an `.EXE` this program would write to the PSP.

---

## Program 40.6 — Merge two sorted arrays

```asm
; merge.asm — merge two sorted arrays into a third
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, arr1           ; SI -> current element of arr1
        mov  di, arr2           ; DI -> current element of arr2
        mov  bx, result         ; BX -> where the next output goes
        mov  cx, len1           ; how many remain in arr1
        mov  dx, len2           ; how many remain in arr2

.next:
        or   cx, cx
        jz   .drain2            ; arr1 exhausted
        or   dx, dx
        jz   .drain1            ; arr2 exhausted

        mov  ax, [si]
        cmp  ax, [di]
        jg   .take2             ; signed: arr2's element is smaller
        ; take from arr1
        mov  [bx], ax
        inc  si
        inc  si
        dec  cx
        jmp  .advance
.take2:
        mov  ax, [di]
        mov  [bx], ax
        inc  di
        inc  di
        dec  dx
.advance:
        inc  bx
        inc  bx
        jmp  .next

.drain1:                        ; copy what is left of arr1
        or   cx, cx
        jz   .done
        mov  ax, [si]
        mov  [bx], ax
        inc  si
        inc  si
        inc  bx
        inc  bx
        dec  cx
        jmp  .drain1

.drain2:                        ; copy what is left of arr2
        or   dx, dx
        jz   .done
        mov  ax, [di]
        mov  [bx], ax
        inc  di
        inc  di
        inc  bx
        inc  bx
        dec  dx
        jmp  .drain2

.done:
        print msg
        mov  bx, result
        mov  cx, len1 + len2
.show:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .show
        newline
        exit 0

arr1:   dw   1, 5, 9, 12, 20
len1    equ  ($ - arr1) / 2
arr2:   dw   2, 3, 11, 15
len2    equ  ($ - arr2) / 2
result: times (len1 + len2) dw 0
msg:    db   'Merged: $'
```

**Output:** `Merged: 1 2 3 5 9 11 12 15 20`

### Explanation

**Three phases:** compare-and-take while both arrays have elements; then drain whichever still does.

**The `jg` uses a signed comparison**, matching the data. For unsigned data use `ja`.

**`jg .take2` takes from arr1 on a tie**, which makes the merge *stable* — equal elements keep their
relative order, with arr1's first. That matters when merging records rather than bare numbers.

---

## Program 40.7 — Frequency count

```asm
; freq.asm — count how often each value 0-9 appears
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- zero the counters ---
        cld
        mov  di, counts
        mov  cx, 10
        xor  ax, ax
        rep  stosw              ; the fastest way to clear memory

        ; --- count ---
        mov  si, data
        mov  cx, datalen
.next:
        lodsb                   ; AL = the next value, SI advances
        cmp  al, 9
        ja   .skip              ; ignore anything out of range
        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — counters are words
        inc  word [counts+bx]   ; the counter for this value
.skip:
        loop .next

        ; --- report ---
        xor  bx, bx             ; BX = the value being reported
.report:
        mov  ax, bx
        call print_udec
        print colon
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        call print_udec
        newline
        inc  bx
        cmp  bx, 10
        jb   .report
        exit 0

data:    db   3,7,3,1,9,3,7,0,1,3,5,7,9,9,3
datalen  equ  $ - data
counts:  times 10 dw 0
colon:   db   ': $'
```

**Output:**

```
0: 1
1: 2
2: 0
3: 5
4: 0
5: 1
6: 0
7: 3
8: 0
9: 3
```

Check: 15 values, and 1+2+0+5+0+1+0+3+0+3 = 15. ✔

### Explanation

**`rep stosw` to clear the counters** — 10 words in 9 + 10×10 = 109 clocks, against a loop's ~300.
It requires `ES = DS` and `DF = 0`, both of which hold here.

**`shl bx, 1` converts a value to a word offset.** This is the standard array-indexing step: index ×
element size.

**`inc word [counts+bx]`** — the `word` keyword is required because NASM cannot tell the size from a
memory-only operand (Chapter 33 §4.2).

**The range check** (`cmp al, 9` / `ja .skip`) is essential. Without it, a value of 200 would
increment `[counts + 400]`, which is 380 bytes past the end of the table — into whatever follows.

---

## Program 40.8 — Remove duplicates from a sorted array

```asm
; dedup.asm — remove adjacent duplicates from a sorted array, in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  cx, len
        call show

        cmp  cx, 2
        jb   .done              ; 0 or 1 elements — nothing to do

        mov  si, arr + 2        ; SI -> the element being examined (index 1)
        mov  di, arr + 2        ; DI -> where the next kept element goes
        mov  bx, [arr]          ; BX = the last value we kept
        mov  cx, len - 1
        mov  dx, 1              ; DX counts the elements kept

.next:
        mov  ax, [si]
        cmp  ax, bx
        je   .duplicate         ; same as the previous — skip it
        mov  [di], ax           ; keep it
        mov  bx, ax
        inc  di
        inc  di
        inc  dx
.duplicate:
        inc  si
        inc  si
        loop .next

        mov  [newlen], dx

.done:
        print arrow
        mov  cx, [newlen]
        call show
        print cntmsg
        mov  ax, [newlen]
        call print_udec
        newline
        exit 0

; show — print CX elements starting at arr
show:
        push ax
        push bx
        push cx
        mov  bx, arr
        jcxz .out
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
.out:
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

arr:     dw   1, 1, 2, 3, 3, 3, 5, 8, 8, 13
len      equ  ($ - arr) / 2
newlen:  dw   len
arrow:   db   '  dedup ->', 0x0D, 0x0A, '$'
cntmsg:  db   'Elements remaining: $'
```

**Output:**

```
1 1 2 3 3 3 5 8 8 13
  dedup ->
1 2 3 5 8 13
Elements remaining: 6
```

### Explanation

**Two pointers at different speeds.** `SI` reads every element; `DI` advances only when an element is
kept. This "read pointer / write pointer" pattern is the standard in-place filter, and it works for
any predicate, not just duplicate removal.

**It requires a sorted array**, because it only compares adjacent elements. On unsorted data it
removes only *runs* of duplicates.

---

## Program 40.9 — Dot product

```asm
; dotprod.asm — dot product of two vectors
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, vec1
        mov  di, vec2
        mov  cx, len
        xor  bx, bx             ; BX = the low word of the accumulator
        xor  bp, bp             ; BP = the high word
        jcxz .done

.next:
        mov  ax, [si]
        imul word [di]          ; DX:AX = signed product of the two elements
        add  bx, ax             ; accumulate into BP:BX
        adc  bp, dx             ; with the full 32-bit carry
        inc  si
        inc  si
        inc  di
        inc  di
        loop .next

.done:
        print msg
        mov  dx, bp
        mov  ax, bx
        call print_hex32
        newline

        ; decimal, if it fits in 16 bits
        or   bp, bp
        jz   .small
        cmp  bp, 0xFFFF         ; a negative value sign-extends to FFFF
        jne  .big
.small:
        print dmsg
        mov  ax, bx
        call print_sdec
        newline
.big:
        exit 0

vec1:   dw   1, 2, 3, 4, 5
len     equ  ($ - vec1) / 2
vec2:   dw   10, 20, 30, 40, 50
msg:    db   'Dot product (hex): 0x$'
dmsg:   db   'Dot product (dec): $'
```

**Output:**

```
Dot product (hex): 0x00000226
Dot product (dec): 550
```

Check: 1×10 + 2×20 + 3×30 + 4×40 + 5×50 = 10 + 40 + 90 + 160 + 250 = 550 = `0x226`. ✔

### Explanation

**`imul word [di]`** — signed multiply with a memory operand. `AX` is the implicit multiplicand and
`DX:AX` receives the 32-bit product (Chapter 23 §3).

**`add bx, ax` / `adc bp, dx`** accumulates the *full* 32-bit product, not just its low word. Summing
only `AX` would be wrong whenever any single product exceeded 16 bits.

**`BP` is used as a general register here**, which is legal — `BP` is only special when it appears in
a memory operand (Chapter 7 §3.2). We never write `[bp]`, so nothing goes to the stack segment.

---

## Program 40.10 — Copy an array with `REP MOVSW`

```asm
; copyarr.asm — copy an array, and time the two methods
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- method 1: REP MOVSW ---
        cld
        mov  si, src
        mov  di, dst
        mov  cx, len
        rep  movsw              ; 9 + len × 17 clocks

        print msg1
        mov  bx, dst
        mov  cx, len
        call show

        ; --- method 2: a hand-written loop, for comparison ---
        mov  si, src
        mov  di, dst2
        mov  cx, len
.next:
        mov  ax, [si]           ; 8 + 5  = 13
        mov  [di], ax           ; 9 + 5  = 14
        inc  si                 ; 2
        inc  si                 ; 2
        inc  di                 ; 2
        inc  di                 ; 2
        loop .next              ; 17
                                ;         = 52 clocks per element

        print msg2
        mov  bx, dst2
        mov  cx, len
        call show
        exit 0

show:
        push ax
        push bx
        push cx
.next:
        mov  ax, [bx]
        call print_udec
        putc ' '
        inc  bx
        inc  bx
        loop .next
        newline
        pop  cx
        pop  bx
        pop  ax
        ret

src:    dw   11, 22, 33, 44, 55, 66
len     equ  ($ - src) / 2
dst:    times len dw 0
dst2:   times len dw 0
msg1:   db   'REP MOVSW:   $'
msg2:   db   'Manual loop: $'
```

**Output:**

```
REP MOVSW:   11 22 33 44 55 66
Manual loop: 11 22 33 44 55 66
```

### The timing comparison

| Method | Clocks per element | For 1000 elements |
|--------|-------------------|-------------------|
| `REP MOVSW` | 17 | 17,009 |
| Manual loop | 52 | 52,000 |

**Three times faster, in one instruction.** And on an 8086, `REP MOVSW` moves two bytes per 17
clocks, where `REP MOVSB` would need 34 for the same two bytes.

---

## Exercises

**40.1** Modify Program 40.1 to sum an array of *bytes* rather than words. What changes?

**40.2** Program 40.3 uses `jge`/`jle`. Change them to `jae`/`jbe` and predict the reported minimum
and maximum for the given data.

**40.3** Write a program that counts how many elements of an array are negative.

**40.4** Write a program that finds the *second* largest element of an array in a single pass.

**40.5** Modify Program 40.4 to reverse only the first half of the array.

**40.6** Write a program that rotates an array *right* by one position. Which direction must the
`MOVSW` go, and why?

**40.7** Extend Program 40.5 to rotate left by *n* positions, where *n* is read from the keyboard.

**40.8** Program 40.7 counts values 0–9. Extend it to count values 0–255 using a byte-sized counter
table, and say what happens if a value appears more than 255 times.

**40.9** Write a program that checks whether an array is sorted in ascending order.

**40.10** Modify Program 40.8 to work on an *unsorted* array, removing all duplicates rather than
just adjacent ones. What is the cost in time complexity?

**40.11** Write a program that computes the sum of the elements at even indices minus the sum at odd
indices.

**40.12** Program 40.9 accumulates in `BP:BX`. Rewrite it to accumulate in a memory variable instead
and compare the clock counts.

**40.13** Write a program that merges two sorted arrays *in place* when the destination array has
room at the end. (Hint: work backwards from the highest index.)

**40.14** Write a program that finds the longest run of equal values in an array and reports its
length and value.

Answers in [Appendix H](H-exercise-solutions.md#chapter-40).

---

[← Programs: arithmetic](39-programs-arithmetic.md) · [Contents](README.md) · [Next: Sorting and searching →](41-programs-sorting-searching.md)
