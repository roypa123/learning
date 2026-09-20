/* ===========================================================================
 *  nimbus/include/nimbus/idt.h  --  the interrupt descriptor table
 * ===========================================================================
 *  Explained in: docs/16-idt-exceptions.md
 * =========================================================================== */
#ifndef NIMBUS_IDT_H
#define NIMBUS_IDT_H

#include <nimbus/types.h>

#define IDT_ENTRIES 256

/*  A gate descriptor. Same 8 bytes as a GDT entry, different meaning: instead
 *  of describing a region of memory it describes a *place to jump to* when
 *  vector N fires.
 *
 *  The 32-bit offset of the handler is split into two 16-bit halves at
 *  opposite ends of the structure -- again, 80286 compatibility. The 286's
 *  gates were 6 bytes with a 16-bit offset; the 386 bolted the high half onto
 *  the end rather than redesigning the layout.                                */
typedef struct idt_entry {
    uint16_t offset_low;     /* handler address, bits 0..15                   */
    uint16_t selector;       /* which GDT code segment to run the handler in  */
    uint8_t  zero;           /* must be 0                                     */
    uint8_t  type_attr;      /* P | DPL(2) | 0 | type(4)                      */
    uint16_t offset_high;    /* handler address, bits 16..31                  */
} PACKED idt_entry_t;

typedef struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} PACKED idt_ptr_t;

/* ---- type_attr values ------------------------------------------------------
 *
 *  0x8E = 1 00 0 1110 : present, DPL 0, 32-bit interrupt gate
 *  0xEE = 1 11 0 1110 : present, DPL 3, 32-bit interrupt gate
 *  0x8F = 1 00 0 1111 : present, DPL 0, 32-bit trap gate
 *
 *  Interrupt gate vs trap gate is one bit and one behaviour: an interrupt gate
 *  clears IF on entry, a trap gate leaves it alone. Hardware interrupt
 *  handlers want IF cleared so they are not re-entered by the same device
 *  mid-handler. Debug traps want it left alone so the debugger can breathe.
 *
 *  DPL is the privilege level required to invoke the vector *with an `int`
 *  instruction*. Hardware can always deliver an interrupt regardless of DPL,
 *  which is why every vector except our syscall gate is DPL 0: if the page
 *  fault vector were DPL 3, a user program could execute `int 14` and hand the
 *  kernel a fabricated fault.                                                 */
#define IDT_INTERRUPT_GATE_K  0x8E
#define IDT_INTERRUPT_GATE_U  0xEE
#define IDT_TRAP_GATE_K       0x8F
#define IDT_TRAP_GATE_U       0xEF

void idt_init(void);
void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags);

#endif /* NIMBUS_IDT_H */
