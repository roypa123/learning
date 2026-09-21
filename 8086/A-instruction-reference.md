# Appendix A — Instruction set reference

[← Where to go next](55-where-next.md) · [Contents](README.md) · [Appendix B →](B-opcode-map.md)

---

Every 8086 instruction, alphabetically. Clock counts are for a 5 MHz 8086 with the operand bytes
already in the queue; add the effective-address time from §1 for any memory operand, and 4 more for
a word access at an odd address.

---

## 1. Effective-address timings

Add to every instruction with a memory operand.

| Addressing form | Clocks |
|-----------------|--------|
| `[disp16]` — direct | 6 |
| `[BX]` `[BP]` `[SI]` `[DI]` | 5 |
| `[BX+d]` `[BP+d]` `[SI+d]` `[DI+d]` | 9 |
| `[BX+SI]` `[BP+DI]` | 7 |
| `[BX+DI]` `[BP+SI]` | 8 |
| `[BX+SI+d]` `[BP+DI+d]` | 11 |
| `[BX+DI+d]` `[BP+SI+d]` | 12 |
| **plus a segment override prefix** | **+2** |
| **plus a word operand at an odd address** | **+4** |

---

## 2. Flag notation

| Symbol | Meaning |
|--------|---------|
| `×` | modified according to the result |
| `?` | **undefined** — do not rely on it |
| `0` / `1` | forced to that value |
| blank | unaffected |

Flag order throughout: **O D I T S Z A P C**.

---

## 3. The instructions

### AAA — ASCII adjust after addition

```
   37            1 byte, 8 clocks
```

If `(AL AND 0x0F) > 9` or `AF = 1`: `AL += 6`, `AH += 1`, `AF = CF = 1`. Then `AL &= 0x0F`.

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| ? | | | | ? | ? | × | ? | × |

Chapter 24 §4.

### AAD — ASCII adjust before division

```
   D5 0A         2 bytes, 60 clocks
```

`AL = AH × 10 + AL; AH = 0`. The second byte is the radix and may be changed.

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| ? | | | | × | × | ? | × | ? |

Chapter 24 §7.

### AAM — ASCII adjust after multiplication

```
   D4 0A         2 bytes, 83 clocks
```

`AH = AL / 10; AL = AL MOD 10`. The second byte is the divisor; `AAM 16` splits a byte into nibbles.
`AAM 0` generates `INT 0`.

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| ? | | | | × | × | ? | × | ? |

Chapter 24 §6.

### AAS — ASCII adjust after subtraction

```
   3F            1 byte, 8 clocks
```

Chapter 24 §5. Flags as `AAA`.

### ADC — add with carry

```
   10 /r         ADC r/m8, r8      3 / 16+EA
   11 /r         ADC r/m16, r16    3 / 16+EA
   12 /r         ADC r8, r/m8      3 / 9+EA
   13 /r         ADC r16, r/m16    3 / 9+EA
   14 ib         ADC AL, imm8      4
   15 iw         ADC AX, imm16     4
   80 /2 ib      ADC r/m8, imm8    4 / 17+EA
   81 /2 iw      ADC r/m16, imm16  4 / 17+EA
   83 /2 ib      ADC r/m16, imm8   4 / 17+EA   (sign-extended)
```

`dest = dest + src + CF`

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | × | × | × | × | × |

Chapter 22 §2.

### ADD

Same forms as `ADC` with opcodes `00`–`05` and group extension `/0`. Same clocks, same flags.

Chapter 22 §1.

### AND

```
   20 /r  21 /r  22 /r  23 /r      3 / 9+EA / 16+EA
   24 ib  25 iw                    4
   80 /4  81 /4                    4 / 17+EA
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| **0** | | | | × | × | ? | × | **0** |

Chapter 25 §2.

### CALL

```
   E8 cw         CALL near label       3 bytes, 19
   9A cd         CALL far seg:off      5 bytes, 28
   FF /2         CALL near r/m16       16 (reg) / 21+EA (mem)
   FF /3         CALL far m32          37+EA
```

Near: push `IP`. Far: push `CS`, then `IP`. No flags affected.

Chapter 28 §1.

### CBW — convert byte to word

```
   98            1 byte, 2 clocks
```

Sign-extends `AL` into `AX`. No flags.

### CLC / CLD / CLI

```
   F8  CLC   CF = 0        2 clocks
   FC  CLD   DF = 0        2 clocks
   FA  CLI   IF = 0        2 clocks
```

Chapter 30 §1.

### CMC — complement carry

```
   F5            1 byte, 2 clocks
```

### CMP

Same forms as `ADC` with opcodes `38`–`3D` and group extension `/7`. Computes `dest − src`, sets the
flags, **discards the result**. Clocks: 3 (reg,reg), 9+EA (reg,mem), 9+EA (mem,reg).

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | × | × | × | × | × |

Chapter 22 §5.

### CMPS / CMPSB / CMPSW

```
   A6            CMPSB    1 byte, 22 clocks
   A7            CMPSW    1 byte, 22 clocks
   F3 A6/A7      REPE CMPS          9 + 22/rep
   F2 A6/A7      REPNE CMPS         9 + 22/rep
```

Compares `DS:[SI]` with `ES:[DI]`, adjusts both. Flags as `CMP`.

Chapter 29 §2.2.

### CWD — convert word to doubleword

```
   99            1 byte, 5 clocks
```

Sign-extends `AX` into `DX:AX`. No flags.

### DAA / DAS — decimal adjust

```
   27  DAA       1 byte, 4 clocks
   2F  DAS       1 byte, 4 clocks
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| ? | | | | × | × | × | × | × |

Chapter 24 §2, §3.

### DEC

```
   48+r          DEC r16       1 byte, 2 clocks
   FE /1         DEC r/m8      3 (reg) / 15+EA
   FF /1         DEC r/m16     3 (reg) / 15+EA
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | × | × | × | × | *unchanged* |

**`CF` is not affected.** Chapter 22 §6.

### DIV

```
   F6 /6         DIV r/m8      80-90 (reg) / 86-96+EA
   F7 /6         DIV r/m16     144-162 (reg) / 150-168+EA
```

8-bit: `AL = AX / src`, `AH = AX MOD src`.
16-bit: `AX = DX:AX / src`, `DX = DX:AX MOD src`.

**All flags undefined.** A zero divisor or an oversized quotient generates `INT 0`.

Chapter 23 §4.

### ESC

```
   D8-DF /r      2+ bytes, 2 clocks (reg) / 8+EA (mem)
```

Hands the operand address to a coprocessor. No flags. Chapter 30 §9.

### HLT

```
   F4            1 byte, 2 clocks
```

Halts until an interrupt, NMI or reset. Chapter 30 §5.

### IDIV

```
   F6 /7         IDIV r/m8     101-112 / 107-118+EA
   F7 /7         IDIV r/m16    165-184 / 171-190+EA
```

Signed. All flags undefined. Chapter 23 §5.

### IMUL

```
   F6 /5         IMUL r/m8     80-98 / 86-104+EA
   F7 /5         IMUL r/m16    128-154 / 134-160+EA
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | ? | ? | ? | ? | × |

`CF = OF = 0` when the upper half is merely sign extension. Chapter 23 §3.

### IN

```
   E4 ib         IN AL, imm8       2 bytes, 10
   E5 ib         IN AX, imm8       2 bytes, 14
   EC            IN AL, DX         1 byte,  8
   ED            IN AX, DX         1 byte,  12
```

No flags. Chapter 17 §2.

### INC

```
   40+r          INC r16       1 byte, 2 clocks
   FE /0         INC r/m8      3 / 15+EA
   FF /0         INC r/m16     3 / 15+EA
```

Flags as `DEC` — **`CF` unaffected**.

### INT / INTO / IRET

```
   CD ib         INT imm8      2 bytes, 51
   CC            INT 3         1 byte,  52
   CE            INTO          1 byte,  53 taken / 4 not
   CF            IRET          1 byte,  24
```

`INT`: push flags, `IF = 0`, `TF = 0`, push `CS`, push `IP`, load from `4n`.
`IRET`: pop `IP`, `CS`, flags.

Chapter 31 §4.

### JMP

```
   EB cb         JMP short     2 bytes, 15   (-128..+127)
   E9 cw         JMP near      3 bytes, 15
   EA cd         JMP far       5 bytes, 15
   FF /4         JMP near r/m16    11 (reg) / 18+EA
   FF /5         JMP far m32       24+EA
```

No flags. Chapter 27 §2.

### Jcc — conditional jumps

**All are 2 bytes, short range only (−128…+127). 16 clocks taken, 4 not taken.**

| Opcode | Mnemonics | Condition |
|--------|-----------|-----------|
| `70` | `JO` | `OF = 1` |
| `71` | `JNO` | `OF = 0` |
| `72` | `JB` `JNAE` `JC` | `CF = 1` |
| `73` | `JNB` `JAE` `JNC` | `CF = 0` |
| `74` | `JE` `JZ` | `ZF = 1` |
| `75` | `JNE` `JNZ` | `ZF = 0` |
| `76` | `JBE` `JNA` | `CF = 1 or ZF = 1` |
| `77` | `JA` `JNBE` | `CF = 0 and ZF = 0` |
| `78` | `JS` | `SF = 1` |
| `79` | `JNS` | `SF = 0` |
| `7A` | `JP` `JPE` | `PF = 1` |
| `7B` | `JNP` `JPO` | `PF = 0` |
| `7C` | `JL` `JNGE` | `SF ≠ OF` |
| `7D` | `JGE` `JNL` | `SF = OF` |
| `7E` | `JLE` `JNG` | `ZF = 1 or SF ≠ OF` |
| `7F` | `JG` `JNLE` | `ZF = 0 and SF = OF` |

**`A`/`B` are unsigned; `G`/`L` are signed.** Chapter 27 §4.

### JCXZ

```
   E3 cb         2 bytes, 18 taken / 6 not
```

Jumps if `CX = 0`. Tests no flag. Chapter 27 §5.

### LAHF / SAHF

```
   9F  LAHF      1 byte, 4 clocks    AH = low byte of FLAGS
   9E  SAHF      1 byte, 4 clocks    low byte of FLAGS = AH
```

`SAHF` sets `SF ZF AF PF CF`. Chapter 21 §9.

### LDS / LES

```
   C5 /r         LDS r16, m32      16+EA
   C4 /r         LES r16, m32      16+EA
```

Loads the offset into the register and the segment into `DS`/`ES`. No flags. Chapter 21 §4.

### LEA

```
   8D /r         2-4 bytes, 2+EA clocks
```

Computes the effective address without accessing memory. No flags. Chapter 21 §3.

### LOCK

```
   F0            prefix, 2 clocks
```

Asserts `LOCK#` for the following instruction. Chapter 30 §8.

### LODS / LODSB / LODSW

```
   AC  LODSB     1 byte, 12 clocks
   AD  LODSW     1 byte, 12 clocks
```

`AL`/`AX` ← `DS:[SI]`, then adjusts `SI`. **No flags.** Chapter 29 §2.4.

### LOOP / LOOPE / LOOPNE

```
   E2 cb   LOOP              2 bytes, 17 taken / 5 not
   E1 cb   LOOPE  LOOPZ      2 bytes, 18 taken / 6 not
   E0 cb   LOOPNE LOOPNZ     2 bytes, 19 taken / 5 not
```

Decrements `CX`, then branches. **No flags affected.** Short range only. Chapter 27 §6.

### MOV

```
   88 /r         MOV r/m8, r8      2 / 9+EA
   89 /r         MOV r/m16, r16    2 / 9+EA
   8A /r         MOV r8, r/m8      2 / 8+EA
   8B /r         MOV r16, r/m16    2 / 8+EA
   8C /r         MOV r/m16, sreg   2 / 9+EA
   8E /r         MOV sreg, r/m16   2 / 8+EA
   A0 iw         MOV AL, moffs8    10
   A1 iw         MOV AX, moffs16   10
   A2 iw         MOV moffs8, AL    10
   A3 iw         MOV moffs16, AX   10
   B0+r ib       MOV r8, imm8      4
   B8+r iw       MOV r16, imm16    4
   C6 /0 ib      MOV r/m8, imm8    10+EA
   C7 /0 iw      MOV r/m16, imm16  10+EA
```

**No flags.** No memory-to-memory, no immediate-to-segment, no segment-to-segment.

Chapter 21 §1.

### MOVS / MOVSB / MOVSW

```
   A4  MOVSB     1 byte, 18 clocks
   A5  MOVSW     1 byte, 18 clocks
   F3 A4/A5      REP MOVS    9 + 17/rep
```

`ES:[DI]` ← `DS:[SI]`, adjusts both. **No flags.** The only memory-to-memory move.

Chapter 29 §2.1.

### MUL

```
   F6 /4         MUL r/m8      70-77 / 76-83+EA
   F7 /4         MUL r/m16     118-133 / 124-139+EA
```

8-bit: `AX = AL × src`. 16-bit: `DX:AX = AX × src`.

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | ? | ? | ? | ? | × |

`CF = OF = 0` when the upper half is zero. Chapter 23 §2.

### NEG

```
   F6 /3         NEG r/m8      3 / 16+EA
   F7 /3         NEG r/m16     3 / 16+EA
```

`dest = 0 − dest`. `CF = 1` unless the operand was zero. `OF = 1` only for `0x80`/`0x8000`.

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × | | | | × | × | × | × | × |

Chapter 22 §7.

### NOP

```
   90            1 byte, 3 clocks
```

It *is* `XCHG AX, AX`. No flags.

### NOT

```
   F6 /2         NOT r/m8      3 / 16+EA
   F7 /2         NOT r/m16     3 / 16+EA
```

**No flags at all.** Chapter 25 §5.

### OR

Same forms as `AND` with opcodes `08`–`0D` and group extension `/1`. `CF = OF = 0`.

### OUT

```
   E6 ib         OUT imm8, AL      2 bytes, 10
   E7 ib         OUT imm8, AX      2 bytes, 14
   EE            OUT DX, AL        1 byte,  8
   EF            OUT DX, AX        1 byte,  12
```

No flags.

### POP

```
   58+r          POP r16           1 byte, 8
   07/17/1F      POP ES/SS/DS      1 byte, 8
   8F /0         POP m16           17+EA
```

**No `POP CS`** that is safe (`0F` exists and is useless). No flags.

### POPF

```
   9D            1 byte, 8 clocks
```

**Loads every flag** from the stack.

### PUSH

```
   50+r          PUSH r16          1 byte, 11
   06/0E/16/1E   PUSH ES/CS/SS/DS  1 byte, 10
   FF /6         PUSH m16          16+EA
```

`SP -= 2`, then store. No flags. `PUSH imm` is 80186+.

### PUSHF

```
   9C            1 byte, 10 clocks
```

### RCL / RCR / ROL / ROR

```
   D0 /2  RCL r/m8, 1      2 (reg) / 15+EA
   D1 /2  RCL r/m16, 1
   D2 /2  RCL r/m8, CL     8+4n (reg) / 20+EA+4n
   D3 /2  RCL r/m16, CL
   /3 = RCR   /0 = ROL   /1 = ROR
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × (count 1 only) | | | | | | | | × |

**Rotates do not affect `SF`, `ZF`, `AF` or `PF`.** Chapter 26 §6, §7.

### REP / REPE / REPNE

```
   F3            REP / REPE / REPZ     prefix
   F2            REPNE / REPNZ         prefix
```

Chapter 29 §3.

### RET

```
   C3            RET               1 byte, 16
   C2 iw         RET imm16         3 bytes, 20
   CB            RETF              1 byte, 26
   CA iw         RETF imm16        3 bytes, 25
```

No flags. Chapter 28 §1.

### SAHF

See `LAHF`.

### SAL / SAR / SHL / SHR

```
   D0 /4  SHL/SAL r/m8, 1     2 (reg) / 15+EA
   D1 /4  SHL/SAL r/m16, 1
   D2 /4  SHL/SAL r/m8, CL    8+4n / 20+EA+4n
   D3 /4  SHL/SAL r/m16, CL
   /5 = SHR   /7 = SAR
```

| O | D | I | T | S | Z | A | P | C |
|---|---|---|---|---|---|---|---|---|
| × (count 1 only) | | | | × | × | ? | × | × |

**Shifts do set `SF`, `ZF` and `PF`.** An immediate count other than 1 is 80186+.

Chapter 26 §3–§5.

### SBB

Same forms as `ADC` with opcodes `18`–`1D` and group extension `/3`. `dest = dest − src − CF`.

### SCAS / SCASB / SCASW

```
   AE  SCASB     1 byte, 15 clocks
   AF  SCASW     1 byte, 15 clocks
   F3/F2 AE/AF   REPE/REPNE SCAS    9 + 15/rep
```

Compares `AL`/`AX` with `ES:[DI]`, adjusts `DI`. Flags as `CMP`.

Chapter 29 §2.3.

### STC / STD / STI

```
   F9  STC   CF = 1        2 clocks
   FD  STD   DF = 1        2 clocks
   FB  STI   IF = 1        2 clocks (effective after the NEXT instruction)
```

### STOS / STOSB / STOSW

```
   AA  STOSB     1 byte, 11 clocks
   AB  STOSW     1 byte, 11 clocks
   F3 AA/AB      REP STOS    9 + 10/rep
```

`ES:[DI]` ← `AL`/`AX`, adjusts `DI`. **No flags.**

### SUB

Same forms as `ADC` with opcodes `28`–`2D` and group extension `/5`.

### TEST

```
   84 /r         TEST r/m8, r8     3 / 9+EA
   85 /r         TEST r/m16, r16   3 / 9+EA
   A8 ib         TEST AL, imm8     4
   A9 iw         TEST AX, imm16    4
   F6 /0 ib      TEST r/m8, imm8   5 / 11+EA
   F7 /0 iw      TEST r/m16, imm16 5 / 11+EA
```

ANDs and sets the flags without storing. `CF = OF = 0`.

Chapter 25 §6.

### WAIT

```
   9B            1 byte, 3+ clocks
```

Waits for `TEST#` to go low. Chapter 30 §6.

### XCHG

```
   90+r          XCHG AX, r16      1 byte, 3 clocks
   86 /r         XCHG r/m8, r8     4 (reg) / 17+EA
   87 /r         XCHG r/m16, r16   4 (reg) / 17+EA
```

**No flags.** Asserts `LOCK#` automatically with a memory operand. Chapter 21 §2.

### XLAT / XLATB

```
   D7            1 byte, 11 clocks
```

`AL = [DS:BX + AL]`, with `AL` zero-extended. **No flags.** Chapter 21 §8.

### XOR

Same forms as `AND` with opcodes `30`–`35` and group extension `/6`. `CF = OF = 0`.

---

## 4. Quick clock reference

| Category | Clocks |
|----------|--------|
| `MOV`/`INC`/`DEC`/shift by 1, register | **2** |
| `ADD`/`SUB`/`AND`/`OR`/`XOR`/`CMP`/`TEST`/`NEG`/`NOT`, register | **3** |
| `Jcc` not taken, immediate to register | **4** |
| `POP r16`, `CWD` | 8 |
| `IN`/`OUT` via `DX` | 8 |
| `PUSH r16`, `XLAT` | 10–11 |
| `MOV r16, [BX]` | 8 + 5 = **13** |
| `ADD r16, [BX]` | 9 + 5 = **14** |
| `Jcc` taken, `JMP` | **15–16** |
| `RET` near, `ADD [BX], r16` | 16 + 5 = **21** |
| `LOOP` taken | 17 |
| `MOVSB` (with `REP`) | 17 per byte |
| `CALL` near | 19 |
| `IRET` | 24 |
| `INT n` | **51** |
| `MUL r8` | 70–77 |
| `AAM` | 83 |
| `DIV r8` | 80–90 |
| `MUL r16` | **118–133** |
| `DIV r16` | **144–162** |
| `IDIV r16` | 165–184 |

---

## 5. Instructions that set no flags at all

```
   MOV   PUSH  POP   PUSHF   LEA   LDS   LES   XCHG
   IN    OUT   XLAT  NOT     NOP   JMP   Jcc   JCXZ
   CALL  RET   LOOP  LOOPE   LOOPNE
   MOVS  LODS  STOS  (the string moves — but CMPS and SCAS DO set flags)
   CBW   CWD   ESC   HLT     WAIT  LOCK
```

Memorising this list is worth more than it looks: it tells you exactly what you may put between a
flag-setting instruction and the branch that reads it.

---

## 6. Instructions the 8086 does **not** have

Meeting one of these in a listing means the code is for an 80186 or later.

| Instruction | First appeared |
|-------------|----------------|
| `PUSH imm` | 80186 |
| `PUSHA` / `POPA` | 80186 |
| `ENTER` / `LEAVE` | 80186 |
| `BOUND` | 80186 |
| `INS` / `OUTS` | 80186 |
| `IMUL r16, r/m16, imm` | 80186 |
| shift/rotate by an immediate count > 1 | 80186 |
| `LGDT` `LIDT` `LMSW` and protected mode | 80286 |
| `MOVZX` `MOVSX` `BT` `SETcc` `SHLD` | 80386 |
| near `Jcc` (±32 K) | 80386 |
| 32-bit registers and addressing | 80386 |
| `CMPXCHG` `BSWAP` `XADD` | 80486 |
| `CPUID` `RDTSC` | Pentium |

Put `cpu 8086` at the top of every source file and NASM enforces this for you (Chapter 33 §8).

---

[← Where to go next](55-where-next.md) · [Contents](README.md) · [Appendix B →](B-opcode-map.md)
