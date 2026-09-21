# Chapter 17 — I/O interfacing

[← Memory interfacing](16-memory-interfacing.md) · [Contents](README.md) · [Next: 8088 vs 8086 →](18-8088-differences.md)

---

## Goal

Cover the 8086's separate I/O address space: the `IN` and `OUT` instructions, how ports are decoded,
why 8-bit peripherals live at even addresses, and how the whole thing compares with memory-mapped
I/O. Then map the IBM PC's port space, because every DOS program you write will use it.

---

## 1. Two ways to reach a device

### 1.1 Isolated (port-mapped) I/O

The 8086 has a **separate 64 KiB address space** for I/O, reached only by `IN` and `OUT`. A bus cycle
to this space is identical to a memory cycle except that **`M/IO#` is low**.

```asm
        in   al, 0x60           ; read port 0x60 into AL
        out  0x20, al           ; write AL to port 0x20
        mov  dx, 0x03F8
        in   al, dx             ; read the port whose number is in DX
        out  dx, ax             ; write a 16-bit value to a port pair
```

### 1.2 Memory-mapped I/O

Put the device in the *memory* address space, so ordinary memory instructions reach it.

```asm
        mov  ax, 0xB800
        mov  es, ax
        mov  word [es:0], 0x1F41    ; write 'A' white-on-blue to the screen
```

### 1.3 The comparison

| | Isolated I/O | Memory-mapped I/O |
|---|---|---|
| Address space used | separate 64 KiB | eats into the 1 MiB |
| Instructions | `IN`, `OUT` only | every memory instruction |
| Addressing modes | immediate byte, or `DX` | all of them |
| Operand size | byte or word via `AL`/`AX` only | any register |
| Can you `ADD` to a register? | no — you must `IN`, modify, `OUT` | yes, directly |
| String instructions | no (`INS`/`OUTS` are 80186) | yes — `REP MOVSW` to a framebuffer |
| Decoding | must include `M/IO# = 0` | one fewer signal |
| Protection (on later CPUs) | `IOPL` in protected mode | page-table based |

**The rule of thumb.** Registers that you poke occasionally — a timer's counter, a UART's status —
belong in I/O space. Buffers that you move data through in bulk — a framebuffer, a disk sector buffer
— belong in memory space, so that `REP MOVSW` can be used.

The IBM PC did exactly that: all the controller chips in I/O space, the video buffer memory-mapped.

---

## 2. The `IN` and `OUT` instructions in detail

### 2.1 The four forms

| Instruction | Opcode | Port range | Size | Bytes |
|-------------|--------|-----------|------|-------|
| `IN AL, imm8` | `E4 ib` | 0–255 | byte | 2 |
| `IN AX, imm8` | `E5 ib` | 0–255 | word | 2 |
| `IN AL, DX` | `EC` | 0–65535 | byte | 1 |
| `IN AX, DX` | `ED` | 0–65535 | word | 1 |
| `OUT imm8, AL` | `E6 ib` | 0–255 | byte | 2 |
| `OUT imm8, AX` | `E7 ib` | 0–255 | word | 2 |
| `OUT DX, AL` | `EE` | 0–65535 | byte | 1 |
| `OUT DX, AX` | `EF` | 0–65535 | word | 1 |

Three restrictions that trip everyone up:

**Only `AL` and `AX`.** There is no `IN BL, DX`. If you need the value elsewhere, move it afterwards.

**The immediate form reaches only ports 0–255.** `in al, 0x3F8` will not assemble as an immediate
form; you must use `DX`:

```asm
        mov  dx, 0x03F8
        in   al, dx
```

NASM will produce an error (`operand 2 out of range`) rather than silently truncating, which is
helpful.

**A 16-bit `IN`/`OUT` uses two consecutive ports.** `out dx, ax` with `DX = 0x40` writes `AL` to port
`0x40` and `AH` to port `0x41`, in one bus cycle if both are on the same 16-bit device, or in two
cycles otherwise (same bank logic as Chapter 10). Most 8-bit peripherals are wired to `D7–D0` only,
so a word `OUT` to them writes the low byte to the port and the high byte into nothing. Use byte
forms with 8-bit peripherals.

### 2.2 Timing

| Instruction | 8086 clocks |
|-------------|-------------|
| `IN AL, imm8` / `IN AX, imm8` | 10 / 14 |
| `IN AL, DX` / `IN AX, DX` | 8 / 12 |
| `OUT imm8, AL` / `AX` | 10 / 14 |
| `OUT DX, AL` / `AX` | 8 / 12 |

The `DX` form is *faster*, because it is one byte shorter and the immediate does not need fetching.
So in a tight loop, load `DX` once outside and use the register form.

`IN` and `OUT` affect **no flags**.

---

## 3. Why 8-bit peripherals live at even addresses

A peripheral like the 8255 has eight data pins. In a 16-bit 8086 system it must connect to either
`D7–D0` or `D15–D8`. The near-universal convention is `D7–D0`, the **even** bank.

A device on `D7–D0` responds only when `A0 = 0` — that is, at even addresses. So its four internal
registers, which it selects with its own `A1` and `A0` pins, must appear at:

```
   system port 0x00   ->  device register 0     (device A1 A0 = 00)
   system port 0x02   ->  device register 1     (device A1 A0 = 01)
   system port 0x04   ->  device register 2     (device A1 A0 = 10)
   system port 0x06   ->  device register 3     (device A1 A0 = 11)
```

and the wiring is:

```
   device A0  ◄──  system A1
   device A1  ◄──  system A2
```

**Every system address line shifts down by one on its way to the device**, exactly as it does for
memory chips (Chapter 16 §3). Odd port numbers `0x01`, `0x03`, `0x05` simply do not reach this
device.

### 3.1 On an 8088 this does not happen

The 8088 has an 8-bit bus and no banks, so peripherals sit at consecutive addresses:
`0x00`, `0x01`, `0x02`, `0x03`. That is why the IBM PC's port map is dense — the 8259A at
`0x20`–`0x21`, the 8253 at `0x40`–`0x43` — while an 8086 trainer board's is spread out.

**This matters when you port code.** A program written for a PC that writes to port `0x43` will write
to the wrong register on a 16-bit 8086 board where the timer is at `0x46`.

---

## 4. Decoding I/O addresses

### 4.1 The simple case

Put an 8255 at ports `0x00`–`0x06` (even only):

```
   Which lines are fixed?  A15-A3 must all be 0.
   Which vary?             A2, A1 select the register (via device A1, A0)
   Which is ignored?       A0 — but it must be 0, so include it
```

```
   8255_CS#  =  NOT( NOT M/IO# · NOT A15 · NOT A14 · ... · NOT A3 · NOT A0 )
```

That is a lot of inverters. In practice, small systems decode only the low eight or so lines and
accept aliasing (§5).

A practical version using a 74LS138:

```
   74LS138:  C B A   ◄─  A5 A4 A3
             G1      ◄─  NOT M/IO#        (high when M/IO# is low, i.e. an I/O cycle)
             G2A#    ◄─  A7
             G2B#    ◄─  A6

   Y0 -> ports 0x00-0x07     -> 8255
   Y1 -> ports 0x08-0x0F     -> 8253
   Y2 -> ports 0x10-0x17     -> 8259A
   Y3 -> ports 0x18-0x1F     -> 8251A
   ...
```

One chip gives eight device slots of eight ports each. `A15`–`A8` are ignored, so each device also
appears at `0x0100`, `0x0200`, … — aliasing, and acceptable on a board that will not grow.

### 4.2 Producing `IOR#` and `IOW#`

In minimum mode:

```
   IOR#  =  RD#  OR  M/IO#
   IOW#  =  WR#  OR  M/IO#
```

Two OR gates. In maximum mode the 8288 gives you `IORC#` and `IOWC#` directly.

### 4.3 Complete 8255 connection

```
   8255A                       from
   ──────────────────────────  ────────────────────
   D7–D0        ◄──►           D7–D0  (low half only)
   A1           ◄──            system A2
   A0           ◄──            system A1
   CS#          ◄──            74LS138 Y0#
   RD#          ◄──            IOR#
   WR#          ◄──            IOW#
   RESET        ◄──            8284A RESET
   PA7–PA0      ──►            8 LEDs (through resistors)
   PB7–PB0      ◄──            8 switches (with pull-ups)
   PC7–PC0      ◄──►           spare
```

Chapter 47 programs this.

---

## 5. Aliasing, and how to detect it from software

If only `A7`–`A0` are decoded, a device at port `0x40` also answers at `0x140`, `0x240`, … `0xFF40`
— 256 aliases.

### 5.1 Is it a problem?

Usually not, until you add a second device in a range you thought was free. Then two devices answer
the same port and you get bus contention on reads.

### 5.2 Detecting it

Write a value to a port with a readable register, then read back at a suspected alias:

```asm
; probe.asm — test whether ports 0x40 and 0x140 are the same device
; Uses the 8253 counter 0 latch, which is readable. Adapt to your hardware.
        org  0x100

        mov  dx, 0x43           ; 8253 control port
        mov  al, 0x36           ; counter 0, LSB then MSB, mode 3, binary
        out  dx, al

        mov  dx, 0x40           ; counter 0 data port
        mov  al, 0x34
        out  dx, al             ; LSB
        mov  al, 0x12
        out  dx, al             ; MSB -> counter 0 loaded with 0x1234

        mov  dx, 0x143          ; suspected alias of the control port
        mov  al, 0x00           ; latch counter 0
        out  dx, al

        mov  dx, 0x140          ; suspected alias of the data port
        in   al, dx
        mov  bl, al
        in   al, dx
        mov  bh, al             ; BX = the latched count

        ; If BX is close to 0x1234 (counting down), 0x140 IS 0x40.
        ...
```

The general technique — write a distinctive value here, look for it there — works for any pair of
addresses and for memory as well as I/O.

---

## 6. The IBM PC's I/O map

Worth knowing, because DOS programs in Part IV use these directly.

| Ports | Device |
|-------|--------|
| `0x000`–`0x00F` | DMA controller 1 (8237A) |
| `0x020`–`0x021` | **Interrupt controller 1 (8259A)** |
| `0x040`–`0x043` | **Timer (8253/8254)** |
| `0x060`–`0x063` | **Keyboard controller (8255 on the PC/XT, 8042 on the AT)** |
| `0x070`–`0x071` | CMOS RAM and real-time clock (AT) |
| `0x080`–`0x08F` | DMA page registers |
| `0x0A0`–`0x0A1` | Interrupt controller 2 (AT) |
| `0x0C0`–`0x0DF` | DMA controller 2 (AT) |
| `0x1F0`–`0x1F7` | Hard disk (AT) |
| `0x200`–`0x20F` | Game port |
| `0x278`–`0x27F` | Parallel port LPT2 |
| `0x2F8`–`0x2FF` | Serial port COM2 |
| `0x378`–`0x37F` | **Parallel port LPT1** |
| `0x3B0`–`0x3BF` | MDA video |
| `0x3C0`–`0x3CF` | EGA/VGA video |
| `0x3D0`–`0x3DF` | **CGA video** |
| `0x3F0`–`0x3F7` | Floppy disk controller |
| `0x3F8`–`0x3FF` | **Serial port COM1** |

The six in bold are the ones you will actually touch.

### 6.1 Three you will use in Part IV

**Port `0x60` — the keyboard scan code.** Reading it gives the last scan code. Chapter 37 §5 and
Chapter 53.

**Ports `0x40`–`0x43` — the timer.** Reprogramming counter 0 changes the system tick rate; counter 2
drives the speaker. Chapter 48.

**Port `0x61` — the speaker gate.** Bits 0 and 1 enable the timer's output to the speaker:

```asm
; turn the PC speaker on
        in   al, 0x61
        or   al, 0x03           ; set bits 0 and 1
        out  0x61, al
```

### 6.2 A warning about DOSBox

DOSBox emulates the 8253, 8259A and keyboard controller at these addresses, so programs that use
them mostly work. It does **not** emulate an 8255 at a trainer board's addresses, an ADC, or a
stepper motor driver. Each Part V chapter says whether its program runs under DOSBox.

---

## 7. Timing quirks of I/O cycles

**The 8088 inserts one automatic wait state into every I/O cycle.** The 8086 does not.

The reason is historical: the IBM PC's peripherals were slow 8-bit parts designed for 8080-era
timing, and IBM needed the margin. Since the 8088 is what the PC used, PC software's sense of "how
long an `OUT` takes" is calibrated to a 5-clock-minimum I/O cycle.

**Some peripherals need recovery time between accesses.** The 8259A in particular requires a delay
between consecutive writes to the same chip — its internal state machine needs time. The standard
idiom on a PC is:

```asm
        out  0x20, al
        jmp  $+2                ; a 2-byte jump to the next instruction
        jmp  $+2                ; flushes the queue and burns ~15 clocks
        out  0x21, al
```

`jmp $+2` jumps to the instruction immediately following it — a no-op in effect, but one that
flushes the prefetch queue and therefore takes about 15 clocks instead of 3. Two of them give roughly
6 µs at 5 MHz, which is enough for an 8259A. You will see this idiom constantly in period code, and
now you know what it is for.

A cleaner modern equivalent is `out 0x80, al` — writing to the PC's unused POST diagnostic port,
which takes a full bus cycle and harms nothing.

---

## 8. A complete worked example — LED and switch board

**Requirement.** Eight LEDs and eight switches on an 8086 minimum-mode board, at I/O ports starting
at `0x00`.

**Chip.** One 8255A. Port A → LEDs (output), Port B → switches (input).

**Decoding.**

```
   74LS138:  C B A   ◄─  A5 A4 A3
             G1      ◄─  NOT M/IO#
             G2A#    ◄─  A7
             G2B#    ◄─  A6
   Y0# -> 8255 CS#          covers system ports 0x00-0x07
```

**Register addresses.** With device `A1 A0` ← system `A2 A1`:

| Device register | Device `A1 A0` | System port |
|-----------------|----------------|-------------|
| Port A | 00 | `0x00` |
| Port B | 01 | `0x02` |
| Port C | 10 | `0x04` |
| Control | 11 | `0x06` |

**Software.**

```asm
; leds.asm — mirror eight switches onto eight LEDs, forever.
; 8255 at ports 0x00 (A), 0x02 (B), 0x04 (C), 0x06 (control).
        org  0x100

PORTA   equ  0x00
PORTB   equ  0x02
CTRL    equ  0x06

start:
        ; Control word 0x82:
        ;   bit 7 = 1   mode-set flag
        ;   bits 6,5 = 00  port A mode 0
        ;   bit 4 = 0   port A output
        ;   bit 3 = 0   port C upper output
        ;   bit 2 = 0   port B mode 0
        ;   bit 1 = 1   port B INPUT
        ;   bit 0 = 0   port C lower output
        mov  al, 0x82
        out  CTRL, al

.loop:
        in   al, PORTB          ; read the switches
        not  al                 ; switches pull low when closed
        out  PORTA, al          ; drive the LEDs

        mov  ah, 0x0B           ; DOS: check for a keypress
        int  0x21
        or   al, al
        jz   .loop              ; no key -> keep going

        mov  ax, 0x4C00
        int  0x21
```

Chapter 47 derives that control word bit by bit and extends the program.

**Does it run under DOSBox?** No — there is no 8255 at those ports. It runs on a trainer board.

---

## 9. Summary

```
  the 8086 has a SEPARATE 64 KiB I/O space, selected by M/IO# = 0
  reached only by IN and OUT, and only through AL or AX

  IN  AL, imm8   ports 0-255     2 bytes, 10 clocks
  IN  AL, DX     ports 0-65535   1 byte,   8 clocks   <- prefer this in loops
  OUT imm8, AL / OUT DX, AL      likewise
  IN and OUT change NO flags

  8-bit peripherals sit on D7-D0 -> they answer only EVEN port numbers
  device A1,A0 connect to system A2,A1 — everything shifts down one

  IOR# = RD# OR M/IO#          IOW# = WR# OR M/IO#
  (or MRDC/IORC directly from an 8288 in maximum mode)

  partial decoding gives aliases; detect them by writing here and reading there

  memory-mapped I/O when you need bulk transfer (REP MOVSW to a framebuffer)
  isolated I/O for control registers you poke occasionally
```

---

## Exercises

**17.1** Write the instruction that reads port `0x70` into `AL`. Now write the instruction sequence
that reads port `0x3CE`. Why are they different?

**17.2** How many bytes and how many clocks does `OUT DX, AL` take, compared with `OUT 0x20, AL`?
Which would you use inside a loop, and why?

**17.3** An 8255 is decoded so that its `CS#` covers system ports `0x40`–`0x47`, with device `A1 A0`
driven from system `A2 A1`. Give the system port number for each of the four 8255 registers.

**17.4** Why does an 8-bit peripheral on an 8086 answer only even port addresses? What is different
on an 8088?

**17.5** Write the two gate equations that produce `IOR#` and `IOW#` from a minimum-mode 8086.

**17.6** A 74LS138 has `C B A` ← `A5 A4 A3`, `G1` ← inverted `M/IO#`, `G2A#` ← `A7`, `G2B#` ← `A6`.
Which output is asserted by `OUT 0x1A, AL`? Which port range does that output cover?

**17.7** In the same design, `A15`–`A8` are not decoded. List three port numbers other than `0x1A`
that would reach the same device.

**17.8** Give one reason to put a device in memory space rather than I/O space, and one reason for
the opposite.

**17.9** Explain what `jmp $+2` does, why it takes about 15 clocks rather than 3, and why period
code puts two of them between consecutive `OUT`s to an 8259A.

**17.10** `out dx, ax` is executed with `DX = 0x42`. Which ports receive which bytes? What happens if
the device at those ports is an 8-bit peripheral wired only to `D7–D0`?

**17.11** Write a short program that reads the PC keyboard scan code from port `0x60` and prints it
as two hex digits. (Assume the conversion routine from Chapter 43 exists.)

**17.12** Why does the 8088 insert an automatic wait state into I/O cycles when the 8086 does not?

Answers in [Appendix H](H-exercise-solutions.md#chapter-17).

---

[← Memory interfacing](16-memory-interfacing.md) · [Contents](README.md) · [Next: 8088 vs 8086 →](18-8088-differences.md)
