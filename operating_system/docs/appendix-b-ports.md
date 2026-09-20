# Appendix B — I/O port map

[Contents](README.md)

---

Every port this project touches, and the ones you will meet next. Addresses fixed by IBM in 1981 and
unchanged since.

---

## Summary

| Ports | Device | Chapter |
|---|---|---|
| `0x20`, `0x21` | Master PIC | 17 |
| `0x40`–`0x43` | PIT (timer) | 18 |
| `0x60`, `0x64` | 8042 keyboard controller | 7, 19 |
| `0x70`, `0x71` | CMOS / RTC | — |
| `0x80` | POST diagnostic — used as an I/O delay | 17 |
| `0x92` | System Control Port A (A20, fast reset) | 7 |
| `0xA0`, `0xA1` | Slave PIC | 17 |
| `0x170`–`0x177`, `0x376` | ATA secondary channel | 37 |
| `0x1F0`–`0x1F7`, `0x3F6` | ATA primary channel | 37 |
| `0x2F8`–`0x2FF` | COM2 | 13 |
| `0x3D4`, `0x3D5` | VGA CRT controller (colour) | 12 |
| `0x3B4`, `0x3B5` | VGA CRT controller (mono) | 12 |
| `0x3C0`–`0x3CF` | VGA attribute, sequencer, graphics | 12 (ex. 10.7) |
| `0x3F8`–`0x3FF` | COM1 | 13 |
| `0xCF8`, `0xCFC` | PCI configuration | 48 |

---

## 0x20 / 0xA0 — PIC command

**Write:**

| Value | Meaning |
|---|---|
| `0x11` | ICW1: begin initialisation, expect ICW4 |
| `0x20` | EOI — end of interrupt |
| `0x0A` | OCW3: next read returns the IRR |
| `0x0B` | OCW3: next read returns the ISR |
| `0x60 \| n` | specific EOI for IRQ n |

**Read** (after OCW3): the Interrupt Request Register (pending) or the In-Service Register (being
handled).

The ISR is how a spurious IRQ 7 is detected: a real interrupt sets its bit, a spurious one does not
(Ch. 17, §4).

## 0x21 / 0xA1 — PIC data

Meaning depends on state:

- After ICW1: ICW2 (vector offset), then ICW3 (cascade wiring), then ICW4 (mode).
- Otherwise: the **interrupt mask**. Bit set = line disabled.

```c
    outb(PIC1_DATA, 0xFF);   /* mask everything */
```

ICW3 is the 8259 in one detail: the master takes a **bitmask** (`0x04` = a slave on line 2), the
slave takes a **number** (`0x02` = cascade identity 2).

---

## 0x40–0x43 — PIT

| Port | Channel |
|---|---|
| `0x40` | 0 — wired to IRQ 0 |
| `0x41` | 1 — DRAM refresh on the original PC |
| `0x42` | 2 — the PC speaker |
| `0x43` | command register (write only) |

**Command byte:**

```
    bits 7-6   channel (00, 01, 10)
    bits 5-4   access: 01 = low byte, 10 = high byte, 11 = low then high
    bits 3-1   mode: 000 = one-shot, 011 = square wave
    bit  0     0 = binary, 1 = BCD
```

`0x36` = channel 0, low-then-high, mode 3, binary.

Base frequency **1,193,182 Hz** — one third of the NTSC colour burst (Ch. 18, §2.1).

```c
    uint32_t divisor = PIT_FREQUENCY / frequency;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
```

**The speaker:** program channel 2 with mode 3, then set bits 0 and 1 of port `0x61` to connect it.

---

## 0x60 / 0x64 — 8042 keyboard controller

| Port | Read | Write |
|---|---|---|
| `0x60` | the data byte | a byte to the keyboard |
| `0x64` | status | a command to the controller |

**Status bits:**

```
    bit 0   output buffer full — the controller has a byte for us
    bit 1   input buffer full  — it is still reading our last write
    bit 2   system flag
    bit 6   timeout
    bit 7   parity error
```

**Controller commands (to 0x64):**

| Command | Effect |
|---|---|
| `0xAD` / `0xAE` | disable / enable the first PS/2 port |
| `0xA7` / `0xA8` | disable / enable the second port (mouse) |
| `0xD0` | read the output port |
| `0xD1` | write the output port |
| `0xFE` | **pulse the CPU reset line** |

**Keyboard commands (to 0x60):**

| Command | Effect |
|---|---|
| `0xED` | set LEDs — next byte is a bitmask |
| `0xF3` | set typematic rate |
| `0xF4` | enable scanning |
| `0xFF` | reset |

Responses: `0xFA` = ACK, `0xFE` = resend.

**Output port bit 1 is the A20 line** (Ch. 7, §1.4) and command `0xFE` reboots the machine
(Ch. 19, §8). Two system-level functions on the keyboard controller, because in 1984 it was the chip
with pins to spare.

> ⚠️ **Read port `0x60` in every path of the keyboard handler.** Not reading it means the controller
> never raises IRQ 1 again, and the keyboard dies after one keystroke.

---

## 0x80 — POST diagnostic

Write-only, ignored after boot, and one I/O bus cycle takes about a microsecond regardless of CPU
speed.

```c
static ALWAYS_INLINE void io_wait(void)
{
    outb(0x80, 0);
}
```

Used to give the 8259 time between initialisation words (Ch. 17, §2.2). A hack, what Linux does, and
there is no better portable option.

On some hardware a POST card plugged into an ISA or LPC slot displays the last byte written as two
hex digits — the fallback when there is no serial port (Ch. 47, §9.3).

---

## 0x92 — System Control Port A

```
    bit 0   FAST RESET -- writing 1 reboots instantly
    bit 1   A20 enable
```

```c
    in al, 0x92
    or al, 0x02                 ; A20 on
    and al, 0xFE                ; mask off FAST RESET
    out 0x92, al
```

> ⚠️ **Always mask bit 0.** A naive `mov al, 0x02; out 0x92, al` is fine; a read-modify-write that
> leaves bit 0 set is a reboot loop.

---

## 0x1F0–0x1F7 / 0x3F6 — ATA primary

| Offset | Read | Write |
|---|---|---|
| 0 | data | data |
| 1 | error | features |
| 2 | sector count | sector count |
| 3 | LBA 0–7 | LBA 0–7 |
| 4 | LBA 8–15 | LBA 8–15 |
| 5 | LBA 16–23 | LBA 16–23 |
| 6 | drive / LBA 24–27 | same |
| 7 | **status** | **command** |

`0x3F6` is the control/alternate-status register. **Reading `0x3F6` does not clear the pending
interrupt; reading `0x1F7` does** — which is why the 400 ns delay reads the alternate
(Ch. 37, §3.1).

**Status bits:**

```
    bit 7   BSY  -- every other bit is meaningless while set
    bit 6   DRDY
    bit 5   DF   -- device fault
    bit 3   DRQ  -- a sector is waiting
    bit 0   ERR
```

**Drive register:** `0xE0 | (slave << 4) | (lba >> 24 & 0x0F)`. Bits 7 and 5 are "obsolete" and must
still be set.

**Commands:** `0x20` read PIO, `0x30` write PIO, `0xE7` flush cache, `0xEC` IDENTIFY.

**Control register bit 1** is `nIEN` — set it to stop the drive asserting IRQ 14.

Secondary channel: `0x170`–`0x177`, control `0x376`, IRQ 15.

---

## 0x3F8–0x3FF — COM1

| Offset | DLAB=0 | DLAB=1 |
|---|---|---|
| 0 | data (R: receive, W: transmit) | divisor low |
| 1 | interrupt enable | divisor high |
| 2 | R: interrupt ID, W: FIFO control | |
| 3 | line control (includes DLAB) | |
| 4 | modem control | |
| 5 | line status | |
| 6 | modem status | |
| 7 | scratch | |

**DLAB** is bit 7 of the line control register. Setting it re-points offsets 0 and 1 at the baud
divisor — bank switching, and the most confusing thing about the 16550 (Ch. 13, §3).

**Line status:** bit 0 = data ready, bit 5 = transmitter empty (THRE), bit 6 = transmitter idle.

**Modem control:** bit 0 DTR, bit 1 RTS, bit 3 **OUT2**, bit 4 loopback.

> **OUT2 is wired to the interrupt enable gate on a PC.** Without it, IRQ 4 never reaches the PIC no
> matter what the IER says (Ch. 13, §4).

**Baud** = 115200 / divisor. Divisor 1 = 115200, 12 = 9600.

COM2: `0x2F8`, IRQ 3. COM3: `0x3E8`. COM4: `0x2E8`.

---

## 0x3D4 / 0x3D5 — VGA CRT controller

An index/data pair: write the register number to `0x3D4`, read or write its value at `0x3D5`.

| Index | Register |
|---|---|
| `0x0A` | cursor start scanline + bit 5 = disable |
| `0x0B` | cursor end scanline |
| `0x0C` | start address high — hardware scrolling |
| `0x0D` | start address low |
| `0x0E` | cursor position high |
| `0x0F` | cursor position low |

The cursor position is a **linear cell index** (`row * 80 + col`), not a coordinate pair.

Monochrome adapters use `0x3B4`/`0x3B5`; bit 0 of the BIOS equipment word at `0x0410` says which.
Every machine since 1990 is colour.

**Framebuffer:** physical `0xB8000` for text mode, `0xA0000` for graphics. Not a port — memory-mapped
I/O (Ch. 12, §1).

---

## 0x70 / 0x71 — CMOS and RTC

Another index/data pair. Not used in this project and worth knowing.

```c
    outb(0x70, reg);        /* bit 7 also masks NMI */
    uint8_t v = inb(0x71);
```

| Register | Contents |
|---|---|
| `0x00`–`0x09` | seconds, minutes, hours, day, month, year (BCD by default) |
| `0x0A`–`0x0D` | status registers A–D |
| `0x0E`+ | 114 bytes of battery-backed RAM |

Register B bit 2 selects binary rather than BCD; bit 1 selects 24-hour. Register C must be read after
an RTC interrupt or no further interrupts arrive — the same trap as reading port `0x60`.

The RTC is on IRQ 8 and can generate a periodic interrupt from 2 Hz to 8 kHz, which makes it a
better timer than the PIT for high-resolution work.

---

## 0xCF8 / 0xCFC — PCI configuration

The gateway to every modern device (Ch. 48, §2.8).

```c
static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus << 16)
                  | ((uint32_t)dev << 11)
                  | ((uint32_t)fn  << 8)
                  | (off & 0xFC);

    outl(0xCF8, addr);
    return inl(0xCFC);
}
```

Each function has 256 bytes of configuration space:

| Offset | Field |
|---|---|
| `0x00` | vendor ID (`0xFFFF` = no device) |
| `0x02` | device ID |
| `0x08` | revision, then class/subclass/prog-IF |
| `0x0E` | header type — bit 7 = multifunction |
| `0x10`–`0x24` | six base address registers |
| `0x3C` | interrupt line |

Eighty lines to enumerate every device on the machine, and it is the single best return on effort
available after this book.

---

## Unused but present

| Ports | Device |
|---|---|
| `0x00`–`0x0F`, `0xC0`–`0xDF` | DMA controllers (8237) |
| `0x61` | keyboard control port B — speaker gate, bits 0–1 |
| `0x278`, `0x378`, `0x3BC` | parallel ports |
| `0x3F0`–`0x3F7` | floppy controller |
| `0xF0`–`0xFF` | coprocessor |

---

[Contents](README.md)
