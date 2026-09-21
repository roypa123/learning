# The 8086 Book

A complete, line-by-line course on the Intel 8086 microprocessor — its architecture, its pins, its
bus, its instruction set, its assembly language, and the family of support chips that turn a bare
CPU into a computer.

This is written to be *read in order*, but every chapter stands on its own well enough to be used as
a reference later. Nothing is hand-waved. Every instruction is given with its encoding, its flag
effects and its clock count. Every program is disassembled and explained one byte at a time.

---

## Who this is for

You should be comfortable with the idea that a computer stores numbers and follows instructions.
That is the only prerequisite. You do **not** need prior assembly experience, prior electronics, or
a degree in anything. Chapters 2 and 3 build the number-system and digital-logic background from
scratch, and if you already have it, you can skip them in ten minutes.

If you are here for an exam syllabus — most university microprocessor courses are built around the
8086 — the book covers every topic those syllabi contain, and the appendices are laid out to be
revised from.

---

## The toolchain

Everything in this book is assembled with **NASM** and run under **DOSBox**, both free, both
available on Windows, macOS and Linux. Programs are real 16-bit DOS `.COM` and `.EXE` files — the
same format the 8086 actually ran.

```
nasm -f bin hello.asm -o hello.com     ; assemble
dosbox hello.com                       ; run
```

Where the MASM/TASM syntax you may meet in a textbook differs from NASM, a box like this appears:

> **MASM note.** MASM writes `MOV AX, OFFSET msg` where NASM writes `mov ax, msg`. The reason is
> explained in Chapter 33, §4.

Chapter 1 sets all of this up, step by step, with a check at each stage so you know it works before
you move on.

---

## How the book is organised

| Part | Chapters | What it covers |
|------|----------|----------------|
| **I — Foundations** | 0–4 | Setup, number systems, digital logic, what a CPU actually does |
| **II — Inside the 8086** | 5–18 | Architecture, registers, segmentation, every pin, bus timing, system design |
| **III — The instruction set** | 19–32 | Addressing modes, machine encoding, every instruction group in full |
| **IV — Programming** | 33–46 | NASM, `.COM` vs `.EXE`, DOS & BIOS services, worked programs, debugging |
| **V — Interfacing** | 47–55 | 8255, 8253/54, 8259A, 8251A, 8237, ADC/DAC, 8279, 8087 |
| **Appendices** | A–H | Instruction reference, opcode map, interrupt reference, ASCII, pins, timing, glossary, solutions |

---

## Contents

### Part I — Foundations

| # | Chapter | What you get |
|---|---------|--------------|
| 0 | [Introduction](00-introduction.md) | Why a 45-year-old chip is the right one to learn on |
| 1 | [Setting up the toolchain](01-toolchain-setup.md) | NASM, DOSBox, DEBUG.EXE, an editor, and a first working build |
| 2 | [Number systems](02-number-systems.md) | Binary, hex, BCD, two's complement, overflow — the arithmetic the flags describe |
| 3 | [Digital logic recap](03-digital-logic-recap.md) | Gates, latches, decoders, buses, tri-state — enough to read a schematic |
| 4 | [What a processor actually does](04-what-a-cpu-does.md) | Fetch–decode–execute, built up from a hand-traced machine |

### Part II — Inside the 8086

| # | Chapter | What you get |
|---|---------|--------------|
| 5 | [The family](05-family-history.md) | 4004 → 8008 → 8080 → 8085 → 8086/8088 → 80186/286, and what each change bought |
| 6 | [Architecture: BIU and EU](06-architecture-biu-eu.md) | The two halves of the chip, the instruction queue, why pipelining works |
| 7 | [The registers](07-registers.md) | All fourteen, their hidden specialisations, and which instructions assume which |
| 8 | [The FLAGS register](08-flags.md) | Nine flags, bit by bit, with the exact rule that sets each one |
| 9 | [Memory segmentation](09-segmentation.md) | Why 20 bits from 16-bit registers, physical address generation, aliasing |
| 10 | [Memory organisation](10-memory-organisation.md) | Even/odd banks, `BHE`/`A0`, alignment penalties, the reserved vectors |
| 11 | [The pin diagram](11-pin-diagram.md) | All 40 pins, one at a time, in both minimum and maximum mode |
| 12 | [Clock, reset and the 8284A](12-clock-reset-8284.md) | Duty cycle, `READY` synchronisation, power-on reset timing |
| 13 | [Bus cycles and timing](13-bus-cycles-timing.md) | T1–T4, wait states, read and write waveforms read line by line |
| 14 | [Minimum mode systems](14-minimum-mode.md) | A complete schematic: latches, transceivers, decoding, control |
| 15 | [Maximum mode and the 8288](15-maximum-mode-8288.md) | Status codes `S2–S0`, bus arbitration, why multiprocessing needs it |
| 16 | [Memory interfacing](16-memory-interfacing.md) | Address decoding worked out for EPROM and SRAM, with maps and timing margins |
| 17 | [I/O interfacing](17-io-interfacing.md) | Isolated vs memory-mapped I/O, the 64 K port space, decoding examples |
| 18 | [8088 vs 8086](18-8088-differences.md) | What halving the bus changed, and why the IBM PC used the slower part |

### Part III — The instruction set

| # | Chapter | What you get |
|---|---------|--------------|
| 19 | [Addressing modes](19-addressing-modes.md) | All modes with effective-address arithmetic worked out numerically |
| 20 | [Machine code encoding](20-machine-encoding.md) | Prefix, opcode, ModR/M, displacement, immediate — assembling by hand |
| 21 | [Data transfer](21-data-transfer.md) | `MOV` `XCHG` `LEA` `LDS` `LES` `PUSH` `POP` `IN` `OUT` `XLAT` `LAHF` `SAHF` |
| 22 | [Arithmetic](22-arithmetic.md) | `ADD` `ADC` `SUB` `SBB` `CMP` `INC` `DEC` `NEG`, and multi-precision maths |
| 23 | [Multiply and divide](23-multiply-divide.md) | `MUL` `IMUL` `DIV` `IDIV` `CBW` `CWD`, the divide-overflow trap |
| 24 | [BCD and ASCII adjust](24-bcd-ascii-adjust.md) | `DAA` `DAS` `AAA` `AAS` `AAM` `AAD` — the six instructions nobody explains |
| 25 | [Logical instructions](25-logical.md) | `AND` `OR` `XOR` `NOT` `TEST`, and bit manipulation idioms |
| 26 | [Shifts and rotates](26-shift-rotate.md) | `SHL` `SHR` `SAL` `SAR` `ROL` `ROR` `RCL` `RCR`, with the carry path drawn |
| 27 | [Jumps and loops](27-jumps-and-loops.md) | Short/near/far, all the conditional jumps, `LOOP` `LOOPE` `LOOPNE` `JCXZ` |
| 28 | [Procedures and the stack](28-procedures-and-stack.md) | `CALL` `RET`, stack frames, parameter passing, recursion, `BP` discipline |
| 29 | [String instructions](29-string-instructions.md) | `MOVS` `CMPS` `SCAS` `LODS` `STOS`, `REP` prefixes, the direction flag |
| 30 | [Processor control](30-processor-control.md) | `CLC` `STC` `CMC` `CLD` `STD` `CLI` `STI` `HLT` `WAIT` `LOCK` `ESC` `NOP` |
| 31 | [Interrupts](31-interrupts.md) | The vector table, `INT` `INT3` `INTO` `IRET`, `NMI`, priority, latency |
| 32 | [Instruction timing](32-instruction-timing.md) | Clock counts, EA overhead, queue effects, how to count cycles honestly |

### Part IV — Programming

| # | Chapter | What you get |
|---|---------|--------------|
| 33 | [NASM and program structure](33-nasm-directives.md) | `org` `section` `db/dw/dd` `equ` `times` `$` `$$`, and MASM equivalents |
| 34 | [Your first program, byte by byte](34-first-program-byte-by-byte.md) | Hello world disassembled to the last byte, hex dump annotated |
| 35 | [`.COM` versus `.EXE`](35-com-vs-exe.md) | The PSP, the EXE header, relocation, when you need each |
| 36 | [DOS services (INT 21h)](36-dos-int21.md) | Console, file, directory and memory functions with runnable examples |
| 37 | [BIOS services](37-bios-services.md) | `INT 10h` video, `INT 16h` keyboard, `INT 13h` disk, `INT 1Ah` clock |
| 38 | [Macros and modular programs](38-macros-and-modules.md) | `%macro`, `%rep`, conditional assembly, multi-file builds and linking |
| 39 | [Programs: arithmetic](39-programs-arithmetic.md) | From 8-bit addition to 64-bit multiply, each one traced |
| 40 | [Programs: arrays](40-programs-arrays.md) | Sum, min/max, reverse, rotate, merge, frequency count |
| 41 | [Programs: sorting and searching](41-programs-sorting-searching.md) | Bubble, selection, insertion, binary search, with cycle counts compared |
| 42 | [Programs: strings](42-programs-strings.md) | Length, copy, compare, palindrome, case conversion, token split |
| 43 | [Programs: number conversion](43-programs-number-conversion.md) | Binary↔ASCII↔hex↔BCD, the routines every other program needs |
| 44 | [Programs: matrices](44-programs-matrices.md) | Addition, transpose, multiply, with index arithmetic derived |
| 45 | [Programs: graphics](45-programs-graphics.md) | Mode 13h, pixel plotting, lines, circles, a bouncing ball |
| 46 | [Debugging](46-debugging.md) | `DEBUG.EXE` and the DOSBox-X debugger: trace, breakpoints, reading memory |

### Part V — Interfacing

| # | Chapter | What you get |
|---|---------|--------------|
| 47 | [8255A PPI](47-8255-ppi.md) | Modes 0/1/2, BSR mode, control word derived bit by bit, LED and switch programs |
| 48 | [8253/8254 timer](48-8253-8254-timer.md) | All six modes with waveforms, square-wave and delay programs |
| 49 | [8259A interrupt controller](49-8259a-pic.md) | ICW1–ICW4, OCW1–OCW3, cascading, the PC's IRQ map |
| 50 | [8251A USART](50-8251a-usart.md) | Mode and command words, baud rates, a serial echo program |
| 51 | [8237 DMA controller](51-8237-dma.md) | Cycle stealing vs burst, registers, `HOLD`/`HLDA` handshake |
| 52 | [ADC and DAC](52-adc-dac.md) | ADC0808 and DAC0800 interfacing, polled and interrupt-driven sampling |
| 53 | [Motors, displays and the 8279](53-stepper-seven-segment-8279.md) | Stepper sequencing, multiplexed 7-segment, keyboard/display controller |
| 54 | [The 8087 coprocessor](54-8087-coprocessor.md) | The stack model, `ESC` and `WAIT`, 80-bit reals, a float program |
| 55 | [Where to go next](55-where-next.md) | 80286 protected mode, 386 and beyond, and how this connects to OS work |

### Appendices

| # | Appendix | What you get |
|---|----------|--------------|
| A | [Instruction set reference](A-instruction-reference.md) | Every instruction: operands, encoding, flags, clocks — one table |
| B | [Opcode map](B-opcode-map.md) | The full 256-entry one-byte map plus group extensions |
| C | [Interrupt reference](C-interrupt-reference.md) | `INT 21h`, `10h`, `16h`, `13h`, `1Ah` function lists |
| D | [ASCII and scan codes](D-ascii-table.md) | Full table plus the PC keyboard scan-code set |
| E | [Pin reference](E-pin-reference.md) | 8086, 8088, 8284A, 8288, 8255, 8253, 8259A pinouts in one place |
| F | [Timing and electrical data](F-timing-electrical.md) | AC/DC characteristics, the numbers a real design needs |
| G | [Glossary](G-glossary.md) | Every term the book uses, defined |
| H | [Exercise solutions](H-exercise-solutions.md) | Worked answers to every chapter's exercises |

---

## Conventions used throughout

**Number bases.** Hexadecimal is written `0x1F` in prose and NASM listings, and `1Fh` when quoting a
datasheet or MASM source. Binary is `0b10100011` or `10100011b`. A bare number is decimal.

**Signals.** An active-low signal is written with an overbar in figures, and as a trailing `#` in
text where an overbar is impossible: `RD#`, `WR#`, `MEMR#`. The datasheet's own notation appears in
the pin chapters.

**Addresses.** A segmented address is `CS:IP` form, e.g. `0700:0100`. A physical (20-bit) address is
a five-digit hex number, e.g. `0x07100`.

**Register contents.** `AX = 0x1234` means the whole 16-bit register; `AH = 0x12`, `AL = 0x34`.

**Bit numbering.** Bit 0 is the least significant bit, always. `D7–D0` for a byte, `D15–D0` for a
word — this matches Intel's datasheets.

**Code listings** are complete and runnable unless a comment says otherwise. Fragments meant only to
show a form are marked `; fragment`.

---

## Source code

Every complete program in the book is also in [`code/`](code/), organised by chapter, so you can
assemble and run without retyping.

## Figures

Figures live in [`images/`](images/) as SVG. They are drawn to be read, not decorated: a timing
diagram shows the actual transitions at the actual T-states, and a schematic shows every connection
needed to build the thing.

---

*Start here: [Chapter 0 — Introduction](00-introduction.md)*
