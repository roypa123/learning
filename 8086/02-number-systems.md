# Chapter 2 — Number systems

[← Toolchain setup](01-toolchain-setup.md) · [Contents](README.md) · [Next: Digital logic recap →](03-digital-logic-recap.md)

---

## Goal

Build the arithmetic the 8086 actually performs. Not "binary is base two" — that takes a paragraph —
but the specific things that bite: why the carry flag and the overflow flag are different, why
`0xFF + 1 = 0x00` is sometimes correct and sometimes a bug, what the processor means by *signed*
when the bits are identical either way, and why a number stored in memory looks backwards.

If you already know two's complement cold, read §5 (carry vs overflow), §8 (BCD) and §9
(little-endian), and skip the rest.

---

## 1. Positional notation, stated once

A number written in base *b* with digits `d₃d₂d₁d₀` means

```
d₃·b³ + d₂·b² + d₁·b¹ + d₀·b⁰
```

That is the whole idea, and it is the same in every base. Decimal `3407` is
3·1000 + 4·100 + 0·10 + 7·1. Binary `1011` is 1·8 + 0·4 + 1·2 + 1·1 = 11. Hexadecimal `2F` is
2·16 + 15·1 = 47.

What changes between bases is only *how many symbols* there are:

| Base | Name | Digits |
|------|------|--------|
| 2 | binary | `0 1` |
| 8 | octal | `0`–`7` |
| 10 | decimal | `0`–`9` |
| 16 | hexadecimal | `0`–`9`, then `A`(10) `B`(11) `C`(12) `D`(13) `E`(14) `F`(15) |

The 8086 works in binary because a wire is either at a high voltage or a low one. We *write* in
hexadecimal because binary is unreadable at 16 digits and hex compresses it exactly four bits to the
digit, with no arithmetic needed.

---

## 2. The units

These names appear constantly, so fix them now.

| Term | Size | Range (unsigned) | 8086 significance |
|------|------|------------------|-------------------|
| **bit** | 1 | 0–1 | one wire, one flag |
| **nibble** | 4 bits | 0–15 | one hex digit; the unit BCD works in |
| **byte** | 8 bits | 0–255 | `AL`, `AH`, one memory location |
| **word** | 16 bits | 0–65,535 | `AX`, the 8086's natural size |
| **double word** | 32 bits | 0–4,294,967,295 | a far pointer (`seg:off`), a `MUL` result |
| **quad word** | 64 bits | 0–1.8×10¹⁹ | only built by software on the 8086 |

Two things follow from "the 8086's natural size is 16 bits":

- Every general register holds a word. `AX` is a word; `AH` and `AL` are its two halves.
- Every address is computed 16 bits at a time, which is why reaching 20 bits needs segmentation
  (Chapter 9).

Useful powers of two, worth memorising to 2¹⁶:

```
2⁰=1      2⁴=16     2⁸=256      2¹²=4096     2¹⁶=65,536
2¹=2      2⁵=32     2⁹=512      2¹³=8192     2²⁰=1,048,576  (1 MiB — the 8086's address space)
2²=4      2⁶=64     2¹⁰=1024    2¹⁴=16,384
2³=8      2⁷=128    2¹¹=2048    2¹⁵=32,768
```

---

## 3. Converting

### 3.1 Binary ↔ hexadecimal: free

Four binary digits are exactly one hex digit, always. Group from the *right*, pad the left with
zeros.

```
1 0 1 1 1 1 0 1
└──┬──┘ └──┬──┘
   B        D        ->  0xBD
```

```
0x7E3  ->  7    E    3
           0111 1110 0011   ->  0b011111100011
```

Learn this table and you will never do binary–hex arithmetic again:

| Hex | Binary | Hex | Binary |
|-----|--------|-----|--------|
| 0 | 0000 | 8 | 1000 |
| 1 | 0001 | 9 | 1001 |
| 2 | 0010 | A | 1010 |
| 3 | 0011 | B | 1011 |
| 4 | 0100 | C | 1100 |
| 5 | 0101 | D | 1101 |
| 6 | 0110 | E | 1110 |
| 7 | 0111 | F | 1111 |

### 3.2 Decimal → binary: repeated division by 2

Divide by 2, write the remainder, repeat on the quotient, then read the remainders **bottom to top**.

Convert 218:

```
218 ÷ 2 = 109  r 0   ← LSB
109 ÷ 2 =  54  r 1
 54 ÷ 2 =  27  r 0
 27 ÷ 2 =  13  r 1
 13 ÷ 2 =   6  r 1
  6 ÷ 2 =   3  r 0
  3 ÷ 2 =   1  r 1
  1 ÷ 2 =   0  r 1   ← MSB

reading upwards: 1101 1010  =  0xDA
```

Check: 128 + 64 + 16 + 8 + 2 = 218. ✔

### 3.3 Decimal → hexadecimal: repeated division by 16

Faster, because there are fewer steps. Convert 41,394:

```
41394 ÷ 16 = 2587  r  2   ← least significant hex digit
 2587 ÷ 16 =  161  r 11 = B
  161 ÷ 16 =   10  r  1
   10 ÷ 16 =    0  r 10 = A

reading upwards: 0xA1B2
```

Check: 10·4096 + 1·256 + 11·16 + 2 = 40960 + 256 + 176 + 2 = 41,394. ✔

### 3.4 Binary/hex → decimal: weighted sum, or Horner

Weighted sum for short values. For longer ones, Horner's method is less error-prone: start with the
leftmost digit, then repeatedly *multiply by the base and add the next digit*.

`0xA1B2`:

```
start          10
×16 + 1   =   161
×16 + 11  =  2587
×16 + 2   = 41394
```

### 3.5 Fractions (rarely needed, but asked)

Multiply the fraction by the base repeatedly and take the integer parts **top to bottom**.

0.6875 → binary:

```
0.6875 × 2 = 1.375   -> 1
0.375  × 2 = 0.75    -> 0
0.75   × 2 = 1.5     -> 1
0.5    × 2 = 1.0     -> 1
0.0                     stop

0.6875 = 0b0.1011
```

Most decimal fractions never terminate in binary — 0.1 does not — which is the root of floating-point
surprise. The 8086 core has no fractions at all; see Chapter 54 for the 8087.

---

## 4. Binary arithmetic

### 4.1 Addition

Four cases per column:

```
0+0 = 0        0+1 = 1        1+0 = 1        1+1 = 0 carry 1
```

and with a carry in, `1+1+1 = 1 carry 1`.

```
     1111          <- carries
     0110 1101     0x6D  = 109
   + 0011 1010     0x3A  =  58
   -----------
     1010 0111     0xA7  = 167   ✔
```

### 4.2 Subtraction by borrowing

```
0-0 = 0     1-0 = 1     1-1 = 0     0-1 = 1 borrow 1
```

```
     0100 0010     0x42 = 66
   - 0001 1001     0x19 = 25
   -----------
     0010 1001     0x29 = 41   ✔
```

The 8086 does not actually subtract this way internally — it adds the two's complement (§5.4) — but
the answer is identical and borrowing is easier to check by hand.

### 4.3 Hexadecimal arithmetic directly

You can add hex without converting, and you should learn to, because every dump you read is hex.
The only rule: a column carries when it reaches 16, not 10.

```
    1                 <- carry
    2 F 8
  + 1 9 C
  -------
    4 9 4
```

Column by column, right to left: 8+C = 8+12 = 20 = 16+4, so write 4 carry 1. F+9+1 = 15+9+1 = 25 =
16+9, write 9 carry 1. 2+1+1 = 4. Result `0x494`.

---

## 5. Signed numbers — and the two flags that describe them

This is the section that matters. The 8086 has *one* adder. It does not know or care whether you
think your bits are signed. It computes the same 16-bit sum either way, and sets **two different
flags**, one of which is meaningful if you meant unsigned and the other if you meant signed. Reading
the wrong one is the classic beginner bug.

### 5.1 The three historical schemes

| Scheme | 8-bit representation of −5 | Problem |
|--------|---------------------------|---------|
| Sign-magnitude | `1000 0101` | two zeros (`0000 0000`, `1000 0000`); adder needs special cases |
| One's complement | `1111 1010` | still two zeros; needs end-around carry |
| **Two's complement** | `1111 1011` | none worth mentioning — this is what everything uses |

### 5.2 Two's complement, defined

To negate an *n*-bit number: **invert every bit, then add 1.**

```
  +5   =  0000 0101
  invert  1111 1010
  add 1   1111 1011  =  −5   (= 0xFB)
```

Check by adding: `0000 0101 + 1111 1011 = 1 0000 0000`. The ninth bit falls off the end of an 8-bit
register, leaving `0000 0000` = 0. That is exactly what we want from *x* + (−*x*), and it is why the
scheme works: negation is defined so that addition needs no modification at all.

An equivalent definition, often faster by hand: **copy bits from the right up to and including the
first `1`, then invert everything above it.**

```
  +12  =  0000 1100
                 ^ first 1 from the right, at bit 2
  copy bits 2..0 unchanged: ...100
  invert bits 7..3:         11110
  -12  =  1111 0100   (= 0xF4)
```

### 5.3 Ranges, and the asymmetry

For *n* bits, two's complement covers −2ⁿ⁻¹ … +2ⁿ⁻¹−1.

| Width | Unsigned | Signed |
|-------|----------|--------|
| 8-bit | 0 … 255 | −128 … +127 |
| 16-bit | 0 … 65,535 | −32,768 … +32,767 |

There is one more negative value than positive, because zero occupies a slot on the positive side.
Consequence: **−128 has no 8-bit positive counterpart**, so `NEG` applied to `0x80` yields `0x80`
again and sets the overflow flag. Chapter 22 §7 covers this.

The top bit **is** the sign bit, in the sense that it is 1 for every negative value and 0 for every
non-negative one. But do not think of it as a separate sign *field*: `1111 1011` is not "minus
1111011"; it is the single number −5, and the top bit carries weight −128 in the positional sum:

```
1111 1011 = −128 + 64 + 32 + 16 + 8 + 0 + 2 + 1 = −5 ✔
```

That reading — **the most significant bit has negative weight** — is the cleanest mental model.

### 5.4 Subtraction is addition

The 8086 computes `A − B` as `A + (NOT B) + 1`. That is why:

- `SUB` and `CMP` set the flags identically (Chapter 22 §5);
- the carry flag after a subtraction means *borrow*, and is the **inverse** of the carry out of the
  adder;
- `SBB` (subtract with borrow) exists and is the mirror of `ADC`.

### 5.5 Carry versus overflow

**The carry flag `CF` is set when the result does not fit as an unsigned number.** Concretely, when
there is a carry out of (or, for subtraction, a borrow into) the most significant bit.

**The overflow flag `OF` is set when the result does not fit as a signed number.** Concretely, when
the carry *into* the sign bit differs from the carry *out of* it.

They are independent. All four combinations occur. Here are 8-bit examples of each — work through
every row, because this table is the whole of §5.

| # | Operation | Hex | Unsigned view | Signed view | CF | OF |
|---|-----------|-----|---------------|-------------|----|----|
| 1 | `0x3C + 0x05` | `0x41` | 60 + 5 = 65 ✔ | 60 + 5 = 65 ✔ | 0 | 0 |
| 2 | `0xFF + 0x02` | `0x01` | 255 + 2 = 257 ✘ (wrapped) | −1 + 2 = 1 ✔ | **1** | 0 |
| 3 | `0x7F + 0x01` | `0x80` | 127 + 1 = 128 ✔ | 127 + 1 = −128 ✘ | 0 | **1** |
| 4 | `0x80 + 0x80` | `0x00` | 128+128 = 256 ✘ | −128 + −128 = 0 ✘ | **1** | **1** |

Row 2 in detail. Bits: `1111 1111 + 0000 0010`. The carry out of bit 7 is 1, so `CF = 1` — correct,
because 257 does not fit in 8 bits. The carry *into* bit 7 is also 1, and carry-in equals carry-out,
so `OF = 0` — also correct, because as signed numbers −1 + 2 = +1, which fits perfectly and is the
answer sitting in the register.

Row 3 in detail. `0111 1111 + 0000 0001`. There is a carry into bit 7 (from the chain of 1s below)
but no carry out of it. They differ, so `OF = 1`: adding two positives produced a negative, which is
impossible and therefore an overflow. Unsigned, 128 is perfectly representable, so `CF = 0`.

**The rule you actually use when writing code:** after an operation on quantities you are treating as
unsigned, branch on `CF` (`JC`, `JNC`, `JA`, `JB`); after an operation on quantities you are treating
as signed, branch on `OF` and `SF` (`JO`, `JG`, `JL`). Chapter 27 §5 tabulates which conditional jump
belongs to which world, and getting that wrong is the commonest source of "my sort works except with
negative numbers".

A quicker signed test that avoids thinking about carries: **overflow on addition happens only when
the two operands have the same sign and the result has the opposite sign.** Overflow on subtraction
happens only when the operands have different signs and the result's sign differs from the first
operand's.

---

## 6. Sign extension

Copy an 8-bit signed value into 16 bits and you cannot just zero the top byte: `0xFB` (−5) would
become `0x00FB` (+251). You must replicate the sign bit:

```
  0x7F   +127  ->  0x007F
  0xFB     −5  ->  0xFFFB
```

The 8086 gives you one instruction for this, `CBW` (convert byte to word, `AL` → `AX`), and its
16→32 partner `CWD` (`AX` → `DX:AX`). Chapter 23 §2 explains why they exist: `IDIV` needs a
double-width dividend, and building it by hand is error-prone.

For *unsigned* widening, the operation is different — zero the top — and on the 8086 there is no
instruction for it because `mov ah, 0` does the job.

---

## 7. Storing numbers in memory: little-endian

A 16-bit value occupies two consecutive byte addresses. The 8086 stores the **low byte at the lower
address**.

```
mov word [0x0200], 0x1234

address:  0x0200  0x0201
content:    34      12
```

This is *little-endian*. The Motorola 68000 of the same era did the opposite (big-endian, `12 34`).
Neither is better; they differ, and network protocols standardised on big-endian, which is why you
meet byte-swapping.

Three practical consequences, all of which you will hit:

**Hex dumps read backwards.** In Chapter 1's dump, `ba 0d 01` is `mov dx, 0x010D` — the operand
bytes are `0D 01`, low first. When you see a 16-bit operand or a stored word in a dump, mentally
reverse the pair.

**Byte access to a word is natural for the low half.** `mov al, [num]` loads the low byte of the
word at `num`, and `mov al, [num+1]` loads the high byte. This is genuinely useful: `AL`/`AH` map
onto `[num]`/`[num+1]` in the same order.

**A far pointer is offset-then-segment.** `LES BX, [ptr]` loads `BX` from `[ptr]` and `ES` from
`[ptr+2]`. The offset comes first because it is the low half of the 32-bit quantity. Chapter 21 §8.

---

## 8. Binary-coded decimal

Some applications — a digital clock, a cash register, an instrument display — must be decimal, and
converting binary to decimal on every update is expensive on a 5 MHz processor. BCD stores decimal
digits directly, one per nibble.

### 8.1 Packed and unpacked

**Packed BCD** puts two digits in a byte:

```
decimal 59  ->  0101 1001  =  0x59
```

Note that the hex *looks* like the decimal. That is the point, and it is also the trap: the byte
`0x59` interpreted as a binary number is 89, not 59.

**Unpacked BCD** puts one digit in a byte, in the low nibble, with the high nibble zero (or, for
ASCII digits, `0x3`):

```
decimal 59  ->  0x05, 0x09        (unpacked)
ASCII   59  ->  0x35, 0x39        ('5','9')
```

Valid packed-BCD nibbles are `0`–`9` only. `0x0A` through `0x0F` are invalid BCD.

### 8.2 Why the adjust instructions exist

Add two packed BCD values with ordinary `ADD` and the answer is wrong whenever a digit exceeds 9,
because the adder carries at 16, and decimal carries at 10.

```
   0x28   (BCD 28)
 + 0x14   (BCD 14)
 -------
   0x3C   -> but BCD 28+14 = 42, and 0x3C is not valid BCD at all
```

The fix is to add 6 to any nibble that overflowed, because 16 − 10 = 6:

```
   0x3C + 0x06 = 0x42   ✔ BCD 42
```

`DAA` (decimal adjust after addition) does exactly that, using the carry flag and the *auxiliary*
carry flag `AF` (carry out of bit 3) to decide which nibbles need it. This is the entire reason `AF`
exists — it is useless for anything else. Chapter 24 covers `DAA`, `DAS`, `AAA`, `AAS`, `AAM` and
`AAD` in full, with every case worked.

---

## 9. Characters: ASCII

The 8086 has no character type. A character is a byte holding an ASCII code.

Four facts do most of the work:

```
'0' = 0x30   ... '9' = 0x39      digit -> value:   sub al, '0'
'A' = 0x41   ... 'Z' = 0x5A      value -> digit:   add al, '0'
'a' = 0x61   ... 'z' = 0x7A
space = 0x20                     upper -> lower:   or  al, 0x20
                                 lower -> upper:   and al, 0xDF
```

The case trick works because `'a' − 'A' = 0x20` exactly, i.e. case is bit 5. That is not an accident;
ASCII was laid out that way deliberately. Chapter 25 §6 uses it.

For hex digits the conversion has a step: values 10–15 map to `'A'`–`'F'`, which is 7 further on than
`'9'`+1, because of the punctuation between `0x39` and `0x41`. The standard routine is in Chapter 43.

Full table: [Appendix D](D-ascii-table.md).

---

## 10. Worked examples

### 10.1 What are these bits?

The byte `1001 0110` = `0x96`. It is:

| Interpreted as | Value |
|----------------|-------|
| unsigned integer | 150 |
| signed integer (two's complement) | −106 |
| packed BCD | 96 |
| ASCII | not printable (above 0x7F) |
| an 8086 opcode | `XCHG AX, SI` |

**All five at once.** The bits carry no type. What they mean is decided entirely by which instruction
you point at them. This is the deepest idea in the chapter.

To confirm −106: invert `1001 0110` → `0110 1001`, add 1 → `0110 1010` = 0x6A = 106. So the original
is −106. ✔

### 10.2 A 16-bit subtraction with flags

```
mov ax, 0x2000
sub ax, 0x3000
```

`0x2000 − 0x3000`. Compute as `0x2000 + (two's complement of 0x3000)`:

```
  0x3000 = 0011 0000 0000 0000
  invert = 1100 1111 1111 1111
  +1     = 1101 0000 0000 0000  = 0xD000

  0x2000 + 0xD000 = 0xF000  (no carry out of bit 15)
```

Flags:

- `AX = 0xF000`.
- **`CF = 1`.** There was no carry out of the adder, and for subtraction the 8086 sets `CF` to the
  *inverted* carry-out, so `CF` = 1, meaning "a borrow happened". Correct: unsigned, 0x2000 < 0x3000.
- **`OF = 0`.** Signed, +8192 − +12288 = −4096, and `0xF000` is indeed −4096. No overflow.
- **`SF = 1`** (bit 15 of the result is 1).
- **`ZF = 0`** (result non-zero).

So after this, `JB` (jump if below, tests `CF`) would be taken — and correctly, because unsigned
0x2000 is below 0x3000. `JL` (jump if less, tests `SF ≠ OF`) would also be taken — also correctly,
because signed +8192 < +12288. Both worlds agree here. Construct a case where they disagree in
Exercise 2.9.

### 10.3 Multi-byte addition

The 8086 adds 16 bits at a time. To add two 32-bit numbers stored at `a` and `b`:

```asm
        mov  ax, [a]            ; low word of a
        add  ax, [b]            ; low word of b       -> CF holds the carry out
        mov  [r], ax            ; store low word of result
        mov  ax, [a+2]          ; high word of a
        adc  ax, [b+2]          ; high word of b + CF  <- the carry is used here
        mov  [r+2], ax          ; store high word
```

`ADC` (add with carry) computes `dest + src + CF`. That single flag is how arbitrary-precision
arithmetic is built from a 16-bit adder, and the technique extends to any width — Chapter 39 does
64-bit. The crucial detail is that nothing between the `ADD` and the `ADC` may disturb `CF`; `MOV`
never touches flags, which is why the `mov [r], ax` above is safe.

---

## 11. Quick reference

```
Negate (two's complement) :  invert all bits, add 1
Sign of a byte            :  bit 7     Sign of a word : bit 15
Unsigned byte range       :  0 .. 255            word :  0 .. 65535
Signed   byte range       :  -128 .. +127        word :  -32768 .. +32767
CF set when               :  result doesn't fit UNSIGNED
OF set when               :  result doesn't fit SIGNED
AF set when               :  carry out of bit 3   (only DAA/DAS/AAA/AAS use it)
Little-endian             :  low byte at low address
BCD adjust constant       :  6      (because 16 - 10 = 6)
ASCII digit to value      :  sub al, 0x30
ASCII case bit            :  bit 5   (0x20)
```

---

## Exercises

**2.1** Convert to hexadecimal: (a) 4095 decimal, (b) `1011 0110 0101` binary, (c) 60,000 decimal.

**2.2** Convert to decimal: (a) `0x7FFF`, (b) `0xFFFF` treated as unsigned, (c) `0xFFFF` treated as
signed, (d) `0b10000000` treated as signed 8-bit.

**2.3** Write the 16-bit two's complement representations of −1, −128, −256, −32,768. Give each in
hex.

**2.4** `0x80` is an 8-bit value. Negate it using invert-and-add-1. What do you get, and what does
that tell you about the signed range?

**2.5** For each 8-bit addition, give the result and the values of `CF`, `OF`, `SF` and `ZF`:
(a) `0x7F + 0x7F`, (b) `0x80 + 0xFF`, (c) `0x40 + 0x40`, (d) `0xFF + 0x01`.

**2.6** A 16-bit register holds `0x8000`. What is its value as unsigned? As signed? What single
instruction would make it `0x8000` again if you negated it, and why?

**2.7** The bytes at addresses `0x300` and `0x301` are `0xCD` and `0xAB`. What 16-bit value does
`mov ax, [0x300]` load into `AX`? What is in `AH`? In `AL`?

**2.8** Express decimal 87 as (a) binary, (b) packed BCD, (c) unpacked BCD, (d) the two ASCII bytes
a program would print.

**2.9** Find two 16-bit values such that after `CMP` of the first with the second, `JB` is taken but
`JL` is not. Explain in one sentence why the two disagree.

**2.10** Add the packed BCD values `0x47` and `0x38` with a plain `ADD`. Show the raw result, show
why it is wrong, and show the adjustment that fixes it.

**2.11** A 32-bit value `0x12345678` is stored at address `0x400`. List the four bytes at addresses
`0x400` through `0x403` in order.

**2.12** Without using a calculator, add `0x9F3A` and `0x76C8` in hex. Then state whether an 8086
`ADD AX, BX` producing this result would set `CF`, and whether it would set `OF` if both were
treated as signed.

Answers in [Appendix H](H-exercise-solutions.md#chapter-2).

---

[← Toolchain setup](01-toolchain-setup.md) · [Contents](README.md) · [Next: Digital logic recap →](03-digital-logic-recap.md)
