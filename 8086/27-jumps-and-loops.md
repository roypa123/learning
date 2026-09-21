# Chapter 27 — Jumps and loops

[← Shifts and rotates](26-shift-rotate.md) · [Contents](README.md) · [Next: Procedures and the stack →](28-procedures-and-stack.md)

---

## Goal

Control flow: `JMP` in its five forms, all sixteen conditional jumps with both of their mnemonic
families, the `LOOP` family, and `JCXZ`. Then the range limits, which on the 8086 are tighter than
you expect and cause a specific assembler error you will meet.

---

## 1. How a jump works

A jump changes `IP` — and, for far jumps, `CS` as well. The queue is flushed (Chapter 6 §3.3) and
fetching restarts at the new address.

Three ways the target can be specified:

**Relative.** The instruction contains a signed *displacement* added to `IP`. This is what all
conditional jumps and most `JMP`s use. Relative jumps are **position independent** — the code can be
loaded anywhere and still works.

**Direct (absolute).** The instruction contains the target address outright. Only far jumps do this.

**Indirect.** The target is in a register or in memory.

### 1.1 The displacement is measured from the *next* instruction

This is the detail that matters for hand-assembly.

```
   0100:  EB 05        jmp short target    ; displacement = +5
   0102:  ...                              ; IP is 0x0102 when the addition happens
   ...
   0107:  target:                          ; 0x0102 + 5 = 0x0107  ✔
```

`IP` has already advanced past the jump by the time the displacement is added, because the
instruction was fetched before it was executed (Chapter 4 §4.2). So:

```
   displacement  =  target_address − address_of_next_instruction
```

A jump to itself is `EB FE` — displacement −2, because the next instruction is 2 bytes further on.
That is the classic infinite loop:

```asm
        jmp  $                  ; NASM: $ means "here". Assembles to EB FE.
```

---

## 2. `JMP` — unconditional

### 2.1 The five forms

| Form | Syntax | Opcode | Bytes | Range | Clocks |
|------|--------|--------|-------|-------|--------|
| Short | `jmp short label` | `EB cb` | **2** | −128 … +127 | 15 |
| Near direct | `jmp label` | `E9 cw` | 3 | anywhere in the segment | 15 |
| Far direct | `jmp seg:off` | `EA cd` | 5 | anywhere in 1 MiB | 15 |
| Near indirect | `jmp bx` / `jmp [bx]` | `FF /4` | 2–4 | anywhere in the segment | 11 / 18 + EA |
| Far indirect | `jmp far [bx]` | `FF /5` | 2–4 | anywhere in 1 MiB | 24 + EA |

### 2.2 Short versus near

```asm
        jmp  short next         ; EB xx     — 2 bytes, target within ±127
        jmp  next               ; E9 xx xx  — 3 bytes, anywhere in the segment
```

NASM chooses automatically when it can: if the target is already known and close, it emits the short
form. When the target is *forward* and not yet known, NASM assumes the long form unless you write
`short` explicitly. So `jmp short` on a nearby forward jump saves a byte.

### 2.3 Far jumps

```asm
        jmp  0xF000:0xE05B      ; EA 5B E0 00 F0
```

Five bytes: opcode, then **offset** (2 bytes, low first), then **segment** (2 bytes, low first).
This is the instruction at the reset vector (Chapter 12 §4).

In NASM, to jump to a far label:

```asm
        jmp  far [fptr]         ; indirect through a 32-bit pointer in memory
fptr:   dw   0xE05B             ; offset
        dw   0xF000             ; segment
```

### 2.4 Indirect jumps — jump tables

The reason indirect jumps exist:

```asm
; Dispatch on a value 0-3 in BL.
        xor  bh, bh
        mov  bl, [choice]
        shl  bx, 1              ; ×2 — each table entry is a word
        jmp  [table + bx]       ; FF A7 xx xx — jump to the address stored there

table:  dw   option0
        dw   option1
        dw   option2
        dw   option3
```

One instruction replaces four compares and four jumps, and it is O(1) in the number of cases. This
is how a `switch` statement compiles.

**Always range-check first.** An out-of-range index reads whatever follows the table and jumps into
it:

```asm
        cmp  bl, 3
        ja   .invalid           ; unsigned compare — catches negatives too
```

---

## 3. The range limit that will bite you

**Every conditional jump on the 8086 is short-only: −128 … +127 bytes.**

There is no near conditional jump. `JZ` cannot reach further than 127 bytes forward, full stop. (The
80386 added near conditional jumps with a `0F 8x` two-byte opcode.)

When a target is out of range, NASM says:

```
error: short jump is out of range
```

### 3.1 The fix

Invert the condition and jump over an unconditional jump:

```asm
; This fails if 'faraway' is more than 127 bytes ahead:
        jz   faraway

; This always works:
        jnz  .skip
        jmp  faraway            ; near jump — reaches the whole segment
.skip:
```

Three bytes more, and 15 + 4 = 19 clocks in the taken case instead of 16. Worth knowing before you
need it, because the error message arrives late in a long function.

### 3.2 Counting the range

The displacement is measured from the *end* of the 2-byte jump, so:

```
   backwards:  up to 128 bytes before the instruction after the jump
   forwards:   up to 127 bytes after it
```

Roughly 40–60 instructions either way. Long loop bodies exceed it.

---

## 4. The conditional jumps

Sixteen opcodes, `0x70`–`0x7F`, each two bytes, each testing a flag condition.

**Taken: 16 clocks. Not taken: 4 clocks.** The four-fold difference is the queue flush (Chapter 6
§3.3), and it is the basis of the optimisation in §8.

### 4.1 The complete table

| Opcode | Mnemonics | Jumps if | Flags tested |
|--------|-----------|----------|--------------|
| `70` | `JO` | overflow | `OF = 1` |
| `71` | `JNO` | no overflow | `OF = 0` |
| `72` | `JB` `JNAE` `JC` | below / carry | `CF = 1` |
| `73` | `JNB` `JAE` `JNC` | not below / no carry | `CF = 0` |
| `74` | `JE` `JZ` | equal / zero | `ZF = 1` |
| `75` | `JNE` `JNZ` | not equal / not zero | `ZF = 0` |
| `76` | `JBE` `JNA` | below or equal | `CF = 1 or ZF = 1` |
| `77` | `JA` `JNBE` | above | `CF = 0 and ZF = 0` |
| `78` | `JS` | sign (negative) | `SF = 1` |
| `79` | `JNS` | no sign (positive) | `SF = 0` |
| `7A` | `JP` `JPE` | parity even | `PF = 1` |
| `7B` | `JNP` `JPO` | parity odd | `PF = 0` |
| `7C` | `JL` `JNGE` | less (signed) | `SF ≠ OF` |
| `7D` | `JGE` `JNL` | greater or equal (signed) | `SF = OF` |
| `7E` | `JLE` `JNG` | less or equal (signed) | `ZF = 1 or SF ≠ OF` |
| `7F` | `JG` `JNLE` | greater (signed) | `ZF = 0 and SF = OF` |

The multiple mnemonics are the same opcode. `JE` and `JZ` both assemble to `74`; use whichever reads
better at the call site.

### 4.2 The naming scheme

Once you see it, the sixteen names become four.

```
   ABOVE / BELOW   ->  UNSIGNED comparisons  (test CF and ZF)
   GREATER / LESS  ->  SIGNED comparisons    (test SF, OF and ZF)
   N               ->  "not"
   E               ->  "or equal"
```

So:

```
   JA    jump if above                  unsigned  >
   JAE   jump if above or equal         unsigned  >=
   JB    jump if below                  unsigned  <
   JBE   jump if below or equal         unsigned  <=
   JG    jump if greater                signed    >
   JGE   jump if greater or equal       signed    >=
   JL    jump if less                   signed    <
   JLE   jump if less or equal          signed    <=
```

**Memorise this distinction. It is the most consequential thing in the chapter.**

### 4.3 Why they differ — a concrete case

```asm
        mov  ax, 0xFFFF         ; 65535 unsigned, −1 signed
        mov  bx, 0x0001         ; 1
        cmp  ax, bx
        ja   .unsigned_bigger   ; TAKEN:  65535 > 1
        jg   .signed_bigger     ; NOT taken: −1 < 1
```

Same bits, two correct-but-opposite answers. The processor computed `AX − BX` once and set both
`CF` and `OF`; the jump you chose decides which reading applies.

**Rule:** if the values are addresses, sizes, counts or characters, they are unsigned — use
`JA`/`JB`. If they can be negative, they are signed — use `JG`/`JL`.

### 4.4 The classic bug

```asm
; Bubble sort inner comparison — WRONG for signed data
        mov  al, [si]
        cmp  al, [si+1]
        jbe  .no_swap           ; unsigned!  −1 (0xFF) compares as 255
```

Sorts `[-1, 5]` into `[5, -1]`. Use `jle` for signed data. This exact bug is why Chapter 41 sorts
twice, once each way.

---

## 5. `JCXZ`

```asm
        jcxz label              ; E3 cb — 2 bytes, short range only
```

Jumps if `CX = 0`. **It does not test any flag** — it examines `CX` directly.

Clocks: 18 taken, 6 not taken.

### 5.1 Why it exists

`LOOP` decrements `CX` *then* tests, so a loop entered with `CX = 0` executes **65,536 times**:

```asm
        mov  cx, 0
.next:  ; ... body ...
        loop .next              ; CX: 0 -> 0xFFFF -> ... -> 0.  65,536 iterations!
```

`JCXZ` guards against it:

```asm
        jcxz .done              ; skip the loop entirely if CX is zero
.next:  ; ... body ...
        loop .next
.done:
```

**Put a `JCXZ` in front of every `LOOP` whose count comes from outside your control.** It is two
bytes and it prevents a class of bug that manifests as a hang.

The same applies to `REP`-prefixed string instructions — but there the processor checks for you
(Chapter 29 §3), so `REP MOVSB` with `CX = 0` correctly does nothing.

---

## 6. The `LOOP` family

Three instructions, all two bytes, all short-range only.

| Instruction | Opcode | Action | Clocks (taken / not) |
|-------------|--------|--------|---------------------|
| `LOOP` | `E2 cb` | `CX--`; jump if `CX ≠ 0` | 17 / 5 |
| `LOOPE` / `LOOPZ` | `E1 cb` | `CX--`; jump if `CX ≠ 0` **and** `ZF = 1` | 18 / 6 |
| `LOOPNE` / `LOOPNZ` | `E0 cb` | `CX--`; jump if `CX ≠ 0` **and** `ZF = 0` | 19 / 5 |

### 6.1 `LOOP`

```asm
        mov  cx, 10
.next:
        ; body — executed exactly 10 times
        loop .next
```

Three facts:

**`LOOP` decrements `CX` before testing.** So it runs the body `CX` times, and `CX` is 0 at the end.

**`LOOP` does not affect any flag.** That is what makes it usable inside a multi-precision carry
chain (Chapter 22 §8.3).

**`LOOP` is short-range.** A long loop body gets `error: short jump is out of range`, and the fix is:

```asm
        dec  cx
        jnz  .next              ; JNZ is also short-range...
; or, for a really long body:
        dec  cx
        jz   .done
        jmp  .next              ; near jump
.done:
```

### 6.2 `LOOP` versus `DEC`/`JNZ`

```asm
        loop .next              ; 2 bytes, 17 clocks
; versus
        dec  cx                 ; 1 byte,  2 clocks
        jnz  .next              ; 2 bytes, 16 clocks  -> 3 bytes, 18 clocks
```

On an 8086 they are almost identical — `LOOP` is one byte shorter and one clock faster. On the 80486
and later, `LOOP` became much slower than `DEC`/`JNZ`, which is why modern x86 code never uses it.
For 8086 work, use `LOOP`.

The one real difference: `DEC` **sets flags** and `LOOP` does not. Inside a carry chain, `LOOP` is
required.

### 6.3 `LOOPE` and `LOOPNE` — search loops

These combine a count limit with an early exit.

**`LOOPNE` — "keep looping while not equal":** used to search for a match.

```asm
; Find the first byte equal to AL in a buffer of CX bytes at [SI].
        mov  cx, count
        mov  si, buffer
        dec  si                 ; pre-decrement, because we INC at the top
.next:
        inc  si
        cmp  al, [si]
        loopne .next            ; continue while CX != 0 AND ZF = 0
        jne  .not_found         ; fell out because CX hit 0
        ; SI points at the match
```

**`LOOPE` — "keep looping while equal":** used to find the first *difference*.

```asm
; Compare two buffers, stop at the first mismatch.
.next:
        mov  al, [si]
        cmp  al, [di]
        inc  si
        inc  di
        loope .next             ; continue while equal and count remains
        jne  .differ
```

In practice `REPE CMPSB` (Chapter 29) does this better and faster. `LOOPE`/`LOOPNE` are for cases
where the body is more complex than a single string operation.

### 6.4 The loop patterns

```asm
; --- count down, N times ---
        mov  cx, N
.next:  ; body
        loop .next

; --- with an index that counts up ---
        mov  cx, N
        xor  si, si
.next:  mov  al, [array+si]
        ; body
        inc  si
        loop .next

; --- guarded against CX = 0 ---
        jcxz .done
.next:  ; body
        loop .next
.done:

; --- nested loops: CX must be saved ---
        mov  cx, OUTER
.outer:
        push cx                 ; the inner loop needs CX
        mov  cx, INNER
.inner: ; body
        loop .inner
        pop  cx                 ; restore the outer count
        loop .outer

; --- a while loop (test at the top) ---
.while: cmp  word [count], 0
        jle  .done
        ; body
        dec  word [count]
        jmp  .while
.done:

; --- a do-while loop (test at the bottom) ---
.do:    ; body
        cmp  al, 0
        jne  .do
```

**Nested loops and `CX`.** This is the commonest beginner bug in Part IV: the inner loop destroys the
outer loop's count. Push it, or use a different register and `DEC`/`JNZ` for the outer loop.

---

## 7. Building higher-level control flow

Assembly has no `if`, `while` or `for`. Here is how each maps.

### 7.1 `if`

```c
if (ax > bx) { X } else { Y }
```

```asm
        cmp  ax, bx
        jle  .else              ; NOTE: invert the condition
        ; X
        jmp  .endif
.else:
        ; Y
.endif:
```

**Always invert.** You jump *past* the code you want to run when the condition is false.

### 7.2 `while`

```c
while (ax < 10) { X }
```

```asm
.while: cmp  ax, 10
        jge  .endwhile
        ; X
        jmp  .while
.endwhile:
```

### 7.3 `for`

```c
for (i = 0; i < 10; i++) { X }
```

```asm
        xor  si, si
.for:   cmp  si, 10
        jge  .endfor
        ; X
        inc  si
        jmp  .for
.endfor:
```

or, when the count is known and `CX` is free, the far better:

```asm
        mov  cx, 10
        xor  si, si
.for:   ; X
        inc  si
        loop .for
```

### 7.4 Compound conditions

```c
if (ax > 5 && ax < 10) { X }
```

```asm
        cmp  ax, 5
        jle  .skip              ; short-circuit: fail the first test -> skip
        cmp  ax, 10
        jge  .skip
        ; X
.skip:
```

```c
if (ax == 1 || ax == 5) { X }
```

```asm
        cmp  ax, 1
        je   .do_it             ; short-circuit: succeed -> go
        cmp  ax, 5
        jne  .skip
.do_it:
        ; X
.skip:
```

`&&` chains jumps to the *failure* label; `||` chains jumps to the *success* label. That is the whole
pattern.

---

## 8. Optimisation: arrange the common case to fall through

Taken = 16 clocks. Not taken = 4 clocks. So:

```asm
; If the error case is rare, write it this way:
        cmp  ax, 0
        jne  .ok                ; TAKEN almost every time — 16 clocks
        call handle_error
.ok:

; Better — the common path falls through:
        cmp  ax, 0
        je   .error             ; NOT taken almost every time — 4 clocks
        ; common path continues here
        ...
        jmp  .done
.error: call handle_error
.done:
```

Twelve clocks saved per iteration. In a loop executed a million times, that is 2.4 seconds at 5 MHz.

The same reasoning says: **put the loop-back jump at the bottom**, which is what `LOOP` naturally
does. A loop that tests at the top and jumps back at the bottom pays for two jumps per iteration.

---

## 9. Worked program — a jump table menu

```asm
; menu.asm — read a digit and dispatch through a jump table
; nasm -f bin menu.asm -o menu.com
        org  0x100

start:
        mov  dx, prompt
        mov  ah, 0x09
        int  0x21

        mov  ah, 0x01           ; DOS: read a character, echo it
        int  0x21               ; AL = the character

        sub  al, '1'            ; '1'..'4' -> 0..3
        cmp  al, 3
        ja   .invalid           ; UNSIGNED: catches both < '1' and > '4',
                                ;   because '0'-'1' wraps to 0xFF

        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — table entries are words
        jmp  [table + bx]       ; dispatch

.invalid:
        mov  dx, badmsg
        jmp  .say

opt1:   mov  dx, msg1
        jmp  .say
opt2:   mov  dx, msg2
        jmp  .say
opt3:   mov  dx, msg3
        jmp  .say
opt4:   mov  dx, msg4

.say:
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C00
        int  0x21

table:  dw   opt1, opt2, opt3, opt4

prompt: db   0x0D, 0x0A, 'Choose 1-4: $'
msg1:   db   0x0D, 0x0A, 'You chose addition.', 0x0D, 0x0A, '$'
msg2:   db   0x0D, 0x0A, 'You chose subtraction.', 0x0D, 0x0A, '$'
msg3:   db   0x0D, 0x0A, 'You chose multiplication.', 0x0D, 0x0A, '$'
msg4:   db   0x0D, 0x0A, 'You chose division.', 0x0D, 0x0A, '$'
badmsg: db   0x0D, 0x0A, 'Not a valid choice.', 0x0D, 0x0A, '$'
```

### 9.1 Notes

**`ja` not `jg`.** After `sub al, '1'`, an input of `'0'` gives `0xFF`. As a *signed* byte that is
−1, which `jg 3` would let through; as an *unsigned* byte it is 255, which `ja 3` correctly rejects.
The unsigned test catches both ends of the range with one comparison. This is a standard trick and
worth remembering.

**`shl bx, 1` not `shl bx, 2`.** Entries are words (2 bytes), so multiply the index by 2. If the
table held far pointers (4 bytes) you would shift twice.

**`jmp [table + bx]`** is a near indirect jump: it reads a word from memory and puts it in `IP`.
`table` must contain *offsets*, which is what `dw opt1` produces.

---

## 10. Summary

```
  displacement is measured from the address of the NEXT instruction
  jmp $  ->  EB FE  (displacement −2)

  JMP short  EB cb   2 bytes, −128..+127          15 clocks
  JMP near   E9 cw   3 bytes, anywhere in segment 15
  JMP far    EA cd   5 bytes, offset then segment 15
  JMP r/m16  FF /4   near indirect — jump tables  11 / 18+EA
  JMP m32    FF /5   far indirect                 24+EA

  ALL CONDITIONAL JUMPS ARE SHORT-ONLY: −128..+127.  Two bytes, 70h-7Fh.
     out of range -> invert the condition and jump over a near JMP
     taken 16 clocks, not taken 4 clocks

  UNSIGNED: JA JAE JB JBE (and JC JNC JE JNE)
  SIGNED:   JG JGE JL JLE
     using the wrong family is the most common logic bug in 8086 code

  JCXZ   E3   jumps if CX = 0 — tests no flag.  Guard every LOOP with it.
  LOOP   E2   CX--, jump if CX != 0.  NO FLAGS AFFECTED.  17/5 clocks.
  LOOPE  E1   ... and ZF = 1
  LOOPNE E0   ... and ZF = 0

  LOOP with CX = 0 runs 65,536 times.
  Nested loops must PUSH CX.
  Arrange the common case to FALL THROUGH: 4 clocks instead of 16.
```

---

## Exercises

**27.1** `jmp` at offset `0x0200` assembles to `EB 20`. What is the target offset?

**27.2** What two bytes does `jmp $` assemble to, and why is the displacement −2?

**27.3** A label is 200 bytes ahead. Can `jz` reach it? If not, write the code that achieves the same
effect.

**27.4** `AX = 0x8000`, `BX = 0x7FFF`. After `cmp ax, bx`, state whether each of `JA`, `JAE`, `JB`,
`JG`, `JGE`, `JL`, `JE` is taken. Explain the `JA`/`JG` disagreement.

**27.5** Which conditional jump follows `cmp al, bl` to jump when `AL` is strictly greater, treating
both as (a) unsigned, (b) signed?

**27.6** What is wrong with this code, and what does it actually do?

```asm
        mov  cx, 0
.next:  inc  word [total]
        loop .next
```

**27.7** Fix Exercise 27.6 with one extra instruction.

**27.8** Why does `LOOP` not affect the flags? Give a fragment that would break if it did.

**27.9** Write a nested loop that prints a 5 × 3 grid of asterisks, handling `CX` correctly.

**27.10** Translate to assembly:

```c
if (ax >= 10 && bx != 0) { cx = 1; } else { cx = 0; }
```

**27.11** Translate to assembly, using `LOOP`:

```c
sum = 0;
for (i = 0; i < 20; i++) sum += array[i];
```

**27.12** Rewrite this so the common case falls through, and state the clock saving per iteration
assuming the jump is taken 99% of the time.

```asm
.next:  cmp  al, 0
        jne  .continue
        call rare_case
.continue:
        ; ... body ...
        loop .next
```

**27.13** A jump table has 6 entries. Write the complete dispatch code, including the range check,
for an index 0–5 in `AL`.

**27.14** Why does the menu program in §9 use `ja` rather than `jg` after `sub al, '1'`? Give the
input that would break the `jg` version.

**27.15** `LOOPNE` is used to search for a byte. After the loop, how do you tell "found" from
"ran out of count"?

Answers in [Appendix H](H-exercise-solutions.md#chapter-27).

---

[← Shifts and rotates](26-shift-rotate.md) · [Contents](README.md) · [Next: Procedures and the stack →](28-procedures-and-stack.md)
