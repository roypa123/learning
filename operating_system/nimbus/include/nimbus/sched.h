/* ===========================================================================
 *  nimbus/include/nimbus/sched.h  --  who runs next
 * ===========================================================================
 *  Explained in: docs/31-scheduler.md, docs/35-blocking.md
 * =========================================================================== */
#ifndef NIMBUS_SCHED_H
#define NIMBUS_SCHED_H

#include <nimbus/types.h>
#include <nimbus/task.h>

#define SCHED_TIME_SLICE  5     /* ticks == 50 ms at 100 Hz                   */
#define PRIORITY_HIGH     0
#define PRIORITY_NORMAL   1
#define PRIORITY_LOW      2
#define PRIORITY_IDLE     3
#define PRIORITY_LEVELS   4

void  sched_init(void);

/*  Put a task on the run queue. Idempotent: adding a task that is already
 *  ready is a no-op rather than a corrupted list, because every wakeup path in
 *  the kernel would otherwise need to check first.                            */
void  sched_add(task_t *task);
void  sched_remove(task_t *task);

/*  Give up the CPU voluntarily. Returns when this task is scheduled again.    */
void  schedule(void);

/*  Called from the timer interrupt. Decrements the slice and sets the
 *  need-resched flag; the actual switch happens on the way out of the
 *  interrupt, never inside it. Chapter 31 explains why switching from inside
 *  an interrupt handler with a half-unwound stack is a trap.                  */
void  sched_tick(void);

/*  Block the current task on `channel`, and wake every task blocked on it.
 *  A "channel" is just an address used as a token -- typically the address of
 *  the thing being waited for, such as a device struct or a pipe. This is the
 *  sleep/wakeup interface from Unix V6 and it is still the clearest one.      */
void  sched_block(void *channel);
void  sched_wake(void *channel);
void  sched_wake_one(void *channel);

void  sleep_ms(uint32_t ms);

extern bool sched_enabled;

#endif /* NIMBUS_SCHED_H */
