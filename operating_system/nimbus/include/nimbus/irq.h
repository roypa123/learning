/* ===========================================================================
 *  nimbus/include/nimbus/irq.h  --  hardware interrupts and the 8259 PIC
 * ===========================================================================
 *  Explained in: docs/17-pic-irqs.md
 * =========================================================================== */
#ifndef NIMBUS_IRQ_H
#define NIMBUS_IRQ_H

#include <nimbus/types.h>
#include <nimbus/isr.h>

/*  The two 8259 chips. The second is cascaded into line 2 of the first, which
 *  is why IRQ 2 does not exist as a device line and why the old serial port on
 *  IRQ 9 is sometimes described as being on IRQ 2 -- on the original PC/XT
 *  there was one chip and it was.                                             */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20    /* "end of interrupt": we are done, send more    */

/*  The standard PC assignment of the 16 lines. Anything not listed is either
 *  free or belongs to a device we do not drive.                               */
#define IRQ_TIMER        0   /* 8253/8254 PIT                                 */
#define IRQ_KEYBOARD     1   /* 8042 controller, first PS/2 port              */
#define IRQ_CASCADE      2   /* the second PIC hangs here                     */
#define IRQ_COM2         3
#define IRQ_COM1         4
#define IRQ_LPT2         5
#define IRQ_FLOPPY       6
#define IRQ_LPT1         7   /* also where spurious interrupts appear         */
#define IRQ_RTC          8
#define IRQ_FREE1        9
#define IRQ_FREE2       10
#define IRQ_FREE3       11
#define IRQ_MOUSE       12   /* second PS/2 port                              */
#define IRQ_FPU         13
#define IRQ_ATA_PRIMARY 14
#define IRQ_ATA_SECOND  15   /* also spurious, on the slave                   */

void irq_init(void);
void irq_register(uint8_t irq, isr_handler_t handler);
void irq_unregister(uint8_t irq);

/*  Mask (disable) or unmask (enable) one line at the PIC. The PIC starts with
 *  everything masked in irq_init(); each driver unmasks its own line when it
 *  is ready to receive, which means an interrupt can never arrive before the
 *  handler that understands it exists.                                        */
void irq_mask(uint8_t irq);
void irq_unmask(uint8_t irq);

/*  Send the end-of-interrupt signal. Until this arrives, the PIC refuses to
 *  deliver any interrupt of equal or lower priority -- so a handler that
 *  forgets its EOI does not crash, it silently kills the keyboard and the
 *  timer, and the machine appears to hang for no reason.                      */
void pic_send_eoi(uint8_t irq);

void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_disable(void);

/*  Read the In-Service Register: which interrupts is the PIC currently
 *  handling? Used to distinguish a real IRQ 7 from a spurious one.            */
uint16_t pic_get_isr(void);
uint16_t pic_get_irr(void);

#endif /* NIMBUS_IRQ_H */
