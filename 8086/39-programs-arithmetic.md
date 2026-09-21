# Chapter 39 — Programs: arithmetic

[← Macros and modular programs](38-macros-and-modules.md) · [Contents](README.md) · [Next: Programs: arrays →](40-programs-arrays.md)

---

## Goal

Twelve complete, runnable arithmetic programs, from 8-bit addition to 64-bit multiplication. Each
one is listed in full, each instruction is explained, and each is traced with real numbers.

This is where Parts I–III stop being theory.

---

## 0. The shared library

Several programs need decimal output. Rather than repeating it, put these in `io.inc` and
`%include` it.

```asm
; io.inc — number input and output routines
%ifndef IO_INC
%define IO_INC

; ---------------------------------------------------------------
; print_udec — print the UNSIGNED 16-bit value in AX as decimal.
;
;   In:        AX = the value
;   Out:       nothing
;   Destroys:  nothing
;
;   Method: divide by 10 repeatedly, pushing each remainder. The
;   stack reverses the digits, which is why this idiom is standard.
; ---------------------------------------------------------------
print_udec:
        push ax
        push bx
        push cx
        push dx

        mov  bx, 10
        xor  cx, cx             ; CX counts the digits pushed
.divide:
        xor  dx, dx             ; DX:AX = the remaining value.
                                ; CLEARING DX EACH TIME IS ESSENTIAL.
        div  bx                 ; AX = value/10, DX = value mod 10
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide

.output:
        pop  dx                 ; digits come back most significant first
        add  dl, '0'
        mov  ah, 0x02
        int  0x21
        loop .output

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; print_sdec — print the SIGNED 16-bit value in AX as decimal.
; ---------------------------------------------------------------
print_sdec:
        push ax
        or   ax, ax
        jns  .positive
        push ax
        mov  dl, '-'
        mov  ah, 0x02
        int  0x21
        pop  ax
        neg  ax
.positive:
        call print_udec
        pop  ax
        ret

; ---------------------------------------------------------------
; print_hex16 — print AX as exactly four hex digits.
; ---------------------------------------------------------------
print_hex16:
        push ax
        push bx
        push cx
        push dx

        mov  bx, ax
        mov  cx, 4
.digit:
        rol  bx, 1              ; four ROLs bring the top nibble to the bottom
        rol  bx, 1              ;   (8086 has no ROL bx,4)
        rol  bx, 1
        rol  bx, 1
        mov  dl, bl
        and  dl, 0x0F
        add  dl, '0'
        cmp  dl, '9'
        jbe  .emit
        add  dl, 7              ; 'A'..'F' sit 7 past '9'+1
.emit:
        mov  ah, 0x02
        int  0x21
        loop .digit

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; print_hex32 — print DX:AX as eight hex digits.
; ---------------------------------------------------------------
print_hex32:
        push ax
        mov  ax, dx
        call print_hex16        ; high word first
        pop  ax
        call print_hex16
        ret

; ---------------------------------------------------------------
; read_udec — read an unsigned decimal number from the keyboard.
;
;   Out:       AX = the value, CF = 1 if nothing valid was entered
;   Destroys:  nothing else
; ---------------------------------------------------------------
read_udec:
        push bx
        push cx
        push dx

        xor  bx, bx             ; BX accumulates the value
        xor  cx, cx             ; CX counts valid digits
.next:
        mov  ah, 0x01           ; read with echo
        int  0x21
        cmp  al, 0x0D           ; Enter?
        je   .done
        cmp  al, '0'
        jb   .next              ; ignore anything that is not a digit
        cmp  al, '9'
        ja   .next

        sub  al, '0'            ; ASCII -> value
        mov  ah, 0
        push ax                 ; save the new digit
        mov  ax, bx
        mov  dx, 10
        mul  dx                 ; DX:AX = old value × 10
        mov  bx, ax             ;   (we ignore overflow past 65535)
        pop  ax
        add  bx, ax             ; + the new digit
        inc  cx
        jmp  .next

.done:
        mov  ax, bx
        or   cx, cx
        jnz  .ok
        stc                     ; no digits entered
        jmp  .out
.ok:
        clc
.out:
        pop  dx
        pop  cx
        pop  bx
        ret

%endif
```

Every program below assumes `io.inc` and `macros.inc` are in the same directory.

---

## Program 39.1 — Add two 8-bit numbers

The simplest possible arithmetic program.

```asm
; add8.asm — add two 8-bit numbers and print the result
; nasm -f bin add8.asm -o add8.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  al, [num1]         ; AL = the first number
        add  al, [num2]         ; AL = AL + the second; flags set

        mov  ah, 0              ; zero-extend AL into AX for printing
                                ;   (the sum of two bytes can exceed 255,
                                ;    but here AL has already wrapped)
        mov  [result], al

        print msg
        mov  al, [result]
        mov  ah, 0
        call print_udec
        newline
        exit 0

num1:   db   87
num2:   db   56
result: db   0
msg:    db   '87 + 56 = $'
```

**Output:** `87 + 56 = 143`

### Explanation

`mov al, [num1]` — an 8-bit load from the data segment. `DS` already points at our segment, so
`[num1]` means `DS:num1`. Direct addressing, 6 EA clocks, 8 base = 14 clocks.

`add al, [num2]` — adds the second byte to `AL`, setting all six status flags. 9 + 6 = 15 clocks.

`mov ah, 0` — `print_udec` prints a 16-bit value in `AX`, so the high byte must be cleared.

**The overflow problem.** 87 + 56 = 143, which fits in a byte. But 200 + 100 = 300, which does not —
`AL` would hold 44 and `CF` would be 1. The next program handles that.

---

## Program 39.2 — 8-bit addition with carry detection

```asm
; add8c.asm — add two bytes, correctly handling a result over 255
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  al, [num1]
        add  al, [num2]         ; CF = 1 if the true sum exceeds 255
        mov  ah, 0              ; assume it fits...
        adc  ah, 0              ; ...then add the carry into AH.
                                ;   AH becomes 1 exactly when CF was 1.
                                ;   AX is now the true 16-bit sum.
        print msg
        call print_udec
        newline
        exit 0

num1:   db   200
num2:   db   100
msg:    db   '200 + 100 = $'
```

**Output:** `200 + 100 = 300`

### Explanation

The pair

```asm
        mov  ah, 0
        adc  ah, 0
```

is the idiom for promoting an 8-bit sum to 16 bits. `MOV` does not disturb `CF`; `ADC AH, 0` adds
0 + 0 + `CF`, so `AH` becomes exactly the carry bit. Two instructions, no branch.

Trace: 200 + 100 = 300 = `0x12C`. `AL` = `0x2C` = 44, `CF` = 1. Then `AH` = 0 + 0 + 1 = 1, so
`AX` = `0x012C` = 300. ✔

---

## Program 39.3 — Subtract, with sign

```asm
; sub16.asm — subtract two 16-bit numbers, printing a signed result
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        sub  ax, [num2]         ; CF = borrow, OF = signed overflow

        print msg
        call print_sdec         ; handles the minus sign
        newline
        exit 0

num1:   dw   1500
num2:   dw   4200
msg:    db   '1500 - 4200 = $'
```

**Output:** `1500 - 4200 = -2700`

### Explanation

`sub ax, [num2]` computes `1500 − 4200 = −2700`, which as a 16-bit two's complement value is
`0xF574`.

`print_sdec` tests the sign bit with `or ax, ax` / `jns`, prints `'-'`, negates, and prints the
magnitude. Note the `push ax` / `pop ax` around the `'-'` output: DOS function 02h destroys `AL`.

`CF = 1` here (unsigned, 1500 < 4200) and `OF = 0` (signed, −2700 fits). Both flags are correct; we
happen to care about neither.

---

## Program 39.4 — Multiply two 16-bit numbers

```asm
; mul16.asm — 16 × 16 = 32-bit multiply
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        mov  bx, [num2]
        mul  bx                 ; DX:AX = AX × BX.  DESTROYS DX.

        push ax                 ; save the low word
        push dx

        print msg

        pop  dx
        pop  ax
        push ax
        push dx
        call print_hex32        ; print DX:AX as 8 hex digits

        print dmsg

        ; --- also print it in decimal, if it fits in 16 bits ---
        pop  dx
        pop  ax
        or   dx, dx
        jnz  .too_big           ; the high word is non-zero
        call print_udec
        newline
        exit 0

.too_big:
        print bigmsg
        exit 0

num1:   dw   1234
num2:   dw   5678
msg:    db   '1234 * 5678 = 0x$'
dmsg:   db   ' = $'
bigmsg: db   '(too large for 16 bits)', 0x0D, 0x0A, '$'
```

**Output:** `1234 * 5678 = 0x006B4E2E = (too large for 16 bits)`

### Explanation

1234 × 5678 = 7,006,652 = `0x6B4E2E`. So `DX` = `0x006B` and `AX` = `0x4E2E`.

**`MUL BX` destroys `DX`** (Chapter 23 §2.1). If you had a value there, save it first. Here we use
`DX` deliberately, as the high half of the product.

**The `push`/`pop` dance** is because `print_hex32` and `print_udec` both need `DX:AX`, and the
`print` macro clobbers `DX`. Saving on the stack is simpler than finding spare registers.

**Testing `DX` for zero** is how you decide whether the product fits in 16 bits — and it is exactly
what `CF` and `OF` already told you after the `MUL`. `jnc` immediately after `mul bx` would be
equivalent and shorter:

```asm
        mul  bx
        jnc  .fits_in_16_bits   ; CF = 0 means DX is zero
```

---

## Program 39.5 — Divide with quotient and remainder

```asm
; div16.asm — divide, with a guard against both failure modes
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  bx, [divisor]
        or   bx, bx
        jz   .div_by_zero       ; guard 1: divisor must not be zero

        mov  ax, [dividend]
        xor  dx, dx             ; zero-extend to 32 bits: DX:AX = dividend
                                ; guard 2: with DX = 0 and a 16-bit divisor,
                                ;   the quotient always fits, so no INT 0.
        div  bx                 ; AX = quotient, DX = remainder

        mov  [quot], ax
        mov  [rem], dx

        print qmsg
        mov  ax, [quot]
        call print_udec
        newline

        print rmsg
        mov  ax, [rem]
        call print_udec
        newline
        exit 0

.div_by_zero:
        print zmsg
        exit 1

dividend: dw   50000
divisor:  dw   7
quot:     dw   0
rem:      dw   0
qmsg:     db   'Quotient:  $'
rmsg:     db   'Remainder: $'
zmsg:     db   'Division by zero!', 0x0D, 0x0A, '$'
```

**Output:**

```
Quotient:  7142
Remainder: 6
```

Check: 7142 × 7 + 6 = 49,994 + 6 = 50,000. ✔

### Explanation

**Two guards, both necessary.**

The `or bx, bx` / `jz` catches a zero divisor, which would generate `INT 0`.

The `xor dx, dx` does more than zero-extend. With `DX = 0`, the dividend is at most 65,535 and the
divisor is at least 1, so the quotient is at most 65,535 — which always fits in `AX`. **A 16-bit
`DIV` with `DX = 0` and a non-zero divisor can never overflow.** That is why this form is safe and
`div bl` (8-bit) is not.

---

## Program 39.6 — Signed division

```asm
; idiv16.asm — signed division, showing why CWD matters
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [dividend]
        cwd                     ; SIGN-extend AX into DX:AX.
                                ;   xor dx,dx would be WRONG for a negative value.
        mov  bx, [divisor]
        idiv bx                 ; AX = quotient, DX = remainder

        push dx
        print qmsg
        call print_sdec
        newline
        pop  ax
        print rmsg
        call print_sdec
        newline
        exit 0

dividend: dw   -100
divisor:  dw   7
qmsg:     db   'Quotient:  $'
rmsg:     db   'Remainder: $'
```

**Output:**

```
Quotient:  -14
Remainder: -2
```

### Explanation

−100 ÷ 7 = −14 remainder −2, because `IDIV` **truncates toward zero** and the remainder takes the
dividend's sign (Chapter 23 §5.2). Check: −14 × 7 = −98, and −98 + (−2) = −100. ✔

**If you wrote `xor dx, dx` instead of `cwd`:** `DX:AX` would be `0x0000FF9C` = 65,436, and the
answer would be 9348 remainder 0 — silently wrong, no crash, no flag.

---

## Program 39.7 — 32-bit addition

```asm
; add32.asm — add two 32-bit numbers
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]         ; low word of num1
        add  ax, [num2]         ; + low word of num2; CF = carry out
        mov  [result], ax       ; MOV does not disturb CF

        mov  ax, [num1+2]       ; high word
        adc  ax, [num2+2]       ; + high word + the carry
        mov  [result+2], ax

        print msg
        mov  dx, [result+2]
        mov  ax, [result]
        call print_hex32
        newline
        exit 0

num1:   dd   0x0001FFFF
num2:   dd   0x00020001
result: dd   0
msg:    db   '0001FFFF + 00020001 = 0x$'
```

**Output:** `0001FFFF + 00020001 = 0x00040000`

### Explanation

Low words: `0xFFFF + 0x0001 = 0x0000` with `CF = 1`.
High words: `0x0001 + 0x0002 + 1 = 0x0004`.
Result `0x00040000`. ✔

**Nothing between the `ADD` and the `ADC` may touch `CF`**, and `mov [result], ax` does not. If you
inserted `xor ah, ah` there, the carry would be lost and the answer would be `0x00030000`.

---

## Program 39.8 — 32-bit subtraction

```asm
; sub32.asm — subtract two 32-bit numbers
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        sub  ax, [num2]         ; CF = borrow out
        mov  [result], ax
        mov  ax, [num1+2]
        sbb  ax, [num2+2]       ; − the borrow
        mov  [result+2], ax

        print msg
        mov  dx, [result+2]
        mov  ax, [result]
        call print_hex32
        newline
        exit 0

num1:   dd   0x00040000
num2:   dd   0x00000001
result: dd   0
msg:    db   '00040000 - 00000001 = 0x$'
```

**Output:** `00040000 - 00000001 = 0x0003FFFF`

Low words: `0x0000 − 0x0001 = 0xFFFF` with `CF = 1` (borrow).
High words: `0x0004 − 0x0000 − 1 = 0x0003`. ✔

---

## Program 39.9 — 32 × 16 multiplication

Multiplying a 32-bit value by a 16-bit one needs two `MUL`s and an addition, because the 8086 has
only a 16×16 multiply.

```asm
; mul32x16.asm — multiply a 32-bit value by a 16-bit value
;
;   Let the 32-bit value be H:L (H = high word, L = low word) and the
;   multiplier be M.
;
;      (H×65536 + L) × M  =  H×M×65536  +  L×M
;
;   L×M gives a 32-bit partial product.
;   H×M gives another, shifted up by 16 bits.
;   Add them with the shift accounted for.
;
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- L × M ---
        mov  ax, [val]          ; low word L
        mul  word [mult]        ; DX:AX = L × M
        mov  [res], ax          ; the low word of the result is final
        mov  bx, dx             ; BX = the carry into the next 16 bits

        ; --- H × M ---
        mov  ax, [val+2]        ; high word H
        mul  word [mult]        ; DX:AX = H × M
        add  ax, bx             ; add the carry from the first product
        adc  dx, 0              ; propagate into the top word
        mov  [res+2], ax
        mov  [res+4], dx        ; the result is 48 bits wide

        print msg
        mov  ax, [res+4]
        call print_hex16
        mov  ax, [res+2]
        call print_hex16
        mov  ax, [res]
        call print_hex16
        newline
        exit 0

val:    dd   0x00012345
mult:   dw   0x1000
res:    times 3 dw 0            ; 48 bits
msg:    db   '00012345 * 1000 = 0x$'
```

**Output:** `00012345 * 1000 = 0x000012345000`

Check: `0x12345 × 0x1000 = 0x12345000`. ✔

### Explanation

`mul word [mult]` — a memory operand, so NASM needs the `word` keyword to know the operand size
(Chapter 33 §4.2).

**`adc dx, 0`** propagates the carry from `add ax, bx` into the top word. Without it, a carry out of
the middle word would be lost.

The result needs **48 bits** because 32 + 16 = 48. Declaring `res` as three words and printing all
three is the honest thing to do.

---

## Program 39.10 — Factorial

```asm
; fact.asm — compute n! iteratively
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print prompt
        call read_udec
        jc   .bad
        cmp  ax, 8              ; 8! = 40320 fits in 16 bits; 9! does not
        ja   .too_big
        mov  [n], ax
        newline

        ; --- the computation ---
        mov  cx, [n]
        mov  ax, 1              ; 0! = 1
        jcxz .done              ; guard: CX = 0 would loop 65536 times
.next:
        mul  cx                 ; DX:AX = AX × CX.  We ignore DX because
                                ;   we checked n <= 8 above.
        loop .next              ; CX counts down: n, n-1, ... 1
.done:
        mov  [result], ax

        print rmsg
        mov  ax, [n]
        call print_udec
        print emsg
        mov  ax, [result]
        call print_udec
        newline
        exit 0

.bad:
        print badmsg
        exit 1
.too_big:
        print bigmsg
        exit 1

n:       dw   0
result:  dw   0
prompt:  db   'Enter n (0-8): $'
rmsg:    db   '$'
emsg:    db   '! = $'
badmsg:  db   0x0D, 0x0A, 'Not a number.', 0x0D, 0x0A, '$'
bigmsg:  db   0x0D, 0x0A, 'Too large — 9! exceeds 16 bits.', 0x0D, 0x0A, '$'
```

**Output for input 7:** `7! = 5040`

### Explanation

**`LOOP` counts `CX` down from *n* to 1**, and `MUL CX` multiplies by each value in turn. That is
exactly `n × (n−1) × … × 1`, in reverse order — which does not matter, since multiplication is
commutative.

**The `jcxz` guard** is essential: with *n* = 0, `LOOP` would decrement `CX` to `0xFFFF` and run
65,536 times (Chapter 27 §5). With the guard, `AX` keeps its initial 1, which is the correct value
of 0!.

**The range check** is not optional. 9! = 362,880, which needs 19 bits. Without the check the program
would print 362,880 mod 65,536 = 34,944 and look confident about it.

---

## Program 39.11 — Greatest common divisor

Euclid's algorithm, using repeated subtraction (no division needed).

```asm
; gcd.asm — greatest common divisor by Euclid's algorithm
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [a]
        mov  bx, [b]

        ; --- Euclid: while a != b, subtract the smaller from the larger ---
.loop:
        cmp  ax, bx
        je   .done              ; equal -> that is the GCD
        ja   .a_bigger
        sub  bx, ax             ; b > a
        jmp  .loop
.a_bigger:
        sub  ax, bx             ; a > b
        jmp  .loop
.done:
        mov  [result], ax

        print msg
        mov  ax, [a]
        call print_udec
        print andmsg
        mov  ax, [b]
        call print_udec
        print eqmsg
        mov  ax, [result]
        call print_udec
        newline
        exit 0

a:       dw   1071
b:       dw   462
result:  dw   0
msg:     db   'gcd($'
andmsg:  db   ', $'
eqmsg:   db   ') = $'
```

**Output:** `gcd(1071, 462) = 21`

### Explanation

Trace:

```
   1071, 462  ->  609, 462  ->  147, 462  ->  147, 315  ->  147, 168
        ->  147, 21  ->  126, 21  ->  105, 21  ->  84, 21  ->  63, 21
        ->  42, 21  ->  21, 21  ->  done, answer 21
```

**`ja` not `jg`.** These are unsigned quantities, so the unsigned comparison is correct
(Chapter 27 §4.3). With `jg`, a value above 32,767 would compare as negative and the loop would run
forever.

**A faster version** uses `DIV` and the remainder:

```asm
.loop:
        or   bx, bx
        jz   .done              ; b = 0 -> a is the answer
        xor  dx, dx
        div  bx                 ; DX = a mod b
        mov  ax, bx             ; a <- b
        mov  bx, dx             ; b <- a mod b
        jmp  .loop
.done:                          ; AX = the GCD
```

Twelve iterations become three, at the cost of a 144-clock `DIV` each. For these numbers the
subtraction version is actually faster; for 65,000 and 3 it is catastrophically slower.

---

## Program 39.12 — 64-bit addition

```asm
; add64.asm — add two 64-bit numbers held as four words each
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, num1
        mov  di, num2
        mov  bx, result
        mov  cx, 4              ; four words = 64 bits
        clc                     ; no carry into the first word

.next:
        mov  ax, [si]
        adc  ax, [di]           ; add with the carry from the previous word
        mov  [bx], ax           ; MOV preserves CF

        inc  si                 ; INC preserves CF — `add si, 2` would NOT
        inc  si
        inc  di
        inc  di
        inc  bx
        inc  bx
        loop .next              ; LOOP preserves CF

        ; --- print the result, most significant word first ---
        print msg
        mov  ax, [result+6]
        call print_hex16
        mov  ax, [result+4]
        call print_hex16
        mov  ax, [result+2]
        call print_hex16
        mov  ax, [result]
        call print_hex16
        newline
        exit 0

; stored least significant word first
num1:   dw   0xFFFF, 0xFFFF, 0xFFFF, 0x0000
num2:   dw   0x0001, 0x0000, 0x0000, 0x0000
result: times 4 dw 0
msg:    db   '0000FFFFFFFFFFFF + 1 = 0x$'
```

**Output:** `0000FFFFFFFFFFFF + 1 = 0x0001000000000000`

### Explanation

**This is the whole of arbitrary-precision arithmetic**, in eleven instructions. The loop works
because every instruction in it except the `ADC` preserves `CF`:

| Instruction | Touches `CF`? |
|-------------|---------------|
| `mov ax, [si]` | no |
| `adc ax, [di]` | **yes — this is the point** |
| `mov [bx], ax` | no |
| `inc si` ×2 | **no** (Chapter 22 §6.1) |
| `inc di` ×2 | no |
| `inc bx` ×2 | no |
| `loop` | no |

Replace any `inc` with `add reg, 2` and the program breaks silently: the carry from word *n* never
reaches word *n*+1.

**To extend to 128 bits**, change `mov cx, 4` to `mov cx, 8` and widen the data. Nothing else
changes.

---

## Exercises

**39.1** Modify Program 39.1 to add three bytes, handling a total that may exceed 255.

**39.2** Write a program that reads two numbers from the keyboard and prints their sum, difference,
product and quotient.

**39.3** Program 39.4 prints "too large for 16 bits" by testing `DX`. Rewrite it to test `CF`
immediately after the `MUL` instead, and explain why that works.

**39.4** Program 39.5 is safe from `INT 0`. Write a version using an 8-bit `DIV` that is *not*, and
give an input that crashes it.

**39.5** Change Program 39.6's dividend to −7 and divisor to 2. Predict the quotient and remainder
before running it.

**39.6** Write a program that computes `(a + b) × (c − d)` for four 16-bit values, handling a 32-bit
product.

**39.7** Extend Program 39.9 to a full 32 × 32 = 64-bit multiply. (Hint: four 16×16 partial products,
each shifted by 0, 16, 16 and 32 bits.)

**39.8** Rewrite Program 39.10 to compute factorials up to 12! using a 32-bit accumulator.

**39.9** Add the range check to the fast GCD in §39.11 so that a zero input is handled correctly.

**39.10** Program 39.12 uses six `INC` instructions. Rewrite the pointer advance using `LEA` and
confirm that `CF` still survives. Which version is faster?

**39.11** Write a program that raises a number to a power using repeated multiplication, detecting
overflow.

**39.12** Write a routine that computes the average of an array of 16-bit values without overflowing
on the sum. (Hint: accumulate in 32 bits.)

**39.13** Write a program that converts a temperature from Celsius to Fahrenheit using `F = C×9/5 +
32`, correct for negative temperatures.

Answers in [Appendix H](H-exercise-solutions.md#chapter-39).

---

[← Macros and modular programs](38-macros-and-modules.md) · [Contents](README.md) · [Next: Programs: arrays →](40-programs-arrays.md)
