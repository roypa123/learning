/* ===========================================================================
 *  nimbus/kernel/printk.c  --  kprintf, klog, panic
 * ===========================================================================
 *
 *  The formatter lives in lib/printf.c and knows nothing about the kernel.
 *  This file supplies the three sinks that make it useful here: the screen,
 *  the serial log, and the last thing you ever see.
 *
 *  Explained in: docs/14-printf.md
 * =========================================================================== */

#define NIMBUS_KERNEL 1

#include <nimbus/kernel.h>
#include <nimbus/printf.h>
#include <nimbus/vga.h>
#include <nimbus/serial.h>
#include <nimbus/io.h>
#include <nimbus/string.h>
#include <nimbus/isr.h>
#include <nimbus/timer.h>

/*  Set once vga_init() has run. Before that, kprintf output goes only to the
 *  serial port -- which is exactly why serial_init() is the first line of
 *  kmain(). It means the very first thing the kernel does can be reported.    */
static bool console_ready = false;

void printk_enable_console(void) { console_ready = true; }

/* ---------------------------------------------------------------------------
 *  Sinks
 * ------------------------------------------------------------------------- */
static void sink_both(void *ctx UNUSED, char c)
{
    serial_putc(c);
    if (console_ready) vga_putc(c);
}

static void sink_serial(void *ctx UNUSED, char c)
{
    serial_putc(c);
}

void kvprintf(const char *fmt, va_list ap)
{
    fmt_vformat(sink_both, NULL, fmt, ap);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

/* ---------------------------------------------------------------------------
 *  klog -- serial only, with a timestamp
 *
 *  Two decisions worth defending.
 *
 *  Serial only, because a 25-line screen is a terrible log. Anything
 *  interesting scrolls away in under a second, and the one line you needed was
 *  the one pushed off the top. The screen is for the user; the log is for you.
 *
 *  A timestamp on every line, because the single most common kernel question
 *  is "what happened between these two events", and without timestamps you
 *  cannot tell a 200-microsecond gap from a 4-second one.
 * ------------------------------------------------------------------------- */
void klog(const char *level, const char *fmt, ...)
{
    uint64_t ms = timer_ms();

    /*  [    3.140] inf  message
     *
     *  The prefix goes through snprintf into a small stack buffer rather than
     *  through the streaming sink, because it needs its own argument list and
     *  a va_list cannot be conjured from nothing. 32 bytes is ample and the
     *  buffer never escapes this frame.
     *
     *  Field widths are chosen so the columns stay aligned for the first 27
     *  hours of uptime, after which they drift and nobody minds.              */
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "[%5u.%03u] %s  ",
             (uint32_t)(ms / 1000), (uint32_t)(ms % 1000), level);
    serial_puts(prefix);

    va_list ap;
    va_start(ap, fmt);
    fmt_vformat(sink_serial, NULL, fmt, ap);
    va_end(ap);

    sink_serial(NULL, '\n');
}

/* ---------------------------------------------------------------------------
 *  panic
 *
 *  What a kernel does instead of throwing. There is nobody to catch it, the
 *  machine is in a state we no longer understand, and the single most valuable
 *  thing we can do is stop immediately -- before the corruption reaches the
 *  disk -- and put everything we know on the screen.
 *
 *  The rules panic() follows, in order of importance:
 *
 *    1. Disable interrupts first. If the timer fires during the panic and the
 *       scheduler switches away, the message never finishes printing and the
 *       machine limps on in its broken state.
 *    2. Use as little machinery as possible. No kmalloc, no locks, no VFS.
 *       Everything panic touches is a thing that might be the reason we are
 *       panicking.
 *    3. Write to the serial port *first*, character by character. If the
 *       screen code is what is broken, the log still gets the message.
 *    4. Never return. `for(;;) hlt` with interrupts off, forever.
 * ------------------------------------------------------------------------- */
void panic(const char *fmt, ...)
{
    cli();

    serial_puts("\n\n*** KERNEL PANIC ***\n");

    va_list ap;
    va_start(ap, fmt);
    fmt_vformat(sink_serial, NULL, fmt, ap);
    va_end(ap);
    serial_putc('\n');

    if (console_ready) {
        /* A full-screen red banner. Unmistakable, and impossible to confuse
         * with ordinary output that happens to contain the word "panic". */
        vga_set_color(VGA_WHITE, VGA_RED);
        vga_clear();
        vga_puts("\n  *** KERNEL PANIC ***\n\n  ");

        va_list ap2;
        va_start(ap2, fmt);
        fmt_vformat(sink_both, NULL, fmt, ap2);
        va_end(ap2);

        vga_puts("\n\n  The system has been halted. Nothing further will run.\n");
        vga_puts("  The serial log holds everything that led up to this.\n");
    }

    for (;;)
        hlt();
}

/* ---------------------------------------------------------------------------
 *  A register dump, for exception handlers
 * ------------------------------------------------------------------------- */
void isr_dump_registers(registers_t *r)
{
    kprintf("  eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
            r->eax, r->ebx, r->ecx, r->edx);
    kprintf("  esi=%08x edi=%08x ebp=%08x esp=%08x\n",
            r->esi, r->edi, r->ebp, (uint32_t)r + sizeof(registers_t));
    kprintf("  eip=%08x cs=%04x eflags=%08x\n",
            r->eip, r->cs & 0xFFFF, r->eflags);
    kprintf("  int=%u err=%08x ds=%04x\n",
            r->int_no, r->err_code, r->ds & 0xFFFF);

    /* Only print the user stack fields when they are real. The CPU pushes
     * SS:ESP only on a privilege change, so for a fault in kernel mode those
     * two slots hold whatever was on the stack already, and printing them
     * invents evidence. */
    if ((r->cs & 3) == 3)
        kprintf("  user esp=%08x ss=%04x  (fault came from ring 3)\n",
                r->useresp, r->ss & 0xFFFF);
    else
        kprintf("  (fault came from ring 0; no user stack was pushed)\n");
}
