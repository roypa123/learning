/* ===========================================================================
 *  nimbus/kernel/gdt.c  --  six descriptors
 * ===========================================================================
 *
 *  The bootloader already installed a GDT -- it had to, or the CPU could not
 *  be in protected mode. We replace it for three reasons:
 *
 *    1. We do not know where the bootloader's table is, how big it is, or
 *       whether the memory holding it is about to be handed out by the frame
 *       allocator. A GDT that vanishes is a triple fault at an unpredictable
 *       moment.
 *    2. It has no ring 3 descriptors, so userland is impossible without ours.
 *    3. It has no TSS, so taking an interrupt from ring 3 is impossible too.
 *
 *  Explained in: docs/15-gdt.md
 *  Line by line: docs/line-by-line/nimbus-gdt.md
 * =========================================================================== */

#include <nimbus/gdt.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>

/*  Both tables are plain globals in .bss. They must stay valid for the entire
 *  life of the machine -- the CPU reads the GDT on every segment register load
 *  -- so they cannot be on a stack or in the heap. A static array is the
 *  simplest thing that is permanently valid.                                  */
static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_pointer;
static tss_entry_t tss;

/*  Implemented in boot/cpu.asm.                                               */
extern void gdt_flush(gdt_ptr_t *ptr);
extern void tss_flush(void);

/* ---------------------------------------------------------------------------
 *  Fill in one descriptor.
 *
 *  Nothing here is interesting except the bit-shuffling, and the bit-shuffling
 *  is interesting only because of how wrong it looks. A 32-bit base is stored
 *  in three fields at offsets 2, 4 and 7. A 20-bit limit is stored in a 16-bit
 *  field and the low nibble of a byte that otherwise holds flags. The 80286
 *  had 24-bit bases and 16-bit limits in six bytes; the 386 needed 32 and 20,
 *  and rather than redesign the descriptor it bolted the extra bits into the
 *  two bytes that were left.
 * ------------------------------------------------------------------------- */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[index].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[index].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[index].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[index].granularity |= (uint8_t)(flags & 0xF0);

    gdt[index].access      = access;
}

void gdt_init(void)
{
    /* ---- 0: the null descriptor ---------------------------------------------
     * Required to be all zeros. Its purpose is to make selector 0 an invalid
     * one, so that a segment register left uninitialised -- which contains 0 --
     * faults on first use rather than addressing something plausible. */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* ---- 1: kernel code, 2: kernel data --------------------------------------
     * Base 0, limit 0xFFFFF with 4 KiB granularity, which is 4 GiB. Both
     * segments cover all of memory, which means segmentation does nothing --
     * a "flat" model. Every modern operating system does this, because paging
     * is a far better protection mechanism and x86-64 removed most of
     * segmentation for exactly that reason.
     *
     * So why have four segments at all instead of one? Because the *privilege
     * level* lives in the descriptor. Ring 3 must run with a CS whose DPL is
     * 3, and ring 0 with a CS whose DPL is 0, even though both describe the
     * same 4 GiB. The segments are not there to divide memory; they are there
     * to carry two bits. */
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_EXEC | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    /* ---- 3: user code, 4: user data ------------------------------------------
     * Identical, except DPL 3.
     *
     * Note that the user segments also span the full 4 GiB, including the
     * kernel's higher half. Segmentation is *not* what stops a user program
     * reading kernel memory -- paging is, via the USER bit in the page tables.
     * If you ever find yourself trying to protect the kernel by shrinking the
     * user segment limit, stop: that is the 1990s answer, it breaks as soon as
     * you want a higher-half kernel, and Chapter 23 explains the modern one. */
    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL3 | GDT_SEGMENT | GDT_EXEC | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL3 | GDT_SEGMENT | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    /* ---- 5: the TSS -----------------------------------------------------------
     * A system descriptor, not a segment descriptor, so S = 0 and the type
     * field means something different: 0x9 is "available 32-bit TSS".
     *
     * Base is the address of our one TSS; limit is its size minus one. DPL 3
     * so that `iret` into ring 3 and the automatic stack switch on the way
     * back in are both legal.
     *
     * The access byte 0xE9 = 1 11 0 1001 = present, DPL 3, system, TSS-32. */
    memset(&tss, 0, sizeof(tss));
    tss.ss0  = SEL_KDATA;
    tss.esp0 = 0;                 /* filled in per task by the scheduler */

    /* iomap_base pointing past the end of the structure means "there is no I/O
     * permission bitmap", which makes every `in` and `out` from ring 3 raise a
     * general protection fault. That is the behaviour we want: a user program
     * that can write to port 0x64 can reboot the machine. */
    tss.iomap_base = sizeof(tss_entry_t);

    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss_entry_t) - 1,
                  GDT_PRESENT | GDT_DPL3 | 0x09,
                  0x00);

    /* ---- Install it -----------------------------------------------------------
     * The limit is the size in bytes MINUS ONE, because the CPU stores "offset
     * of the last valid byte". Writing sizeof(gdt) here instead creates a
     * seventh descriptor made of whatever follows the array, and the fault it
     * eventually causes will point anywhere but at this line. */
    gdt_pointer.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_pointer.base  = (uint32_t)&gdt;

    gdt_flush(&gdt_pointer);
    tss_flush();

    LOG_INFO("gdt: %u descriptors at %p, tss at %p",
             (uint32_t)GDT_ENTRIES, (void *)gdt, (void *)&tss);
}

/* ---------------------------------------------------------------------------
 *  tss_set_kernel_stack
 *
 *  Called from the scheduler on every switch to a task that can enter ring 3.
 *
 *  Here is why it matters. When a user program makes a system call, the CPU
 *  switches to ring 0 -- and a ring 0 stack. It does not know which stack: it
 *  reads ss0:esp0 out of the TSS. There is one TSS for the whole machine, so
 *  that field must be updated to point at the incoming task's kernel stack
 *  before that task ever runs again.
 *
 *  Forget this and the symptom is spectacular: two processes make system calls
 *  and quietly scribble over each other's kernel stacks, and the crash happens
 *  in whichever one returns second, in a function that did nothing wrong.
 * ------------------------------------------------------------------------- */
void tss_set_kernel_stack(uint32_t esp0)
{
    tss.ss0  = SEL_KDATA;
    tss.esp0 = esp0;
}
