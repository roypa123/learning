# Appendix H — Exercise solutions

[← Appendix G](G-glossary.md) · [Contents](README.md)

---

Worked answers to every exercise. Where the reasoning matters more than the result, the reasoning is
given.

---

## Chapter 0

**0.1** 2²⁰ = 1,048,576 bytes = 1024 KiB = 1 MiB.

**0.2** 1 ÷ 5,000,000 = 200 ns. Four periods = 800 ns, so at most 1,250,000 memory reads per second.

**0.3** 20 address + 16 data = 36, plus 2 power + 2 ground + 1 clock = 41, which already exceeds 40
before a single control signal. Address and data therefore had to share pins.

**0.4** 65,536. The register holds 0–65,535; starting at 0 and decrementing wraps to 65,535 and takes
65,536 decrements to return to 0.

**0.5** Multi-byte values are stored **low byte first** — little-endian.

---

## Chapter 1

**1.1** The size changes by the difference in message length. `'The toolchain works.'` is 20
characters; a 9-character name makes the file 36 − 20 + 9 = 25 bytes.

**1.2** The message prints, followed by garbage, until DOS happens to find a `0x24` byte somewhere
after it. Function 09h prints until it meets `$`; with no terminator it walks on through whatever
follows in memory.

**1.3** Every address operand drops by `0x100`. `ba 0d 01` becomes `ba 0d 00`. At run time `DX`
points into the PSP, so DOS prints whatever is there — different garbage from 1.2.

**1.4** File offset `0x0B` + `0x100` = **`0x010B`**.

**1.5**

```asm
        cpu  8086
        org  0x100
start:  mov  ah, 0x09
        mov  dx, msg1
        int  0x21
        mov  ah, 0x09
        mov  dx, msg2
        int  0x21
        mov  ax, 0x4C00
        int  0x21
msg1:   db   'First line', 0x0D, 0x0A, '$'
msg2:   db   'Second line', 0x0D, 0x0A, '$'
```

**1.6** `AX = 0x4C00`, `IP = 0x010B`. `DX` still holds `0x010D` unless DOS changed it — it is not
guaranteed.

**1.7** NASM assembles `push 0x1234` to `68 34 12`. With `cputype=8086` the emulated processor does
not recognise opcode `0x68` and takes an invalid-opcode trap — which on an 8086 is undefined
behaviour, so DOSBox typically halts or executes something arbitrary.

---

## Chapter 2

**2.1** (a) `0x0FFF`. (b) `0xB65`. (c) 60,000 = `0xEA60`.

**2.2** (a) 32,767. (b) 65,535. (c) −1. (d) −128.

**2.3** −1 = `0xFFFF`; −128 = `0xFF80`; −256 = `0xFF00`; −32,768 = `0x8000`.

**2.4** Invert `1000 0000` → `0111 1111`, add 1 → `1000 0000` = `0x80` again, with `OF = 1`. +128
does not exist in a signed byte, so the negative range has one more value than the positive.

**2.5**

| | Result | `CF` | `OF` | `SF` | `ZF` |
|---|--------|------|------|------|------|
| (a) `0x7F + 0x7F` | `0xFE` | 0 | **1** | 1 | 0 |
| (b) `0x80 + 0xFF` | `0x7F` | **1** | **1** | 0 | 0 |
| (c) `0x40 + 0x40` | `0x80` | 0 | **1** | 1 | 0 |
| (d) `0xFF + 0x01` | `0x00` | **1** | 0 | 0 | **1** |

**2.6** Unsigned 32,768; signed −32,768. `NEG AX` leaves `0x8000` unchanged and sets `OF`, because
+32,768 has no 16-bit signed representation.

**2.7** `AX = 0xABCD`, `AH = 0xAB`, `AL = 0xCD`. The low byte comes from the lower address.

**2.8** (a) `0101 0111`. (b) `0x87`. (c) `0x08, 0x07`. (d) `0x38, 0x37`.

**2.9** `AX = 0x8000`, `BX = 0x0001`. Unsigned 32,768 > 1 so `JB` is *not* taken — you need the
reverse. Take `AX = 0x0001`, `BX = 0x8000`: unsigned 1 < 32,768 so `JB` **is** taken; signed
+1 > −32,768 so `JL` is not. They disagree because the top bit means "large" to an unsigned reading
and "negative" to a signed one.

**2.10** `0x47 + 0x38 = 0x7F`. Invalid BCD, because the low nibble `F` exceeds 9. `DAA` adds 6 to the
low nibble: `0x7F + 0x06 = 0x85` = BCD 85 ✔ (47 + 38 = 85).

**2.11** `78 56 34 12`.

**2.12** `0x9F3A + 0x76C8 = 0x1_1602`, so the 16-bit result is `0x1602` with a carry out —
`CF = 1`. Signed: `0x9F3A` = −24,774 and `0x76C8` = +30,408, different signs, so `OF = 0`.

---

## Chapter 3

**3.1** Eight rows; only one (all inputs 1) gives output 0.

**3.2** By De Morgan, `NOT(A) AND NOT(B)` = `NOT(A OR B)` = a **NOR** gate.

**3.3** `Y5#` corresponds to `CBA = 101`, i.e. `A15 A14 A13 = 101`, so addresses `0xA000`–`0xBFFF`.

**3.4** With `A19`–`A16` ignored, `0x0A000`, `0x1A000` and `0x2A000` all assert `Y5#`.

**3.5** `ALE` captures the address into a latch while it is still on the multiplexed pins, so it
remains stable after the CPU reuses those pins for data. Without the latch, memory would see the
address vanish and be replaced by data part-way through the cycle, and the access would go to a
random location.

**3.6** During a write, `DT/R# = 1` and `DEN# = 0`. Data flows from the `A` side (CPU) to the `B`
side (memory).

**3.7** A latch is level-triggered and transparent while enabled; a flip-flop is edge-triggered and
never transparent. The 74LS373 is a latch, which is right because `ALE` is a level that brackets the
period during which the address is valid.

**3.8** Both drive the data bus simultaneously. Where they disagree, one pulls high and one pulls
low, producing a short circuit through both output stages: excessive current, indeterminate data,
and chips that run hot and eventually fail.

**3.9** At 5 MHz a T-state is 200 ns, so three are 600 ns. After the CPU's own 110 ns address delay
and 30 ns data setup, about 460 ns remains, minus perhaps 90 ns of glue — roughly 370 ns. A 250 ns
chip fits; the question's 250 ns device does. (A 450 ns device would not.)

**3.10** A floating TTL input drifts into the forbidden band between 0.8 V and 2.0 V, where its value
is unpredictable and may differ between readings or with temperature. Tying it fixes the value.

---

## Chapter 4

**4.1**

```
   0  1D   LOAD 13
   1  2E   ADD 14
   2  2F   ADD 15
   3  3C   STORE 12
   4  00   HALT
```

**4.2** `0: 1E LOAD 14` / `1: 2E ADD 14` / `2: 40 JMP 0` loops for ever but re-loads `A` each time,
so it never accumulates; and with `JMP 1` it accumulates but never stops. The machine needs a
**conditional** branch to be useful.

**4.3** **None.** Both operands are already in registers, so no bus cycle is needed — only the
instruction fetch, which the queue has usually already done.

**4.4** One, to read the word at `[SI]`. The address is even, so it is a single cycle.

**4.5** 2²⁴ = 16,777,216 bytes. One bus cycle transfers 4 bytes.

**4.6** The processor cannot distinguish instructions from data, so it decodes the ASCII bytes as
opcodes. The failure is often not immediate because many ASCII values happen to be harmless
one-byte instructions (`0x48` is `DEC AX`, `0x40` is `INC AX`), so execution wanders for a while
before hitting something fatal.

**4.7** 4 × 200 ns = 800 ns; 1,250,000 machine cycles per second.

**4.8** Advantage: one memory, so programs can be loaded and manipulated as data — which makes
loaders, compilers and operating systems possible. Disadvantage: instructions and data compete for
one bus, which is the von Neumann bottleneck.

**4.9** A taken jump flushes the prefetch queue, so the next instruction must be fetched from cold —
a full bus cycle before decoding can start. A not-taken jump leaves the queue intact.

**4.10** `PC` is 6 after the fetch, so `d = 9 − 6 = 3`.

---

## Chapter 5

**5.1** 16× larger; four extra address lines.

**5.2** `DAA` (decimal adjust after addition — 8080 had it for business arithmetic); `XLAT` (table
lookup, an 8080 idiom); `LAHF`/`SAHF` (so 8080 code that pushed and popped the flag byte could be
translated); also `IN`/`OUT` with a separate I/O space.

**5.3** `BX` inherits the role of the 8080's `HL`, which was that machine's memory pointer. The
mechanical 8080-to-8086 translation mapped `HL` to `BX`, so `BX` had to be the register that works
in brackets.

**5.4** Twenty-bit registers would have cost transistors Intel did not have in 1978, and would have
broken the mechanical 8080 translation on which the whole software-compatibility strategy depended.

**5.5** Shared: instruction set, registers, flags, addressing modes, segmentation, 1 MiB address
space, interrupt structure. Different: external data bus width (16 vs 8) and queue depth (6 vs 4).

**5.6** `mov cl, 4` / `shl ax, cl`.

**5.7** (a) 8259A. (b) 8282/74LS373. (c) 8253/8254. (d) 8255A.

**5.8** It can enforce a segment limit, so a runaway pointer faults instead of corrupting memory.
It can enforce privilege levels, so application code cannot execute kernel instructions or touch
kernel data.

---

## Chapter 6

**6.1** EU: `AX`, the flags, the ALU, `SP`. BIU: `CS`, `IP`, the address adder, the instruction
queue.

**6.2** The 8086 fetches a **word** at a time over its 16-bit bus. Refilling with only one byte free
would waste half of every fetch cycle.

**6.3** The **BIU**. Six 2-byte instructions is 12 bytes consumed in about 6 × 3 = 18 clocks of
execution, while the BIU needs 6 bus cycles × 4 = 24 clocks to supply them. The EU outruns the bus.

**6.4** The taken case flushes the six-byte queue and must refetch from the target — a full bus cycle
plus decode before the next instruction can start. The not-taken case leaves the queue intact and
simply advances `IP`.

**6.5** The `nop` at `patch` was fetched into the queue before the `mov` wrote over it, and the queue
is not invalidated by writes, so the **old** byte executes. Fixes: (a) put at least six bytes of
other instructions between the write and `patch`; (b) force a queue flush with a jump —
`jmp short $+2` immediately after the write.

**6.6** The **operand access** wins. Prefetching is opportunistic and only uses bus cycles nobody
else wants; an EU stalled on an operand is stalled *now*, whereas a late prefetch merely risks a
stall later.

**6.7** At least one full bus cycle — 4 clocks — before the first byte arrives, plus decode time.

**6.8** Fetch-bound. 2.5 bytes at one byte per 4-clock cycle is 10 clocks of bus time, against 10
clocks of execution — and the operand accesses have to fit in the same budget. The bus is saturated.

**6.9** So that address arithmetic and data arithmetic can happen **simultaneously**. A shared ALU
would serialise them and the BIU could not prefetch while the EU computed.

**6.10** If the breakpoint address is within six bytes ahead of the current `IP`, the original byte
is already in the prefetch queue and executes instead of the `0xCC`.

---

## Chapter 7

**7.1** `AH = 0x7F`, `AL = 0x2C`. After `mov ah, 0x10`, `AX = 0x102C`.

**7.2** (a) legal. (b) **illegal** — two base registers; use `[bx+si]` or compute the sum first.
(c) **illegal** — no segment-to-segment `MOV`; use `mov ax, es` / `mov ds, ax`. (d) legal.
(e) **illegal** — no memory-to-memory; use a register. (f) **illegal** — `SP` cannot appear in
brackets; use `BP`. (g) legal. (h) **illegal on an 8086** — immediate count > 1 is 80186; use
`mov cl, 3` / `shl bx, cl`. (i) **illegal** — the immediate `IN` form reaches only ports 0–255; use
`mov dx, 0x3F8` / `in al, dx`. (j) legal.

**7.3** `mov ax, ds` / `mov es, ax`. Without a general register: `push ds` / `pop es`.

**7.4** `[bp+4]` reads **`SS`**; `[bx+4]` reads `DS`. Any address containing `BP` defaults to the
stack segment, because `BP` was designed for stack frames.

**7.5** `AX = 0x0010 × 0x0300 = 0x3000`; `DX = 0x0000`. Whatever was in `DX` before is destroyed —
`MUL` always writes `DX:AX` for a 16-bit multiply.

**7.6** `mov dx, 0x0378` / `in al, dx`. `in al, 0x378` fails because the immediate form encodes the
port as a single byte and cannot express a value above 255.

**7.7** `PUSH`, `POP`, `PUSHF`, `POPF`, `CALL`, `RET`, `RETF`, `INT`, `INTO`, `IRET`, and any
hardware interrupt.

**7.8** The 8086 inhibits interrupts for the duration of the instruction following a `MOV` into
`SS`, so the pair cannot be interrupted with a new `SS` and an old `SP`. Inserting a `nop` moves the
`mov sp` outside the protected window, and an interrupt arriving there would push onto a garbage
stack.

**7.9** `call next` / `next: pop ax`. `CALL` pushes the address of the instruction after it, which is
`next`; popping it retrieves that address.

**7.10** `[BX]` `DS` · `[BP]` **`SS`** · `[SI]` `DS` · `[DI]` `DS` · `[BP+SI]` **`SS`** ·
`[BX+DI]` `DS` · `[0x1234]` `DS` · `STOSB` destination **`ES`** · `LODSB` source `DS` · instruction
fetch `CS`.

**7.11** `add ax, 5` uses the accumulator-immediate form (`05`) at 3 bytes; `add bx, 5` uses the
sign-extended byte form (`83 /0`) also at 3 bytes. `add ax, 0x1234` is still the 3-byte accumulator
form, but `add bx, 0x1234` needs the general form with a full 16-bit immediate — 4 bytes.

**7.12** Reads `DX:AX` (the dividend) and `BX` (the divisor); writes `AX` (quotient) and `DX`
(remainder). If the quotient exceeds 16 bits the 8086 generates `INT 0`, not a flag.

---

## Chapter 8

**8.1**

| | Result | `CF` | `ZF` | `SF` | `OF` | `AF` | `PF` |
|---|--------|------|------|------|------|------|------|
| (a) `0x3A + 0x7C` | `0xB6` | 0 | 0 | 1 | **1** | 1 | 0 |
| (b) `0xF0 + 0x10` | `0x00` | **1** | **1** | 0 | 0 | 0 | 1 |
| (c) `0x50 − 0x60` | `0xF0` | **1** | 0 | 1 | 0 | 0 | 1 |
| (d) `NEG 0x80` | `0x80` | **1** | 0 | 1 | **1** | 0 | 1 |
| (e) `INC 0x0F` | `0x10` | *unchanged* | 0 | 0 | 0 | **1** | 0 |

**8.2** So that pointer arithmetic can sit inside a multi-precision carry chain without destroying
the carry:

```asm
        add  ax, [si]
        inc  si
        inc  si
        adc  dx, [si]       ; the carry from the ADD survives the INCs
```

**8.3** `AX − BX` = `0x7FFF`, so `CF = 0`, `ZF = 0`, `SF = 0`, `OF = 1`.
`JB` (CF=1): **not taken**. `JL` (SF≠OF): 0 ≠ 1, **taken**. `JS` (SF=1): not taken.
`JG` (ZF=0 and SF=OF): SF≠OF, **not taken**. Unsigned 32,768 > 1 so `JB` is right; signed
−32,768 < 1 so `JL` is right.

**8.4** `or ax, ax`, `test ax, ax`, `and ax, ax`.

**8.5** `CF = 0` and `OF = 0`, unconditionally. Inside a multi-precision loop this destroys the carry
being propagated between words, so the high halves come out wrong.

**8.6** The low byte of `0x1234` is `0x34` = `0011 0100`, which has three 1 bits — odd — so
**`PF = 0`**.

**8.7**

```asm
        pushf
        pop  ax
        or   ax, 0x0100
        push ax
        popf
```

The 8086 tests `TF` at the *end* of each instruction, and `POPF` itself completes before the test —
so the trap occurs after the *next* instruction.

**8.8** Entry: `cld`. Exit: nothing extra, because `IRET` restores the whole flag word including
`DF`. In a procedure entered by `CALL` rather than an interrupt, bracket it with `pushf` / `popf`.

**8.9**

```asm
        mov  ah, 0x3D
        int  0x21
        jc   error
```

**8.10** First instruction `pushf`; last instruction before `ret`, `popf`.

**8.11** `AX = 200 × 3 = 600 = 0x0258`. `AH = 0x02` is non-zero, so `CF = OF = 1`, meaning the
product did not fit in `AL` alone.

**8.12** Both are opcode `0x74`, testing `ZF = 1`. `JE` reads better after `CMP AX, BX` ("jump if
equal"); `JZ` reads better after `DEC CX` ("jump if the count reached zero").

---

## Chapter 9

**9.1** (a) `0x00500`. (b) `0x10100`. (c) `0xA0000 + 0xFFFF = 0xAFFFF`. (d) `0xFFFF0 + 0x0F =
0xFFFFF`. (e) `0x7C00`.

**9.2** `2500:0000`, `2000:5000`, `24FF:0010` — and 4093 others.

**9.3** `0x3ABC0` to `0x4ABBF`.

**9.4** `0x12340 + 0x5678 = 0x179B8`.

**9.5** `SS:BP+4` = `0x20000 + 0x0104 = 0x20104`. With the override, `DS:BP+4` =
`0x30000 + 0x0104 = 0x30104`.

**9.6** **2049.** Solve `seg × 16 + off = 0x08000` with `0 ≤ off ≤ 0xFFFF`:

```
   off = 0x08000 − seg × 16
   off ≥ 0       requires  seg ≤ 0x0800
   off ≤ 0xFFFF  requires  seg × 16 ≥ 0x08000 − 0xFFFF, which is negative — always true
```

So `seg` runs from `0x0000` (offset `0x8000`) to `0x0800` (offset `0x0000`): **2049 pairs**.

The familiar "4096 ways" figure applies only once the address is high enough that the segment can
start a full 64 KiB below it — that is, from physical `0x0FFF0` upwards. For `0x10000` the range is
`0x0001` to `0x1000`, which is exactly 4096. Below `0x0FFF0` the count is limited by the segment not
being able to go negative.

**9.7** `DS:0080` = `0x10000 + 0x80 = 0x10080`. `ES:0000` = `0x10080`. **Yes**, the same byte.

**9.8** `EA = 0xFFFE + 4 = 0x10002`, truncated to 16 bits = `0x0002`. Physical
`0x40000 + 0x0002 = 0x40002`. The offset arithmetic is 16-bit and wraps within the segment; it does
not carry into the segment register.

**9.9**

```asm
        mov  ax, 0xB800
        mov  es, ax
        mov  di, 0
        mov  word [es:di], 0x0741
```

**9.10** A `.COM` program contains only offsets — no segment values at all — so wherever DOS loads
it, every address is still correct relative to the segment registers DOS sets. An `.EXE` contains
literal segment values in instructions such as `mov ax, DATASEG`, and those must be adjusted by the
load address.

**9.11** `2345:6789` → physical `0x23450 + 0x6789 = 0x29BD9`. Normalised: segment `0x29BD`, offset
`0x0009` → `29BD:0009`.

**9.12** Source physical = `0x20000 + 0x0000 = 0x20000`. Destination = `0x1FF00 + 0x0100 =
0x20000`. They are the **same address** — the copy overwrites its own source from the first byte.

---

## Chapter 10

**10.1** `0x3A7F1` is odd, so the **odd bank**, on data lines `D15`–`D8`.

**10.2** `0x0400` is even and it is a word access: `A0 = 0`, `BHE# = 0`, **one** bus cycle.

**10.3** `0x0403` is odd.

| Cycle | Address | `A0` | `BHE#` | Delivers |
|-------|---------|------|--------|----------|
| 1 | `0x0403` | 1 | 0 | the **low** byte of `AX`, on `D15`–`D8` |
| 2 | `0x0404` | 0 | 1 | the **high** byte of `AX`, on `D7`–`D0` |

**10.4** `0x1001` is odd: `A0 = 1`, `BHE# = 0`, the odd bank, data lines `D15`–`D8`.

**10.5** Because `A0` selects *which bank* rather than which location within a bank. Both banks are
addressed by `A19`–`A1`; `A0` and `BHE#` decide which of them drives its half of the data bus.

**10.6** The chip responds only at even addresses (if wired to the even bank). Byte reads at even
addresses work; byte reads at odd addresses return floating-bus garbage; word reads return the
correct low byte and garbage in the high byte. The memory appears to be half its size and full of
holes.

**10.7** 64 KiB. Decoding `A19`–`A16` = `1111` places them at `0xF0000`–`0xFFFFF`.

**10.8** Every `PUSH` and `POP` becomes a misaligned word access — two bus cycles instead of one, so
4 extra clocks each. `SP` became odd because something adjusted it by an odd amount, such as
`dec sp` or `sub sp, 9`.

**10.9** Vector `0x13` is at `4 × 0x13 = 0x4C`; the `CS` word is at `0x4C + 2 = **0x4E**`.

**10.10** Hardware: no display adapter fitted, or the adapter decoded to a different address.
Software: the adapter is in a graphics mode, so `0xB8000` is not the text buffer — or `ES` was never
loaded and the write went somewhere else entirely.

**10.11** Starting at `0x0101` (odd): every word is misaligned, so 500 words × 2 = **1000 bus
cycles**. Starting at `0x0100` (even): 500 words × 1 = **500 bus cycles**.

**10.12** Both banks share the same nineteen address lines `A19`–`A1`, so one address selects the
same *row* in each. A byte needs only one bank, so any address works in one cycle. A word at an even
address needs row *n* of both banks — one address, one cycle. A word at an odd address needs row *n*
of the odd bank and row *n*+1 of the even bank — two different addresses, so two cycles.

---

## Chapter 11

**11.1** 20 + 16 + 2 + 2 + 1 = 41 pins minimum, before any control signals; the package has 40.
Intel multiplexed the address and data buses onto the same sixteen pins and the upper address lines
with status outputs.

**11.2** During T1, address bits `A15`–`A0`. During T3, data bits `D15`–`D0`.

**11.3** `S4 S3` = `11` → **`DS`**.

**11.4** A **byte** transfer at an **odd** address, on data lines `D15`–`D8`.

**11.5** (a) high, minimum mode: 24 `INTA#`, 25 `ALE`, 26 `DEN#`, 27 `DT/R#`, 28 `M/IO#`, 29 `WR#`,
30 `HLDA`, 31 `HOLD`. (b) low, maximum mode: 24 `QS1`, 25 `QS0`, 26 `S0#`, 27 `S1#`, 28 `S2#`,
29 `LOCK#`, 30 `RQ/GT1#`, 31 `RQ/GT0#`.

**11.6** So the address latches cannot accidentally reopen while another master owns the bus. `ALE`
must be deterministic at all times; a floating `ALE` could let the latches go transparent and destroy
the address the DMA controller is driving.

**11.7** A stray `WAIT` instruction hangs for ever, because `TEST#` never goes low. The symptom is a
program that freezes at an apparently arbitrary point, reproducibly.

**11.8** `READY` has a setup-time requirement relative to the clock. A decoder output changes
whenever the address changes, which can be inside the sampling window; the internal flip-flop then
goes **metastable** and settles unpredictably, so the processor either hangs or continues with
corrupt state.

**11.9** Maskability: `INTR` is blocked by `CLI`, `NMI` is not. Triggering: `INTR` is level,
sampled at the end of each instruction; `NMI` is rising-edge. Vector: `INTR`'s type number is
supplied by the device on `AD7`–`AD0`; `NMI` is always type 2. Use: `INTR` for normal devices,
`NMI` for catastrophic events.

**11.10** From the interrupting device, on `AD7`–`AD0`, during the **second** of **two** `INTA#`
cycles.

**11.11** 2 MHz. The 8086 uses dynamic internal storage whose charge leaks away; below that rate it
loses state. Consequently you cannot single-step it by stopping the clock.

**11.12** `100` is an **instruction fetch**; the 8288 asserts `MRDC#`. `101` is a **data read from
memory**; the 8288 also asserts `MRDC#`. The memory does not care, but the distinction lets the 8087
— and any bus analyser — tell code from data.

**11.13** (1) `VCC`/both `GND` pins connected? (2) Is `CLK` present, 33% duty, MOS levels? (3) Is
`RESET` pulsing and then releasing? (4) Is `MN/MX#` tied? (5) Is `READY` high — a floating `READY`
read as low stalls in T3 for ever. Then check `ALE` for activity.

---

## Chapter 12

**12.1** 24 MHz crystal; `PCLK` = 24 ÷ 6 = **4 MHz**.

**12.2** The 8086 requires `tCHCL` ≥ 69 ns high and `tCLCH` ≥ 118 ns low at 5 MHz — a 33% duty
cycle. A 50% wave gives 100 ns each way, which violates the low-time minimum.

**12.3** High 69 ns (one third of 200), low 131 ns.

**12.4** 2 MHz, because the internal storage is dynamic. It rules out single-stepping by halting the
clock — you must use the trap flag in software instead.

**12.5** τ = 47 kΩ × 10 µF = 0.47 s; the Schmitt threshold is crossed at roughly 0.4τ ≈ 190 ms. That
is far longer than the required 50 µs, so yes.

**12.6** `CS = 0xFFFF`, `IP = 0x0000`, `DS = SS = ES = 0x0000`, `FLAGS = 0x0000`. `AX` is
**undefined**.

**12.7** `0xFFFF × 16 + 0 = 0xFFFF0`. Sixteen bytes. Always a far jump into the real ROM.

**12.8** `EA` = far `JMP`; the operand is offset `0xE05B` then segment `0xF000`. Target physical
= `0xF0000 + 0xE05B` = `0xFE05B`.

**12.9** About 460 ns is available before glue delays; subtracting ~90 ns leaves ~370 ns, so a 450 ns
EPROM **does not fit** — insert one wait state, giving ~570 ns and a 120 ns margin. Aim for at least
20% margin (Appendix F §10).

**12.10** Because a decoder output is asynchronous to the clock and can change inside the `READY`
setup window, producing metastability. The 8284A clocks it into a flip-flop first.

**12.11** A floating `RDY1` will eventually read as low, so the 8284A holds `READY` low and the 8086
stalls in T3 for ever. The board looks completely dead, but the clock and reset are both fine — a
confusing symptom.

**12.12** The 8255, 8253, 8259A and 8251A all have reset inputs and come up in an undefined state
otherwise. Initialisation code that writes a control word to a chip which is mid-sequence produces
unpredictable configuration.

**12.13** 14.31818 MHz ÷ 3. That crystal is four times the NTSC colour subcarrier frequency, so it
was mass-produced and cheap — and the same crystal also drove the CGA video timing.

---

## Chapter 13

**13.1** Four T-states. 800 ns at 5 MHz; 500 ns at 8 MHz.

**13.2** In **T1**. It captures the address into the external latches, so the address remains stable
for the whole bus cycle after the CPU reuses those pins for data.

**13.3** Read: **nobody** — the 8086 has floated them and memory has not yet responded. Write: the
**8086**, which drives the data immediately with no turnaround.

**13.4** So the transceivers' direction is already correct before any data moves. Setting it in T2
would leave a window in which the transceiver points the wrong way.

**13.5** Because during T1 the address is on the same pins. If the transceivers were enabled then,
they would fight the address latches.

**13.6** On the **rising** edge, at the start of T4. That gives the longest possible setup time and a
guaranteed 88 ns of hold (`TWHDX`) afterwards.

**13.7** (a) 3 × 200 − 110 − 30 = **460 ns**. (b) 660 ns. (c) 1060 ns.

**13.8** 460 − (30 + 25 + 12 + ~5) = 460 − 72 = **388 ns**. A 350 ns EPROM fits; a 450 ns one does
not.

**13.9** At 8 MHz, 3 × 125 − 60 − 20 = 295 ns, minus 72 ns of glue = 223 ns. A 250 ns EPROM needs
**one** wait state (giving 348 ns).

**13.10** A wait state `Tw` is inserted between T3 and T4, and `READY` is sampled again. This repeats
indefinitely — the processor will wait for ever.

**13.11** `Tw` is inserted *within* a bus cycle because `READY` is low; the cycle is in progress and
all its signals are held. `Ti` is an *idle* clock between bus cycles, when the BIU has nothing to do.

**13.12** `tCLAV` = clock low to address valid — a **promise** (output). `tDVCL` = data valid before
clock low — a **demand** (input). `tRLRH` = `RD#` low to `RD#` high, the pulse width — a promise.
`tWHDX` = `WR#` high to data invalid, the data hold — a promise.

**13.13** **Two** bus cycles. The type number appears on `AD7`–`AD0` during the **second**.

**13.14** The EPROM is too slow for the shortened bus cycle at 8 MHz, while the RAM still fits.
Cheapest fix: a one-wait-state generator triggered by the ROM's chip select, leaving RAM at full
speed.

---

## Chapter 14

**14.1** `A19`–`A0` is twenty signals plus `BHE#` — twenty-one — and each 74LS373 latches eight. The
third one carries `A19`, `A18`, `A17`, `A16` and **`BHE#`**.

**14.2** `BHE#` is valid only during T1; from T2 the pin carries `S7`. Memory would lose its bank
selection part-way through every cycle. The symptom: word writes intermittently corrupt one of the
two bytes.

**14.3** `DT/R# = 1`, `DEN# = 0`. Data flows from the CPU side to the memory side.

**14.4** So the transceivers are off during T1, when the address is on the same pins. Tying `OE#`
low would make the transceiver drive against the address latch.

**14.5** 32 KiB at `0xF8000`–`0xFFFFF` means `A19`–`A15` are all 1 and `A14`–`A0` vary. A 74LS30
8-input NAND with inputs `A19 A18 A17 A16 A15 M/IO# +5V +5V` produces the active-low select.

**14.6** `0xA4000` → `A19 A18 A17` = `101` → **`Y5#`**, covering `0xA0000`–`0xBFFFF` (128 KiB).

**14.7** System `A13`–`A1` connect to the chip's `A12`–`A0`. Internal location 0 of the even-bank
chip holds physical address **`0x00000`**.

**14.8** The chip's internal addresses advance twice as fast as they should relative to the bank, so
the memory appears half its size and each location is reachable at two addresses — with every other
byte of a word coming from the wrong place. Word accesses are corrupt.

**14.9** Because 8-bit peripherals connect to `D7`–`D0`, the low half of the bus, which is selected
only when `A0 = 0`. The 8255's `A1` and `A0` connect to the system's `A2` and `A1`.

**14.10** (1) `READY` is stuck low, so the processor stalls in T3 after its first fetch. (2) There is
no valid code at `0xFFFF0` — the ROM is missing, empty, or decoded to the wrong range.

**14.11** `MN/MX#` → +5 V; `NMI` → GND; `INTR` → GND; `TEST#` → GND; `HOLD` → GND; `READY` → high
via the 8284A (`RDY1` high, `AEN1#` low). Plus both `GND` pins connected.

**14.12** Four MOS chips at ~10 pF each is 40 pF; 15 cm of trace is roughly 8 pF; add the latch and
transceiver inputs and you are at 60–70 pF — inside the 100 pF limit but with little margin. Buffer
it, because any expansion pushes you over.

---

## Chapter 15

**15.1** `MN/MX#` (pin 33): +5 V for minimum mode, GND for maximum.

**15.2** Fetch `100`; I/O write `010`; memory write `110`; interrupt acknowledge `000`; idle `111`.

**15.3** Reads: `MRDC#` (memory) and `IORC#` (I/O). Writes: `MWTC#` and `AMWC#` (memory), `IOWC#`
and `AIOWC#` (I/O). The advanced forms assert one clock earlier, for peripherals that need a longer
write pulse and latch on the trailing edge.

**15.4** The 8086's minimum-mode `DEN#` is active **low**; the 8288's `DEN` is active **high**. A
74LS245's `OE#` is active low, so the 8288's output must be inverted.

**15.5** `MWTC#` asserts at the start of T3; `AMWC#` at the start of T2 — one clock, 200 ns at
5 MHz, earlier. Use the advanced form only for a device that needs the longer pulse *and* latches on
the trailing edge.

**15.6** Maximum mode distinguishes an **instruction fetch** (`100`) from a data read (`101`). The
**8087** needs that, because it must track the instruction stream.

**15.7** Three one-clock low pulses on a single wire: request from the peripheral, grant from the
8086 (after which it has floated the bus), and release from the peripheral. `RQ/GT0#` has the higher
priority.

**15.8** The instruction queue was **emptied** — a jump, call, return or interrupt occurred, and the
8087 must discard its shadow copy too.

**15.9**

```
   MEMR# = RD# OR (NOT M/IO#)
   MEMW# = WR# OR (NOT M/IO#)
   IOR#  = RD# OR M/IO#
   IOW#  = WR# OR M/IO#
```

**15.10** So no other bus master can take the bus between the two `INTA#` cycles. If one did, the
vector fetch would be broken and the wrong type number read.

**15.11** **Minimum mode.** No coprocessor and no second processor, so the 8288's benefits are
unused; and minimum mode saves a chip and is easier to probe with a scope.

**15.12** Because `MRDC#` and `MWTC#` assert only for memory cycles, and `IORC#`/`IOWC#` only for
I/O. The command lines themselves carry the distinction, so it need not be decoded from `M/IO#`.

---

## Chapter 16

**16.1** Four — two pairs. Each chip receives system `A12`–`A1` on its `A12`–`A0`... more precisely,
each 2764 is 8 K × 8 and takes system `A13`–`A1` on its `A12`–`A0`; the pair covers 16 KiB, so two
pairs cover 32 KiB, with `A14` selecting between them.

**16.2** 8 KiB at `0x10000`–`0x11FFF`. `A19`–`A13` are fixed at `0000100`; `A12`–`A1` go to the
chips; `A0`/`BHE#` do the bank split. A 74LS138 with `A19 A18 A17` on `CBA` and the rest gated, or a
74LS30 NAND with the inverted lines, produces the select.

**16.3** `0x1A000` → `A19 A18` = `00` (enables), `A17 A16 A15` = `011` → **`Y3#`**. Each output
covers 32 KiB: `Y0#` = `0x00000`–`0x07FFF`, `Y1#` = `0x08000`–`0x0FFFF`, `Y3#` =
`0x18000`–`0x1FFFF`.

**16.4** `CE#` powers the chip up and has a long access time; `OE#` only turns on the output drivers
and is fast. The address decode is available early (from T1) and `RD#` late (T2), so putting the slow
signal on the early input and the fast one on the late input maximises the time available.

**16.5** With `OE#` permanently low the EPROM drives the data bus whenever it is selected — including
during a *write* to another device in the same decoded range, and during the address phase. The
result is bus contention.

**16.6** 3 × 125 − 60 − 20 = 295 ns; minus 90 ns of glue = **205 ns**. Devices at 200 ns or faster
work: 2732A-20, 2764-20, 27256-20, and all the SRAMs.

**16.7** At 10 MHz, 3 × 100 − 50 − 15 = 235 ns; minus 90 = 145 ns. A 300 ns device needs
`(300 − 145) / 100` = 1.55, so **two** wait states (giving 345 ns).

**16.8** `mov al, [0x2000]` (even) works. `mov al, [0x2001]` (odd) reads the floating bus — garbage.
`mov ax, [0x2000]` gets the correct low byte and garbage in `AH`.

**16.9** On a read, both chips drive the data bus; where they disagree the bus is shorted, the data
is indeterminate and the chips heat. On a write, both accept the data — harmless electrically, but
now two copies exist and later reads are ambiguous.

**16.10** Because the scrambling is *consistent*: writing N to address N and reading address N
reaches the same physical cell either way, so the value matches. A test that detects it writes a
pattern dependent on the address bits — for example, write `0x5555` to every address whose bit *k*
is set and `0xAAAA` elsewhere, for each *k*, and check.

**16.11** 4164 is 64 K × 1 = 8 KiB per chip, so 128 KiB of 16-bit memory needs 128 × 1024 × 8 ÷
65536 = **16 chips**. In 6264 SRAM (8 KiB × 8): 128 ÷ 8 = **16 chips** too — but the DRAM would cost
a fraction as much per bit at the time.

**16.12** DRAM stores each bit as charge on a capacitor, which leaks; every row must be accessed at
least every 2–4 ms. On the IBM PC, 8253 counter 1 triggered DMA channel 0 to perform a dummy read
every ~15 µs.

**16.13** A 2764 (8 KiB) uses `A13`–`A1` on the chips, leaving `A19`–`A14` — six lines — for
decoding, so 64 distinct device positions. A 27256 (32 KiB) uses `A15`–`A1`, leaving only
`A19`–`A16` — four lines, 16 positions. Bigger chips mean coarser granularity.

---

## Chapter 17

**17.1** `in al, 0x70` — a single 2-byte instruction, because `0x70` < 256. For `0x3CE`:
`mov dx, 0x03CE` / `in al, dx`, because the immediate form encodes the port in one byte.

**17.2** `OUT DX, AL` is 1 byte and 8 clocks; `OUT 0x20, AL` is 2 bytes and 10 clocks. Inside a loop,
load `DX` once outside and use the register form.

**17.3** With `A1 A0` from system `A2 A1`: Port A `0x40`, Port B `0x42`, Port C `0x44`, Control
`0x46`.

**17.4** An 8-bit peripheral connects to `D7`–`D0`, the low half of the bus, which is enabled only
when `A0 = 0` — i.e. at even addresses. On an **8088** there is a single 8-bit bus and no banks, so
peripherals sit at consecutive addresses.

**17.5** `IOR# = RD# OR M/IO#` and `IOW# = WR# OR M/IO#`.

**17.6** `OUT 0x1A, AL`: `A7 A6` = `00` (enables satisfied), `A5 A4 A3` = `011` → **`Y3#`**, covering
ports `0x18`–`0x1F`.

**17.7** `0x11A`, `0x21A`, `0xFF1A` — any address whose low eight bits are `0x1A`.

**17.8** Memory space for bulk transfer, because `REP MOVSW` and all the addressing modes work — a
framebuffer is the example. I/O space for occasional control registers, because it costs none of the
1 MiB address space and keeps the memory map clean.

**17.9** `jmp $+2` jumps to the instruction immediately after it — functionally a no-op. It takes
about 15 clocks rather than 3 because a taken jump **flushes the prefetch queue** and the next
instruction must be refetched. Two of them give roughly 6 µs at 5 MHz, which is the recovery time an
8259A needs between consecutive writes.

**17.10** `AL` goes to port `0x42` and `AH` to port `0x43`. With an 8-bit device wired only to
`D7`–`D0`, the low byte reaches port `0x42` and the high byte is written to nothing.

**17.11**

```asm
        in   al, 0x60
        push ax
        mov  cl, 4
        shr  al, cl
        call hexdigit           ; prints AL as one hex character
        pop  ax
        and  al, 0x0F
        call hexdigit
```

**17.12** The IBM PC's peripherals were 8080-generation parts with slow bus timing, and IBM wanted
the extra margin without adding external wait-state logic.

---

## Chapter 18

**18.1** Pins 8–15 and 39: `AD8`–`AD15` become **`A8`–`A15`**, address only. Pin 34: `BHE#/S7`
becomes **`SS0#`**. Pin 28: `M/IO#` becomes **`IO/M#`** — inverted.

**18.2** `IO/M#` is low for memory where `M/IO#` was high, so the `G1` connection must be inverted —
either use `G2A#` instead, or add an inverter.

**18.3** **Two** latches (for `A7`–`A0` and `A19`–`A16`+`SS0#`) and **one** transceiver, because
`A15`–`A8` are not multiplexed and there is only one data byte. That is two chips fewer than an
8086 system — exactly the saving IBM wanted.

**18.4** Because the 8-bit bus can deliver only one byte per 4-clock cycle. A deeper queue would
rarely be full, so the extra storage would buy nothing.

**18.5** 8086: one fetch cycle (4 clocks) brings 2 bytes, so 2.5 bytes need 1.25 cycles = 5 clocks of
bus time per 10 clocks of execution — half the bus is free for operands. 8088: 2.5 bytes need 2.5
cycles = 10 clocks of bus time per 10 clocks of execution — the bus is saturated by fetch alone.

**18.6** `mov ax, [0x0200]` (even): 8086 **one** cycle, 8088 **two**. `mov ax, [0x0201]` (odd):
8086 **two**, 8088 **two**.

**18.7** Because every word access on an 8088 takes two bus cycles regardless of alignment — there
are no banks and no aligned fast path.

**18.8** 8086: 1000 words × 1 cycle = **1000** cycles. 8088: 1000 words × 2 = **2000** cycles.

**18.9** (1) The 8088 inserts an automatic wait state into every I/O cycle, so `OUT`-based timing
loops run longer on a PC. (2) The 8088 is fetch-bound, so instruction sequences take longer than
their published clock counts — the loop is genuinely slower on the PC than the tables suggest.

**18.10** Not by any instruction, since the instruction sets are identical. It can be done by
**timing**: execute a long sequence of word memory accesses and compare the elapsed time against a
timer. The classic method exploits the queue depth — a self-modifying sequence behaves differently
with a 4-byte queue than a 6-byte one.

**18.11** To match the **8085**, whose `IO/M#` pin had that polarity. The 8088 was aimed at customers
upgrading 8085 designs.

**18.12** Differ: the instruction queue depth (6 vs 4) and the number of memory banks (2 vs 1).
Identical: the number of interrupt vectors, the addressing modes, the reset address, and the clock
duty-cycle requirement.

---

## Chapter 19

**19.1** (a) legal. (b) **illegal** — `AX` cannot appear in brackets. (c) legal. (d) **illegal** —
two index registers. (e) **illegal** — two base registers. (f) legal. (g) **illegal** — `SP` cannot
appear in brackets. (h) **illegal** — scaled index is 80386. (i) legal. (j) legal. (k) **illegal** —
`CX` cannot appear in brackets. (l) legal.

**19.2**

| | EA | Segment | Physical |
|---|-----|---------|----------|
| (a) `[bx]` | `0x0050` | `DS` | `0x10050` |
| (b) `[bp]` | `0x0060` | **`SS`** | `0x40060` |
| (c) `[bx+si]` | `0x0055` | `DS` | `0x10055` |
| (d) `[bp+di+2]` | `0x006A` | **`SS`** | `0x4006A` |
| (e) `[0x0200]` | `0x0200` | `DS` | `0x10200` |
| (f) `[es:bx+4]` | `0x0054` | **`ES`** | `0x20054` |
| (g) `[bx-0x10]` | `0x0040` | `DS` | `0x10040` |

**19.3** `[0x1234]` 6 · `[SI]` 5 · `[BX+2]` 9 · `[BX+SI]` 7 · `[BX+DI]` 8 · `[BP+SI+4]` 12 ·
`[ES:BX+SI]` 7 + 2 = 9.

**19.4** `[bx+di+100]` = 9 + 12 = **21** clocks. `[bx+si+100]` = 9 + 11 = **20**. `BX+SI` and
`BP+DI` are the "natural" pairings in the microcode and cost one clock less than `BX+DI` and
`BP+SI`.

**19.5** `mod = 00, r/m = 110` is taken by the direct-address form `[disp16]`, so `[BP]` with no
displacement has no encoding. The assembler emits `mod = 01` with a zero displacement byte —
`8B 46 00` instead of a 2-byte form.

**19.6** `mov ax, [es:bx+si+6]`.

**19.7** `EA = 0xFFF0 + 0x20 = 0x10010`, truncated to `0x0010`. Physical `0x30000 + 0x0010 =
**0x30010**` — it wrapped within the segment.

**19.8**

```asm
NAME    equ  0
AGE     equ  16
SALARY  equ  18

        mov  cx, [bx+AGE]
        mov  ax, [bx+SALARY]
```

**19.9**

```asm
        xor  ah, ah
        mov  al, [r]
        mov  cx, 5
        mul  cx                 ; AX = r × 5 (columns)
        xor  bh, bh
        mov  bl, [c]
        add  ax, bx             ; + c
        shl  ax, 1              ; × 2 for words
        mov  si, ax
        mov  ax, [M + si]
```

**19.10** (a) direct `[count]`. (b) register indirect `[bx]`, or `LODSB`. (c) based `[bx+FIELD]`.
(d) string instructions — `REP MOVSW`.

**19.11** In NASM, `mov ax, count` loads the **address** of `count` and `mov ax, [count]` loads its
**contents**. MASM writes `MOV AX, OFFSET count` and `MOV AX, count` respectively.

**19.12** Swap the index register so the pairing is `BX+SI`, which costs 7 clocks rather than 8:

```asm
        mov  bx, table
        mov  si, 0
.next:  mov  al, [bx+si]
        inc  si
        loop .next
```

---

## Chapter 20

**20.1** `ADD BX, CX`. Opcode `01` (`d = 0`, `w = 1`). ModR/M: `mod = 11`, `reg = 001` (`CX`, the
source), `r/m = 011` (`BX`, the destination) = `11 001 011` = `0xCB`. → **`01 CB`**

**20.2** `MOV DL, [SI]`. Opcode `8A` (`d = 1`, `w = 0`). ModR/M: `mod = 00`, `reg = 010` (`DL`),
`r/m = 100` (`[SI]`) = `00 010 100` = `0x14`. → **`8A 14`**

**20.3** `SUB AX, [BX+DI]`. Opcode `2B` (`d = 1`, `w = 1`). ModR/M: `mod = 00`, `reg = 000` (`AX`),
`r/m = 001` (`[BX+DI]`) = `0x01`. → **`2B 01`**

**20.4** `MOV [BP+6], AX`. Opcode `89` (`d = 0`, `w = 1`). ModR/M: `mod = 01` (disp8),
`reg = 000` (`AX`), `r/m = 110` (`[BP]`) = `01 000 110` = `0x46`. Displacement `06`.
→ **`89 46 06`**

**20.5** `CMP byte [DI+0x100], 0x20`. Opcode `80` (group 1, byte, imm8). ModR/M: `mod = 10`
(disp16), `reg = 111` (`CMP`), `r/m = 101` (`[DI]`) = `10 111 101` = `0xBD`. Displacement
`00 01`. Immediate `20`. → **`80 BD 00 01 20`**

**20.6** `MOV word [0x0300], 0x1234`. Opcode `C7`. ModR/M: `mod = 00`, `reg = 000`, `r/m = 110`
(direct) = `0x06`. Displacement `00 03`. Immediate `34 12`. → **`C7 06 00 03 34 12`**

**20.7** `AND AL, [ES:BX+SI+4]`. Prefix `26`. Opcode `22` (`d = 1`, `w = 0`). ModR/M: `mod = 01`,
`reg = 000` (`AL`), `r/m = 000` (`[BX+SI]`) = `0x40`. Displacement `04`. → **`26 22 40 04`**

**20.8** `MUL word [BP+2]`. Opcode `F7` (group 3, word). ModR/M: `mod = 01`, `reg = 100` (`MUL`),
`r/m = 110` (`[BP]`) = `01 100 110` = `0x66`. Displacement `02`. → **`F7 66 02`**

**20.9** `SHR DX, CL`. Opcode `D3` (group 2, word, by `CL`). ModR/M: `mod = 11`, `reg = 101`
(`SHR`), `r/m = 010` (`DX`) = `11 101 010` = `0xEA`. → **`D3 EA`**

**20.10** Short form `0x46` (`0x40 + 110` for `SI`). Group form `FF C6` (`mod = 11`, `reg = 000`
= `INC`, `r/m = 110` = `SI`). NASM emits the **short** form.

**20.11** `8B 5E FE`: `8B` = `MOV r16, r/m16`. `5E` = `01 011 110`: `mod = 01`, `reg = 011` (`BX`),
`r/m = 110` (`[BP]`). `FE` = −2. → **`MOV BX, [BP-2]`**

**20.12** `F6 27`: `F6` = group 3, byte. `27` = `00 100 111`: `mod = 00`, `reg = 100` (`MUL`),
`r/m = 111` (`[BX]`). → **`MUL byte [BX]`**

**20.13** `81 C6 00 01`: `81` = group 1, word, imm16. `C6` = `11 000 110`: `mod = 11`, `reg = 000`
(`ADD`), `r/m = 110` (`SI`). Immediate `0x0100`. → **`ADD SI, 0x0100`**

**20.14** `2E 8A 07`: `2E` = `CS:` prefix. `8A` = `MOV r8, r/m8`. `07` = `00 000 111`: `reg = 000`
(`AL`), `r/m = 111` (`[BX]`). → **`MOV AL, [CS:BX]`**

**20.15** `C6 46 FF 00`: `C6` = `MOV r/m8, imm8`. `46` = `01 000 110`: `mod = 01`, `reg = 000`
(extension), `r/m = 110` (`[BP]`). Displacement `FF` = −1. Immediate `00`.
→ **`MOV byte [BP-1], 0`**

**20.16** `D3 E8`: `D3` = group 2, word, by `CL`. `E8` = `11 101 000`: `reg = 101` (`SHR`),
`r/m = 000` (`AX`). → **`SHR AX, CL`**

**20.17** Because `mod = 00, r/m = 110` encodes the direct address `[disp16]`, not `[BP]`. The
assembler must use `mod = 01` with a zero displacement byte, adding one byte.

**20.18** **Three bits** — the `reg` field of the ModR/M byte. `ADD` is `000` and `CMP` is `111`, so
`83 C3 05` becomes `83 FB 05`.

**20.19** Group 2 provides only two count forms: the literal 1 (`D0`/`D1`) and `CL` (`D2`/`D3`).
There is no opcode for an immediate count. The **80186** added `C0` and `C1` for it.

**20.20** The longest possible instruction is prefix + opcode + ModR/M + 2 displacement + 2
immediate = **six bytes**. A six-byte queue therefore always holds at least one complete
instruction.

---

## Chapter 21

**21.1** (a) legal. (b) **illegal** — no memory-to-memory; use a register or `MOVSB`. (c)
**illegal** — no immediate to segment register; `mov ax, 0x1000` / `mov ds, ax`. (d) **illegal** —
no segment to segment; `mov ax, ds` / `mov es, ax`. (e) legal. (f) legal. (g) **illegal** — `PUSH`
is always 16 bits; `push ax`. (h) **illegal in practice** — `POP CS` exists but corrupts execution.
(i) **illegal** — `LEA` needs a memory operand; `mov ax, 5`. (j) legal. (k) **illegal** — only
`AL`/`AX` can do I/O; `in al, dx` / `mov bl, al`. (l) **illegal** — ambiguous size;
`mov byte [bx], 5`.

**21.2** `mov ax, 0xA000` / `mov es, ax`. There is no immediate-to-segment-register `MOV` opcode.

**21.3** `lea bx, [si+4]` gives `BX = 0x0104` — the address. `mov bx, [si+4]` gives `BX = 0x9999` —
the contents of physical `0x20104`.

**21.4**

```asm
ptr:    dw   0x0000             ; offset first
        dw   0xB800             ; then segment

        les  di, [ptr]          ; ES = 0xB800, DI = 0x0000
```

**21.5** `SP = 0x0FFA`. `AX`'s value is at offset `0x0FFE`.

**21.6**

```asm
myproc: pushf
        push ax
        push bx
        push cx
        push dx
        ; ...
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        popf
        ret
```

**21.7** `XCHG AX, r16` has a dedicated one-byte opcode `0x90 + r`. Any other pair needs the general
form `87 /r`, which is opcode plus ModR/M = two bytes.

**21.8** `0x90` is `0x90 + 000`, i.e. `XCHG AX, AX` — exchanging a register with itself, which
changes nothing. Intel did not spend a separate opcode on a do-nothing instruction.

**21.9**

```asm
hextab: db  '0123456789ABCDEF'

        mov  bx, hextab
        and  al, 0x0F
        xlat
```

**21.10** `LAHF` captures only the low flag byte: `SF`, `ZF`, `AF`, `PF`, `CF`. It does **not**
capture `OF` — so after a signed addition you cannot test for overflow through `AH`. Use
`PUSHF`/`POP AX` when `OF` matters.

**21.11** With `XCHG`: `mov al, [si]` (13) + `xchg al, [di]` (17+5 = 22) + `mov [si], al` (14) = 49
clocks. The four-`MOV` version: 13 + 13 + 14 + 14 = 54. `XCHG` is slightly faster here, and one
instruction shorter.

**21.12** (a) `mov ax, ds` / `mov es, ax` — 4 bytes, 4 clocks. (b) `push ds` / `pop es` — 2 bytes,
18 clocks. The register version is far faster; the stack version is for when no register is free.

---

## Chapter 22

**22.1**

| | Result | `CF` | `OF` | `ZF` | `SF` |
|---|--------|------|------|------|------|
| (a) `0x64 + 0x64` | `0xC8` | 0 | **1** | 0 | 1 |
| (b) `0xC8 + 0xC8` | `0x90` | **1** | **1** | 0 | 1 |
| (c) `0x14 − 0x28` | `0xEC` | **1** | 0 | 0 | 1 |
| (d) `0x7FFF + 1` | `0x8000` | *unchanged* | **1** | 0 | 1 |
| (e) `NEG 0x80` | `0x80` | **1** | **1** | 0 | 1 |

**22.2** Because a memory destination needs a **read** bus cycle to fetch the current value and a
**write** cycle to store the result, where a register destination needs neither.

**22.3**

```asm
        mov  cx, 1000
        mov  bx, [total]
.next:  add  bx, 5              ; 4 clocks, no bus cycle
        loop .next
        mov  [total], bx
```

Original: 1000 × (17 + 6 + 17) = 40,000 clocks. New: 1000 × (4 + 17) = 21,000. **Saving ≈ 19,000
clocks.**

**22.4**

```asm
        mov  ax, [a]
        add  ax, [b]
        mov  [a], ax
        mov  ax, [a+2]
        adc  ax, [b+2]
        mov  [a+2], ax
```

**22.5** Because `ADD` sets `CF` and would destroy the carry being propagated between words. `INC`
leaves `CF` alone.

**22.6**

```asm
        add  ax, [si]           ; CF = carry out of the low word
        inc  si
        inc  si
        adc  dx, [si]           ; needs that CF
```

If `INC` cleared `CF`, the high-word addition would lose the carry.

**22.7** `AL = 0x00`, `ZF = 1`, `CF` **unchanged**. To detect the wrap, use `add al, 1` instead,
which does set `CF`.

**22.8** `0x80` (−128). `NEG` leaves `0x80` in the register and sets `OF = 1`, because +128 has no
signed byte representation.

**22.9** `AX − BX` = `0x8000 − 0x0001 = 0x7FFF`. `CF = 0`, `ZF = 0`, `SF = 0`, `OF = 1`.
`JB` (CF=1) not taken — correct, unsigned 32,768 > 1. `JA` (CF=0 and ZF=0) **taken** — correct.
`JL` (SF≠OF) **taken** — correct, signed −32,768 < 1. `JG` (ZF=0 and SF=OF) not taken — correct.
`JE` not taken.

**22.10**

```asm
        or   ax, ax
        jns  .done
        neg  ax
.done:
```

With `AX = 0x8000`, `NEG` leaves `0x8000` and sets `OF` — the absolute value of −32,768 cannot be
represented.

**22.11**

```asm
        mov  ax, [a+2]
        cmp  ax, [b+2]
        ja   a_bigger
        jb   .done
        mov  ax, [a]
        cmp  ax, [b]
        ja   a_bigger
.done:
```

**22.12** Four `ROL`s bring the top nibble to the bottom *and* leave `BX` rotated a full 16 bits
after four iterations — so it ends up holding its original value. With `SHR`, the register would be
zero after the first few digits and every subsequent digit would print as `0`.

**22.13**

```asm
; 64-bit add: [a] += [b], four words each, least significant first
        mov  si, a
        mov  di, b
        mov  cx, 4
        clc
.next:  mov  ax, [di]
        adc  [si], ax
        inc  si
        inc  si
        inc  di
        inc  di
        loop .next
```

---

## Chapter 23

**23.1** `AX = 250 = 0x00FA`. `AH = 0`, so `CF = OF = 0`, meaning the product fits in `AL`.

**23.2** `0x0200 × 0x0300 = 0x60000`. `DX = 0x0006`, `AX = 0x0000`.

**23.3** Because a 16-bit multiply always produces a 32-bit product in `DX:AX`. Protect `DX` with
`push dx` / `pop dx` around the `MUL`.

**23.4** 1000 ÷ 2 = 500, which does not fit in `AL` (maximum 255), so the 8086 generates **`INT 0`**
— a divide-overflow exception, not a flag. Fixes: (a) use a 16-bit divide with `xor dx, dx` /
`mov bx, 2` / `div bx`; (b) check first with `cmp ah, bl` / `jae .overflow`.

**23.5**

```asm
        xor  dx, dx
        mov  bx, 7
        div  bx                 ; AX = quotient, DX = remainder
```

**23.6**

```asm
        cwd                     ; SIGN-extend, not zero-extend
        mov  bx, 7
        idiv bx
```

`CWD` replaces `xor dx, dx` because a negative dividend must be sign-extended into `DX`, or
`DX:AX` represents a large positive number instead.

**23.7** `CBW` gives `AX = 0xFFF0` (sign-extended). `mov ah, 0` gives `AX = 0x00F0`. Use `CBW`
before `IDIV` (signed); use `mov ah, 0` before `DIV` (unsigned).

**23.8** `IDIV`: −7 ÷ 2 = **−3** remainder **−1** (truncation toward zero; the remainder takes the
dividend's sign). `SAR AX, 1` on −7 gives **−4**, because `SAR` rounds toward negative infinity.
They differ by one for negative odd numbers.

**23.9**

```asm
        mov  bx, ax
        shl  ax, 1              ; 2n
        shl  ax, 1              ; 4n
        shl  ax, 1              ; 8n
        shl  bx, 1              ; 2n
        add  ax, bx             ; 10n
```

About 2+2+2+2+2+3 = **13 clocks**, against `MUL`'s 118–133. Nearly ten times faster.

**23.10** `n × 7 = n × 8 − n`:

```asm
        mov  bx, ax
        shl  ax, 1
        shl  ax, 1
        shl  ax, 1              ; 8n
        sub  ax, bx             ; 7n
```

**23.11** `DX` retains the previous remainder, so `DX:AX` becomes an enormous number and the
quotient usually exceeds 16 bits — producing a **divide-overflow crash**, not a wrong digit.

**23.12** Because dividing by 10 produces the digits **least significant first**, and they must be
printed most significant first. Pushing and popping reverses the order for free.

**23.13**

```asm
; DX:AX / BX, handling a quotient that would exceed 16 bits
        cmp  dx, bx
        jb   .safe              ; DX < BX guarantees the quotient fits
        ; otherwise divide the high half first
        push ax
        mov  ax, dx
        xor  dx, dx
        div  bx                 ; AX = high quotient, DX = remainder
        mov  cx, ax             ; keep it
        pop  ax
        div  bx                 ; DX:AX now safe; AX = low quotient
        ; result is CX:AX
        ret
.safe:
        div  bx
        xor  cx, cx
        ret
```

**23.14**

```asm
        div  bl
        or   ah, ah             ; the remainder
        jz   .exact
```

`DIV` leaves every flag undefined, so the flags must be regenerated by an explicit test.

---

## Chapter 24

**24.1** Because a nibble carries at 16 but a decimal digit carries at 10, and 16 − 10 = 6. Adding 6
converts a binary nibble carry into a decimal one.

**24.2** `0x37 + 0x48`: low nibbles 7 + 8 = 15 = `0xF`, no carry out of bit 3, so `AF = 0`. High:
3 + 4 = 7. `AL = 0x7F`, `CF = 0`, `AF = 0`. `DAA`: `(AL AND 0x0F) = 0xF > 9` → `AL = 0x7F + 6 =
0x85`, `AF = 1`. `0x85 ≤ 0x9F` and `CF = 0` → no high adjustment. **`AL = 0x85` = BCD 85**, and
37 + 48 = 85 ✔

**24.3** `0x99 + 0x99`: low 9 + 9 = 18 → nibble 2, carry → `AF = 1`. High 9 + 9 + 1 = 19 → nibble 3,
carry → `CF = 1`. `AL = 0x32`. `DAA`: `AF = 1` → `AL = 0x38`; `CF = 1` → `AL = 0x38 + 0x60 = 0x98`,
`CF = 1`. **`AL = 0x98` with `CF = 1` = BCD 198**, and 99 + 99 = 198 ✔

**24.4** `0x19 + 0x08` gives `AL = 0x21` with `AF = 1`. The low nibble is 1, which is **not** greater
than 9, so the first test alone would do nothing and leave `0x21` — wrong by 6. The `AF` test
catches it and produces `0x27` ✔

**24.5** `0x40 − 0x15`: low 0 − 5 borrows → `0xB`, `AF = 1`. High 4 − 1 − 1 = 2. `AL = 0x2B`,
`CF = 0`. `DAS`: `0xB > 9` → `AL = 0x2B − 6 = 0x25`. **BCD 25**, and 40 − 15 = 25 ✔

**24.6** `AH = 0`, `AL = '6' + '7'` = `0x36 + 0x37 = 0x6D`. Low nibbles 6 + 7 = 13 = `0xD`, no carry
out of bit 3 → `AF = 0`, but `(AL AND 0x0F) = 0xD > 9` → `AL = 0x6D + 6 = 0x73`, `AH = 1`,
`AL &= 0x0F` → `3`. **`AX = 0x0103`, `CF = 1`** — the digits 1 and 3, and 6 + 7 = 13 ✔
Then `add ax, 0x3030` gives `0x3133` = `'1','3'`.

**24.7** Because the operands are ASCII digits whose high nibbles are `0x3`, and adding them leaves
`0x6` in the high nibble — garbage. Clearing it leaves only the digit value. Without the clear, the
next `AAA` in a chain would see a corrupt value.

**24.8** Because `AAA` **increments** `AH` rather than setting it. Starting with junk in `AH` makes
the tens digit wrong.

**24.9** `AL = 0x4F` = 79. `AAM`: `AH = 79 / 10 = 7`, `AL = 79 mod 10 = 9` → **`AX = 0x0709`**.
`AAM 16`: `AH = 79 / 16 = 4`, `AL = 79 mod 16 = 15` → **`AX = 0x040F`**.

**24.10** `AAD` computes `AL = AH × 10 + AL; AH = 0`, packing two unpacked digits into a binary
value. It is the only one of the six that runs **before** its arithmetic operation — the others
correct a result afterwards.

**24.11** `AX = 0x0904` → `AL = 9 × 10 + 4 = 94 = 0x5E`, `AH = 0`. Dividing by 5: quotient
**18**, remainder **4**.

**24.12** `add si, 1` sets `CF`, destroying the decimal carry that `DAA` produced and that the next
`ADC` needs. `INC` leaves `CF` alone.

**24.13**

```asm
; 8-digit packed BCD subtract: [a] -= [b], four bytes each, LSB first
        mov  si, a
        mov  di, b
        mov  cx, 4
        clc
.next:  mov  al, [si]
        sbb  al, [di]
        das
        mov  [si], al
        inc  si
        inc  di
        loop .next
```

**24.14** `aam 16` — `AH` receives the high nibble and `AL` the low one. It costs **83 clocks**
against about 10 for `mov ah, al` / `shr ah, 4` / `and al, 0x0F` — so it is shorter but much
slower.

---

## Chapter 25

**25.1** `AL = 0xA7` = `1010 0111`. `and al, 0x0F` → `0x07`. `or al, 0x0F` → `0xAF`.
`xor al, 0x0F` → `0xA8`. `not al` → `0x58`.

**25.2** `and al, 0xDB` (clearing bits 2 and 5: `~(0x04 | 0x20)` = `~0x24` = `0xDB`).

**25.3** `or bl, 0x83`.

**25.4** `xor dh, 0x10`.

**25.5** `test al, 0x40` / `jnz found`.

**25.6** `x XOR x = 0` for every bit. Two bytes, 3 clocks — against `mov ax, 0` at three bytes and
4 clocks.

**25.7** `x OR x = x`, so `AX` is unchanged, but the instruction still sets `ZF` (and `SF` and `PF`)
from the result.

**25.8** `AND` forces `CF = 0`, destroying the carry the `ADD` produced, so the following `ADC` adds
no carry and the high half comes out one too small.

**25.9**

```asm
        cmp  al, 'a'
        jb   .done
        cmp  al, 'z'
        ja   .done
        and  al, 0xDF
.done:
```

**25.10**

```asm
        mov  ax, [old]
        xor  ax, [new]          ; 1 bits mark the positions that differ
```

**25.11**

```asm
        mov  ah, al
        and  ah, 0x0C
        cmp  ah, 0x0C
        je   both
```

A single `TEST al, 0x0C` sets `ZF = 0` if **at least one** of the bits is set; it cannot distinguish
one from two.

**25.12** Subtracting 1 turns the lowest set bit into 0 and every bit below it into 1; `AND`ing with
the original therefore clears exactly that bit and preserves everything above it.

```
   x     = 0010 1100
   x-1   = 0010 1011
   AND   = 0010 1000      the lowest set bit (bit 2) is gone
```

**25.13** `add ax, 255` / `and ax, 0xFF00`.

**25.14** `not al` / `or al, al`.

---

## Chapter 26

**26.1** `AL = 1100 1010` = `0xCA`.

| | `AL` | `CF` |
|---|------|------|
| `shl al,1` | `1001 0100` = `0x94` | **1** |
| `shr al,1` | `0110 0101` = `0x65` | 0 |
| `sar al,1` | `1110 0101` = `0xE5` | 0 |
| `rol al,1` | `1001 0101` = `0x95` | **1** |
| `ror al,1` | `0110 0101` = `0x65` | 0 |

**26.2** `AL = 0x35` = `0011 0101`, `CF = 1`. `rcl al, 1`: the old `CF` enters at the bottom, bit 7
leaves to `CF` → `AL = 0110 1011 = 0x6B`, `CF = 0`. `rcr al, 1` from the original: the old `CF`
enters at the top, bit 0 leaves → `AL = 1001 1010 = 0x9A`, `CF = 1`.

**26.3** `shl ax, 1` three times = 6 clocks, against `MUL`'s 118–133 plus loading the multiplier —
about twenty times faster.

**26.4** `40 = 32 + 8`:

```asm
        mov  bx, ax
        mov  cl, 5
        shl  ax, cl             ; 32n
        mov  cl, 3
        shl  bx, cl             ; 8n
        add  ax, bx             ; 40n
```

**26.5** `SHR` brings a **zero** in at the top, destroying the sign: −8 (`1111 1000`) becomes
`0111 1100` = +124. The correct instruction is `SAR`, which replicates the sign bit.

**26.6** `AX = −9 = 0xFFF7`. `sar ax, 1` → `0xFFFB` = **−5**. `IDIV` by 2 gives **−4** remainder −1.
`SAR` rounds toward negative infinity; `IDIV` truncates toward zero.

**26.7** `shl ax, 1` / `rcl dx, 1`.

**26.8** `shr word [q+6], 1` / `rcr word [q+4], 1` / `rcr word [q+2], 1` / `rcr word [q], 1`.

**26.9** **No.** Rotates do not affect `ZF`, `SF`, `PF` or `AF` — only `CF` and (for a count of 1)
`OF`.

**26.10**

```asm
        mov  cx, 16
        xor  dl, dl
.next:  rol  ax, 1
        jnc  .skip
        inc  dl
.skip:  loop .next
        ; AX is unchanged after 16 rotations; DL = the count
```

**26.11** With `CL`: `mov cl, 4` / `shr al, cl` = 4 + 8 + 16 = **28 clocks**, 4 bytes. With repeated
single shifts: four `shr al, 1` = **8 clocks**, 8 bytes. The single-shift version is much faster; the
`CL` version is shorter.

**26.12** On an 8086 the count is **not masked**, so 33 shift steps execute and `AX` becomes 0,
taking 8 + 4×33 = 140 clocks. On an 80286 and later the count is masked to 5 bits, so 33 becomes 1
and `AX` is merely doubled.

**26.13** `xchg al, ah` is **one byte and 3 clocks**; `mov cl,8` / `rol ax,cl` is 4 bytes and
4 + 8 + 32 = 44 clocks.

---

## Chapter 27

**27.1** The displacement is measured from the instruction *after* the jump, which is at `0x0202`.
Target = `0x0202 + 0x20` = **`0x0222`**.

**27.2** `EB FE`. The next instruction is 2 bytes further on, so to land back on the jump itself the
displacement must be −2 = `0xFE`.

**27.3** **No** — conditional jumps reach only ±127 bytes. Invert and jump over:

```asm
        jnz  .skip
        jmp  faraway
.skip:
```

**27.4** `AX − BX` = `0x8000 − 0x7FFF = 0x0001`. `CF = 0`, `ZF = 0`, `SF = 0`, and `OF = 1` —
the operands have **different** signs (`0x8000` is −32,768, `0x7FFF` is +32,767) and the result's
sign differs from the first operand's, which is exactly the subtraction-overflow condition.
`JA` (CF=0, ZF=0) **taken** — unsigned 32,768 > 32,767 ✔
`JAE` **taken**. `JB` not taken.
`JG` (ZF=0, SF=OF): 0 ≠ 1, **not taken**. `JGE` (SF=OF) not taken. `JL` (SF≠OF) **taken** — signed
−32,768 < 32,767 ✔ `JE` not taken.
`JA` and `JG` disagree because the top bit means "large" unsigned and "negative" signed.

**27.5** (a) `JA`. (b) `JG`.

**27.6** `CX = 0`, so `LOOP` decrements to `0xFFFF` and the body runs **65,536 times** instead of
none.

**27.7** Add `jcxz .done` before the loop and a `.done:` label after it.

**27.8** So that it can be used inside a multi-precision carry chain without destroying `CF`:

```asm
        clc
.next:  adc  ax, [si]
        inc  si
        inc  si
        loop .next              ; CF survives
```

**27.9**

```asm
        mov  cx, 3              ; three rows
.row:   push cx
        mov  cx, 5              ; five columns
.col:   putc '*'
        loop .col
        newline
        pop  cx
        loop .row
```

**27.10**

```asm
        cmp  ax, 10
        jl   .else
        cmp  bx, 0
        je   .else
        mov  cx, 1
        jmp  .endif
.else:  mov  cx, 0
.endif:
```

**27.11**

```asm
        xor  ax, ax             ; sum
        xor  si, si
        mov  cx, 20
.next:  add  ax, [array+si]
        inc  si
        inc  si
        loop .next
        mov  [sum], ax
```

**27.12**

```asm
.next:  cmp  al, 0
        je   .rare
.back:  ; ... body ...
        loop .next
        jmp  .done
.rare:  call rare_case
        jmp  .back
.done:
```

Original: the `jne` is taken 99% of the time at 16 clocks. New: the `je` is not taken 99% of the time
at 4 clocks. **Saving 12 clocks per iteration**, or about 118,800 clocks over 10,000 iterations.

**27.13**

```asm
        cmp  al, 5
        ja   .invalid           ; UNSIGNED — catches negatives too
        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 for word entries
        jmp  [table + bx]
table:  dw   opt0, opt1, opt2, opt3, opt4, opt5
```

**27.14** Because `sub al, '1'` on an input of `'0'` gives `0xFF`. As a **signed** byte that is −1,
which `jg 3` would let through, and the dispatch would read `[table - 2]`. As an **unsigned** byte it
is 255, which `ja 3` correctly rejects. The breaking input is **`'0'`** (or anything below `'1'`).

**27.15** Test `ZF`. `LOOPNE` exits either because `CX` reached 0 (no match — `ZF = 0`) or because a
match set `ZF = 1`. So `jne .not_found` after the loop distinguishes them.

---

## Chapter 28

**28.1** Near pushes `IP` — **2 bytes**. Far pushes `CS` then `IP` — **4 bytes**.

**28.2** `SP = 0x07FE`. The return address (the offset of the instruction after the `CALL`) is stored
at `[SS:07FE]`.

**28.3** Because `RET` pops one word and `RETF` pops two. A near `CALL` matched with `RETF` pops the
return address into `IP` and then pops whatever is below it into `CS` — so execution jumps to a
wild segment, and `SP` is 2 too high for ever after.

**28.4**

```asm
swapregs:
        xchg ax, bx
        ret
```

**28.5** Parameters pushed left to right; near call.

```
   [BP+8]  parameter 1
   [BP+6]  parameter 2
   [BP+4]  parameter 3
   [BP+2]  return address
   [BP+0]  caller's BP
   [BP-2]  local 1
   [BP-4]  local 2
```

**28.6** Every parameter offset increases by 2, because the far `CALL` pushed `CS` as well:
`[BP+10]`, `[BP+8]`, `[BP+6]`; the return address occupies `[BP+2]` and `[BP+4]`. Locals are
unchanged.

**28.7** (1) `SP` moves on every `PUSH` and `POP`, so an offset from it means different things at
different points; `BP` stays fixed for the whole call. (2) `[SP]` is not a legal addressing mode on
an 8086 at all.

**28.8** Three word parameters (6 ÷ 2), and the **callee-cleans-up** (Pascal) convention.

**28.9** The pops are in the **wrong order** — `AX` and `BX` come back swapped. It should be
`pop bx` / `pop ax`.

**28.10** On the early-exit path the `push cx` is never matched by a `pop cx`, so `RET` pops the
saved `CX` as the return address and jumps to a wild location.

**28.11**

```asm
; sum_array(ptr, count) -> AX; caller cleans up
sum_array:
        push bp
        mov  bp, sp
        push si
        push cx
        mov  si, [bp+6]         ; ptr
        mov  cx, [bp+4]         ; count
        xor  ax, ax
        jcxz .done
.next:  add  ax, [si]
        inc  si
        inc  si
        loop .next
.done:  pop  cx
        pop  si
        pop  bp
        ret

; call:
        push word [ptr]
        push word [count]
        call sum_array
        add  sp, 4
```

**28.12**

```asm
fib:    push bp
        mov  bp, sp
        mov  ax, [bp+4]
        cmp  ax, 1
        jbe  .base
        dec  ax
        push ax
        call fib                ; fib(n-1)
        add  sp, 2
        push ax
        mov  ax, [bp+4]
        sub  ax, 2
        push ax
        call fib                ; fib(n-2)
        add  sp, 2
        pop  bx
        add  ax, bx
        jmp  .done
.base:  mov  ax, [bp+4]
.done:  pop  bp
        ret
```

For *n* = 10 the recursion is 10 deep at most, so about 10 × 6 = 60 bytes of stack — but it makes
**177 calls**, because each level calls twice. The iterative version makes 10 additions.

**28.13** `SP` drifts **4 bytes per call**, because both sides remove the parameters. After about
16,000 calls `SP` has wrapped past `0xFFFE` and the stack is writing into the program's data. In
practice the crash comes much sooner, as soon as `SP` climbs into the code or data area.

**28.14** `JAE` is unsigned: `0xFE0C` (−500) is 65,036, which is above 1234, so `max16` keeps it and
the program prints **`-500`**. `print_sdec` still interprets it as signed, so the answer looks
absurd — which is exactly what a signed/unsigned mismatch produces.

---

## Chapter 29

**29.1** `DS:SI` (source), `ES:DI` (destination), `DF` (direction), and `CX` if `REP` is used.
`DS` can be overridden; **`ES` cannot**.

**29.2** `SI = 0x102`, `DI = 0x202` — word operation, `DF = 0`, so both increase by 2.

**29.3**

```asm
        cld
        mov  si, src
        mov  di, dst
        mov  cx, 200
        rep  movsb
```

**29.4**

```asm
        cld
        mov  si, src
        mov  di, dst
        mov  cx, 200
        shr  cx, 1              ; CX = whole words; CF = the odd bit
        rep  movsw
        adc  cx, 0              ; CX is 0 here, so this makes it 0 or 1
        rep  movsb              ; moves the odd byte, or nothing
```

`SHR` puts the discarded low bit into `CF`; `ADC CX, 0` turns it into a count of 0 or 1.

**29.5** Because `REP` tests `CX` **before** each iteration, so `CX = 0` does nothing. `LOOP`
decrements first and then tests, so `CX = 0` becomes `0xFFFF` and the loop runs 65,536 times.

**29.6**

```asm
        cld
        mov  ax, 0xB800
        mov  es, ax
        xor  di, di
        mov  ax, 0x0720
        mov  cx, 2000
        rep  stosw
```

**29.7** `DI` points **one byte past** the match, and `CX` holds the number of elements remaining
after it.

```asm
        repne scasb
        jne  .not_found
        dec  di
```

**29.8** It means a mismatch was found. `SI` and `DI` each point **one byte past** the mismatching
pair, and the flags from the last `CMPSB` say which was larger.

**29.9** In a `.COM` program DOS sets `ES = DS`, so `ES:DI` happens to be right. In an `.EXE`, `ES`
points at the PSP. Fix: `mov ax, ds` / `mov es, ax` at the start.

**29.10** Destination `0x340` is **above** source `0x300` and they overlap, so copy **backwards**:

```asm
        std
        mov  si, 0x300 + 99
        mov  di, 0x340 + 99
        mov  cx, 100
        rep  movsb
        cld
```

**29.11** (a) `REP MOVSB`: 9 + 1000 × 17 = **17,009**. (b) `REP MOVSW`: 9 + 500 × 17 = **8,509**.
(c) hand loop: 1000 × (13 + 14 + 2 + 2 + 17) = **48,000**. `REP MOVSW` is about **5.6×** faster than
the hand loop and **2×** faster than `REP MOVSB`.

**29.12** A 5-character string is 6 bytes including the terminator. `CX` starts at `0xFFFF`; after 6
bytes it is `0xFFF9`. `NOT CX` = `0x0006`. `DEC CX` = **5** ✔

**29.13** Because it has no idea what `DF` the interrupted code had set. If `DF = 1`, the handler's
string instructions run backwards and corrupt memory below the buffer instead of filling it.

**29.14**

```asm
        cld
        mov  si, buf
        mov  di, buf            ; the SAME buffer
.next:  lodsb
        or   al, al
        jz   .done
        cmp  al, 'a'
        jb   .store
        cmp  al, 'z'
        ja   .store
        and  al, 0xDF
.store: stosb
        jmp  .next
.done:
```

It works because `LODSB` reads byte *n* and advances `SI` **before** `STOSB` writes byte *n* — `DI`
is always exactly one behind or equal to `SI`, never ahead, so nothing unread is overwritten.

---

## Chapter 30

**30.1** Directly settable: `CF` (`CLC`/`STC`/`CMC`), `DF` (`CLD`/`STD`), `IF` (`CLI`/`STI`).
Not settable: `ZF`, `SF`, `OF`, `PF`, `AF` — change them with `PUSHF` / modify / `POPF`, or by
arranging an operation that sets them.

**30.2** Because the first `ADC` adds whatever `CF` happened to be. Without `CLC` the answer is
wrong one time in two, unpredictably.

**30.3**

```asm
is_upper:
        cmp  al, 'A'
        jb   .no
        cmp  al, 'Z'
        ja   .no
        stc
        ret
.no:    clc
        ret
```

**30.4** If the caller had already disabled interrupts, the `sti` enables them behind its back,
breaking *its* critical section. Correct version:

```asm
myproc: pushf
        cli
        ; ...
        popf
        ret
```

**30.5** So that `sti` / `ret` cannot be interrupted between the two instructions, which would push
an interrupt frame onto a stack the procedure was about to finish with. The same mechanism protects
`mov ss, ax` / `mov sp, bx`.

**30.6** An enabled `INTR`, an `NMI`, or `RESET`. With `IF = 0` only `NMI` and `RESET` can wake it,
so `cli` / `hlt` halts the machine deliberately.

**30.7** `TEST#` floats and is read unpredictably; if it reads high the `WAIT` never completes and
the program freezes at that point, reproducibly.

**30.8** Because it **is** `XCHG AX, AX` — a real exchange the processor performs, not a dedicated
do-nothing opcode.

**30.9** Nothing observable. In minimum mode there is no `LOCK#` pin at all (pin 29 is `WR#`), and
with one bus master there is nothing to lock out. It costs one byte and no extra clocks.

**30.10** **No.** `LOCK` prevents another *bus master* from interleaving, but an interrupt on the
same processor occurs between instructions, after the locked one has completed. For that you need
`CLI`.

**30.11**

```asm
        pushf
        cli
        mov  ax, [counter]
        mov  dx, [counter+2]
        popf
```

**30.12** Some later, unrelated call — a library string copy, a DOS function, a BIOS routine — runs
its `MOVS` or `STOS` backwards and overwrites memory *below* its buffer instead of filling it. The
corruption appears far from the handler, in code that is entirely correct.

**30.13** One byte and two clocks. Without the override:

```asm
        push ds
        push es
        pop  ds
        mov  al, [bx+si]
        pop  ds
```

Five instructions, 5 bytes and about 40 clocks against the override's 1 byte and 2 clocks. **Not
worth it** — unless the same segment is used many times, in which case reload `DS` once outside the
loop.

---

## Chapter 31

**31.1** `INT 21h`: `0x21 × 4` = **`0x84`**. `INT 1Ch`: `0x1C × 4` = **`0x70`**.

**31.2** (1) push `FLAGS`; (2) `IF = 0`; (3) `TF = 0`; (4) push `CS`; (5) push `IP`; (6) `IP` ←
word at `4n`; (7) `CS` ← word at `4n+2`.

**31.3** **Six bytes**: the flag word, `CS` and `IP`.

**31.4** `IF = 0` so the handler is not immediately re-entered before it has established its state.
`TF = 0` so a single-step handler does not step itself into infinite recursion. **`IRET` restores
both**, because it pops the flag word pushed at step 1.

**31.5** Because a breakpoint must be settable on **any** instruction, including a one-byte one. A
two-byte breakpoint would overwrite the first byte of the following instruction. Every debugger
depends on this.

**31.6** `RETF` pops only `IP` and `CS`, leaving the flag word on the stack. `SP` ends 2 too high,
the caller's flags are not restored, and every subsequent stack operation is off by two.

**31.7** Maskability: `INTR` is blocked by `CLI`; `NMI` never. Triggering: `INTR` is level-sensitive,
`NMI` is rising-edge. Type number: `INTR`'s comes from the device on `AD7`–`AD0`; `NMI` is always
type 2. Bus cycles: `INTR` needs two `INTA#` cycles; `NMI` needs none.

**31.8** 2000 × 51 = **102,000 clocks** of `INT` overhead alone = 20.4 ms at 5 MHz — and the DOS
handler itself costs several times that again.

**31.9**

```asm
handler:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push cs
        pop  ds
        cld
        ; ... work ...
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        iret
```

**31.10**

```asm
        mov  ax, 0x3509
        int  0x21               ; ES:BX = the old handler
        mov  [old_off], bx
        mov  [old_seg], es

        push ds
        mov  dx, my_handler
        push cs
        pop  ds
        mov  ax, 0x2509
        int  0x21
        pop  ds
```

**31.11** The vector still points into the memory the program occupied, which DOS has freed and will
reuse. The next timer tick — within 55 ms — jumps into whatever is loaded there. The machine
usually survives until something else is loaded, then crashes apparently at random.

**31.12** An `INT` pushes flags, `CS`, `IP`. `PUSHF` supplies the flags; the far `CALL` supplies `CS`
and `IP`. The old handler's `IRET` then pops all three and returns to the instruction after the
`CALL` — exactly as if it had been entered by an interrupt.

**31.13** So that when you single-step an instruction that triggers a hardware interrupt, the
**hardware** handler runs first and the step is reported afterwards. If single step were highest, a
debugger would step into every interrupt and stepping would be unusable.

**31.14** At 8 MHz one clock is 125 ns. 162 (the `DIV`) + 11 (two `INTA#` cycles) + 51 (the `INT`
sequence) ≈ 224 clocks = **28 µs**.

**31.15** Because DOS is **not reentrant**. If the foreground is inside an `INT 21h` call when the
interrupt fires and the handler calls `INT 21h` too, DOS's internal state — its stack pointer, its
buffers — is corrupted.

---

## Chapter 32

**32.1** `mov ax, bx` 2 · `mov ax, [bx]` 8+5 = **13** · `mov ax, [bx+si]` 8+7 = **15** ·
`mov ax, [bx+di+8]` 8+12 = **20** · `mov ax, [es:bx+si+8]` 8+11+2 = **21**.

**32.2** `add [count], ax` = 16 + 6 = **22 clocks**. With a register accumulator, `add bx, ax` = 3.
Over 1000 iterations: 22,000 → 3,000, a saving of **19,000 clocks** (3.8 ms at 5 MHz).

**32.3** 4 extra clocks per access × 1,000,000 = **4,000,000 clocks** = 0.8 s at 5 MHz.

**32.4** `[bx+si]` costs 7; `[bx+di]` costs 8. **One clock faster**, because `BX+SI` and `BP+DI` are
the microcode's natural pairings.

**32.5** `MUL` by 16: at least 118 clocks plus loading the multiplier. Four `shl ax, 1`: **8
clocks**. Saving about **114 clocks per iteration**.

**32.6** Invert the condition so the common case falls through: 4 clocks instead of 16. Over 10,000
iterations at 95% taken: 9,500 × 12 = **114,000 clocks** saved.

**32.7** `REP MOVSB` 4000 bytes: 9 + 4000 × 17 = **68,009**. `REP MOVSW` 2000 words: 9 + 2000 × 17 =
**34,009**. On an **8088** both are 68,009, because every word access is two bus cycles.

**32.8** Per iteration, assuming the character is a lower-case letter (the worst case):
`mov al,[si]` 13 + `cmp al,'a'` 4 + `jb` 4 + `cmp al,'z'` 4 + `ja` 4 + `sub al,0x20` 4 +
`mov [si],al` 14 + `inc si` 2 + `loop` 17 = **66 clocks**.

**32.9**

```asm
.next:  lodsb                   ; 12
        cmp  al, 'a'            ; 4
        jb   .skip              ; 4
        cmp  al, 'z'            ; 4
        ja   .skip              ; 4
        sub  al, 0x20           ; 4
        mov  [si-1], al         ; 9+9 = 18
.skip:  loop .next              ; 17
```

About **67 clocks** — no better, because the `[si-1]` store costs more than the saved `inc`. The
real win is to use `STOSB` with `DI` trailing `SI` (Exercise 29.14), giving 12 + 4 + 4 + 4 + 4 + 4 +
11 + 17 = **60**.

**32.10** `CALL` 19 + `RET` 16 = 35 clocks per call × 1,000,000 = **35,000,000 clocks** = **7 seconds**
at 5 MHz. A 3-instruction body costs perhaps 10 clocks, so the overhead is 3.5× the work —
**inlining is very much worth it**.

**32.11** Because the 8088's 8-bit bus makes it **fetch-bound**: the published counts assume the
bytes are already in the queue, and on an 8088 they usually are not. The queue is empty most of the
time, so the real cost includes fetch time the tables ignore.

**32.12** Unrolled ×8: 8 × (12 + 3) + 17 = 137 clocks for 8 elements = **17.1 per element**, against
the ×4 version's 19.25. Diminishing returns — the loop overhead is already mostly amortised.

**32.13** `INT 21h` function 02h: 2000 × (51 + several hundred for the handler) ≈ **1,000,000+
clocks** = 200 ms. `REP STOSW` into `0xB8000`: 9 + 2000 × 10 = **20,009 clocks** = 4 ms. About
**fifty times faster**.

---

## Chapter 33

**33.1** (a) `41 42`. (b) `34 12` — little-endian. (c) `41 42`. (d) `42 41` — the word is stored low
byte first, so the characters come out reversed. (e) `78 56 34 12`. (f) `FF FF FF`.

**33.2**

```asm
buf     times 16 db 0
count   dw   1000
msg     db   'Ready$'
vowels  db   'aeiou'
```

**33.3** `msglen equ $ - msg` — 21.

**33.4** **No bytes.** It tells the assembler that byte 0 of the output will live at offset `0x100`
at run time, so every label is computed as `0x100 + its file offset`. Omit it and every address
operand is `0x100` too low, pointing into the PSP.

**33.5**

```asm
        mov  ax, buffer         ; MASM: MOV AX, OFFSET buffer
        mov  bx, [count]        ; MASM: MOV BX, count
        mov  byte [si], 0       ; MASM: MOV BYTE PTR [SI], 0
buf     times 50 db 0           ; MASM: buf DB 50 DUP(?)
```

**33.6** Because NASM does not track types, so with a memory destination and an immediate source
there is nothing to say whether one byte or two is meant. With a register operand the size is
unambiguous and no keyword is needed.

**33.7** `equ` is evaluated once, at the point of definition, and cannot be redefined or take
parameters. `%define` is a preprocessor substitution, re-evaluated at each use, and **can** take
parameters:

```asm
%define CELL(r,c) (((r)*80+(c))*2)
```

**33.8** It pads the output with zeros up to offset 510. It appears in a **boot sector**, which must
be exactly 512 bytes with `0xAA55` in the last two.

**33.9** Because the processor executes straight through whatever follows the last instruction. Data
placed between instructions would be decoded as code. Putting it after an unconditional exit is the
only safe arrangement in a flat binary.

**33.10** `jmp .past` before the table, `.past:` after it.

**33.11** It makes NASM **reject** any instruction the 8086 cannot execute. Without it, 80186 and
later instructions assemble happily and run under DOSBox, so you would learn the wrong architecture.

**33.12** Make them local, with a leading dot:

```asm
strlen:
.next:  ...
        jnz  .next
strcpy:
.next:  ...                     ; a different symbol: strcpy.next
```

**33.13** `1 << 5` = 32 = `0x20`. `~(1 << 3)` as a byte = `~0x08` = `0xF7`.
`or al, 1 << 5` and `and al, ~(1 << 3)`.

---

## Chapter 34

**34.1** `nop` is one byte, so the file becomes **29 bytes**, `msg` moves to **`0x010E`**, and the
operand becomes `BA 0E 01`.

**34.2** `B8 00 4C` — three bytes instead of four, so the file loses **one byte** (27 total) and
`msg` moves down to **`0x010C`**.

**34.3** The pattern `0xB0 + reg` is "move an immediate byte into an 8-bit register". `AH` is
register `100` = 4, so `0xB0 + 4 = 0xB4`. `AL` is register `000` = 0, so `0xB0`.

**34.4** The order is **little-endian** — low byte first. The value `0x010D` is `msg`'s address:
`0x100` (from `org`) plus its file offset of 13.

**34.5** `int 0x21` is **two** bytes (`CD 21`). `int 3` is **one** byte (`CC`), a dedicated opcode,
because a breakpoint must be settable over any instruction including a one-byte one.

**34.6**

| Pushed | `SP` after |
|--------|-----------|
| `FLAGS` | `0xFFFC` |
| `CS` = `0x0700` | `0xFFFA` |
| `IP` = `0x0107` | `0xFFF8` |

**34.7** It is a zero that DOS pushes so that a plain `RET` at the end of a `.COM` program pops 0
into `IP` and jumps to `CS:0000` — PSP offset 0, which holds `CD 20`, an `INT 20h`, terminating the
program. A CP/M inheritance.

**34.8** Without the `$`: the message prints, then DOS keeps printing whatever bytes follow until it
finds a `0x24` — garbage of unpredictable length. Without `org 0x100`: `msg` is computed as `0x000D`,
so `DS:DX` points inside the **PSP** and DOS prints whatever is there. The two differ because the
first walks *forward* from the right place and the second starts from the wrong place.

**34.9**

```asm
        cpu  8086
        org  0x100
start:  mov  ah, 0x09           ; B4 09
        mov  dx, msg1           ; BA xx xx
        int  0x21               ; CD 21
        mov  ah, 0x09           ; B4 09
        mov  dx, msg2           ; BA xx xx
        int  0x21               ; CD 21
        mov  ax, 0x4C00         ; B8 00 4C
        int  0x21               ; CD 21
msg1:   db   'One', 0x0D, 0x0A, '$'
msg2:   db   'Two', 0x0D, 0x0A, '$'
```

Code is 2+3+2+2+3+2+3+2 = 19 bytes, so `msg1` is at `0x113` and `msg2` at `0x11A`.

**34.10** `CX` must be the **file length in bytes** — `1C` for the 28-byte program. `DEBUG` writes
`CX` bytes starting from offset `0x100`.

**34.11** DOS function 09h returns the **terminating character** in `AL`, and the terminator is `$` =
`0x24`.

**34.12** `-t` steps *into* the DOS interrupt handler, where you would spend a long time
single-stepping kernel code. `-p` executes the whole `INT` and stops afterwards.

**34.13** 67 clocks is about 13 µs. The two DOS calls dominate completely — writing thirteen
characters to the screen takes DOS on the order of a millisecond. **Your code is well under 2% of
the run time.**

---

## Chapter 35

**35.1** **Zero** bytes of header — the file is a raw image. Maximum **65,280** bytes, because the
256-byte PSP occupies the start of the 64 KiB segment.

**35.2** `CS = DS = ES = SS` = the PSP segment. For an `.EXE`, **`DS` and `ES` point at the PSP**,
not at your data. Forgetting to load `DS` means every memory reference reads the PSP.

**35.3** `CD 20` — an `INT 20h` instruction. DOS pushes a zero word at load time, so `RET` pops 0
into `IP` and execution lands on that `INT 20h`, which terminates the program.

**35.4** `[0x80]` = **15** (the space plus `-v file.txt`). `[0x81]`, `[0x82]`, `[0x83]` = `' '`,
`'-'`, `'v'`.

**35.5**

```asm
        mov  cl, [0x80]
        xor  ch, ch
        jcxz .none
        mov  si, 0x81
.skip:  cmp  byte [si], ' '     ; step over leading spaces
        jne  .print
        inc  si
        loop .skip
.print: mov  dl, [si]
        mov  ah, 0x02
        int  0x21
        inc  si
        loop .print
.none:
```

**35.6** A `.COM` program contains only offsets, never a segment value, so it is correct wherever it
is loaded. An `.EXE` contains instructions such as `mov ax, DATASEG` whose operand is a literal
segment number that depends on the load address; the relocation table lists them so DOS can add the
load segment to each.

**35.7**

```asm
        mov  ax, [0x02]         ; segment past the end of our block
        mov  bx, cs
        sub  ax, bx             ; paragraphs owned
        mov  cl, 6
        shr  ax, cl             ; ÷ 64 -> KiB  (paragraphs × 16 ÷ 1024)
```

**35.8** `INT 21h`/`4Ch` (returns an exit code), `INT 20h`, and `RET`. **Only `4Ch` returns an exit
code**, which is what batch files test with `ERRORLEVEL`.

**35.9** DOS decides the format by extension, so it loads the file raw at offset `0x100` and jumps
there — landing in the middle of the `MZ` header, which is not code. It crashes immediately.

**35.10** The signature bytes `4D 5A` at the start of every `.EXE`. They are the initials of **Mark
Zbikowski**, the Microsoft developer who designed the format.

**35.11** Because an `.EXE` enters with `DS` pointing at the **PSP**, not at the data segment. A
`.COM` program has only one segment, and DOS has already set all four registers to it.

**35.12** **`.EXE`.** 40 + 30 = 70 KiB exceeds the 64 KiB a `.COM` program's single segment can hold.

**35.13** (1) One build command, no linker and no relocation table. (2) The output is a `.COM` file,
which is smaller and loads faster — and is exactly the bytes you wrote, so it can be hex-dumped and
accounted for.

---

## Chapter 36

**36.1** **`AH`** selects the function. DOS guarantees only the **segment registers and `SP`**;
every general-purpose register may be destroyed.

**36.2** `mov dl, bl` / `mov ah, 2` / `int 0x21`. Function 02h takes its character from `DL`
specifically — there is no form that prints `BL`.

**36.3** `mov dl, '$'` / `mov ah, 2` / `int 0x21`. Function 09h cannot, because `$` is its
terminator.

**36.4**

```asm
.next:  mov  ah, 0x08
        int  0x21
        cmp  al, 0x0D
        je   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:
```

**36.5**

```asm
        mov  dx, inbuf
        mov  ah, 0x0A
        int  0x21
        mov  cl, [inbuf+1]      ; the length DOS filled in
        xor  ch, ch

inbuf:  db   41                 ; maximum, including the CR
        db   0                  ; DOS writes the count here
        times 41 db 0
```

**36.6**

```asm
        mov  dx, filename
        mov  ax, 0x3D00
        int  0x21
        jc   .error
        mov  [handle], ax
        ; ...
.error: mov  dx, errmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C01
        int  0x21
filename: db 'DATA.TXT', 0
```

**36.7** **End of file.** `AX = 0` with no carry means the read succeeded and returned zero bytes,
which is how DOS reports EOF — it is not an error.

**36.8** Fewer bytes were written than asked, with no error flagged: **the disk is full**. Always
compare `AX` with `CX` after a write.

**36.9** A `$`-terminated string ends with `0x24` and is used by **display** function 09h. An ASCIIZ
string ends with `0x00` and is used by every **file and directory** function. Mixing them up is a
standard mistake.

**36.10** See Chapter 36 §9's `show_time`. The essential point is to `push` `CX` and `DX`
immediately after function 2Ch, because the very next `INT 21h` may destroy them.

**36.11** Because DOS allocates **all** remaining memory to a `.COM` program, so there is nothing
left for `48h` to hand out until `4Ah` has shrunk the program's own block.

**36.12**

```asm
        mov  dx, dta
        mov  ah, 0x1A
        int  0x21               ; set the DTA
        mov  dx, pattern
        xor  cx, cx
        mov  ah, 0x4E
        int  0x21
        jc   .done
.next:  mov  dx, dta + 0x1E     ; the ASCIIZ filename
        call print_asciiz
        mov  ah, 0x4F
        int  0x21
        jnc  .next
.done:
pattern: db '*.ASM', 0
dta:     times 43 db 0
```

**36.13** `CF` catches a hardware or handle error; `AX < CX` with no carry catches a **full disk**,
which DOS reports by writing fewer bytes and *not* setting carry. Checking only `CF` silently
truncates the file.

**36.14** Advantages of function 40h: it can write **any** byte, including `0x24` and zeros, and it
reports how many bytes were actually written — so a full disk is detectable.

---

## Chapter 37

**37.1** `mov ax, 0x0003` / `int 0x10`.

**37.2**

```asm
        mov  ax, 0x0600         ; scroll up 0 lines = clear
        mov  bh, 0x1E           ; yellow (14) on blue (1)
        xor  cx, cx
        mov  dx, 0x184F
        int  0x10
```

**37.3** Bright white on red = 4×16 + 15 = **`0x4F`**. Green on black = **`0x02`**. Blinking yellow
on blue = 128 + 1×16 + 14 = **`0x9E`**.

**37.4**

```asm
        mov  ah, 0x02
        xor  bh, bh
        mov  dh, 12
        mov  dl, 40
        int  0x10
        mov  si, msg
.next:  lodsb
        or   al, al
        jz   .done
        mov  ah, 0x0E
        xor  bh, bh
        int  0x10
        jmp  .next
.done:
msg:    db   'Centre', 0
```

**37.5** Function 09h writes a character **and its attribute**, `CX` times, and does **not** move the
cursor. Function 0Eh is teletype output: it writes one character, **advances the cursor**, and
interprets `CR`, `LF` and backspace.

**37.6**

```asm
        xor  ah, ah
        int  0x16
        or   al, al
        jnz  .normal
        ; extended — AH holds the scan code
        mov  al, ah
        call print_hex8
        jmp  .done
.normal:
        mov  dl, al
        mov  ah, 0x02
        int  0x21
.done:
```

**37.7** `mov ah, 0x02` / `int 0x16` / `test al, 0x04` / `jnz ctrl_down`.

**37.8**

```asm
        mov  cx, 3
.retry: push cx
        mov  ax, 0x0201         ; read, 1 sector
        mov  ch, 0              ; cylinder 0
        mov  cl, 1              ; sector 1
        mov  dh, 0              ; head 0
        mov  dl, 0              ; drive A:
        push ds
        pop  es
        mov  bx, buffer
        int  0x13
        pop  cx
        jnc  .ok
        xor  ah, ah
        mov  dl, 0
        int  0x13               ; reset
        loop .retry
        jmp  .failed
.ok:
```

**37.9** History: the original IBM floppy format numbered physical sectors from 1 on the media, while
cylinder and head are positional coordinates that naturally start at 0. The inconsistency was
preserved for compatibility and has caused off-by-one errors ever since.

**37.10** Cylinder 450 = `0x1C2` = `01 1100 0010`. `CH` = bits 7–0 = **`0xC2`**. `CL` bits 7–6 =
cylinder bits 9–8 = `01`; sector 17 = `010001`. `CL` = `01 010001` = **`0x51`**. `DH` = **3**.

**37.11**

```asm
        mov  ah, 0x00
        int  0x1A
        mov  bx, dx
        add  bx, 55             ; 3 × 18.2 ≈ 55 ticks
.wait:  mov  ah, 0x00
        int  0x1A
        sub  dx, bx
        ; ... compare and loop
```

More robustly, save the starting tick and compare the *difference* against 55, as Chapter 48 §7
does.

**37.12** Reading `0040:006C` directly avoids the 51-clock `INT` plus the BIOS handler — perhaps
200 clocks saved per poll. You must be careful that the counter is 32 bits and an interrupt can
update it between the two word reads, so wrap the pair in `CLI`/`STI`.

**37.13** Because the screen is left in whatever mode and colours the program set. A program that
exits leaving a blue screen or 320×200 graphics is a nuisance; mode 3 is what DOS expects.

**37.14** Status line at the bottom: **BIOS** 10h/02h to position, or direct to `0xB8000`. Log file:
**DOS**. Arrow keys: **BIOS** 16h/00h — DOS cannot report them. Fill the screen with colour:
**BIOS** 10h/06h, or direct. 10,000 pixels: **direct to `0xA0000`** — the BIOS call is about 500
clocks each, or 5,000,000 clocks in total.

---

## Chapter 38

**38.1**

```asm
%macro addto 2
        push ax
        mov  ax, %2
        add  %1, ax
        pop  ax
%endmacro
```

**38.2**

```asm
%macro beep 0
        push ax
        push dx
        mov  dl, 7
        mov  ah, 2
        int  0x21
        pop  dx
        pop  ax
%endmacro
```

Each use costs 2 + 2 + 2 + 2 + 2 + 1 + 1 = **10 bytes**.

**38.3** Because a macro used twice would define the same label twice — `symbol redefined`. `%%`
makes each expansion's label unique.

**38.4**

```asm
%macro max3 3
        mov  ax, %1
        cmp  ax, %2
        jge  %%first
        mov  ax, %2
%%first:
        cmp  ax, %3
        jge  %%done
        mov  ax, %3
%%done:
%endmacro
```

**38.5**

```asm
%macro swapmem 2
        push ax
        push bx
        mov  ax, %1
        mov  bx, %2
        mov  %1, bx
        mov  %2, ax
        pop  bx
        pop  ax
%endmacro
```

**38.6** It is a bad idea because `print_dec` is about 25 instructions; used in ten places it adds
250 instructions to the program, where a procedure would add 25 plus 35 clocks of call overhead per
use.

**38.7**

```asm
fib:
%assign a 0
%assign b 1
%rep 20
        dw   a
%assign t a+b
%assign a b
%assign b t
%endrep
```

**38.8**

```asm
revtab:
%assign n 0
%rep 256
  %assign r 0
  %assign v n
  %rep 8
    %assign r (r << 1) | (v & 1)
    %assign v v >> 1
  %endrep
        db   r
%assign n n+1
%endrep
```

**38.9**

```asm
%macro dbgreg 2
  %ifdef DEBUG
        push ax
        push dx
        mov  dx, %%name
        mov  ah, 9
        int  0x21
        mov  ax, %2
        call print_hex16
        pop  dx
        pop  ax
        jmp  %%past
    %%name: db %1, '=$'
    %%past:
  %endif
%endmacro
```

Assemble with `nasm -DDEBUG ...` to enable it.

**38.10** Every symbol in the third file is defined twice — `symbol redefined` errors. Prevent it
with an include guard: `%ifndef GUARD` / `%define GUARD` / … / `%endif`.

**38.11** (1) A macro is **inlined**, so there is no `CALL`/`RET` overhead but one copy per use; a
procedure is called, so there is 35 clocks of overhead but one copy total. (2) A macro's parameters
are **textual**; a procedure's are values in registers or on the stack. (3) A macro cannot recurse
or be called indirectly. Use a macro for short bodies or where the generated code must differ; a
procedure for anything substantial.

**38.12** A 50-instruction macro used 20 times adds about **1000 instructions** versus a procedure's
50. It saves 20 × 35 = **700 clocks** in total. Almost never worth it.

**38.13** See Chapter 38 §2.5 — `%rep %0` with `%rotate 1` for the pushes and `%rotate -1` for the
pops, which reverses the order automatically.

---

## Chapter 39

**39.1**

```asm
        mov  al, [num1]
        add  al, [num2]
        mov  ah, 0
        adc  ah, 0              ; promote to 16 bits
        mov  bl, [num3]
        mov  bh, 0
        add  ax, bx             ; now a full 16-bit addition
```

**39.2** Use `read_udec` twice, then `ADD`, `SUB`, `MUL` and `DIV` with the guards of Programs 39.4
and 39.5, printing each result with `print_udec`.

**39.3**

```asm
        mul  bx
        jnc  .fits_in_16_bits   ; CF = 0 means DX is zero
```

`MUL` sets `CF = OF = 0` exactly when the upper half of the product is zero, which is the same test
as `or dx, dx` / `jz` but two bytes shorter and available immediately.

**39.4**

```asm
        mov  ax, [dividend]
        mov  bl, [divisor]
        div  bl                 ; 8-bit: quotient must fit in AL
```

With `dividend = 1000` and `divisor = 2` the quotient is 500, which does not fit in `AL` — **`INT 0`**.

**39.5** −7 ÷ 2 = **−3** remainder **−1**. `IDIV` truncates toward zero and the remainder takes the
dividend's sign.

**39.6**

```asm
        mov  ax, [a]
        add  ax, [b]
        mov  bx, ax             ; (a+b)
        mov  ax, [c]
        sub  ax, [d]            ; (c-d)
        imul bx                 ; DX:AX = the 32-bit signed product
```

**39.7** Four partial products. With A = `AH:AL` and B = `BH:BL`:

```
   AL×BL  shifted by 0
   AL×BH  shifted by 16
   AH×BL  shifted by 16
   AH×BH  shifted by 32
```

Accumulate each into a four-word result with `ADD`/`ADC`, propagating carries all the way up.

**39.8** Accumulate in `DX:AX` and use the 32×16 routine of Program 39.9 for each multiplication.
12! = 479,001,600, which fits in 32 bits; 13! does not.

**39.9**

```asm
        or   ax, ax
        jz   .b_is_answer       ; gcd(0, b) = b
        or   bx, bx
        jz   .a_is_answer       ; gcd(a, 0) = a
```

**39.10** `lea si, [si+2]` is 2 + 5 = **7 clocks**, against two `INC`s at **4**. `LEA` also leaves
`CF` alone, so it is correct — but slower. Use the `INC`s.

**39.11**

```asm
        mov  cx, [exponent]
        mov  ax, 1
        jcxz .done
.next:  mul  word [base]
        jc   .overflow          ; DX non-zero -> exceeded 16 bits
        loop .next
.done:
```

**39.12** Accumulate the sum in `DX:AX` with `ADD`/`ADC dx, 0`, then divide by the count with
`div cx` — which is safe because `DX < CX` after summing at most 65,535 values of at most 65,535
each... in fact check `cmp dx, cx` / `jae .overflow` first to be certain.

**39.13** `F = C × 9 / 5 + 32`, correct for negatives:

```asm
        mov  ax, [celsius]
        mov  bx, 9
        imul bx                 ; DX:AX = C × 9, signed
        mov  bx, 5
        idiv bx                 ; AX = C × 9 / 5
        add  ax, 32
```

`IMUL`/`IDIV`, not `MUL`/`DIV` — otherwise −40 °C is treated as 65,496.

---

## Chapter 40

**40.1** Use `LODSB` instead of `LODSW`, advance by 1, and zero-extend each byte before adding:
`lodsb` / `xor ah, ah` / `add bx, ax`.

**40.2** With unsigned comparisons, −12 (`0xFFF4` = 65,524) and −250 (`0xFF06` = 65,286) become
large positive values. Minimum becomes **0**; maximum becomes **−12** (printed as −12 by
`print_sdec`, but chosen because 65,524 is the largest unsigned value present).

**40.3**

```asm
        mov  si, arr
        mov  cx, len
        xor  bx, bx
.next:  lodsw
        or   ax, ax
        jns  .skip
        inc  bx
.skip:  loop .next
```

**40.4** Track two values: `max` and `second`. For each element, if it beats `max`, the old `max`
becomes `second`; otherwise if it beats `second`, it becomes `second`.

**40.5** Change `mov cx, len` / `shr cx, 1` to `mov cx, len/4`, and set `DI` to
`arr + (len/2 - 1)*2`.

**40.6** Rotate right: save the **last** element, then move everything up one place. The destination
(`arr+2`) is **above** the source (`arr`), so the copy must go **backwards** with `STD`.

**40.7** Rotate left by *n* is *n* single rotations, or — far better — a three-reversal trick:
reverse `arr[0..n-1]`, reverse `arr[n..len-1]`, then reverse the whole array. That is O(len) rather
than O(n × len).

**40.8** Change `counts` to `times 256 db 0`, drop the `shl bx, 1` (byte entries), and use
`inc byte [counts+bx]`. A value appearing more than 255 times **wraps to 0** silently — which is why
word counters are usually worth the space.

**40.9**

```asm
        mov  si, arr
        mov  cx, len
        dec  cx
        jcxz .sorted
        lodsw
.next:  mov  bx, ax
        lodsw
        cmp  bx, ax
        jg   .not_sorted        ; signed
        loop .next
.sorted:
```

**40.10** For each element, scan everything before it for an equal value; keep it only if none is
found. That is **O(n²)** rather than the sorted version's O(n).

**40.11**

```asm
        mov  si, arr
        mov  cx, len
        xor  bx, bx             ; even-index sum
        xor  dx, dx             ; odd-index sum
        xor  di, di             ; index
.next:  lodsw
        test di, 1
        jnz  .odd
        add  bx, ax
        jmp  .step
.odd:   add  dx, ax
.step:  inc  di
        loop .next
        mov  ax, bx
        sub  ax, dx
```

**40.12** Accumulating into memory costs 16 + 6 = 22 clocks per `ADD` against 3 for a register,
plus the same again for the `ADC` — about **38 extra clocks per element**. For a 100-element vector
that is 3,800 clocks.

**40.13** Work **backwards** from the highest index of the destination, taking the larger of the two
current elements each time. That way you never overwrite an element you have not yet read.

**40.14** Walk the array keeping `current_value`, `current_run` and `best_run`. When the value
changes, compare `current_run` with `best_run` and reset.

---

## Chapter 41

**41.1** With a shrinking inner loop the comparison count falls from `(n-1)²` to `n(n-1)/2`. For
n = 8: 49 → **28**, a saving of 21 comparisons.

**41.2** Bubble: change `jle` to `jge`. Selection: change `jge` to `jle`. Insertion: change `jle` to
`jge`. One instruction each.

**41.3** With `jbe`, −5 (`0xFFFB` = 65,531) is the **largest** unsigned value, so it sorts to the
end: `11 12 22 25 34 64 90 -5`.

**41.4** Add `inc word [swaps]` at each swap site. On the sample data, selection sort makes at most
7 swaps, bubble sort many more.

**41.5** **Bubble with early exit** — one pass, `n−1` comparisons, no swaps, then it stops.
Insertion sort is also O(n) on sorted data. Selection sort is O(n²) regardless.

**41.6** **Insertion sort is worst** on reverse-sorted data — every element shifts the whole way.
Selection sort is unaffected, because it always does the same work.

**41.7** Scan for the maximum rather than the minimum, and swap it to position `n−1−i`, working the
outer loop from the end inwards.

**41.8** Use `BYTE` loads (`mov al, [si]`), advance pointers by 1 instead of 2, and compare with
`cmp al, bl`.

**41.9** Because `REPNE SCASW` advances `DI` past the matching element before stopping. Subtracting
2 (one word) points it back at the match.

**41.10** Hand loop ≈ 31 clocks per element; `REPNE SCASW` ≈ 15. For 100 elements: 3,100 → 1,500,
saving about **1,600 clocks**.

**41.11** Remove `or dx, dx` / `jz .fail` and search for a value smaller than `arr[0]` — say 1. At
some point `mid = 0` and `arr[0] > 1`, so `hi = mid − 1 = −1 = 0xFFFF`. The loop then compares
`lo (0) > hi (65535)`, which is false, and the search runs away through the whole address space.

**41.12** Quicksort's crossover with insertion sort is typically around **n = 10–20** on an 8086,
because quicksort's per-element constant (partitioning plus recursion overhead) is several times
insertion sort's.

**41.13** Continue the scan after each match rather than returning, collecting the indices into an
array.

**41.14** Sorting 100 elements costs about 1,000,000 clocks. Each search saves ~1,150. Fifty searches
save 57,500 — **far less than the sort costs**. Use `REPNE SCASW` on the unsorted data.

---

## Chapter 42

**42.1** Replace `or al, al` / `jz` with `cmp al, '$'` / `je`. `$`-terminated strings are for DOS
function 09h; ASCIIZ is for file functions and for C compatibility.

**42.2**

```asm
strncpy:                        ; DS:SI -> src, ES:DI -> dst, CX = max
        cld
.next:  jcxz .done
        lodsb
        stosb
        dec  cx
        or   al, al
        jnz  .next
        ; pad the rest with zeros
        xor  al, al
        rep  stosb
.done:
```

**42.3** Fold both characters to lower case before comparing: `or al, 0x20` and `or bl, 0x20`, with
a range check so non-letters are unaffected.

**42.4** Walk the haystack; at each position compare the needle with `CMPSB` in a nested loop;
return the position on a full match.

**42.5** Scan the whole string, remembering the position of the most recent match rather than
returning at the first.

**42.6** It finds the **terminator itself** and returns a pointer to it. That matches C's `strchr`,
where `strchr(s, 0)` returns a pointer to the end of the string — a documented and useful
behaviour.

**42.7** Scan forward past leading spaces; scan back from the end replacing trailing spaces with
zero; then move the remainder down to the start.

**42.8** Track the start position of the current word and its length; when a separator is reached,
compare the length with the best so far.

**42.9** Search from position 0; on a match, advance the search position by **one** (not by the
pattern length) to allow overlaps.

**42.10** Without the checks, `and al, 0xDF` turns `'{'` (`0x7B`) into `'['` (`0x5B`), and
`or al, 0x20` turns `'['` into `'{'`. Any input containing brackets or the characters `@`, `^`,
`` ` `` or `_` is corrupted.

**42.11** Track a "start of word" flag: upper-case the first letter after a separator, lower-case
everything else.

**42.12** `REPNE SCASB` to measure (15 clocks per character) plus `REP MOVSB` to copy (17 per
character) = **32 per character**, against the `LODSB`/`STOSB` loop's 42. For 20 characters: 640 + 18
overhead versus 840 — the string version wins, and `REP MOVSW` would halve the copy again.

**42.13** Compute the length difference; if the replacement is longer, move the tail **right**
(backwards copy) before writing; if shorter, write then move the tail **left**.

**42.14** Put the pattern pointer, replacement pointer and length in locals at `[BP-2]`, `[BP-4]`
and `[BP-6]`, passing the three addresses as stack parameters. That makes the routine reentrant,
which the memory-variable version is not.

**42.15** Count the frequency of each character in both strings (Program 42.11's technique) and
compare the two 26-entry tables.

---

## Chapter 43

**43.1** Because `DX` holds the previous iteration's remainder, and `DIV` uses `DX:AX` as the
dividend. A stale `DX` makes the dividend enormous, so the quotient exceeds 16 bits and the 8086
raises **`INT 0`** — a crash, not a wrong digit.

**43.2** Count the digits produced; emit `5 − count` leading `'0'` characters before popping them.

**43.3** Emit a `','` every three digits from the right, which is easy in the pop loop: track the
digit index and insert a separator when `(count − index) mod 3 == 0` and `index > 0`.

**43.4** Multiplication overflow: `"70000"` — at the fifth digit, `7000 × 10 = 70,000`, so `DX`
becomes non-zero. Addition carry: `"65540"` — `6554 × 10 = 65,540` already overflows; a subtler case
is `"65536"`, where `6553 × 10 = 65,530` fits and adding 6 carries out.

**43.5** Keep the accumulator in `DX:AX`; multiply by 10 with a 32×16 routine (Program 39.9), and
add each digit with `ADD`/`ADC`.

**43.6** `'9'` is `0x39` and `'A'` is `0x41` — a gap of 8, of which `add al, '0'` supplies one.
Without the `add al, 7`, the value 10 would print as `0x3A`, which is `':'`.

**43.7** Change `add al, 7` to `add al, 39` (`'a'` = `0x61` is 39 past `0x3A`), or use a lower-case
`XLAT` table.

**43.8**

```asm
bin_to_hex8:
        push ax
        mov  bx, hextab
        mov  ah, al
        mov  cl, 4
        shr  al, cl
        xlat
        stosb
        mov  al, ah
        and  al, 0x0F
        xlat
        stosb
        pop  ax
        ret
```

**43.9** Call `bin_to_hex` on `DX` first, then on `AX`.

**43.10** Skip leading zeros before starting the digit count, and only increment `CX` once a non-zero
digit has been seen.

**43.11** Horner with a base of 8: `result = result × 8 + digit`, with the ×8 done as three `SHL`s
and the digit range checked as `'0'`–`'7'`.

**43.12** For 9999: thousands = 9 → `CX = 0x0009` → shifted `0x0090`; hundreds = 9 → `0x0099` →
`0x0990`; tens = 9 → `0x0999` → `0x9990`; units = 9 → **`0x9999`** ✔

**43.13** Extract each nibble with `ROL` and `AND 0x0F`, add `'0'`, and store — four digits, no
division at all.

**43.14**

```asm
; BL = base (2-16), DS:SI = the string; AX = value, CF = 1 on error
str_to_base:
        xor  cx, cx             ; accumulator
.next:  lodsb
        or   al, al
        jz   .done
        ; convert the character to 0-15
        cmp  al, '9'
        jbe  .digit
        or   al, 0x20
        sub  al, 'a'-10
        jmp  .check
.digit: sub  al, '0'
.check: cmp  al, bl
        jae  .bad               ; the digit is not valid in this base
        push ax
        mov  ax, cx
        xor  bh, bh
        mul  bx                 ; accumulator × base
        mov  cx, ax
        pop  ax
        xor  ah, ah
        add  cx, ax
        jmp  .next
.done:  mov  ax, cx
        clc
        ret
.bad:   stc
        ret
```

**43.15** Compare-and-add ≈ 62 clocks per digit × 4 = **248**. `XLAT` ≈ 53 × 4 = **212**, plus the
16-byte table. The `XLAT` version wins on speed and has no branch; 16 bytes is negligible unless you
are writing a boot sector.

---

## Chapter 44

**44.1** `M[2][3]` in a 4×5 word matrix: `(2 × 5 + 3) × 2` = **26**.

**44.2** Column-major: `(3 × 4 + 2) × 2` = **28**. **C uses row-major.**

**44.3**

```asm
        mov  al, bl             ; i
        mov  ah, 6
        mul  ah                 ; AX = i × 6
        xor  ch, ch
        add  ax, cx             ; + j
        mov  si, ax
        mov  al, [M + si]       ; byte elements — no shift needed
```

**44.4** Change `add ax, [di]` to `sub ax, [di]`.

**44.5** Swap `M[i][j]` with `M[j][i]` for all `j > i`. It fails for a non-square matrix because the
transpose has different dimensions — element `[0][3]` of a 3×4 has no in-place home in a 3×4 array.

**44.6** The **destination** matrix has `ROWS` columns, because it is the transpose. Using `COLS`
would index as though the destination were still 3×4, scattering the elements.

**44.7** Keep the accumulator in a 32-bit memory variable and use `add`/`adc` with the full `DX:AX`
product after each `IMUL`.

**44.8** Hoisting `i × COLS_A` out of the `j` loop removes one multiply per inner iteration, and
replacing `B[j][k]`'s calculation with a pointer that advances by `COLS_B × 2` removes the other.
For a 10×10 product that is 2,000 multiplies at ~130 clocks each — about **260,000 clocks** saved,
roughly 65% of the total.

**44.9** Walk the flattened array and `IMUL` each element by the scalar.

**44.10** Walk sequentially with `LODSW`, tracking the maximum and its linear index; at the end,
`row = index / COLS` and `col = index mod COLS` — one `DIV`.

**44.11** Transposing costs `ROWS × COLS` element moves plus the index arithmetic. It pays only if
the columns will be summed **many** times, because each strided pass costs roughly the same as one
transpose.

**44.12** Any matrix whose cofactors exceed 16 bits — for example all elements around 300, giving
products near 90,000. Fix: keep each `IMUL`'s full `DX:AX` and accumulate in 32 bits.

**44.13** Compare `M[i][j]` with `M[j][i]` for all `j > i`; report the first mismatch.

**44.14** The diagonal elements are at linear indices 0, N+1, 2(N+1), … — so advance a pointer by
`(N+1) × 2` bytes each step. No multiplication needed.

---

## Chapter 45

**45.1**

```asm
        cmp  ax, 320
        jae  .out               ; unsigned: catches x < 0 as a huge value
        cmp  bx, 200
        jae  .out
```

With `x = 400` the unchecked version writes to `y × 320 + 400`, which lands 80 pixels into the
**next row** — the pixel appears in the wrong place, and at `y = 199` it writes past the end of the
buffer.

**45.2** `150 × 320 + 200` = **48,200** = `0xBC48`.

**45.3** Because an 8086 transfers a word in one bus cycle, so `STOSW` moves two bytes for the same
10 clocks. An **8088** has an 8-bit bus, so a word is two bus cycles and the two forms take the same
time.

**45.4** A vertical gradient (each *row* a colour) is fast — one `REP STOSB` per row. A **horizontal**
gradient (each *column* a colour) needs a separate store per pixel with a 320-byte stride, because
consecutive pixels of one colour are not adjacent. It is about 3.5× slower, the same row-major
asymmetry as Chapter 44 §5.

**45.5** Clamp `x1` to ≥ 0 and `x2` to ≤ 319, and return immediately if `y` is outside 0–199 or if
`x1 > x2` after clamping.

**45.6** The table costs 200 × 25 = 5,000 clocks to build and saves about 60 clocks per pixel, so it
pays for itself after about **84 pixel writes** — essentially immediately.

**45.7** Two `hline` calls (top and bottom) and two `vline` calls (left and right), with the
corners drawn by either.

**45.8** Because `err`, `dy` and `e2` are genuinely **signed** — `dy` is deliberately kept negative
and `err` starts as `dx + dy`, which can be either sign. Unsigned comparisons would treat a negative
`err` as a huge positive value and the line would go astray.

**45.9** Replace `plot8` with four `hline` calls spanning from `cx−px` to `cx+px` at
`cy±py`, and from `cx−py` to `cx+py` at `cy±px`.

**45.10** An ellipse uses the same midpoint approach with the decision variable derived from
`b²x² + a²y² = a²b²`; it needs two loops, one for each region where the slope crosses −1.

**45.11** Use `setcolour` in three loops: entries 0–63 with `(n, 0, 0)`, 64–127 with `(0, n−64, 0)`,
128–191 with `(0, 0, n−128)`.

**45.12** The copy is 32,000 words with `REP MOVSW`: 9 + 32,000 × 17 = **544,009 clocks** = 109 ms at
5 MHz. The vertical retrace lasts about 1.4 ms — so **no**, it does not fit, and an 8086 cannot
double-buffer a full mode-13h screen at frame rate. Copy only the changed rectangles instead.

**45.13** Keep two sets of position and velocity; after moving both, compare the distance between
centres against the sum of the radii — using squared distances to avoid a square root.

**45.14** For each row `dy` from `−r` to `+r`, compute `w = isqrt(r² − dy²)` once and draw a
horizontal line from `−w` to `+w`. That replaces 289 per-pixel tests with 17 square roots and 17
`hline` calls — roughly **five times faster**, and better still because each row is a `REP STOSB`.

---

## Chapter 46

**46.1** `t` traces **into** calls and interrupts; `p` proceeds **over** them. Use `p` on `int 0x21`,
or you will spend a long time single-stepping the DOS kernel.

**46.2** The **file size in bytes**. DOS leaves it there, and it is one of the few register values
you can rely on at entry.

**46.3** All clear: no overflow, direction up, interrupts enabled, result plus (positive), not zero,
no auxiliary carry, parity odd, no carry.

**46.4**

```
   debug hello.com
   -g 10b
   -d ds:010d l 10
```

**46.5** By writing `0xCC` (`INT 3`) over the first byte of the target instruction, saving the
original. It must be one byte because a breakpoint has to be settable on **any** instruction,
including one-byte ones — a two-byte breakpoint would overwrite the start of the next instruction.

**46.6** `pushf` / `pop ax` / `or ax, 0x0100` / `push ax` / `popf` — five, strictly, but the `or` is
the one that matters.

**46.7** `IP` at **`[BP+2]`**, `CS` at `[BP+4]`, flags at `[BP+6]`.

**46.8** (1) Count the pushes and pops on **every** path through the procedure, including early
exits. (2) Check that `RET` matches the `CALL`'s distance — near with near, far with far.

**46.9** (a) `LOOP` entered with `CX = 0` — 65,536 iterations. (b) A missing `$` terminator.
(c) An interrupt vector not restored before exit. (d) A missing `xor dx, dx` before `DIV`, or a
quotient too large for its destination. (e) `ES` not set before a string instruction — it points at
the PSP in an `.EXE`.

**46.10**

```asm
%macro dbgax 0
        push ax
        push bx
        push cx
        push dx
        push ds
        push cs
        pop  ds
        mov  ax, [bp-2]         ; or wherever the value is
        call print_hex16
        pop  ds
        pop  dx
        pop  cx
        pop  bx
        pop  ax
%endmacro
```

The key points for interrupt-handler use: save everything, set `DS` from `CS`, and use BIOS `INT 10h`
rather than DOS output.

**46.11** `mov cx, 0` then `LOOP` — the loop runs **65,536 times**, copying far past the end of the
buffer and corrupting everything after it. The symptom is a program that appears to hang for about
13 seconds and then behaves impossibly.

**46.12** Put `int3` at instruction 1000. If it is reached, move to 1500; if not, to 500. Each run
halves the interval, so **11 runs** locate any single instruction in 2000.

**46.13** `AX` after each iteration: 10, then `0x0A0A` (2570), then it continues accumulating
garbage — because `SI` advanced by 1 and the second read took the high byte of element 0 and the low
byte of element 1 as a word.

---

## Chapter 47

**47.1** Port A input (bit 4 = 1), Port B output (bit 1 = 0), Port C upper output (bit 3 = 0),
Port C lower input (bit 0 = 1), all mode 0:
`1 00 1 0 0 0 1` = `1001 0001` = **`0x91`**.

**47.2** `1 00 1 1 0 1 1` = `1001 1011` = **`0x9B`**.

**47.3** `0x92` = `1001 0010`: mode set; Port A **mode 0, input**; Port C upper **output**;
Port B **mode 0, input**; Port C lower **output**.

**47.4** Port A `0x40`, Port B `0x42`, Port C `0x44`, Control `0x46`.

**47.5**

```asm
        mov  al, 0000_1011b     ; bits3-1 = 101 (PC5), bit0 = 1 (set)
        out  CTRL, al
        mov  al, 0000_0100b     ; bits3-1 = 010 (PC2), bit0 = 0 (clear)
        out  CTRL, al
```

**47.6** Because reading Port C does not reliably return what you last **wrote** to it — in modes 1
and 2 some bits are handshake status, and inputs are not latched. Bit set/reset changes exactly one
bit atomically, with no read at all.

**47.7** `in al, PORTB` / `not al` / `shl al, 1` / `out PORTA, al`.

**47.8** Keep two patterns in different registers, shift one left and the other right, `OR` them
together before the `out`:

```asm
        mov  bl, 0x01           ; running left
        mov  bh, 0x80           ; running right
.next:  mov  al, bl
        or   al, bh
        out  PORTA, al
        call delay
        shl  bl, 1
        jnz  .no_reset_l
        mov  bl, 0x01
.no_reset_l:
        shr  bh, 1
        jnz  .no_reset_r
        mov  bh, 0x80
.no_reset_r:
        jmp  .next
```

**47.9** `ROL` brings the departing bit back in at the other end, so `0xFE` cycles through `0xFD`,
`0xFB`, `0xF7` and back to `0xFE` — exactly one row low at a time, for ever. `SHL` would bring in a
zero and eventually drive **all** rows low.

**47.10** A mechanical switch bounces for 5–20 ms, producing dozens of transitions per press.
Without debouncing, one keypress registers many times.

**47.11** Invert every entry: `db 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90` — or
keep the common-cathode table and `not al` after the `XLAT`.

**47.12** Four digits × 2 ms = 8 ms per cycle, so **125 Hz**. At 20 ms per digit the cycle is 80 ms
= 12.5 Hz, which is well below the flicker threshold — the display would visibly strobe.

**47.13** `IBF` (Input Buffer Full) is driven **by the 8255** and tells the peripheral "I have your
byte; do not send another". `STB#` is driven **by the peripheral** and tells the 8255 "here is a
byte; latch it".

**47.14** Because the mode-set control word has no spare bits — all eight are used for the mode and
direction fields. Intel reused the bit set/reset path as a back door to internal flip-flops that
have nothing to do with the Port C pins those bit numbers name.

**47.15** Decode with a 74LS138: `C B A` ← `A5 A4 A3`, `G1` ← inverted `M/IO#`, `G2A#` ← `A7`,
`G2B#` ← `A6`. `Y4#` then covers ports `0x20`–`0x27`. Connect `Y4#` to `CS#`, system `A2 A1` to the
8255's `A1 A0`, `D7`–`D0` to the low half of the data bus, `RD#`←`IOR#`, `WR#`←`IOW#`, and `RESET`
to the 8284A's reset output.

---

## Chapter 48

**48.1** Counter 1 (`01`), read/write both (`11`), mode 2 (`010`), binary (`0`):
`01 11 010 0` = **`0x74`**.

**48.2** `0xB6` = `10 11 011 0`: counter **2**, LSB then MSB, mode **3**, binary. This is the PC
speaker's control word.

**48.3**

```asm
        mov  al, 0x36
        out  0x43, al
        mov  ax, 5000
        out  0x40, al
        mov  al, ah
        out  0x40, al
```

**48.4** N = 1,193,182 ÷ 2000 = **596** (596.59 rounded down).

**48.5** 1,193,182 ÷ 2048 = **582.6 Hz**.

**48.6** Because the counter decrements *then* tests for zero, so a loaded value of 0 wraps to 65,536
counts. On a PC that gives 1,193,182 ÷ 65,536 = **18.2065 Hz**, a period of 54.925 ms.

**48.7** Mode 2 is low for **one clock** out of N — a narrow periodic strobe. Mode 3 is a symmetric
**square wave**, high N/2 and low N/2. For a tone use **mode 3**: mode 2's one-clock pulse is far
quieter and buzzier.

**48.8**

```asm
        cli
        mov  al, 0x00           ; counter 0, LATCH
        out  0x43, al
        in   al, 0x40
        mov  ah, al
        in   al, 0x40
        xchg al, ah             ; AX = the latched count
        sti
```

The latch is necessary because the counter is decrementing continuously; reading the two bytes
separately can catch it mid-decrement and return a value that never existed.

**48.9** Because the 8253 has an internal flip-flop tracking which byte comes next. An interrupt
handler that writes to the same counter in between leaves the flip-flop in the wrong state, and the
counter loads the bytes reversed.

**48.10** N = 1,193,182 ÷ 440 = 2712.

```asm
        mov  al, 0xB6
        out  0x43, al
        mov  ax, 2712
        out  0x42, al
        mov  al, ah
        out  0x42, al
        in   al, 0x61
        or   al, 0x03
        out  0x61, al
        ; wait one second, then
        in   al, 0x61
        and  al, 0xFC
        out  0x61, al
```

**48.11** On an XT it triggers **DRAM refresh** every ~15 µs through DMA channel 0. Reprogramming it
stops the refresh and memory contents decay within milliseconds — the machine crashes and the data
is gone.

**48.12** N = 1,193,182 ÷ 100 = 11,932, control word `0x36`. You must also hook `INT 08h` and chain
to the original BIOS handler every 5.5 of your ticks (100 ÷ 18.2), or the time of day runs fast by a
factor of 5.5.

**48.13**

```asm
        mov  al, 1110_1000b     ; read-back: status only, counter 2
        out  0x43, al
        in   al, 0x42
        test al, 0x80           ; bit 7 = the current OUT pin state
```

**48.14** N = 2,000,000 ÷ 1000 = **2000**.

**48.15** Mode 3 gives only 50%. For 20% you need either two counters — one in mode 2 setting a
flip-flop and a second resetting it — or a dedicated PWM peripheral. A single 8253 counter cannot
produce an arbitrary duty cycle.

---

## Chapter 49

**49.1** (1) The 8086 has one `INTR` pin, and an OR gate loses which device asserted. (2) Something
must supply the **type number** during the second `INTA#` cycle, and a plain device does not know
its own. (3) Simultaneous requests need **priority** resolution.

**49.2** **IRR** — which inputs are currently asserting. **ISR** — which interrupts are being
serviced. **IMR** — which inputs are masked off (1 = disabled).

**49.3** ICW1 = `0x13` (edge, single, ICW4 needed). ICW2 = `0x40` (IR0 → `INT 40h`). ICW3 —
**omitted**, because `SNGL = 1`. ICW4 = `0x01` (8086 mode).

**49.4** **Bit 0 of ICW4** (`µPM`). With it 0 the chip generates 8080-style `CALL` instructions on
the data bus instead of an interrupt type number, and nothing works at all.

**49.5** `mov al, 0xBE` / `out 0x21, al` — bits 0 and 6 clear (IRQ0 and IRQ6 enabled), all others
set.

**49.6** See Chapter 49 §4.1's `enable_irq` — `shl ah, cl` builds the bit, `not ah` inverts it, and
`and` clears it in the mask read back from the port.

**49.7** It writes a **non-specific EOI** to OCW2, clearing the highest-priority ISR bit. Without it,
that ISR bit stays set for ever, the priority resolver believes the interrupt is still being
serviced, and no further interrupt of that priority or lower is ever delivered.

**49.8** IRQ6's ISR bit remains set. IRQ6 and everything of lower priority (IRQ7) are blocked
permanently. The interrupt fires exactly **once** and the device appears dead thereafter.

**49.9**

```asm
        mov  al, 0x0B           ; OCW3: read ISR
        out  0x20, al
        in   al, 0x20           ; AL = ISR; a set bit is a handler in progress
```

**49.10** IR2 has **higher** priority than IR5, so it interrupts the IR5 handler — *provided* that
handler has executed `STI`. IR7 has lower priority, so it waits in the IRR until the IR5 handler
sends its EOI.

**49.11** Because the 8086's `INT` sequence clears `IF` automatically. Until the handler executes
`STI`, no maskable interrupt of any priority can occur.

**49.12** **Fifteen**, not sixteen, because one of the master's eight inputs is consumed by the
cascade connection to the slave.

**49.13** IRQ12 is on the slave, so **two** EOIs:

```asm
        mov  al, 0x20
        out  0xA0, al           ; slave FIRST
        out  0x20, al           ; then master
```

**49.14** An `INT` pushes flags, `CS`, `IP`. `PUSHF` supplies the flags and the far `CALL` supplies
`CS` and `IP`, so the stack looks exactly as it would after a hardware interrupt. The chained
handler's `IRET` therefore pops all three and returns to the instruction after the `CALL`.

**49.15**

```asm
; master at 0x20/0x21
        mov  al, 0x11           ; ICW1: edge, CASCADED, ICW4 needed
        out  0x20, al
        mov  al, 0x20           ; ICW2: IR0 -> INT 20h
        out  0x21, al
        mov  al, 0x08           ; ICW3: a slave on IR3 (bit 3)
        out  0x21, al
        mov  al, 0x01           ; ICW4: 8086 mode
        out  0x21, al

; slave at 0xA0/0xA1
        mov  al, 0x11
        out  0xA0, al
        mov  al, 0x28           ; ICW2: slave IR0 -> INT 28h
        out  0xA1, al
        mov  al, 0x03           ; ICW3: "I am the slave on IR3"
        out  0xA1, al
        mov  al, 0x01
        out  0xA1, al
```

---

## Chapter 50

**50.1** 1 start + 7 data + 1 parity + 2 stop = **11 bits**. At 1200 baud that is 1200 ÷ 11 =
**109 characters per second**.

**50.2** Stop = 2 (`11`), even parity (`11`), 7 bits (`10`), ×16 (`10`) = `1111 1010` = **`0xFA`**.

**50.3** `0xCE` = `11 00 11 10`: **2 stop bits, no parity, 8 data bits, ×16** — "8-N-2".

**50.4** Because at ×16 the receiver samples each bit sixteen times and takes the middle sample,
giving tolerance to clock mismatch between the two ends. At ×1 there is no margin at all — the
sampling instant must coincide exactly with the bit centre.

**50.5** Three dummy writes of `0x00` followed by command `0x40` (internal reset), then the mode word
`0x4E`, then the command word `0x37`. The dummy writes are needed because the chip has no
"what state are you in?" query: it may be part-way through a previous mode or sync sequence, and
three zero bytes followed by an internal reset put it in a known state whatever it was doing.

**50.6** 4800 × 16 = **76,800 Hz**.

**50.7** 2400 × 16 = 38,400. N = 1,843,200 ÷ 38,400 = **48**.

**50.8** Because it divides exactly to every standard rate: 1,843,200 ÷ 16 = 115,200, and
115,200 ÷ 12 = 9600, 115,200 ÷ 48 = 2400. Round frequencies like 2 MHz give fractional divisors and
an accumulating rate error.

**50.9** **Parity**: the received parity bit disagreed — a corrupted bit, usually noise.
**Overrun**: a character arrived before the previous one was read — **a software problem**, because
the polling loop or interrupt handler is too slow. **Framing**: the stop bit was not high — almost
always a baud-rate mismatch.

**50.10** The two ends disagree about the **baud rate**. The receiver samples at the wrong instants,
so the frame boundary falls in the middle of a bit and the stop bit reads low.

**50.11**

```asm
puts_serial:
        cld
.next:  lodsb
        or   al, al
        jz   .done
        call putc_serial
        jmp  .next
.done:  ret
```

**50.12** Because writing while the transmit buffer is still full **overwrites the pending
character**, which is then lost with no error indication.

**50.13** A power of two lets `and bx, BUFSIZE-1` replace a division — 3 clocks instead of 144. One
slot is left unused so that `head == tail` unambiguously means **empty**; otherwise a full buffer
would look identical to an empty one.

**50.14** Because in `buf_getc` the interrupt handler can change `rxhead` between the `mov bx,
[rxtail]` and the `cmp bx, [rxhead]`, producing a wrong decision. Inside the handler, interrupts are
already disabled by the `INT` sequence.

**50.15** The **Divisor Latch Access Bit**, bit 7 of the line control register at `0x3FB`. With it
set, ports `0x3F8`/`0x3F9` address the baud-rate divisor instead of the data and interrupt-enable
registers. Leaving it set means every subsequent write goes to the divisor and nothing is ever
transmitted.

**50.16** `TxD` and `RxD` cross because both ends are "terminals" — each transmits on pin 3 and
listens on pin 2, so without the crossover both would talk into each other's transmitters and
neither would hear anything. `RTS`/`CTS` and `DTR`/`DSR` cross for the same reason.

---

## Chapter 51

**51.1** `IN`/`STOSB`/`LOOP` ≈ 8 + 11 + 17 = 36 clocks and **two** bus cycles per byte, so
512 × 36 = **18,432 clocks**. DMA needs **512 bus cycles** — about 2,048 clocks of bus time, during
much of which the CPU can still run from its queue.

**51.2** (1) `DREQn` from the device. (2) `HRQ` from the 8237 to the 8086's `HOLD`. (3) The 8086
finishes its cycle, floats its bus pins and asserts `HLDA`. (4) The 8237 drives the bus and asserts
`DACKn`. (5) The transfer, with `IOR#` and `MEMW#` together. (6) The 8237 releases `HRQ`. (7) The
8086 drops `HLDA` and resumes.

**51.3** Because it has floated **all** its bus pins — it has no address bus to fetch with.

**51.4** Single mode releases the bus after each byte, so the CPU gets it back between transfers.
Block mode holds the bus for the whole count, freezing the machine — a 64 KiB block at 5 MHz would
lock out interrupts for about 50 ms and lose timer ticks.

**51.5** **Device → memory.** The names are from memory's point of view, so a DMA "write" fills
memory, which is what a disk *read* does.

**51.6** Channel 3, single (`01`), increment (`0`), auto-init (`1`), read from memory (`10`):
`01 0 1 10 11` = `0101 1011` = **`0x5B`**.

**51.7** `0x58` = `01 0 1 10 00`: channel **0**, single mode, increment, **auto-initialise**, read
from memory.

**51.8** Because the flip-flop decides whether the next write goes to the low or high byte of the
16-bit register. If it is in the wrong state the bytes land reversed, and the transfer address is
off by a factor of 256 — writing into the wrong part of memory entirely.

**51.9** **1023.** The 8237 transfers `count + 1` bytes.

**51.10** `0x1FF00 + 512 = 0x20100`, which crosses the `0x20000` boundary. **Yes, it crosses.** The
page register does not increment, so after 256 bytes the 8237's address wraps from `0xFFFF` to
`0x0000` and the remaining bytes are written to `0x10000`–`0x100FF` — the **start of the same
64 KiB page**, overwriting whatever is there.

**51.11**

```asm
        mov  ax, es
        mov  dx, ax
        mov  cl, 4
        shl  ax, cl             ; the low 16 bits of ES × 16
        shr  dx, cl             ; DX = the top 4 bits = the page
        add  ax, bx
        adc  dx, 0              ; carry into the page
```

**51.12** It drives **DRAM refresh** on an XT, triggered by 8253 counter 1. Stopping it destroys
memory contents within milliseconds.

**51.13** With bit 4 of the mode register set, the address and count reload from internal base
registers when the transfer completes, and the channel restarts automatically. Needed for continuous
transfers — sound playback from a circular buffer, or video capture — where a software gap between
buffers would be audible or visible.

**51.14**

```asm
        in   al, 0x08           ; read ONCE — this clears the TC bits
        mov  bl, al
        test bl, 0x04           ; channel 2 TC
        test bl, 0x08           ; channel 3 TC
```

**51.15** See Chapter 51 §10 — the 8237 (DMA), the floppy controller, the 8086 (bus handshake), the
8259A (the completion interrupt), and the 8253 indirectly, since DRAM refresh continues throughout.

---

## Chapter 52

**52.1** 10-bit over 0–5 V: 5 ÷ 1024 = **4.88 mV**. 12-bit over 0–2.5 V: 2.5 ÷ 4096 = **0.61 mV**.

**52.2** 200 × 5 ÷ 256 = **3.906 V**. The step size is 19.5 mV, so quoting **3.91 V** is as precise
as the converter justifies.

**52.3** Because integer division truncates. `128 × 5000 / 256 = 2500` mV ✔, but
`128 / 256 × 5000 = 0 × 5000 = 0` ✘.

**52.4** Minimum **8 kHz** (strictly, more than 8 kHz). At 6 kHz, components between 3 kHz and 4 kHz
alias down to 2–3 kHz and appear as tones that were never present.

**52.5** (1) Put the channel number on `ADD C/B/A`. (2) Pulse `ALE` to latch it. (3) Pulse `START`.
(4) Wait for `EOC` to go high. (5) Assert `OE` and read `D7`–`D0`. (6) Release `OE`.

**52.6** Because a disconnected, unpowered or faulty ADC never asserts `EOC`, and the loop spins for
ever. The program freezes with no indication of why.

**52.7** Because the conversion time depends on the ADC's clock frequency, which may not be what you
assumed. Polling `EOC` is correct whatever the clock; a fixed delay is correct only for one
particular clock.

**52.8**

```asm
        mov  cx, 8
        xor  bl, bl             ; channel
        mov  di, readings
.next:  mov  al, bl
        call adc_read
        jc   .fail
        mov  [di], al
        inc  di
        inc  bl
        loop .next
```

**52.9** Maximum 255 × 625 = **159,375** = `0x26E8F`, which needs 18 bits. A plain `shr ax, 5` would
discard the two bits that ended up in `DX`, giving 4980 − 4096 = 884 mV instead of 4980. The `DX`
handling shifts the full 32-bit value.

**52.10** The sum of the first *n* odd numbers is *n*², so subtracting 1, 3, 5, 7… until the
remainder goes negative counts the integer square root. For 30: 30−1 = 29 (n=1), 29−3 = 26 (n=2),
26−5 = 21 (n=3), 21−7 = 14 (n=4), 14−9 = 5 (n=5), 5 < 11 → stop. **Answer 5**, and 5² = 25 ≤ 30 <
36 ✔

**52.11** 1 kHz means one cycle per millisecond = 5000 clocks at 5 MHz. At 30 clocks per sample that
is **166 samples per cycle** — so a 128-entry table is comfortable, 256 is not.

**52.12** Averaging reduces uncorrelated noise by √8 ≈ 2.8×, so roughly 1.5 bits of effective
resolution are gained. It costs **eight conversions** — about 800 µs instead of 100 µs — so the
maximum sample rate falls by a factor of eight.

**52.13** 8255 control word `0x99` (A input for the ADC result, B output for the ADC control lines,
C input for `EOC`) — but that leaves no output port for the DAC. Use two 8255s, or put the DAC on
Port C upper and the `EOC` on Port C lower with control word `0x98`... in practice, dedicate Port A
to the ADC data (input), Port B to the DAC (output), and Port C lower to the ADC control with
Port C upper reading `EOC` — control word `0x98`, and drive `ALE`/`START`/`OE` from Port C lower via
bit set/reset.

**52.14** (1) The reference voltage is low — measure `VREF+` directly. (2) The source impedance is
too high for the ADC's sampling capacitor — buffer it with an op-amp follower and see whether the
error disappears. A reference error scales every reading by the same factor; an impedance error is
worse for higher readings.

---

## Chapter 53

**53.1** 360 ÷ 1.8 = **200 steps** per revolution; in half-step mode, **400**.

**53.2** Full step: `0x03`, `0x06`, `0x0C`, `0x09`. To reverse, walk the table **backwards** —
`0x09`, `0x0C`, `0x06`, `0x03`.

**53.3** `AND` with 3 keeps only the bottom two bits. Going forward, 3 + 1 = 4 = `0b100` → masked to
`0b00` = 0. Going backward from 0: 0 − 1 = `0xFFFF` = `...1111` → masked to `0b11` = **3** ✔

**53.4** A motor winding draws hundreds of milliamps; a 74LS-compatible output supplies about 2 mA
sinking. Use a **ULN2003** Darlington array, which handles 500 mA per channel and includes the
flyback diodes.

**53.5** They give the winding's collapsing magnetic field somewhere to dump its energy when the
drive switches off. Without them the inductive kick reaches hundreds of volts and destroys the
driver transistor.

**53.6** The motor **stalled** — the step rate was too high, or the load too heavy, for the rotor to
follow. The software does not know because a stepper has **no feedback**: it counts the steps it
commanded, not the steps that happened.

**53.7**

```asm
goto_position:                  ; target in AX
        mov  bx, [position]
        cmp  ax, bx
        je   .done
        jg   .forward
.back:  call step_back
        call step_delay
        cmp  ax, [position]
        jne  .back
        ret
.forward:
        call step_forward
        call step_delay
        cmp  ax, [position]
        jne  .forward
.done:  ret
```

**53.8** PWM switches the supply fully on and fully off rapidly; the motor's inertia and inductance
average it. A series resistor drops the excess voltage as **heat**, wasting the same power the motor
is not using; a switch that is either fully on (near-zero voltage drop) or fully off (zero current)
dissipates almost nothing.

**53.9** Compare a 4-bit counter with a 4-bit duty value. It buys a **16× shorter period** for the
same loop time — so a higher PWM frequency, which matters for audible whine — at the cost of coarser
control.

**53.10** (1) The processor writes once when the value changes rather than refreshing continuously.
(2) It scans and **debounces** the keyboard itself, with an 8-key FIFO. (3) It drives up to 16
digits, against 4 from one 8255's worth of pins.

**53.11** 16 digits right entry (`11`), encoded scan with N-key rollover (`010`):
`000 11 010` = **`0x1A`**.

**53.12** 3,000,000 ÷ 100,000 = **30**. Command = `0x20 | 30` = `0x20 | 0x1E` = **`0x3E`**.

**53.13** The internal scan and debounce timing is wrong. Too fast and keys register two or three
times; too slow and the display flickers or keys are missed.

**53.14** Split `AX` into eight digits by repeated division by 10, writing the segment patterns into
the buffer from the right; fill the remaining left-hand positions with the blank pattern `0x00`;
then send all eight with one auto-incrementing `0x90` command.

**53.15** **2-key lockout** ignores a second key pressed while the first is held — right for a
numeric keypad, where simultaneous presses are always errors. **N-key rollover** accepts them all in
order — right for a typewriter keyboard, where fast typists do press the next key before releasing
the last.

---

## Chapter 54

**54.1** Transistor budget. The 8087 has **45,000 transistors** — more than the 8086's 29,000 — and
putting both on one die was not possible in 1980. The 80486DX was the first x86 to integrate it.

**54.2** **Maximum mode.** The 8087 needs the `QS1`/`QS0` queue-status pins to shadow the 8086's
instruction queue, and `RQ/GT0#` to request the bus. Neither exists in minimum mode.

**54.3** (1) It watches `QS1`/`QS0` and maintains its own copy of the instruction queue. (2) When it
sees a byte leave the queue it decodes it in step with the 8086. (3) If the opcode is `D8`–`DF` —
an `ESC` — it knows the 8086 is executing one of its instructions.

**54.4** It computes the **effective address**, performs a memory read at that address, and
**discards** the result. The point is that the address appears on the bus, where the 8087 captures
it.

**54.5** `BUSY` connects to the 8086's **`TEST#`** pin, and the **`WAIT`** instruction reads it.

**54.6** Because the eight registers are a **stack**: `ST0` is whatever the top currently is, and a
push renames everything one place down.

**54.7**

```asm
        fld   qword [a]
        fadd  qword [b]         ; ST0 = a + b
        fld   qword [c]
        fsub  qword [d]         ; ST0 = c - d, ST1 = a + b
        fdivp st1, st0          ; ST0 = (a+b) / (c-d)
```

**54.8** `FSUB` computes `ST0 − src`; `FSUBR` computes `src − ST0`. With the operands already on the
stack in the wrong order, `FSUBR` saves an `FXCH` — two instructions become one.

**54.9** `FST` stores `ST0` and **leaves it** on the stack; `FSTP` stores and **pops**. Using `FST`
in a loop pushes a new value each iteration without removing the old one, so after eight iterations
the stack overflows and the invalid-operation exception is raised.

**54.10** 1.0: sign 0, exponent 127 = `01111111`, mantissa 0 → `0 01111111 00000000000000000000000`
= **`0x3F800000`**. −2.5 = −1.01₂ × 2¹: sign 1, exponent 128 = `10000000`, mantissa `0100…` →
**`0xC0200000`**.

**54.11** Because a normalised binary mantissa always begins with 1, so storing it would waste a bit.
Omitting it buys **one extra bit of precision** for free.

**54.12**

```asm
        fcom  qword [y]
        fstsw word [status]
        fwait
        mov   ax, [status]
        sahf
        ja    .bigger
```

**54.13** Because `FSTSW` is executed by the 8087 asynchronously — the 8086 may read the memory
location before the coprocessor has written it. `FWAIT` blocks until the store has completed.

**54.14**

```asm
        fstcw word [cw]
        fwait
        mov   ax, [cw]
        and   ax, 0xFFF7        ; clear bit 3 — unmask overflow
        mov   [cw], ax
        fldcw word [cw]
```

**54.15** Because the normal forms carry a `WAIT` prefix, and with no coprocessor fitted `TEST#` may
float high — so the `WAIT` never completes and the detection routine hangs for ever. `FNINIT` and
`FNSTCW` omit the prefix.

**54.16** `fldln2` / `fld qword [x]` / `fyl2x`. `FYL2X` computes `ST1 × log₂(ST0)` and pops, so
`ln(2)` must be pushed **first** (ending up in `ST1`) and `x` second (in `ST0`).

---

## Chapter 55

Chapter 55 has no exercises — except §10, which asks you to account for the 28-byte `hello.com`
completely from memory. If you can, you understand the 8086. Chapters 34 and 35 hold the answers.

---

[← Appendix G](G-glossary.md) · [Contents](README.md)
