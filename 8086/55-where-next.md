# Chapter 55 — Where to go next

[← The 8087 coprocessor](54-8087-coprocessor.md) · [Contents](README.md) · [Appendix A →](A-instruction-reference.md)

---

## Goal

What you now know, what the natural next steps are, and how each one connects back to what this book
covered. This is the last chapter.

---

## 1. What you have

Working through Parts I–V, you can now:

- **read the 8086 datasheet** — every pin, every timing parameter, every bus cycle;
- **assemble and disassemble by hand**, including ModR/M bytes;
- **predict the clock count** of any instruction sequence, and say which of two implementations is
  faster and why;
- **write a complete DOS program**, from `org 0x100` to the last `$`;
- **design a small computer** — clock, reset, latches, transceivers, decoding, memory, peripherals —
  and say what every chip is for;
- **debug at the instruction level**, with a debugger or without one.

That last one transfers to every architecture you will ever meet. The specific opcodes do not; the
habit of asking "what is actually in that register, and how did it get there?" does.

---

## 2. The 80286 and protected mode

The direct next step, and the one that makes the most of what you know.

### 2.1 What changes

Real mode on an 80286 is identical to an 8086 — same registers, same instructions, same
segmentation. **Protected mode** changes one thing, and everything follows from it:

> A segment register no longer holds a paragraph address. It holds a **selector** — an index into a
> table of **descriptors**.

```
   8086 real mode:
       physical = segment × 16 + offset

   80286 protected mode:
       selector -> descriptor table -> { base, limit, access rights }
       physical = base + offset,  provided offset <= limit and the
                                  access rights permit it
```

### 2.2 What that buys

**A limit.** A segment can be 1 byte or 64 KiB, and an access beyond it faults. On an 8086 a runaway
pointer silently corrupts the interrupt vector table; on an 80286 it raises a general protection
fault.

**Access rights.** A segment can be read-only, execute-only, or unreadable to unprivileged code.

**Four privilege rings.** Ring 0 for the kernel, ring 3 for applications, with controlled gates
between them.

**A 24-bit physical address** — 16 MiB instead of 1 MiB.

**A larger virtual space.** Each task has its own descriptor table, so the total addressable space is
1 GiB even though physical memory is 16 MiB.

### 2.3 What to read

Intel's *80286 Programmer's Reference Manual* is the original and is still the clearest. The chapters
on descriptors and gates are the ones that matter; the rest is the 8086 you already know.

### 2.4 The catch

The 80286 could enter protected mode but not leave it cleanly — the only documented way back to real
mode was a processor reset. A decade of ingenious workarounds followed, including triple-faulting
deliberately and stashing a return address in CMOS RAM. The 80386 fixed it.

---

## 3. The 80386 and 32-bit

### 3.1 What changes

**32-bit registers.** `EAX`, `EBX`, `ESI` and so on, with the 16-bit names still addressing the
bottom halves. `AX` is `EAX`'s low 16 bits exactly as `AL` is `AX`'s low 8.

**32-bit addresses**, so 4 GiB, and a flat memory model becomes practical: set every segment base to
0 and every limit to 4 GiB, and segmentation disappears from view.

**Better addressing modes.** Any register can be a base or index, and the index can be scaled:

```
   mov eax, [ebx + ecx*4 + 8]      ; legal on a 386, impossible on an 8086
```

That one change removes most of Chapter 19's restrictions. Array indexing stops needing a shift.

**Paging.** A second translation layer under segmentation, mapping 4 KiB pages through page tables.
This is what makes virtual memory, demand paging and per-process address spaces possible.

**Virtual 8086 mode.** A protected-mode operating system can run a real-mode program — a DOS
application — in a sandbox, with its segmentation emulated. This is how Windows 3.x and DOS boxes
worked.

**New instructions:** `MOVZX` and `MOVSX` (zero and sign extending moves), `BT`/`BTS`/`BTR` (bit
test), `SETcc`, `SHLD`/`SHRD` (double shifts), and near conditional jumps with a 32-bit
displacement — which finally removes the ±127 limit of Chapter 27 §3.

### 3.2 The through-line

Everything you learned still applies. `MOV`, `ADD`, `JMP`, `CALL`, the flags, the stack, the
interrupt vector mechanism — all unchanged, just wider. The ModR/M byte gained a SIB byte and a
prefix for 32-bit operands, but its structure is the same three fields.

---

## 4. Modern x86-64

Three further layers.

**x86-64 (2003).** Sixteen 64-bit registers (`RAX`…`R15`), 64-bit addresses, and **segmentation
finally vestigial** — in 64-bit mode the segment bases are forced to zero and the limits ignored.
`CS`, `DS`, `SS` and `ES` still exist but do nothing. The mechanism you learned in Chapter 9 was
retired after 25 years.

**SSE/AVX.** Wide vector registers, 128 to 512 bits, operating on several values at once. A separate
instruction set alongside the integer one.

**Micro-architecture.** Out-of-order execution, register renaming, branch prediction, three levels of
cache, speculative execution. **The clock counts of Chapter 32 no longer exist**: an instruction has
no fixed cost, because its cost depends on what is in cache and what else is in flight.

That last point is worth dwelling on. On an 8086, `MOV AX, [BX]` costs 13 clocks, always. On a
modern processor it costs 4 cycles from L1 cache, 12 from L2, 40 from L3 or 200 from DRAM — and it
may execute before the instruction written above it. **Performance reasoning became statistical.**

The 8086 is the last x86 where you can say exactly what will happen. That is precisely why it is
worth learning on.

---

## 5. Operating systems

The natural project. You already have most of what you need.

**A boot sector** is a 512-byte real-mode program loaded at `0x7C00` by the BIOS. You know how to
write one: `org 0x7C00`, `INT 13h` to read more sectors (Chapter 37 §4), `INT 10h` to print
(Chapter 37 §2.6), and `times 510-($-$$) db 0` / `dw 0xAA55` to pad and sign it (Chapter 33 §2.3).

**The steps from there:**

1. Boot sector → load a second-stage loader from disk.
2. Set up a GDT and enter protected mode.
3. Set up an IDT and remap the 8259A away from `INT 08h`–`0Fh` (Chapter 49 §3.2), which collide with
   the 80286's exceptions.
4. Enable paging.
5. A physical memory manager, then a heap.
6. Tasks and a context switch — which is `PUSHA`, swap `SP`, `POPA`, `IRET`.
7. A scheduler driven by the 8253's IRQ0 (Chapter 48 §9).
8. System calls through an interrupt gate.
9. A disk driver, a filesystem, a shell.

**Every one of those steps uses something from this book.** The interrupt vector table, the 8259A,
the 8253, the stack frame, the segment registers.

Your `operating_system/` directory already walks this path for the 32-bit case; this book is the
layer underneath it.

---

## 6. Embedded systems

If what appealed was Part V — the chips, the decoding, the timing — that work still exists, on
modern parts.

| 8086-era concept | Modern equivalent |
|------------------|-------------------|
| 8255 parallel I/O | GPIO peripheral, on-chip |
| 8253 timer | timer/counter peripheral, with PWM built in |
| 8259A interrupt controller | NVIC (ARM) or PLIC (RISC-V), on-chip |
| 8251A USART | UART peripheral, on-chip, with its own baud generator |
| 8237 DMA | DMA controller, on-chip |
| ADC0808 | ADC peripheral, 12-bit, on-chip |
| Address decoding | memory-mapped peripherals at fixed addresses |

**They are all on the die now**, but they are programmed the same way: write a control word to a
register, poll a status bit or take an interrupt. An STM32's timer has modes that map almost
one-to-one onto the 8253's.

Good next steps: an **ARM Cortex-M** (STM32, or a Raspberry Pi Pico's RP2040), or a **RISC-V** part.
Both have free toolchains and clean documentation. The transition is much easier having done this —
you already know what a peripheral register is and why it needs a delay after a write.

---

## 7. Retro hardware

Building an actual 8086 system is entirely feasible, and everything in Part II is buildable.

**The parts are still available** — 8086, 8284A, 8288, 74LS373, 74LS245, 62256 SRAM, 27C256 EPROM.
Suppliers and surplus dealers carry them, and they are cheap.

**Simplifications worth making:**

- Use an **80C88** rather than an 8086: fully static, so you can single-step by stopping the clock
  (Chapter 18 §7), and it needs only one transceiver and two latches.
- Use **62256 SRAM** rather than DRAM — no refresh, no multiplexing (Chapter 16 §8).
- Use a **GAL or CPLD** for address decoding instead of a 74LS138, so you can change the memory map
  without rewiring.
- Use a **modern USB-serial bridge** rather than an RS-232 level shifter.

**A minimal working system** is: 80C88, 8284A, two 74LS373, one 74LS245, a 27C256 EPROM, a 62256
SRAM, a 74LS138, and a 16550 UART. Nine chips, one board, and it runs code you write.

The established modern projects to look at are the *Homebrew 8088* designs and the various
*8088-based single-board computers* documented on retrocomputing forums — several publish complete
schematics and BIOS listings.

---

## 8. Other architectures worth meeting

**6502** — 3,500 transistors, 56 instructions, and the chip in the Apple II, the Commodore 64 and
the NES. Even simpler than the 8086, with a beautiful zero-page addressing model. Worth a weekend.

**Z80** — the 8080's other descendant, and the 8086's contemporary competitor. Two register banks,
block instructions, and a cleaner instruction set than the 8086's.

**68000** — Motorola's 1979 answer: sixteen 32-bit registers, a flat 16 MiB address space, and
**no segmentation**. Reading its manual after this book is instructive, because it shows what Intel
gave up to get 8080 compatibility.

**RISC-V** — the modern contrast. Fixed 32-bit instructions, 32 registers, no flags register at all,
no addressing modes beyond register-plus-offset. Everything the 8086 does with a complex encoding,
RISC-V does with more, simpler instructions. Seeing both makes the trade-off concrete.

---

## 9. Ten things worth remembering

If you take nothing else from this book:

1. **A processor is a state machine that fetches, decodes and executes.** Everything else is
   optimisation of those three steps.

2. **The instruction encoding explains the instruction set.** `[AX]` is illegal because there is no
   bit pattern for it, not because of a rule.

3. **Flags describe the last result in two ways at once** — signed and unsigned — and choosing the
   wrong conditional jump is the commonest logic error in assembly.

4. **The bus is the bottleneck.** Registers are free; memory is not; I/O is worse.

5. **Timing analysis is adding propagation delays and checking the total fits.** That is all it ever
   is, at any scale.

6. **Every peripheral is a set of registers**: write a control word, poll a status bit or take an
   interrupt. The 8255 and an STM32 GPIO differ in detail, not in kind.

7. **An interrupt is a forced call with the flags saved.** Once you have traced one by hand, every
   interrupt system is the same system.

8. **Hardware needs guards.** Timeouts on wait loops, range checks before jump tables, zero checks
   before division, `JCXZ` before `LOOP`.

9. **Measure before optimising, and know what the numbers mean.** Chapter 32's tables were useful
   because they were honest about what they did not model.

10. **Read the datasheet.** Everything in Part II came from one, and the skill of reading a timing
    diagram outlasts any particular chip.

---

## 10. A last exercise

Take the 28-byte `hello.com` from Chapter 34 and account for it completely, from memory:

- every byte, and why the assembler chose it;
- what DOS did before the first instruction ran;
- what the processor did on each of the six instructions;
- what happened inside `INT 21h`;
- how many clocks the whole thing took, and where they went;
- what the address `0x010D` is, and what would change it.

If you can do that without looking anything up, you understand the 8086.

---

## 11. The appendices

Everything that follows is reference material, laid out to be looked things up in rather than read.

| | |
|---|---|
| [A — Instruction set reference](A-instruction-reference.md) | every instruction: operands, encoding, flags, clocks |
| [B — Opcode map](B-opcode-map.md) | the 256-entry one-byte map and the group extensions |
| [C — Interrupt reference](C-interrupt-reference.md) | `INT 21h`, `10h`, `16h`, `13h`, `1Ah` |
| [D — ASCII and scan codes](D-ascii-table.md) | the full table, plus code page 437 and the keyboard |
| [E — Pin reference](E-pin-reference.md) | 8086, 8088, 8284A, 8288, 8255, 8253, 8259A, 8251A |
| [F — Timing and electrical data](F-timing-electrical.md) | the AC and DC characteristics a real design needs |
| [G — Glossary](G-glossary.md) | every term the book uses |
| [H — Exercise solutions](H-exercise-solutions.md) | worked answers to every chapter |

---

[← The 8087 coprocessor](54-8087-coprocessor.md) · [Contents](README.md) · [Appendix A →](A-instruction-reference.md)
