# Chapter 32 — Instruction timing

[← Interrupts](31-interrupts.md) · [Contents](README.md) · [Next: NASM and program structure →](33-nasm-directives.md)

---

## Goal

Count clocks honestly. By the end you should be able to take a loop, work out how long it takes on a
real 8086, and decide which change is worth making — with numbers rather than folklore.

This closes Part III. Everything here is measured against a 5 MHz 8086 with no wait states, where
one clock is **200 ns**.

---

## 1. The three components of an instruction's cost

```
   total  =  base clocks  +  effective-address clocks  +  memory-access penalties
```

### 1.1 Base clocks

The number in the datasheet for the register-only form. `ADD reg, reg` = 3. `MUL r16` = 118–133.
Appendix A lists every one.

### 1.2 Effective-address clocks

Added whenever an operand is in memory (Chapter 19 §7):

| Addressing mode | Clocks |
|-----------------|--------|
| `[disp16]` direct | 6 |
| `[BX]` `[BP]` `[SI]` `[DI]` | 5 |
| `[BX+disp]` `[BP+disp]` `[SI+disp]` `[DI+disp]` | 9 |
| `[BX+SI]` `[BP+DI]` | 7 |
| `[BX+DI]` `[BP+SI]` | 8 |
| `[BX+SI+disp]` `[BP+DI+disp]` | 11 |
| `[BX+DI+disp]` `[BP+SI+disp]` | 12 |
| any of the above, with a segment override | **+2** |

### 1.3 Memory-access penalties

**+4 clocks for every word access at an odd address** (Chapter 10 §4), because it becomes two bus
cycles.

On an **8088**, +4 clocks for *every* word access, aligned or not.

### 1.4 A worked total

```asm
        add  ax, [es:bx+si+4]
```

```
   base: ADD reg, mem                    9
   EA:   [BX+SI+disp]                   11
   segment override                      2
   word at an odd address (if so)        4
                                        ───
   total                                26 clocks  =  5.2 µs at 5 MHz
```

Against `add ax, bx` at 3 clocks. **Nearly nine times the cost**, for the same arithmetic.

---

## 2. What the tables do not tell you

Intel's clock counts assume **the instruction bytes are already in the queue**. Three things break
that assumption.

### 2.1 Queue refill after a jump

Every taken jump, call, return and interrupt flushes the queue (Chapter 6 §3.3). The next
instruction must be fetched from cold: a full bus cycle (4 clocks) before decoding can even start.

This is already folded into the published figures — `JMP near` is listed at 15 clocks, of which
about 11 is the flush-and-refetch. It is *not* folded into the cost of the instruction *after* the
jump, which may still stall.

### 2.2 Queue starvation

A run of short, fast instructions consumes the queue faster than the BIU can refill it:

```asm
        inc  ax                 ; 1 byte, 2 clocks
        inc  bx                 ; 1 byte, 2 clocks
        inc  cx                 ; 1 byte, 2 clocks
```

Three instructions, 3 bytes, 6 clocks of execution — but the BIU needs 4 clocks to fetch 2 bytes, so
8 clocks to supply them. **The EU outruns the BIU** and stalls.

On an 8086 this only matters for the very fastest instructions. On an **8088** it is the normal
state of affairs (Chapter 18 §4.1).

### 2.3 Bus contention with operand accesses

Every memory operand access is a bus cycle that the BIU cannot use for prefetching. A loop that
touches memory every iteration keeps the queue permanently short.

### 2.4 The practical rule

> **Published clock counts are accurate to about ±10% for typical code on an 8086, and optimistic by
> 20–40% on an 8088.**

Use them for *comparing* two implementations, which is what they are good for. Do not use them to
predict absolute wall-clock time.

---

## 3. The cost table you will actually use

Ranked, so the shape is visible:

| Operation | Clocks | Relative |
|-----------|--------|----------|
| `MOV reg, reg` | 2 | 1× |
| `INC`/`DEC reg16` | 2 | 1× |
| `ADD`/`SUB`/`AND`/`OR`/`XOR reg, reg` | 3 | 1.5× |
| `CMP reg, reg` | 3 | 1.5× |
| `SHL`/`SHR reg, 1` | 2 | 1× |
| `Jcc` not taken | 4 | 2× |
| `MOV reg, imm` | 4 | 2× |
| `MOV reg, [BX]` | 8 + 5 = 13 | 6.5× |
| `ADD reg, [BX]` | 9 + 5 = 14 | 7× |
| `ADD [BX], reg` | 16 + 5 = 21 | 10× |
| `Jcc` taken | 16 | 8× |
| `LOOP` taken | 17 | 8.5× |
| `PUSH reg` | 11 | 5.5× |
| `POP reg` | 8 | 4× |
| `CALL near` | 19 | 9.5× |
| `RET near` | 16 | 8× |
| `MOVSB` (with `REP`) | 17 per byte | 8.5× |
| `MUL r8` | 70–77 | 35× |
| `MUL r16` | 118–133 | 60× |
| `DIV r8` | 80–90 | 42× |
| `DIV r16` | 144–162 | 76× |
| `INT n` | 51 | 25× |
| `IRET` | 24 | 12× |
| `AAM` | 83 | 41× |

**Four bands, and they are what you optimise against:**

```
   2-4 clocks    register operations              — free
   8-21 clocks   memory operations, jumps         — the normal cost
   50-90 clocks  MUL8, DIV8, INT                  — avoid in loops
   118-184       MUL16, DIV16                     — avoid entirely if you can
```

---

## 4. Worked analysis — summing an array

The same task, five ways. Array of 1000 words at `array`.

### Version 1 — the naive one

```asm
        mov  cx, 1000
        mov  si, 0
        mov  word [total], 0
.next:
        mov  ax, [array+si]     ; 8 + 9  (index + disp)   = 17
        add  [total], ax        ; 16 + 6 (direct)         = 22
        add  si, 2              ; 4
        loop .next              ; 17
                                ;                          ───
                                ;                          60 per iteration
```

**60,000 clocks = 12.0 ms.**

### Version 2 — accumulate in a register

```asm
        mov  cx, 1000
        mov  si, 0
        xor  ax, ax
.next:
        add  ax, [array+si]     ; 9 + 9   = 18
        add  si, 2              ; 4
        loop .next              ; 17
                                ;          ───
                                ;          39 per iteration
        mov  [total], ax
```

**39,000 clocks = 7.8 ms.** The single change of keeping the running total in `AX` saved 35%.

### Version 3 — cheaper addressing

`[array+si]` costs 9 (index + displacement). `[bx]` costs 5.

```asm
        mov  cx, 1000
        mov  bx, array
        xor  ax, ax
.next:
        add  ax, [bx]           ; 9 + 5   = 14
        inc  bx                 ; 2
        inc  bx                 ; 2
        loop .next              ; 17
                                ;          ───
                                ;          35 per iteration
```

**35,000 clocks = 7.0 ms.** Note `inc bx` twice (2+2 = 4) rather than `add bx, 2` (4) — identical
here, but the `INC` form preserves `CF`, which matters in a carry chain.

### Version 4 — string instructions

```asm
        cld
        mov  cx, 1000
        mov  si, array
        xor  ax, ax
        xor  dx, dx
.next:
        lodsw                   ; 12
        add  dx, ax             ; 3
        loop .next              ; 17
                                ;          ───
                                ;          32 per iteration
```

**32,000 clocks = 6.4 ms.**

### Version 5 — unrolled

The loop overhead (`LOOP` at 17 clocks) is half the cost. Unroll four times:

```asm
        cld
        mov  cx, 250            ; 1000 / 4
        mov  si, array
        xor  dx, dx
.next:
        lodsw                   ; 12
        add  dx, ax             ; 3
        lodsw                   ; 12
        add  dx, ax             ; 3
        lodsw                   ; 12
        add  dx, ax             ; 3
        lodsw                   ; 12
        add  dx, ax             ; 3
        loop .next              ; 17
                                ;          ───
                                ;          77 per iteration, for FOUR elements
                                ;          = 19.25 per element
```

**19,250 clocks = 3.85 ms.**

### The summary

| Version | Clocks/element | Total | Speedup |
|---------|---------------|-------|---------|
| 1 — naive | 60 | 12.0 ms | 1.0× |
| 2 — register accumulator | 39 | 7.8 ms | 1.5× |
| 3 — cheaper addressing | 35 | 7.0 ms | 1.7× |
| 4 — `LODSW` | 32 | 6.4 ms | 1.9× |
| 5 — unrolled ×4 | 19.25 | 3.85 ms | **3.1×** |

Three times faster, with no change to what the code computes. Every step came from the tables, not
from guesswork.

**Where to stop:** version 2 is the one that matters — it is a one-line change for 35%. Versions 3
and 4 cost readability for 10% each. Version 5 costs a lot of readability and a division of the
count, for another 40%. Do version 2 always; do version 5 only when you have measured that this loop
matters.

---

## 5. The optimisation rules, ranked

### 5.1 Keep working values in registers

The single biggest win. A memory operand costs 5–12 clocks of address calculation plus a bus cycle,
every time.

```asm
        add  [total], ax        ; 22 clocks
        add  bx, ax             ;  3 clocks
```

### 5.2 Choose the cheapest addressing mode

```
   [BX]           5
   [BX+SI]        7
   [array+SI]     9
   [BX+SI+4]     11
   [BX+DI+4]     12
```

If a loop uses `[array+si]` (9), advancing a pointer and using `[bx]` (5) saves 4 clocks per access.

### 5.3 Prefer `BX+SI` and `BP+DI` over `BX+DI` and `BP+SI`

One clock, free. Just a matter of which index register you choose.

### 5.4 Arrange conditional jumps to fall through

Taken 16, not taken 4. Twelve clocks per iteration (Chapter 27 §8).

### 5.5 Replace `MUL` and `DIV` by shifts where the operand is a constant power of two

```asm
        mov  bx, 8
        mul  bx                 ; 118-133 clocks
; versus
        shl  ax, 1
        shl  ax, 1
        shl  ax, 1              ; 6 clocks
```

**Twenty times faster.** This is the largest single-instruction win available.

### 5.6 Use word string operations

`REP MOVSW` is half the clocks of `REP MOVSB` on an 8086 (Chapter 29 §3.3). No benefit on an 8088.

### 5.7 Align word data

`align 2` before every `dw`. Four clocks per access, free (Chapter 10 §4.4).

### 5.8 Avoid segment overrides in loops

Two clocks and one byte per access. Rearrange the segment registers so the defaults are right.

### 5.9 Unroll only when measured

Costs code size and clarity. Buys the loop overhead, which is 17–21 clocks per iteration.

### 5.10 Do not call DOS in a loop

`INT 21h` is 51 clocks before the handler starts, and DOS's character output is hundreds more.
Writing directly to `0xB800:0000` is roughly 50 times faster for full-screen output (Chapter 45).

---

## 6. The queue-flush cost, quantified

A loop with a conditional jump at the bottom:

```asm
.next:  ; body
        dec  cx
        jnz  .next              ; taken 999 times, not taken once
```

The taken cost (16) includes the queue flush. Compare the same loop written with the test at the top
and an unconditional jump at the bottom:

```asm
.next:  cmp  cx, 0
        jz   .done              ; 4 (not taken)
        ; body
        dec  cx
        jmp  .next              ; 15 (always taken)
.done:
```

```
   bottom-test:  16 per iteration
   top-test:     4 + 15 = 19 per iteration
```

Three clocks per iteration, purely from having two control transfers instead of one. **Always test at
the bottom** when the loop is guaranteed to run at least once — and use `JCXZ` to guard the
zero-iteration case (Chapter 27 §5).

---

## 7. Measuring on real hardware

Clock counting is a model. To measure, use the 8253 timer (Chapter 48).

```asm
; ---------------------------------------------------------------
; Measure elapsed time using timer channel 0, which counts DOWN
; at 1,193,182 Hz on a PC.
; ---------------------------------------------------------------
        cli
        mov  al, 0x00           ; latch counter 0
        out  0x43, al
        in   al, 0x40           ; low byte
        mov  bl, al
        in   al, 0x40           ; high byte
        mov  bh, al             ; BX = the count before
        sti

        ; ---- the code being measured ----
        call the_thing
        ; ---------------------------------

        cli
        mov  al, 0x00
        out  0x43, al
        in   al, 0x40
        mov  cl, al
        in   al, 0x40
        mov  ch, al             ; CX = the count after
        sti

        mov  ax, bx
        sub  ax, cx             ; the counter counts DOWN, so before − after
        ; AX = elapsed ticks at 1.193182 MHz = 0.838 µs each
```

Resolution is 0.838 µs — about four 8086 clocks. For anything shorter, run it 1000 times in a loop
and divide.

**This does not work under DOSBox**, which does not model timing faithfully. Chapter 1 §9.

---

## 8. Instruction size matters too

On an 8086, and especially an 8088, **shorter code is faster code**, because the queue refills
faster.

| Instruction | Bytes |
|-------------|-------|
| `xor ax, ax` | 2 |
| `mov ax, 0` | 3 |
| `inc ax` | 1 |
| `add ax, 1` | 3 |
| `add ax, 5` (accumulator form) | 3 |
| `add bx, 5` (sign-extended imm8) | 3 |
| `add bx, 500` (full imm16) | 4 |
| `mov ax, [addr]` (accumulator form) | 3 |
| `mov bx, [addr]` (general form) | 4 |
| `mov ax, [bx]` | 2 |
| `mov ax, [bp]` | 3 (the `[BP]` anomaly, Chapter 20 §3.4) |
| `push ax` | 1 |
| `jmp short` | 2 |
| `jmp near` | 3 |

Four habits that cost nothing:

- `xor reg, reg` instead of `mov reg, 0`
- `inc`/`dec` instead of `add`/`sub` by 1
- keep the hot value in `AX` for the accumulator short forms
- `jmp short` for nearby forward jumps

---

## 9. A checklist for a slow loop

In order, because the first two usually finish the job:

1. **Is anything in memory that could be in a register?** Move it.
2. **Is there a `MUL` or `DIV` by a constant power of two?** Replace with shifts.
3. **Is the addressing mode more expensive than it needs to be?** Walk a pointer instead of
   indexing.
4. **Is the common case taking the branch?** Invert it.
5. **Is word data at an odd address?** `align 2`.
6. **Is there a segment override inside the loop?** Rearrange the segments.
7. **Are you calling DOS or the BIOS per element?** Batch it, or write to hardware directly.
8. **Is there a `REP MOVSB` that could be `REP MOVSW`?** (8086 only.)
9. **Only now**: consider unrolling.

---

## 10. Summary

```
  total = base + EA + misalignment penalty (+2 for a segment override)

  at 5 MHz, one clock = 200 ns

  register op            2-4 clocks
  memory op              8-21
  taken jump             15-17     not taken  4
  CALL/RET               19 / 16
  MUL r16                118-133   DIV r16   144-162
  INT n                  51

  EA: [BX] 5 · [BX+SI] 7 · [BX+DI] 8 · [disp] 6 · [BX+d] 9 · [BX+SI+d] 11 · [BX+DI+d] 12
  odd-address word access: +4 clocks on an 8086; every word costs +4 on an 8088

  published counts assume the bytes are already queued; they are
  accurate to ~10% on an 8086 and optimistic by 20-40% on an 8088

  biggest wins, in order:
     1. accumulate in registers, not memory        (up to 7x per operation)
     2. shifts instead of MUL/DIV by 2^n           (20x)
     3. cheaper addressing mode                    (up to 2.4x on the EA)
     4. fall through on the common branch          (4 vs 16)
     5. align word data                            (free)
     6. MOVSW instead of MOVSB                     (2x, 8086 only)
     7. unroll                                     (removes 17-21 per iteration)
```

---

## Exercises

**32.1** Compute the total clocks for each: `mov ax, bx`, `mov ax, [bx]`, `mov ax, [bx+si]`,
`mov ax, [bx+di+8]`, `mov ax, [es:bx+si+8]`.

**32.2** `add [count], ax` where `count` is at a direct address. Total clocks? Now rewrite it to use
a register accumulator and give the saving over 1000 iterations.

**32.3** A word variable is at an odd offset and is read a million times. How many clocks are wasted?
How long is that at 5 MHz?

**32.4** Which is faster and by how much: `mov ax, [bx+si]` or `mov ax, [bx+di]`? Why?

**32.5** A loop multiplies `AX` by 16 using `MUL`. Rewrite it with shifts and give the clock saving
per iteration.

**32.6** A conditional jump inside a loop is taken 95% of the time. Rewrite the loop so it falls
through in the common case and compute the saving over 10,000 iterations.

**32.7** `REP MOVSB` copies 4000 bytes. Compute the clocks. Now compute it for `REP MOVSW`. What is
the answer on an 8088?

**32.8** Analyse this loop and give the clocks per iteration:

```asm
.next:  mov  al, [si]
        cmp  al, 'a'
        jb   .skip
        cmp  al, 'z'
        ja   .skip
        sub  al, 0x20
        mov  [si], al
.skip:  inc  si
        loop .next
```

**32.9** Rewrite the loop in 32.8 using `LODSB` and `STOSB` and compute the new figure.

**32.10** A program calls a 3-instruction procedure a million times. Compute the `CALL`/`RET`
overhead alone, in clocks and in seconds at 5 MHz. Would inlining be worth it?

**32.11** Why do published clock counts understate the cost of code on an 8088 more than on an 8086?

**32.12** Unroll a 1000-iteration `LODSW`/`ADD` loop by 8 and compute the clocks per element. Compare
with the ×4 version in §4.

**32.13** A loop writes 2000 characters to the screen with `INT 21h` function 02h. Estimate the cost
and compare with `REP STOSW` into `0xB800:0000`.

Answers in [Appendix H](H-exercise-solutions.md#chapter-32).

---

[← Interrupts](31-interrupts.md) · [Contents](README.md) · [**Part IV begins: NASM and program structure →**](33-nasm-directives.md)
