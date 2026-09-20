/* ===========================================================================
 *  nimbus/kernel/isr.c  --  where every interrupt in the system arrives
 * ===========================================================================
 *
 *  Exactly one C function is reachable from the interrupt stubs, and this is
 *  it. Everything -- a divide by zero, the timer tick, a keystroke, a page
 *  fault, a system call -- comes through interrupt_dispatch() with a
 *  registers_t describing the interrupted machine.
 *
 *  Explained in: docs/16-idt-exceptions.md and docs/17-pic-irqs.md
 * =========================================================================== */

#define NIMBUS_KERNEL 1

#include <nimbus/isr.h>
#include <nimbus/irq.h>
#include <nimbus/idt.h>
#include <nimbus/kernel.h>
#include <nimbus/syscall.h>
#include <nimbus/paging.h>
#include <nimbus/task.h>
#include <nimbus/string.h>
#include <nimbus/io.h>

static isr_handler_t handlers[IDT_ENTRIES];

static const char *exception_names[32] = {
    "divide error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved (15)",
    "x87 floating-point exception",
    "alignment check",
    "machine check",
    "SIMD floating-point exception",
    "virtualization exception",
    "control protection exception",
    "reserved (22)", "reserved (23)", "reserved (24)", "reserved (25)",
    "reserved (26)", "reserved (27)", "reserved (28)", "reserved (29)",
    "security exception",
    "reserved (31)"
};

const char *isr_exception_name(uint32_t vector)
{
    return (vector < 32) ? exception_names[vector] : "hardware interrupt";
}

void isr_register(uint8_t vector, isr_handler_t handler)
{
    handlers[vector] = handler;
}

void isr_init(void)
{
    memset(handlers, 0, sizeof(handlers));
}

/* ---------------------------------------------------------------------------
 *  Decoding a selector error code
 *
 *  Several exceptions push an error code that refers to a descriptor. Its
 *  layout is not the same as a page fault's, which is the usual source of
 *  confusion when a #GP prints something that looks like a page fault code.
 *
 *      bit 0      external: the fault came from a hardware interrupt
 *      bit 1      the index refers to the IDT, not the GDT
 *      bit 2      with bit 1 clear: the index refers to the LDT
 *      bits 3..15 the selector index
 *
 *  An error code of 0 means "no descriptor was involved", which for a #GP
 *  usually means a privileged instruction was executed in ring 3.
 * ------------------------------------------------------------------------- */
static void describe_selector_error(uint32_t err)
{
    if (err == 0) {
        kprintf("  error code 0: no segment involved\n"
                "  (usually a privileged instruction executed in ring 3,\n"
                "   or a write to a read-only control register)\n");
        return;
    }
    kprintf("  error code %08x: %s, table=%s, index=%u\n",
            err,
            (err & 1) ? "external" : "internal",
            (err & 2) ? "IDT" : ((err & 4) ? "LDT" : "GDT"),
            (err >> 3) & 0x1FFF);
}

/* ---------------------------------------------------------------------------
 *  The default behaviour for an exception nobody claimed
 * ------------------------------------------------------------------------- */
static void unhandled_exception(registers_t *regs)
{
    kprintf("\n");
    kprintf("EXCEPTION %u: %s\n", regs->int_no, isr_exception_name(regs->int_no));

    if (regs->int_no == INT_GENERAL_PROTECTION ||
        regs->int_no == INT_INVALID_TSS ||
        regs->int_no == INT_SEGMENT_NOT_PRESENT ||
        regs->int_no == INT_STACK_FAULT)
        describe_selector_error(regs->err_code);

    isr_dump_registers(regs);

    /*  A fault in ring 3 kills the process; a fault in ring 0 kills the
     *  machine, because there is nothing left that we can trust. The test is
     *  the bottom two bits of the saved CS -- the privilege level the code was
     *  running at when the exception happened.                                */
    if ((regs->cs & 3) == 3) {
        kprintf("killing the faulting process\n");
        task_exit(-(int)regs->int_no);
    }

    panic("unhandled exception %u (%s) in kernel mode at eip=%08x",
          regs->int_no, isr_exception_name(regs->int_no), regs->eip);
}

/* ---------------------------------------------------------------------------
 *  interrupt_dispatch -- called from isr_common_stub in boot/isr.asm
 *
 *  Three jobs, in a strict order:
 *
 *    1. Find and run the handler.
 *    2. If this was a hardware IRQ, tell the PIC we are finished -- and do it
 *       *after* the handler, so that a second interrupt from the same device
 *       cannot arrive while the first is still being processed.
 *    3. Return, letting the stub restore the registers and `iret`.
 *
 *  What it must not do: call schedule() directly. Switching tasks from inside
 *  an interrupt handler means the stack we are standing on belongs to a task
 *  that is no longer running, and the `iret` at the end will unwind into it.
 *  The scheduler sets a flag instead, and the switch happens at a defined
 *  point. Chapter 31 goes through the failure mode in detail.
 * ------------------------------------------------------------------------- */
void interrupt_dispatch(registers_t *regs)
{
    uint32_t vector = regs->int_no;

    /*  The page fault gets its own path because it is the only exception we
     *  routinely expect: demand paging, stack growth and copy-on-write all
     *  arrive here as faults that are not errors.                             */
    if (vector == INT_PAGE_FAULT) {
        page_fault_handler(regs);
        return;
    }

    if (vector == INT_SYSCALL) {
        syscall_dispatch(regs);
        return;
    }

    if (handlers[vector]) {
        handlers[vector](regs);
    } else if (vector < 32) {
        unhandled_exception(regs);
    } else if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        /*  An unclaimed hardware interrupt. Not fatal -- a device we do not
         *  drive is allowed to shout -- but worth one log line, because an IRQ
         *  storm from an unhandled device is a real failure mode and it shows
         *  up here first.                                                     */
        static uint32_t complaints = 0;
        if (complaints < 8) {
            LOG_WARN("unhandled IRQ %u", vector - IRQ_BASE);
            complaints++;
        }
    }

    /*  End of interrupt. Only for hardware IRQs -- sending an EOI for a CPU
     *  exception tells the PIC to un-stack an interrupt that was never
     *  stacked, and corrupts its priority state.                              */
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16)
        pic_send_eoi((uint8_t)(vector - IRQ_BASE));
}
