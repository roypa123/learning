# Chapter 23 — Multiply and divide

[← Arithmetic](22-arithmetic.md) · [Contents](README.md) · [Next: BCD and ASCII adjust →](24-bcd-ascii-adjust.md)

---

## Goal

Six instructions: `MUL`, `IMUL`, `DIV`, `IDIV`, `CBW`, `CWD`. They are the most *implicit*
instructions in the 8086 — they read and write registers you never mention — and `DIV` is the only
instruction in the whole set that can crash your program by itself.

Cover the implicit operands, the flag behaviour, the divide-overflow trap, and how to multiply and
divide faster than these instructions can.

---

## 1. Why these are different

Every instruction so far had the form `op dst, src`. These have the form `op src`, and everything
else is assumed:

```asm
        mul  bl                 ; AX  <- AL × BL
        mul  bx                 ; DX:AX <- AX × BX
        div  bl                 ; AL <- AX / BL,    AH <- AX mod BL
        div  bx                 ; AX <- DX:AX / BX, DX <- DX:AX mod BX
```

The reason is that a product is **twice as wide** as its operands. 8 × 8 = 16 bits; 16 × 16 = 32
bits. There is nowhere in a two-operand instruction to put a double-width result, so Intel fixed the
destination: `AX` for byte operations, `DX:AX` for word operations.

Division is the same in reverse: the dividend must be double width, so it comes from `AX` or
`DX:AX`.

**Consequence you must internalise: `MUL BX` destroys `DX`.** If you had something in `DX`, it is
gone. This is the single most common bug with these instructions.

---

## 2. `MUL` — unsigned multiply

```asm
        mul  source
```

### 2.1 The two sizes

| Source size | Implicit operand | Result | Example |
|-------------|------------------|--------|---------|
| 8-bit (`BL`, `[bx]`, …) | `AL` | **`AX`** | `mul bl` → `AX = AL × BL` |
| 16-bit (`BX`, `[bx]`, …) | `AX` | **`DX:AX`** | `mul bx` → `DX:AX = AX × BX` |

`DX:AX` means the 32-bit value with `DX` as the high word and `AX` as the low word.

The source can be a register or memory, but **never an immediate**. `mul 10` does not exist; you
must load the 10 into a register first.

### 2.2 Encoding and clocks

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `MUL r8` | `F6 /4` | 2 | **70–77** |
| `MUL m8` | `F6 /4` | 2–4 | 76–83 + EA |
| `MUL r16` | `F7 /4` | 2 | **118–133** |
| `MUL m16` | `F7 /4` | 2–4 | 124–139 + EA |

**Look at those numbers.** A 16-bit multiply takes up to 133 clocks — 26.6 µs at 5 MHz — against 3
for an `ADD`. The range (118 to 133) exists because the microcode is a shift-and-add loop whose
length depends on the operand bits.

This is why §7 on multiplying by shifting matters.

### 2.3 Flags

**Only `CF` and `OF` are meaningful**, and they carry a specific message:

```
   CF = OF = 0   ->  the upper half of the result is zero; the product fits in the lower half
   CF = OF = 1   ->  the upper half is non-zero; you need both halves
```

So after `MUL BL`, `CF = 1` means "`AH` is not zero". After `MUL BX`, `CF = 1` means "`DX` is not
zero".

`SF`, `ZF`, `AF` and `PF` are **undefined** — different steppings leave different values. Never
branch on them.

```asm
        mov  al, 20
        mov  bl, 10
        mul  bl                 ; AX = 200 = 0x00C8. AH = 0, so CF = OF = 0
        jnc  fits_in_a_byte     ; taken
```

### 2.4 Worked examples

```asm
        mov  al, 0x10           ; 16
        mov  bl, 0x10           ; 16
        mul  bl                 ; AX = 256 = 0x0100.  AH = 1, so CF = OF = 1

        mov  ax, 0x1234
        mov  bx, 0x0010
        mul  bx                 ; 0x1234 × 0x10 = 0x12340
                                ; DX = 0x0001, AX = 0x2340. CF = OF = 1

        mov  ax, 1000
        mov  bx, 60
        mul  bx                 ; 60,000 = 0xEA60. DX = 0, AX = 0xEA60. CF = OF = 0
```

---

## 3. `IMUL` — signed multiply

Same shapes, same implicit registers, but the operands are treated as two's complement.

| Form | Opcode | Clocks |
|------|--------|--------|
| `IMUL r8` | `F6 /5` | 80–98 |
| `IMUL r16` | `F7 /5` | 128–154 |

### 3.1 Flags mean something slightly different

```
   CF = OF = 0   ->  the upper half is merely the SIGN EXTENSION of the lower half
   CF = OF = 1   ->  the upper half contains significant bits
```

For `IMUL BL`: `CF = 0` means `AH` is `0x00` (if `AL` is positive) or `0xFF` (if `AL` is negative) —
i.e. the 16-bit result fits in `AL` as a signed byte.

### 3.2 Why `MUL` and `IMUL` give different answers

```asm
        mov  al, 0xFF           ; 255 unsigned, −1 signed
        mov  bl, 0x02
        mul  bl                 ; AX = 255 × 2 = 510 = 0x01FE
        ; versus
        mov  al, 0xFF
        mov  bl, 0x02
        imul bl                 ; AX = (−1) × 2 = −2 = 0xFFFE
```

Same bits in, different bits out. **The processor cannot know which you meant; you must choose the
instruction.**

The low half is actually the same in both cases (`0xFE`), and that is a general fact: signed and
unsigned multiplication agree on the low *n* bits. They differ only in the upper half. That is why
32-bit processors got away with a single `IMUL` for the common "I only want the low half" case.

---

## 4. `DIV` — unsigned divide

```asm
        div  source
```

### 4.1 The two sizes

| Source size | Dividend | Quotient | Remainder |
|-------------|----------|----------|-----------|
| 8-bit | **`AX`** (16 bits) | `AL` | `AH` |
| 16-bit | **`DX:AX`** (32 bits) | `AX` | `DX` |

The dividend is **always twice the width of the divisor**. This is the detail that causes most
divide bugs: before an 8-bit `DIV` you must set up the whole of `AX`, and before a 16-bit `DIV` you
must set up the whole of `DX:AX`.

### 4.2 Encoding and clocks

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `DIV r8` | `F6 /6` | 2 | **80–90** |
| `DIV m8` | `F6 /6` | 2–4 | 86–96 + EA |
| `DIV r16` | `F7 /6` | 2 | **144–162** |
| `DIV m16` | `F7 /6` | 2–4 | 150–168 + EA |

Up to 162 clocks — 32 µs at 5 MHz. Division is the most expensive thing the 8086 does.

### 4.3 All flags are undefined

`CF`, `OF`, `SF`, `ZF`, `AF`, `PF` — every one of them is undefined after `DIV` and `IDIV`. Do not
branch on any flag after a division. If you need to test the quotient or remainder, compare
explicitly:

```asm
        div  bl
        or   ah, ah             ; NOW the flags mean something
        jz   divided_exactly
```

### 4.4 The divide-overflow trap

**This is the important part of the chapter.**

If the quotient does not fit in its destination — `AL` for an 8-bit divide, `AX` for a 16-bit one —
the 8086 does **not** set a flag. It generates **interrupt 0**, the divide-error exception, exactly
as if you had written `INT 0`.

Two situations cause it:

**Division by zero.**

```asm
        mov  ax, 100
        mov  bl, 0
        div  bl                 ; INT 0 — division by zero
```

**Quotient too large.**

```asm
        mov  ax, 1000           ; dividend
        mov  bl, 2              ; divisor
        div  bl                 ; 1000 / 2 = 500, which does NOT fit in AL (max 255)
                                ; INT 0 — divide overflow
```

The second case catches people constantly. `1000 / 2` is a perfectly reasonable calculation, and it
crashes, because the *quotient* is too big for the 8-bit destination. Under DOS the result is
typically a "Divide overflow" message and immediate termination.

### 4.5 Avoiding it

**Use the wider form.** If the quotient might exceed 255, use a 16-bit divide:

```asm
        mov  ax, 1000
        xor  dx, dx             ; DX:AX = 1000, zero-extended
        mov  bx, 2
        div  bx                 ; AX = 500, DX = 0 — fine
```

**Check the divisor first.**

```asm
        or   bl, bl
        jz   .div_by_zero
        div  bl
```

**Check the magnitude first**, when the quotient's size is in doubt:

```asm
; 16-bit dividend in AX, 8-bit divisor in BL.
; The quotient fits in AL only if AH < BL.
        mov  bh, 0
        cmp  ah, bl
        jae  .would_overflow
        div  bl
```

**Or install a handler for `INT 0`** and recover (Chapter 31 §6).

### 4.6 The interrupt-0 return address quirk

On the 8086 and 8088, the address pushed by the divide-error interrupt is the address of the
**instruction following** the `DIV`... actually, it is the address of the `DIV` instruction itself on
some steppings and the following instruction on others — the 8086 documentation is inconsistent and
the behaviour differs from the 80286 onward, where it reliably points at the faulting instruction.

The practical consequence: an `INT 0` handler on an 8086 cannot reliably resume. It should report
and terminate. Do not write a handler that tries to fix up and continue.

---

## 5. `IDIV` — signed divide

Same shapes, signed operands.

| Form | Opcode | Clocks |
|------|--------|--------|
| `IDIV r8` | `F6 /7` | 101–112 |
| `IDIV r16` | `F7 /7` | 165–184 |

### 5.1 Ranges

The quotient must fit in the destination *as a signed value*:

```
   8-bit  IDIV :  quotient must be in −128 … +127
   16-bit IDIV :  quotient must be in −32768 … +32767
```

Outside that, `INT 0`.

### 5.2 The sign of the remainder

**The remainder takes the sign of the dividend.** This is truncation toward zero, the same
convention C uses:

```
    −7 /  2  =  −3  remainder  −1
     7 / −2  =  −3  remainder  +1
    −7 / −2  =  +3  remainder  −1
```

Not floor division. `−7 / 2` is `−3`, not `−4`.

### 5.3 Preparing the dividend correctly

For `IDIV`, the dividend must be properly **sign-extended**, not zero-extended:

```asm
; WRONG for a negative dividend
        mov  ax, -100
        xor  dx, dx             ; DX = 0 -> DX:AX = 0x0000FF9C = 65436, not −100
        mov  bx, 7
        idiv bx                 ; wrong answer

; RIGHT
        mov  ax, -100
        cwd                     ; DX:AX = 0xFFFFFF9C = −100
        mov  bx, 7
        idiv bx                 ; AX = −14, DX = −2   ✔
```

That is what `CBW` and `CWD` are for.

---

## 6. `CBW` and `CWD`

```asm
        cbw                     ; 98 — Convert Byte to Word:  AL -> AX
        cwd                     ; 99 — Convert Word to Doubleword: AX -> DX:AX
```

| Instruction | Bytes | Clocks | What it does |
|-------------|-------|--------|--------------|
| `CBW` | 1 | 2 | copies bit 7 of `AL` into every bit of `AH` |
| `CWD` | 1 | 5 | copies bit 15 of `AX` into every bit of `DX` |

```asm
        mov  al, 0x7F           ; +127
        cbw                     ; AX = 0x007F

        mov  al, 0x80           ; −128
        cbw                     ; AX = 0xFF80

        mov  ax, 0x1234
        cwd                     ; DX = 0x0000

        mov  ax, 0x8234         ; negative
        cwd                     ; DX = 0xFFFF
```

Neither affects any flag.

### 6.1 The rule

```
   before IDIV with an 8-bit divisor  :  CBW   (extends AL into AX)
   before IDIV with a 16-bit divisor  :  CWD   (extends AX into DX:AX)

   before DIV  with an 8-bit divisor  :  mov ah, 0
   before DIV  with a 16-bit divisor  :  xor dx, dx
```

**Signed → sign-extend. Unsigned → zero-extend.** Getting this wrong gives wildly wrong answers
rather than a crash, which makes it harder to find.

---

## 7. Multiplying and dividing faster

Given that `MUL` costs up to 133 clocks, it is worth avoiding.

### 7.1 Powers of two — use shifts

```asm
        shl  ax, 1              ; AX × 2      2 clocks
        shl  ax, 1              ; AX × 4      4 clocks total
        shl  ax, 1              ; AX × 8      6
        shl  ax, 1              ; AX × 16     8 clocks vs 118+ for MUL
```

`SHR` divides unsigned; `SAR` divides signed (with a caveat — §7.4).

### 7.2 Other constants — shift and add

Multiply by 10:

```asm
; AX × 10 = AX × 8 + AX × 2
        mov  bx, ax
        shl  ax, 1              ; AX = 2n
        shl  ax, 1              ; 4n
        shl  ax, 1              ; 8n
        shl  bx, 1              ; BX = 2n
        add  ax, bx             ; 10n
```

Six instructions, about 13 clocks, versus 118+ for `MUL`. Nearly ten times faster.

Multiply by 100 = 64 + 32 + 4:

```asm
; n × 100 = n×64 + n×32 + n×4
        mov  bx, ax             ; BX = n
        mov  cl, 6
        shl  ax, cl             ; AX = 64n        8 + 4×6 = 32 clocks
        mov  dx, bx
        mov  cl, 5
        shl  dx, cl             ; DX = 32n        8 + 4×5 = 28
        add  ax, dx             ; 96n
        shl  bx, 1
        shl  bx, 1              ; BX = 4n
        add  ax, bx             ; 100n
```

About 80 clocks — still faster than `MUL`'s 118–133, but the margin has narrowed and three registers
are now in use. **The crossover on an 8086 is around four or five shift-add steps.** Beyond that,
`MUL` is both faster and clearer.

### 7.3 Multiplying by a variable — still use `MUL`

Shift-and-add only works when the multiplier is a constant known at assembly time. For a run-time
multiplier, `MUL` is what you have.

### 7.4 Signed division by a power of two — the `SAR` trap

`SAR` shifts right arithmetically, preserving the sign bit. It looks like signed division by 2, and
almost is:

```asm
        mov  ax, -8
        sar  ax, 1              ; AX = -4   ✔
        mov  ax, -7
        sar  ax, 1              ; AX = -4   ✘ — IDIV would give −3
```

**`SAR` rounds toward negative infinity; `IDIV` truncates toward zero.** For negative odd numbers
they differ by one. If that matters, correct the value before shifting:

```asm
        cwd                     ; DX = 0x0000 if AX >= 0, 0xFFFF if AX < 0
        sub  ax, dx             ; subtracting −1 adds 1, but only when negative
        sar  ax, 1              ; now equivalent to IDIV by 2
```

Trace it with `AX = −7` (`0xFFF9`): `CWD` gives `DX = 0xFFFF`; `SUB AX, DX` gives
`0xFFF9 − 0xFFFF = 0xFFFA` = −6; `SAR` gives −3 — which is what `IDIV` would produce. With `AX = 7`,
`DX = 0`, nothing is added, and `SAR` gives 3. ✔

Three instructions and about 9 clocks, against `IDIV`'s 165+. Or just use `IDIV` and accept the
clocks when clarity matters more.

---

## 8. Worked program — divide and print quotient and remainder

```asm
; divmod.asm — divide two 16-bit numbers, print quotient and remainder
; nasm -f bin divmod.asm -o divmod.com
        org  0x100

start:
        mov  ax, [dividend]
        xor  dx, dx             ; zero-extend: DX:AX = dividend (UNSIGNED)
        mov  bx, [divisor]

        or   bx, bx             ; guard against division by zero
        jz   .divzero

        div  bx                 ; AX = quotient, DX = remainder
        mov  [quot], ax
        mov  [rem], dx

        mov  dx, qmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, [quot]
        call print_dec

        mov  dx, rmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, [rem]
        call print_dec

        mov  ax, 0x4C00
        int  0x21

.divzero:
        mov  dx, zmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C01
        int  0x21

; ---------------------------------------------------------------
; print_dec — print the unsigned 16-bit value in AX as decimal,
;             followed by CR LF.
;
; Method: repeatedly divide by 10, pushing each remainder, then pop
;         them back in reverse order. The stack reverses the digits
;         for us, which is why this is the standard idiom.
; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx

        mov  bx, 10
        xor  cx, cx             ; CX counts the digits pushed

.divide:
        xor  dx, dx             ; DX:AX = the remaining value
        div  bx                 ; AX = value/10, DX = value mod 10
        push dx                 ; save the digit
        inc  cx
        or   ax, ax             ; anything left?
        jnz  .divide

.output:
        pop  dx                 ; digits come back most significant first
        add  dl, '0'            ; binary -> ASCII
        mov  ah, 0x02           ; DOS: print the character in DL
        int  0x21
        loop .output

        mov  dl, 0x0D
        mov  ah, 0x02
        int  0x21
        mov  dl, 0x0A
        mov  ah, 0x02
        int  0x21

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
dividend: dw  1000
divisor:  dw  7
quot:     dw  0
rem:      dw  0
qmsg:     db  'Quotient:  $'
rmsg:     db  'Remainder: $'
zmsg:     db  'Division by zero!', 0x0D, 0x0A, '$'
```

**Output:**

```
Quotient:  142
Remainder: 6
```

Check: 1000 = 7 × 142 + 6. ✔

### 8.1 Notes on the code

**`xor dx, dx` before every `div`.** Inside `.divide`, `DX` holds the previous remainder. It must be
cleared before the next division, or the dividend is wrong. Forgetting this inside a loop is the
classic `print_dec` bug — and it usually produces a divide-overflow crash rather than a wrong
number, because a stale `DX` makes `DX:AX` enormous.

**The stack reverses the digits.** Dividing by 10 produces digits least-significant first, but we
need to print most-significant first. Pushing them and popping them back reverses the order for
free. This is the standard 8086 decimal-output idiom and you will use it in half the programs in
Part IV.

**`CX` counts the digits** so `LOOP` knows how many to pop. `CX` cannot be used for anything else
between the two loops.

**The zero guard.** `or bx, bx` / `jz` costs 5 clocks and prevents a crash. Always include it when
the divisor comes from outside the program.

---

## 9. Summary

```
  MUL  src    8-bit : AX    <- AL × src        70-77 clocks
              16-bit: DX:AX <- AX × src       118-133 clocks
  IMUL src    same shapes, signed              80-98 / 128-154
     CF = OF = 0  means the upper half is redundant (zero, or sign extension)
     SF ZF AF PF are UNDEFINED

  DIV  src    8-bit : AL <- AX / src,    AH <- AX mod src        80-90
              16-bit: AX <- DX:AX / src, DX <- DX:AX mod src    144-162
  IDIV src    same shapes, signed                               101-112 / 165-184
     ALL FLAGS UNDEFINED
     quotient too big, or divisor zero  ->  INT 0, not a flag

  the dividend is ALWAYS twice the divisor's width — set up AH or DX first
     unsigned: mov ah,0  /  xor dx,dx
     signed:   CBW       /  CWD

  MUL destroys DX. DIV destroys DX. Save it if you need it.
  no immediate operand: mul 10 does not exist

  IDIV truncates toward zero; the remainder takes the dividend's sign
  SAR rounds toward −infinity, so SAR ≠ IDIV for negative odd numbers

  × by a power of two: use SHL — 2 clocks each instead of 118+
```

---

## Exercises

**23.1** After `mov al, 25` / `mov bl, 10` / `mul bl`, what is in `AX`? What are `CF` and `OF`, and
what do they tell you?

**23.2** After `mov ax, 0x0200` / `mov bx, 0x0300` / `mul bx`, what is in `DX` and `AX`?

**23.3** Why does `mul bx` destroy `DX`? Write the two instructions that protect a value in `DX`
across a `MUL`.

**23.4** `mov ax, 1000` / `mov bl, 2` / `div bl` crashes. Explain precisely why, and give two
different fixes.

**23.5** Write the four instructions that divide the unsigned 16-bit value in `AX` by 7, leaving the
quotient in `AX` and the remainder in `DX`.

**23.6** Write the four instructions that divide the *signed* 16-bit value in `AX` by 7. What is
different, and why?

**23.7** `AL = 0xF0`. What is `AX` after `CBW`? What would it be after `mov ah, 0`? When is each
correct?

**23.8** Compute `−7 / 2` and `−7 mod 2` as `IDIV` would. Now compute what `SAR AX, 1` gives for
`AX = −7`. Explain the difference.

**23.9** Write a sequence that multiplies `AX` by 10 using only shifts and adds. Count the clocks
and compare with `MUL`.

**23.10** Write a sequence that multiplies `AX` by 7 using shifts and subtraction.

**23.11** In the `print_dec` routine of §8, what goes wrong if you remove the `xor dx, dx` from
inside the `.divide` loop? Predict the symptom before testing.

**23.12** Why does `print_dec` push the digits and pop them rather than printing them as it finds
them?

**23.13** Write a routine that divides a 32-bit value in `DX:AX` by a 16-bit divisor and handles the
case where the quotient would exceed 16 bits, without crashing.

**23.14** After `DIV`, all flags are undefined. Write the two instructions that correctly test
whether the division was exact.

Answers in [Appendix H](H-exercise-solutions.md#chapter-23).

---

[← Arithmetic](22-arithmetic.md) · [Contents](README.md) · [Next: BCD and ASCII adjust →](24-bcd-ascii-adjust.md)
