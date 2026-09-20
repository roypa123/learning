# Chapter 43 — File descriptors and pipes

[← FAT16, write](42-fat16-write.md) · [Contents](README.md) · [Next: The ELF loader →](44-elf-loader.md)

---

## Goal

Build the layer between a small integer and a file, and then the object that has no file behind it at
all. This finishes Part V.

The three-level structure looks like over-engineering until you see the two behaviours that require
it.

---

## 1. Three levels

```
    fd            a small integer, per process, an index into an array
    struct file   an *open* file: a node plus a read/write offset
    vfs_node_t    the file itself: one per file on disk, shared by all
```

> Why the middle layer exists, in one example. Two processes open the same file; they get different
> `struct file`s, so they have independent offsets and reading in one does not move the other. Then
> one of them forks; the parent and child share the *same* `struct file`, so they share an offset,
> and `(echo a; echo b) > f` produces "a\nb\n" instead of "b\n". Both behaviours are required, and
> neither is expressible without exactly this three-level split.

Draw it:

```
    process A                     process B
    fds[0] -> file X  ----+       fds[0] -> file Y ----+
    fds[1] -> file X  --+ |                            |
                        | |                            |
                        v v                            v
                     +--------+  offset=0        +--------+ offset=500
                     | file X |                  | file Y |
                     +--------+                  +--------+
                          |                           |
                          +------------+--------------+
                                       v
                                +--------------+
                                |  vfs_node    |  /mnt/log.txt
                                +--------------+
```

Two `file` objects on one node — independent offsets. Two descriptors on one `file` — shared offset.
Both are needed and they are different relationships.

---

## 2. The open file

```c
typedef struct file {
    vfs_node_t *node;
    off_t       offset;
    uint32_t    flags;
    uint32_t    refcount;
} file_t;
```

Sixteen bytes.

```c
file_t *file_open_node(vfs_node_t *node, uint32_t flags)
{
    file_t *f = (file_t *)kcalloc(1, sizeof(file_t));
    if (!f) return NULL;

    f->node     = node;
    f->offset   = 0;
    f->flags    = flags;
    f->refcount = 1;

    if (flags & O_APPEND) f->offset = node->length;

    if (flags & O_TRUNC) {
        node->length = 0;
    }

    vfs_open(node, flags);
    return f;
}
```

### 2.1 `O_APPEND` is not a seek

```c
    /*  O_APPEND positions at the end on open. Note this is not the same as
     *  seeking to the end: a real O_APPEND re-seeks before *every* write, so
     *  that two processes appending to one log file cannot overwrite each
     *  other. Ours is the simpler version, and the difference is exactly the
     *  bug you get in a multi-process logger. */
```

Two processes with `O_APPEND` on the same log:

```
    A: offset = 1000 (from open)
    B: offset = 1000 (from open)
    A: write 50 bytes at 1000, offset -> 1050
    B: write 50 bytes at 1000            <- overwrites A
```

Real `O_APPEND` makes the seek-and-write atomic in the kernel, per write. That is what makes it
usable for logging from multiple processes, and it is one of the few genuinely atomic guarantees
POSIX makes about files.

Exercise 43.4 — three lines in `sys_write`.

### 2.2 `O_TRUNC` leaks

```c
    if (flags & O_TRUNC) {
        node->length = 0;
        /* A real truncate also frees the file's blocks; FAT16's write path
         * handles a shrinking file, so this is enough for our purposes. */
    }
```

Setting `length = 0` makes the file appear empty. The cluster chain is still allocated and still
linked from the directory entry.

So `echo x > bigfile` repeatedly leaks the old clusters every time. Exercise 42.6 implements a real
truncate; this comment is here so the leak is known rather than discovered.

---

## 3. Reference counting

```c
void file_ref(file_t *f)
{
    if (f) f->refcount++;
}

void file_unref(file_t *f)
{
    if (!f) return;

    if (f->refcount > 0) f->refcount--;
    if (f->refcount > 0) return;

    vfs_close(f->node);
    kfree(f);
}
```

Who holds a reference:

- Each descriptor pointing at it (`fd_alloc`, `dup2`, `fork`).
- Nobody else.

```c
    for (int i = 0; i < MAX_FDS; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i]) file_ref(child->fds[i]);
    }
```

`fork` copies the table and takes a reference per entry (Chapter 34, §2.3).

### 3.1 The last close is a signal

```c
    /*  Last reference gone. Only now does the underlying node get closed --
     *  which for a pipe is what signals EOF to the other end, and is why a
     *  shell must close *both* ends of a pipe in the parent or the reader
     *  never sees end-of-file and the pipeline hangs forever. */
```

This is the sentence that explains §5.3. A close is not just cleanup; for a pipe it is the *only*
way the other end learns anything.

### 3.2 The atomic decrement that is not

```c
    if (f->refcount > 0) f->refcount--;
    if (f->refcount > 0) return;
```

Read, decrement, test. On a uniprocessor with no preemption inside this function it is safe; with
threads sharing a descriptor table it is not — two concurrent unrefs could both see 1, both
decrement to 0, and both free.

`atomic_dec_and_test` exists for exactly this (Chapter 36, §4.3) and is not used here, which is a
small honest gap. Exercise 43.5.

---

## 4. The descriptor table

```c
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
```

### 4.1 Lowest free, guaranteed

> `fd_alloc` returns the *lowest* free descriptor. That is not an arbitrary choice — it is a
> guarantee POSIX makes, and the entire shell redirection idiom depends on it:
>
> ```c
>     close(1);              // descriptor 1 is now free
>     open("out.txt", ...);  // ...so this is guaranteed to return 1
> ```
>
> which is how redirection worked before dup2 existed.

That guarantee is why a linear scan from 0 is the *correct* implementation and not merely a simple
one. An allocator that returned any free descriptor would break every shell written before 1983.

### 4.2 Both bounds

```c
file_t *fd_get(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !current_task) return NULL;
    return current_task->fds[fd];
}
```

Chapter 33, §6:

> `fd < 0` is not hypothetical: every failed syscall returns a negative number, and a program that
> forgets to check will hand that straight back to `read()`. Without the lower check, fd = −2 indexes
> eight bytes before the array — which in `struct task` is `cwd`, a pointer the kernel would then
> treat as a `file_t *`.

### 4.3 `dup2`

```c
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
```

"Make `newfd` another name for whatever `oldfd` refers to, closing `newfd` first."

Three details:

**`oldfd == newfd` returns early**, before the unref. Without that check, `dup2(1, 1)` would unref
the file, possibly freeing it, and then store a dangling pointer.

**The old `newfd` is closed**, which is what makes it a *replacement* rather than an allocation.

**A reference is taken**, because there are now two descriptors.

And this is the whole of redirection:

```c
    if (r->out_file) {
        int fd = open(r->out_file, O_WRONLY | O_CREAT | O_TRUNC);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
```

> dup2 makes descriptor 1 a second name for the same open file, and closes whatever 1 was. The
> program that execs next reads its stdin and has no idea it is a file — which is the point.

---

## 5. Pipes

> A pipe is a 4 KiB ring buffer with two VFS nodes pointing at it. Write into one, read out of the
> other. It is the smallest complete demonstration of why a VFS is worth having: `ls | grep txt`
> works because grep's stdin is a file descriptor, and nothing in grep, in `read()`, or in the fd
> layer cares that the thing behind it has no blocks on any disk.

```c
typedef struct pipe {
    uint8_t  buffer[PIPE_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;

    uint32_t readers;
    uint32_t writers;
} pipe_t;
```

### 5.1 Two nodes, one buffer

```c
    strlcpy(r->name, "pipe:r", VFS_NAME_MAX);
    r->flags    = VFS_PIPE;
    r->device   = p;
    r->impl     = 0;
    r->read     = pipe_read;
    r->close    = pipe_close;

    strlcpy(w->name, "pipe:w", VFS_NAME_MAX);
    w->flags    = VFS_PIPE;
    w->device   = p;
    w->impl     = 1;
    w->write    = pipe_write;
    w->close    = pipe_close;
```

The read end has a `read` and no `write`; the write end the reverse. So `write(readfd, ...)` gets
`-EINVAL` from the dispatch layer (Chapter 39, §3) with no explicit check.

`impl` distinguishes the ends for `pipe_close`, which needs to know which counter to decrement.

### 5.2 Blocking on both sides

```c
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
            sched_wake(p);
            break;
        }

        if (!writers_left) return 0;

        sched_block(p);
    }

    return (ssize_t)got;
}
```

**A read returns as soon as *anything* is available** — not when the buffer is full. That is what
makes a pipeline stream rather than batch, and it is why `while (got == 0)` rather than
`while (got < size)`.

**`sched_wake(p)` after reading** frees space, so a blocked writer can continue.

**Zero writers means EOF**:

> If no writer remains, this is end of file — a read of zero bytes, which is exactly how `cat` knows
> to stop.

Chapter 39's `cat` loops `while ((n = read(...)) > 0)`. The 0 comes from here.

### 5.3 `EPIPE`, and the signal we do not have

```c
        if (p->readers == 0) {
            irq_restore(flags);
            return sent > 0 ? (ssize_t)sent : -EPIPE;
        }
```

> Writing into a pipe nobody will ever read is an error, not a silent discard. On a real Unix this
> also raises SIGPIPE, which is why `yes | head -1` terminates instead of running forever — head
> exits, the pipe loses its reader, and yes is killed by the signal it did not handle. We have no
> signals, so the write simply fails with `-EPIPE` and a well-behaved program stops.

The difference matters: with SIGPIPE, a program that ignores the error is killed anyway. With only
`-EPIPE`, a program that ignores the return value of `write` spins forever.

That is why `yes | head -1` is the canonical test, and why our `cat` checks:

```c
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);

    return n < 0 ? 1 : 0;
```

— except that it does not check `write`'s return, which means `cat bigfile | head -1` would loop.
Exercise 43.6.

### 5.4 Close wakes the other end

```c
static int pipe_close(vfs_node_t *node)
{
    ...
    if (node->impl == 0) { if (p->readers) p->readers--; }
    else                 { if (p->writers) p->writers--; }

    bool dead = (p->readers == 0 && p->writers == 0);
    irq_restore(flags);

    sched_wake(p);

    if (dead) kfree(p);
    return 0;
}
```

> Wake the other end so it can notice that we are gone. Without this, a reader blocked on an empty
> pipe sleeps forever after its writer exits: nothing would ever run `sched_wake`, because the
> writer's last act was to close.

Chapter 35, §6.1 — the missing-wakeup deadlock, and this is the line that prevents it.

The wake is **unconditional**, even when the pipe is not dead, because closing one of several writers
may still change what a reader should do.

### 5.5 The buffer is freed by the last closer

```c
    if (dead) kfree(p);
```

Both ends are gone, so nothing can reference it.

Note that the `vfs_node_t`s themselves leak — nothing frees them. That is a real bug and it is one
allocation per pipe, which for a shell running pipelines adds up. Exercise 43.7.

---

## 6. Building a pipeline

```c
static void run_pipeline(char *left[], char *right[])
{
    int fds[2];
    if (pipe(fds) < 0) { printf("sh: cannot create pipe\n"); return; }

    pid_t a = fork();
    if (a == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(left[0], left);
        ...
    }

    pid_t b = fork();
    if (b == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(right[0], right);
        ...
    }

    close(fds[0]);
    close(fds[1]);

    int status;
    wait(&status);
    wait(&status);
}
```

### 6.1 The step everyone forgets

> The parent closing both ends is the step everyone forgets, and the symptom is a pipeline that never
> finishes. While the parent still holds the write end open, the pipe has a writer, so the reader's
> `read()` never returns 0, so cmd2 waits forever for input that will never come — and the shell
> waits forever for cmd2.

Count the references after the two forks:

```
    write end:  parent, child A, child B   = 3
    read end:   parent, child A, child B   = 3
```

Child A closes `fds[0]` and dups then closes `fds[1]` — but the dup means it still holds the write
end as descriptor 1.

Child B closes both after duping `fds[0]` to 0.

So after both children have set up:

```
    write end:  parent, child A            = 2
    read end:   parent, child B            = 2
```

The parent's two closes bring both to 1. When A exits, the write count reaches 0 and B's next read
returns 0.

Without the parent's closes, the write count never reaches 0 and B blocks forever.

### 6.2 Why each child closes both

Child A needs only the write end, and child B only the read end — but each inherited both. Leaving
the unused one open has the same effect as the parent leaving one open, one level down.

The rule: **every process closes every descriptor it does not need, immediately.**

---

## 7. Running it

```
nimbus> ls /bin | cat
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
nimbus> hexdump /bin/echo | cat
00000000  7f 45 4c 46 01 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
...
nimbus> echo hello > /mnt/a.txt
nimbus> cat < /mnt/a.txt
hello
```

Four mechanisms: a pipe, redirection out, redirection in, and `dup2` in all three.

### 7.1 Watching the descriptors

```c
void fd_dump(void)
{
    kprintf("pid %d fds:\n", current_task->pid);
    for (int i = 0; i < MAX_FDS; i++) {
        file_t *f = current_task->fds[i];
        if (!f) continue;
        kprintf("  %2d -> %-12s offset %u refs %u\n",
                i, f->node->name, f->offset, f->refcount);
    }
}
```

```
pid 6 fds:
   0 -> pipe:r       offset 0 refs 1
   1 -> console      offset 0 refs 4
   2 -> console      offset 0 refs 4
```

`cat`'s stdin is the read end of a pipe; its stdout is the console, shared with three other
descriptors across the shell and its other child.

That listing is the whole chapter in one screen.

---

## 8. Part V, in retrospect

Seven chapters, and the kernel can now:

| | |
|---|---|
| Read a disk | ATA PIO, IDENTIFY, the BSY/DRQ handshake (37) |
| Find filesystems on it | MBR partitions, and the block layer we do not have (38) |
| Present them uniformly | Seven function pointers, and path resolution (39) |
| Boot before it can | A tar archive in memory (40) |
| Read a real format | FAT16: clusters, chains, 8.3 names (41) |
| Write it | Allocation, directory updates, and six power-cut outcomes (42) |
| Give programs handles | Three levels, `dup2`, and pipes (43) |

The shell can now run a program from a disk, redirect its output to a file, and pipe it to another
program. Everything in that sentence works.

What is missing is the loader that turns a file into a running program — which is Part VI's first
chapter, and which the kernel has secretly had since Chapter 32.

---

## 9. Exercises

🟢 **43.1** Add the `fd_dump` from §7.1 as a shell builtin and run it inside a pipeline.

🟢 **43.2** Remove the parent's two `close` calls in `run_pipeline` and run `ls | cat`.

🟢 **43.3** Call `dup2(1, 1)` and confirm the early return saves you.

🟡 **43.4** Implement real `O_APPEND`: seek to `node->length` inside `sys_write`, before the write,
under the same critical section. Then write a test with two processes appending.

🟡 **43.5** Use `atomic_dec_and_test` in `file_unref` and argue about whether it matters on a
uniprocessor.

🟡 **43.6** Make `cat` check `write`'s return value and exit on `-EPIPE`. Confirm
`hexdump /bin/sh | head` — once you have written `head` — terminates.

🟡 **43.7** Free the pipe's `vfs_node_t`s. You will need to decide who owns them, which is the
interesting part.

🔴 **43.8** Implement three-command pipelines by generalising `run_pipeline` to an array of commands
and *n*−1 pipes. Get the closing right — this is where the rule in §6.2 stops being optional.

🔴 **43.9** Add `select`: a syscall that blocks until any of a set of descriptors is ready. You will
need a per-node "is data available" query and a way to block on several channels at once, which is
the interesting design problem.

---

## What we covered

- Three levels, and the two different sharing relationships that require all three.
- `O_APPEND` as a per-write atomic seek, and the multi-process logger bug from getting it wrong.
- `O_TRUNC` that leaks a cluster chain, documented rather than hidden.
- Reference counting, and the last close as a *signal* rather than cleanup.
- Lowest-free-descriptor as a POSIX guarantee that shell redirection was built on.
- `dup2`'s three details, including the self-dup that would otherwise free the file.
- A pipe as a ring buffer with two nodes, distinguished by `impl`, each exposing one operation.
- Reads that return as soon as anything is available, zero writers meaning EOF, and `-EPIPE` without
  the signal that makes it work properly.
- The close that wakes the other end, and the deadlock without it.
- Reference counting a pipeline by hand, and the parent's two closes that everyone forgets.

**Part V is finished.** [Chapter 44](44-elf-loader.md) turns a file into a running program.

---

[← FAT16, write](42-fat16-write.md) · [Contents](README.md) · [Next: The ELF loader →](44-elf-loader.md)
