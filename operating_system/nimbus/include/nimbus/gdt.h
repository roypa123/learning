/* ===========================================================================
 *  nimbus/include/nimbus/gdt.h  --  segments and the task state segment
 * ===========================================================================
 *  Explained in: docs/15-gdt.md and docs/32-usermode.md
 * =========================================================================== */
#ifndef NIMBUS_GDT_H
#define NIMBUS_GDT_H

#include <nimbus/types.h>

/*  Our selectors. A selector is not an index -- it is a byte offset into the
 *  GDT, with the bottom three bits carrying extra meaning:
 *
 *      bits 15..3  index of the descriptor
 *      bit  2      table indicator: 0 = GDT, 1 = LDT (we have no LDT)
 *      bits 1..0   RPL, the "requested privilege level"
 *
 *  So descriptor 3 at byte offset 0x18, requested at ring 3, is 0x18 | 3 =
 *  0x1B. That is why the user selectors below have odd-looking values, and
 *  why an `iret` to userland with CS = 0x18 instead of 0x1B general-protection
 *  faults: the CPU checks that RPL matches the privilege you are entering.    */
#define SEL_NULL        0x00
#define SEL_KCODE       0x08    /* ring 0 code,  descriptor 1 */
#define SEL_KDATA       0x10    /* ring 0 data,  descriptor 2 */
#define SEL_UCODE       0x1B    /* ring 3 code,  descriptor 3, RPL 3 */
#define SEL_UDATA       0x23    /* ring 3 data,  descriptor 4, RPL 3 */
#define SEL_TSS         0x2B    /* the TSS,      descriptor 5, RPL 3 */

#define GDT_ENTRIES     6

/*  One 8-byte descriptor, in the order the bytes appear in memory. The base
 *  and limit are split across non-adjacent fields for backwards compatibility
 *  with the 80286 and for no other reason at all.                             */
typedef struct gdt_entry {
    uint16_t limit_low;      /* limit bits 0..15                              */
    uint16_t base_low;       /* base  bits 0..15                              */
    uint8_t  base_mid;       /* base  bits 16..23                             */
    uint8_t  access;         /* P | DPL(2) | S | Type(4)                      */
    uint8_t  granularity;    /* G | D/B | L | AVL | limit bits 16..19         */
    uint8_t  base_high;      /* base  bits 24..31                             */
} PACKED gdt_entry_t;

/*  What `lgdt` reads: six bytes, a 16-bit limit and a 32-bit base.            */
typedef struct gdt_ptr {
    uint16_t limit;          /* size of the table in bytes, MINUS ONE         */
    uint32_t base;           /* linear address of the first descriptor        */
} PACKED gdt_ptr_t;

/*  Access byte bits.                                                          */
#define GDT_PRESENT     0x80    /* P: this descriptor is valid                */
#define GDT_DPL0        0x00    /* descriptor privilege level 0 (kernel)      */
#define GDT_DPL3        0x60    /* descriptor privilege level 3 (user)        */
#define GDT_SEGMENT     0x10    /* S=1: a code or data segment, not a gate    */
#define GDT_EXEC        0x08    /* code segment (executable)                  */
#define GDT_DIRECTION   0x04    /* data: expand-down. code: conforming.       */
#define GDT_RW          0x02    /* code: readable. data: writable.            */
#define GDT_ACCESSED    0x01    /* the CPU sets this; we leave it clear       */

/*  Granularity byte bits.                                                     */
#define GDT_GRAN_4K     0x80    /* G: the limit counts 4 KiB pages, not bytes */
#define GDT_SIZE_32     0x40    /* D/B: 32-bit default operand size           */
#define GDT_LONG_MODE   0x20    /* L: 64-bit code. Not us.                    */

/* ---------------------------------------------------------------------------
 *  The Task State Segment
 *
 *  The 386 designed the TSS for hardware task switching: one instruction
 *  saving every register into a TSS and loading another. Nobody uses it --
 *  it is slower than doing it by hand and it cannot be made to work with
 *  modern CPUs' register sets -- but the structure survives, because two of
 *  its fields are mandatory for something we absolutely do need.
 *
 *  When the CPU takes an interrupt while in ring 3, it must switch to a ring 0
 *  stack, and the only place it will look for that stack's address is ss0/esp0
 *  in the current TSS. Without a TSS, userland is impossible.
 *
 *  Every other field in this structure is dead weight we must still allocate.
 * ------------------------------------------------------------------------- */
typedef struct tss_entry {
    uint32_t prev_tss;   /* hardware task linking; unused                     */
    uint32_t esp0;       /* <-- the ring 0 stack pointer. This one matters.   */
    uint32_t ss0;        /* <-- the ring 0 stack segment. So does this.       */
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base; /* offset of the I/O permission bitmap. Setting it to
                          * sizeof(tss) means "no bitmap", which makes every
                          * `in`/`out` from ring 3 fault -- exactly what we
                          * want, since userland has no business touching
                          * hardware ports directly.                          */
} PACKED tss_entry_t;

void gdt_init(void);

/*  Called on every context switch: tells the CPU which kernel stack to use the
 *  next time this task traps in from ring 3. Get this wrong and the first
 *  system call from a new process corrupts another process's stack.           */
void tss_set_kernel_stack(uint32_t esp0);

#endif /* NIMBUS_GDT_H */
