/* ===========================================================================
 *  nimbus/fs/pipe.c  --  a filesystem object with no file behind it
 * ===========================================================================
 *
 *  A pipe is a 4 KiB ring buffer with two VFS nodes pointing at it. Write into
 *  one, read out of the other. It is the smallest complete demonstration of
 *  why a VFS is worth having: `ls | grep txt` works because grep's stdin is a
 *  file descriptor, and nothing in grep, in read(), or in the fd layer cares
 *  that the thing behind it has no blocks on any disk.
 *
 *  It is also the place where blocking becomes unavoidable. A reader on an
 *  empty pipe must wait for a writer; a writer on a full pipe must wait for a
 *  reader; and either may wait forever if the other end is closed -- which is
 *  what EOF and EPIPE are for.
 *
 *  Explained in: docs/43-fds-and-pipes.md
 * =========================================================================== */

#include <nimbus/fs.h>
#include <nimbus/vfs.h>
#include <nimbus/sched.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/syscall.h>
#include <nimbus/io.h>

typedef struct pipe {
    uint8_t  buffer[PIPE_BUFFER_SIZE];
    uint32_t head;              /* where the writer puts the next byte       */
    uint32_t tail;              /* where the reader takes the next byte      */
    uint32_t count;             /* bytes currently in the buffer             */

    /*  Refcounts per end, not per node. A reader needs to know whether *any*
     *  writer still exists, and the answer changes when a process exits or
     *  closes a descriptor -- which is why these are decremented in close()
     *  and why a shell that forgets to close the unused end of a pipe in the
     *  parent creates a pipeline that never terminates.                       */
    uint32_t readers;
    uint32_t writers;
} pipe_t;

static ssize_t pipe_read(vfs_node_t *node, off_t offset UNUSED,
                         size_t size, uint8_t *buf)
{
    pipe_t *p = (pipe_t *)node->device;
    size_t  got = 0;

    while (got == 0) {
        uint32_t flags = irq_save();

        while (p->count > 0 && got < size) {
            buf[got++] = p->buffer[p->tail];
            p->tail = (p->tail + 1) % PIPE_BUFFER_SIZE;
            p->count--;
        }

        bool writers_left = (p->writers > 0);
        irq_restore(flags);

        if (got > 0) {
            /*  Space freed. Anyone blocked trying to write can continue.      */
            sched_wake(p);
            break;
        }

        /*  Nothing to read. If no writer remains, this is end of file -- a
         *  read of zero bytes, which is exactly how `cat` knows to stop. If a
         *  writer still exists, wait for it.                                  */
        if (!writers_left) return 0;

        sched_block(p);
    }

    return (ssize_t)got;
}

static ssize_t pipe_write(vfs_node_t *node, off_t offset UNUSED,
                          size_t size, const uint8_t *buf)
{
    pipe_t *p = (pipe_t *)node->device;
    size_t  sent = 0;

    while (sent < size) {
        uint32_t flags = irq_save();

        /*  No readers left: writing into a pipe nobody will ever read is an
         *  error, not a silent discard. On a real Unix this also raises
         *  SIGPIPE, which is why `yes | head -1` terminates instead of running
         *  forever -- head exits, the pipe loses its reader, and yes is killed
         *  by the signal it did not handle. We have no signals, so the write
         *  simply fails with EPIPE and a well-behaved program stops.          */
        if (p->readers == 0) {
            irq_restore(flags);
            return sent > 0 ? (ssize_t)sent : -EPIPE;
        }

        while (p->count < PIPE_BUFFER_SIZE && sent < size) {
            p->buffer[p->head] = buf[sent++];
            p->head = (p->head + 1) % PIPE_BUFFER_SIZE;
            p->count++;
        }

        irq_restore(flags);

        sched_wake(p);                 /* a reader may be waiting */

        if (sent < size) sched_block(p);   /* buffer full: wait for room */
    }

    return (ssize_t)sent;
}

static int pipe_close(vfs_node_t *node)
{
    pipe_t *p = (pipe_t *)node->device;
    if (!p) return 0;

    uint32_t flags = irq_save();

    /*  Which end is this? `impl` was set at creation: 0 = read end, 1 = write. */
    if (node->impl == 0) { if (p->readers) p->readers--; }
    else                 { if (p->writers) p->writers--; }

    bool dead = (p->readers == 0 && p->writers == 0);
    irq_restore(flags);

    /*  Wake the other end so it can notice that we are gone. Without this, a
     *  reader blocked on an empty pipe sleeps forever after its writer exits:
     *  nothing would ever run sched_wake, because the writer's last act was to
     *  close.                                                                 */
    sched_wake(p);

    if (dead) kfree(p);
    return 0;
}

int pipe_create(vfs_node_t **read_end, vfs_node_t **write_end)
{
    pipe_t *p = (pipe_t *)kcalloc(1, sizeof(pipe_t));
    if (!p) return -ENOMEM;

    vfs_node_t *r = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    vfs_node_t *w = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!r || !w) { kfree(p); kfree(r); kfree(w); return -ENOMEM; }

    p->readers = 1;
    p->writers = 1;

    strlcpy(r->name, "pipe:r", VFS_NAME_MAX);
    r->flags    = VFS_PIPE;
    r->device   = p;
    r->impl     = 0;
    r->read     = pipe_read;
    r->close    = pipe_close;
    r->refcount = 1;

    strlcpy(w->name, "pipe:w", VFS_NAME_MAX);
    w->flags    = VFS_PIPE;
    w->device   = p;
    w->impl     = 1;
    w->write    = pipe_write;
    w->close    = pipe_close;
    w->refcount = 1;

    *read_end  = r;
    *write_end = w;
    return 0;
}
