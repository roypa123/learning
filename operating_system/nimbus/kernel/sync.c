/* ===========================================================================
 *  nimbus/kernel/sync.c  --  spinlocks, semaphores, mutexes
 * ===========================================================================
 *  Explained in: docs/36-synchronisation.md
 * =========================================================================== */

#include <nimbus/sync.h>
#include <nimbus/sched.h>
#include <nimbus/task.h>
#include <nimbus/kernel.h>
#include <nimbus/io.h>

/* ---------------------------------------------------------------------------
 *  Spinlocks
 *
 *  On one CPU, a spinlock that actually spins is a deadlock: the holder cannot
 *  release it while we are spinning, because we are the only CPU and we are
 *  not running the holder. So the entire mechanism reduces to disabling
 *  interrupts, which is the only concurrency that exists here.
 *
 *  Keeping the spinlock API anyway is not ceremony. It marks the places in the
 *  kernel that would need a real lock the day a second CPU appears, it gives
 *  those places a name that shows up in a panic, and it means the change to
 *  support SMP is confined to this file.
 * ------------------------------------------------------------------------- */

void spinlock_init(spinlock_t *lock, const char *name)
{
    lock->locked      = 0;
    lock->saved_flags = 0;
    lock->name        = name;
    lock->holder      = NULL;
}

void spin_lock(spinlock_t *lock)
{
    uint32_t flags = irq_save();

    /*  With interrupts off on a uniprocessor, finding the lock already held
     *  means *this* CPU took it and did not release it -- a genuine deadlock
     *  that would spin forever. Detecting it is free and the diagnostic is the
     *  difference between "the machine froze" and a file and line.            */
    if (lock->locked)
        panic("spin_lock(%s): already held by %p -- recursive acquisition",
              lock->name ? lock->name : "?", lock->holder);

    lock->locked      = 1;
    lock->holder      = current_task;
    lock->saved_flags = flags;
}

bool spin_trylock(spinlock_t *lock)
{
    uint32_t flags = irq_save();

    if (lock->locked) {
        irq_restore(flags);
        return false;
    }

    lock->locked      = 1;
    lock->holder      = current_task;
    lock->saved_flags = flags;
    return true;
}

void spin_unlock(spinlock_t *lock)
{
    if (!lock->locked)
        panic("spin_unlock(%s): not held", lock->name ? lock->name : "?");

    uint32_t flags = lock->saved_flags;

    lock->locked = 0;
    lock->holder = NULL;

    /*  Restore, do not blindly `sti`. If the caller already had interrupts
     *  disabled for its own reasons, turning them on here would break an
     *  invariant it is relying on -- and the resulting bug would appear in the
     *  caller's caller, with nothing to connect it to this line.              */
    irq_restore(flags);
}

/* ---------------------------------------------------------------------------
 *  Semaphores
 *
 *  A counter and a wait channel. `wait` decrements, blocking while the count
 *  is zero; `post` increments and wakes someone.
 *
 *  The subtlety is the loop around the sleep. When sem_post wakes us, the
 *  count is positive -- but by the time we are scheduled, another task may
 *  have taken it. So the woken task must re-check rather than assume, which is
 *  why this is `while (count == 0)` and never `if (count == 0)`. That
 *  distinction is the single most common concurrency bug in kernel code, and
 *  it is the reason condition variables are always documented with "always
 *  wait in a loop".
 * ------------------------------------------------------------------------- */

void sem_init(semaphore_t *sem, int32_t initial, const char *name)
{
    sem->count = initial;
    sem->name  = name;
}

void sem_wait(semaphore_t *sem)
{
    for (;;) {
        uint32_t flags = irq_save();

        if (sem->count > 0) {
            sem->count--;
            irq_restore(flags);
            return;
        }

        irq_restore(flags);
        sched_block(sem);            /* the semaphore's address is the channel */
    }
}

bool sem_trywait(semaphore_t *sem)
{
    uint32_t flags = irq_save();

    bool got = false;
    if (sem->count > 0) { sem->count--; got = true; }

    irq_restore(flags);
    return got;
}

void sem_post(semaphore_t *sem)
{
    uint32_t flags = irq_save();
    sem->count++;
    irq_restore(flags);

    /*  Wake one, not all. Waking everyone for a resource only one of them can
     *  have is the "thundering herd": n tasks are scheduled, n-1 find the
     *  count zero again and go back to sleep, and the wakeup cost is n times
     *  what it should be.                                                     */
    sched_wake_one(sem);
}

/* ---------------------------------------------------------------------------
 *  Mutexes
 * ------------------------------------------------------------------------- */

void mutex_init(mutex_t *m, const char *name)
{
    sem_init(&m->sem, 1, name);
    m->owner = NULL;
}

void mutex_lock(mutex_t *m)
{
    /*  Self-deadlock check. A mutex is not recursive: taking it twice from the
     *  same task blocks forever waiting for yourself. Some systems offer a
     *  recursive mutex; they are almost always a sign that the locking design
     *  is unclear, and they hide exactly this bug rather than fixing it.      */
    if (m->owner && m->owner == (void *)current_task)
        panic("mutex_lock(%s): already held by this task (pid %d)",
              m->sem.name ? m->sem.name : "?",
              current_task ? current_task->pid : -1);

    sem_wait(&m->sem);
    m->owner = (void *)current_task;
}

void mutex_unlock(mutex_t *m)
{
    if (m->owner != (void *)current_task)
        panic("mutex_unlock(%s): released by a task that does not hold it",
              m->sem.name ? m->sem.name : "?");

    m->owner = NULL;
    sem_post(&m->sem);
}
