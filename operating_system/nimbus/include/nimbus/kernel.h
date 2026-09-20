/* ===========================================================================
 *  nimbus/include/nimbus/kernel.h  --  printing, asserting, and dying
 * ===========================================================================
 *
 *  Every kernel needs three things before it can be developed at all: a way to
 *  print, a way to check an assumption, and a way to stop with an explanation
 *  instead of rebooting silently. This header is those three things.
 * =========================================================================== */
#ifndef NIMBUS_KERNEL_H
#define NIMBUS_KERNEL_H

#include <nimbus/types.h>

/* ---------------------------------------------------------------------------
 *  Formatted output
 *
 *  kprintf goes to the console *and* the serial port. That duplication is
 *  deliberate: the console is what you look at, and the serial log is what
 *  survives. When the machine triple-faults halfway through a line, the
 *  console shows a rebooting machine and the serial log shows the half line,
 *  which is usually enough to find the instruction.
 *
 *  Supported conversions: %d %i %u %x %X %p %s %c %% and %b (binary).
 *  Flags: `-` (left align), `0` (zero pad), a field width, and `l` (long,
 *  which on this target is the same as int -- accepted so that copied-in code
 *  compiles).
 * ------------------------------------------------------------------------- */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

int  snprintf(char *buf, size_t size, const char *fmt, ...)
         __attribute__((format(printf, 3, 4)));
int  vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/*  The `format(printf, n, m)` attribute makes GCC type-check our format
 *  strings exactly as it does for the real printf: passing a uint64_t to %d
 *  becomes a warning instead of two garbage columns of hex at 3 a.m.          */

/* ---------------------------------------------------------------------------
 *  Log levels
 *
 *  These write only to the serial port, not to the screen. A kernel that
 *  narrates every page fault onto a 25-line console has no room left to show
 *  you a shell -- but you still want the narration when something breaks, and
 *  a serial log costs nothing to read and nothing to ignore.
 * ------------------------------------------------------------------------- */
void klog(const char *level, const char *fmt, ...)
         __attribute__((format(printf, 2, 3)));

#define LOG_DEBUG(...) klog("dbg", __VA_ARGS__)
#define LOG_INFO(...)  klog("inf", __VA_ARGS__)
#define LOG_WARN(...)  klog("WRN", __VA_ARGS__)
#define LOG_ERR(...)   klog("ERR", __VA_ARGS__)

/* ---------------------------------------------------------------------------
 *  Dying well
 * ------------------------------------------------------------------------- */

/*  panic: print a message and a register dump, then stop forever.
 *
 *  It never returns, and telling GCC that with `noreturn` is not cosmetic: it
 *  lets the compiler know that the code after `panic(...)` is unreachable, so
 *  `if (!p) panic("null"); use(p);` does not warn about a possibly-null `p`.  */
void panic(const char *fmt, ...) NORETURN __attribute__((format(printf, 1, 2)));

/*  The same, but called from an exception handler that already has a register
 *  frame worth printing. Declared in isr.h once registers_t exists.           */

#define ASSERT(cond)                                                        \
    do {                                                                    \
        if (!(cond))                                                        \
            panic("assertion failed: %s\n  at %s:%d in %s()",               \
                  #cond, __FILE__, __LINE__, __func__);                     \
    } while (0)

/*  Unlike userland's assert, ours is always compiled in. A kernel assertion
 *  that fires in production tells you about corruption *before* it spreads to
 *  the disk; the same bug with assertions compiled out writes the corruption
 *  to a filesystem. The cost is a compare and a branch. Pay it.               */

/*  For the arm of a switch that cannot happen, or a stub not written yet.     */
#define UNREACHABLE()  panic("unreachable code at %s:%d", __FILE__, __LINE__)
#define TODO(what)     panic("not implemented: %s (%s:%d)", what, __FILE__, __LINE__)

/* ---------------------------------------------------------------------------
 *  Symbols the linker defines. Declared as arrays, never as pointers: the
 *  *address* of the symbol is the value we want. `extern char x[]` gives us
 *  that; `extern char *x` would make the compiler emit a load from that
 *  address, reading whatever code happens to be there as a pointer.
 * ------------------------------------------------------------------------- */
extern char __kernel_start[];
extern char __kernel_end[];
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __data_end[];
extern char __bss_start[], __bss_end[];

#endif /* NIMBUS_KERNEL_H */
