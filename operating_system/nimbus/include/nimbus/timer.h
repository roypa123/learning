/* ===========================================================================
 *  nimbus/include/nimbus/timer.h  --  the 8253/8254 programmable interval timer
 * ===========================================================================
 *  Explained in: docs/18-pit-timer.md
 * =========================================================================== */
#ifndef NIMBUS_TIMER_H
#define NIMBUS_TIMER_H

#include <nimbus/types.h>

#define PIT_CHANNEL0  0x40   /* wired to IRQ 0                                */
#define PIT_CHANNEL1  0x41   /* used for DRAM refresh on the original PC      */
#define PIT_CHANNEL2  0x42   /* wired to the PC speaker                       */
#define PIT_COMMAND   0x43

/*  The PIT counts down from a divisor at a fixed 1.193182 MHz. That number is
 *  1/3 of the NTSC colour burst frequency, because in 1981 the cheapest way to
 *  get a stable clock was to divide the one already on the board for the video
 *  output. Every PC since has kept it for compatibility.                      */
#define PIT_FREQUENCY 1193182u

/*  100 Hz: a 10 ms tick. Fast enough that a time slice feels instant, slow
 *  enough that the interrupt overhead is invisible. Linux used 100 Hz for
 *  years for the same reasons.                                                */
#define TIMER_HZ      100u

void     timer_init(uint32_t hz);

/*  Ticks since boot. 32 bits at 100 Hz wraps after 497 days; we use 64 so that
 *  the wrap is in the year 7 billion and nobody has to think about it again.  */
uint64_t timer_ticks(void);
uint64_t timer_ms(void);

/*  Busy-wait. Only legal before the scheduler exists (and in drivers that must
 *  not sleep); after Chapter 35, use sleep() instead, which yields the CPU.   */
void     timer_spin_ms(uint32_t ms);

/*  Set by the timer IRQ, read by the scheduler: has this task used up its
 *  slice? Chapter 31.                                                         */
extern volatile bool timer_need_resched;

#endif /* NIMBUS_TIMER_H */
