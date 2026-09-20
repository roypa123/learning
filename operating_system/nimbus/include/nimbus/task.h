/* ===========================================================================
 *  nimbus/include/nimbus/task.h  --  processes
 * ===========================================================================
 *  Explained in: docs/29-what-is-a-process.md, docs/30-context-switch.md,
 *                docs/34-fork-exec.md
 * =========================================================================== */
#ifndef NIMBUS_TASK_H
#define NIMBUS_TASK_H

#include <nimbus/types.h>
#include <nimbus/paging.h>

#define MAX_TASKS        64
#define MAX_FDS          16
#define TASK_NAME_LEN    32
#define KERNEL_STACK_SIZE 8192      /* 2 pages per task                       */

/*  Where a user program's pieces go in its own address space. These are
 *  choices, not hardware constraints -- but they are the traditional ones, and
 *  the reasons are worth knowing:
 *
 *  Code at 0x08048000 rather than 0: leaving the first 128 MiB unmapped means
 *  dereferencing a null pointer, or a small offset from one (`p->field` where
 *  p is NULL), faults instead of reading real memory.
 *
 *  Stack at the top of the user half, growing down, so that the heap growing
 *  up and the stack growing down have the maximum distance between them.      */
#define USER_CODE_BASE   0x08048000u
#define USER_STACK_TOP   0xBFFFF000u
#define USER_STACK_SIZE  (64 * KiB)
#define USER_HEAP_BASE   0x40000000u

typedef enum task_state {
    TASK_UNUSED = 0,   /* this slot is free                                   */
    TASK_EMBRYO,       /* being created; not yet runnable                     */
    TASK_READY,        /* runnable, waiting for the CPU                       */
    TASK_RUNNING,      /* on the CPU right now                                */
    TASK_BLOCKED,      /* waiting for something: I/O, a child, a lock         */
    TASK_SLEEPING,     /* waiting for a deadline in ticks                     */
    TASK_ZOMBIE        /* exited, but the parent has not collected the status */
} task_state_t;

struct vfs_node;
struct file;

/*  The context we save and restore across a switch.
 *
 *  Only six registers, not sixteen. The other ten are saved by the *calling
 *  convention*: the System V ABI says a function may clobber EAX, ECX and EDX,
 *  so anyone who called switch_context() has already spilled anything it cared
 *  about in those. We only need the callee-saved set, plus ESP and EIP, which
 *  are the switch itself.
 *
 *  This is why a context switch is twenty instructions and not a hundred, and
 *  why the field order here must match switch.s exactly.                      */
typedef struct context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;      /* where to resume: the return address of the switch   */
} context_t;

typedef struct task {
    /* --- identity ------------------------------------------------------- */
    pid_t             pid;
    pid_t             ppid;
    char              name[TASK_NAME_LEN];
    task_state_t      state;

    /* --- what the scheduler needs ---------------------------------------- */
    context_t        *context;       /* saved ESP, pointing at a context_t    */
    uint32_t          kernel_stack;  /* top of this task's ring 0 stack       */
    uint32_t          priority;      /* 0 = highest                           */
    uint32_t          time_slice;    /* ticks left in this slice              */
    uint64_t          wake_tick;     /* for TASK_SLEEPING                     */
    void             *wait_channel;  /* for TASK_BLOCKED: what we wait on     */
    int               exit_status;

    /* --- address space ---------------------------------------------------- */
    page_directory_t *directory;
    vaddr_t           brk;           /* top of the user heap, for sbrk()      */
    vaddr_t           user_stack_bottom;

    /* --- open files -------------------------------------------------------- */
    struct file      *fds[MAX_FDS];
    struct vfs_node  *cwd;

    /* --- bookkeeping ------------------------------------------------------- */
    uint64_t          ticks_used;
    struct task      *next;          /* the run queue is a linked list         */
} task_t;

extern task_t *current_task;

void    task_init(void);

/*  Create a task that runs a C function in ring 0. Used for the idle task and
 *  for kernel worker threads; a user process is made with task_fork/task_exec. */
task_t *task_spawn_kernel(const char *name, void (*entry)(void));

/*  Build a user process from an executable, with no parent to copy from. Used
 *  exactly once, for pid 1; everything after it arrives via fork + exec.      */
task_t *task_spawn_user(const char *path);

/*  The classic three. fork() returns twice -- 0 in the child, the child's pid
 *  in the parent -- which is the strangest interface in Unix and Chapter 34
 *  explains exactly how the second return is manufactured.                    */
pid_t   task_fork(void);
int     task_exec(const char *path, char *const argv[]);

/*  The real implementations. Both need the trap frame of the system call that
 *  invoked them -- fork to copy it for the child, exec to rewrite it so that
 *  the `iret` at the end of the syscall lands in the new program. The two
 *  wrappers above exist only to give a clear panic if something calls them
 *  from a context that has no trap frame.                                     */
struct registers;
pid_t   task_fork_regs(struct registers *regs);
int     task_exec_regs(struct registers *regs, const char *path, char *const argv[]);

/*  The process table, for the scheduler and for `ps`.                         */
task_t *task_table(void);
void    task_exit(int status) NORETURN;
pid_t   task_wait(int *status);

task_t *task_find(pid_t pid);
void    task_dump_all(void);

/*  Implemented in boot/switch.s.                                              */
void    switch_context(context_t **old_context, context_t *new_context);
void    enter_usermode(uint32_t entry, uint32_t user_stack) NORETURN;

#endif /* NIMBUS_TASK_H */
