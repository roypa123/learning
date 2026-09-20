/* ===========================================================================
 *  nimbus/include/nimbus/sync.h  --  keeping two tasks out of each other's way
 * ===========================================================================
 *
 *  Nimbus runs on one CPU, which removes half of the concurrency problem and
 *  leaves the other half firmly in place. Two tasks never execute at the same
 *  instant -- but a task can be preempted between any two instructions, and an
 *  interrupt handler can run between any two instructions, including in the
 *  middle of a read-modify-write on a variable the handler also touches.
 *
 *  So we need three tools, and they are not interchangeable:
 *
 *    irq_save/restore   the only thing that works against an interrupt
 *                       handler. Cheap, and it stops *everything*, so it must
 *                       be held for a handful of instructions at most.
 *
 *    spinlock           on one CPU this is just irq disabling with a name and
 *                       a debug check. It is here so that the code reads the
 *                       way it would on a multiprocessor, and so that the day
 *                       you add a second CPU you change spinlock.c and nothing
 *                       else.
 *
 *    mutex/semaphore    for code that can sleep. A task that cannot get the
 *                       lock blocks and the CPU goes to someone else. Never
 *                       usable from an interrupt handler, which has no task to
 *                       put to sleep.
 *
 *  Explained in: docs/36-synchronisation.md
 * =========================================================================== */
#ifndef NIMBUS_SYNC_H
#define NIMBUS_SYNC_H

#include <nimbus/types.h>
#include <nimbus/io.h>

/* ---------------------------------------------------------------------------
 *  Atomics
 *
 *  `lock xchg` is atomic against interrupts and against other cores. On a
 *  uniprocessor the LOCK prefix is redundant, but it costs one cycle and it
 *  makes the intent explicit, so we keep it.
 * ------------------------------------------------------------------------- */
static ALWAYS_INLINE uint32_t atomic_xchg(volatile uint32_t *ptr, uint32_t value)
{
    __asm__ volatile ("lock xchgl %0, %1"
                      : "+r"(value), "+m"(*ptr)
                      :: "memory");
    return value;
}

static ALWAYS_INLINE void atomic_inc(volatile uint32_t *ptr)
{
    __asm__ volatile ("lock incl %0" : "+m"(*ptr) :: "memory", "cc");
}

static ALWAYS_INLINE bool atomic_dec_and_test(volatile uint32_t *ptr)
{
    bool zero;
    __asm__ volatile ("lock decl %0; sete %1"
                      : "+m"(*ptr), "=q"(zero) :: "memory", "cc");
    return zero;
}

/* ---------------------------------------------------------------------------
 *  Spinlocks
 * ------------------------------------------------------------------------- */
typedef struct spinlock {
    volatile uint32_t locked;
    uint32_t          saved_flags;
    const char       *name;
    void             *holder;      /* the task that holds it, for diagnostics */
} spinlock_t;

#define SPINLOCK_INIT(nm) { 0, 0, (nm), NULL }

void spinlock_init(spinlock_t *lock, const char *name);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
bool spin_trylock(spinlock_t *lock);

/* ---------------------------------------------------------------------------
 *  Sleeping locks
 * ------------------------------------------------------------------------- */
typedef struct semaphore {
    volatile int32_t count;
    const char      *name;
} semaphore_t;

void sem_init(semaphore_t *sem, int32_t initial, const char *name);
void sem_wait(semaphore_t *sem);      /* P / down: block while count == 0     */
void sem_post(semaphore_t *sem);      /* V / up:   increment and wake a waiter */
bool sem_trywait(semaphore_t *sem);

/*  A mutex is a semaphore with a count of 1 and an owner check. The owner
 *  check is what turns "the system froze" into "task 4 tried to take a mutex
 *  it already holds, at fat16.c:212".                                         */
typedef struct mutex {
    semaphore_t sem;
    void       *owner;
} mutex_t;

void mutex_init(mutex_t *m, const char *name);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* NIMBUS_SYNC_H */
