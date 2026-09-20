/* ===========================================================================
 *  nimbus/include/nimbus/isr.h  --  the shape of an interrupted CPU
 * ===========================================================================
 *  Explained in: docs/16-idt-exceptions.md and docs/17-pic-irqs.md
 * =========================================================================== */
#ifndef NIMBUS_ISR_H
#define NIMBUS_ISR_H

#include <nimbus/types.h>

/* ---------------------------------------------------------------------------
 *  registers_t -- what the stack looks like when a handler is entered
 *
 *  This structure is not a convenience. Its field order is *exactly* the order
 *  things end up on the stack, from lowest address to highest, and if you
 *  reorder a field here without changing isr.s the kernel will read EIP out of
 *  the slot holding EDI and jump somewhere at random.
 *
 *  Reading from the bottom (last pushed, lowest address) upwards:
 *
 *      ds          pushed by our stub, so the handler can restore it
 *      edi..eax    pushed by `pusha`, in its fixed order
 *      int_no      pushed by our stub: which vector fired
 *      err_code    pushed by the CPU for some exceptions, faked as 0 by us
 *                  for the rest, so that every frame has the same shape
 *      eip         --+
 *      cs            | pushed by the CPU when it took the interrupt
 *      eflags      --+
 *      useresp     --+ pushed by the CPU *only* if the interrupt crossed a
 *      ss          --+ privilege boundary (ring 3 -> ring 0)
 *
 *  Those last two are the subtle one. When an interrupt happens in kernel mode
 *  there is no stack switch, so the CPU does not push SS:ESP, and the two
 *  fields are whatever was on the stack already -- reading them is meaningless.
 *  Chapter 32 explains how to tell, and why the check is `(cs & 3) == 3`.
 * ------------------------------------------------------------------------- */
typedef struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

/*  `esp_dummy` is where `pusha` stored ESP. It is the value ESP had *before*
 *  the pusha, which is not useful and must not be popped back -- `popa`
 *  deliberately discards it. Naming it "dummy" stops anyone trusting it.      */

typedef void (*isr_handler_t)(registers_t *regs);

void isr_init(void);
void isr_register(uint8_t vector, isr_handler_t handler);

/*  The 32 architecturally defined exceptions. Vectors 0-31 are Intel's; 32-47
 *  are the hardware IRQs after we remap the PIC (Chapter 17); 0x80 is our
 *  system call gate (Chapter 33); everything else is unused.                  */
#define INT_DIVIDE_ERROR         0
#define INT_DEBUG                1
#define INT_NMI                  2
#define INT_BREAKPOINT           3
#define INT_OVERFLOW             4
#define INT_BOUND_RANGE          5
#define INT_INVALID_OPCODE       6
#define INT_DEVICE_NOT_AVAIL     7
#define INT_DOUBLE_FAULT         8
#define INT_COPROC_SEG_OVERRUN   9
#define INT_INVALID_TSS         10
#define INT_SEGMENT_NOT_PRESENT 11
#define INT_STACK_FAULT         12
#define INT_GENERAL_PROTECTION  13
#define INT_PAGE_FAULT          14
#define INT_RESERVED_15         15
#define INT_X87_FP              16
#define INT_ALIGNMENT_CHECK     17
#define INT_MACHINE_CHECK       18
#define INT_SIMD_FP             19
#define INT_VIRTUALIZATION      20

#define IRQ_BASE                32
#define INT_SYSCALL             0x80

const char *isr_exception_name(uint32_t vector);

/*  Print a full register dump. Used by the panic screen and by the default
 *  exception handler, and available from anywhere for ad-hoc debugging.       */
void isr_dump_registers(registers_t *regs);

#endif /* NIMBUS_ISR_H */
