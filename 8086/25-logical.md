# Chapter 25 — Logical instructions

[← BCD and ASCII adjust](24-bcd-ascii-adjust.md) · [Contents](README.md) · [Next: Shifts and rotates →](26-shift-rotate.md)

---

## Goal

Five instructions — `AND`, `OR`, `XOR`, `NOT`, `TEST` — and the bit-manipulation vocabulary built on
them. These are how you set, clear, flip and test individual bits, which is most of what hardware
programming consists of.

One fact governs the whole chapter: **`AND`, `OR`, `XOR` and `TEST` always clear `CF` and `OF`.**
Unconditionally, whatever the operands. `NOT` affects no flags at all.

---

## 1. The four operations, bit by bit

Each works on corresponding bit pairs, independently, with no carries between positions.

```
   AND                OR                 XOR                NOT
   0 0 -> 0           0 0 -> 0           0 0 -> 0           0 -> 1
   0 1 -> 0           0 1 -> 1           0 1 -> 1           1 -> 0
   1 0 -> 0           1 0 -> 1           1 0 -> 1
   1 1 -> 1           1 1 -> 1           1 1 -> 0
```

Now read each column as a *tool*:

| Operation | With a 1 in the mask | With a 0 in the mask | Therefore it is a tool for |
|-----------|---------------------|---------------------|---------------------------|
| `AND` | leaves the bit alone | **forces the bit to 0** | **clearing** bits |
| `OR` | **forces the bit to 1** | leaves the bit alone | **setting** bits |
| `XOR` | **flips** the bit | leaves the bit alone | **toggling** bits |

That three-row table is the whole of bit manipulation. Everything below is an application of it.

---

## 2. `AND`

```asm
        and  destination, source
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| reg, reg | `20`/`21`/`22`/`23` | 2 | 3 |
| reg, mem | `22`/`23` | 2–4 | 9 + EA |
| mem, reg | `20`/`21` | 2–4 | 16 + EA |
| reg/mem, imm | `80`/`81 /4` | 3–6 | 4 / 17 + EA |
| **acc, imm** | `24`/`25` | 2–3 | 4 |

### 2.1 Masking — keeping some bits, zeroing the rest

```asm
        and  al, 0x0F           ; keep the low nibble, clear the high one
        and  al, 0x80           ; keep only bit 7
        and  ax, 0xFF00         ; keep the high byte
        and  al, 0xFE           ; clear bit 0 — force the value even
```

Worked:

```
        AL = 1011 0110   (0xB6)
      AND  0000 1111   (0x0F)
      -----------------
             0000 0110   (0x06)
```

### 2.2 Testing a bit

```asm
        and  al, 0x08           ; isolate bit 3
        jnz  bit3_was_set       ; ZF = 0 if it was 1
```

But this **destroys `AL`**. Use `TEST` instead (§6) when you only want the flag.

### 2.3 Flags

`ZF`, `SF` and `PF` from the result. **`CF = 0` and `OF = 0`, always.** `AF` undefined.

The forced `CF = 0` is the trap: it silently breaks a multi-precision carry chain.

```asm
        add  ax, cx             ; CF = carry out
        and  bx, 0x00FF         ; ✘ CF is now 0 — the chain is broken
        adc  dx, si             ; wrong
```

---

## 3. `OR`

```asm
        or   destination, source
```

Opcodes `08`–`0D`, group extension `/1`. Same forms and clocks as `AND`.

### 3.1 Setting bits

```asm
        or   al, 0x80           ; force bit 7 to 1
        or   al, 0x03           ; force bits 0 and 1 to 1
        or   al, 0x20           ; force bit 5 — converts a letter to lower case
```

```
        AL = 1011 0110
       OR    0000 1001
       -----------------
             1011 1111
```

### 3.2 Combining fields

Building a control word from parts:

```asm
; 8255 control word: mode-set flag + port directions
        mov  al, 0x80           ; bit 7: mode-set
        or   al, 0x02           ; bit 1: port B is input
        or   al, 0x10           ; bit 4: port A is input
        out  0x06, al           ; AL = 0x92
```

Chapter 47 does exactly this.

### 3.3 `OR reg, reg` as a zero test

```asm
        or   ax, ax             ; AX unchanged (x OR x = x), but ZF and SF are set
        jz   is_zero
```

Two bytes, 3 clocks — shorter and faster than `cmp ax, 0`. It was the universal idiom. Its one cost
is that it clears `CF`.

---

## 4. `XOR`

```asm
        xor  destination, source
```

Opcodes `30`–`35`, group extension `/6`.

### 4.1 Toggling bits

```asm
        xor  al, 0xFF           ; flip every bit — same as NOT, but sets flags
        xor  al, 0x01           ; flip bit 0
        xor  al, 0x20           ; flip the case of an ASCII letter
```

### 4.2 Clearing a register

```asm
        xor  ax, ax             ; AX = 0.  2 bytes, 3 clocks
        mov  ax, 0              ; AX = 0.  3 bytes, 4 clocks
        sub  ax, ax             ; AX = 0.  2 bytes, 3 clocks
```

`x XOR x = 0` for every bit, so `XOR AX, AX` zeroes the register. It is the shortest way, and it
also sets `ZF = 1`, `SF = 0`, `CF = 0`, `OF = 0` — sometimes useful, sometimes not.

You will see `XOR AX, AX` everywhere in 8086 code. Now you know why.

### 4.3 The swap trick

```asm
        xor  ax, bx
        xor  bx, ax
        xor  ax, bx             ; AX and BX are now swapped, with no temporary
```

Trace with `AX = a`, `BX = b`:

```
   after 1:  AX = a^b,  BX = b
   after 2:  AX = a^b,  BX = b^(a^b) = a
   after 3:  AX = (a^b)^a = b,  BX = a
```

Six bytes, 9 clocks. **`XCHG AX, BX` is one byte and 3 clocks.** The trick is a curiosity on the
8086, not an optimisation — it matters on architectures without an exchange instruction.

### 4.4 Comparing for equality

`a XOR b = 0` if and only if `a == b`:

```asm
        mov  ax, [val1]
        xor  ax, [val2]
        jz   they_are_equal
```

`CMP` is usually clearer. But `XOR` leaves the *differing bits* in `AX`, which is useful when you
want to know *which* bits differ:

```asm
        mov  ax, [old_state]
        xor  ax, [new_state]    ; AX = the bits that changed
```

That idiom is the basis of change detection in an input loop.

---

## 5. `NOT`

```asm
        not  destination        ; one's complement: invert every bit
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `NOT r8`/`r16` | `F6`/`F7 /2` | 2 | 3 |
| `NOT mem` | `F6`/`F7 /2` | 2–4 | 16 + EA |

**`NOT` affects no flags whatsoever.** It is the only logical instruction that does not. If you need
flags afterwards:

```asm
        not  al
        or   al, al             ; now ZF and SF are set
```

### 5.1 `NOT` versus `NEG`

```asm
        mov  al, 5              ; 0000 0101
        not  al                 ; 1111 1010 = 0xFA — one's complement, no flags
        mov  al, 5
        neg  al                 ; 1111 1011 = 0xFB — two's complement (= −5), flags set
```

`NEG` = `NOT` + 1, and `NEG` sets flags. Use `NEG` for arithmetic negation and `NOT` for bitwise
inversion.

### 5.2 Building a mask

```asm
        mov  ax, 0x00FF
        not  ax                 ; AX = 0xFF00
```

Often clearer than writing the complement literal, especially when the mask came from a computation.

---

## 6. `TEST`

```asm
        test operand1, operand2         ; AND them, set flags, DISCARD the result
```

Opcodes `84`, `85` (reg/mem), `A8`, `A9` (accumulator + immediate), `F6`/`F7 /0` (r/m + immediate).

**`TEST` is to `AND` what `CMP` is to `SUB`.** Same flags, no result stored.

| Form | Bytes | Clocks |
|------|-------|--------|
| reg, reg | 2 | 3 |
| reg, mem | 2–4 | 9 + EA |
| reg/mem, imm | 3–6 | 5 / 11 + EA |
| acc, imm | 2–3 | 4 |

### 6.1 Testing a single bit

```asm
        test al, 0x08           ; is bit 3 set?
        jnz  bit3_set           ; ZF = 0 -> the bit was 1
        jz   bit3_clear         ; ZF = 1 -> the bit was 0
```

`AL` is unchanged. This is the correct way to test a bit.

### 6.2 Testing several bits

```asm
        test al, 0x0C           ; bits 2 and 3
        jz   neither_set        ; ZF = 1 only if BOTH are 0
        jnz  at_least_one_set
```

Note what `TEST` can and cannot tell you: `ZF = 1` means "none of the masked bits are set". `ZF = 0`
means "at least one is set". It **cannot** distinguish "one" from "both". For that:

```asm
        mov  ah, al
        and  ah, 0x0C
        cmp  ah, 0x0C
        je   both_set
```

### 6.3 Testing the sign

```asm
        test ax, ax
        js   negative           ; SF = 1
        jz   zero
        jns  positive
```

Or, equivalently:

```asm
        test ah, 0x80           ; bit 15 of AX is bit 7 of AH
        jnz  negative
```

### 6.4 Testing for even/odd

```asm
        test al, 1
        jz   even
        jnz  odd
```

Cheaper than dividing by 2.

---

## 7. The idioms, collected

```asm
; --- set bit n ---
        or   al, (1 << n)               ; NASM computes the constant at assembly time
        or   al, 0x08                   ; set bit 3

; --- clear bit n ---
        and  al, ~(1 << n)              ; NASM's ~ is bitwise NOT, at assembly time
        and  al, 0xF7                   ; clear bit 3

; --- toggle bit n ---
        xor  al, (1 << n)
        xor  al, 0x08

; --- test bit n ---
        test al, (1 << n)
        jnz  bit_was_set

; --- clear a register ---
        xor  ax, ax

; --- test for zero ---
        or   ax, ax
        jz   zero

; --- keep the low nibble ---
        and  al, 0x0F

; --- keep the high nibble, moved down ---
        mov  cl, 4
        shr  al, cl

; --- force upper case (ASCII letters) ---
        and  al, 0xDF                   ; clear bit 5

; --- force lower case ---
        or   al, 0x20                   ; set bit 5

; --- toggle case ---
        xor  al, 0x20

; --- ASCII digit to value ---
        and  al, 0x0F                   ; works because '0'..'9' are 0x30..0x39

; --- round up to a multiple of 16 ---
        add  ax, 15
        and  ax, 0xFFF0

; --- is it a power of two? (result zero means yes, for non-zero input) ---
        mov  bx, ax
        dec  bx
        and  bx, ax                     ; ZF = 1 -> AX is a power of two

; --- which bits changed? ---
        mov  ax, [new]
        xor  ax, [old]                  ; 1 bits mark the changes
```

### 7.1 The case trick, explained

ASCII was laid out so that the only difference between `'A'` (`0x41`) and `'a'` (`0x61`) is bit 5:

```
   'A' = 0100 0001
   'a' = 0110 0001
              ^
              bit 5
```

That holds for every letter, so one `AND`, `OR` or `XOR` converts case with no comparison. **It only
works on letters** — applying `or al, 0x20` to a digit turns `'0'` (`0x30`) into `0x30` (unchanged,
since bit 5 is already set), which is harmless, but applying it to `'['` (`0x5B`) gives `'{'`
(`0x7B`), which is not. Guard it:

```asm
        cmp  al, 'A'
        jb   .not_letter
        cmp  al, 'Z'
        ja   .not_letter
        or   al, 0x20           ; safe: we know it is an upper-case letter
.not_letter:
```

---

## 8. Worked program — count the set bits in a word

```asm
; popcount.asm — count the 1 bits in a 16-bit value and print the count
; nasm -f bin popcount.asm -o popcount.com
;
;   BX -> the value being examined
;   CX -> loop counter (16 bits to check)
;   DL -> the running count
        org  0x100

start:
        mov  bx, [value]
        mov  cx, 16
        xor  dl, dl             ; DL = 0, the count

.next:
        test bx, 1              ; is the bottom bit set?
        jz   .skip              ;   no
        inc  dl                 ;   yes — count it
.skip:
        shr  bx, 1              ; move the next bit down
        loop .next

        ; --- print DL as two decimal digits ---
        mov  al, dl
        xor  ah, ah
        mov  bl, 10
        div  bl                 ; AL = tens, AH = units
        add  ax, 0x3030
        mov  [out_hi], al
        mov  [out_lo], ah

        mov  dx, msg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

value:  dw   0xB6D3             ; 1011 0110 1101 0011
msg:    db   'Bits set: '
out_hi: db   '0'
out_lo: db   '0'
        db   0x0D, 0x0A, '$'
```

Count by hand: `1011 0110 1101 0011` has 1s at bits 0,1,4,6,7,9,10,12,13,15 — **10 bits**.

**Output:** `Bits set: 10`

### 8.1 A faster version

The loop above always runs 16 times. This one runs once per *set* bit:

```asm
; Kernighan's method: x AND (x−1) clears the lowest set bit.
        mov  bx, [value]
        xor  dl, dl
.next:
        or   bx, bx             ; any bits left?
        jz   .done
        mov  ax, bx
        dec  ax
        and  bx, ax             ; clear the lowest set bit
        inc  dl
        jmp  .next
.done:
```

For `0xB6D3` that is 10 iterations instead of 16. For a sparse value it is dramatically better; for
`0xFFFF` it is worse. Which to use depends on your data — the sort of judgement Chapter 32 makes
quantitative.

---

## 9. Summary

```
  AND  clears bits      mask bit 0 -> result bit 0;  mask bit 1 -> unchanged
  OR   sets bits        mask bit 1 -> result bit 1;  mask bit 0 -> unchanged
  XOR  toggles bits     mask bit 1 -> flipped;       mask bit 0 -> unchanged
  NOT  inverts all bits — NO FLAGS AT ALL
  TEST ANDs and sets flags without storing — the AND analogue of CMP

  AND, OR, XOR, TEST all force CF = 0 and OF = 0, unconditionally.
     -> never use them inside a multi-precision carry chain
  ZF, SF, PF come from the result. AF is undefined.

  set bit n      or   reg, (1 << n)
  clear bit n    and  reg, ~(1 << n)
  toggle bit n   xor  reg, (1 << n)
  test bit n     test reg, (1 << n)  /  jnz
  clear register xor  reg, reg           2 bytes, 3 clocks
  test for zero  or   reg, reg  /  jz    2 bytes, 3 clocks

  ASCII case is bit 5:  AND 0xDF -> upper,  OR 0x20 -> lower,  XOR 0x20 -> toggle
```

---

## Exercises

**25.1** `AL = 0xA7`. Give `AL` after each of: `and al, 0x0F`, `or al, 0x0F`, `xor al, 0x0F`,
`not al`. (Start from `0xA7` each time.)

**25.2** Write one instruction that clears bits 2 and 5 of `AL`, leaving the rest alone.

**25.3** Write one instruction that sets bits 0, 1 and 7 of `BL`.

**25.4** Write one instruction that flips bit 4 of `DH`.

**25.5** Write the two instructions that jump to `found` if bit 6 of `AL` is set, without changing
`AL`.

**25.6** Why does `xor ax, ax` clear `AX`? Give its byte count and clock count, and compare with
`mov ax, 0`.

**25.7** Explain why `or ax, ax` sets `ZF` correctly without changing `AX`.

**25.8** A multi-precision addition loop contains `and si, 0xFFFE` between the `ADD` and the `ADC`.
What goes wrong, and why?

**25.9** `AL` holds an ASCII character. Write the code that converts it to upper case **only if it
is a lower-case letter**, leaving everything else unchanged.

**25.10** Write the three instructions that leave in `AX` a value whose set bits mark the positions
where `[old]` and `[new]` differ.

**25.11** Write a test that jumps to `both` only when *both* bits 2 and 3 of `AL` are set. Explain
why a single `TEST` cannot do it.

**25.12** Explain how `x AND (x − 1)` clears the lowest set bit. Trace it for `x = 0b0010_1100`.

**25.13** Write code that rounds the value in `AX` up to the next multiple of 256 using only `ADD`
and `AND`.

**25.14** `NOT` sets no flags. Give a two-instruction sequence that complements `AL` and leaves `ZF`
correctly reflecting the result.

Answers in [Appendix H](H-exercise-solutions.md#chapter-25).

---

[← BCD and ASCII adjust](24-bcd-ascii-adjust.md) · [Contents](README.md) · [Next: Shifts and rotates →](26-shift-rotate.md)
