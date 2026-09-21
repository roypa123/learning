# Chapter 41 — Programs: sorting and searching

[← Programs: arrays](40-programs-arrays.md) · [Contents](README.md) · [Next: Programs: strings →](42-programs-strings.md)

---

## Goal

Three sorting algorithms and three searching algorithms, each complete and runnable, each with its
clock count worked out so you can see *why* one beats another rather than being told.

All programs assume `macros.inc` and `io.inc` from Chapter 39 §0.

---

## 1. The shared display routine

Every program here uses this. Put it in `show.inc`:

```asm
; show.inc — print an array of words
%ifndef SHOW_INC
%define SHOW_INC

; ---------------------------------------------------------------
; show_array — print CX words starting at DS:BX, space separated.
;   Destroys: nothing
; ---------------------------------------------------------------
show_array:
        push ax
        push bx
        push cx
        jcxz .out
.next:
        mov  ax, [bx]
        call print_sdec
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

%endif
```

---

## Program 41.1 — Bubble sort

The simplest sort: repeatedly walk the array, swapping adjacent elements that are out of order.

```asm
; bubble.asm — bubble sort with an early-exit optimisation
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

        call bubble_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array

        print passmsg
        mov  ax, [passes]
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; bubble_sort — sort the word array at `arr`, ascending, signed.
;
;   Outer loop: up to len-1 passes.
;   Inner loop: compare each adjacent pair, swap if out of order.
;   Early exit: if a pass makes no swaps, the array is sorted.
;
;   Destroys: AX, BX, CX, DX, SI
; ---------------------------------------------------------------
bubble_sort:
        mov  cx, len
        dec  cx                 ; at most len-1 passes
        jcxz .done              ; 0 or 1 elements

.pass:
        push cx                 ; the inner loop needs CX
        inc  word [passes]
        xor  dx, dx             ; DX = "a swap happened this pass" flag
        mov  si, arr            ; SI walks the pairs

.compare:
        mov  ax, [si]
        mov  bx, [si+2]
        cmp  ax, bx
        jle  .in_order          ; SIGNED: ax <= bx is fine
        ; out of order — swap them
        mov  [si], bx
        mov  [si+2], ax
        mov  dx, 1              ; remember that we swapped
.in_order:
        inc  si
        inc  si
        loop .compare

        pop  cx
        or   dx, dx
        jz   .done              ; a clean pass means we are finished
        loop .pass
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
passes:   dw   0
before:   db   'Before: $'
after:    db   'After:  $'
passmsg:  db   'Passes: $'
```

**Output:**

```
Before: 64 34 25 12 22 11 90 -5
After:  -5 11 12 22 25 34 64 90
Passes: 7
```

### Explanation

**The inner loop's count.** The first pass compares `len-1` pairs, and after it the largest element
is in place. A proper bubble sort shrinks the inner loop each pass. This version does not — it uses
the same `CX` for both, which is simpler and only slightly slower, and the early exit recovers most
of the difference.

**The swap flag in `DX`.** If a whole pass makes no swaps, the array is sorted and we stop. On
already-sorted data this turns an O(n²) algorithm into O(n).

**`push cx` / `pop cx` around the inner loop** — the classic nested-loop requirement (Chapter 27
§6.4). Forget it and the outer loop's count is destroyed.

**`jle` not `jbe`** — the data contains −5.

### Cost

For *n* elements, worst case:

```
   passes         : n − 1
   comparisons    : (n − 1) per pass
   clocks per comparison:
        mov ax, [si]        8 + 5  = 13
        mov bx, [si+2]      8 + 9  = 17
        cmp ax, bx                   3
        jle (taken)                 16
        inc si ×2                    4
        loop                        17
                                  ────
                                    70   (no swap)
        + mov [si], bx      9 + 5  = 14
        + mov [si+2], ax    9 + 9  = 18
        + mov dx, 1                  4
                                  ────
                                   106   (with a swap)
```

For *n* = 100, worst case ≈ 99 × 99 × 106 ≈ **1,039,000 clocks = 208 ms** at 5 MHz.

---

## Program 41.2 — Selection sort

Find the smallest remaining element and swap it into place. Always *n*−1 swaps, regardless of the
data.

```asm
; select.asm — selection sort
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

        call selection_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array
        exit 0

; ---------------------------------------------------------------
; selection_sort — sort `arr` ascending, signed.
;
;   for i = 0 to n-2:
;       find the index m of the smallest element in arr[i..n-1]
;       swap arr[i] and arr[m]
;
;   Registers:
;     SI = address of arr[i]      (the outer position)
;     DI = address of arr[j]      (the scan position)
;     BX = address of the smallest found so far
;     CX = outer loop counter
;     DX = inner loop counter
; ---------------------------------------------------------------
selection_sort:
        mov  cx, len
        dec  cx
        jcxz .done

        mov  si, arr
.outer:
        mov  bx, si             ; assume arr[i] is the smallest
        mov  di, si
        add  di, 2              ; start scanning at i+1
        mov  dx, cx             ; this many elements remain to scan

.inner:
        mov  ax, [di]
        cmp  ax, [bx]
        jge  .not_smaller       ; SIGNED comparison
        mov  bx, di             ; a new minimum
.not_smaller:
        inc  di
        inc  di
        dec  dx
        jnz  .inner

        ; --- swap arr[i] with the minimum, if they differ ---
        cmp  bx, si
        je   .no_swap
        mov  ax, [si]
        mov  dx, [bx]
        mov  [si], dx
        mov  [bx], ax
.no_swap:
        inc  si
        inc  si
        loop .outer
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
before:   db   'Before: $'
after:    db   'After:  $'
```

**Output:**

```
Before: 64 34 25 12 22 11 90 -5
After:  -5 11 12 22 25 34 64 90
```

### Explanation

**`BX` holds an *address*, not a value.** That is what makes the swap at the end trivial — we already
know where the minimum is.

**`dec dx` / `jnz` rather than `LOOP`** for the inner loop, because `CX` is carrying the outer count.
Using a different register for one of two nested loops avoids the `push`/`pop`.

**The `cmp bx, si` / `je .no_swap`** skips a pointless self-swap. It saves 40 clocks in the common
case where the minimum is already in place.

### Cost versus bubble sort

| | Comparisons | Swaps |
|---|-------------|-------|
| Bubble (worst) | n(n−1)/2 | n(n−1)/2 |
| Bubble (sorted input) | n−1 | 0 |
| Selection (always) | n(n−1)/2 | **at most n−1** |

**Selection sort does far fewer swaps.** When an element is expensive to move — a 64-byte record
rather than a word — that dominates, and selection sort wins decisively. For bare words the
difference is smaller.

---

## Program 41.3 — Insertion sort

Take each element and insert it into the sorted portion to its left.

```asm
; insert.asm — insertion sort
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

        call insertion_sort

        print after
        mov  bx, arr
        mov  cx, len
        call show_array
        exit 0

; ---------------------------------------------------------------
; insertion_sort — sort `arr` ascending, signed.
;
;   for i = 1 to n-1:
;       key = arr[i]
;       j = i - 1
;       while j >= 0 and arr[j] > key:
;           arr[j+1] = arr[j]
;           j = j - 1
;       arr[j+1] = key
;
;   Registers:
;     SI = address of arr[i]
;     DI = address of arr[j]
;     AX = the key being inserted
;     CX = outer counter
; ---------------------------------------------------------------
insertion_sort:
        mov  cx, len
        dec  cx
        jcxz .done

        mov  si, arr + 2        ; start at i = 1
.outer:
        mov  ax, [si]           ; AX = key
        mov  di, si
        sub  di, 2              ; DI -> arr[i-1]

.shift:
        cmp  di, arr
        jb   .place             ; ran off the front of the array
        mov  bx, [di]
        cmp  bx, ax
        jle  .place             ; SIGNED: found the insertion point
        mov  [di+2], bx         ; shift this element right
        sub  di, 2
        jmp  .shift

.place:
        mov  [di+2], ax         ; drop the key into the gap
        inc  si
        inc  si
        loop .outer
.done:
        ret

arr:      dw   64, 34, 25, 12, 22, 11, 90, -5
len       equ  ($ - arr) / 2
before:   db   'Before: $'
after:    db   'After:  $'
```

**Output:**

```
Before: 64 34 25 12 22 11 90 -5
After:  -5 11 12 22 25 34 64 90
```

### Explanation

**`cmp di, arr` / `jb .place`** is the boundary test. `DI` walks backwards; when it goes below the
array's start we stop. Using `jb` (unsigned) is correct because these are addresses.

**The shift, not a swap.** Each out-of-place element moves *once* to its final position; everything
larger slides right. That is why insertion sort beats bubble sort on nearly-sorted data — often
dramatically.

### When each sort wins

| Data | Best choice | Why |
|------|-------------|-----|
| Nearly sorted | **insertion** | O(n) — almost no shifting |
| Random, small (n < 20) | **insertion** | lowest constant factor |
| Random, expensive elements | **selection** | fewest moves |
| Already sorted | **bubble with early exit** | one pass, O(n) |
| Random, large n | none of these — you want quicksort or heapsort |

All three are O(n²). For n above a few hundred on an 8086 you need a better algorithm; Exercise
41.12 asks for quicksort.

---

## Program 41.4 — Linear search

```asm
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
```

### 41.4a — The `SCASW` version

`REPNE SCASW` does the same search in one instruction:

```asm
; ---------------------------------------------------------------
; linear_search_fast — the same search using a string instruction.
; ---------------------------------------------------------------
linear_search_fast:
        cld
        mov  di, arr            ; ES:DI — ES = DS in a .COM program
        mov  cx, len
        mov  ax, [target]
        repne scasw             ; scan while AX != [ES:DI]
        jne  .fail              ; CX ran out without a match

        ; DI advanced PAST the match — back up and compute the index
        sub  di, 2
        sub  di, arr
        shr  di, 1              ; byte offset -> element index
        mov  [index], di
        clc
        ret
.fail:
        stc
        ret
```

### Cost comparison

| Version | Clocks per element |
|---------|-------------------|
| Hand-written loop | 3 + 5 (cmp) + 2 + 2 + 2 + 17 ≈ **31** |
| `REPNE SCASW` | **15** |

Twice as fast, and four instructions shorter. **Use `SCASW` for any linear search of a word array.**

Remember the two follow-ups it needs: test `ZF` to distinguish "found" from "ran out", and `sub di,
2` because the pointer advanced past the match (Chapter 29 §3.1).

---

## Program 41.5 — Binary search

Requires sorted data, and takes O(log n) instead of O(n).

```asm
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
```

**Output for input 67:** `Found at index 8 in 3 steps`
**Output for input 50:** `Not found. Steps: 4`

### Explanation

**The `mid = 0` special case.** If `arr[0] > target`, we want `hi = -1`, which as an unsigned index
would wrap to 65,535 and the loop would run wildly. The `or dx, dx` / `jz .fail` catches it.

**`shr bx, 1` after the addition, then `shl bx, 1`.** The first halves `lo + hi` to get `mid`; the
second converts the index to a byte offset. They look like they cancel, and they do not — there is a
`mov dx, bx` between them that preserves the index.

**`jb` not `jl`.** This array is unsigned. For signed data, use `jl`.

### The comparison

| n | Linear (average) | Binary (worst) |
|---|-----------------|----------------|
| 14 | 7 comparisons | **4** |
| 100 | 50 | **7** |
| 1,000 | 500 | **10** |
| 10,000 | 5,000 | **14** |
| 65,535 | 32,768 | **16** |

**Binary search's step count is ⌈log₂(n+1)⌉.** At n = 65,535 it is 16 steps against 32,768 — a
factor of two thousand.

But each binary-search step is more expensive (about 60 clocks, with the divide-by-two and index
arithmetic) than a `SCASW` element (15 clocks). The crossover is around **n = 20**: below that,
`REPNE SCASW` on unsorted data beats binary search on sorted data, *and* you save the cost of
sorting.

---

## Program 41.6 — Sort, then search

The realistic combination.

```asm
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
```

### When sorting first pays

Sorting costs O(n²) here — about 1,000,000 clocks for n = 100. Each binary search then costs ~400
clocks instead of a linear search's ~1,550.

```
   break-even searches  =  sort cost / saving per search
                        =  1,000,000 / 1,150
                        ≈  870 searches
```

**Sort first only if you will search the array hundreds of times.** For a handful of searches,
`REPNE SCASW` on the unsorted data is faster overall. This is the kind of judgement the clock counts
exist to support.

---

## Exercises

**41.1** Modify the bubble sort to shrink the inner loop by one each pass. How many comparisons does
it save for n = 8?

**41.2** Change all three sorts to descending order. What is the minimal change in each?

**41.3** The bubble sort uses `jle`. Change it to `jbe` and predict the output for the given data.

**41.4** Add a swap counter to each of the three sorts and compare them on the same data.

**41.5** Which sort is fastest on already-sorted data, and why? Verify by adding a comparison
counter.

**41.6** Which sort is fastest on reverse-sorted data?

**41.7** Write a version of selection sort that finds the *maximum* and places it at the end.

**41.8** Modify the insertion sort to sort an array of bytes rather than words.

**41.9** In Program 41.4a, why is `sub di, 2` needed after `REPNE SCASW`?

**41.10** Rewrite the linear search to use `REPNE SCASW` and return the index, and count the clocks
saved for a 100-element array.

**41.11** Program 41.5 handles `mid = 0` specially. Remove that check and give an input that breaks
it.

**41.12** Write a recursive quicksort for a word array. Compare its clock count with insertion sort
for n = 8 and estimate the crossover point.

**41.13** Write a program that finds all indices at which a value occurs, not just the first.

**41.14** A program will build an array once and search it 50 times. Work out whether sorting first
is worth it, using the figures in §41.6.

Answers in [Appendix H](H-exercise-solutions.md#chapter-41).

---

[← Programs: arrays](40-programs-arrays.md) · [Contents](README.md) · [Next: Programs: strings →](42-programs-strings.md)
