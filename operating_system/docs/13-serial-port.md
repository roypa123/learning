# Chapter 13 — The serial port

[← VGA driver](12-vga-driver.md) · [Contents](README.md) · [Next: printf from nothing →](14-printf.md)

---

## Goal

Write the driver whose output survives a crash. Nine port writes to configure a 16550 UART, a
loopback self-test, and a transmit path that works from an interrupt handler, from `panic()`, and
before anything else in the kernel exists.

This chapter is short and it is the highest-value chapter in Part II. A kernel without a serial log
is a kernel you debug by guessing.

---

## 1. Why, in 2026, for a port no laptop has

Four reasons, and every one of them has saved a debugging session.

**It works before everything else.** No paging, no interrupts, no memory allocator, no console. You
can call `serial_putc()` from the third instruction of the kernel. That is why
[`main.c`](../nimbus/kernel/main.c)'s very first line is `serial_init(COM1)`:

```c
    /* ---- 1. The log, before anything else -----------------------------------
     * Nothing depends on this and everything benefits from it. If the kernel
     * dies on line 3, the serial log is the only evidence that will exist.
     */
    serial_init(COM1);
    serial_puts("\n\n=== Nimbus starting ===\n");
```

**It survives.** When the machine triple-faults and reboots, the screen is gone — but every byte you
sent is already in a file on the host. The half-written line at the end of that file is frequently
enough to find the instruction.

**It is a file.** `-serial file:serial.log` makes the kernel's output greppable, diffable, and
attachable to a question. Comparing a boot that worked with a boot that did not, line by line, is a
technique you will use constantly.

**It is bidirectional.** A debug shell over a wire, on a machine whose keyboard driver is the thing
you are debugging.

The screen is for the user. The log is for you.

---

## 2. What a UART is

Universal Asynchronous Receiver/Transmitter. It converts bytes to a serial bit stream and back.

*Asynchronous* means there is no clock signal on the wire — both ends agree on a bit rate in advance
and sample accordingly. That is why both ends must be configured identically, and why a mismatched
baud rate produces garbage rather than nothing.

Each byte goes out as: a start bit (low), 8 data bits, an optional parity bit, and 1 or 2 stop bits
(high). The "8N1" that every terminal program asks for is 8 data bits, No parity, 1 stop bit.

The chip is a 16550A, or an emulation of one. Eight registers at consecutive port addresses. On a PC
the first serial port is at `0x3F8`.

---

## 3. The registers, and the one that changes meaning

```c
#define UART_DATA        0   /* R: receive  W: transmit          (DLAB=0) */
#define UART_IER         1   /* interrupt enable                 (DLAB=0) */
#define UART_DIVISOR_LO  0   /* baud divisor low byte            (DLAB=1) */
#define UART_DIVISOR_HI  1   /* baud divisor high byte           (DLAB=1) */
#define UART_FCR         2   /* W: FIFO control  R: interrupt ID           */
#define UART_LCR         3   /* line control: word length, parity, DLAB    */
#define UART_MCR         4   /* modem control: DTR, RTS, loopback          */
#define UART_LSR         5   /* line status: is the transmitter empty?     */
#define UART_MSR         6   /* modem status                                */
#define UART_SCRATCH     7   /* one byte of scratch RAM                     */
```

Offsets 0 and 1 mean **two different things** depending on bit 7 of the Line Control Register — the
Divisor Latch Access Bit.

With DLAB clear, offset 0 is the data register and offset 1 is the interrupt enable register. With
DLAB set, they are the two halves of the baud rate divisor.

This is bank switching, and it is why `serial_init` appears to write to the same port twice for no
reason. It is also the single most confusing thing about the 16550, and the cause of the classic bug
where a driver sets the baud rate and then wonders why interrupts are enabled — it wrote the divisor
high byte into IER because DLAB was already clear.

---

## 4. Initialisation, nine writes in order

```c
bool serial_init(uint16_t port)
{
    serial_port = port;
    serial_ok   = false;

    outb(port + UART_IER, 0x00);            /* 1 */

    outb(port + UART_LCR, 0x80);            /* 2 */
    outb(port + UART_DIVISOR_LO, 0x01);     /* 3 */
    outb(port + UART_DIVISOR_HI, 0x00);
    outb(port + UART_LCR, 0x03);            /* 4 */

    outb(port + UART_FCR, 0xC7);            /* 5 */
    outb(port + UART_MCR, 0x0B);            /* 6 */

    outb(port + UART_MCR, 0x1E);            /* 7 -- loopback */
    outb(port + UART_DATA, 0xAE);

    if (inb(port + UART_DATA) != 0xAE) {
        serial_port = 0;
        return false;
    }

    outb(port + UART_MCR, 0x0F);            /* 8 */

    serial_ok = true;
    return true;
}
```

### Step 1: interrupts off

If a character arrives mid-setup and IRQ 4 fires into an IDT that does not exist yet, the machine
triple-faults during boot. `serial_init` runs before `idt_init`, so this write is not optional.

### Step 2: DLAB on

`0x80` sets bit 7 of the LCR and nothing else. The next two writes now go to the divisor.

### Step 3: the divisor

The UART's internal clock is 1.8432 MHz divided by 16, giving 115200 Hz. The baud rate is
115200 / divisor.

| Divisor | Baud |
|---|---|
| 1 | 115200 |
| 3 | 38400 |
| 12 | 9600 |
| 96 | 1200 |

Divisor 1, so 115200 baud — what QEMU and every terminal program default to. There is no reason to go
slower; we are not driving a modem.

### Step 4: DLAB off, and the line format

`0x03` is `00000011`: bits 0–1 are the word length (`11` = 8 bits), bit 2 is stop bits (0 = one),
bits 3–5 are parity (000 = none), and bit 7 — DLAB — is now clear.

8N1, and offsets 0 and 1 are back to being data and IER.

### Step 5: the FIFOs

`0xC7` enables the transmit and receive FIFOs, clears both, and sets the receive interrupt trigger
level to 14 bytes.

The FIFO is what makes a 16550 a 16550 rather than an 8250. Without it the CPU must collect every
received byte before the next one arrives — 87 microseconds at 115200 baud — or the byte is lost.
With a 16-byte FIFO the deadline becomes 1.4 milliseconds.

### Step 6: modem control, and the PC wiring quirk

`0x0B` is `00001011`: DTR (bit 0), RTS (bit 1), and **OUT2** (bit 3).

DTR and RTS tell the other end we are here and ready. OUT2 is the interesting one: the 16550 defines
it as a general-purpose output pin, and **on a PC it is wired to the interrupt enable gate**. Without
it, IRQ 4 never reaches the PIC no matter what the IER says.

That is a motherboard wiring detail, not a UART feature, and it is documented in about one sentence
of the IBM technical reference. It is also the reason a serial driver that looks completely correct
receives nothing by interrupt.

### Step 7: the self-test

```c
    outb(port + UART_MCR, 0x1E);          /* loopback on */
    outb(port + UART_DATA, 0xAE);

    if (inb(port + UART_DATA) != 0xAE) {
        serial_port = 0;
        return false;
    }
```

Bit 4 of the MCR puts the chip in loopback mode: the transmitter is wired to its own receiver. Send a
byte, see if it comes back.

If the port does not exist, reads return `0xFF` and the test fails cleanly. Without it,
`serial_putc` would spin forever waiting for a transmitter-empty bit that will never be set on a chip
that is not there — a hang during boot with no output, which is the worst possible failure mode and
exactly the one this driver exists to prevent.

`0xAE` is arbitrary. It is worth choosing something with alternating bits rather than `0x00` or
`0xFF`, so that a stuck-low or stuck-high bus also fails the test.

### Step 8: normal operation

`0x0F` is DTR, RTS, OUT1 and OUT2, with loopback off.

---

## 5. Transmitting

```c
static inline void serial_wait_tx(void)
{
    for (int spins = 0; spins < 100000; spins++)
        if (inb(serial_port + UART_LSR) & 0x20)
            return;
}

void serial_putc(char c)
{
    if (!serial_ok) return;

    if (c == '\n') {
        serial_wait_tx();
        outb(serial_port + UART_DATA, '\r');
    }

    serial_wait_tx();
    outb(serial_port + UART_DATA, (uint8_t)c);
}
```

Bit 5 of the Line Status Register is THRE — Transmitter Holding Register Empty. Spin until it is set,
then write the byte.

### 5.1 Why we spin, and accept it

A busy wait. At 115200 baud a byte takes 87 microseconds, which is an eternity — tens of thousands of
instructions.

The alternative is an interrupt-driven transmit queue: put bytes in a ring buffer, enable the
transmit-empty interrupt, and let the handler drain it. That is what a production driver does and it
is maybe sixty lines.

We do not, because of *where* this function has to work:

- from `panic()`, when the machine is in an unknown state and the scheduler may be the thing that
  broke;
- from an interrupt handler, where blocking is not an option and the queue's lock might be held;
- before the scheduler exists at all, which is most of `kmain`.

An interrupt-driven path cannot serve any of those. A spinning path serves all of them.

The cost is real: heavy logging slows the kernel measurably. That turns out to be a feature the first
time a race condition disappears when you add a `kprintf` — which tells you, immediately and for
free, that the bug is timing-dependent.

### 5.2 The bounded wait

```c
    for (int spins = 0; spins < 100000; spins++)
```

An unbounded `while (!(inb(...) & 0x20));` turns a missing or wedged UART into a hang. The self-test
in §4 should have caught a missing one, but "should have" is not "did" — a port that disappears
mid-run, or a QEMU serial backend whose pipe has closed, produces exactly this.

100,000 iterations at roughly one microsecond per I/O read is a tenth of a second. If the transmitter
is not empty by then, dropping the character is better than stopping the machine.

### 5.3 CRLF

```c
    if (c == '\n') {
        serial_wait_tx();
        outb(serial_port + UART_DATA, '\r');
    }
```

Chapter 10, §4: `\n` on Unix means "move down and return to column 0". A terminal emulator on the
other end of a serial line does not make that assumption — `\n` moves down, `\r` returns.

Without this translation the log comes out as a diagonal staircase:

```
=== Nimbus starting ===
                       gdt: 6 descriptors at c0104000
                                                     idt: 256 vectors
```

Every serial driver does this and almost none of them mention it.

---

## 6. Receiving

```c
int serial_getc_nonblock(void)
{
    if (!serial_ok) return -1;
    if (!(inb(serial_port + UART_LSR) & 0x01)) return -1;
    return (int)inb(serial_port + UART_DATA);
}
```

Bit 0 of the LSR is "data ready". Polling is fine for our purposes — nothing types at us over the
serial line in normal operation.

The interrupt-driven version is Exercise 13.5 and is about twenty lines once Chapter 17 exists: set
bit 0 of IER, register a handler on IRQ 4, and push into a ring buffer exactly as the keyboard driver
does.

The reason to do it: a serial console. A kernel you can drive over a wire is a kernel you can debug
when the keyboard driver is broken, and it is how every embedded system in the world is developed.

---

## 7. Two sinks, one formatter

[`printk.c`](../nimbus/kernel/printk.c) wires this up:

```c
static void sink_both(void *ctx UNUSED, char c)
{
    serial_putc(c);
    if (console_ready) vga_putc(c);
}

static void sink_serial(void *ctx UNUSED, char c)
{
    serial_putc(c);
}
```

`kprintf` goes to both. `klog` goes to serial only.

That split is deliberate and worth stating as a rule:

**The screen is for the user. Put on it what a person watching the machine boot should see.**

**The log is for you. Put on it everything.**

A 25-line console cannot hold a log. Anything interesting scrolls away in under a second, and the one
line you needed is the one pushed off the top. Meanwhile a serial log costs nothing to write and
nothing to ignore, and `grep` works on it.

The four macros:

```c
#define LOG_DEBUG(...) klog("dbg", __VA_ARGS__)
#define LOG_INFO(...)  klog("inf", __VA_ARGS__)
#define LOG_WARN(...)  klog("WRN", __VA_ARGS__)
#define LOG_ERR(...)   klog("ERR", __VA_ARGS__)
```

Uppercase for the two that matter, so that scanning a log finds them. It is a small thing and it
works.

### 7.1 Timestamps

```c
void klog(const char *level, const char *fmt, ...)
{
    uint64_t ms = timer_ms();

    char prefix[32];
    snprintf(prefix, sizeof(prefix), "[%5u.%03u] %s  ",
             (uint32_t)(ms / 1000), (uint32_t)(ms % 1000), level);
    serial_puts(prefix);
    ...
```

Every line timestamped, because the most common kernel question is "what happened between these two
events" and without timestamps you cannot tell a 200-microsecond gap from a four-second one.

```
[    0.000] inf  gdt: 6 descriptors at c0104060, tss at c01040a0
[    0.000] inf  idt: 256 vectors at c0104100 (49 populated)
[    0.000] inf  pic: remapped to vectors 32-39 and 40-47 (masks were ff ff)
[    0.010] inf  pit: divisor 11931 -> 100.006 Hz (asked for 100)
[    0.010] inf  paging: direct map 128 MiB at c0000000, kernel text read-only
[    0.020] inf  heap: 1024 KiB at d0000000, header 16 bytes
```

Note the prefix goes through `snprintf` into a stack buffer rather than through the streaming sink.
The reason is prosaic: the prefix needs its own argument list, and you cannot conjure a `va_list`
from nothing. 32 bytes on the stack, never escaping the frame.

---

## 8. Using it

### 8.1 To the terminal

```bat
run nimbus
```

`-serial stdio` puts the log in the terminal you launched from. This is the default and it is what
you want 90% of the time.

### 8.2 To a file

```bash
qemu-system-i386 -m 128M -kernel bin/nimbus.elf -initrd bin/initrd.tar \
                 -serial file:bin/serial.log -no-reboot
```

Then `grep`, `diff`, `tail -f`. Diffing a working boot against a broken one is the single most
effective debugging technique in this book.

### 8.3 Both, on real hardware

On a machine with a real serial port, a null-modem cable to another machine running
`screen /dev/ttyUSB0 115200` gives you the same log. USB-to-serial adapters work and cost about five
pounds.

This is how you debug on metal, and Chapter 47 assumes it.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| No output at all | `serial_init` returned false; the self-test failed |
| Garbage characters | Baud rate mismatch — check the divisor and the terminal's setting |
| Output is a diagonal staircase | Missing CRLF translation |
| Boot hangs with partial output | Unbounded wait on a wedged transmitter |
| Output stops after a while | The QEMU backend closed, or the FIFO trigger is wrong |
| Works with `-serial stdio`, not `file:` | Usually a path problem, not a driver problem |
| Interrupts never arrive (Ex. 13.5) | OUT2 not set in the MCR |

---

## 10. Exercises

🟢 **13.1** Change the divisor to 12 (9600 baud) and run without changing the terminal. Describe the
output, then work out why it looks the way it does.

🟢 **13.2** Remove the CRLF translation and look at the log in a terminal. Then look at the same log
in a text editor — why does it look fine there?

🟢 **13.3** Comment out the loopback self-test and run with `-serial none`. What happens, and how long
does it take?

🟡 **13.4** Add `serial_printf` that writes only to serial, and use it to instrument `vga_scroll` —
one line per scroll with the cursor position. Watch the ordering during boot.

🟡 **13.5** Make reception interrupt-driven: set bit 0 of IER, register a handler on IRQ 4 (Chapter
17), push into a ring buffer. Confirm that typing in the QEMU terminal reaches the kernel.

🟡 **13.6** Detect whether a UART is a 16550 (with FIFO) or an 8250 (without): write `0xC7` to FCR,
read the Interrupt Identification register, and check bits 6–7. Log which one you found.

🔴 **13.7** Build a serial console: a loop that reads commands over the wire and executes them —
`mem`, `ps`, `dump <addr>`, `reboot`. Now you can drive the kernel with no keyboard driver at all,
which is exactly what you want in Chapter 19 while the keyboard driver is the thing being written.

---

## What we covered

- Four reasons a serial driver is the first thing to write: it works early, it survives a crash, it
  produces a file, and it goes both ways.
- What a UART is, why 8N1, and why both ends must agree on a rate with no clock on the wire.
- DLAB, and the register bank switch that makes offsets 0 and 1 mean two different things.
- Nine initialisation writes in a fixed order, including OUT2 — a motherboard wiring detail without
  which interrupts silently never arrive.
- A loopback self-test, and why a driver that cannot detect a missing port hangs the boot.
- Spinning on THRE, why an interrupt-driven transmit path cannot serve `panic()`, and why the bounded
  wait matters.
- CRLF translation, and the diagonal staircase you get without it.
- Two sinks: the screen for the user, the log for you — and timestamps, because gaps are the
  question you will be asking.

[Chapter 14](14-printf.md) builds the formatter both sinks feed: varargs on x86, a conversion state
machine, and the callback design that gives us `snprintf`, `kprintf` and userland's `printf` from one
implementation.

---

[← VGA driver](12-vga-driver.md) · [Contents](README.md) · [Next: printf from nothing →](14-printf.md)
