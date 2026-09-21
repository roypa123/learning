# Appendix G — Glossary

[← Appendix F](F-timing-electrical.md) · [Contents](README.md) · [Appendix H →](H-exercise-solutions.md)

---

Every term the book uses. Chapter references point at the full explanation.

---

## A

**Accumulator** — `AX`, or `AL` for byte operations. The register that `MUL`, `DIV`, `IN`, `OUT`,
`XLAT` and the string instructions assume, and which has shorter encodings for several instructions.
Chapter 7 §2.1.

**Active low** — a signal that performs its function when at 0 V rather than 5 V, written `RD#` or
with an overbar. Common because TTL outputs sink more current than they source. Chapter 3 §4.

**Address bus** — the twenty lines `A19`–`A0` that say *which* location. Their number sets the
address space: 2²⁰ = 1 MiB. Chapter 4 §3.

**Addressing mode** — how an instruction names its operand: register, immediate, direct, register
indirect, based, indexed, or based-indexed. Chapter 19.

**AF** — the auxiliary carry flag, set by a carry out of bit 3. Used only by the six BCD adjust
instructions; no conditional jump tests it. Chapter 8 §6.

**Aliasing** — (1) in address decoding, a device responding at several addresses because not all
address lines are decoded (Chapter 3 §5.2); (2) in sampling, a false low-frequency signal produced
by sampling too slowly (Chapter 52 §3.1); (3) in segmentation, the 4096 `segment:offset` pairs that
name one physical address (Chapter 9 §5).

**ALE** — Address Latch Enable, the 8086 output that pulses during T1 while the address is on the
multiplexed pins. It clocks the external latches that demultiplex the bus. Chapter 3 §6.1,
Chapter 13 §3.

**ALU** — Arithmetic and Logic Unit. Combinational logic performing add, subtract, AND, OR, XOR and
shift, producing a result and status bits. Chapter 4 §2.2.

**Anti-aliasing filter** — a low-pass filter placed *before* an ADC to remove frequencies above half
the sample rate. Chapter 52 §3.1.

**ASCIIZ** — a zero-terminated string, as DOS file functions require — as opposed to the
`$`-terminated form function 09h needs. Chapter 36 §5.1.

**Assembler** — a program that turns mnemonics into machine code. This book uses NASM. Chapter 1 §1.

**Auto-initialise** — an 8237 DMA mode in which the address and count reload automatically at the
end of a transfer. Chapter 51 §8.

---

## B

**Base register** — `BX` or `BP`, the registers that may appear as the base of a memory operand.
Chapter 19 §6.

**BCD** — Binary Coded Decimal: decimal digits stored one per nibble (packed) or one per byte
(unpacked). Chapter 2 §8, Chapter 24.

**BHE#** — Bus High Enable. With `A0`, selects which of the two memory banks responds. Chapter 10
§2.

**BIOS** — Basic Input/Output System. Firmware in ROM providing hardware services through `INT 10h`
and friends. Chapter 37.

**BIU** — Bus Interface Unit. The half of the 8086 that owns the outside world: segment registers,
`IP`, the address adder, bus control and the prefetch queue. Chapter 6 §4.

**Break** — a serial condition in which the line is held in the space state longer than a whole
frame. Chapter 50 §3.3.

**Bresenham's algorithm** — drawing a line or circle using only integer addition. Chapter 45 §4,
§6.

**Bus contention** — two devices driving the same bus line at once. Excessive current, corrupted
data, and eventual chip failure. Chapter 3 §8.

**Bus cycle** — one complete transaction on the bus: four T-states plus any wait states. Also
*machine cycle*. Chapter 13.

---

## C

**Cascade** — connecting a second 8259A to one of the first's inputs, giving fifteen interrupt
sources rather than eight. Chapter 49 §6.

**CF** — the carry flag. Set when a result does not fit as an **unsigned** value: a carry out on
addition, a borrow on subtraction. Also the bit shifted out by every shift and rotate. Chapter 8 §4.

**CHS** — Cylinder, Head, Sector — the three coordinates of a disk sector. Cylinders and heads count
from 0; **sectors count from 1**. Chapter 37 §4.

**Circular buffer** — a fixed array used as a queue, with head and tail indices that wrap. Making
its size a power of two lets `AND` replace division. Chapter 50 §8.

**CMOS** — (1) the logic family; (2) the battery-backed configuration RAM in a PC, at ports
`70h`/`71h`.

**Code page 437** — the IBM PC's extended character set, containing the box-drawing and block
characters. Appendix D §3.

**`.COM`** — a DOS executable that is a raw memory image, loaded at offset `0x100`, with all four
segment registers equal. Maximum 65,280 bytes. Chapter 35 §2.

**Control bus** — the signals saying what kind of access is happening and when: `RD#`, `WR#`,
`M/IO#`, `ALE`, `DT/R#`, `DEN#`. Chapter 4 §3.

**Control word** — a byte written to a peripheral's control register to configure it. The 8255's,
the 8253's and the 8251A's are each derived bit by bit in their chapters.

---

## D

**DAC / ADC** — Digital-to-Analogue and Analogue-to-Digital Converters. Chapter 52.

**Debouncing** — ignoring the multiple transitions a mechanical switch produces over 5–20 ms.
Chapter 47 §4.

**DEN#** — Data Enable. The 8086 output that switches the data bus transceivers on, only during the
data phase. Chapter 13 §3.

**DF** — the direction flag. 0 makes string instructions count `SI`/`DI` upwards; 1 downwards. The
convention is 0. Chapter 8 §8.2.

**Displacement** — the signed constant added to a base and index to form an effective address; also
the signed offset in a relative jump. Chapter 19 §6, Chapter 27 §1.1.

**DMA** — Direct Memory Access: a controller moving data between a device and memory without the
processor, in one bus cycle instead of two. Chapter 51.

**DT/R#** — Data Transmit/Receive. Sets the direction of the data bus transceivers: 1 = the CPU is
writing. Chapter 13 §3.

**DTA** — Disk Transfer Area. The 43-byte block DOS fills with directory information during a
find-first/find-next search. Chapter 36 §7.

---

## E

**EA** — Effective Address. The 16-bit offset computed from an addressing mode, before the segment
is added. Its computation costs 5 to 12 clocks. Chapter 19 §7.

**Endianness** — the order of bytes in a multi-byte value. The 8086 is **little-endian**: the low
byte is at the lower address. Chapter 2 §7.

**EOI** — End Of Interrupt. The command every hardware interrupt handler must write to the 8259A,
clearing its In-Service bit. Forgetting it means the interrupt fires exactly once. Chapter 49 §4.2.

**EPROM** — Erasable Programmable Read-Only Memory. Non-volatile, slow (200–450 ns), and what holds
the reset vector. Chapter 16 §1.

**`ESC`** — the 8086 instruction (opcodes `D8`–`DF`) that hands an operand address to a coprocessor.
Without an 8087 it does nothing at all. Chapter 30 §9.

**EU** — Execution Unit. The half of the 8086 containing the general registers, the ALU, the flags
and the decoder. It has no connection to the pins. Chapter 6 §5.

**`.EXE`** — a DOS executable with an `MZ` header and a relocation table, supporting multiple
segments and any size. `DS` points at the PSP at entry, not at your data. Chapter 35 §4.

---

## F

**Far** — a pointer or control transfer that includes a segment as well as an offset: 32 bits, and
`CALL`/`RET` push and pop `CS` too. Chapter 28 §7.

**Flags** — the 16-bit register holding `CF`, `PF`, `AF`, `ZF`, `SF`, `TF`, `IF`, `DF` and `OF`.
Chapter 8.

**Framing error** — a serial receive error in which the stop bit was not high. Almost always a baud
rate mismatch. Chapter 50 §4.1.

---

## H

**Handle** — the small integer DOS returns from `3Ch`/`3Dh` to identify an open file. Chapter 36
§5.

**HMOS** — the process the 8086 was fabricated in: high-performance N-channel MOS, 3 µm.

**HOLD / HLDA** — the minimum-mode bus request handshake used by a DMA controller. Chapter 11 §8.8.

---

## I

**ICW** — Initialisation Command Word. The two to four bytes that configure an 8259A, in a fixed
order. Chapter 49 §3.

**IF** — the interrupt flag. 1 accepts maskable interrupts on `INTR`. Does not affect `NMI`,
software `INT` or exceptions. Chapter 8 §8.1.

**Immediate** — an operand that is a constant stored inside the instruction. Chapter 19 §3.

**Index register** — `SI` or `DI`. Chapter 19 §6.

**Interrupt** — a forced call to a handler whose address comes from the vector table. Chapter 31.

**IRR / ISR / IMR** — the 8259A's Interrupt Request, In-Service and Interrupt Mask registers.
Chapter 49 §2.1.

**IVT** — Interrupt Vector Table. 256 four-byte entries at physical `0x00000`–`0x003FF`; vector *n*
is at `4n`. Chapter 31 §2.

---

## L

**Latch** — level-triggered storage: transparent while its enable is asserted, holding otherwise.
The 74LS373 is the one that demultiplexes the 8086's address. Chapter 3 §6.

**LOCK#** — the maximum-mode signal asserted by the `LOCK` prefix, preventing another bus master
from interleaving. Chapter 30 §8.

---

## M

**Machine cycle** — see *bus cycle*.

**Maximum mode** — `MN/MX#` grounded. The 8086 emits status codes for an 8288 to decode, and
supports an 8087 and bus arbitration. Chapter 15.

**Minimum mode** — `MN/MX#` at +5 V. The 8086 generates its own bus control signals. Chapter 14.

**ModR/M** — the byte after most opcodes, containing `mod` (2 bits), `reg` (3) and `r/m` (3). It
encodes which register and which addressing mode. Chapter 20 §3.

**Multiplexing** — (1) sharing pins between address and data at different times, which the 8086 does
on `AD15`–`AD0` (Chapter 11 §1); (2) lighting display digits one at a time fast enough to look
continuous (Chapter 47 §8).

---

## N

**Near** — a pointer or control transfer within the current segment: 16 bits. Chapter 28 §7.

**NMI** — Non-Maskable Interrupt. Edge-triggered, always accepted, always type 2. Chapter 31 §5.2.

**Nibble** — four bits; one hexadecimal digit.

**Null modem** — a serial cable that crosses `TxD`/`RxD`, `RTS`/`CTS` and `DTR`/`DSR`, for connecting
two computers. Chapter 50 §10.2.

**Nyquist rate** — twice the highest frequency present; the minimum sample rate. Chapter 52 §3.1.

---

## O

**OCW** — Operation Command Word. The three run-time control words of an 8259A: the mask, the EOI
and the IRR/ISR read. Chapter 49 §4.

**OF** — the overflow flag. Set when a result does not fit as a **signed** value: when the carry into
the sign bit differs from the carry out. Chapter 8 §5.

**Opcode** — the byte identifying an instruction. Appendix B.

**Overrun error** — a serial receive error in which a character arrived before the previous one was
read. **A software fault, not a line fault.** Chapter 50 §4.1.

---

## P

**Page register** — the external latch supplying address bits `A19`–`A16` for a DMA channel, because
the 8237 has only 16 address bits. It does not increment, so a DMA buffer must not cross a 64 KiB
boundary. Chapter 51 §5.

**Paragraph** — sixteen bytes; the granularity of a segment base. A segment register holds a
paragraph number. Chapter 9 §3.3.

**Parity** — (1) the `PF` flag, set when the low byte of a result has an **even** number of 1 bits
(Chapter 8 §7); (2) the serial error-detection bit (Chapter 50 §1.1).

**PF** — see *parity*.

**Prefetch queue** — the six-byte FIFO the BIU fills with instruction bytes ahead of time. Flushed
by every taken jump. Chapter 6 §3.

**Prefix** — a byte before an opcode: a segment override (`26`, `2E`, `36`, `3E`), `LOCK` (`F0`) or
`REP` (`F2`, `F3`). One byte, two clocks each. Chapter 20 §8.

**Protected mode** — the 80286's mode in which a segment register holds a selector into a descriptor
table, giving limits, access rights and privilege levels. Chapter 55 §2.

**PSP** — Program Segment Prefix. The 256-byte control block DOS builds before every program, holding
the command tail, the environment segment and the memory size. Chapter 35 §3.

**PWM** — Pulse Width Modulation: controlling average power by varying the on/off ratio rather than
the voltage. Chapter 53 §2.

---

## Q

**Queue** — see *prefetch queue*.

**Quantisation** — the rounding an ADC performs; the step size is the full-scale range divided by
2ⁿ. Chapter 52 §2.

---

## R

**Real mode** — the 8086's only mode, and the one every x86 processor still powers on into:
segmentation by shift-and-add, no protection, 1 MiB. Chapter 9.

**READY** — the 8086 input that, when low, inserts wait states. Must be synchronised to the clock,
which is the 8284A's job. Chapter 13 §7.

**Relocation** — adjusting segment values in an `.EXE` image at load time. A `.COM` program needs
none, because it contains no segment values. Chapter 35 §4.2.

**REP** — the prefix that repeats a string instruction `CX` times. Chapter 29 §3.

**RS-232** — the serial electrical standard: logic 1 is −3 to −15 V, logic 0 is +3 to +15 V —
inverted and bipolar relative to TTL. Chapter 50 §10.

---

## S

**Scan code** — the position-based code a keyboard produces, as opposed to ASCII. `INT 16h` returns
both. Appendix D §4.

**Segment** — a 64 KiB window starting at a paragraph boundary. Chapter 9.

**Segment override** — a prefix forcing a different segment register for one instruction. One byte,
two clocks. Cannot override an instruction fetch, a stack access, or a string destination.
Chapter 9 §4.1.

**SF** — the sign flag: a copy of the result's most significant bit. Chapter 8 §3.

**Stack frame** — the region of stack a procedure builds with `push bp` / `mov bp, sp`, holding
parameters at positive offsets from `BP` and locals at negative ones. Chapter 28 §5.

**Successive approximation** — the ADC technique that tests one bit at a time, so *n* bits take *n*
clock periods. Chapter 52 §4.

---

## T

**T-state** — one clock period; 200 ns at 5 MHz. Chapter 13 §1.

**TF** — the trap flag. 1 generates `INT 1` after every instruction; this is how single-step
debuggers work. Chapter 8 §8.3, Chapter 46 §4.

**Three-state** — see *tri-state*.

**Tri-state** — an output that can be 0, 1 or high-impedance (electrically disconnected). What makes
a shared bus possible. Chapter 3 §8.

**TSR** — Terminate and Stay Resident: a DOS program that exits but leaves part of itself in memory,
usually hooked to an interrupt. `INT 21h`, `AH = 31h`.

**Two's complement** — the signed representation in which negation is invert-all-bits-and-add-one,
so that one adder serves both signed and unsigned arithmetic. Chapter 2 §5.2.

---

## U

**USART** — Universal Synchronous/Asynchronous Receiver Transmitter. The 8251A. Chapter 50.

---

## V

**Vector** — a four-byte interrupt table entry: offset then segment. Chapter 31 §2.

**Von Neumann architecture** — instructions and data in the same memory, so a program can be loaded
and modified like data — and so a runaway `IP` can execute your string table. Chapter 4 §1.

---

## W

**Wait state** — an extra clock period inserted between T3 and T4 while `READY` is low, giving slow
memory more time. Chapter 13 §7.

**Word** — sixteen bits; the 8086's natural quantity. A word at an odd address costs two bus cycles
instead of one. Chapter 2 §2, Chapter 10 §4.

---

## X

**XLAT** — the one-byte instruction computing `AL = [DS:BX + AL]`. The fastest byte-to-byte lookup
the 8086 has. Chapter 21 §8.

---

## Z

**ZF** — the zero flag, set when a result is exactly zero. Chapter 8 §2.

---

## Numbers and symbols

**`$`** — in NASM, the address of the current line; also DOS function 09h's string terminator.
Chapter 33 §5.

**`$$`** — in NASM, the start of the current section. Chapter 33 §5.

**8-N-1** — a serial format: 8 data bits, no parity, one stop bit. The 8251A mode word `0x4E`.
Chapter 50 §3.2.

**640 K** — the conventional memory limit, arising from IBM putting the video adapter at physical
`0xA0000` in 1981. Chapter 10 §6.5.

---

[← Appendix F](F-timing-electrical.md) · [Contents](README.md) · [Appendix H →](H-exercise-solutions.md)
