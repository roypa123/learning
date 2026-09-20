/* ===========================================================================
 *  nimbus/fs/fd.c  --  file descriptors
 * ===========================================================================
 *
 *  There are three levels here and conflating any two of them produces bugs
 *  that are very hard to see:
 *
 *      fd            a small integer, per process, an index into an array
 *      struct file   an *open* file: a node plus a read/write offset
 *      vfs_node_t    the file itself: one per file on disk, shared by all
 *
 *  Why the middle layer exists, in one example. Two processes open the same
 *  file; they get different `struct file`s, so they have independent offsets
 *  and reading in one does not move the other. Then one of them forks; the
 *  parent and child share the *same* `struct file`, so they share an offset,
 *  and `(echo a; echo b) > f` produces "a\nb\n" instead of "b\n". Both
 *  behaviours are required, and neither is expressible without exactly this
 *  three-level split.
 *
 *  Explained in: docs/43-fds-and-pipes.md
 * =========================================================================== */

#include <nimbus/fs.h>
#include <nimbus/vfs.h>
#include <nimbus/task.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/syscall.h>
#include <nimbus/io.h>

file_t *file_open_node(vfs_node_t *node, uint32_t flags)
{
    if (!node) return NULL;

    file_t *f = (file_t *)kcalloc(1, sizeof(file_t));
    if (!f) return NULL;

    f->node     = node;
    f->offset   = 0;
    f->flags    = flags;
    f->refcount = 1;

    /*  O_APPEND positions at the end on open. Note this is not the same as
     *  seeking to the end: a real O_APPEND re-seeks before *every* write, so
     *  that two processes appending to one log file cannot overwrite each
     *  other. Ours is the simpler version, and the difference is exactly the
     *  bug you get in a multi-process logger.                                 */
    if (flags & O_APPEND) f->offset = node->length;

    if (flags & O_TRUNC) {
        node->length = 0;
        /* A real truncate also frees the file's blocks; FAT16's write path
         * handles a shrinking file, so this is enough for our purposes. */
    }

    vfs_open(node, flags);
    return f;
}

void file_ref(file_t *f)
{
    if (f) f->refcount++;
}

void file_unref(file_t *f)
{
    if (!f) return;

    if (f->refcount > 0) f->refcount--;
    if (f->refcount > 0) return;

    /*  Last reference gone. Only now does the underlying node get closed --
     *  which for a pipe is what signals EOF to the other end, and is why a
     *  shell must close *both* ends of a pipe in the parent or the reader
     *  never sees end-of-file and the pipeline hangs forever.                 */
    vfs_close(f->node);
    kfree(f);
}

/* ---------------------------------------------------------------------------
 *  The per-process table
 *
 *  fd_alloc returns the *lowest* free descriptor. That is not an arbitrary
 *  choice -- it is a guarantee POSIX makes, and the entire shell redirection
 *  idiom depends on it:
 *
 *      close(1);              // descriptor 1 is now free
 *      open("out.txt", ...);  // ...so this is guaranteed to return 1
 *
 *  which is how redirection worked before dup2 existed.
 * ------------------------------------------------------------------------- */
int fd_alloc(file_t *f)
{
    if (!f || !current_task) return -EBADF;

    uint32_t flags = irq_save();

    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (!current_task->fds[fd]) {
            current_task->fds[fd] = f;
            irq_restore(flags);
            return fd;
        }
    }

    irq_restore(flags);
    return -EMFILE;
}

file_t *fd_get(int fd)
{
    /*  Both bounds. `fd < 0` is not hypothetical: every failed syscall returns
     *  a negative number, and a program that forgets to check will hand that
     *  straight back to read(). Without the lower check, fd = -2 indexes eight
     *  bytes before the array -- which in `struct task` is `cwd`, a pointer
     *  the kernel would then treat as a `file_t *`.                           */
    if (fd < 0 || fd >= MAX_FDS || !current_task) return NULL;
    return current_task->fds[fd];
}

int fd_close(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !current_task) return -EBADF;

    file_t *f = current_task->fds[fd];
    if (!f) return -EBADF;

    current_task->fds[fd] = NULL;
    file_unref(f);
    return 0;
}
