# Chapter 17 — Interrupts II: the PIC and IRQs

[← The IDT](16-idt-exceptions.md) · [Contents](README.md) · [Next: The PIT →](18-pit-timer.md)

---

## Goal

Make hardware interrupts work. Remap two 1981 chips so their vectors stop colliding with CPU
exceptions, mask every line until a driver claims it, and handle the end-of-interrupt signal whose
absence silently kills the keyboard.

---

## 1. The problem the 8259 solves

A CPU has one interrupt pin. A PC has sixteen devices that need to interrupt it.

The 8259 Programmable Interrupt Controller multiplexes them. Devices connect to it, it connects to
the CPU's `INTR` pin, and when it raises that pin it also puts a vector number on the bus so the CPU
knows which handler to run.

It does three more things worth knowing:

- **Priority.** Lower-numbered lines win. IRQ 0 (the timer) outranks IRQ 1 (the keyboard).
- **Masking.** Each line can be individually disabled.
- **Nesting control.** While a handler is running, the PIC refuses to deliver interrupts of equal or
  lower priority until it is told the handler has finished.

That last one is the EOI, and §5 is about what happens when you forget it.

### 1.1 Why there are two

The original IBM PC had one 8259 with eight lines. The PC/AT in 1984 needed more, so IBM wired a
second chip into line 2 of the first.

```
                            +---------+
    IRQ 0  timer      ----->|         |
    IRQ 1  keyboard   ----->|         |
    IRQ 2  (cascade)  <-----|  master |-----> CPU INTR
    IRQ 3  COM2       ----->|  0x20   |
    ...                     |         |
    IRQ 7  LPT1       ----->|         |
                            +---------+
                                 ^
                            +---------+
    IRQ 8  RTC        ----->|         |
    IRQ 9             ----->|  slave  |
    ...                     |  0xA0   |
    IRQ 15 ATA 2nd    ----->|         |
                            +---------+
```

Consequences:

- **IRQ 2 is not a device line.** Anything that claims to be on IRQ 2 is really on IRQ 9 — which is
  why the old serial port is sometimes described both ways. On the PC/XT, with one chip, it genuinely
  was IRQ 2.
- **A slave interrupt must be acknowledged at both chips** (§5).
- **Unmasking a slave line does nothing** unless line 2 on the master is also unmasked.

### 1.2 What replaced it

The APIC — Advanced Programmable Interrupt Controller — arrived with the Pentium and is better in
every way: more lines, per-CPU delivery for SMP, message-signalled interrupts from PCI devices, and
no cascade.

Finding it requires parsing ACPI tables, which requires an ACPI parser, which is several thousand
lines. The 8259 is still emulated by every chipset, is still what the machine boots with, and is what
every teaching kernel uses. Chapter 48 says what the upgrade involves.

---

## 2. The remap, which is not optional

At power-on the PICs deliver:

- IRQ 0–7 as vectors **8–15**
- IRQ 8–15 as vectors **0x70–0x77**

In real mode that was fine. In protected mode, vectors 8–15 are CPU exceptions:

| Vector | Exception | IRQ that would collide |
|---|---|---|
| 8 | Double fault | IRQ 0, the timer |
| 9 | Coprocessor overrun | IRQ 1, the keyboard |
| 13 | General protection | IRQ 5 |
| 14 | Page fault | IRQ 6, the floppy |

So a timer tick arrives as a double fault, a keystroke as a coprocessor overrun, and the kernel
panics about an exception that never happened.

We move them to 32–47, the first vectors Intel promises never to use.

```c
void pic_remap(uint8_t offset1, uint8_t offset2)
{
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);  io_wait();

    outb(PIC1_DATA, offset1);                   io_wait();
    outb(PIC2_DATA, offset2);                   io_wait();

    outb(PIC1_DATA, 0x04);                      io_wait();
    outb(PIC2_DATA, 0x02);                      io_wait();

    outb(PIC1_DATA, ICW4_8086);                 io_wait();
    outb(PIC2_DATA, ICW4_8086);                 io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
```

### 2.1 Four Initialisation Control Words, in order

The chip latches into "expecting the next ICW" state after each write. You cannot reorder them, skip
one, or talk to the other chip in between.

**ICW1** — `0x11`: begin initialisation (bit 4), and promise an ICW4 (bit 0). On a PC the ICW4 is
mandatory.

**ICW2** — the vector offset. This is the whole point of the exercise.

**ICW3** — the wiring. And here is the 8259 in one detail:

```c
    outb(PIC1_DATA, 0x04);      /* master: "a slave is on line 2" as a BITMASK */
    outb(PIC2_DATA, 0x02);      /* slave:  "you are cascade identity 2" as a NUMBER */
```

The master is told as a bitmask — `1 << 2` = 4. The slave is told as a plain number — 2. Two
different encodings of the same fact, in two registers with the same name.

**ICW4** — `0x01`, 8086 mode. Without it the chip stays in MCS-80/85 mode and delivers a CALL
instruction instead of a vector number.

### 2.2 `io_wait()` is not superstition

```c
static ALWAYS_INLINE void io_wait(void)
{
    outb(0x80, 0);
}
```

The 8259 needs a few hundred nanoseconds to latch each control word. A modern CPU issues two
consecutive `out` instructions far faster than that, and the chip silently drops one.

The symptom is that interrupts arrive at the wrong vectors — which looks exactly like forgetting to
remap at all, and sends you looking in the wrong place.

Port `0x80` is the POST diagnostic port: unused after boot, harmless to write, and an I/O bus cycle
takes about a microsecond regardless of CPU speed. It is a hack, it is what Linux does, and there is
no better portable option.

---

## 3. Masking everything, then unmasking per driver

```c
void irq_init(void)
{
    for (int i = 0; i < 16; i++) irq_handlers[i] = NULL;

    pic_remap(IRQ_BASE, IRQ_BASE + 8);

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    for (int v = IRQ_BASE; v < IRQ_BASE + 16; v++)
        isr_register((uint8_t)v, irq_trampoline);
}
```

All sixteen lines masked. Each driver unmasks its own in `irq_register`:

```c
void irq_register(uint8_t irq, isr_handler_t handler)
{
    if (irq >= 16) return;
    irq_handlers[irq] = handler;
    irq_unmask(irq);
    LOG_INFO("irq: line %u claimed", irq);
}
```

**An interrupt can never arrive before the handler that understands it exists.** That is the
invariant, and it is worth the two lines.

It also protects against devices the BIOS left half-configured. The BIOS enables the timer and the
keyboard before handing over; without masking, IRQ 0 fires the instant we `sti`, into a dispatcher
whose timer handler has not been registered.

### 3.1 The cascade line

```c
void irq_unmask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(1 << (irq & 7));

    outb(port, (uint8_t)(inb(port) & ~bit));

    if (irq >= 8) {
        uint8_t m = inb(PIC1_DATA);
        outb(PIC1_DATA, (uint8_t)(m & ~(1 << IRQ_CASCADE)));
    }
}
```

Unmasking a slave line does nothing unless the master's line 2 is also unmasked — the slave's output
goes *through* the master, and a masked master line blocks it.

Forgetting this is why the ATA driver's interrupt (IRQ 14) never arrives even though everything
looks right. Three lines, done once, in the place that cannot be forgotten.

### 3.2 The read-modify-write race

```c
    outb(port, (uint8_t)(inb(port) | bit));
```

Read, modify, write. If an interrupt lands between the read and the write, and its handler also
touches the mask, one of the two updates is lost.

On a uniprocessor with interrupts already disabled inside a handler this is safe. Called from task
context with interrupts on, it is not. The source says so:

> which is why callers are expected to be in early boot or holding interrupts off. Chapter 36
> revisits it.

This is the first genuine race in the kernel and it is worth noticing now, because Chapter 36 is
going to ask you to find several more.

---

## 4. The trampoline and spurious interrupts

```c
static void irq_trampoline(registers_t *regs)
{
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);

    if (irq == 7 || irq == 15) {
        uint16_t isr = pic_get_isr();
        if (!(isr & (1 << irq))) {
            if (irq == 15)
                outb(PIC1_COMMAND, PIC_EOI);
            return;
        }
    }

    if (irq_handlers[irq])
        irq_handlers[irq](regs);
}
```

### 4.1 What a spurious interrupt is

If a device drops its request line in the window between the PIC raising `INTR` and the CPU
acknowledging it, the PIC has already committed to delivering *something*. What it delivers is
**IRQ 7** on the master, or **IRQ 15** on the slave.

The give-away is the In-Service Register: a real interrupt sets the corresponding bit, a spurious one
does not.

```c
static uint16_t pic_read_register(uint8_t ocw3)
{
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (uint16_t)((inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND));
}

uint16_t pic_get_isr(void) { return pic_read_register(PIC_READ_ISR); }
```

### 4.2 The rule that matters

**A spurious interrupt must NOT be acknowledged with an EOI.**

The chip never marked it in-service, so an EOI would clear the bit for whatever *is* in service —
losing a genuine interrupt and leaving its device waiting forever.

The exception is a spurious IRQ 15: the master does not know it was spurious, because it saw a real
request from the slave. So the master gets an EOI and the slave does not.

This is not hypothetical. It appears as "the keyboard stops working after a few minutes on hardware",
and it is one of the differences between QEMU (which never generates spurious interrupts) and a real
machine (which does, occasionally, particularly with a parallel port).

---

## 5. End of interrupt

```c
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    outb(PIC1_COMMAND, PIC_EOI);
}
```

Two lines, and both matter.

**Until the EOI arrives, the PIC refuses to deliver any interrupt of equal or lower priority.** That
is the nesting control from §1 doing its job — it stops a device re-interrupting its own handler.

So a handler that forgets its EOI does not crash. It silently kills the timer, the keyboard and
everything below it in priority, and the machine appears to hang for no reason. If you ever see
"everything stops after exactly one keypress", this is the first thing to check.

**An interrupt from the slave passed through the master**, so both chips have it marked in-service
and both need telling. Acknowledging only the master leaves the slave believing it is still busy, and
every subsequent interrupt from lines 8–15 is dropped — so the disk works once and then never again.

### 5.1 Where the EOI goes

```c
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));

        if (timer_need_resched && sched_enabled) {
            timer_need_resched = false;
            schedule();
        }
    }
```

**After the handler, before the reschedule.** The ordering is deliberate and Chapter 31 depends on
it.

If we switched tasks before the EOI, the PIC would still be waiting for the acknowledgement of *this*
interrupt while an entirely different task ran. No further timer interrupt would be delivered until
this task happened to be scheduled back in — and since scheduling depends on timer interrupts, the
system would deadlock at random.

Sending the EOI first means the PIC is free to deliver the next interrupt no matter which task we
switch to.

**Only for hardware IRQs.** Sending an EOI for a CPU exception tells the PIC to un-stack an interrupt
that was never stacked, and corrupts its priority state.

---

## 6. Automatic EOI, and why not

ICW4 bit 1 is `AUTO_EOI`, which makes the chip acknowledge automatically on delivery.

```c
#define ICW4_AUTO_EOI   0x02   /* acknowledge automatically. We do not want */
                               /* this: a manual EOI is what lets a handler */
                               /* finish before the next interrupt arrives. */
```

It removes two port writes per interrupt and removes the ability to forget them. It also removes the
nesting control entirely: a slow handler can be re-entered by the same device, and now you need
re-entrant handlers or a software-level "am I already in here" flag.

Manual EOI, in the one place all interrupts funnel through, is the better trade.

---

## 7. The sixteen lines

```c
#define IRQ_TIMER        0   /* 8253/8254 PIT                   */  Chapter 18
#define IRQ_KEYBOARD     1   /* 8042 controller, first PS/2 port */  Chapter 19
#define IRQ_CASCADE      2   /* the second PIC hangs here        */
#define IRQ_COM2         3
#define IRQ_COM1         4   /* serial receive                   */  Ex. 13.5
#define IRQ_LPT2         5
#define IRQ_FLOPPY       6
#define IRQ_LPT1         7   /* also where spurious appear       */
#define IRQ_RTC          8
#define IRQ_FREE1        9
#define IRQ_FREE2       10
#define IRQ_FREE3       11
#define IRQ_MOUSE       12   /* second PS/2 port                 */
#define IRQ_FPU         13
#define IRQ_ATA_PRIMARY 14   /* ...which our driver masks        */  Chapter 37
#define IRQ_ATA_SECOND  15   /* also spurious, on the slave      */
```

These assignments were fixed by IBM in 1984 and have not moved. A modern machine still routes its
PS/2 keyboard emulation to IRQ 1 and its legacy ATA controller to IRQ 14, even when neither device
physically exists.

---

## 8. Running it

```c
    irq_init();
    ...
    sti();
```

Nothing visible happens yet — no driver has registered. The log shows:

```
[    0.000] inf  pic: remapped to vectors 32-39 and 40-47 (masks were ff ff)
```

The "masks were" part is informative: it tells you what the BIOS had enabled. `ff ff` means
everything was masked, which QEMU's SeaBIOS does. Real BIOSes often leave the timer and keyboard
unmasked, and seeing `fc ff` there is a reminder of why we mask everything immediately.

### 8.1 Proving it works

Register a handler for a line nothing uses and trigger it by hand:

```c
static void test_handler(registers_t *regs UNUSED)
{
    kprintf("IRQ 5 fired!\n");
}

    irq_register(5, test_handler);
    __asm__ volatile ("int $37");     /* 32 + 5 */
```

`int 37` goes through the IDT to the same stub, so the dispatcher runs the handler — and then sends
an EOI for an interrupt the PIC never raised, which is harmless once but is not something to leave
in.

The real test is Chapter 18: enable the timer and watch a counter increase.

### 8.2 Watching interrupts in QEMU

```bash
qemu-system-i386 ... -d int -D bin/qemu.log
```

```
    33: v=20 e=0000 i=0 cpl=0 IP=0008:c010235a pc=c010235a SP=0010:c0106f8c
```

`v=20` is vector 32 — IRQ 0, the timer. `cpl=0` is the privilege level it interrupted. `IP=` is
where it interrupted.

This log is enormous — 100 lines per second from the timer alone — and it is the single most useful
diagnostic when interrupts are misbehaving. `grep -v "v=20"` to drop the timer noise.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| Panic about a double fault on `sti` | PIC not remapped |
| Everything stops after one interrupt | Missing EOI |
| Lines 8–15 work once, then never | EOI sent to the master only |
| IRQ 14 never fires | Cascade line 2 still masked |
| Interrupts at the wrong vectors | Missing `io_wait` between ICWs |
| Keyboard dies after minutes on hardware | Spurious IRQ 7 acknowledged with an EOI |
| Random deadlocks under load | Reschedule before EOI |
| Interrupt storm in the log | A device asserting continuously; check the `complaints` counter output |

---

## 10. Exercises

🟢 **17.1** Remove the `io_wait()` calls and boot. In QEMU it will probably still work — explain why,
and what would happen on a 3 GHz machine with a real 8259.

🟢 **17.2** Comment out `pic_send_eoi` and boot with the timer enabled (Chapter 18). How many ticks
do you get?

🟢 **17.3** Print the mask registers before and after `irq_init`, and after each driver registers.
Confirm the cascade line opens when a slave line is claimed.

🟡 **17.4** Implement `irq_stats()`: a count per line, printed by a shell command. Use it to see the
timer's rate and confirm it is 100 Hz.

🟡 **17.5** Write the code to detect a spurious IRQ 7 deliberately: mask IRQ 7, then trigger a
parallel port interrupt. (Hard in QEMU; read `pic_get_isr()` output instead and reason about it.)

🟡 **17.6** Make `irq_mask`/`irq_unmask` safe to call from task context by wrapping them in
`irq_save`/`irq_restore`. Explain why that is sufficient on a uniprocessor and what SMP would need.

🔴 **17.7** Set `ICW4_AUTO_EOI` and remove all EOI calls. Everything will appear to work. Now write a
handler that takes 50 ms and see what re-entrancy does to it.

---

## What we covered

- One interrupt pin, sixteen devices, and the chip that multiplexes them — plus priority, masking and
  nesting control.
- Two chips cascaded through line 2, and the three consequences that follow.
- The remap: why vectors 8–15 colliding with exceptions makes a timer tick look like a double fault.
- Four ICWs in a fixed order, including ICW3's two different encodings of the same fact.
- `io_wait`, and the failure it prevents looking exactly like a different failure.
- Mask everything, unmask per driver — so no interrupt can arrive before its handler exists.
- The cascade line that must be opened for any slave interrupt to arrive.
- Spurious interrupts on IRQ 7 and 15, how to detect them, and why acknowledging one loses a real
  interrupt.
- EOI: two chips for a slave interrupt, and *after* the handler but *before* the reschedule.

[Chapter 18](18-pit-timer.md) claims IRQ 0 and gives the kernel a heartbeat — the first thing that
makes it act on its own.

---

[← The IDT](16-idt-exceptions.md) · [Contents](README.md) · [Next: The PIT →](18-pit-timer.md)
