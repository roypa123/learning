/* ===========================================================================
 *  nimbus/kernel/timer.c  --  the heartbeat
 * ===========================================================================
 *
 *  Until now the kernel has been entirely reactive: it runs when something
 *  calls it. The timer is the first thing that makes it happen *on its own*,
 *  and everything that follows -- preemption, sleeping, timeouts, the very
 *  idea that a program can be interrupted against its will -- is built on this
 *  one interrupt arriving a hundred times a second.
 *
 *  Explained in: docs/18-pit-timer.md
 * =========================================================================== */

#include <nimbus/timer.h>
#include <nimbus/irq.h>
#include <nimbus/isr.h>
#include <nimbus/io.h>
#include <nimbus/kernel.h>
#include <nimbus/sched.h>

static volatile uint64_t ticks = 0;
static uint32_t          hz    = TIMER_HZ;

volatile bool timer_need_resched = false;

/* ---------------------------------------------------------------------------
 *  The interrupt handler
 *
 *  Three lines, and every one of them is load-bearing.
 *
 *  It runs with interrupts disabled (an interrupt gate cleared IF) and it must
 *  finish fast: at 100 Hz there are 10 milliseconds between ticks, and a
 *  handler that takes longer than that means the next tick arrives before this
 *  one is acknowledged, which the PIC handles by dropping it. Time then runs
 *  slow, silently.
 * ------------------------------------------------------------------------- */
static void timer_callback(registers_t *regs UNUSED)
{
    ticks++;

    /*  Tell the scheduler a tick happened. It decrements the running task's
     *  time slice and, if the slice is used up, sets timer_need_resched.
     *
     *  What it does NOT do is switch tasks. We are on an interrupt stack
     *  frame that isr_common_stub is going to unwind with `iret`; switching
     *  away now means that `iret` would run on a different task's stack.
     *  The switch happens later, at a place that chose to be switched.        */
    sched_tick();
}

/* ---------------------------------------------------------------------------
 *  Programming the chip
 *
 *  The command byte at port 0x43:
 *
 *      bits 7-6   which channel (00 = channel 0)
 *      bits 5-4   access mode (11 = write low byte then high byte)
 *      bits 3-1   operating mode (011 = square wave generator)
 *      bit  0     BCD instead of binary (0 = binary; nobody uses BCD)
 *
 *  0x36 = 00 11 011 0.
 *
 *  Mode 3, the square wave generator, is the right choice for a periodic tick:
 *  the counter reloads itself automatically, so the interrupt repeats forever
 *  with no help from us. Mode 0 fires once and stops, which is what you would
 *  use for a one-shot timeout.
 * ------------------------------------------------------------------------- */
void timer_init(uint32_t frequency)
{
    hz = frequency;

    uint32_t divisor = PIT_FREQUENCY / frequency;

    /*  The divisor is 16 bits. Ask for anything below 19 Hz and it overflows;
     *  ask for more than 1193182 Hz and it underflows to zero, which the chip
     *  interprets as 65536 -- the slowest possible rate. Both failures present
     *  as "the timer runs at completely the wrong speed", so check here.      */
    if (divisor == 0)      divisor = 1;
    if (divisor > 0xFFFF)  divisor = 0xFFFF;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));         /* low byte  */
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));  /* high byte */

    /*  The actual frequency is not exactly what was asked for: 1193182/100 is
     *  11931.82, which truncates to 11931, giving 100.0069 Hz. Over a day that
     *  is six seconds of drift. Real kernels correct for it against the RTC or
     *  the TSC; we log it and move on, because six seconds a day is not going
     *  to confuse anything in this book.                                      */
    uint32_t actual_mhz = PIT_FREQUENCY * 1000 / divisor;
    LOG_INFO("pit: divisor %u -> %u.%03u Hz (asked for %u)",
             divisor, actual_mhz / 1000, actual_mhz % 1000, frequency);

    irq_register(IRQ_TIMER, timer_callback);
}

uint64_t timer_ticks(void)
{
    /*  A 64-bit read on a 32-bit machine is two loads, and the timer interrupt
     *  can land between them -- returning a value where the low half has
     *  wrapped but the high half has not, which is off by four billion.
     *
     *  At 100 Hz the low half wraps every 497 days, so the window is
     *  vanishingly rare and catastrophic when it hits. Reading twice and
     *  retrying if the high half changed costs two instructions and removes
     *  the bug entirely. This is the standard "sequence lock" pattern in
     *  miniature.                                                             */
    uint64_t t;
    do {
        t = ticks;
    } while (t != ticks);
    return t;
}

uint64_t timer_ms(void)
{
    return timer_ticks() * 1000 / hz;
}

void timer_spin_ms(uint32_t ms)
{
    /*  A busy wait. Legal only before the scheduler exists, and inside drivers
     *  that must not sleep. Note it reads timer_ticks() rather than caching --
     *  and note that if interrupts are disabled, ticks never advances and this
     *  hangs forever. That has caught everyone at least once.                 */
    uint64_t target = timer_ticks() + ((uint64_t)ms * hz) / 1000 + 1;
    while (timer_ticks() < target)
        __asm__ volatile ("pause");
}
