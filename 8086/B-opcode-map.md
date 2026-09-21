# Appendix B — Opcode map

[← Appendix A](A-instruction-reference.md) · [Contents](README.md) · [Appendix C →](C-interrupt-reference.md)

---

All 256 one-byte opcodes, plus the group extensions that use the ModR/M `reg` field as three extra
opcode bits. Chapter 20 explains the encoding; this is the lookup table for it.

## Notation

| Symbol | Meaning |
|--------|---------|
| `eb` / `ew` | an r/m operand, byte or word — a register or a memory address, from ModR/M |
| `rb` / `rw` | a register operand, byte or word, from the ModR/M `reg` field |
| `sr` | a segment register, from the ModR/M `reg` field |
| `ib` / `iw` | an immediate byte or word, following the instruction |
| `m` | a memory operand only (no register form) |
| `—` | not used on the 8086; these became instructions on the 80186 and 80386 |
| `*name*` | undocumented or dangerous |

An entry with `eb`, `ew`, `rb`, `rw`, `sr` or `m` is followed by a **ModR/M byte**.

---

## 1. The one-byte map

### Low nibble 0–7

| | **0** | **1** | **2** | **3** | **4** | **5** | **6** | **7** |
|---|---|---|---|---|---|---|---|---|
| **0x** | ADD eb,rb | ADD ew,rw | ADD rb,eb | ADD rw,ew | ADD AL,ib | ADD AX,iw | PUSH ES | POP ES |
| **1x** | ADC eb,rb | ADC ew,rw | ADC rb,eb | ADC rw,ew | ADC AL,ib | ADC AX,iw | PUSH SS | POP SS |
| **2x** | AND eb,rb | AND ew,rw | AND rb,eb | AND rw,ew | AND AL,ib | AND AX,iw | ES: | DAA |
| **3x** | XOR eb,rb | XOR ew,rw | XOR rb,eb | XOR rw,ew | XOR AL,ib | XOR AX,iw | SS: | AAA |
| **4x** | INC AX | INC CX | INC DX | INC BX | INC SP | INC BP | INC SI | INC DI |
| **5x** | PUSH AX | PUSH CX | PUSH DX | PUSH BX | PUSH SP | PUSH BP | PUSH SI | PUSH DI |
| **6x** | � | � | � | � | � | � | � | � |
| **7x** | JO | JNO | JB/JC | JNB/JNC | JE/JZ | JNE/JNZ | JBE | JA |
| **8x** | GRP1 eb,ib | GRP1 ew,iw | GRP1 eb,ib | GRP1 ew,ib | TEST eb,rb | TEST ew,rw | XCHG eb,rb | XCHG ew,rw |
| **9x** | NOP | XCHG AX,CX | XCHG AX,DX | XCHG AX,BX | XCHG AX,SP | XCHG AX,BP | XCHG AX,SI | XCHG AX,DI |
| **Ax** | MOV AL,[iw] | MOV AX,[iw] | MOV [iw],AL | MOV [iw],AX | MOVSB | MOVSW | CMPSB | CMPSW |
| **Bx** | MOV AL,ib | MOV CL,ib | MOV DL,ib | MOV BL,ib | MOV AH,ib | MOV CH,ib | MOV DH,ib | MOV BH,ib |
| **Cx** | � | � | RET iw | RET | LES rw,m | LDS rw,m | MOV eb,ib | MOV ew,iw |
| **Dx** | GRP2 eb,1 | GRP2 ew,1 | GRP2 eb,CL | GRP2 ew,CL | AAM | AAD | *SALC* | XLAT |
| **Ex** | LOOPNE | LOOPE | LOOP | JCXZ | IN AL,ib | IN AX,ib | OUT ib,AL | OUT ib,AX |
| **Fx** | LOCK | � | REPNE | REP | HLT | CMC | GRP3 byte | GRP3 word |

### Low nibble 8–F

| | **8** | **9** | **A** | **B** | **C** | **D** | **E** | **F** |
|---|---|---|---|---|---|---|---|---|
| **0x** | OR eb,rb | OR ew,rw | OR rb,eb | OR rw,ew | OR AL,ib | OR AX,iw | PUSH CS | *POP CS* |
| **1x** | SBB eb,rb | SBB ew,rw | SBB rb,eb | SBB rw,ew | SBB AL,ib | SBB AX,iw | PUSH DS | POP DS |
| **2x** | SUB eb,rb | SUB ew,rw | SUB rb,eb | SUB rw,ew | SUB AL,ib | SUB AX,iw | CS: | DAS |
| **3x** | CMP eb,rb | CMP ew,rw | CMP rb,eb | CMP rw,ew | CMP AL,ib | CMP AX,iw | DS: | AAS |
| **4x** | DEC AX | DEC CX | DEC DX | DEC BX | DEC SP | DEC BP | DEC SI | DEC DI |
| **5x** | POP AX | POP CX | POP DX | POP BX | POP SP | POP BP | POP SI | POP DI |
| **6x** | � | � | � | � | � | � | � | � |
| **7x** | JS | JNS | JP | JNP | JL | JGE | JLE | JG |
| **8x** | MOV eb,rb | MOV ew,rw | MOV rb,eb | MOV rw,ew | MOV ew,sr | LEA rw,m | MOV sr,ew | POP ew |
| **9x** | CBW | CWD | CALL far | WAIT | PUSHF | POPF | SAHF | LAHF |
| **Ax** | TEST AL,ib | TEST AX,iw | STOSB | STOSW | LODSB | LODSW | SCASB | SCASW |
| **Bx** | MOV AX,iw | MOV CX,iw | MOV DX,iw | MOV BX,iw | MOV SP,iw | MOV BP,iw | MOV SI,iw | MOV DI,iw |
| **Cx** | � | � | RETF iw | RETF | INT 3 | INT ib | INTO | IRET |
| **Dx** | ESC 0 | ESC 1 | ESC 2 | ESC 3 | ESC 4 | ESC 5 | ESC 6 | ESC 7 |
| **Ex** | CALL near | JMP near | JMP far | JMP short | IN AL,DX | IN AX,DX | OUT DX,AL | OUT DX,AX |
| **Fx** | CLC | STC | CLI | STI | CLD | STD | GRP4 | GRP5 |

---

## 2. The group extensions

When an opcode is marked `GRPn`, the **`reg` field of the ModR/M byte** supplies three more opcode
bits. Chapter 20 §5.

### Group 1 — immediate arithmetic (`80`, `81`, `82`, `83`)

| `reg` | Instruction |
|-------|-------------|
| `000` | `ADD` |
| `001` | `OR` |
| `010` | `ADC` |
| `011` | `SBB` |
| `100` | `AND` |
| `101` | `SUB` |
| `110` | `XOR` |
| `111` | `CMP` |

`80` = byte, imm8 · `81` = word, imm16 · `82` = byte, imm8 (an alias of `80`) ·
`83` = word, **sign-extended** imm8.

### Group 2 — shifts and rotates (`D0`, `D1`, `D2`, `D3`)

| `reg` | Instruction |
|-------|-------------|
| `000` | `ROL` |
| `001` | `ROR` |
| `010` | `RCL` |
| `011` | `RCR` |
| `100` | `SHL` / `SAL` |
| `101` | `SHR` |
| `110` | — |
| `111` | `SAR` |

`D0` = byte by 1 · `D1` = word by 1 · `D2` = byte by `CL` · `D3` = word by `CL`.

**There is no immediate count other than 1** on an 8086. `C0` and `C1` are 80186 instructions.

### Group 3 — `TEST`, `NOT`, `NEG`, `MUL`, `DIV` (`F6`, `F7`)

| `reg` | Instruction |
|-------|-------------|
| `000` | `TEST r/m, imm` |
| `001` | — |
| `010` | `NOT` |
| `011` | `NEG` |
| `100` | `MUL` |
| `101` | `IMUL` |
| `110` | `DIV` |
| `111` | `IDIV` |

`F6` = byte · `F7` = word. Only `reg = 000` has an immediate operand.

### Group 4 — byte `INC`/`DEC` (`FE`)

| `reg` | Instruction |
|-------|-------------|
| `000` | `INC r/m8` |
| `001` | `DEC r/m8` |
| others | — |

### Group 5 — word `INC`/`DEC`, `CALL`, `JMP`, `PUSH` (`FF`)

| `reg` | Instruction |
|-------|-------------|
| `000` | `INC r/m16` |
| `001` | `DEC r/m16` |
| `010` | `CALL near r/m16` — indirect |
| `011` | `CALL far m32` — indirect |
| `100` | `JMP near r/m16` — indirect, the jump-table form |
| `101` | `JMP far m32` — indirect |
| `110` | `PUSH r/m16` |
| `111` | — |

---

## 3. The ModR/M byte

```
   7  6  5  4  3  2  1  0
  ┌─────┬────────┬────────┐
  │ mod │  reg   │  r/m   │
  └─────┴────────┴────────┘
```

### `mod`

| `mod` | Meaning |
|-------|---------|
| `00` | memory, **no displacement** — except `r/m = 110`, which means `[disp16]` |
| `01` | memory + **signed 8-bit** displacement |
| `10` | memory + **16-bit** displacement |
| `11` | `r/m` is a register |

### `reg` and `r/m` when `mod = 11`

| Value | `w = 1` | `w = 0` | Segment (for `MOV sr`) |
|-------|---------|---------|------------------------|
| `000` | `AX` | `AL` | `ES` |
| `001` | `CX` | `CL` | `CS` |
| `010` | `DX` | `DL` | `SS` |
| `011` | `BX` | `BL` | `DS` |
| `100` | `SP` | `AH` | — |
| `101` | `BP` | `CH` | — |
| `110` | `SI` | `DH` | — |
| `111` | `DI` | `BH` | — |

### `r/m` when `mod ≠ 11`

| `r/m` | Effective address |
|-------|-------------------|
| `000` | `[BX + SI]` |
| `001` | `[BX + DI]` |
| `010` | `[BP + SI]` |
| `011` | `[BP + DI]` |
| `100` | `[SI]` |
| `101` | `[DI]` |
| `110` | `[BP]` — **or `[disp16]` when `mod = 00`** |
| `111` | `[BX]` |

**There is no pattern for `[AX]`, `[CX]`, `[DX]` or `[SP]`.** That is why those addressing modes do
not exist.

---

## 4. Prefixes

| Byte | Prefix |
|------|--------|
| `26` | `ES:` segment override |
| `2E` | `CS:` segment override |
| `36` | `SS:` segment override |
| `3E` | `DS:` segment override |
| `F0` | `LOCK` |
| `F2` | `REPNE` / `REPNZ` |
| `F3` | `REP` / `REPE` / `REPZ` |

Each costs one byte and two clocks.

---

## 5. Patterns worth noticing

The map has structure, and seeing it makes hand-assembly much faster.

**The arithmetic block, `00`–`3F`.** Eight operations, eight opcodes each, in the order
**ADD OR ADC SBB AND SUB XOR CMP** — and the same order appears as the `reg` field of Group 1. Each
operation's base is 8 higher than the last:

```
   ADD 00   OR 08   ADC 10   SBB 18   AND 20   SUB 28   XOR 30   CMP 38
```

Within each group of eight: `+0` = `r/m8,r8`, `+1` = `r/m16,r16`, `+2` = `r8,r/m8`,
`+3` = `r16,r/m16`, `+4` = `AL,imm8`, `+5` = `AX,imm16`. That is the `d` and `w` bits of
Chapter 20 §2.

**The `+r` blocks.** Eight consecutive opcodes with the register number in the low three bits:

```
   40+r INC   48+r DEC   50+r PUSH   58+r POP
   90+r XCHG AX,r        B0+r MOV r8,imm8      B8+r MOV r16,imm16
```

Register order is always `AX CX DX BX SP BP SI DI`.

**The conditional jumps, `70`–`7F`**, are in complementary pairs: even opcodes test a condition, odd
opcodes test its negation. `74` is `JE` and `75` is `JNE`; `7C` is `JL` and `7D` is `JGE`.

**The string instructions, `A4`–`AF`**, alternate byte and word: `A4` `MOVSB`, `A5` `MOVSW`, `A6`
`CMPSB`, `A7` `CMPSW`, and so on. The low bit is `w`.

**The segment prefixes** are `001ss110`: `26` = `00`, `2E` = `01`, `36` = `10`, `3E` = `11` — the
same two-bit segment encoding as the `reg` field of `MOV sr`.

**`0x90` is `XCHG AX, AX`**, which does nothing, which is why `NOP` has that opcode.

---

## 6. The gaps

| Opcodes | On the 8086 | Later |
|---------|-------------|-------|
| `60`–`6F` | undefined | 80186: `PUSHA`, `POPA`, `BOUND`, `PUSH imm`, `IMUL imm`, `INS`, `OUTS`, and the 80386's `Jcc` near |
| `C0`, `C1` | undefined | 80186: shift/rotate by an immediate count |
| `C8`, `C9` | undefined | 80186: `ENTER`, `LEAVE` |
| `F1` | undefined | 80386: `ICEBP` (undocumented) |
| `0F` | `POP CS` — useless and dangerous | 80286: the two-byte opcode escape prefix |
| `D6` | `SALC` — undocumented: `AL = CF ? 0xFF : 0x00` | still undocumented |

**`0F` becoming an escape prefix is the single most consequential change in the map's history.**
Every instruction added since 1982 — protected mode, 32-bit, MMX, SSE, AVX — lives behind it.

---

[← Appendix A](A-instruction-reference.md) · [Contents](README.md) · [Appendix C →](C-interrupt-reference.md)
