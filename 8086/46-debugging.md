# Chapter 46 — Debugging

[← Programs: graphics](45-programs-graphics.md) · [Contents](README.md) · [Next: 8255A PPI →](47-8255-ppi.md)

---

## Goal

How to find out what your program is actually doing: `DEBUG.EXE`, the DOSBox-X debugger, the trap
flag, breakpoints, and the handful of techniques that solve most 8086 bugs.

This closes Part IV.

---

## 1. The tools

| Tool | Where from | Best for |
|------|-----------|----------|
| **`DEBUG.EXE`** | FreeDOS, or any DOS | assembling, tracing, patching, hex-dumping |
| **DOSBox-X debugger** | built in | breakpoints on interrupts and ports, watching memory |
| **NASM listing file** | `nasm -l` | checking what an instruction assembled to |
| **`printf` debugging** | your own code | when a debugger changes the timing |

Chapter 1 §7 covers installing them.

---

## 2. `DEBUG.EXE`

Start it with a program to load, or with nothing to assemble by hand:

```
C:\> debug myprog.com
-
```

The `-` is its prompt. Commands are single letters.

### 2.1 The command set

| Command | Meaning |
|---------|---------|
| `r` | show all registers and the next instruction |
| `r ax` | show `AX` and prompt for a new value |
| `d addr` | dump memory in hex and ASCII |
| `d addr len` | dump `len` bytes |
| `e addr list` | enter bytes into memory |
| `u addr` | unassemble (disassemble) |
| `a addr` | assemble instructions at `addr` |
| `t` | trace — execute **one** instruction, stepping *into* calls and interrupts |
| `p` | proceed — execute one instruction, stepping **over** calls and interrupts |
| `g` | go — run until the program ends |
| `g addr` | run until `addr` is reached (a temporary breakpoint) |
| `f addr len value` | fill memory |
| `m src len dst` | move (copy) memory |
| `s addr len list` | search memory for a byte sequence |
| `n name` | set a filename |
| `l` | load the named file |
| `w` | write memory to the named file |
| `q` | quit |

### 2.2 A worked session

Using `hello.com` from Chapter 34:

```
C:\> debug hello.com

-r
AX=0000  BX=0000  CX=001C  DX=0000  SP=FFFE  BP=0000  SI=0000  DI=0000
DS=0AF3  ES=0AF3  SS=0AF3  CS=0AF3  IP=0100   NV UP EI PL NZ NA PO NC
0AF3:0100 B409          MOV     AH,09
```

Read that carefully:

- **`CX=001C`** — 28, the file size. DOS leaves it there (Chapter 35 §3.4).
- **`DS=ES=SS=CS=0AF3`** — all four equal, the `.COM` model (Chapter 35 §2.3).
- **`IP=0100`** — the entry point.
- **`NV UP EI PL NZ NA PO NC`** — the flags, in `DEBUG`'s two-letter notation (Chapter 8 §10.1).
  All clear.
- The last line shows the **next** instruction, already disassembled.

```
-u 100 10c
0AF3:0100 B409          MOV     AH,09
0AF3:0102 BA0D01        MOV     DX,010D
0AF3:0105 CD21          INT     21
0AF3:0107 B44C          MOV     AH,4C
0AF3:0109 B000          MOV     AL,00
0AF3:010B CD21          INT     21
```

Every byte from Chapter 34 §3, confirmed by the disassembler.

```
-t
AX=0900  BX=0000  CX=001C  DX=0000  ...  IP=0102
0AF3:0102 BA0D01        MOV     DX,010D
```

`AH` is now `09`. One instruction executed.

```
-t
AX=0900  ...  DX=010D  ...  IP=0105
0AF3:0105 CD21          INT     21
```

`DX` now holds `010D`, the address of the string.

```
-d 10d 11c
0AF3:010D  48 65 6C 6C 6F 2C 20 38-30 38 36 21 0D 0A 24     Hello, 8086!..$
```

The string, at exactly the address `DX` points at.

```
-p
Hello, 8086!
AX=0924  ...  IP=0107
```

**`p`, not `t`.** `t` would step *into* the DOS handler and you would spend ten minutes there. `p`
runs the whole `INT 21h` and stops afterwards.

Note `AX=0924` — DOS left `0x24` (the `$`) in `AL`. That is the sort of thing you discover only by
looking.

```
-g
Hello, 8086!

Program terminated normally
-q
```

### 2.3 Setting a value

```
-r ax
AX 0924
:1234
-r
AX=1234  ...
```

Useful for testing a branch without editing and rebuilding: set the register to the value that
triggers the case you want.

### 2.4 Patching bytes

```
-e 100 90 90
```

Writes `90 90` (two `NOP`s) over the first instruction. Then `g` runs the patched version. This is
"NOPping out" an instruction (Chapter 30 §7.1) and it is how you test whether a suspect line is the
cause.

Patches live only in memory. `w` writes them back to the file — which is how DOS-era patches were
distributed.

---

## 3. Breakpoints

### 3.1 `g addr` — the temporary breakpoint

```
-g 10b
```

Runs until `IP` reaches `0x010B`, then stops and shows the registers. This is the workhorse: run to
the interesting place, then start tracing.

`DEBUG` implements this by writing `0xCC` (`INT 3`) at the address, running, catching the interrupt
and restoring the original byte (Chapter 31 §4.1).

### 3.2 `INT 3` in your own source

You can put the breakpoint in the program:

```asm
        int3                    ; NASM's mnemonic for the one-byte CC
```

When run under a debugger, execution stops there. When run normally, DOS's default `INT 3` handler
returns immediately, so it is harmless — but remove it before shipping.

### 3.3 DOSBox-X breakpoints

The DOSBox-X debugger adds kinds `DEBUG.EXE` cannot do:

| Command | Effect |
|---------|--------|
| `BP cs:0150` | break at an address |
| `BPINT 21` | break on **any** `INT 21h` |
| `BPINT 21 09` | break on `INT 21h` with `AH = 09h` |
| `BPINT 10 00` | break on a video mode change |
| `BPM 0AF3:0200` | break when that **memory** location is written |
| `BPLIST` | list the breakpoints |
| `BPDEL *` | delete them all |

**`BPINT` is the one that solves real problems.** "Which of my forty DOS calls is failing?" —
`BPINT 21` and step through them. **`BPM` is the other**: "what is corrupting this variable?" — set
a memory breakpoint and the debugger stops on the instruction that writes it.

---

## 4. Single-stepping with the trap flag

The mechanism every debugger uses, and you can use it directly.

**`TF = 1` generates `INT 1` after every instruction** (Chapter 8 §8.3). The `INT` sequence clears
`TF`, so the handler runs at full speed; `IRET` restores the pushed flags, turning stepping back on.

```asm
; tracer.asm — single-step a section of code, printing IP at each step
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- install our INT 1 handler ---
        mov  ah, 0x35
        mov  al, 0x01
        int  0x21               ; ES:BX = the old handler
        mov  [old_off], bx
        mov  [old_seg], es

        push ds
        mov  dx, step_handler
        push cs
        pop  ds
        mov  ax, 0x2501
        int  0x21
        pop  ds

        ; --- turn on single-stepping ---
        pushf
        pop  ax
        or   ax, 0x0100         ; set TF (bit 8)
        push ax
        popf                    ; the NEXT instruction will trap

        ; --- the code being traced ---
        mov  ax, 1
        mov  bx, 2
        add  ax, bx
        mov  cx, ax
        nop

        ; --- turn it off ---
        pushf
        pop  ax
        and  ax, 0xFEFF         ; clear TF
        push ax
        popf

        ; --- restore the old handler ---
        push ds
        mov  dx, [old_off]
        mov  ax, [old_seg]
        mov  ds, ax
        mov  ax, 0x2501
        int  0x21
        pop  ds

        print donemsg
        exit 0

; ---------------------------------------------------------------
; step_handler — called after every instruction while TF = 1.
;
;   The stack on entry:   [SP+0] = IP,  [SP+2] = CS,  [SP+4] = FLAGS
;   of the interrupted instruction's SUCCESSOR.
; ---------------------------------------------------------------
step_handler:
        push bp
        mov  bp, sp
        push ax
        push bx
        push cx
        push dx
        push ds

        push cs
        pop  ds                 ; our data segment

        mov  ax, [bp+2]         ; the saved IP  (BP+0 is the saved BP)
        call print_hex16
        putc ' '

        pop  ds
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        pop  bp
        iret

old_off:  dw  0
old_seg:  dw  0
donemsg:  db  0x0D, 0x0A, 'Trace complete.', 0x0D, 0x0A, '$'
```

**Output:** a list of the addresses of each instruction executed between the two `POPF`s.

### 4.1 The stack layout inside the handler

This is the part that trips people up:

```
   on entry to the handler:        after `push bp` / `mov bp, sp`:

   [SP+4]  FLAGS                   [BP+6]  FLAGS
   [SP+2]  CS                      [BP+4]  CS
   [SP+0]  IP                      [BP+2]  IP
                                   [BP+0]  the saved BP
```

`[BP+2]` is therefore the `IP` that will be resumed — i.e. the address of the *next* instruction.
Modifying it changes where execution continues, which is how a debugger implements "skip this
instruction".

**This is the same frame layout as any far procedure plus the flags word** (Chapter 28 §7).

---

## 5. The techniques that actually find bugs

### 5.1 Check the listing first

Before reaching for a debugger, look at what the assembler produced:

```
nasm -f bin prog.asm -o prog.com -l prog.lst
```

Half of all "impossible" behaviour is an instruction that assembled to something other than what you
meant — usually a missing `byte`/`word` keyword, or a label that resolved to the wrong thing.

### 5.2 Bisect with `int3`

Put `int3` halfway through the program. Run it.

- **Reached?** The bug is in the second half. Move the `int3` down.
- **Not reached?** The bug is in the first half. Move it up.

Eight or nine iterations locate any bug in a 1000-instruction program. Crude, fast, and it works
when a debugger's timing changes the symptom.

### 5.3 Print the state

```asm
%macro dbgax 1
        push ax
        push dx
        print %%msg
        pop  dx
        pop  ax
        push ax
        call print_hex16
        newline
        pop  ax
        jmp  %%past
    %%msg: db %1, ' AX=', '$'
    %%past:
%endmacro

; use:
        dbgax 'after the loop:'
```

Slower than a debugger but it works everywhere, including inside interrupt handlers where a debugger
cannot easily go.

### 5.4 Check the stack balance

When a `RET` jumps somewhere impossible, count the pushes and pops on every path through the
procedure — including the early exits (Chapter 28 §9.1).

In `DEBUG`, set a breakpoint on the `RET` and look at `SP` and `[SS:SP]`:

```
-g 0150
AX=...  SP=FFF8  ...
-d ss:fff8 fffb
0AF3:FFF8  34 12 ...
```

If the word at `[SS:SP]` is not a plausible code offset, something is unbalanced.

### 5.5 Dump the data

```
-d ds:0200 021f
```

Look at your variables directly. A buffer that contains the wrong thing usually tells you *what*
wrote it, because the pattern is recognisable.

---

## 6. The bugs you will actually hit

Ranked by how often they occur in practice.

### 6.1 Wrong conditional jump family

```asm
        cmp  al, bl
        jg   bigger             ; SIGNED — wrong if these are characters
```

**Symptom:** works for small values, fails for anything above 127 (bytes) or 32,767 (words).
**Fix:** `JA`/`JB` for unsigned, `JG`/`JL` for signed (Chapter 27 §4.2).
**Find it:** in `DEBUG`, set the register to `0x80` and trace the comparison.

### 6.2 `CX` destroyed by an inner loop

```asm
        mov  cx, 10
.outer: mov  cx, 5              ; ✘ the outer count is gone
.inner: loop .inner
        loop .outer             ; runs 5 times, not 10
```

**Symptom:** the outer loop runs the wrong number of times, usually once.
**Fix:** `push cx` / `pop cx`, or use a different register for one loop (Chapter 27 §6.4).

### 6.3 Missing `xor dx, dx` before `DIV`

```asm
.next:  div  bx                 ; DX still holds the previous remainder
```

**Symptom:** "Divide overflow" and immediate termination, or wildly wrong numbers.
**Fix:** `xor dx, dx` immediately before every `DIV` (Chapter 23 §8.1).

### 6.4 `ES` not set before a string instruction

**Symptom:** works in a `.COM` program, corrupts the PSP in an `.EXE`.
**Fix:** `mov ax, ds` / `mov es, ax` (Chapter 29 §1.3).

### 6.5 Forgetting `CLD`

**Symptom:** a string operation runs backwards, usually after some *other* code set `DF`.
**Fix:** `CLD` at the start of the program and after any `STD` (Chapter 30 §3).

### 6.6 An off-by-one in a `LOOP` with `CX = 0`

**Symptom:** the program hangs for about 13 seconds (65,536 iterations), then continues.
**Fix:** `JCXZ` before the loop (Chapter 27 §5).

### 6.7 Data executed as code

**Symptom:** a crash immediately after the last useful instruction.
**Fix:** put all data after an unconditional exit (Chapter 33 §7.1).
**Find it:** `u` at the address where it crashed — you will see nonsense instructions that are
recognisably ASCII.

### 6.8 Segment override forgotten

```asm
        mov  al, [bp]           ; reads SS:BP, not DS:BP
```

**Symptom:** reads garbage from the stack.
**Fix:** `mov al, [ds:bp]` (Chapter 7 §3.2).

### 6.9 A `$` missing from a string

**Symptom:** the message prints, followed by garbage of unpredictable length.
**Fix:** add it (Chapter 1 §10.3).

### 6.10 An interrupt vector not restored

**Symptom:** the program works; the machine crashes some seconds after it exits.
**Fix:** restore the vector before terminating (Chapter 31 §8.2).

---

## 7. A debugging worked example

A program that should print the sum of an array, and prints nothing.

```asm
; buggy.asm
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, arr
        mov  cx, len
        xor  ax, ax
.next:
        add  ax, [si]
        inc  si
        loop .next

        call print_udec
        newline
        exit 0

arr:    dw   10, 20, 30, 40
len     equ  ($ - arr) / 2
```

**Step 1 — run it.** It prints `10`. Expected 100.

**Step 2 — check the listing.**

```
     8 00000006 0304                    add  ax, [si]
     9 00000008 46                      inc  si
    10 00000009 E2FB                    loop .next
```

The instructions are what we wrote. So the bug is logical, not a typo.

**Step 3 — trace it.**

```
-g 106
AX=0000  SI=0111  CX=0004  ...
-t
AX=000A  SI=0111  ...             ; added 10.  Good.
-t
AX=000A  SI=0112  ...             ; SI advanced by ONE
-t
AX=000A  CX=0003 ...              ; loop
-t
AX=0A0A  SI=0112 ...              ; added the WRONG thing
```

**There it is.** `SI` advanced by 1, not 2, so the second `add ax, [si]` read the *high byte of
element 0 and the low byte of element 1* as a word — `0x0A00 + 0x000A`... and the accumulator went
wrong.

**Step 4 — the fix.**

```asm
        inc  si
        inc  si                 ; elements are WORDS — advance by 2
```

or, equivalently and more clearly:

```asm
        lodsw                   ; loads AX and advances SI by 2 automatically
```

but `LODSW` overwrites `AX`, which is our accumulator, so:

```asm
        mov  bx, [si]
        add  ax, bx
        inc  si
        inc  si
```

or restructure to accumulate in `BX`. The corrected version:

```asm
start:
        mov  si, arr
        mov  cx, len
        xor  bx, bx             ; accumulate in BX
        cld
.next:
        lodsw                   ; AX = element, SI += 2
        add  bx, ax
        loop .next

        mov  ax, bx
        call print_udec
        newline
        exit 0
```

**Output:** `100` ✔

### 7.1 What the process was

1. **Reproduce** — run it, note the exact wrong output.
2. **Check the assembler's output** — rule out a typo or a size ambiguity.
3. **Trace** — watch the registers change, one instruction at a time, until one of them is not what
   you expected.
4. **Explain** — say *why* it is wrong before changing anything. A fix you cannot explain usually
   moves the bug rather than removing it.
5. **Fix and re-verify.**

Step 4 is the one people skip, and it is the one that matters.

---

## 8. Writing debuggable code

The habits that make step 3 short.

**One job per procedure**, with a header comment stating inputs, outputs and what it destroys
(Chapter 28 §3.3).

**Register allocation written down** at the top of every loop (Chapter 7 §11).

**`cpu 8086`** so you cannot accidentally use a later instruction (Chapter 33 §8).

**Named constants**, not magic numbers. `mov ah, PRINT_STRING` is checkable; `mov ah, 9` is not.

**Guard clauses** — `jcxz` before every `LOOP`, a zero check before every `DIV`, a range check before
every jump table.

**Preserve registers in every procedure**, so a caller's state is never a variable in your
debugging.

**Keep the data at the end**, so a runaway `IP` crashes immediately rather than corrupting
something.

---

## 9. Summary

```
  DEBUG.EXE commands:
     r        registers        r ax     set a register
     u addr   unassemble       d addr   dump memory
     a addr   assemble         e addr   enter bytes
     t        trace INTO       p        proceed OVER  <- use p on INT
     g        go               g addr   run to a temporary breakpoint
     n / l / w  name, load, write a file      q  quit

  DOSBox-X adds:  BP addr · BPINT 21 09 · BPM addr (break on write) · BPLIST

  breakpoints work by writing 0xCC (one-byte INT 3) over the instruction

  TF = 1 -> INT 1 after every instruction. Inside the handler, after
     push bp / mov bp, sp:   [BP+2] = IP, [BP+4] = CS, [BP+6] = FLAGS

  the ten commonest bugs:
     1  JG/JL where JA/JB was meant (or the reverse)
     2  the inner loop destroying CX
     3  missing xor dx,dx before DIV
     4  ES not set before a string instruction
     5  missing CLD
     6  LOOP entered with CX = 0  -> 65,536 iterations
     7  data executed as code
     8  [BP] defaulting to SS when DS was meant
     9  a missing '$' terminator
    10  an interrupt vector not restored before exit

  the process: reproduce -> check the listing -> trace -> EXPLAIN -> fix -> verify
```

---

## Exercises

**46.1** In `DEBUG`, what is the difference between `t` and `p`? Which do you use on `int 0x21`, and
why?

**46.2** What does `CX` contain when `DEBUG` loads a `.COM` file, and why?

**46.3** Decode the flag display `NV UP EI PL NZ NA PO NC`. Which flags are set?

**46.4** Write the `DEBUG` commands that load `hello.com`, run to the second `INT 21h`, and dump the
16 bytes at `DS:010D`.

**46.5** How does a debugger set a breakpoint? Why must the breakpoint instruction be one byte?

**46.6** Write the four instructions that set the trap flag.

**46.7** Inside an `INT 1` handler that begins `push bp` / `mov bp, sp`, which offset from `BP` holds
the interrupted code's `IP`? Its `CS`? Its flags?

**46.8** A program's `RET` jumps to a random address. Describe the two things you would check first.

**46.9** For each symptom, name the likely cause: (a) the program hangs for about 13 seconds; (b) a
message prints followed by garbage; (c) the machine crashes several seconds after the program exits;
(d) "Divide overflow"; (e) the program works under DOS but corrupts memory when built as an `.EXE`.

**46.10** Write a macro that prints a register's name and value in hex, and works inside an interrupt
handler.

**46.11** Given this buggy fragment, find the bug by inspection and say what the symptom would be:

```asm
        mov  cx, 0
.next:  mov  al, [si]
        stosb
        inc  si
        loop .next
```

**46.12** Use the bisection technique of §5.2 to describe how you would locate a bug in a program of
2000 instructions. How many runs would it take?

**46.13** Trace the buggy program in §7 yourself and give the value of `AX` after each of the four
loop iterations.

Answers in [Appendix H](H-exercise-solutions.md#chapter-46).

---

[← Programs: graphics](45-programs-graphics.md) · [Contents](README.md) · [**Part V begins: 8255A PPI →**](47-8255-ppi.md)
