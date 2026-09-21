# Chapter 24 — BCD and ASCII adjust

[← Multiply and divide](23-multiply-divide.md) · [Contents](README.md) · [Next: Logical instructions →](25-logical.md)

---

## Goal

The six instructions most textbooks list and then abandon: `DAA`, `DAS`, `AAA`, `AAS`, `AAM`, `AAD`.
Each is explained here with its exact algorithm, a trace of every branch, and a complete working
program that uses it.

They exist for one reason — decimal arithmetic — and once you see the problem they solve, all six
become obvious.

---

## 1. The problem

Chapter 2 §8 introduced BCD. Recap:

**Packed BCD** stores two decimal digits in a byte, one per nibble:

```
   decimal 59  ->  0101 1001  =  0x59
```

The hex *looks* like the decimal. That is the point.

**The 8086's adder does not know this.** It carries at 16, not at 10:

```
        0x28      (BCD 28)
      + 0x14      (BCD 14)
      ------
        0x3C      but BCD 28 + 14 = 42, and 0x3C is not even valid BCD
```

The result is wrong by exactly **6**, because the adder allowed the low nibble to go to 12 instead of
carrying at 10:

```
        0x3C + 0x06 = 0x42      ✔
```

**Six is the magic number, always**, because 16 − 10 = 6. Every one of these six instructions is
built around adding or subtracting 6.

### 1.1 Why bother

In 1980, financial and instrument software worked in decimal because:

- **Exact decimal fractions.** 0.1 has no exact binary representation; in BCD it is a digit.
- **No conversion cost.** A cash register displays the number it stores. Converting binary to
  decimal costs a `DIV` per digit — 90 clocks each.
- **Arbitrary precision is easy.** A 20-digit number is 10 bytes, and adding two of them is a loop.

COBOL, RPG and every accounting package used packed decimal. The 8086 inherited the instructions
from the 8080, which had them for the same reason.

---

## 2. `DAA` — Decimal Adjust after Addition

```asm
        daa                     ; 27 — one byte, 4 clocks. Operates on AL only.
```

### 2.1 The algorithm

```
   if (AL AND 0x0F) > 9  OR  AF = 1 then
       AL  = AL + 6
       AF  = 1
       CF  = CF OR (carry produced by that addition)

   if AL > 0x9F  OR  CF = 1 then
       AL  = AL + 0x60
       CF  = 1
```

In words: **fix the low digit, then fix the high digit.**

The two conditions in each test do different jobs:

- `(AL AND 0x0F) > 9` catches a nibble that came out as A–F.
- `AF = 1` catches a nibble that *carried* — e.g. `9 + 8 = 17`, which leaves `1` in the nibble and
  sets `AF`. The nibble looks legal, but it is wrong.

You need both. Missing the `AF` test is the classic error in a hand-written decimal adjust.

### 2.2 Flags

`CF`, `AF`, `SF`, `ZF` and `PF` are set. `OF` is **undefined**.

**`CF` after `DAA` is the decimal carry** — the tens-carry out of the two-digit result. That is what
makes multi-digit BCD addition work (§6).

### 2.3 Traced examples

**Case 1 — no adjustment needed.**

```asm
        mov  al, 0x25           ; BCD 25
        add  al, 0x13           ; BCD 13
                                ; AL = 0x38, AF = 0, CF = 0
        daa                     ; low nibble 8 <= 9 and AF = 0  -> no change
                                ; AL = 0x38 <= 0x9F and CF = 0  -> no change
                                ; AL = 0x38 = BCD 38            ✔ 25+13 = 38
```

**Case 2 — low nibble too big.**

```asm
        mov  al, 0x28
        add  al, 0x14           ; AL = 0x3C, AF = 0 (C + 4... let's check:
                                ;   8 + 4 = 12 = 0xC, no carry out of bit 3, so AF = 0)
        daa                     ; (AL AND 0x0F) = 0xC > 9  -> AL = 0x3C + 6 = 0x42, AF = 1
                                ; 0x42 <= 0x9F, CF = 0     -> no high adjustment
                                ; AL = 0x42 = BCD 42        ✔ 28+14 = 42
```

**Case 3 — low nibble carried (this is why the `AF` test exists).**

```asm
        mov  al, 0x19
        add  al, 0x08           ; 9 + 8 = 17 -> nibble 1, carry out of bit 3
                                ; AL = 0x21, AF = 1, CF = 0
        daa                     ; (AL AND 0x0F) = 1, which is NOT > 9
                                ;   but AF = 1  -> AL = 0x21 + 6 = 0x27, AF = 1
                                ; 0x27 <= 0x9F -> no high adjustment
                                ; AL = 0x27 = BCD 27        ✔ 19+8 = 27
```

Without the `AF` test this would have left `0x21`, which is wrong by 6 and *looks* like valid BCD.

**Case 4 — high nibble too big, and a decimal carry.**

```asm
        mov  al, 0x88
        add  al, 0x79           ; AL = 0x101 truncated to 0x01, CF = 1, AF = 1
                                ;   (8+9 = 17 -> nibble 1 carry; 8+7+1 = 16 -> 0, CF = 1)
        daa                     ; low: (AL AND 0x0F) = 1, but AF = 1 -> AL = 0x01+6 = 0x07
                                ; high: CF = 1 -> AL = 0x07 + 0x60 = 0x67, CF = 1
                                ; AL = 0x67, CF = 1  =  BCD 167       ✔ 88+79 = 167
```

`CF = 1` carries the hundreds digit into the next byte. That is how you chain.

**Case 5 — high nibble in A–F.**

```asm
        mov  al, 0x54
        add  al, 0x55           ; AL = 0xA9, AF = 0, CF = 0
        daa                     ; low:  9 <= 9, AF = 0 -> no change
                                ; high: 0xA9 > 0x9F    -> AL = 0xA9 + 0x60 = 0x09, CF = 1
                                ; AL = 0x09, CF = 1  =  BCD 109       ✔ 54+55 = 109
```

### 2.4 The precise definition

Intel's exact pseudocode tests `old_AL > 0x99` (the value *before* the low adjustment) rather than
`AL > 0x9F` (after). The two agree in every case that arises from adding two valid BCD bytes, which
is the only situation `DAA` is defined for. If you feed `DAA` non-BCD input the two differ, and the
result is meaningless either way.

---

## 3. `DAS` — Decimal Adjust after Subtraction

```asm
        das                     ; 2F — one byte, 4 clocks
```

### 3.1 The algorithm

The mirror of `DAA`, subtracting 6 instead of adding it:

```
   if (AL AND 0x0F) > 9  OR  AF = 1 then
       AL = AL − 6
       AF = 1
       CF = CF OR (borrow produced by that subtraction)

   if AL > 0x9F  OR  CF = 1 then
       AL = AL − 0x60
       CF = 1
```

### 3.2 Traced examples

**Case 1 — a borrow from the low digit.**

```asm
        mov  al, 0x52           ; BCD 52
        sub  al, 0x25           ; BCD 25
                                ; 2 − 5 borrows: AL = 0x2D, AF = 1, CF = 0
        das                     ; (AL AND 0x0F) = 0xD > 9 -> AL = 0x2D − 6 = 0x27, AF = 1
                                ; 0x27 <= 0x9F, CF = 0    -> no high adjustment
                                ; AL = 0x27 = BCD 27       ✔ 52 − 25 = 27
```

**Case 2 — the result goes negative.**

```asm
        mov  al, 0x25
        sub  al, 0x52           ; AL = 0xD3, CF = 1 (borrow), AF = 0
        das                     ; low:  3 <= 9, AF = 0  -> no change
                                ; high: CF = 1          -> AL = 0xD3 − 0x60 = 0x73, CF = 1
                                ; AL = 0x73, CF = 1
```

`CF = 1` means "borrow out". The result `0x73` is the ten's complement: 100 − 27 = 73. In a
multi-byte BCD subtraction the borrow propagates and the answer comes out right.

---

## 4. `AAA` — ASCII Adjust after Addition

For **unpacked** BCD: one digit per byte, in the low nibble.

```asm
        aaa                     ; 37 — one byte, 8 clocks. Uses AL AND AH.
```

### 4.1 The algorithm

```
   if (AL AND 0x0F) > 9  OR  AF = 1 then
       AL = AL + 6
       AH = AH + 1
       AF = 1
       CF = 1
   else
       AF = 0
       CF = 0

   AL = AL AND 0x0F            ; always — the high nibble is cleared
```

Two differences from `DAA`:

1. **The carry goes into `AH`**, not into `CF` alone. So `AX` holds a two-digit unpacked result.
2. **`AL`'s high nibble is always cleared**, which is what makes this work directly on ASCII digits.

### 4.2 Why it works on ASCII

`'7'` is `0x37`. Its low nibble is 7 — the value. So:

```asm
        mov  al, '7'            ; 0x37
        add  al, '5'            ; 0x35  ->  AL = 0x6C
        aaa                     ; (AL AND 0x0F) = 0xC > 9
                                ;   AL = 0x6C + 6 = 0x72
                                ;   AH = AH + 1
                                ;   AL = 0x72 AND 0x0F = 0x02
                                ; AX = 0x0102, CF = 1   ->  the digits 1 and 2
                                ; 7 + 5 = 12            ✔
```

The garbage in the high nibbles (`0x30 + 0x30 = 0x60`) is simply discarded by the final `AND`. That
is the entire trick, and it is why the instruction is called **ASCII** adjust.

To print the result, add `0x30` back to each digit:

```asm
        add  ax, 0x3030         ; AX = 0x3132 = '1','2'
```

### 4.3 Set `AH` to zero first

`AAA` *increments* `AH`; it does not set it. So clear it before you start:

```asm
        mov  ah, 0
        mov  al, '9'
        add  al, '9'
        aaa                     ; AX = 0x0108 -> digits 1 and 8.  9+9 = 18 ✔
```

---

## 5. `AAS` — ASCII Adjust after Subtraction

```asm
        aas                     ; 3F — one byte, 8 clocks
```

```
   if (AL AND 0x0F) > 9  OR  AF = 1 then
       AL = AL − 6
       AH = AH − 1
       AF = 1
       CF = 1
   else
       AF = 0
       CF = 0

   AL = AL AND 0x0F
```

```asm
        mov  ah, 0
        mov  al, '3'            ; 0x33
        sub  al, '8'            ; 0x38  ->  AL = 0xFB, AF = 1, CF = 1
        aas                     ;   AL = 0xFB − 6 = 0xF5
                                ;   AH = 0 − 1 = 0xFF
                                ;   AL = 0xF5 AND 0x0F = 0x05
                                ; AX = 0xFF05, CF = 1
                                ; meaning: −5, as a borrow and the digit 5   ✔ 3 − 8 = −5
```

---

## 6. `AAM` — ASCII Adjust after Multiplication

```asm
        aam                     ; D4 0A — TWO bytes, 83 clocks
```

```
   AH = AL / 10
   AL = AL MOD 10
```

It splits a binary value 0–99 in `AL` into two unpacked decimal digits in `AH` and `AL`.

```asm
        mov  al, 7
        mov  bl, 9
        mul  bl                 ; AX = 63 = 0x003F  (binary!)
        aam                     ; AH = 63/10 = 6, AL = 63 mod 10 = 3
                                ; AX = 0x0603
        add  ax, 0x3030         ; AX = 0x3633 = '6','3'
```

`SF`, `ZF` and `PF` are set from `AL`; `OF`, `AF` and `CF` are undefined.

### 6.1 The hidden second byte

`AAM` assembles to **`D4 0A`** — and that `0A` is the *divisor*, not part of the opcode. The
processor divides by whatever byte follows.

This is undocumented in the original manuals but real on every x86 chip since. NASM lets you use it:

```asm
        aam  16                 ; D4 10 — splits AL into two HEX digits
```

```asm
        mov  al, 0xAB
        aam  16                 ; AH = 0xAB / 16 = 0x0A, AL = 0xAB mod 16 = 0x0B
                                ; AX = 0x0A0B — the two hex nibbles, separated
```

A one-instruction nibble split. Handy, though at 83 clocks it is slower than shifting and masking.

**`AAM 0` generates `INT 0`**, because it is a division by zero.

### 6.2 Why it is called "after multiplication"

Because the intended sequence is: multiply two unpacked digits (giving a binary product 0–81), then
`AAM` to turn it back into two unpacked digits. The name describes the use, not the operation.

---

## 7. `AAD` — ASCII Adjust before Division

```asm
        aad                     ; D5 0A — two bytes, 60 clocks
```

```
   AL = (AH × 10) + AL
   AH = 0
```

The *inverse* of `AAM`: it packs two unpacked digits into a single binary value, ready to be divided.

```asm
        mov  ax, 0x0705         ; unpacked digits 7 and 5, i.e. 75
        aad                     ; AL = 7×10 + 5 = 75 = 0x4B, AH = 0
        mov  bl, 9
        div  bl                 ; AL = 75/9 = 8, AH = 75 mod 9 = 3
```

**Note the name.** `AAD` runs **before** the division, unlike the other five, which run after their
operation. It is the only one, and it is a frequent exam question.

Like `AAM`, the second byte is the radix:

```asm
        aad  16                 ; D5 10 — AL = AH×16 + AL: packs two hex nibbles into a byte
```

`SF`, `ZF`, `PF` set from `AL`; `OF`, `AF`, `CF` undefined.

---

## 8. Worked program — multi-byte packed BCD addition

Add two six-digit packed BCD numbers (three bytes each) and print the result.

```asm
; bcdadd.asm — add two 6-digit packed BCD numbers
; nasm -f bin bcdadd.asm -o bcdadd.com
;
; Numbers are stored LEAST significant byte first, so we can walk
; upwards with the carry propagating naturally.
;
;   SI -> current byte of num1
;   DI -> current byte of num2
;   BX -> current byte of result
;   CX -> byte count
        org  0x100

start:
        mov  si, num1
        mov  di, num2
        mov  bx, result
        mov  cx, 3              ; three bytes = six digits
        clc                     ; no carry into the first byte

.next:
        mov  al, [si]           ; a digit pair from num1
        adc  al, [di]           ; + the pair from num2 + the carry from last time
        daa                     ; make it decimal again; CF = the decimal carry
        mov  [bx], al           ; MOV does not disturb CF

        inc  si                 ; INC does not disturb CF either
        inc  di
        inc  bx
        loop .next              ; LOOP does not disturb CF

        ; --- print the six digits, most significant byte first ---
        mov  si, result + 2
        mov  cx, 3
.show:
        mov  al, [si]
        call print_bcd_byte
        dec  si
        loop .show

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; print_bcd_byte — print the two BCD digits in AL.
; ---------------------------------------------------------------
print_bcd_byte:
        push ax
        push cx
        push dx

        mov  dl, al
        mov  cl, 4
        shr  dl, cl             ; the high nibble
        add  dl, '0'
        mov  ah, 0x02
        int  0x21

        pop  dx
        push dx
        mov  dl, al
        and  dl, 0x0F           ; the low nibble
        add  dl, '0'
        mov  ah, 0x02
        int  0x21

        pop  dx
        pop  cx
        pop  ax
        ret

; ---------------------------------------------------------------
; 123456 + 876544 = 999 - wait: 123456 + 876544 = 1,000,000.
; We store only six digits, so the result is 000000 with a final
; carry - which is exactly what the program will show.
num1:   db   0x56, 0x34, 0x12   ; 123456, least significant byte first
num2:   db   0x44, 0x65, 0x87   ; 876544
result: db   0, 0, 0
crlf:   db   0x0D, 0x0A, '$'
```

**Output:** `000000` — and `CF = 1` on exit from the loop, representing the millions digit that does
not fit. Change `num2` to `db 0x21, 0x43, 0x65` (654321) and the output becomes `777777`.

### 8.1 Why this loop works

Exactly the same reason as Chapter 22 §8.3: **`MOV`, `INC` and `LOOP` all preserve `CF`**, so the
decimal carry produced by `DAA` survives until the next `ADC`. Replace `inc si` with `add si, 1` and
the program breaks.

The pairing is:

```
   ADC  -> binary sum plus the previous carry
   DAA  -> corrects it to decimal and produces the DECIMAL carry in CF
```

`ADC` alone would give a binary answer. `DAA` alone could not chain across bytes. Together they give
arbitrary-precision decimal arithmetic in three instructions per byte.

---

## 9. Worked program — unpacked digit addition with `AAA`

```asm
; aaademo.asm — add two ASCII digits and print the two-digit result
; nasm -f bin aaademo.asm -o aaademo.com
        org  0x100

start:
        mov  ah, 0              ; AAA increments AH, so start it at zero
        mov  al, [d1]           ; '8' = 0x38
        add  al, [d2]           ; '7' = 0x37 -> AL = 0x6F
        aaa                     ; (0x6F AND 0x0F) = 0xF > 9
                                ;   AL = 0x6F + 6 = 0x75
                                ;   AH = 1
                                ;   AL = 0x75 AND 0x0F = 5
                                ; AX = 0x0105, CF = 1
        add  ax, 0x3030         ; AX = 0x3135 = '1','5'

        mov  [out_hi], ah
        mov  [out_lo], al

        mov  dx, outmsg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

d1:     db   '8'
d2:     db   '7'
outmsg: db   '8 + 7 = '
out_hi: db   '0'
out_lo: db   '0'
        db   0x0D, 0x0A, '$'
```

**Output:** `8 + 7 = 15`

---

## 10. When to use which

| Situation | Instruction |
|-----------|-------------|
| Added two **packed** BCD bytes | `DAA` |
| Subtracted two **packed** BCD bytes | `DAS` |
| Added two **unpacked** or ASCII digits | `AAA` |
| Subtracted two **unpacked** or ASCII digits | `AAS` |
| Multiplied two unpacked digits, want digits back | `AAM` |
| About to divide an unpacked two-digit value | `AAD` (**before** the `DIV`) |
| Want to split a byte into two hex nibbles | `AAM 16` |

### 10.1 Are they worth using today

For a *program you write in this book*: rarely. Binary arithmetic plus the `print_dec` routine of
Chapter 23 §8 is simpler and usually faster.

For **understanding the instruction set**, and for exam questions: essential. They are also the
clearest illustration in the whole 8086 of why `AF` exists — no other instruction reads it, and no
conditional jump tests it.

And there is one genuinely good modern use: `AAM 16` splits a byte into nibbles in one instruction,
which is neat in a hex-dump routine.

---

## 11. Summary

```
  the magic number is 6, because 16 − 10 = 6

  DAA   27      4 clk   packed BCD after ADD/ADC.  Fix low nibble, then high.
                        CF on exit = the decimal carry -> chain with ADC.
  DAS   2F      4 clk   packed BCD after SUB/SBB.  Subtract 6 / 60h.
  AAA   37      8 clk   unpacked after ADD. Carry goes into AH. AL masked to 0Fh.
  AAS   3F      8 clk   unpacked after SUB. Borrow taken from AH. AL masked.
  AAM   D4 0A  83 clk   AH = AL/10, AL = AL mod 10.  Second byte is the radix!
  AAD   D5 0A  60 clk   AL = AH*10 + AL, AH = 0.  Runs BEFORE the DIV.

  DAA/DAS work on AL only.  AAA/AAS/AAM/AAD use AH as well.
  All six read or write AF — the only instructions that do.
  OF is undefined after every one of them.

  multi-byte packed BCD:   CLC ; loop { ADC ; DAA ; store ; INC pointers } 
                           because MOV, INC and LOOP all preserve CF
```

---

## Exercises

**24.1** Why is 6 the adjustment constant, and not some other number?

**24.2** Trace `mov al, 0x37` / `add al, 0x48` / `daa`. Give `AL`, `CF` and `AF` at each step, and
state the decimal sum.

**24.3** Trace `mov al, 0x99` / `add al, 0x99` / `daa`. What is `AL` and what is `CF`, and what
decimal value does the pair represent?

**24.4** Give a case where the `(AL AND 0x0F) > 9` test alone would fail and the `AF = 1` test saves
it. Show the numbers.

**24.5** Trace `mov al, 0x40` / `sub al, 0x15` / `das`.

**24.6** `AH = 0`, `AL = '6'`. Trace `add al, '7'` / `aaa`. Give `AX` and `CF`, and then the
instruction that turns `AX` into two printable ASCII digits.

**24.7** Why does `AAA` clear the high nibble of `AL`? What would go wrong if it did not?

**24.8** Why must `AH` be zeroed before a sequence of `AAA`s?

**24.9** `AL = 0x4F` (79 decimal). What is `AX` after `AAM`? After `AAM 16`?

**24.10** What does `AAD` do, and what is unusual about *when* it runs compared with the other five?

**24.11** `AX = 0x0904`. What is in `AL` after `AAD`? Now divide by 5 — what are the quotient and
remainder?

**24.12** In the program of §8, explain precisely why replacing `inc si` with `add si, 1` breaks it.

**24.13** Write a routine that subtracts two 8-digit packed BCD numbers (four bytes each).

**24.14** Write a one-instruction way to split the byte in `AL` into its two hex nibbles, and say how
many clocks it takes compared with the shift-and-mask version.

Answers in [Appendix H](H-exercise-solutions.md#chapter-24).

---

[← Multiply and divide](23-multiply-divide.md) · [Contents](README.md) · [Next: Logical instructions →](25-logical.md)
