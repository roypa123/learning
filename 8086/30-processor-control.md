# Chapter 30 — Processor control instructions

[← String instructions](29-string-instructions.md) · [Contents](README.md) · [Next: Interrupts →](31-interrupts.md)

---

## Goal

The instructions that control the processor itself rather than manipulating data: `CLC`, `STC`,
`CMC`, `CLD`, `STD`, `CLI`, `STI`, `HLT`, `WAIT`, `NOP`, `LOCK` and `ESC`.

Each is one or two bytes, most take a handful of clocks, and several of them are the difference
between a program that works and one that fails once an hour.

---

## 1. The flag-control instructions

Nine instructions, three groups, all one byte.

| Instruction | Opcode | Clocks | Effect |
|-------------|--------|--------|--------|
| `CLC` | `F8` | 2 | `CF = 0` |
| `STC` | `F9` | 2 | `CF = 1` |
| `CMC` | `F5` | 2 | `CF = NOT CF` |
| `CLD` | `FC` | 2 | `DF = 0` (strings go forwards) |
| `STD` | `FD` | 2 | `DF = 1` (strings go backwards) |
| `CLI` | `FA` | 2 | `IF = 0` (maskable interrupts off) |
| `STI` | `FB` | 2 | `IF = 1` (maskable interrupts on) |

Each affects **only** its own flag. Nothing else changes.

There is **no instruction to set or clear `ZF`, `SF`, `OF`, `PF` or `AF` directly.** For those you
must use `PUSHF`/`POPF` (Chapter 21 §6.2), or arrange an operation that sets them as a side effect.

---

## 2. `CLC`, `STC`, `CMC` — the carry flag

### 2.1 Starting a carry chain

```asm
        clc                     ; no carry into the first word
.next:  mov  ax, [si]
        adc  ax, [di]
        ...
        loop .next
```

Without the `CLC`, the first `ADC` adds whatever `CF` happened to be, and the answer is wrong one
time in two. Chapter 22 §8.3.

### 2.2 The error-return convention

The near-universal 8086 convention, used by DOS and the BIOS:

```asm
; A procedure that can fail:
myproc: ...
        jc   .error             ; something went wrong
        clc                     ; success
        ret
.error: mov  ax, ERROR_CODE
        stc                     ; failure
        ret

; The caller:
        call myproc
        jc   .handle_error
```

One flag, no register consumed, and the test is one instruction. Every DOS `INT 21h` call uses it
(Chapter 36 §3).

### 2.3 `CMC` — flipping a decision

```asm
        cmc                     ; invert the sense of the carry
```

Used where a routine returns the *opposite* convention from what you want:

```asm
        call some_routine       ; returns CF=1 for success (the wrong way round)
        cmc                     ; now CF=1 means failure, matching our convention
        ret
```

Rare, but it saves a branch.

### 2.4 Using `CF` as a boolean

```asm
; Return "is AL a digit?" in CF.
is_digit:
        cmp  al, '0'
        jb   .no
        cmp  al, '9'
        ja   .no
        stc
        ret
.no:    clc
        ret
```

`CF` is the cheapest boolean the 8086 has: no register, one instruction to test.

---

## 3. `CLD` and `STD` — the direction flag

Covered in Chapter 29 §1.2. The rules:

**Default is `CLD`.** DOS, the BIOS and every library assume `DF = 0`.

**Set `STD` only for the duration you need it, then `CLD`.**

```asm
        std
        rep  movsb              ; a backwards copy
        cld                     ; restore immediately
```

**An interrupt handler must `CLD` on entry** if it uses string instructions, and restore `DF` on
exit:

```asm
handler:
        pushf                   ; save the interrupted code's flags, including DF
        cld                     ; now we know DF = 0
        ; ... string instructions ...
        popf                    ; DF restored to whatever it was
        iret
```

Actually `IRET` restores the flags from the stack anyway (Chapter 31 §4), so the `PUSHF`/`POPF` pair
is redundant in a true interrupt handler. It *is* needed in a procedure that is not entered via an
interrupt.

**Forgetting to restore `DF`** produces the most baffling class of bug in DOS programming: some
unrelated library call, made much later, copies a block backwards and corrupts memory.

---

## 4. `CLI` and `STI` — the interrupt flag

### 4.1 What they control

`IF = 1`: the processor accepts maskable interrupts on the `INTR` pin.
`IF = 0`: it ignores them.

**What `CLI` does not block:**

- `NMI` — by definition unmaskable
- software `INT n` instructions
- exceptions: divide error (`INT 0`), single step (`INT 1`), `INTO` (`INT 4`)

### 4.2 Critical sections

The legitimate use: protecting a multi-instruction update to data an interrupt handler also touches.

```asm
; Read the 32-bit tick counter that the timer ISR increments.
        cli
        mov  ax, [ticks]        ; the ISR must not run between these
        mov  dx, [ticks+2]      ;   two instructions
        sti
```

Without `CLI`, the timer could increment `ticks` from `0x0000FFFF` to `0x00010000` between the two
`MOV`s, and you would read `DX:AX = 0x0001FFFF` — a value that never existed.

### 4.3 Keep it short

**Every clock spent with `IF = 0` is a clock during which the keyboard and timer cannot be serviced.**
The PC's timer interrupts 18.2 times per second, so a `CLI` region longer than about 55 ms loses a
tick and the system clock drifts. A `CLI` region long enough to miss a keystroke loses input.

Rule of thumb: **fewer than 100 instructions**, and never around an I/O wait loop.

### 4.4 `STI` has a one-instruction delay

`STI` does not enable interrupts until *after the following instruction*. This is deliberate:

```asm
        sti
        ret                     ; the RET is guaranteed to execute before any interrupt
```

Without the delay, an interrupt could occur between `STI` and `RET`, pushing onto a stack that the
procedure was about to finish with. The same mechanism protects the `MOV SS` / `MOV SP` pair
(Chapter 7 §4.2).

### 4.5 Nested critical sections — save and restore

A procedure that does `CLI` … `STI` **enables interrupts even if its caller had disabled them**.
That silently breaks the caller's critical section. The correct pattern:

```asm
        pushf                   ; remember the caller's IF
        cli
        ; ... critical work ...
        popf                    ; restore whatever IF was — do NOT use STI
```

This is the standard idiom in any code that might be called from an interrupt handler.

---

## 5. `HLT`

```asm
        hlt                     ; F4 — one byte, 2 clocks (then stops)
```

Stops the processor. It stays stopped until:

- an **enabled** interrupt arrives on `INTR`, or
- an `NMI` arrives, or
- `RESET`.

After servicing the interrupt, execution resumes at the instruction **after** the `HLT`.

### 5.1 The idle loop

```asm
        sti                     ; make sure interrupts are ENABLED first
.idle:  hlt
        jmp  .idle
```

This is how an operating system waits with nothing to do. The processor stops clocking its execution
unit; on a CMOS part it draws far less current.

### 5.2 The trap

```asm
        cli
        hlt                     ; ✘ the machine is now dead until RESET
```

With `IF = 0`, only `NMI` and `RESET` can wake it. `CLI`/`HLT` is the standard way to halt a machine
deliberately — you will see it at the end of bare-metal code — but it is not what you want in an
idle loop.

### 5.3 `HLT` in a `.COM` program

Do not. DOS expects `INT 21h` function `4Ch`. `HLT` in a DOS program hangs the machine, since DOS
has no way to recover.

---

## 6. `WAIT`

```asm
        wait                    ; 9B — one byte, 3 or more clocks
```

Suspends the processor until the **`TEST#` pin goes low**, checking every five clocks. Interrupts
are still recognised while waiting.

### 6.1 What it is for

Coprocessor synchronisation. The 8087's `BUSY` output connects to the 8086's `TEST#` pin. When the
8087 is computing, `BUSY` is high, so `TEST#` is high, so `WAIT` waits.

```asm
        fmul                    ; tell the 8087 to multiply (an ESC instruction)
        ; ... the 8086 can do other work here ...
        wait                    ; now wait for the 8087 to finish
        mov  ax, [result]       ; safe to read the result
```

Assemblers insert `WAIT` automatically before most 8087 instructions; the `FWAIT` mnemonic is the
same byte. Chapter 54.

### 6.2 The trap

**If there is no 8087 and `TEST#` is not tied low, `WAIT` hangs forever.** Chapter 11 §10.5: tie
`TEST#` to ground in any system without a coprocessor.

---

## 7. `NOP`

```asm
        nop                     ; 90 — one byte, 3 clocks
```

Does nothing. It is literally `XCHG AX, AX`, which is why the opcode is `0x90` and why it takes 3
clocks rather than 1 (Chapter 21 §2).

### 7.1 Uses

**Timing delays** — a precise short delay in a hardware interface:

```asm
        out  dx, al
        nop                     ; give the device 3 clocks of recovery time
        nop
        in   al, dx
```

Though `jmp $+2` (15 clocks) is the more common recovery idiom on a PC (Chapter 17 §7).

**Alignment** — padding code to an even address, although the 8086 gains nothing from code
alignment (only *data* alignment matters, Chapter 10 §4).

**Patch space** — reserving bytes that a loader or debugger will overwrite later. Self-modifying
code and copy-protection schemes used this heavily.

**Deleting an instruction in a debugger** — overwrite it with `0x90` bytes and the code runs on
without it. This is what "NOP it out" means.

### 7.2 Why `NOP` costs 3 clocks

Because it is a real `XCHG` that the processor actually performs, on a register with itself. Intel
did not waste an opcode on a dedicated do-nothing instruction. The 8086 opcode map is dense
(Appendix B) and `0x90` was already taken.

---

## 8. `LOCK`

```asm
        lock xchg [semaphore], al       ; F0 prefix
```

A **prefix**, not an instruction. It asserts the `LOCK#` pin (maximum mode, pin 29) for the duration
of the instruction it precedes, telling other bus masters not to take the bus.

### 8.1 What it is for

Atomic read-modify-write in a multiprocessor system. Consider two processors trying to acquire a
lock:

```asm
; Without LOCK, this is a race:
        mov  al, 1
        xchg [semaphore], al    ; read the old value, write 1
        or   al, al
        jnz  .already_taken     ; someone else had it
        ; we have the lock
```

`XCHG` with a memory operand is a read *and* a write. If another processor reads between our read
and our write, both think they acquired the lock.

**On the 8086, `XCHG` with a memory operand asserts `LOCK#` automatically**, even without the
prefix. So the code above is already safe. The explicit `LOCK` prefix is needed for other
read-modify-write instructions:

```asm
        lock inc word [counter]     ; atomic increment
        lock and byte [flags], 0xFE ; atomic bit clear
```

### 8.2 When it does nothing

In a single-processor system with no DMA contention, `LOCK` has no observable effect — there is no
other master to lock out. It costs one byte and no extra clocks.

In **minimum mode** there is no `LOCK#` pin at all (pin 29 is `WR#`), so the prefix is decoded and
ignored.

### 8.3 What it does *not* do

`LOCK` does **not** disable interrupts. An interrupt can still occur after the locked instruction
completes. For mutual exclusion against an *interrupt handler* on the same processor, you need
`CLI`, not `LOCK`.

---

## 9. `ESC`

```asm
        esc  op, source         ; D8-DF — the coprocessor escape
```

Six bits of opcode plus a ModR/M byte. The 8086 **computes the effective address, performs a dummy
read from it, and otherwise ignores the instruction** — but the address and data appear on the bus,
where the 8087 is watching.

You will essentially never write `ESC` directly. The assembler emits it for every 8087 mnemonic:

```asm
        fadd  st0, st1          ; assembles to an ESC instruction
```

The mechanism:

1. The 8086 fetches the `ESC` instruction and puts its operand address on the bus.
2. The 8087, shadowing the instruction queue via `QS1`/`QS0` (Chapter 15 §6), recognises the opcode
   as its own.
3. The 8087 captures the address from the bus and performs the operation itself.
4. If it needs more bus cycles, it requests the bus through `RQ/GT0#`.
5. The 8086 continues; a later `WAIT` synchronises them.

Chapter 54 covers all of it.

**Without an 8087 fitted, an `ESC` instruction does nothing** — no fault, no error. The dummy read
happens and execution continues. That is why you cannot detect a missing coprocessor by trapping;
the standard detection routine writes a known value to the 8087's control word and reads it back.

---

## 10. Segment override prefixes

Not usually listed as "processor control", but they belong here as prefixes.

| Prefix | Byte | Forces |
|--------|------|--------|
| `ES:` | `0x26` | `ES` |
| `CS:` | `0x2E` | `CS` |
| `SS:` | `0x36` | `SS` |
| `DS:` | `0x3E` | `DS` |

One byte, **two clocks**, applies to one instruction.

```asm
        mov  ax, [es:bx]
        mov  al, [cs:table+bx]      ; read a table stored in the code segment
        mov  [ss:si], al
```

Cannot override: instruction fetch (`CS:IP`), stack operations (`SS:SP`), string destination
(`ES:DI`).

---

## 11. Worked example — a safe critical section

```asm
; ---------------------------------------------------------------
; read_ticks — read the 32-bit BIOS tick counter safely.
;
;   The BIOS counter at 0040:006C is incremented 18.2 times a
;   second by the timer ISR. Reading two words non-atomically can
;   catch it mid-update.
;
;   Out:       DX:AX = the tick count
;   Destroys:  ES
;   Preserves: the caller's interrupt state
; ---------------------------------------------------------------
read_ticks:
        push bx
        pushf                   ; remember the caller's IF
        cli                     ; our critical section begins

        mov  bx, 0x0040
        mov  es, bx
        mov  ax, [es:0x006C]    ; low word
        mov  dx, [es:0x006E]    ; high word

        popf                    ; restore IF — NOT sti
        pop  bx
        ret
```

Two details worth naming:

**`pushf` / `popf` rather than `cli` / `sti`.** If the caller had already disabled interrupts, an
`STI` here would enable them behind the caller's back and break *its* critical section. `POPF`
restores exactly what was there.

**The section is four instructions long.** That is about 30 clocks — 6 µs at 5 MHz. Nothing is lost.

---

## 12. Summary

```
  CLC F8  CF=0     STC F9  CF=1     CMC F5  CF = NOT CF
  CLD FC  DF=0     STD FD  DF=1     — always restore CLD
  CLI FA  IF=0     STI FB  IF=1     — STI takes effect AFTER the next instruction

  no instruction sets ZF, SF, OF, PF or AF directly — use PUSHF/POPF

  HLT  F4   stop until INTR (if IF=1), NMI or RESET.
            `sti / hlt / jmp $-1` is an idle loop; `cli / hlt` is a dead machine.
  WAIT 9B   suspend until TEST# goes low. Hangs forever if TEST# floats.
  NOP  90   does nothing, 3 clocks. It IS `XCHG AX, AX`.
  LOCK F0   prefix: asserts LOCK# for one instruction. XCHG with memory
            does it automatically. Does NOT disable interrupts.
  ESC  D8-DF  hands the instruction to the 8087; does nothing without one.

  segment overrides 26 ES / 2E CS / 36 SS / 3E DS — 1 byte, 2 clocks

  critical sections:  PUSHF / CLI / ... / POPF      not  CLI / ... / STI
  keep them under ~100 instructions or you lose timer ticks and keystrokes
```

---

## Exercises

**30.1** Which flags can be set and cleared by a dedicated instruction? Which cannot, and how would
you change one of those?

**30.2** Why must a multi-precision addition loop begin with `CLC`?

**30.3** Write a procedure that returns `CF = 1` if `AL` holds an upper-case letter and `CF = 0`
otherwise.

**30.4** Explain what is wrong with this, and give the correct version:

```asm
myproc: cli
        ; ... critical work ...
        sti
        ret
```

**30.5** `STI` does not take effect until after the following instruction. Give a concrete reason
that matters.

**30.6** What wakes a processor from `HLT`? What happens if `IF = 0` when it executes?

**30.7** A board has no 8087 and `TEST#` is left unconnected. A program contains a stray `WAIT`.
Describe the symptom.

**30.8** Why does `NOP` take three clocks rather than one?

**30.9** What does the `LOCK` prefix do in a single-processor minimum-mode system?

**30.10** Does `LOCK` protect a variable from being changed by an interrupt handler on the same
processor? Explain.

**30.11** Write the code that reads a 32-bit counter updated by an interrupt handler, preserving the
caller's interrupt state.

**30.12** An interrupt handler uses `STD` and forgets to restore `DF`. Describe a plausible symptom
that appears much later and in unrelated code.

**30.13** How many bytes and clocks does a segment override cost? Rewrite `mov al, [es:bx+si]` as a
sequence that avoids the override, and say whether it is worth it.

Answers in [Appendix H](H-exercise-solutions.md#chapter-30).

---

[← String instructions](29-string-instructions.md) · [Contents](README.md) · [Next: Interrupts →](31-interrupts.md)
