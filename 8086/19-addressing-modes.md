# Chapter 19 — Addressing modes

[← 8088 vs 8086](18-8088-differences.md) · [Contents](README.md) · [Next: Machine code encoding →](20-machine-encoding.md)

---

## Goal

Cover every way an 8086 instruction can name its operands. For each mode: the syntax, the effective
address arithmetic worked numerically, the default segment, the clock cost, and what it is actually
*for*.

By the end you should be able to look at any bracketed expression and say immediately what it
computes, which segment it uses, and how many clocks the address calculation costs.

---

## 1. What an addressing mode is

An instruction has to say *where* its operands are. The 8086 offers seven places:

```
   mov  ax, bx             ; in another register
   mov  ax, 1234           ; in the instruction itself
   mov  ax, [1234]         ; at a fixed memory address
   mov  ax, [bx]           ; at an address held in a register
   mov  ax, [bx+4]         ; at a register plus a constant
   mov  ax, [bx+si]        ; at the sum of two registers
   mov  ax, [bx+si+4]      ; at the sum of two registers and a constant
```

The last five all produce a 16-bit **effective address** (EA) — an *offset*. The physical address is
then `segment × 16 + EA` (Chapter 9).

Two computations, always, and keeping them separate is the key to not getting confused:

```
   1.  EA       = whatever the brackets say          (16 bits)
   2.  physical = segment register × 16 + EA         (20 bits)
```

---

## 2. Register addressing

The operand is in a register. No memory access at all.

```asm
        mov  ax, bx
        add  cl, dh
        xchg si, di
        inc  bp
```

**Cost: 2 clocks, no bus cycle.** This is the cheapest thing the 8086 does, and the whole art of
optimising 8086 code is keeping operands here.

Both operands can be registers, but they must be the **same size**:

```asm
        mov  ax, bx             ; both 16-bit  ✔
        mov  al, bl             ; both 8-bit   ✔
        mov  ax, bl             ; ✘ size mismatch
        mov  ah, ax             ; ✘ size mismatch
```

---

## 3. Immediate addressing

The operand is a constant, stored inside the instruction.

```asm
        mov  ax, 0x1234         ; B8 34 12
        add  bl, 5              ; 80 C3 05
        cmp  cx, -1             ; 83 F9 FF   (sign-extended byte form)
        mov  byte [bx], 0       ; C6 07 00
```

**Cost: 4 clocks, no bus cycle** (the bytes come from the queue).

Three notes:

**The immediate is always the *source*.** There is no `mov 5, ax`.

**Immediates cannot go into segment registers.** `mov ds, 0x1234` does not exist (Chapter 7 §4.1).

**NASM needs a size when the destination is memory.** `mov [bx], 5` is ambiguous — one byte or two?
Write `mov byte [bx], 5` or `mov word [bx], 5`.

### 3.1 The sign-extended short form

For `ADD`, `ADC`, `SUB`, `SBB`, `CMP`, `AND`, `OR` and `XOR` with a 16-bit destination, if the
immediate fits in a signed byte (−128…+127), the assembler can use a 1-byte immediate that the
processor sign-extends:

```
   81 C3 05 00     add bx, 5      4 bytes — full 16-bit immediate
   83 C3 05        add bx, 5      3 bytes — sign-extended byte      ← NASM picks this
```

You get the shorter form automatically. It matters when you are hand-assembling (Chapter 20 §7) or
counting bytes.

---

## 4. Direct addressing

The address is a 16-bit constant in the instruction.

```asm
        mov  ax, [0x1234]       ; EA = 0x1234
        mov  [count], bl        ; EA = whatever offset the assembler computed for 'count'
        inc  word [total]
```

**Default segment: `DS`.** So `mov ax, [0x1234]` with `DS = 0x2000` reads physical
`0x20000 + 0x1234 = 0x21234`.

**EA calculation cost: 6 clocks.**

### 4.1 The accumulator short form

`MOV` between the accumulator and a direct address has its own one-byte opcode:

```
   A1 34 12        mov ax, [0x1234]        3 bytes
   8B 06 34 12     mov bx, [0x1234]        4 bytes
```

Another reason to keep the working value in `AX`.

### 4.2 A note on NASM syntax

In NASM, a label used **without** brackets is its *address*; **with** brackets it is its *contents*:

```asm
count   dw   0

        mov  ax, count          ; AX = the offset of count (e.g. 0x0112)
        mov  ax, [count]        ; AX = the value stored at count (i.e. 0)
```

This is the opposite of MASM's default, which is the single commonest source of confusion when
moving between the two.

> **MASM note.** MASM writes `MOV AX, count` to mean *the contents* and `MOV AX, OFFSET count` to
> mean the address. NASM is consistent: brackets always mean "the contents of". Chapter 33 §4.

---

## 5. Register indirect addressing

The address is in a register. **Only four registers are allowed: `BX`, `BP`, `SI`, `DI`.**

```asm
        mov  ax, [bx]           ; EA = BX,  segment DS
        mov  ax, [si]           ; EA = SI,  segment DS
        mov  ax, [di]           ; EA = DI,  segment DS
        mov  ax, [bp]           ; EA = BP,  segment SS   ← note
```

Not allowed: `[ax]`, `[cx]`, `[dx]`, `[sp]`. Chapter 20 §4 shows why — the ModR/M byte simply has no
encoding for them.

**EA calculation cost: 5 clocks.**

### 5.1 `[BP]` and its special case

`[BP]` defaults to `SS`, because `BP` is the stack-frame register (Chapter 7 §3.2). There is also an
encoding quirk: **`mod = 00, r/m = 110` means a direct address, not `[BP]`.** So `[BP]` with no
displacement cannot be encoded, and assemblers emit `[BP+0]` — a 3-byte instruction instead of 2.

```
   8B 07           mov ax, [bx]        2 bytes
   8B 46 00        mov ax, [bp]        3 bytes  ← the assembler added a zero displacement
```

This is a real, observable oddity, and Chapter 20 §4.2 explains where it comes from.

### 5.2 What it is for

Walking through memory:

```asm
        mov  bx, array
.next:  mov  al, [bx]
        ; ... do something with AL ...
        inc  bx
        loop .next
```

---

## 6. Based, indexed and based-indexed addressing

These are the same mechanism with different names. The general form is:

```
   EA  =  [base]  +  [index]  +  [displacement]
```

where:

- **base** is `BX` or `BP`, or absent
- **index** is `SI` or `DI`, or absent
- **displacement** is an 8-bit or 16-bit signed constant, or absent

**One base, one index, one displacement — any subset, nothing else.** You cannot have two bases
(`[bx+bp]`), two indexes (`[si+di]`), or a scale factor (`[bx+si*2]` is 80386).

### 6.1 The complete list of legal forms

```
   [BX]           [BP]           [SI]           [DI]
   [BX+disp]      [BP+disp]      [SI+disp]      [DI+disp]
   [BX+SI]        [BX+DI]        [BP+SI]        [BP+DI]
   [BX+SI+disp]   [BX+DI+disp]   [BP+SI+disp]   [BP+DI+disp]
   [disp]                                        (direct)
```

Seventeen forms. That is the entire 8086 memory addressing model.

### 6.2 Based addressing

Base register plus displacement. Used for **records/structures**: the base points at the record, the
displacement selects the field.

```asm
; struct employee { word id; word age; word salary; }   — 6 bytes
ID      equ  0
AGE     equ  2
SALARY  equ  4

        mov  bx, employee_ptr
        mov  ax, [bx+ID]
        mov  cx, [bx+AGE]
        mov  dx, [bx+SALARY]
```

The offsets are constants known at assembly time; the base changes at run time. Change `BX` and the
same three instructions read a different record.

**Cost: 9 clocks** for `[BX+disp]` or `[BP+disp]` or `[SI+disp]` or `[DI+disp]`.

### 6.3 Indexed addressing

Index register plus displacement. Used for **arrays**: the displacement is the array's address, the
index selects the element.

```asm
        mov  si, 0
.next:  mov  al, [array+si]     ; assembles as [SI + disp16]
        ; ...
        inc  si
        cmp  si, 10
        jb   .next
```

Note that `[array+si]` and `[si+array]` are the same thing; NASM accepts both.

**Cost: 9 clocks.**

### 6.4 Based-indexed addressing

Base plus index, optionally plus displacement. Used for **two-dimensional arrays** and for arrays of
records.

```asm
; matrix[row][col], 4 columns of words
        mov  bx, matrix         ; base of the matrix
        mov  si, row
        shl  si, 1
        shl  si, 1
        shl  si, 1              ; SI = row × 8   (4 words per row)
        mov  di, col
        shl  di, 1              ; DI = col × 2
        add  si, di
        mov  ax, [bx+si]        ; the element
```

**Cost: 7 or 8 clocks** without displacement, **11 or 12** with. And here is the detail people miss:

| Form | EA clocks | Why the difference |
|------|-----------|--------------------|
| `[BX+SI]` | **7** | |
| `[BP+DI]` | **7** | |
| `[BX+DI]` | **8** | |
| `[BP+SI]` | **8** | |
| `[BX+SI+disp]` | **11** | |
| `[BP+DI+disp]` | **11** | |
| `[BX+DI+disp]` | **12** | |
| `[BP+SI+disp]` | **12** | |

`BX+SI` and `BP+DI` are one clock faster than `BX+DI` and `BP+SI`. The reason is internal
microcode sequencing — the "natural" pairings are faster. It is worth a clock in a hot loop, and it
is the kind of detail that appears in exam questions.

---

## 7. The complete EA cost table

The number you add to every memory-operand instruction's base clock count.

| Addressing mode | Example | EA clocks |
|-----------------|---------|-----------|
| Register | `AX` | 0 |
| Immediate | `1234` | 0 |
| Direct | `[1234]` | 6 |
| Register indirect | `[BX]` `[BP]` `[SI]` `[DI]` | 5 |
| Base + displacement | `[BX+4]` | 9 |
| Index + displacement | `[SI+4]` | 9 |
| Base + index | `[BX+SI]` `[BP+DI]` | 7 |
| Base + index | `[BX+DI]` `[BP+SI]` | 8 |
| Base + index + disp | `[BX+SI+4]` `[BP+DI+4]` | 11 |
| Base + index + disp | `[BX+DI+4]` `[BP+SI+4]` | 12 |
| **plus** a segment override prefix | `[ES:BX]` | **+2** |

Worked example:

```asm
        add  ax, [bx+di+6]      ; ADD reg,mem = 9 clocks, EA = 12  ->  21 clocks
        add  ax, [bx+si+6]      ; 9 + 11                           ->  20 clocks
        add  ax, [bx]           ; 9 + 5                            ->  14 clocks
        add  ax, bx             ;                                  ->   3 clocks
```

**Seven times the cost** between the first and the last. Chapter 32 uses this systematically.

---

## 8. Default segments, restated

This is the table that determines *which* 64 KiB window each EA lands in.

| Addressing form | Default segment |
|-----------------|-----------------|
| Any form containing **`BP`** | **`SS`** |
| Everything else (`[BX]`, `[SI]`, `[DI]`, `[disp]`, `[BX+SI]`, …) | `DS` |
| String source `SI` | `DS` |
| String destination `DI` | **`ES`** — not overridable |
| Instruction fetch `IP` | `CS` — not overridable |
| Stack `SP` | `SS` — not overridable |

### 8.1 Overrides

```asm
        mov  ax, [es:bx]        ; 26 8B 07   — one extra byte, two extra clocks
        mov  ax, [cs:si]        ; 2E 8B 04
        mov  ax, [ss:0x100]     ; 36 8B 06 00 01
        mov  ax, [ds:bp]        ; 3E 8B 46 00
```

The last one is the common case: forcing `BP` to address the data segment.

---

## 9. Special-purpose addressing

### 9.1 String addressing

The string instructions use `SI` and `DI` implicitly, with automatic increment or decrement:

```asm
        cld                     ; DF = 0, forwards
        mov  si, source         ; DS:SI
        mov  di, dest           ; ES:DI
        mov  cx, 100
        rep  movsb              ; copy 100 bytes, SI and DI advance automatically
```

This is a mode of its own: no brackets appear, the registers are fixed, and the increment is built
in. Chapter 29.

### 9.2 `XLAT` — table addressing

```asm
        mov  bx, table          ; BX = base of a 256-byte table
        mov  al, 5              ; AL = index
        xlat                    ; AL = [DS:BX+AL]
```

One byte, 11 clocks, and it zero-extends `AL` before adding. It is the fastest way to do a byte→byte
lookup, and Chapter 43 uses it for hex conversion.

### 9.3 Port addressing

```asm
        in   al, 0x60           ; direct port, 0-255
        in   al, dx             ; indirect port, 0-65535
```

Only these two. Chapter 17 §2.

### 9.4 Implied addressing

Some instructions name no operand at all because the operand is fixed:

```asm
        cbw                     ; AL -> AX
        cwd                     ; AX -> DX:AX
        lahf                    ; FLAGS -> AH
        daa                     ; adjusts AL
        clc                     ; CF = 0
        pushf                   ; FLAGS -> stack
```

---

## 10. Worked examples

Given:

```
   DS = 0x2000     SS = 0x5000     ES = 0x3000
   BX = 0x0100     BP = 0x0200     SI = 0x0010     DI = 0x0020
```

Compute the EA and the physical address for each.

| Instruction | EA | Segment | Physical |
|-------------|-----|---------|----------|
| `mov ax, [0x1234]` | `0x1234` | `DS` | `0x20000 + 0x1234 = 0x21234` |
| `mov ax, [bx]` | `0x0100` | `DS` | `0x20000 + 0x0100 = 0x20100` |
| `mov ax, [bp]` | `0x0200` | **`SS`** | `0x50000 + 0x0200 = 0x50200` |
| `mov ax, [si]` | `0x0010` | `DS` | `0x20010` |
| `mov ax, [bx+4]` | `0x0104` | `DS` | `0x20104` |
| `mov ax, [bp+4]` | `0x0204` | **`SS`** | `0x50204` |
| `mov ax, [bx+si]` | `0x0110` | `DS` | `0x20110` |
| `mov ax, [bp+di]` | `0x0220` | **`SS`** | `0x50220` |
| `mov ax, [bx+si+8]` | `0x0118` | `DS` | `0x20118` |
| `mov ax, [es:bx]` | `0x0100` | **`ES`** | `0x30000 + 0x0100 = 0x30100` |
| `mov ax, [bx-4]` | `0x00FC` | `DS` | `0x200FC` |

Row 11 shows negative displacements: the displacement is **signed**, so `[bx-4]` is legal and
assembles with the byte `0xFC` = −4.

### 10.1 A displacement that wraps

```
   BX = 0xFFFE,  DS = 0x2000

   mov ax, [bx+4]    ->  EA = 0xFFFE + 4 = 0x10002, truncated to 16 bits = 0x0002
                          physical = 0x20000 + 0x0002 = 0x20002
```

**Not** `0x30002`. The EA arithmetic is 16-bit and wraps within the segment (Chapter 9 §6.1). This
catches people whose buffer runs to the end of a segment.

---

## 11. Choosing a mode

| Task | Mode | Why |
|------|------|-----|
| A single global variable | direct `[total]` | shortest to write, 6 clocks |
| Walk a byte array | register indirect `[bx]` or string `LODSB` | 5 clocks, or 0 with strings |
| Field of a record | based `[bx+FIELD]` | base changes, offset is constant |
| Element of a fixed array | indexed `[array+si]` | array address is constant, index varies |
| Element of a record in an array of records | based-indexed `[bx+si]` | both vary |
| 2-D array | based-indexed with scaling | `[bx+si]` where `SI` = row×width + col×size |
| Local variable in a stack frame | `[bp-n]` | and parameters at `[bp+n]` — Chapter 28 |
| Bulk copy | string instructions | `REP MOVSW` |

---

## 12. Summary

```
  EA = [BX | BP]  +  [SI | DI]  +  [disp8 | disp16]      any subset
  or   [disp16]                                          direct
  physical = segment × 16 + EA

  ONLY BX, BP, SI, DI may appear in brackets.
  One base (BX/BP) and one index (SI/DI). Never two of either.

  default segment:  anything with BP  -> SS
                    everything else   -> DS
                    string dest DI    -> ES (not overridable)

  EA clocks:   direct 6 · indirect 5 · base+disp or index+disp 9
               BX+SI or BP+DI  7    ·  BX+DI or BP+SI  8
               those + disp    11   ·                  12
               segment override +2

  register operand 0 · immediate 0
```

---

## Exercises

**19.1** For each, state whether it is a legal 8086 memory operand; if not, say why.

```
(a) [bx+si]      (b) [ax]         (c) [bp+di+10]   (d) [si+di]
(e) [bx+bp]      (f) [di-4]       (g) [sp+2]       (h) [bx+si*2]
(i) [0x1234]     (j) [bp]         (k) [cx+4]       (l) [es:bp+si]
```

**19.2** Given `DS = 0x1000`, `SS = 0x4000`, `BX = 0x0050`, `BP = 0x0060`, `SI = 0x0005`,
`DI = 0x0008`, compute the EA and physical address for:

```
(a) mov ax, [bx]        (b) mov ax, [bp]          (c) mov ax, [bx+si]
(d) mov ax, [bp+di+2]   (e) mov ax, [0x0200]      (f) mov ax, [es:bx+4]  (ES = 0x2000)
(g) mov ax, [bx-0x10]
```

**19.3** How many clocks does the effective-address calculation cost for each of:
`[0x1234]`, `[SI]`, `[BX+2]`, `[BX+SI]`, `[BX+DI]`, `[BP+SI+4]`, `[ES:BX+SI]`?

**19.4** `ADD AX, mem` takes 9 clocks plus the EA time. Compute the total for
`add ax, [bx+di+100]` and for `add ax, [bx+si+100]`, and explain the difference.

**19.5** Why does `mov ax, [bp]` assemble to three bytes while `mov ax, [bx]` assembles to two?

**19.6** Write an instruction that reads a word from the extra segment at offset `BX+SI+6`.

**19.7** `BX = 0xFFF0` and `DS = 0x3000`. What physical address does `mov al, [bx+0x20]` read?

**19.8** A structure has fields `name` (16 bytes), `age` (word), `salary` (dword) in that order.
Write `equ` definitions for the field offsets, and the instructions that load `age` into `CX` and
the low word of `salary` into `AX`, given that `BX` points at the structure.

**19.9** A 2-D array `M` has 6 rows and 5 columns of words. Write the instruction sequence that
loads `M[r][c]` into `AX`, given `r` in `AL` and `c` in `BL`.

**19.10** Which addressing mode would you use for each: (a) a loop counter kept in memory, (b) the
*n*th byte of a string whose address is in a register, (c) a field of a record, (d) copying 500
bytes?

**19.11** Explain the difference between `mov ax, count` and `mov ax, [count]` in NASM, and what
MASM would write for each.

**19.12** `[BX+SI]` costs 7 clocks and `[BX+DI]` costs 8. Rewrite this loop to save one clock per
iteration:

```asm
        mov  bx, table
        mov  di, 0
.next:  mov  al, [bx+di]
        inc  di
        loop .next
```

Answers in [Appendix H](H-exercise-solutions.md#chapter-19).

---

[← 8088 vs 8086](18-8088-differences.md) · [Contents](README.md) · [Next: Machine code encoding →](20-machine-encoding.md)
