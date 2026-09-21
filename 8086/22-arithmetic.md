# Chapter 22 — Arithmetic instructions

[← Data transfer](21-data-transfer.md) · [Contents](README.md) · [Next: Multiply and divide →](23-multiply-divide.md)

---

## Goal

The eight instructions that add and subtract: `ADD`, `ADC`, `SUB`, `SBB`, `CMP`, `INC`, `DEC`,
`NEG`. For each: every operand form, the encoding, the clock count, and — most importantly — exactly
which flags it sets and why.

Then the technique that makes them useful beyond 16 bits: multi-precision arithmetic built on the
carry flag.

---

## 1. `ADD`

```asm
        add  destination, source        ; destination <- destination + source
```

### 1.1 Forms

| Form | Example | Opcode | Bytes | Clocks |
|------|---------|--------|-------|--------|
| reg, reg | `add ax, bx` | `01`/`03` | 2 | 3 |
| reg, mem | `add ax, [bx]` | `03` | 2–4 | 9 + EA |
| mem, reg | `add [bx], ax` | `01` | 2–4 | 16 + EA |
| reg, imm | `add bx, 5` | `81`/`83 /0` | 3–4 | 4 |
| mem, imm | `add word [bx], 5` | `81`/`83 /0` | 3–6 | 17 + EA |
| **acc, imm** | `add ax, 5` | `05` | 3 | 4 |

Note the asymmetry: `add ax, [bx]` is **9 + EA** clocks but `add [bx], ax` is **16 + EA**. The
memory destination costs 7 more, because it needs a read *and* a write bus cycle instead of just a
read. Whenever you have a choice, **accumulate in a register**:

```asm
; slow — 1000 × (16+5) = 21,000 clocks plus 2000 bus cycles
        mov  cx, 1000
.next:  add  [total], ax
        loop .next

; fast — accumulate in BX, store once
        mov  cx, 1000
        mov  bx, [total]
.next:  add  bx, ax             ; 3 clocks, no bus cycle
        loop .next
        mov  [total], bx
```

3000 clocks instead of 21,000.

### 1.2 Flags

`ADD` sets **all six** status flags: `CF`, `PF`, `AF`, `ZF`, `SF`, `OF`.

| Flag | Set when |
|------|----------|
| `CF` | carry out of the most significant bit — the unsigned result did not fit |
| `OF` | carry into the MSB ≠ carry out of it — the signed result did not fit |
| `ZF` | the result is exactly zero |
| `SF` | the result's most significant bit is 1 |
| `AF` | carry out of bit 3 |
| `PF` | the low byte of the result has an even number of 1 bits |

### 1.3 Worked example

```asm
        mov  al, 0x9C           ; 156 unsigned, −100 signed
        add  al, 0x7B           ; 123
```

```
      1001 1100
    + 0111 1011
    -----------
    1 0001 0111      AL = 0x17, carry out = 1
```

`CF = 1` (156 + 123 = 279 > 255). `OF = 0` (−100 + 123 = +23, which fits). `SF = 0`. `ZF = 0`.
`AF = 1` (`C + B` = 23 ≥ 16). `PF = 1` (`0x17` has four 1 bits).

Both readings are simultaneously available in the flags. Chapter 8 §11 walks this same example.

---

## 2. `ADC` — add with carry

```asm
        adc  destination, source        ; destination <- destination + source + CF
```

Identical forms, encodings and clock counts to `ADD` (opcodes `10`–`15`, group extension `/2`).

Its whole purpose is multi-precision arithmetic (§8).

```asm
        mov  al, 0xFF
        add  al, 0x01           ; AL = 0x00, CF = 1
        mov  bl, 0x10
        adc  bl, 0x00           ; BL = 0x10 + 0 + 1 = 0x11
```

**Nothing between the `ADD` and the `ADC` may disturb `CF`.** `MOV`, `PUSH`, `POP`, `LEA`, `INC` and
`DEC` are all safe. `AND`, `OR`, `XOR`, `TEST` and every shift are not.

---

## 3. `SUB`

```asm
        sub  destination, source        ; destination <- destination − source
```

Opcodes `28`–`2D`, group extension `/5`. Same forms and clocks as `ADD`.

### 3.1 How the processor actually does it

It does not subtract. It computes

```
   A − B  =  A + (NOT B) + 1
```

using the same adder. Two consequences:

**`CF` after a subtraction means *borrow*.** It is the **inverted** carry-out of the adder. `CF = 1`
means "the first operand was, as an unsigned number, smaller than the second".

**`OF` after a subtraction** follows the rule: overflow happens when the operands have *different*
signs and the result's sign differs from the first operand's.

```asm
        mov  al, 3
        sub  al, 5              ; AL = 0xFE (−2), CF = 1 (3 < 5, borrow), OF = 0
        mov  al, 0x80           ; −128
        sub  al, 1              ; AL = 0x7F (+127), OF = 1, CF = 0
```

The second case: −128 − 1 = −129, which does not fit in a signed byte, so `OF = 1`. Unsigned,
128 − 1 = 127 fits perfectly, so `CF = 0`.

---

## 4. `SBB` — subtract with borrow

```asm
        sbb  destination, source        ; destination <- destination − source − CF
```

Opcodes `18`–`1D`, group extension `/3`. The mirror of `ADC`, for multi-precision subtraction.

---

## 5. `CMP`

```asm
        cmp  first, second              ; compute first − second, set flags, DISCARD the result
```

Opcodes `38`–`3D`, group extension `/7`. Same forms and clocks as `SUB`.

**`CMP` and `SUB` set the flags identically.** The only difference is that `CMP` throws the answer
away. So everything in §3 about `CF` and `OF` applies unchanged.

### 5.1 Reading the flags after `CMP`

After `cmp a, b`:

| Condition | Flags | Unsigned jump | Signed jump |
|-----------|-------|---------------|-------------|
| `a == b` | `ZF = 1` | `JE` / `JZ` | `JE` / `JZ` |
| `a != b` | `ZF = 0` | `JNE` / `JNZ` | `JNE` / `JNZ` |
| `a < b` | `CF = 1` / `SF ≠ OF` | `JB` / `JNAE` / `JC` | `JL` / `JNGE` |
| `a <= b` | `CF = 1 or ZF = 1` / `ZF = 1 or SF ≠ OF` | `JBE` / `JNA` | `JLE` / `JNG` |
| `a > b` | `CF = 0 and ZF = 0` | `JA` / `JNBE` | `JG` / `JNLE` |
| `a >= b` | `CF = 0` / `SF = OF` | `JAE` / `JNB` / `JNC` | `JGE` / `JNL` |

**Use the right column.** Mixing them is the commonest logic bug in 8086 code:

```asm
        mov  ax, 0x8000         ; −32768 signed, 32768 unsigned
        cmp  ax, 1
        jb   below              ; NOT taken — unsigned, 32768 > 1
        jl   less               ; TAKEN     — signed,  −32768 < 1
```

Both are correct; they are answering different questions. Decide which question you meant.

### 5.2 Testing for zero

```asm
        cmp  ax, 0              ; 3 or 4 bytes
        or   ax, ax             ; 2 bytes, same effect on ZF and SF
        test ax, ax             ; 2 bytes, same
```

All three work. `OR`/`TEST` are shorter, but they clear `CF` and `OF` as a side effect. Inside a
multi-precision loop, use `CMP`.

---

## 6. `INC` and `DEC`

```asm
        inc  destination                ; destination <- destination + 1
        dec  destination                ; destination <- destination − 1
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `INC r16` | `40+r` | **1** | 2 |
| `DEC r16` | `48+r` | **1** | 2 |
| `INC r8` | `FE /0` | 2 | 3 |
| `DEC r8` | `FE /1` | 2 | 3 |
| `INC/DEC mem` | `FE`/`FF` | 2–4 | 15 + EA |

**`INC AX` is one byte and two clocks.** `ADD AX, 1` is three bytes and four clocks. Always use
`INC` where you can.

### 6.1 They do not touch `CF`

`INC` and `DEC` set `OF`, `SF`, `ZF`, `AF` and `PF` — but **leave `CF` unchanged**.

This is not an oversight. It is what allows:

```asm
        clc
.next:  mov  ax, [si]
        adc  ax, [di]           ; uses CF from the previous iteration
        mov  [bx], ax
        inc  si                 ; CF survives
        inc  si
        inc  di
        inc  di
        inc  bx
        inc  bx
        loop .next              ; LOOP does not touch flags either
```

Pointer arithmetic between the `ADC`s does not destroy the carry chain. If `INC` cleared `CF`, every
multi-precision loop would need `PUSHF`/`POPF`.

### 6.2 `DEC` and loop termination

```asm
        mov  cx, 10
.next:  ; ...
        dec  cx
        jnz  .next              ; DEC sets ZF
```

This is `LOOP` written out (Chapter 27 §6). `LOOP` is one byte shorter but, on an 8086, `DEC`+`JNZ`
is actually *faster* (2 + 16 = 18 clocks vs `LOOP`'s 17 — about the same; on later chips `DEC`/`JNZ`
wins clearly).

### 6.3 Overflow at the boundaries

```asm
        mov  al, 0x7F
        inc  al                 ; AL = 0x80, OF = 1, SF = 1, CF unchanged
        mov  al, 0xFF
        inc  al                 ; AL = 0x00, ZF = 1, OF = 0, CF unchanged (!)
```

Note the second case: wrapping from 255 to 0 is an *unsigned* overflow, which would normally set
`CF` — but `INC` does not touch `CF`, so you cannot detect it this way. If you need to, use
`ADD AL, 1`.

---

## 7. `NEG`

```asm
        neg  destination                ; destination <- 0 − destination
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `NEG r8`/`r16` | `F6`/`F7 /3` | 2 | 3 |
| `NEG mem` | `F6`/`F7 /3` | 2–4 | 16 + EA |

Computes the two's complement: invert all bits and add 1 (Chapter 2 §5.2).

### 7.1 Flags

- `CF` is set to **1 unless the operand was zero**. (Because `0 − x` borrows for every non-zero `x`.)
- `OF` is set **only when the operand is `0x80` (byte) or `0x8000` (word)**.
- `ZF`, `SF`, `PF`, `AF` from the result.

### 7.2 The one value that cannot be negated

```asm
        mov  al, 0x80           ; −128
        neg  al                 ; AL = 0x80 still, OF = 1
```

+128 does not exist in a signed byte, so `NEG` returns the same value and flags the overflow. Same
for `0x8000` as a word. This is the asymmetry of two's complement (Chapter 2 §5.3) showing itself.

Any code that negates user data must check `OF`, or accept that one input in 256 is wrong.

### 7.3 `NEG` versus `NOT`

```asm
        mov  al, 5
        not  al                 ; AL = 0xFA = one's complement,  NO FLAGS CHANGED
        mov  al, 5
        neg  al                 ; AL = 0xFB = two's complement,  flags set
```

`NOT` is bitwise complement and touches no flags. `NEG` is arithmetic negation and sets all of them.

---

## 8. Multi-precision arithmetic

The technique that makes 16-bit arithmetic sufficient for any width.

### 8.1 32-bit addition

Two 32-bit values at `a` and `b`, result to `r`:

```asm
        mov  ax, [a]            ; low word of a
        add  ax, [b]            ; + low word of b       -> CF = carry out
        mov  [r], ax            ; store the low word     (MOV preserves CF)
        mov  ax, [a+2]          ; high word of a
        adc  ax, [b+2]          ; + high word of b + CF
        mov  [r+2], ax
```

Six instructions. The `MOV` in the middle is safe because `MOV` sets no flags.

### 8.2 32-bit subtraction

```asm
        mov  ax, [a]
        sub  ax, [b]            ; CF = borrow out
        mov  [r], ax
        mov  ax, [a+2]
        sbb  ax, [b+2]          ; − CF
        mov  [r+2], ax
```

### 8.3 Arbitrary precision — a loop

```asm
; Add two N-word numbers. DS:SI -> a, ES:DI -> b, and the result goes into a.
;   CX = number of words
add_big:
        clc                     ; start with no carry
.next:
        mov  ax, [si]
        adc  ax, [es:di]        ; add with the carry from last time
        mov  [si], ax
        inc  si                 ; pointer bumps do not disturb CF
        inc  si
        inc  di
        inc  di
        loop .next              ; LOOP does not disturb CF either
        ret                     ; CF on exit = the final carry out
```

**Every instruction inside the loop preserves `CF` except the `ADC` itself.** That is the whole
trick, and it is why `INC` and `LOOP` are designed the way they are.

`add si, 2` would **not** work here — `ADD` sets `CF`. You must use two `INC`s, or `LEA SI, [SI+2]`,
which also leaves flags alone.

### 8.4 Comparing multi-precision values

Compare the **high** words first, and only fall through to the low words when they are equal:

```asm
; Compare the 32-bit values at [a] and [b]. Sets flags as if a − b.
        mov  ax, [a+2]
        cmp  ax, [b+2]          ; high words
        jne  .done              ; decided — the flags are already right
        mov  ax, [a]
        cmp  ax, [b]            ; low words decide
.done:
        ret
```

A common mistake is to do the whole subtraction with `SUB`/`SBB` and then test the flags. `CF` comes
out right — the final `SBB`'s borrow is the true 32-bit borrow — but **`ZF` is set from the high word
alone** and is therefore worthless:

```
   0x00010001 − 0x00010000 :  low  0x0001 − 0x0000 = 0x0001, CF = 0
                              high 0x0001 − 0x0001 − 0 = 0x0000, ZF = 1
                              -> ZF = 1, yet the two values are not equal
```

So `SUB`/`SBB` is fine for computing a difference and for `JB`/`JAE` via `CF`, but never use its `ZF`
for equality. Compare word by word, as above.

---

## 9. Worked program — 32-bit addition with output

```asm
; add32.asm — add two 32-bit numbers and print the result in hex
; nasm -f bin add32.asm -o add32.com
;
;   DX:AX holds the running 32-bit value in several places
        org  0x100

start:
        ; --- the addition ---
        mov  ax, [num1]         ; low word of num1
        add  ax, [num2]         ; + low word of num2, CF = carry
        mov  [result], ax       ; MOV does not touch CF
        mov  ax, [num1+2]       ; high word
        adc  ax, [num2+2]       ; + high word + carry
        mov  [result+2], ax

        ; --- print it as 8 hex digits, high word first ---
        mov  ax, [result+2]
        call print_hex16
        mov  ax, [result]
        call print_hex16

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; print_hex16 — print AX as four hex digits.
;   Destroys nothing the caller cares about (AX, BX, CX, DX saved).
; ---------------------------------------------------------------
print_hex16:
        push ax
        push bx
        push cx
        push dx

        mov  cx, 4              ; four digits
        mov  bx, ax             ; keep the value in BX; AX is needed for DOS
.digit:
        rol  bx, 1              ; rotate the top nibble down to the bottom
        rol  bx, 1              ; (8086 has no ROL bx,4)
        rol  bx, 1
        rol  bx, 1
        mov  dl, bl
        and  dl, 0x0F           ; isolate the nibble
        add  dl, '0'
        cmp  dl, '9'
        jbe  .emit
        add  dl, 7              ; 'A'..'F' are 7 past '9'+1
.emit:
        mov  ah, 0x02           ; DOS: print the character in DL
        int  0x21
        loop .digit

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
num1:   dd   0x0001FFFF
num2:   dd   0x00020001
result: dd   0
crlf:   db   0x0D, 0x0A, '$'
```

**Expected output:** `00040000`

Check by hand: `0x0001FFFF + 0x00020001`. Low words: `0xFFFF + 0x0001 = 0x0000` with `CF = 1`.
High words: `0x0001 + 0x0002 + 1 = 0x0004`. Result `0x00040000`. ✔

### 9.1 Notes on the code

**`rol bx, 1` four times.** The 8086 has no immediate shift count greater than 1 (Chapter 20 §5.2).
The alternative is `mov cl, 4` / `rol bx, cl`, which is 2 instructions and 8+4×4 = 24 clocks versus
4 × 2 = 8 clocks for four `ROL`s. The four-instruction version is faster here.

**Why `ROL` and not `SHR`.** After four `ROL`s the top nibble is in the bottom four bits *and* the
original value has rotated around by four, so after four iterations `BX` is back where it started —
which is exactly what we want for printing four digits left to right.

**`add dl, 7`.** ASCII `'9'` is `0x39` and `'A'` is `0x41` — a gap of 8, so after adding `'0'` we
need 7 more. Chapter 43 does this with `XLAT` instead, in fewer instructions.

---

## 10. Common patterns

```asm
; clear a register (2 bytes, 3 clocks — shorter than MOV AX,0)
        xor  ax, ax
        sub  ax, ax             ; equivalent

; add 2 to a pointer without disturbing CF
        inc  si
        inc  si
        lea  si, [si+2]         ; also flag-safe, 2+5 = 7 clocks — slower

; test whether AX is zero
        or   ax, ax
        jz   is_zero

; absolute value of AX
        or   ax, ax
        jns  .positive
        neg  ax
.positive:

; is AX even?
        test al, 1
        jz   even

; round AX up to the next multiple of 16
        add  ax, 15
        and  ax, 0xFFF0
```

---

## 11. Summary

```
  ADD dst,src   dst = dst + src          all six status flags
  ADC dst,src   dst = dst + src + CF     for multi-precision
  SUB dst,src   dst = dst − src          CF means BORROW
  SBB dst,src   dst = dst − src − CF
  CMP a,b       flags from a − b, result discarded — same flags as SUB
  INC dst       dst = dst + 1            ALL FLAGS EXCEPT CF
  DEC dst       dst = dst − 1            ALL FLAGS EXCEPT CF
  NEG dst       dst = 0 − dst            CF=1 unless dst was 0; OF=1 only for 80h/8000h

  memory destination costs ~7 more clocks than a register destination
  INC r16 is ONE byte; ADD r16,1 is three

  multi-precision:  ADD then ADC (or SUB then SBB), low half first
                    nothing between them may disturb CF
                    MOV, INC, DEC, LEA, PUSH, POP and LOOP are all safe
                    ADD, AND, OR, XOR, TEST and shifts are NOT

  after CMP: unsigned -> JB JBE JA JAE JC JNC
             signed   -> JL JLE JG JGE JO JNO
             never mix them
```

---

## Exercises

**22.1** For each, give the result and `CF`, `OF`, `ZF`, `SF`:

```
(a) mov al,0x64 : add al,0x64      (b) mov al,0xC8 : add al,0xC8
(c) mov al,0x14 : sub al,0x28      (d) mov ax,0x7FFF : inc ax
(e) mov al,0x80 : neg al           (f) mov al,0x00 : neg al
```

**22.2** Why does `add [total], ax` cost seven more clocks than `add ax, [total]`?

**22.3** Rewrite this loop so the accumulation happens in a register, and state the clock saving for
1000 iterations.

```asm
        mov  cx, 1000
.next:  add  word [total], 5
        loop .next
```

**22.4** Write the six instructions that add the 32-bit value at `[b]` into the 32-bit value at
`[a]`.

**22.5** In the loop of §8.3, why is `inc si` used twice instead of `add si, 2`?

**22.6** `INC` does not affect `CF`. Give a concrete three-instruction fragment that would produce
the wrong answer if it did.

**22.7** `AL = 0xFF`. After `inc al`, what are `AL`, `ZF` and `CF`? How would you detect the
unsigned wrap?

**22.8** Which single byte value cannot be negated correctly, and what does `NEG` leave in the
register and the flags?

**22.9** `AX = 0x8000`, `BX = 0x0001`. After `cmp ax, bx`, state whether each of `JB`, `JA`, `JL`,
`JG`, `JE` is taken, and explain each answer in one sentence.

**22.10** Write the four-instruction sequence that computes the absolute value of `AX`. What happens
if `AX = 0x8000`?

**22.11** Write code that compares the 32-bit value at `[a]` with the one at `[b]` and jumps to
`a_bigger` if `a > b`, treating both as unsigned.

**22.12** In the `print_hex16` routine of §9, why is `ROL` used rather than `SHR`? What would go
wrong with `SHR`?

**22.13** Write a routine that adds two 64-bit numbers held as four consecutive words each.

Answers in [Appendix H](H-exercise-solutions.md#chapter-22).

---

[← Data transfer](21-data-transfer.md) · [Contents](README.md) · [Next: Multiply and divide →](23-multiply-divide.md)
