/* ===========================================================================
 *  nimbus/kernel/syscall.c  --  the door in the wall
 * ===========================================================================
 *
 *  Everything a user program can ask the kernel to do passes through this
 *  file, which makes it the single most security-relevant thing in Nimbus.
 *
 *  The mental model to hold while reading it: every value that arrives here
 *  was chosen by code we do not trust. Not "code that might have a bug" --
 *  code that may be actively trying to make the kernel write somewhere it
 *  should not. A pointer is not a pointer, it is a 32-bit number. A length is
 *  not a length, it is a 32-bit number that might be 0xFFFFFFFF. A file
 *  descriptor is not a descriptor, it is an integer that might be -1 or 900.
 *
 *  Explained in: docs/33-syscalls.md
 *  Line by line: docs/line-by-line/nimbus-syscall.md
 * =========================================================================== */

#define NIMBUS_KERNEL 1

#include <nimbus/syscall.h>
#include <nimbus/isr.h>
#include <nimbus/task.h>
#include <nimbus/sched.h>
#include <nimbus/paging.h>
#include <nimbus/heap.h>
#include <nimbus/vfs.h>
#include <nimbus/fs.h>
#include <nimbus/console.h>
#include <nimbus/timer.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/io.h>

/* ===========================================================================
 *  Validating what came from ring 3
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 *  user_range_ok
 *
 *  Two questions, and both must be asked.
 *
 *  1. Is the range inside the user half of the address space? A pointer of
 *     0xC0100000 is a perfectly valid address -- it is the kernel's own code.
 *     Without this check, `read(fd, (void*)0xC0100000, 4096)` asks the kernel
 *     to overwrite itself with the contents of a file, from ring 3, using an
 *     entirely legitimate system call.
 *
 *  2. Is every page of it actually mapped, with the right permissions? An
 *     unmapped page would fault, and a fault inside the kernel while copying
 *     on behalf of a user is a kernel page fault -- a panic, from a user
 *     program passing a bad pointer. Real kernels instead use a fixup table so
 *     that the copy can fail gracefully; checking up front is simpler and
 *     costs one walk per page.
 *
 *  The overflow check on `ptr + n` is not paranoia either: with ptr =
 *  0xBFFFF000 and n = 0x80000000, the sum wraps to 0x3FFFF000, which is a
 *  perfectly respectable user address, and a naive range check passes.
 * ------------------------------------------------------------------------- */
bool user_range_ok(const void *user_ptr, size_t n, bool need_write)
{
    uintptr_t start = (uintptr_t)user_ptr;

    if (n == 0) return true;
    if (start >= KERNEL_VIRTUAL_BASE) return false;
    if (start + n < start) return false;                  /* wrapped */
    if (start + n > KERNEL_VIRTUAL_BASE) return false;

    for (uintptr_t v = ALIGN_DOWN(start, PAGE_SIZE); v < start + n; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(current_task->directory, v, false);
        if (!pte || !(*pte & PTE_PRESENT)) return false;
        if (!(*pte & PTE_USER)) return false;
        if (need_write && !(*pte & PTE_WRITABLE)) return false;
    }
    return true;
}

bool copy_from_user(void *dst, const void *user_src, size_t n)
{
    if (!user_range_ok(user_src, n, false)) return false;
    memcpy(dst, user_src, n);
    return true;
}

bool copy_to_user(void *user_dst, const void *src, size_t n)
{
    if (!user_range_ok(user_dst, n, true)) return false;
    memcpy(user_dst, src, n);
    return true;
}

/* ---------------------------------------------------------------------------
 *  Copying a string is harder than copying a buffer, because we do not know
 *  how long it is until we have read it -- and we cannot read it until we know
 *  it is safe. So: validate and copy one byte at a time, stopping at the NUL
 *  or at the limit, whichever comes first.
 *
 *  Checking the page only when we cross a page boundary keeps it to one walk
 *  per 4096 bytes instead of one per byte.
 * ------------------------------------------------------------------------- */
bool user_string_copy(char *dst, const char *user_src, size_t max)
{
    uintptr_t addr = (uintptr_t)user_src;

    for (size_t i = 0; i < max; i++, addr++) {
        if (i == 0 || (addr & PAGE_MASK) == 0) {
            if (!user_range_ok((const void *)addr, 1, false)) return false;
        }

        dst[i] = *(const char *)addr;
        if (dst[i] == '\0') return true;
    }

    /*  Ran out of room. Terminate anyway -- an unterminated buffer handed to
     *  the rest of the kernel is worse than a truncated path.                 */
    dst[max - 1] = '\0';
    return false;
}

/* ===========================================================================
 *  The calls
 * =========================================================================== */

static int32_t sys_exit(registers_t *regs)
{
    task_exit((int)regs->ebx);
    return 0;                                  /* never reached */
}

static int32_t sys_write(registers_t *regs)
{
    int         fd  = (int)regs->ebx;
    const void *buf = (const void *)regs->ecx;
    size_t      len = (size_t)regs->edx;

    if (len > 1 * MiB) return -EINVAL;
    if (!user_range_ok(buf, len, false)) return -EFAULT;

    file_t *f = fd_get(fd);
    if (!f) return -EBADF;
    if (!f->node->write) return -EINVAL;

    ssize_t n = vfs_write(f->node, f->offset, len, (const uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
    return (int32_t)n;
}

static int32_t sys_read(registers_t *regs)
{
    int    fd  = (int)regs->ebx;
    void  *buf = (void *)regs->ecx;
    size_t len = (size_t)regs->edx;

    if (len > 1 * MiB) return -EINVAL;
    if (!user_range_ok(buf, len, true)) return -EFAULT;

    file_t *f = fd_get(fd);
    if (!f) return -EBADF;
    if (!f->node->read) return -EINVAL;

    ssize_t n = vfs_read(f->node, f->offset, len, (uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
    return (int32_t)n;
}

static int32_t sys_open(registers_t *regs)
{
    char path[VFS_PATH_MAX];

    if (!user_string_copy(path, (const char *)regs->ebx, sizeof(path)))
        return -EFAULT;

    uint32_t flags = regs->ecx;

    vfs_node_t *node = vfs_lookup(path);

    if (!node && (flags & O_CREAT)) {
        char last[VFS_NAME_MAX];
        vfs_node_t *dir = vfs_lookup_parent(path, last);
        if (!dir || !dir->create) return -ENOENT;
        if (dir->create(dir, last, 0) < 0) return -EIO;
        node = vfs_lookup(path);
    }

    if (!node) return -ENOENT;

    file_t *f = file_open_node(node, flags);
    if (!f) return -ENOMEM;

    int fd = fd_alloc(f);
    if (fd < 0) { file_unref(f); return -EMFILE; }
    return fd;
}

static int32_t sys_close(registers_t *regs)
{
    return fd_close((int)regs->ebx);
}

static int32_t sys_fork(registers_t *regs)
{
    return task_fork_regs(regs);
}

/* ---------------------------------------------------------------------------
 *  exec
 *
 *  argv arrives as a user pointer to an array of user pointers to user
 *  strings. Three levels, every one of them untrusted, and the array has no
 *  length -- it ends with a NULL that the caller may have forgotten. So we
 *  copy it defensively, element by element, with a hard cap.
 * ------------------------------------------------------------------------- */
static int32_t sys_exec(registers_t *regs)
{
    char path[VFS_PATH_MAX];
    if (!user_string_copy(path, (const char *)regs->ebx, sizeof(path)))
        return -EFAULT;

    char *argv[32];
    int   argc = 0;

    char **user_argv = (char **)regs->ecx;

    if (user_argv) {
        for (argc = 0; argc < 31; argc++) {
            char *user_str;
            if (!copy_from_user(&user_str, &user_argv[argc], sizeof(char *)))
                return -EFAULT;
            if (!user_str) break;

            char *copy = (char *)kmalloc(256);
            if (!copy) { argc--; break; }

            if (!user_string_copy(copy, user_str, 256)) {
                kfree(copy);
                return -EFAULT;
            }
            argv[argc] = copy;
        }
    }
    argv[argc] = NULL;

    int rc = task_exec_regs(regs, path, argv);

    for (int i = 0; i < argc; i++) kfree(argv[i]);
    return rc;
}

static int32_t sys_wait(registers_t *regs)
{
    int  status = 0;
    pid_t pid = task_wait(&status);

    if (pid >= 0 && regs->ebx) {
        if (!copy_to_user((void *)regs->ebx, &status, sizeof(int)))
            return -EFAULT;
    }
    return pid;
}

static int32_t sys_getpid(registers_t *regs UNUSED)
{
    return current_task->pid;
}

/* ---------------------------------------------------------------------------
 *  sbrk -- grow the user heap
 *
 *  The oldest memory interface in Unix: move the "program break", the top of
 *  the data segment, and return where it used to be. malloc() is built on it.
 *
 *  Modern systems use mmap instead, which can place mappings anywhere and can
 *  unmap the middle of a region -- sbrk can only move a single boundary, so
 *  freeing memory back to the kernel requires that the *last* thing allocated
 *  be the first thing freed. That limitation is why every real malloc keeps
 *  freed memory rather than returning it.
 * ------------------------------------------------------------------------- */
static int32_t sys_sbrk(registers_t *regs)
{
    int32_t increment = (int32_t)regs->ebx;
    vaddr_t old_brk   = current_task->brk;

    if (increment == 0) return (int32_t)old_brk;

    if (increment > 0) {
        vaddr_t new_brk = old_brk + (uint32_t)increment;

        if (new_brk >= USER_STACK_TOP - USER_STACK_SIZE * 8) return -ENOMEM;

        paging_map_range(current_task->directory, old_brk,
                         (size_t)increment, PTE_WRITABLE | PTE_USER);
        current_task->brk = ALIGN_UP(new_brk, PAGE_SIZE);
    } else {
        vaddr_t new_brk = old_brk - (uint32_t)(-increment);
        if (new_brk < USER_HEAP_BASE) return -EINVAL;

        paging_free_range(current_task->directory, ALIGN_UP(new_brk, PAGE_SIZE),
                          old_brk - ALIGN_UP(new_brk, PAGE_SIZE));
        current_task->brk = new_brk;
    }

    return (int32_t)old_brk;
}

static int32_t sys_sleep(registers_t *regs)
{
    sleep_ms(regs->ebx);
    return 0;
}

static int32_t sys_yield(registers_t *regs UNUSED)
{
    schedule();
    return 0;
}

static int32_t sys_lseek(registers_t *regs)
{
    file_t *f = fd_get((int)regs->ebx);
    if (!f) return -EBADF;

    int32_t offset = (int32_t)regs->ecx;
    int     whence = (int)regs->edx;

    int32_t base;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (int32_t)f->offset; break;
    case SEEK_END: base = (int32_t)f->node->length; break;
    default: return -EINVAL;
    }

    int32_t target = base + offset;
    if (target < 0) return -EINVAL;

    f->offset = (off_t)target;
    return target;
}

static int32_t sys_readdir(registers_t *regs)
{
    file_t *f = fd_get((int)regs->ebx);
    if (!f) return -EBADF;
    if (!(f->node->flags & VFS_DIRECTORY)) return -ENOTDIR;

    dirent_t *entry = vfs_readdir(f->node, regs->edx);
    if (!entry) return 0;                       /* end of directory */

    if (!copy_to_user((void *)regs->ecx, entry, sizeof(dirent_t)))
        return -EFAULT;
    return 1;
}

static int32_t sys_stat(registers_t *regs)
{
    char path[VFS_PATH_MAX];
    if (!user_string_copy(path, (const char *)regs->ebx, sizeof(path)))
        return -EFAULT;

    stat_t st;
    if (vfs_stat(path, &st) < 0) return -ENOENT;

    if (!copy_to_user((void *)regs->ecx, &st, sizeof(st))) return -EFAULT;
    return 0;
}

static int32_t sys_pipe(registers_t *regs)
{
    vfs_node_t *r, *w;
    if (pipe_create(&r, &w) < 0) return -ENOMEM;

    file_t *fr = file_open_node(r, O_RDONLY);
    file_t *fw = file_open_node(w, O_WRONLY);
    if (!fr || !fw) return -ENOMEM;

    int fds[2];
    fds[0] = fd_alloc(fr);
    fds[1] = fd_alloc(fw);

    if (fds[0] < 0 || fds[1] < 0) {
        if (fds[0] >= 0) fd_close(fds[0]);
        if (fds[1] >= 0) fd_close(fds[1]);
        return -EMFILE;
    }

    if (!copy_to_user((void *)regs->ebx, fds, sizeof(fds))) return -EFAULT;
    return 0;
}

static int32_t sys_dup2(registers_t *regs)
{
    int oldfd = (int)regs->ebx;
    int newfd = (int)regs->ecx;

    if (newfd < 0 || newfd >= MAX_FDS) return -EBADF;

    file_t *f = fd_get(oldfd);
    if (!f) return -EBADF;
    if (oldfd == newfd) return newfd;

    if (current_task->fds[newfd]) file_unref(current_task->fds[newfd]);

    file_ref(f);
    current_task->fds[newfd] = f;
    return newfd;
}

static int32_t sys_chdir(registers_t *regs)
{
    char path[VFS_PATH_MAX];
    if (!user_string_copy(path, (const char *)regs->ebx, sizeof(path)))
        return -EFAULT;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -ENOENT;
    if (!(node->flags & VFS_DIRECTORY)) return -ENOTDIR;

    current_task->cwd = node;
    return 0;
}

static int32_t sys_uptime(registers_t *regs UNUSED)
{
    return (int32_t)timer_ms();
}

static int32_t sys_ps(registers_t *regs UNUSED)
{
    task_dump_all();
    return 0;
}

static int32_t sys_unlink(registers_t *regs)
{
    char path[VFS_PATH_MAX];
    if (!user_string_copy(path, (const char *)regs->ebx, sizeof(path)))
        return -EFAULT;

    char last[VFS_NAME_MAX];
    vfs_node_t *dir = vfs_lookup_parent(path, last);
    if (!dir || !dir->unlink) return -EPERM;

    return dir->unlink(dir, last);
}

static int32_t sys_reboot(registers_t *regs UNUSED)
{
    /*  Pulse the reset line through the keyboard controller. Yes, really: the
     *  8042 has an output pin wired to the CPU's reset, for the same 1984
     *  reason it has one wired to A20. Modern firmware also accepts an ACPI
     *  reset, and QEMU honours this one.                                      */
    kprintf("\nrebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);

    for (;;) hlt();
}

/* ===========================================================================
 *  The table
 * =========================================================================== */
typedef int32_t (*syscall_fn)(registers_t *);

static syscall_fn syscall_table[SYS_MAX] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_READ]    = sys_read,
    [SYS_OPEN]    = sys_open,
    [SYS_CLOSE]   = sys_close,
    [SYS_FORK]    = sys_fork,
    [SYS_EXEC]    = sys_exec,
    [SYS_WAIT]    = sys_wait,
    [SYS_GETPID]  = sys_getpid,
    [SYS_SBRK]    = sys_sbrk,
    [SYS_SLEEP]   = sys_sleep,
    [SYS_YIELD]   = sys_yield,
    [SYS_LSEEK]   = sys_lseek,
    [SYS_READDIR] = sys_readdir,
    [SYS_STAT]    = sys_stat,
    [SYS_PIPE]    = sys_pipe,
    [SYS_DUP2]    = sys_dup2,
    [SYS_CHDIR]   = sys_chdir,
    [SYS_UNLINK]  = sys_unlink,
    [SYS_UPTIME]  = sys_uptime,
    [SYS_PS]      = sys_ps,
    [SYS_REBOOT]  = sys_reboot,
};

/*  A table rather than a switch, for one reason that matters: the bounds check
 *  is explicit and impossible to forget. A switch on an untrusted integer is
 *  fine, but a table indexed by one is a memory read at an attacker-chosen
 *  offset unless somebody wrote the comparison -- so we write it once, here,
 *  where it is the first statement in the function.                           */

void syscall_dispatch(registers_t *regs)
{
    uint32_t number = regs->eax;

    if (number >= SYS_MAX || !syscall_table[number]) {
        LOG_WARN("unknown syscall %u from pid %d", number, current_task->pid);
        regs->eax = (uint32_t)(-ENOSYS);
        return;
    }

    /*  Interrupts back on. We arrived through an interrupt gate, which cleared
     *  IF; leaving them off for the duration of a system call would mean a
     *  read() from the disk blocks the timer, the keyboard and everything
     *  else for the whole transfer.
     *
     *  This is also the moment the kernel becomes re-entrant: from here on
     *  another task can be scheduled in the middle of this call, and every
     *  data structure we touch needs to survive that.                         */
    sti();

    int32_t result = syscall_table[number](regs);

    cli();

    /*  The return value goes back in EAX, in the saved frame -- not in the
     *  real register. The stub restores the whole frame with popa on the way
     *  out, so writing the register here would be overwritten a moment later.
     *  This indirection is exactly why interrupt_dispatch takes a pointer.    */
    regs->eax = (uint32_t)result;
}

void syscall_init(void)
{
    LOG_INFO("syscall: %u calls on vector 0x80", (uint32_t)SYS_MAX);
}
