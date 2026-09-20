# Appendix D — The full syscall table

[Contents](README.md)

---

## The convention

```
    eax = call number
    ebx, ecx, edx, esi, edi = arguments 1..5
    int 0x80
    return value in eax; values in [-4095, -1] are -errno
```

Chapter 33 explains why registers rather than the stack, and why errors travel in the same register
as the result.

```c
static inline int32_t syscall3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t ret;
    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(n), "b"(a), "c"(b), "d"(c)
                      : "memory");
    return ret;
}
```

---

## The table

| # | Name | `ebx` | `ecx` | `edx` | Returns |
|---|---|---|---|---|---|
| 0 | `exit` | status | | | never |
| 1 | `write` | fd | buf | count | bytes, or `-errno` |
| 2 | `read` | fd | buf | count | bytes, 0 at EOF, or `-errno` |
| 3 | `open` | path | flags | | fd, or `-errno` |
| 4 | `close` | fd | | | 0 or `-errno` |
| 5 | `fork` | | | | **0 in the child**, pid in the parent |
| 6 | `exec` | path | argv | | only on failure |
| 7 | `wait` | `int *status` | | | pid, or `-ECHILD` |
| 8 | `getpid` | | | | pid |
| 9 | `sbrk` | increment | | | old break, or `(void*)-1` |
| 10 | `sleep` | ms | | | 0 |
| 11 | `yield` | | | | 0 |
| 12 | `lseek` | fd | offset | whence | new offset, or `-errno` |
| 13 | `readdir` | fd | `dirent_t *` | index | 1, 0 at end, or `-errno` |
| 14 | `stat` | path | `stat_t *` | | 0 or `-errno` |
| 15 | `pipe` | `int fds[2]` | | | 0 or `-errno` |
| 16 | `dup2` | oldfd | newfd | | newfd, or `-errno` |
| 17 | `chdir` | path | | | 0 or `-errno` |
| 18 | `getcwd` | buf | size | | **reserved, not implemented** |
| 19 | `unlink` | path | | | 0 or `-errno` |
| 20 | `mkdir` | path | | | **reserved, not implemented** |
| 21 | `uptime` | | | | milliseconds since boot |
| 22 | `ps` | | | | 0 — prints to the console |
| 23 | `reboot` | | | | never |

Numbers are **append-only**. A gap stays a gap (Ch. 33, §3.1).

---

## Per-call notes

### 0 — `exit(int status)`

Closes every descriptor, re-parents children to pid 1, becomes a zombie, and schedules away. Never
returns; the `panic` after it is a claim being checked (Ch. 34, §4.4).

### 1 — `write(int fd, const void *buf, size_t count)`

Validation: `count > 1 MiB` → `-EINVAL` (a denial-of-service bound); `user_range_ok(buf, count,
false)`; `fd_get`; `node->write` exists.

The offset advances unless the node is a character device (Ch. 20, §5.1).

### 2 — `read(int fd, void *buf, size_t count)`

Same validation with `need_write = true`, because we are writing into the caller's buffer.

**Returns 0 at end of file**, which is how `cat` knows to stop. A short read is normal and says
nothing about the end (Ch. 46, §8.1).

On the console, blocks until Enter (Ch. 20, §4).

### 3 — `open(const char *path, int flags)`

```c
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
```

Same numbers as Linux, so a `strace` reads without translation.

Returns the **lowest free descriptor** — a guarantee shell redirection was built on
(Ch. 43, §4.1).

`O_TRUNC` sets the length to 0 and leaks the cluster chain (Ch. 43, §2.2).
`O_APPEND` positions at the end *on open*, which is not the same as a real per-write append.

### 5 — `fork(void)`

Returns twice. 0 in the child, the child's pid in the parent.

The child's trap frame is a copy of the parent's with `eax` zeroed, and its context points at
`isr_return` (Ch. 34, §2.1).

Copies the descriptor table and **refcounts the `file_t`s** — parent and child share offsets.

### 6 — `exec(const char *path, char *const argv[])`

**Only returns on failure.** On success the address space is replaced and there is no code left to
return to.

`argv` is three levels of untrusted indirection, capped at 31 entries and 256 bytes per string
(Ch. 33, §6.1).

### 7 — `wait(int *status)`

Blocks until a child becomes a zombie, then reaps it — freeing its address space and kernel stack
(Ch. 34, §5).

`-ECHILD` when there are no children at all, which is how a shell knows not to wait forever.

`status` may be NULL.

### 9 — `sbrk(int increment)`

Returns the **old** break. `sbrk(0)` queries it without changing anything.

Growing maps pages with `PTE_WRITABLE | PTE_USER`; shrinking unmaps and frees them. Refuses to grow
into the stack region or shrink below `USER_HEAP_BASE`.

The wrapper distinguishes an error from a high address by the `[-4095, -1]` range (Ch. 45, §2.1).

### 10 — `sleep(unsigned ms)`

Sets `wake_tick` and state `TASK_SLEEPING`; the timer tick wakes it. The task uses no CPU meanwhile
(Ch. 35, §5).

Granularity is one tick (10 ms), always rounded up.

### 12 — `lseek(int fd, int offset, int whence)`

`SEEK_SET` 0, `SEEK_CUR` 1, `SEEK_END` 2. A negative result is `-EINVAL`.

Meaningless on a console or a pipe, and not rejected — the offset moves and the driver ignores it.

### 13 — `readdir(int fd, dirent_t *out, unsigned index)`

Ours, not POSIX. Returns 1 for an entry, 0 past the end.

```c
typedef struct dirent {
    char     name[64];
    uint32_t inode;
    uint32_t type;      /* VFS_FILE, VFS_DIRECTORY, ... */
} dirent_t;
```

The index-based interface is simple across a syscall boundary; POSIX's opaque `DIR*` is nicer to use
and harder to implement (Ch. 46, §8.2).

The kernel copies the shared `dirent` to user space immediately, so the aliasing in
`ramdir_readdir` (Ch. 39, §4.1) is not visible to programs.

### 15 — `pipe(int fds[2])`

`fds[0]` is the read end, `fds[1]` the write end.

Reading with no writers returns 0 (EOF). Writing with no readers returns `-EPIPE` — and with no
signals, a program that ignores the return value loops forever (Ch. 43, §5.3).

### 16 — `dup2(int oldfd, int newfd)`

Makes `newfd` another name for `oldfd`, closing `newfd` first. `oldfd == newfd` returns early, before
the unref that would otherwise free the file.

The whole of redirection (Ch. 46, §5.1).

### 17 — `chdir(const char *path)`

Sets `current_task->cwd` to a node pointer. There is no path string, which is why `pwd` lies
(Ch. 46, §4.1).

### 21 — `uptime(void)`

Milliseconds since boot, as an `int32_t` — so it wraps after 24 days.

### 23 — `reboot(void)`

Pulses the CPU reset line through the 8042 (command `0xFE`). Never returns.

---

## Error numbers

| # | Name | Meaning |
|---|---|---|
| 1 | `EPERM` | operation not permitted |
| 2 | `ENOENT` | no such file or directory |
| 3 | `ESRCH` | no such process |
| 4 | `EINTR` | interrupted |
| 5 | `EIO` | I/O error |
| 7 | `E2BIG` | argument list too long |
| 8 | `ENOEXEC` | not an executable |
| 9 | `EBADF` | bad file descriptor |
| 10 | `ECHILD` | no child processes |
| 11 | `EAGAIN` | try again |
| 12 | `ENOMEM` | out of memory |
| 13 | `EACCES` | permission denied |
| 14 | `EFAULT` | **bad address — a user pointer we refused** |
| 16 | `EBUSY` | |
| 17 | `EEXIST` | |
| 19 | `ENODEV` | |
| 20 | `ENOTDIR` | |
| 21 | `EISDIR` | |
| 22 | `EINVAL` | invalid argument |
| 24 | `EMFILE` | too many open files |
| 28 | `ENOSPC` | no space left on device |
| 29 | `ESPIPE` | illegal seek |
| 30 | `EROFS` | read-only filesystem |
| 32 | `EPIPE` | broken pipe |
| 36 | `ENAMETOOLONG` | |
| 38 | `ENOSYS` | function not implemented |
| 39 | `ENOTEMPTY` | |

Same numbers as Linux where they exist.

**`EFAULT` is the interesting one.** It means the kernel refused a pointer — too high, wrapped,
unmapped, or lacking `PTE_USER`. Seeing it means a program passed something it should not have, and
the kernel caught it rather than faulting (Ch. 33, §5).

---

## Validation, in one place

Every user pointer goes through one of four functions:

```c
bool user_range_ok(const void *user_ptr, size_t n, bool need_write);
bool copy_from_user(void *dst, const void *user_src, size_t n);
bool copy_to_user(void *user_dst, const void *src, size_t n);
bool user_string_copy(char *dst, const char *user_src, size_t max);
```

`user_range_ok` checks three things:

1. The range is entirely below `KERNEL_VIRTUAL_BASE`.
2. `start + n` did not wrap.
3. Every page is present, user-accessible, and writable if needed.

Chapter 33, §5 has the attack each one prevents.

---

## Adding a call

1. `#define SYS_FOO 24` in `syscall.h`, and bump `SYS_MAX`.
2. `static int32_t sys_foo(registers_t *regs)` in `syscall.c`.
3. `[SYS_FOO] = sys_foo,` in the table.
4. A wrapper in `user/libc.c`.
5. A declaration in `user/libc.h`.

The handler's contract:

- Validate **every** pointer before touching it.
- Bound **every** length.
- Return a non-negative result or `-errno`.
- Never trust an integer to be in range.

---

## What is missing, and why

| Call | Why not | Would need |
|---|---|---|
| `kill`, `signal` | No signals | Ch. 46, §10.1 — about 300 lines |
| `mmap` | `sbrk` is enough here | A VMA list and lazy mapping |
| `mount` | Mounting is done in `kmain` | Path validation and a filesystem registry |
| `getcwd` | No path string | Ch. 46, §4.1 |
| `mkdir` | FAT16 subdirectory creation | A `.`/`..` pair and a cluster |
| `rename` | | Two directory updates, atomically |
| `select`, `poll` | | A per-node readiness query and multi-channel blocking |
| `ioctl` | | A per-driver command dispatch |
| `fcntl` | | Descriptor flags |
| `getuid`, `chmod` | **No users and no permissions** | An entire security model |

That last row is the honest one. Chapter 0 said it:

> **Nimbus has no security model beyond the ring 3 boundary.** There is one user, and it is root.

---

[Contents](README.md)
