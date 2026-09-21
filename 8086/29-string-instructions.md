# Chapter 29 — String instructions

[← Procedures and the stack](28-procedures-and-stack.md) · [Contents](README.md) · [Next: Processor control →](30-processor-control.md)

---

## Goal

Five instructions — `MOVS`, `CMPS`, `SCAS`, `LODS`, `STOS` — plus the `REP` prefixes that turn them
into block operations. These are the 8086's most powerful instructions: one byte each, and with a
prefix they process up to 65,535 elements without any loop code at all.

They are also the most implicit instructions in the set. Nothing appears in the source but the
mnemonic; everything comes from `SI`, `DI`, `CX`, `DS`, `ES`, `AL`/`AX` and the direction flag.

---

## 1. The shared machinery

Every string instruction uses the same five things:

| Register | Role |
|----------|------|
| **`DS:SI`** | the **source** pointer. `DS` can be overridden |
| **`ES:DI`** | the **destination** pointer. `ES` **cannot** be overridden |
| **`CX`** | the repeat count, when a `REP` prefix is present |
| **`AL` / `AX`** | the data element, for `LODS`, `STOS` and `SCAS` |
| **`DF`** | direction: 0 = forwards (`SI`/`DI` increase), 1 = backwards |

### 1.1 The automatic adjustment

After every element, `SI` and/or `DI` change by:

```
   DF = 0 (CLD) :  +1 for byte operations, +2 for word operations
   DF = 1 (STD) :  −1 for byte operations, −2 for word operations
```

The processor knows the size from the instruction: `MOVSB` moves a byte and adjusts by 1; `MOVSW`
moves a word and adjusts by 2.

### 1.2 `CLD` first, always

```asm
        cld                     ; DF = 0 — forwards
```

**The convention everywhere is `DF = 0`.** DOS, the BIOS and every library assume it. If you use
`STD` for a backwards copy, clear it again immediately afterwards.

An interrupt handler that uses string instructions **must** `CLD` on entry, because it has no idea
what the interrupted code had set. Chapter 31 §7.

### 1.3 `ES` must be set up

`DI` is relative to `ES`, and in a `.COM` program DOS conveniently sets `ES = DS`, so it works
without thought. In an `.EXE` program `ES` points at the PSP, not your data, and every string
instruction writes to the wrong place until you fix it:

```asm
        mov  ax, ds
        mov  es, ax             ; ES = DS — essential in an .EXE
```

**This is the single commonest string-instruction bug.**

---

## 2. The five instructions

### 2.1 `MOVSB` / `MOVSW` — move

```
   MOVSB :   ES:[DI] <- DS:[SI] ;  SI ±= 1 ;  DI ±= 1
   MOVSW :   ES:[DI] <- DS:[SI] ;  SI ±= 2 ;  DI ±= 2
```

Opcodes `A4` / `A5`. One byte. **18 clocks.**

**This is the only memory-to-memory move in the entire instruction set** (Chapter 21 §1.2).

```asm
        cld
        mov  si, source
        mov  di, dest
        mov  cx, 100
        rep  movsb              ; copy 100 bytes
```

### 2.2 `CMPSB` / `CMPSW` — compare

```
   CMPSB :   compare DS:[SI] with ES:[DI], set flags ;  SI ±= 1 ;  DI ±= 1
```

Opcodes `A6` / `A7`. **22 clocks.**

Note the direction of the comparison: it computes `[SI] − [DI]`, so after `CMPSB`, `JB` means "the
source byte is below the destination byte".

The pointers advance **whether or not** the bytes matched, so after the comparison they point one
element *past* the one compared.

### 2.3 `SCASB` / `SCASW` — scan

```
   SCASB :   compare AL with ES:[DI], set flags ;  DI ±= 1
   SCASW :   compare AX with ES:[DI], set flags ;  DI ±= 2
```

Opcodes `AE` / `AF`. **15 clocks.**

Only `DI` is used — `SI` is untouched. The comparison is `AL − [DI]`.

### 2.4 `LODSB` / `LODSW` — load

```
   LODSB :   AL <- DS:[SI] ;  SI ±= 1
   LODSW :   AX <- DS:[SI] ;  SI ±= 2
```

Opcodes `AC` / `AD`. **12 clocks.**

Only `SI`. Affects no flags. `REP LODSB` is legal but pointless — it would overwrite `AL` `CX` times
and leave only the last value.

`LODSB` is the standard way to walk a string:

```asm
.next:  lodsb                   ; AL = next character, SI advances
        or   al, al
        jz   .done
        ; ... process AL ...
        jmp  .next
.done:
```

Compare with `mov al, [si]` / `inc si` — two instructions, 3 bytes, 13 clocks, versus one
instruction, 1 byte, 12 clocks.

### 2.5 `STOSB` / `STOSW` — store

```
   STOSB :   ES:[DI] <- AL ;  DI ±= 1
   STOSW :   ES:[DI] <- AX ;  DI ±= 2
```

Opcodes `AA` / `AB`. **11 clocks.**

Only `DI`. Affects no flags. With `REP` it is the fastest way to fill memory:

```asm
        cld
        mov  di, buffer
        mov  cx, 1000
        mov  al, 0
        rep  stosb              ; zero 1000 bytes
```

---

## 3. The `REP` prefixes

Three prefix bytes, two distinct behaviours.

| Prefix | Byte | Repeat while | Used with |
|--------|------|--------------|-----------|
| `REP` | `F3` | `CX ≠ 0` | `MOVS`, `STOS`, (`LODS`, `INS`, `OUTS`) |
| `REPE` / `REPZ` | `F3` | `CX ≠ 0` **and** `ZF = 1` | `CMPS`, `SCAS` |
| `REPNE` / `REPNZ` | `F2` | `CX ≠ 0` **and** `ZF = 0` | `CMPS`, `SCAS` |

`REP` and `REPE` are the *same byte* (`F3`). The processor interprets it differently depending on
whether the instruction sets flags: `MOVS` and `STOS` do not, so only `CX` matters; `CMPS` and `SCAS`
do, so `ZF` matters too.

### 3.1 The exact sequence

```
   while CX != 0:
       CX = CX − 1
       execute the string operation
       if (this is CMPS or SCAS) and the ZF condition fails:
           break
```

Three details follow from that:

**`CX` is tested *before* each iteration**, so `REP` with `CX = 0` does nothing at all. Unlike
`LOOP` (Chapter 27 §5), no `JCXZ` guard is needed.

**`CX` is decremented *before* the operation**, so after a `REPNE SCASB` that found a match, `CX`
holds the number of elements *remaining* after the match.

**The pointers always advance**, including on the iteration that terminated the loop. So after a
successful `REPNE SCASB`, `DI` points one byte *past* the match. You almost always need `dec di`.

### 3.2 Clock counts

| Instruction | Alone | With `REP`, per element |
|-------------|-------|------------------------|
| `MOVSB`/`MOVSW` | 18 | 9 + **17** |
| `CMPSB`/`CMPSW` | 22 | 9 + **22** |
| `SCASB`/`SCASW` | 15 | 9 + **15** |
| `LODSB`/`LODSW` | 12 | 9 + 13 |
| `STOSB`/`STOSW` | 11 | 9 + **10** |

The `9 +` is a one-off setup cost. Compare `REP MOVSB` against a hand-written loop:

```asm
; REP MOVSB, 1000 bytes:   9 + 1000×17  =  17,009 clocks
;
; hand-written equivalent:
.next:  mov  al, [si]           ;  8+5 = 13
        mov  [di], al           ;  9+5 = 14
        inc  si                 ;  2
        inc  di                 ;  2
        loop .next              ; 17
                                ; = 48 clocks per byte
;                          1000 × 48  =  48,000 clocks
```

**Nearly three times faster, in one byte of code.**

### 3.3 `MOVSW` is twice as fast again

Each `MOVSW` moves two bytes for the same 17 clocks:

```
   REP MOVSB, 1000 bytes :  9 + 1000 × 17  =  17,009 clocks
   REP MOVSW,  500 words :  9 +  500 × 17  =   8,509 clocks
```

**Always use the word form when you can.** For an odd byte count, do the words and then one byte:

```asm
        mov  cx, count
        shr  cx, 1              ; CX = number of whole words; CF = the odd bit
        rep  movsw
        adc  cx, 0              ; CX = 1 if there was an odd byte, else 0
        rep  movsb              ; moves it, or does nothing
```

`adc cx, 0` after `shr` is a neat trick: `CX` is 0 after `rep movsw`, and `CF` holds the low bit that
`SHR` discarded, so `ADC` turns it into a count of 0 or 1.

**On an 8088 this optimisation is worthless** — its 8-bit bus makes a word move two bus cycles, so
`MOVSW` and `MOVSB` run at the same speed (Chapter 18 §4.3).

---

## 4. The segment asymmetry

| Pointer | Segment | Overridable? |
|---------|---------|--------------|
| `SI` (source) | `DS` | **yes** |
| `DI` (destination) | `ES` | **no** |

```asm
        es movsb                ; NASM: source becomes ES:SI; destination stays ES:DI
        cs movsb                ; source becomes CS:SI
```

The destination is hard-wired to `ES:DI` in the microcode. This is why `ES` must always be set up,
and why a copy *within* one segment needs `ES = DS`.

### 4.1 Copying between segments

This is what the asymmetry is *for*:

```asm
; Copy 80 characters from our data to the video buffer.
        cld
        mov  si, line           ; DS:SI = our string
        mov  ax, 0xB800
        mov  es, ax             ; ES:DI = the screen
        mov  di, 0
        mov  cx, 80
        rep  movsb
```

One segment for the source, another for the destination, no reloading in the loop.

---

## 5. The five idioms

### 5.1 Block copy

```asm
        cld
        mov  si, src
        mov  di, dst
        mov  cx, len
        shr  cx, 1
        rep  movsw
        adc  cx, 0
        rep  movsb
```

### 5.2 Block fill

```asm
        cld
        mov  di, buffer
        mov  cx, 1000
        mov  al, ' '
        rep  stosb              ; fill with spaces
```

For a word fill (e.g. character + attribute on screen):

```asm
        mov  ax, 0x0720         ; grey space
        mov  cx, 2000           ; 80 × 25
        rep  stosw
```

### 5.3 Search for a byte

```asm
; Find AL in the CX bytes at ES:DI.
        cld
        mov  di, buffer
        mov  cx, len
        mov  al, target
        repne scasb             ; repeat while NOT equal
        jne  .not_found         ; fell out because CX reached 0
        dec  di                 ; DI advanced past the match — back up
        ; DI now points at the matching byte
```

**Both the `jne` and the `dec di` are necessary.** `REPNE` stops for two different reasons and only
`ZF` distinguishes them.

### 5.4 Compare two blocks

```asm
; Compare CX bytes at DS:SI with ES:DI.
        cld
        mov  si, str1
        mov  di, str2
        mov  cx, len
        repe cmpsb              ; repeat while EQUAL
        je   .identical         ; ran to the end with everything matching
        ; SI-1 and DI-1 point at the first difference;
        ; the flags from the last CMPSB say which was larger
        jb   .str1_is_smaller
```

### 5.5 Walk a null-terminated string

```asm
        cld
        mov  si, text
.next:  lodsb
        or   al, al
        jz   .done
        ; ... process AL ...
        jmp  .next
.done:
```

`REP` cannot be used here, because the length is not known in advance. `LODSB` still saves an
instruction per character over `mov al, [si]` / `inc si`.

---

## 6. Overlapping copies and the direction flag

Copying a block onto an address that overlaps it is the one case where `STD` earns its keep.

### 6.1 The problem

```
   copy 10 bytes from offset 100 to offset 105 — they overlap by 5
```

Forwards (`DF = 0`), byte 100 is written to 105 — but 105 is a byte we still need to read. It has
been destroyed. The result is that the first 5 bytes get smeared across the whole destination.

### 6.2 The rule

```
   if destination > source and they overlap  ->  copy BACKWARDS (STD)
   otherwise                                  ->  copy forwards  (CLD)
```

Copying backwards:

```asm
        std                     ; DF = 1 — SI and DI DECREASE
        mov  si, src + len - 1  ; point at the LAST byte
        mov  di, dst + len - 1
        mov  cx, len
        rep  movsb
        cld                     ; ALWAYS restore DF = 0
```

Note that the pointers must start at the *end* of each block.

### 6.3 A general copy routine

```asm
; ---------------------------------------------------------------
; memmove — copy CX bytes from DS:SI to ES:DI, handling overlap.
;   Assumes DS = ES (a single-segment program).
;   Destroys: SI, DI, CX, flags. Restores DF = 0.
; ---------------------------------------------------------------
memmove:
        cmp  di, si
        jbe  .forwards          ; dst <= src -> forwards is safe

        ; dst > src: copy backwards
        add  si, cx
        dec  si
        add  di, cx
        dec  di
        std
        rep  movsb
        cld
        ret

.forwards:
        cld
        rep  movsb
        ret
```

This is exactly what C's `memmove` does, and why it exists alongside `memcpy`.

---

## 7. Worked program — string utilities

```asm
; strutil.asm — length, copy, compare and search, using string instructions
; nasm -f bin strutil.asm -o strutil.com
        org  0x100

start:
        cld                     ; forwards, once, for the whole program
        mov  ax, ds
        mov  es, ax             ; ES = DS (already true for a .COM, but be explicit)

        ; --- length of str1 ---
        mov  di, str1
        call strlen             ; CX = length
        mov  ax, cx
        call print_dec

        ; --- copy str1 to buffer ---
        mov  si, str1
        mov  di, buffer
        call strcpy

        mov  dx, buffer
        mov  ah, 0x09
        int  0x21

        ; --- compare str1 with str2 ---
        mov  si, str1
        mov  di, str2
        call strcmp
        je   .same
        mov  dx, diffmsg
        jmp  .say
.same:  mov  dx, samemsg
.say:   mov  ah, 0x09
        int  0x21

        ; --- find 'o' in str1 ---
        mov  di, str1
        mov  al, 'o'
        call strchr
        jc   .notfound
        mov  dx, foundmsg
        jmp  .say2
.notfound:
        mov  dx, nofoundmsg
.say2:  mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; strlen — length of the $-terminated string at ES:DI
;   Out:       CX = length
;   Destroys:  AL, DI, flags
; ---------------------------------------------------------------
strlen:
        push di
        mov  al, '$'
        mov  cx, 0xFFFF         ; scan at most 65535 bytes
        repne scasb             ; stop when AL matches
        ; CX now = 0xFFFF − (length + 1)
        not  cx                 ; CX = length + 1
        dec  cx                 ; CX = length
        pop  di
        ret

; ---------------------------------------------------------------
; strcpy — copy the $-terminated string at DS:SI to ES:DI
;   Destroys:  AL, SI, DI, flags
; ---------------------------------------------------------------
strcpy:
.next:
        lodsb                   ; AL <- [SI], SI++
        stosb                   ; [DI] <- AL, DI++
        cmp  al, '$'
        jne  .next
        ret

; ---------------------------------------------------------------
; strcmp — compare the $-terminated strings at DS:SI and ES:DI
;   Out:       ZF = 1 if identical; otherwise flags from the first difference
;   Destroys:  AL, SI, DI, flags
; ---------------------------------------------------------------
strcmp:
.next:
        mov  al, [si]
        cmpsb                   ; compares [SI] with [DI], advances both
        jne  .done              ; a difference — flags already set
        cmp  al, '$'            ; end of both strings?
        jne  .next
        ; fell through with AL = '$' and everything equal -> ZF is already 1
.done:
        ret

; ---------------------------------------------------------------
; strchr — find AL in the $-terminated string at ES:DI
;   Out:       CF = 0 and DI -> the match, or CF = 1 if not found
;   Destroys:  CX, DI, flags
; ---------------------------------------------------------------
strchr:
        push ax
        mov  ah, al             ; keep the target in AH
        mov  cx, 0xFFFF
.next:
        mov  al, [di]
        cmp  al, '$'
        je   .notfound
        cmp  al, ah
        je   .found
        inc  di
        loop .next
.notfound:
        pop  ax
        stc                     ; CF = 1 -> not found
        ret
.found:
        pop  ax
        clc                     ; CF = 0 -> DI points at it
        ret

; ---------------------------------------------------------------
; print_dec — print AX as unsigned decimal, then CR LF
; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx
        mov  bx, 10
        xor  cx, cx
.divide:
        xor  dx, dx
        div  bx
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide
.output:
        pop  dx
        add  dl, '0'
        mov  ah, 0x02
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
str1:       db   'Hello, world!$'
str2:       db   'Hello, there!$'
buffer:     times 80 db 0
samemsg:    db   'Strings are identical', 0x0D, 0x0A, '$'
diffmsg:    db   'Strings differ', 0x0D, 0x0A, '$'
foundmsg:   db   "Found 'o'", 0x0D, 0x0A, '$'
nofoundmsg: db   "No 'o' found", 0x0D, 0x0A, '$'
```

**Output:**

```
13
Hello, world!
Strings differ
Found 'o'
```

### 7.1 The `strlen` trick, explained

`REPNE SCASB` starting with `CX = 0xFFFF` decrements `CX` once per byte examined, including the
terminator. So after it stops:

```
   CX = 0xFFFF − (length + 1)
```

`NOT CX` computes `0xFFFF − CX`, which gives `length + 1`; `DEC CX` gives the length. Two
instructions, no arithmetic on a register you have to think about.

For `'Hello, world!$'` the length is 13 and the terminator makes 14 bytes scanned, so
`CX = 0xFFFF − 14 = 0xFFF1`; `NOT` gives `0x000E` = 14; `DEC` gives 13. ✔

### 7.2 Why `strcmp` reads `[SI]` before `CMPSB`

`CMPSB` advances both pointers, so after it, `SI` no longer points at the character just compared.
We need that character to test for the terminator, so we copy it into `AL` first. An alternative is
to test `[si-1]` after the `CMPSB`, which is uglier.

---

## 8. Interrupts and `REP` — the 8086 erratum

If a `REP`-prefixed string instruction has **more than one prefix** — for example a segment override
*and* a `REP` — and an interrupt occurs mid-repeat, the 8086 loses all but the last prefix when it
resumes.

```asm
        rep es movsb            ; DANGEROUS on an 8086 if interrupts are enabled
```

The instruction resumes without the `ES` override and copies from the wrong segment for the
remainder of the block. It is intermittent, depends on interrupt timing, and is essentially
impossible to debug from the symptoms.

**Workarounds:**

1. Do not use a segment override with `REP`. Arrange the segments so the defaults are right.
2. Wrap it in `CLI`/`STI` — but that can block the timer for 17,000 clocks on a 1000-byte copy,
   which loses clock ticks.
3. Break the copy into small chunks with interrupts enabled between them.

The 80286 fixed this. It is worth knowing because period code contains the workarounds and they look
inexplicable otherwise.

---

## 9. Summary

```
  all five use:  DS:SI source (overridable)   ES:DI destination (NOT overridable)
                 CX count (with REP)          AL/AX element      DF direction

  MOVSB/W  A4/A5  ES:[DI] <- DS:[SI]     18 clk   the only memory-to-memory move
  CMPSB/W  A6/A7  flags from [SI]-[DI]   22 clk
  SCASB/W  AE/AF  flags from AL-[DI]     15 clk   uses DI only
  LODSB/W  AC/AD  AL <- DS:[SI]          12 clk   uses SI only, no flags
  STOSB/W  AA/AB  ES:[DI] <- AL          11 clk   uses DI only, no flags

  CLD -> DF=0 -> SI/DI INCREASE by 1 (byte) or 2 (word)
  STD -> DF=1 -> they DECREASE.  ALWAYS restore CLD afterwards.

  REP    F3  while CX != 0                     -> MOVS, STOS
  REPE   F3  while CX != 0 and ZF = 1          -> CMPS, SCAS
  REPNE  F2  while CX != 0 and ZF = 0          -> CMPS, SCAS

  CX is tested BEFORE each iteration, so REP with CX=0 does nothing
  the pointers advance even on the terminating iteration -> usually DEC DI after
  after REPNE SCAS, test ZF to tell "found" from "ran out"

  REP MOVSW is twice as fast as REP MOVSB on an 8086, and the same on an 8088
  set ES before any string instruction — the commonest bug in this chapter
  overlapping copy with dst > src  ->  copy backwards with STD
```

---

## Exercises

**29.1** Which registers does `MOVSB` use? Which segment applies to each pointer, and which of the
two can be overridden?

**29.2** `SI = 0x100`, `DI = 0x200`, `DF = 0`. After `MOVSW`, what are `SI` and `DI`?

**29.3** Write the five lines that copy 200 bytes from `src` to `dst` within one segment.

**29.4** Rewrite it to use `MOVSW`, handling an odd byte count. Explain the `adc cx, 0`.

**29.5** Why does `REP` need no `JCXZ` guard when `LOOP` does?

**29.6** Write the code that fills 2000 words of the video buffer at `0xB800:0000` with `0x0720`.

**29.7** After a `REPNE SCASB` that found a match, where does `DI` point, and what does `CX` hold?
Write the two lines that follow it to test for success and correct `DI`.

**29.8** `REPE CMPSB` finishes with `ZF = 0`. What does that mean, and where do `SI` and `DI` point?

**29.9** A `.COM` program's string instructions work; the same code in an `.EXE` writes to the wrong
place. Why, and what is the fix?

**29.10** A block of 100 bytes at offset `0x300` must be copied to offset `0x340`. Which direction,
and why? Write the code.

**29.11** Compute the clock counts for copying 1000 bytes with (a) `REP MOVSB`, (b) `REP MOVSW`,
(c) a hand-written `MOV`/`INC`/`LOOP` loop. Which is fastest, and by how much?

**29.12** Explain the `strlen` trick in §7.1 for a string of length 5. Give `CX` at each step.

**29.13** Why must an interrupt handler that uses `MOVSB` execute `CLD` on entry?

**29.14** Write a routine that converts a string to upper case in place, using `LODSB` and `STOSB`,
with `SI` and `DI` pointing at the same buffer. Explain why that works.

Answers in [Appendix H](H-exercise-solutions.md#chapter-29).

---

[← Procedures and the stack](28-procedures-and-stack.md) · [Contents](README.md) · [Next: Processor control →](30-processor-control.md)
