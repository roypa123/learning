/* ===========================================================================
 *  nimbus/kernel/task.c  --  processes
 * ===========================================================================
 *
 *  A process is four things and nothing more:
 *
 *      an address space        which page directory CR3 points at
 *      a kernel stack          where its state lives while it is in the kernel
 *      a saved context         five registers, so it can be resumed
 *      some bookkeeping        pid, parent, open files, exit status
 *
 *  Everything else people say about processes -- isolation, concurrency, the
 *  illusion of having the machine to yourself -- is an emergent property of
 *  those four plus a timer interrupt.
 *
 *  Explained in: docs/29-what-is-a-process.md, docs/34-fork-exec.md
 *  Line by line: docs/line-by-line/nimbus-task.md
 * =========================================================================== */

#define NIMBUS_KERNEL 1

#include <nimbus/task.h>
#include <nimbus/sched.h>
#include <nimbus/paging.h>
#include <nimbus/pmm.h>
#include <nimbus/heap.h>
#include <nimbus/gdt.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/io.h>
#include <nimbus/isr.h>
#include <nimbus/elf.h>
#include <nimbus/vfs.h>
#include <nimbus/fs.h>
#include <nimbus/syscall.h>

task_t *current_task = NULL;

static task_t  tasks[MAX_TASKS];
static pid_t   next_pid = 0;

/*  The label inside isr_common_stub that unwinds a trap frame and irets. A
 *  forked child's first instruction.                                          */
extern void isr_return(void);

/* ---------------------------------------------------------------------------
 *  Slot allocation
 * ------------------------------------------------------------------------- */
static task_t *task_alloc(void)
{
    uint32_t flags = irq_save();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            memset(&tasks[i], 0, sizeof(task_t));
            tasks[i].state = TASK_EMBRYO;
            tasks[i].pid   = next_pid++;
            irq_restore(flags);
            return &tasks[i];
        }
    }

    irq_restore(flags);
    return NULL;
}

task_t *task_find(pid_t pid)
{
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid)
            return &tasks[i];
    return NULL;
}

/* ---------------------------------------------------------------------------
 *  task_init -- adopt the context we are already running in
 *
 *  We are executing on the boot stack, in the kernel's address space, with no
 *  task struct. Rather than create a task and switch to it -- which needs a
 *  context to switch *from* -- we simply declare that what is already running
 *  is task 0. Its saved context will be written the first time it is switched
 *  away from, which is all a saved context is for.
 *
 *  Task 0 becomes the idle task: when nothing else is runnable, the scheduler
 *  picks it, and it executes `hlt` in a loop.
 * ------------------------------------------------------------------------- */
void task_init(void)
{
    memset(tasks, 0, sizeof(tasks));

    task_t *t = task_alloc();
    ASSERT(t != NULL && t->pid == 0);

    strlcpy(t->name, "idle", TASK_NAME_LEN);
    t->state        = TASK_RUNNING;
    t->directory    = kernel_directory;
    t->priority     = PRIORITY_IDLE;
    t->time_slice   = SCHED_TIME_SLICE;
    t->ppid         = 0;

    /*  The boot stack from boot.asm. We record its top so that tss.esp0 has
     *  something valid before the first real task exists -- an interrupt from
     *  ring 3 must never find a stale esp0.                                   */
    extern char stack_top[];
    t->kernel_stack = (uint32_t)stack_top;

    current_task = t;
    tss_set_kernel_stack(t->kernel_stack);

    LOG_INFO("task: pid 0 (idle) adopted, kernel stack at %08x", t->kernel_stack);
}

/* ---------------------------------------------------------------------------
 *  Kernel threads
 *
 *  A task that runs a C function in ring 0 and shares the kernel address
 *  space. Used for the idle task and for anything that wants to block -- a
 *  driver waiting on a device, say -- without a userland process behind it.
 *
 *  The interesting part is manufacturing a stack for a task that has never
 *  run. switch_context() will restore four registers and then `ret`, so we lay
 *  out exactly what those instructions expect to find:
 *
 *      high address   ... top of the freshly allocated stack
 *                     kernel_thread_exit   <- where `entry` returns to
 *                     entry                <- popped by switch_context's `ret`
 *                     0                       ebp
 *                     0                       ebx
 *                     0                       esi
 *      low address    0                       edi   <- context points here
 *
 *  The first switch to this task therefore pops four zeros and returns to
 *  `entry`, on a stack that looks exactly as though `entry` had been called
 *  normally. There is no special case anywhere in the scheduler for a task
 *  that has not started yet, because there does not need to be.
 * ------------------------------------------------------------------------- */
static void kernel_thread_exit(void)
{
    /*  A kernel thread whose function returned. Give it the same fate as a
     *  user process that fell off the end of main.                            */
    task_exit(0);
}

task_t *task_spawn_kernel(const char *name, void (*entry)(void))
{
    task_t *t = task_alloc();
    if (!t) return NULL;

    strlcpy(t->name, name, TASK_NAME_LEN);

    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (!stack) { t->state = TASK_UNUSED; return NULL; }

    t->kernel_stack = (uint32_t)(stack + KERNEL_STACK_SIZE);

    uint32_t *sp = (uint32_t *)t->kernel_stack;
    *--sp = (uint32_t)kernel_thread_exit;
    *--sp = (uint32_t)entry;
    *--sp = 0;                                  /* ebp */
    *--sp = 0;                                  /* ebx */
    *--sp = 0;                                  /* esi */
    *--sp = 0;                                  /* edi */

    t->context    = (context_t *)sp;
    t->directory  = kernel_directory;
    t->priority   = PRIORITY_NORMAL;
    t->time_slice = SCHED_TIME_SLICE;
    t->ppid       = current_task ? current_task->pid : 0;
    t->state      = TASK_READY;

    sched_add(t);
    LOG_INFO("task: pid %d (%s) is a kernel thread", t->pid, t->name);
    return t;
}

/* ---------------------------------------------------------------------------
 *  fork
 *
 *  The child is a copy of the parent that differs in exactly one observable
 *  way: fork() returns 0 in it. Everything else -- registers, open files,
 *  memory contents, the instruction it resumes at -- is identical.
 *
 *  Producing "returns twice" is the trick. The parent returns through the
 *  normal syscall path with the child's pid in EAX. The child has no syscall
 *  in progress at all, so we manufacture one: copy the parent's trap frame
 *  onto the child's fresh kernel stack, set EAX to 0 in the copy, and point
 *  the child's saved context at isr_return, the tail of the interrupt stub.
 *  When the scheduler first picks the child, it unwinds a trap frame it never
 *  pushed and irets to the instruction after the parent's `int 0x80`.
 * ------------------------------------------------------------------------- */
pid_t task_fork_regs(registers_t *regs)
{
    task_t *parent = current_task;
    task_t *child  = task_alloc();
    if (!child) return -EAGAIN;

    strlcpy(child->name, parent->name, TASK_NAME_LEN);
    child->ppid     = parent->pid;
    child->priority = parent->priority;
    child->brk      = parent->brk;
    child->user_stack_bottom = parent->user_stack_bottom;

    child->directory = paging_clone_directory(parent->directory);
    if (!child->directory) { child->state = TASK_UNUSED; return -ENOMEM; }

    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (!stack) {
        paging_free_directory(child->directory);
        child->state = TASK_UNUSED;
        return -ENOMEM;
    }
    child->kernel_stack = (uint32_t)(stack + KERNEL_STACK_SIZE);

    /*  The trap frame copy, at the very top of the child's kernel stack. It
     *  must be at the top, because tss.esp0 will point there and the next
     *  interrupt the child takes will push below it.                          */
    registers_t *child_frame =
        (registers_t *)(child->kernel_stack - sizeof(registers_t));
    memcpy(child_frame, regs, sizeof(registers_t));
    child_frame->eax = 0;                       /* <- fork() returns 0 here */

    uint32_t *sp = (uint32_t *)child_frame;
    *--sp = (uint32_t)isr_return;               /* popped by switch_context's ret */
    *--sp = 0;                                  /* ebp */
    *--sp = 0;                                  /* ebx */
    *--sp = 0;                                  /* esi */
    *--sp = 0;                                  /* edi */
    child->context = (context_t *)sp;

    /*  Open files are shared, not copied: the *file* object is refcounted and
     *  both processes point at the same one, so they share the read offset.
     *  That is what makes `(echo a; echo b) > f` work -- and it is why a
     *  naive implementation that copies the offset produces a file containing
     *  only "b".                                                              */
    for (int i = 0; i < MAX_FDS; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i]) file_ref(child->fds[i]);
    }
    child->cwd = parent->cwd;

    child->time_slice = SCHED_TIME_SLICE;
    child->state      = TASK_READY;
    sched_add(child);

    return child->pid;
}

pid_t task_fork(void)
{
    /*  fork() without a trap frame has nothing to copy. Kernel threads use
     *  task_spawn_kernel instead.                                             */
    panic("task_fork(): called outside a system call");
}

/* ---------------------------------------------------------------------------
 *  exec
 *
 *  Keep the process, replace the program. The pid, the parent, the open file
 *  descriptors and the current directory all survive; the address space does
 *  not.
 *
 *  This is why Unix has two calls where other systems have one
 *  (`CreateProcess`, `spawn`). Everything a shell does between fork and exec
 *  -- redirecting stdout, closing descriptors, changing directory -- is
 *  ordinary code running in the child, needing no support from exec at all.
 *  Two simple calls compose into every case that a single complicated call has
 *  to enumerate as parameters.
 * ------------------------------------------------------------------------- */
int task_exec_regs(registers_t *regs, const char *path, char *const argv[])
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -ENOENT;
    if (node->flags & VFS_DIRECTORY) return -EISDIR;

    /*  Read the whole image into kernel memory first. Demand paging an
     *  executable straight off the disk is what a real kernel does and it
     *  needs the page fault handler to know how to find the file; Chapter 44
     *  describes the upgrade. Reading it up front means the old address space
     *  is not destroyed until we know the new program is loadable.            */
    uint8_t *image = (uint8_t *)kmalloc(node->length);
    if (!image) return -ENOMEM;

    ssize_t got = vfs_read(node, 0, node->length, image);
    if (got < 0 || (size_t)got != node->length) { kfree(image); return -EIO; }

    if (!elf_validate(image, node->length)) { kfree(image); return -ENOEXEC; }

    /* ---- Copy the arguments somewhere safe -----------------------------------
     * argv points into the *old* address space, which is about to cease
     * existing. Copy the strings into kernel memory before that happens.
     */
    char  *argv_copy[32];
    int    argc = 0;
    size_t argv_bytes = 0;

    while (argv && argv[argc] && argc < 31) {
        size_t len = strlen(argv[argc]) + 1;
        argv_copy[argc] = (char *)kmalloc(len);
        if (!argv_copy[argc]) break;
        memcpy(argv_copy[argc], argv[argc], len);
        argv_bytes += len;
        argc++;
    }
    argv_copy[argc] = NULL;

    /* ---- Build the new address space ----------------------------------------- */
    page_directory_t *old_dir = current_task->directory;
    page_directory_t *new_dir = paging_new_directory();
    if (!new_dir) { kfree(image); return -ENOMEM; }

    vaddr_t  brk = 0;
    uint32_t entry = elf_load(new_dir, image, node->length, &brk);
    kfree(image);

    if (!entry) { paging_free_directory(new_dir); return -ENOEXEC; }

    /*  One page of user stack to begin with; the page fault handler grows it
     *  downwards on demand.                                                   */
    vaddr_t stack_bottom = USER_STACK_TOP - PAGE_SIZE;
    paging_map_range(new_dir, stack_bottom, PAGE_SIZE, PTE_WRITABLE | PTE_USER);

    /*  Switch now, so that we can write the arguments into the new stack using
     *  ordinary pointers. Everything from here to enter_usermode runs in the
     *  new address space with the kernel still mapped -- which it is, because
     *  the kernel half is shared by every directory.                          */
    paging_switch_directory(new_dir);
    current_task->directory = new_dir;

    /* ---- Lay out the stack: argv strings, then the pointer array ------------- */
    uint8_t *sp = (uint8_t *)USER_STACK_TOP;
    char    *user_argv[32];

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv_copy[i]) + 1;
        sp -= len;
        memcpy(sp, argv_copy[i], len);
        user_argv[i] = (char *)sp;
        kfree(argv_copy[i]);
    }

    sp = (uint8_t *)ALIGN_DOWN((uintptr_t)sp, 16);

    /*  argv[] array, NULL terminated, then argc. The layout a crt0 expects:
     *
     *      [esp]      argc
     *      [esp+4]    argv        (a pointer to the array just above)
     */
    sp -= sizeof(char *);
    *(char **)sp = NULL;
    for (int i = argc - 1; i >= 0; i--) {
        sp -= sizeof(char *);
        *(char **)sp = user_argv[i];
    }

    char **argv_base = (char **)sp;

    sp -= sizeof(char **);
    *(char ***)sp = argv_base;
    sp -= sizeof(int);
    *(int *)sp = argc;

    /* ---- Point the trap frame at the new program ------------------------------
     * Rather than call enter_usermode, we rewrite the saved registers that the
     * interrupt stub is about to restore. The `iret` at the end of the syscall
     * path then lands in the new program instead of after the old `int 0x80`.
     * Same mechanism, no second code path.
     */
    regs->eip     = entry;
    regs->useresp = (uint32_t)sp;
    regs->eax     = 0;
    regs->ecx = regs->edx = regs->ebx = 0;
    regs->esi = regs->edi = regs->ebp = 0;
    regs->cs      = SEL_UCODE;
    regs->ss      = SEL_UDATA;
    regs->ds      = SEL_UDATA;
    regs->eflags |= 0x200;                      /* IF: see cpu.asm */

    current_task->brk               = ALIGN_UP(brk, PAGE_SIZE);
    current_task->user_stack_bottom = stack_bottom;
    strlcpy(current_task->name, path, TASK_NAME_LEN);

    /*  Only now is the old address space unreachable and safe to destroy.     */
    if (old_dir != kernel_directory)
        paging_free_directory(old_dir);

    tss_set_kernel_stack(current_task->kernel_stack);
    return 0;
}

int task_exec(const char *path, char *const argv[])
{
    (void)path; (void)argv;
    panic("task_exec(): called outside a system call");
}

/* ---------------------------------------------------------------------------
 *  exit and wait
 *
 *  A process cannot free everything about itself, because it is still using
 *  some of it: the kernel stack it is standing on, and the page directory CR3
 *  points at. So exit() frees what it can, becomes a zombie, and the parent's
 *  wait() does the rest.
 *
 *  That is the entire reason zombies exist -- plus one more: the exit status
 *  has to survive until somebody asks for it. A process with no living parent
 *  is re-parented to init, whose whole job is to call wait() forever and reap
 *  them.
 * ------------------------------------------------------------------------- */
void task_exit(int status)
{
    task_t *t = current_task;

    LOG_INFO("task: pid %d (%s) exiting with status %d", t->pid, t->name, status);

    for (int i = 0; i < MAX_FDS; i++) {
        if (t->fds[i]) { file_unref(t->fds[i]); t->fds[i] = NULL; }
    }

    /*  Hand any children to init (pid 1), so that they still have someone to
     *  reap them. Skipping this leaks a task slot per orphan.                 */
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == t->pid)
            tasks[i].ppid = 1;

    t->exit_status = status;
    t->state       = TASK_ZOMBIE;

    /*  Wake the parent if it is in wait(). The channel is the parent's task
     *  struct, which is a unique address that both sides can name.            */
    task_t *parent = task_find(t->ppid);
    if (parent) sched_wake(parent);

    sched_remove(t);
    schedule();

    panic("task_exit: the scheduler returned to a dead task (pid %d)", t->pid);
}

pid_t task_wait(int *status)
{
    task_t *self = current_task;

    for (;;) {
        bool have_children = false;

        for (int i = 0; i < MAX_TASKS; i++) {
            task_t *t = &tasks[i];
            if (t->state == TASK_UNUSED || t->ppid != self->pid) continue;

            have_children = true;

            if (t->state == TASK_ZOMBIE) {
                pid_t pid = t->pid;
                if (status) *status = t->exit_status;

                /*  Now, from a different stack and a different address space,
                 *  it is safe to release the last of it.                      */
                if (t->directory && t->directory != kernel_directory)
                    paging_free_directory(t->directory);

                if (t->kernel_stack)
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));

                t->state = TASK_UNUSED;
                return pid;
            }
        }

        if (!have_children) return -ECHILD;

        sched_block(self);        /* woken by a child's task_exit() */
    }
}

/* ---------------------------------------------------------------------------
 *  Diagnostics
 * ------------------------------------------------------------------------- */
static const char *state_name(task_state_t s)
{
    switch (s) {
    case TASK_UNUSED:   return "unused";
    case TASK_EMBRYO:   return "embryo";
    case TASK_READY:    return "ready";
    case TASK_RUNNING:  return "running";
    case TASK_BLOCKED:  return "blocked";
    case TASK_SLEEPING: return "sleeping";
    case TASK_ZOMBIE:   return "zombie";
    }
    return "?";
}

void task_dump_all(void)
{
    kprintf(" PID PPID  STATE     TICKS  NAME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = &tasks[i];
        if (t->state == TASK_UNUSED) continue;
        kprintf("%4d %4d  %-9s %5u  %s\n",
                t->pid, t->ppid, state_name(t->state),
                (uint32_t)t->ticks_used, t->name);
    }
}

/*  Exposed for the scheduler, which needs to walk the table.                  */
task_t *task_table(void) { return tasks; }
