/* ===========================================================================
 *  nimbus/kernel/sched.c  --  who runs next
 * ===========================================================================
 *
 *  Round robin with four priority levels. Pick the highest-priority runnable
 *  task; among equals, take the one after the current one, so that nobody
 *  starves within a level. The idle task sits alone at the lowest level and is
 *  chosen only when nothing else can run.
 *
 *  This is about as simple as a scheduler can be while still being a
 *  scheduler, and it has one honest flaw worth naming up front: a CPU-bound
 *  task at PRIORITY_NORMAL will starve a CPU-bound task at PRIORITY_LOW
 *  forever. Real schedulers fix that with ageing (a task that has waited too
 *  long gets promoted) or with virtual time (CFS gives everyone a fair share
 *  of a virtual clock rather than a fixed slice). Chapter 31 implements ageing
 *  as an exercise, in about fifteen lines.
 *
 *  Explained in: docs/31-scheduler.md, docs/35-blocking.md
 *  Line by line: docs/line-by-line/nimbus-sched.md
 * =========================================================================== */

#include <nimbus/sched.h>
#include <nimbus/task.h>
#include <nimbus/timer.h>
#include <nimbus/gdt.h>
#include <nimbus/paging.h>
#include <nimbus/kernel.h>
#include <nimbus/io.h>

bool sched_enabled = false;

static uint32_t switches = 0;

void sched_init(void)
{
    sched_enabled = true;
    LOG_INFO("sched: round robin, %u tick slices, %u priority levels",
             (uint32_t)SCHED_TIME_SLICE, (uint32_t)PRIORITY_LEVELS);
}

/*  Our "run queue" is the process table scanned in order. A real kernel keeps
 *  an actual list per priority so that picking is O(1) instead of O(MAX_TASKS)
 *  -- with 64 slots the scan costs a few hundred cycles and buys a data
 *  structure you can read without a diagram.                                  */
void sched_add(task_t *t)
{
    if (t->state == TASK_READY || t->state == TASK_RUNNING) return;
    t->state = TASK_READY;
}

void sched_remove(task_t *t)
{
    if (t->state == TASK_READY || t->state == TASK_RUNNING)
        t->state = TASK_BLOCKED;
}

/* ---------------------------------------------------------------------------
 *  pick_next -- the policy, in fifteen lines
 * ------------------------------------------------------------------------- */
static task_t *pick_next(void)
{
    task_t  *tasks = task_table();
    int      start = (int)(current_task - tasks);
    task_t  *best  = NULL;

    /*  Start the scan *after* the current task so that equal-priority tasks
     *  take turns. Starting at zero every time would mean the lowest-numbered
     *  runnable task runs forever -- a bug that looks like "the scheduler
     *  works" right up until you have two CPU-bound processes.                */
    for (int i = 1; i <= MAX_TASKS; i++) {
        task_t *t = &tasks[(start + i) % MAX_TASKS];

        if (t->state != TASK_READY && t->state != TASK_RUNNING) continue;
        if (t->state == TASK_RUNNING && t != current_task)      continue;

        if (!best || t->priority < best->priority)
            best = t;
    }

    /*  There is always at least one candidate: the idle task, which is never
     *  blocked. If this fires, something marked task 0 as not runnable.       */
    if (!best) panic("sched: nothing is runnable, not even idle");
    return best;
}

/* ---------------------------------------------------------------------------
 *  schedule -- the switch
 *
 *  Interrupts are disabled for the whole of it. The window between "choose the
 *  next task" and "switch to it" must be atomic: an interrupt in the middle
 *  could block the task we just chose, and we would switch to a task that is
 *  no longer runnable.
 *
 *  The flags are saved and restored around the switch rather than blindly
 *  re-enabled, because schedule() is called both from ordinary code (where
 *  interrupts were on) and from the tail of an interrupt handler (where they
 *  were off). Note that the restore happens *after* the switch returns -- and
 *  the switch returns in a different task, whose saved flags are its own. That
 *  is not a bug; it is how each task keeps its own interrupt state across a
 *  switch.
 * ------------------------------------------------------------------------- */
void schedule(void)
{
    if (!sched_enabled) return;

    uint32_t flags = irq_save();

    task_t *prev = current_task;
    task_t *next = pick_next();

    if (next == prev) {
        /*  Nothing better to run. Refresh the slice so that the next tick does
         *  not immediately try again.                                         */
        prev->time_slice = SCHED_TIME_SLICE;
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        irq_restore(flags);
        return;
    }

    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    next->state      = TASK_RUNNING;
    next->time_slice = SCHED_TIME_SLICE;

    current_task = next;
    switches++;

    /*  Two things must be updated before the switch, in this order.
     *
     *  CR3 first: after this instruction we are running in the new task's
     *  address space. That is safe only because the kernel half of every
     *  directory is identical, so the code we are executing, the stack we are
     *  on and the task structs we are touching are all still mapped. This is
     *  the payoff for sharing the kernel's page tables in
     *  paging_new_directory(), and it is why a kernel that copies them instead
     *  crashes exactly here.
     *
     *  Then esp0: the next time this task traps in from ring 3, the CPU will
     *  read the TSS to find a ring 0 stack. Update it now, while we still know
     *  which task is about to run.
     */
    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);

    tss_set_kernel_stack(next->kernel_stack);

    switch_context(&prev->context, next->context);

    /*  Execution resumes here when *prev* is scheduled again -- possibly
     *  seconds later, possibly in a different address space, and with
     *  `current_task` once more pointing at prev.                             */
    irq_restore(flags);
}

/* ---------------------------------------------------------------------------
 *  The timer tick
 *
 *  Runs in interrupt context, so it does the bookkeeping and nothing else.
 *  The actual switch happens in interrupt_dispatch, after the EOI has been
 *  sent -- if we switched first, the PIC would still be waiting for the
 *  acknowledgement of this interrupt while an entirely different task ran, and
 *  no further timer interrupt would be delivered until that task happened to
 *  be scheduled back in. The system would appear to hang at random.
 * ------------------------------------------------------------------------- */
void sched_tick(void)
{
    if (!sched_enabled) return;

    uint64_t now = timer_ticks();
    task_t  *tasks = task_table();

    /*  Wake anything whose sleep has expired.                                 */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }

    if (current_task) {
        current_task->ticks_used++;

        if (current_task->time_slice > 0)
            current_task->time_slice--;

        if (current_task->time_slice == 0)
            timer_need_resched = true;
    }
}

/* ---------------------------------------------------------------------------
 *  Blocking and waking
 *
 *  A "wait channel" is any address used as a token. Nothing is stored at it;
 *  it is compared for equality and that is all. Using the address of the thing
 *  you are waiting for -- a semaphore, a device's buffer, a parent's task
 *  struct -- makes the pairing between sleeper and waker impossible to get
 *  wrong, because both sides name the same object.
 *
 *  This is the sleep/wakeup interface from Unix Sixth Edition. It is fifty
 *  years old, it is four lines, and nothing has improved on it for a kernel
 *  this size.
 * ------------------------------------------------------------------------- */
void sched_block(void *channel)
{
    if (!sched_enabled) {
        /*  Before the scheduler exists there is nobody to switch to, so the
         *  honest thing is to wait for an interrupt. `sti; hlt` is the correct
         *  pair and the order matters: `hlt` with interrupts disabled is a
         *  permanent stop.                                                    */
        __asm__ volatile ("sti; hlt");
        return;
    }

    uint32_t flags = irq_save();

    current_task->state        = TASK_BLOCKED;
    current_task->wait_channel = channel;

    irq_restore(flags);
    schedule();
}

void sched_wake(void *channel)
{
    uint32_t flags = irq_save();
    task_t  *tasks = task_table();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == channel) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
        }
    }

    irq_restore(flags);
}

void sched_wake_one(void *channel)
{
    uint32_t flags = irq_save();
    task_t  *tasks = task_table();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == channel) {
            tasks[i].state        = TASK_READY;
            tasks[i].wait_channel = NULL;
            break;
        }
    }

    irq_restore(flags);
}

void sleep_ms(uint32_t ms)
{
    if (!sched_enabled) { timer_spin_ms(ms); return; }

    uint32_t flags = irq_save();

    current_task->wake_tick = timer_ticks() + ((uint64_t)ms * TIMER_HZ) / 1000 + 1;
    current_task->state     = TASK_SLEEPING;

    irq_restore(flags);
    schedule();
}
