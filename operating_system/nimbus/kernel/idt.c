/* ===========================================================================
 *  nimbus/kernel/idt.c  --  256 doors, 49 of them with something behind them
 * ===========================================================================
 *  Explained in: docs/16-idt-exceptions.md
 * =========================================================================== */

#include <nimbus/idt.h>
#include <nimbus/gdt.h>
#include <nimbus/isr.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_pointer;

extern void idt_load(idt_ptr_t *ptr);

/*  Built by the %rep in boot/isr.asm: 48 stub addresses, in vector order.
 *  Declaring it as an array of uint32_t rather than of function pointers is
 *  deliberate -- we only ever want the numeric address, and a function pointer
 *  array would invite someone to call one of them from C, which would push a
 *  return address the stub's `iret` is not expecting.                         */
extern uint32_t isr_stub_table[48];
extern uint32_t isr_syscall_stub[1];

void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = flags;
}

void idt_init(void)
{
    /*  Zero first. An IDT entry with the present bit clear means "this vector
     *  is not handled", and the CPU responds to an unhandled vector with a
     *  general protection fault -- which we *do* handle, and which prints a
     *  useful message. Leaving the table full of uninitialised .bss would mean
     *  a stray interrupt jumping to a random address instead.
     *
     *  (In practice .bss is already zero. Doing it explicitly costs a memset
     *  of 2 KiB once, and removes a dependency on that being true.)           */
    memset(idt, 0, sizeof(idt));

    /*  Vectors 0..47: exceptions and hardware IRQs.
     *
     *  Selector SEL_KCODE: the handler runs in the kernel's code segment,
     *  whatever ring the interrupted code was in. That switch is automatic and
     *  is the entire point of a gate.
     *
     *  Flags 0x8E: present, DPL 0, 32-bit interrupt gate. DPL 0 means ring 3
     *  cannot reach these with an `int` instruction. That is not paranoia: if
     *  a user program could execute `int 14`, it could hand the kernel a
     *  fabricated page fault with an error code of its choosing, at a moment
     *  of its choosing, with CR2 left over from some earlier real fault.      */
    for (int v = 0; v < 48; v++)
        idt_set_gate((uint8_t)v, isr_stub_table[v], SEL_KCODE, IDT_INTERRUPT_GATE_K);

    /*  Vector 0x80: the system call.
     *
     *  The one gate with DPL 3, because the whole purpose of a system call is
     *  that ring 3 can invoke it. This single bit is the door in the wall, and
     *  everything in Chapter 33 about validating user pointers exists because
     *  this bit is set.
     *
     *  Still an *interrupt* gate, not a trap gate, so IF is cleared on entry.
     *  A syscall handler that starts with interrupts enabled must be
     *  re-entrant from the first instruction; ours is not, and does not need
     *  to be. syscall_dispatch() re-enables them once it is on a safe footing.
     */
    idt_set_gate(INT_SYSCALL, isr_syscall_stub[0], SEL_KCODE, IDT_INTERRUPT_GATE_U);

    idt_pointer.limit = (uint16_t)(sizeof(idt) - 1);
    idt_pointer.base  = (uint32_t)&idt;

    idt_load(&idt_pointer);

    LOG_INFO("idt: 256 vectors at %p (49 populated)", (void *)idt);
}
