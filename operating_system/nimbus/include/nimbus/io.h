/* ===========================================================================
 *  nimbus/include/nimbus/io.h  --  the I/O address space
 * ===========================================================================
 *
 *  x86 has two address spaces, not one.
 *
 *  Memory space is the one you know: 4 GiB of it, reached with `mov`, and it
 *  is where RAM, video memory and PCI device windows live.
 *
 *  I/O space is a separate 64 KiB space reached only with the `in` and `out`
 *  instructions. It exists because in 1978 address pins were expensive and
 *  Intel copied the 8080, which copied the 8008. Sixty-five thousand addresses
 *  seemed like plenty for peripherals. The keyboard controller, the interrupt
 *  controller, the timer, the serial ports and the ATA disk interface all live
 *  there, and all of them are still at the addresses IBM chose in 1981.
 *
 *  A pointer cannot reach I/O space. There is no way to express `in` in C.
 *  Hence this header: six one-line functions, each wrapping one instruction.
 *
 *  Explained in: docs/12-vga-driver.md (section "ports and MMIO")
 * =========================================================================== */
#ifndef NIMBUS_IO_H
#define NIMBUS_IO_H

#include <nimbus/types.h>

/*  The constraints, once, because they repeat in all six functions:
 *
 *    "a"  = the value must be in AL/AX/EAX      (what in/out require)
 *    "Nd" = the port must be in DX, or be an immediate constant 0..255
 *           (the N is "immediate 0-255"; the assembler picks the short form
 *            `out 0x20, al` when it can, which is one byte smaller)
 *    "=a" = an output, delivered in AL/AX/EAX
 *
 *  `volatile` on the asm block is essential. Without it GCC treats the block as
 *  a pure function of its inputs and is free to delete it, hoist it out of a
 *  loop, or reuse a previous result. Port reads have side effects -- reading
 *  0x60 *consumes* the keyboard byte -- so every one of them must happen
 *  exactly where it was written, exactly as many times.
 */

static ALWAYS_INLINE void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static ALWAYS_INLINE uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static ALWAYS_INLINE void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" :: "a"(value), "Nd"(port));
}

static ALWAYS_INLINE uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static ALWAYS_INLINE void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" :: "a"(value), "Nd"(port));
}

static ALWAYS_INLINE uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*  Block transfer: `rep insw` reads `count` words from `port` into memory.
 *  The ATA PIO driver moves 256 words per sector this way, which is roughly
 *  five times faster than a C loop around inw() because the CPU does not
 *  re-decode the instruction each time.                                       */
static ALWAYS_INLINE void insw(uint16_t port, void *buffer, uint32_t count)
{
    __asm__ volatile ("cld; rep insw"
                      : "+D"(buffer), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static ALWAYS_INLINE void outsw(uint16_t port, const void *buffer, uint32_t count)
{
    __asm__ volatile ("cld; rep outsw"
                      : "+S"(buffer), "+c"(count)
                      : "d"(port)
                      : "memory");
}

/*  io_wait: burn roughly one microsecond.
 *
 *  Chips from 1981 need time to settle between writes. The 8259 interrupt
 *  controller in particular will drop the second byte of an initialisation
 *  sequence if you write it too fast on a modern CPU, which executes the two
 *  `out` instructions several hundred times faster than the chip was designed
 *  for.
 *
 *  The traditional fix is a write to port 0x80, which is the POST diagnostic
 *  port: unused after boot, harmless to write, and an I/O bus cycle takes
 *  about a microsecond regardless of how fast the CPU is. It is a hack, it is
 *  what Linux does, and there is no better portable option.                   */
static ALWAYS_INLINE void io_wait(void)
{
    outb(0x80, 0);
}

/*  Interrupt flag control. `cli`/`sti` are the kernel's most dangerous pair of
 *  instructions: they are how we get atomicity on a single CPU, and forgetting
 *  an `sti` deadlocks the machine with no diagnostic at all.
 *
 *  irq_save/irq_restore exist because `cli; work; sti` is wrong whenever the
 *  caller might already have had interrupts off -- the `sti` would turn them
 *  on underneath code that was relying on them being off. Chapter 36 has the
 *  full story.                                                                */
static ALWAYS_INLINE void cli(void) { __asm__ volatile ("cli" ::: "memory"); }
static ALWAYS_INLINE void sti(void) { __asm__ volatile ("sti" ::: "memory"); }
static ALWAYS_INLINE void hlt(void) { __asm__ volatile ("hlt"); }

static ALWAYS_INLINE uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static ALWAYS_INLINE void irq_restore(uint32_t flags)
{
    __asm__ volatile ("pushl %0; popfl" :: "r"(flags) : "memory", "cc");
}

static ALWAYS_INLINE bool irqs_enabled(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0" : "=r"(flags));
    return (flags & 0x200) != 0;      /* EFLAGS bit 9 = IF */
}

#endif /* NIMBUS_IO_H */
