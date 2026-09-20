/* ===========================================================================
 *  nimbus/kernel/irq.c  --  the 8259 programmable interrupt controller
 * ===========================================================================
 *
 *  A CPU has one interrupt pin. A PC has sixteen devices that need to
 *  interrupt it. The 8259 is the chip that multiplexes them: devices connect
 *  to it, it connects to the CPU, and when it raises the line it also tells
 *  the CPU which vector to dispatch.
 *
 *  Two of them are cascaded -- the original PC had one, with eight lines, and
 *  the PC/AT in 1984 needed more, so IBM wired a second chip into line 2 of
 *  the first. That is why IRQ 2 is not a device, why the lines are numbered
 *  0-7 and 8-15 rather than 0-15, and why acknowledging an interrupt from the
 *  second chip requires telling *both* chips about it.
 *
 *  Modern machines have an APIC instead, which is better in every way and
 *  needs ACPI tables to find. The 8259 is still emulated by every chipset,
 *  still what the machine boots with, and still what every teaching kernel
 *  uses. Chapter 48 says what the upgrade involves.
 *
 *  Explained in: docs/17-pic-irqs.md
 * =========================================================================== */

#include <nimbus/irq.h>
#include <nimbus/isr.h>
#include <nimbus/io.h>
#include <nimbus/kernel.h>

/*  Initialisation Control Words. Four bytes, in a fixed order, and the chip
 *  latches into "expecting the next ICW" state after each one -- so you cannot
 *  reorder them, skip one, or talk to the other chip in between.              */
#define ICW1_ICW4       0x01   /* we will send an ICW4 (we must, on a PC)     */
#define ICW1_SINGLE     0x02   /* single mode; we are cascaded, so not set    */
#define ICW1_INTERVAL4  0x04
#define ICW1_LEVEL      0x08   /* level-triggered; PC is edge-triggered       */
#define ICW1_INIT       0x10   /* the bit that means "start initialisation"   */

#define ICW4_8086       0x01   /* 8086/88 mode rather than MCS-80/85          */
#define ICW4_AUTO_EOI   0x02   /* acknowledge automatically. We do not want   */
                               /* this: a manual EOI is what lets a handler   */
                               /* finish before the next interrupt arrives.   */
#define ICW4_BUF_SLAVE  0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM       0x10

#define PIC_READ_IRR    0x0A
#define PIC_READ_ISR    0x0B

static isr_handler_t irq_handlers[16];

/* ---------------------------------------------------------------------------
 *  pic_remap -- move the hardware interrupts out of the CPU's way
 *
 *  This is not optional and it is the first thing any protected-mode kernel
 *  must do.
 *
 *  At power-on the PICs deliver IRQ 0-7 as vectors 8-15 and IRQ 8-15 as
 *  vectors 0x70-0x77. In real mode that was fine. In protected mode, vectors
 *  8-15 are CPU exceptions: 8 is #DF (double fault), 13 is #GP, 14 is #PF. So
 *  a timer tick arrives as a double fault, and a keystroke arrives as a
 *  general protection fault, and the kernel panics with a message about an
 *  exception that never happened.
 *
 *  We move them to 32-47, the first vectors Intel promises never to use.
 * ------------------------------------------------------------------------- */
void pic_remap(uint8_t offset1, uint8_t offset2)
{
    /*  Save the masks. The BIOS left some lines enabled and some disabled and
     *  we restore its choices afterwards -- except that irq_init() then masks
     *  everything anyway. Reading them is still worth doing once, because the
     *  value is informative in a log.                                         */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /*  ICW1: begin initialisation, and promise an ICW4.                       */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);  io_wait();

    /*  ICW2: the vector offset. This is the whole point of the exercise.      */
    outb(PIC1_DATA, offset1);                   io_wait();
    outb(PIC2_DATA, offset2);                   io_wait();

    /*  ICW3: the wiring. The master is told "a slave is on line 2" as a
     *  bitmask (1 << 2 = 4); the slave is told "you are cascade identity 2" as
     *  a plain number. Two different encodings of the same fact, in two
     *  registers with the same name. This is the 8259 in one detail.          */
    outb(PIC1_DATA, 0x04);                      io_wait();
    outb(PIC2_DATA, 0x02);                      io_wait();

    /*  ICW4: 8086 mode. Without this the chip stays in MCS-80/85 mode and
     *  delivers a CALL instruction instead of a vector number.                */
    outb(PIC1_DATA, ICW4_8086);                 io_wait();
    outb(PIC2_DATA, ICW4_8086);                 io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    LOG_INFO("pic: remapped to vectors %u-%u and %u-%u (masks were %02x %02x)",
             offset1, offset1 + 7, offset2, offset2 + 7, mask1, mask2);
}

/*  The io_wait() after every write is not superstition. The 8259 needs a few
 *  hundred nanoseconds to latch each control word, and a modern CPU issues two
 *  consecutive `out` instructions far faster than that. Without the delay the
 *  chip silently drops a word, and the symptom is that interrupts arrive at
 *  the wrong vectors -- which looks exactly like forgetting to remap at all. */

void pic_send_eoi(uint8_t irq)
{
    /*  An interrupt from the slave passed *through* the master, so both chips
     *  have it marked in-service and both need telling. Acknowledging only the
     *  master leaves the slave believing it is still busy, and every
     *  subsequent interrupt from lines 8-15 is dropped -- so the disk works
     *  once and then never again.                                             */
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    outb(PIC1_COMMAND, PIC_EOI);
}

void irq_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(1 << (irq & 7));

    /*  The mask register is read-modify-write, and it is shared with the
     *  interrupt handler path. On a uniprocessor with interrupts already
     *  disabled inside a handler this is safe; called from task context it is
     *  not, which is why callers are expected to be in early boot or holding
     *  interrupts off. Chapter 36 revisits it.                                */
    outb(port, (uint8_t)(inb(port) | bit));
}

void irq_unmask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(1 << (irq & 7));

    outb(port, (uint8_t)(inb(port) & ~bit));

    /*  Unmasking a line on the slave does nothing unless line 2 on the master
     *  -- the cascade -- is also unmasked. Forgetting this is why the ATA
     *  driver's interrupt never arrives even though everything looks right.   */
    if (irq >= 8) {
        uint8_t m = inb(PIC1_DATA);
        outb(PIC1_DATA, (uint8_t)(m & ~(1 << IRQ_CASCADE)));
    }
}

static uint16_t pic_read_register(uint8_t ocw3)
{
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (uint16_t)((inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND));
}

uint16_t pic_get_irr(void) { return pic_read_register(PIC_READ_IRR); }
uint16_t pic_get_isr(void) { return pic_read_register(PIC_READ_ISR); }

/* ---------------------------------------------------------------------------
 *  The dispatcher for hardware interrupts
 * ------------------------------------------------------------------------- */
static void irq_trampoline(registers_t *regs)
{
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);

    /*  Spurious interrupts.
     *
     *  If a device drops its request line in the window between the PIC
     *  raising INTR and the CPU acknowledging it, the PIC has to deliver
     *  *something*, and what it delivers is IRQ 7 (or IRQ 15 on the slave).
     *  The give-away is that the In-Service Register bit is not set: a real
     *  interrupt sets it, a spurious one does not.
     *
     *  A spurious interrupt must NOT be acknowledged with an EOI -- the chip
     *  never marked it in service, so an EOI would clear the wrong bit and
     *  lose a genuine interrupt. This is a real bug that appears as "the
     *  keyboard stops working after a few minutes on hardware".               */
    if (irq == 7 || irq == 15) {
        uint16_t isr = pic_get_isr();
        if (!(isr & (1 << irq))) {
            if (irq == 15)
                outb(PIC1_COMMAND, PIC_EOI);   /* the master still needs one  */
            return;
        }
    }

    if (irq_handlers[irq])
        irq_handlers[irq](regs);
}

void irq_register(uint8_t irq, isr_handler_t handler)
{
    if (irq >= 16) return;
    irq_handlers[irq] = handler;
    irq_unmask(irq);
    LOG_INFO("irq: line %u claimed", irq);
}

void irq_unregister(uint8_t irq)
{
    if (irq >= 16) return;
    irq_mask(irq);
    irq_handlers[irq] = NULL;
}

void irq_init(void)
{
    for (int i = 0; i < 16; i++) irq_handlers[i] = NULL;

    pic_remap(IRQ_BASE, IRQ_BASE + 8);

    /*  Mask everything. Each driver unmasks its own line in irq_register(),
     *  which means no interrupt can arrive before a handler that understands
     *  it exists -- and that a device left half-configured by the BIOS cannot
     *  interrupt us at all.
     *
     *  0xFF on both chips masks all sixteen lines. The cascade line gets
     *  unmasked automatically the first time anything on the slave registers. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    for (int v = IRQ_BASE; v < IRQ_BASE + 16; v++)
        isr_register((uint8_t)v, irq_trampoline);
}

void pic_disable(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
