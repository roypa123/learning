# Chapter 21 — Data transfer instructions

[← Machine code encoding](20-machine-encoding.md) · [Contents](README.md) · [Next: Arithmetic →](22-arithmetic.md)

---

## Goal

Every instruction that moves data without changing it: `MOV`, `XCHG`, `LEA`, `LDS`, `LES`, `PUSH`,
`POP`, `PUSHF`, `POPF`, `IN`, `OUT`, `XLAT`, `LAHF`, `SAHF`. For each: syntax, every legal operand
combination, encoding, clock count, flag effects, and what it is for.

**One rule covers the whole chapter: with two exceptions, none of these instructions affects any
flag.** The exceptions are `POPF` and `SAHF`, which load the flags by definition.

---

## 1. `MOV`

Copies the source to the destination. The source is unchanged.

```asm
        mov  destination, source
```

### 1.1 Every legal form

| Form | Example | Opcode | Bytes | Clocks |
|------|---------|--------|-------|--------|
| register ← register | `mov ax, bx` | `89`/`8B` | 2 | 2 |
| register ← memory | `mov ax, [bx]` | `8B` | 2–4 | 8 + EA |
| memory ← register | `mov [bx], ax` | `89` | 2–4 | 9 + EA |
| register ← immediate | `mov ax, 5` | `B8+r` | 2–3 | 4 |
| memory ← immediate | `mov word [bx], 5` | `C7` | 3–6 | 10 + EA |
| accumulator ← direct | `mov ax, [0x1234]` | `A1` | 3 | 10 |
| direct ← accumulator | `mov [0x1234], ax` | `A3` | 3 | 10 |
| segment register ← r/m16 | `mov ds, ax` | `8E` | 2–4 | 2 (reg) / 8+EA (mem) |
| r/m16 ← segment register | `mov ax, ds` | `8C` | 2–4 | 2 / 9+EA |

### 1.2 Every *illegal* form

```asm
        mov  [bx], [si]         ; ✘ memory to memory
        mov  ds, 0x1234         ; ✘ immediate to segment register
        mov  es, ds             ; ✘ segment to segment
        mov  cs, ax             ; ✘ (assembles, but destroys the program)
        mov  ax, bl             ; ✘ size mismatch
        mov  al, [bx], 5        ; ✘ three operands
```

**Memory to memory** is the big one. It is not an assembler limitation: the ModR/M byte has one
`r/m` field, so only one operand can be a memory address. The workarounds:

```asm
        mov  ax, [si]           ; via a register — two instructions
        mov  [di], ax

        movsw                   ; or a string instruction — the ONLY memory-to-memory move
```

`MOVS` is the sole exception in the entire instruction set (Chapter 29).

**Immediate to segment register** costs you a scratch register every time:

```asm
        mov  ax, 0xB800
        mov  es, ax
```

or, if `AX` is precious:

```asm
        push 0xB800             ; ✘ 80186 only
        push word [seg_value]   ; ✔ 8086 — push from memory
        pop  es
```

### 1.3 `MOV` into `SS` and the interrupt shadow

Covered in Chapter 7 §4.2, and worth repeating: after a `MOV` into `SS` or a `POP SS`, **interrupts
are inhibited for exactly one instruction**, so that

```asm
        mov  ss, ax
        mov  sp, 0x1000
```

cannot be interrupted between the two. Put anything at all between them and the protection is gone.

### 1.4 `MOV` never sets flags

```asm
        mov  al, 0              ; ZF is NOT set
        or   al, al             ; now ZF is set
```

This trips up people coming from architectures where loads set condition codes. It is deliberate and
useful: you can compute a condition, then rearrange registers, then branch.

---

## 2. `XCHG`

Swaps two operands.

```asm
        xchg ax, bx             ; 93     — AX and BX swap
        xchg al, ah             ; 86 C4
        xchg [bx], cx           ; 87 0F
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `XCHG AX, r16` | `90+r` | 1 | 3 |
| `XCHG r/m, r` | `86`/`87` | 2–4 | 4 (reg) / 17 + EA (mem) |

**`XCHG AX, reg` is one byte.** That is remarkably compact, and it is why `NOP` is `0x90` — `NOP` is
literally `XCHG AX, AX`.

**With a memory operand, `XCHG` asserts `LOCK#` automatically on the 8086**, even without the
`LOCK` prefix. That makes `xchg [semaphore], al` a genuine atomic test-and-set, which is the basis of
every mutual-exclusion primitive. Chapter 30 §8.

### 2.1 Uses

Swapping without a temporary:

```asm
        xchg ax, bx             ; 3 clocks, 1 byte
; versus
        mov  cx, ax             ; 2 clocks
        mov  ax, bx             ; 2
        mov  bx, cx             ; 2  = 6 clocks, 6 bytes, and CX destroyed
```

Byte-swapping a word (converting endianness):

```asm
        xchg al, ah             ; AX = 0x1234 -> 0x3412
```

---

## 3. `LEA` — Load Effective Address

Computes an effective address and puts it in a register, **without accessing memory**.

```asm
        lea  bx, [si+4]         ; BX = SI + 4        (does NOT read memory)
        mov  bx, [si+4]         ; BX = the word AT SI+4  (does read memory)
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `LEA r16, mem` | `8D` | 2–4 | 2 + EA |

The distinction between `LEA` and `MOV` is the single most useful thing in this chapter. `LEA`
computes; `MOV` fetches.

### 3.1 Uses

**Getting the address of a variable:**

```asm
        lea  dx, msg            ; DX = offset of msg
        mov  dx, msg            ; identical in NASM — both give the offset
```

In NASM these are the same, because `msg` without brackets already means the offset. In MASM, `LEA
DX, msg` and `MOV DX, OFFSET msg` are the two ways to write it, and `MOV DX, msg` would load the
*contents*. Use `MOV` where you can — it is faster (4 clocks vs 2+6=8).

**Arithmetic in one instruction:**

```asm
        lea  ax, [bx+si+10]     ; AX = BX + SI + 10, 2+11 = 13 clocks
; versus
        mov  ax, bx             ; 2
        add  ax, si             ; 3
        add  ax, 10             ; 4  = 9 clocks, and it disturbs the flags
```

`LEA` is slower here but **does not touch the flags**, which matters inside a multi-precision
sequence.

**Computing an address that depends on run-time values:**

```asm
        lea  di, [bx+si]        ; DI = the computed address, for a later STOSB
```

---

## 4. `LDS` and `LES` — load a far pointer

Load a 32-bit far pointer from memory into a register **and** a segment register, in one
instruction.

```asm
        lds  si, [ptr]          ; SI <- [ptr],  DS <- [ptr+2]
        les  di, [ptr]          ; DI <- [ptr],  ES <- [ptr+2]
```

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `LDS r16, m32` | `C5` | 2–4 | 16 + EA |
| `LES r16, m32` | `C4` | 2–4 | 16 + EA |

### 4.1 The memory layout

A far pointer is stored **offset first, then segment** — because the offset is the low half of a
32-bit little-endian value (Chapter 2 §7):

```
   ptr:    dw  0x1234          ; offset   at ptr+0
           dw  0xB800          ; segment  at ptr+2

   les  di, [ptr]              ; DI = 0x1234, ES = 0xB800
```

### 4.2 Uses

**Following a pointer stored in memory:**

```asm
buffer_ptr:  dd  0             ; a far pointer, filled in at run time

        les  di, [buffer_ptr]
        mov  al, [es:di]        ; read through it
```

**Reading an interrupt vector:**

```asm
        xor  ax, ax
        mov  ds, ax
        lds  dx, [0x21*4]       ; DX:?? ... careful — this overwrites DS
```

That last one is a trap: `LDS` loads `DS`, and you were using `DS` to address the vector table. Use
`LES` instead:

```asm
        xor  ax, ax
        mov  ds, ax
        les  bx, [0x21*4]       ; ES:BX = the INT 21h handler's address
```

**Retrieving a far parameter from a stack frame** (Chapter 28 §7):

```asm
        les  di, [bp+6]         ; the far pointer passed by the caller
```

---

## 5. `PUSH` and `POP`

### 5.1 What they do, exactly

```
   PUSH src :   SP <- SP - 2  ;  [SS:SP] <- src
   POP  dst :   dst <- [SS:SP] ;  SP <- SP + 2
```

**Decrement then write; read then increment.** The stack grows downwards and `SP` always points at
the most recently pushed word.

**Everything is 16 bits.** There is no `push al`. `PUSH` and `POP` always move a word, and `SP`
always changes by exactly 2.

### 5.2 Legal forms

| Form | Opcode | Bytes | Clocks |
|------|--------|-------|--------|
| `PUSH r16` | `50+r` | 1 | 11 |
| `PUSH sreg` | `06`/`0E`/`16`/`1E` | 1 | 10 |
| `PUSH m16` | `FF /6` | 2–4 | 16 + EA |
| `POP r16` | `58+r` | 1 | 8 |
| `POP sreg` | `07`/`17`/`1F` | 1 | 8 |
| `POP m16` | `8F /0` | 2–4 | 17 + EA |
| `PUSHF` | `9C` | 1 | 10 |
| `POPF` | `9D` | 1 | 8 |

Not available on the 8086: `PUSH imm16` (80186), `PUSHA`/`POPA` (80186).

`POP CS` (`0x0F`) exists and is useless — it changes `CS` without changing `IP`, so execution
continues at the same offset in a different segment. Never write it. On the 80286 and later, `0x0F`
became the two-byte-opcode prefix.

### 5.3 The stack in pictures

```
   Initially:  SP = 0x1000
               
               0x0FFC  ┌──────────┐
               0x0FFE  │          │
               0x1000  │          │  ◄── SP
                       └──────────┘

   push ax     (AX = 0x1234)
               
               0x0FFC  ┌──────────┐
               0x0FFE  │  0x1234  │  ◄── SP
               0x1000  │          │
                       └──────────┘

   push bx     (BX = 0x5678)
               
               0x0FFC  │  0x5678  │  ◄── SP
               0x0FFE  │  0x1234  │
               0x1000  │          │
                       └──────────┘

   pop cx      CX = 0x5678, SP back to 0x0FFE
   pop dx      DX = 0x1234, SP back to 0x1000
```

**Last in, first out.** The order of pops is the reverse of the order of pushes, and forgetting that
is the commonest stack bug.

### 5.4 Saving and restoring registers

The standard idiom at the start and end of a subroutine:

```asm
myproc: push ax
        push bx
        push cx
        push dx
        ; ... work ...
        pop  dx                 ; reverse order!
        pop  cx
        pop  bx
        pop  ax
        ret
```

On an 80186 this is `PUSHA` / `POPA`; on an 8086 you write it out.

### 5.5 Keeping `SP` even

`PUSH` and `POP` move words, so if `SP` is odd every stack access costs two bus cycles (Chapter 10
§4.5). DOS gives you an even `SP`; keep it that way by only ever adjusting `SP` in even amounts.

### 5.6 Transferring between segment registers

```asm
        push ds
        pop  es                 ; ES = DS, without using a general register
```

Two bytes, 18 clocks. Compared with `mov ax, ds` / `mov es, ax` — also two instructions, 4 bytes,
4 clocks. The `MOV` version is much faster; the `PUSH`/`POP` version is used when no register is
free.

---

## 6. `PUSHF` and `POPF`

```asm
        pushf                   ; push the 16-bit FLAGS register
        popf                    ; pop 16 bits into FLAGS
```

`POPF` is one of only two instructions in this chapter that changes flags — it changes **all** of
them, including `IF`, `DF` and `TF`.

### 6.1 Preserving flags across a routine

```asm
        pushf
        ; ... anything, including instructions that modify flags ...
        popf                    ; caller's flags restored exactly
```

### 6.2 Setting an individual flag

Since there is no `STT` for the trap flag, or any way to set `OF` directly:

```asm
        pushf
        pop  ax
        or   ax, 0x0100         ; set TF (bit 8)
        push ax
        popf
```

### 6.3 Reading the flags

```asm
        pushf
        pop  ax                 ; AX = the flag word
        test ax, 0x0001         ; is CF set?
```

Slower than `JC`, but it lets you save a whole condition for later.

---

## 7. `IN` and `OUT`

Covered fully in Chapter 17 §2. Summarised:

```asm
        in   al, 0x60           ; E4 60    ports 0-255
        in   ax, 0x60           ; E5 60
        in   al, dx             ; EC       ports 0-65535
        in   ax, dx             ; ED
        out  0x20, al           ; E6 20
        out  0x20, ax           ; E7 20
        out  dx, al             ; EE
        out  dx, ax             ; EF
```

Only `AL` and `AX`. No flags affected.

---

## 8. `XLAT` — table lookup

```asm
        xlat                    ; D7 — one byte
                                ; AL <- [DS:BX + AL]   (AL zero-extended)
```

11 clocks. `BX` points at a table of up to 256 bytes; `AL` is the index on entry and the looked-up
value on exit.

### 8.1 Hex digit conversion

The classic use:

```asm
hextab: db  '0123456789ABCDEF'

; Convert the value in AL (0-15) to its ASCII hex digit.
        mov  bx, hextab
        and  al, 0x0F           ; make sure it is 0-15
        xlat                    ; AL = '0'..'F'
```

Three instructions, and no branching for the `'9'`→`'A'` discontinuity. The alternative:

```asm
        and  al, 0x0F
        cmp  al, 10
        jb   .digit
        add  al, 7              ; skip the punctuation between '9' and 'A'
.digit: add  al, '0'
```

Five instructions and a branch. `XLAT` is better, and Chapter 43 uses it.

### 8.2 Other uses

Any byte→byte mapping: upper-case conversion, character set translation, a sine table quantised to
bytes, a gamma correction curve. The table is built once; the lookup is 11 clocks with no
arithmetic.

### 8.3 Segment override

`XLAT` uses `DS:BX` by default. It accepts a segment override:

```asm
        xlat                    ; DS:BX+AL
        es xlat                 ; ES:BX+AL   (NASM: 'es xlat' or 'xlatb' with a prefix)
```

---

## 9. `LAHF` and `SAHF`

```asm
        lahf                    ; 9F — AH <- low byte of FLAGS
        sahf                    ; 9E — low byte of FLAGS <- AH
```

4 clocks each. They move only the low eight bits, so they reach `SF`, `ZF`, `AF`, `PF` and `CF` —
**not** `OF`, `IF`, `DF` or `TF`.

The layout of the byte:

```
   bit    7   6   5   4   3   2   1   0
         SF  ZF   0  AF   0  PF   1  CF
```

`SAHF` is the second of the two flag-changing instructions in this chapter.

**These exist purely for 8080 compatibility** (Chapter 5 §2.2). In new code, use `PUSHF`/`POPF`,
which preserve everything.

One legitimate modern use: `LAHF` is faster than `PUSHF`/`POP AX` (4 clocks vs 18) when you only
need the arithmetic flags.

---

## 10. Choosing between the alternatives

| Task | Best choice | Why |
|------|-------------|-----|
| Copy a register to a register | `MOV` | 2 clocks |
| Swap two registers | `XCHG` | 3 clocks, 1 byte if `AX` is one of them |
| Get the address of a label | `MOV reg, label` (NASM) | 4 clocks vs `LEA`'s 8 |
| Compute `BX+SI+n` into a register | `LEA` | one instruction, no flags touched |
| Load a far pointer | `LES`/`LDS` | one instruction instead of two |
| Save a register over a call | `PUSH`/`POP` | |
| Save the flags | `PUSHF`/`POPF` | `LAHF`/`SAHF` lose `OF` |
| Read the flags quickly | `LAHF` | 4 clocks |
| Byte→byte table lookup | `XLAT` | 11 clocks, no branching |
| Copy `DS` to `ES` | `MOV AX, DS` / `MOV ES, AX` | 4 clocks vs 18 for push/pop |
| Copy `DS` to `ES` with no free register | `PUSH DS` / `POP ES` | |

---

## 11. Worked program — reversing a string in place

Uses `MOV`, `XCHG`, `LEA` and `PUSH`/`POP` together.

```asm
; reverse.asm — reverse a string in place, then print it
; nasm -f bin reverse.asm -o reverse.com
;
;   SI -> left-hand character
;   DI -> right-hand character
;   AL, AH -> the two characters being swapped
        org  0x100

start:
        ; find the end of the string
        mov  si, text
        mov  di, si
.findend:
        cmp  byte [di], '$'
        je   .found
        inc  di
        jmp  .findend
.found:
        dec  di                 ; DI now points at the last real character

.swap:
        cmp  si, di             ; pointers met or crossed?
        jae  .done
        mov  al, [si]           ; two MOVs and an XCHG would also work;
        mov  ah, [di]           ;   this is clearer
        mov  [si], ah
        mov  [di], al
        inc  si
        dec  di
        jmp  .swap

.done:
        mov  dx, text
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

text:   db   'Hello, 8086!', '$'
```

Output: `!6808 ,olleH`

### 11.1 Notes on the code

**`cmp si, di` / `jae`** uses an *unsigned* comparison, which is correct because these are addresses
and addresses are unsigned. Using `jge` here would be a bug on any string whose address is above
`0x8000` — the sort of bug that appears only in production.

**Two `MOV`s each way rather than `XCHG`.** `xchg al, [di]` would work but costs 17 + EA clocks
because of the memory operand; four register/memory `MOV`s cost 8+5+9+5 = 27... actually more. The
version shown is chosen for clarity; Exercise 21.11 asks you to optimise it.

**The `'$'` search.** DOS function 09h needs the `$`, and we must not reverse it, hence the `dec di`.

---

## 12. Summary

```
  MOV   dst, src    no flags. No memory-to-memory. No immediate-to-segment.
                    No segment-to-segment. Accumulator forms are 1 byte shorter.
  XCHG  a, b        swaps. XCHG AX,r16 is ONE byte. Asserts LOCK# on memory.
  LEA   r16, mem    computes the address, does NOT read memory. No flags.
  LDS/LES r16, m32  loads offset AND segment. Offset first in memory.
  PUSH  src         SP <- SP-2, then store.  Always 16 bits.
  POP   dst         load, then SP <- SP+2.   Pops reverse the pushes.
  PUSHF / POPF      the whole flag word. POPF changes every flag.
  IN / OUT          AL or AX only; DX for ports above 255.
  XLAT              AL <- [DS:BX + AL]. One byte, 11 clocks, no branching.
  LAHF / SAHF       AH <-> low flag byte. Loses OF, IF, DF, TF.

  Of all of these, only POPF and SAHF change any flag.
```

---

## Exercises

**21.1** Which of these are legal? Fix the illegal ones.

```
(a) mov  ax, [bx]        (b) mov  [bx], [si]      (c) mov  ds, 0x1000
(d) mov  es, ds          (e) mov  al, ah          (f) xchg ax, [bx]
(g) push al              (h) pop  cs              (i) lea  ax, 5
(j) les  bx, [si]        (k) in   bl, dx          (l) mov  [bx], 5
```

**21.2** Write the two-instruction sequence that sets `ES` to `0xA000`. Why can it not be done in
one?

**21.3** What is the difference between `lea bx, [si+4]` and `mov bx, [si+4]`? Give the value of
`BX` after each, assuming `SI = 0x100`, `DS = 0x2000`, and the word at physical `0x20104` is
`0x9999`.

**21.4** `ptr` holds a far pointer to the video buffer. Write the two `dw` definitions and the one
instruction that loads it into `ES:DI`.

**21.5** After `mov sp, 0x1000` / `push ax` / `push bx` / `push cx`, what is `SP`? At what offset is
the value that was in `AX`?

**21.6** Write a subroutine prologue and epilogue that preserves `AX`, `BX`, `CX`, `DX` and the
flags.

**21.7** `XCHG AX, BX` is one byte. Why is `XCHG BX, CX` two?

**21.8** Explain why `NOP` and `XCHG AX, AX` are the same instruction, and give the opcode.

**21.9** Build a 16-byte translation table and write the three instructions that convert a value
0–15 in `AL` into its ASCII hex digit using `XLAT`.

**21.10** `LAHF` puts the flags in `AH`. Which flags does it *not* capture? Give a situation where
that matters.

**21.11** Rewrite the swap in §11 to use `XCHG` and count the clocks of both versions. Which is
faster on an 8086?

**21.12** Write the code that copies `DS` into `ES` (a) using a general register, (b) without using
one. Give the byte count and clock count of each.

Answers in [Appendix H](H-exercise-solutions.md#chapter-21).

---

[← Machine code encoding](20-machine-encoding.md) · [Contents](README.md) · [Next: Arithmetic →](22-arithmetic.md)
