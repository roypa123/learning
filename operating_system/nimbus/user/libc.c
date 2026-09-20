/* ===========================================================================
 *  nimbus/user/libc.c  --  the C library
 * ===========================================================================
 *  Explained in: docs/45-user-libc.md
 *  Line by line: docs/line-by-line/nimbus-libc.md
 * =========================================================================== */

#include "libc.h"

/* ===========================================================================
 *  System call stubs
 *
 *  One inline-assembly template, six times. The constraints say: put the call
 *  number in EAX, argument 1 in EBX, argument 2 in ECX, argument 3 in EDX,
 *  execute `int 0x80`, and take the result out of EAX.
 *
 *  "memory" in the clobber list is essential and easy to leave out. It tells
 *  GCC that the instruction may read or write any memory, which stops it
 *  caching a value in a register across a read() that is about to overwrite
 *  the buffer that value came from. Without it, code like
 *
 *      buf[0] = 'x';  read(fd, buf, 10);  if (buf[0] == 'x') ...
 *
 *  can be "optimised" into a comparison that is always true.
 * =========================================================================== */

static inline int32_t syscall0(uint32_t n)
{
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(n) : "memory");
    return ret;
}

static inline int32_t syscall1(uint32_t n, uint32_t a)
{
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(n), "b"(a) : "memory");
    return ret;
}

static inline int32_t syscall2(uint32_t n, uint32_t a, uint32_t b)
{
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b) : "memory");
    return ret;
}

static inline int32_t syscall3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t ret;
    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(n), "b"(a), "c"(b), "d"(c)
                      : "memory");
    return ret;
}

/* ---- the thin wrappers ---------------------------------------------------- */

void exit(int status)
{
    syscall1(SYS_EXIT, (uint32_t)status);
    for (;;) { }                 /* unreachable; keeps GCC's noreturn happy */
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, count);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return syscall3(SYS_READ, (uint32_t)fd, (uint32_t)buf, count);
}

int   open(const char *path, int flags) { return syscall2(SYS_OPEN, (uint32_t)path, (uint32_t)flags); }
int   close(int fd)                     { return syscall1(SYS_CLOSE, (uint32_t)fd); }
pid_t fork(void)                        { return syscall0(SYS_FORK); }
int   execv(const char *p, char *const a[]) { return syscall2(SYS_EXEC, (uint32_t)p, (uint32_t)a); }
pid_t wait(int *status)                 { return syscall1(SYS_WAIT, (uint32_t)status); }
pid_t getpid(void)                      { return syscall0(SYS_GETPID); }
int   sleep_ms(unsigned ms)             { return syscall1(SYS_SLEEP, ms); }
int   yield(void)                       { return syscall0(SYS_YIELD); }
int   lseek(int fd, int off, int wh)    { return syscall3(SYS_LSEEK, (uint32_t)fd, (uint32_t)off, (uint32_t)wh); }
int   stat(const char *p, stat_t *o)    { return syscall2(SYS_STAT, (uint32_t)p, (uint32_t)o); }
int   pipe(int fds[2])                  { return syscall1(SYS_PIPE, (uint32_t)fds); }
int   dup2(int o, int n)                { return syscall2(SYS_DUP2, (uint32_t)o, (uint32_t)n); }
int   chdir(const char *p)              { return syscall1(SYS_CHDIR, (uint32_t)p); }
int   unlink(const char *p)             { return syscall1(SYS_UNLINK, (uint32_t)p); }
unsigned uptime_ms(void)                { return (unsigned)syscall0(SYS_UPTIME); }
int   ps(void)                          { return syscall0(SYS_PS); }
int   reboot(void)                      { return syscall0(SYS_REBOOT); }

int readdir_fd(int fd, dirent_t *out, unsigned index)
{
    return syscall3(SYS_READDIR, (uint32_t)fd, (uint32_t)out, index);
}

void *sbrk(int increment)
{
    int32_t r = syscall1(SYS_SBRK, (uint32_t)increment);
    if (r < 0 && r > -4096) return (void *)-1;      /* an -errno, not an address */
    return (void *)r;
}

/* ===========================================================================
 *  printf
 *
 *  The formatter is the same code the kernel uses -- lib/printf.c is compiled
 *  into both -- with a different sink. Here the sink batches into a buffer and
 *  flushes with one write(), because every write() is a system call and a
 *  system call per character would make printf hundreds of times slower than
 *  the string formatting it is wrapped around.
 * =========================================================================== */

typedef struct {
    int    fd;
    char   buf[256];
    size_t pos;
} fd_sink_t;

static void fd_flush(fd_sink_t *s)
{
    if (s->pos) {
        write(s->fd, s->buf, s->pos);
        s->pos = 0;
    }
}

static void fd_putc(void *ctx, char c)
{
    fd_sink_t *s = (fd_sink_t *)ctx;
    s->buf[s->pos++] = c;
    if (s->pos == sizeof(s->buf)) fd_flush(s);
}

int fprintf(int fd, const char *fmt, ...)
{
    fd_sink_t sink = { fd, { 0 }, 0 };

    va_list ap;
    va_start(ap, fmt);
    int n = fmt_vformat(fd_putc, &sink, fmt, ap);
    va_end(ap);

    fd_flush(&sink);
    return n;
}

int printf(const char *fmt, ...)
{
    fd_sink_t sink = { STDOUT_FILENO, { 0 }, 0 };

    va_list ap;
    va_start(ap, fmt);
    int n = fmt_vformat(fd_putc, &sink, fmt, ap);
    va_end(ap);

    fd_flush(&sink);
    return n;
}

int puts(const char *s)
{
    size_t len = strlen(s);
    write(STDOUT_FILENO, s, len);
    write(STDOUT_FILENO, "\n", 1);
    return (int)len + 1;
}

int putchar(int c)
{
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
    return c;
}

int getline(char *buf, size_t max)
{
    ssize_t n = read(STDIN_FILENO, buf, max - 1);
    if (n <= 0) return -1;

    buf[n] = '\0';

    /*  The console hands us the newline, because that is what a terminal read
     *  returns. Strip it: every caller wants the line, not the line plus a
     *  character it has to remember to remove.                                */
    if (n > 0 && buf[n - 1] == '\n') buf[--n] = '\0';

    return (int)n;
}

/* ===========================================================================
 *  malloc
 *
 *  The same first-fit free list as the kernel heap, minus the growable arena
 *  and plus sbrk. About sixty lines, and it is genuinely what malloc was until
 *  the 1990s.
 *
 *  What a modern malloc adds, and why: size classes (so small allocations do
 *  not search), per-thread arenas (so two threads do not contend on one lock),
 *  mmap for large blocks (so a 10 MiB allocation can be returned to the OS
 *  independently), and a great deal of care about the fact that sbrk can only
 *  shrink from the top.
 * =========================================================================== */

typedef struct mblock {
    size_t         size;        /* payload bytes                              */
    struct mblock *next;
    int            free;
} mblock_t;

static mblock_t *malloc_head = NULL;

#define MBLOCK_HEADER sizeof(mblock_t)

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    size = (size + 7) & ~(size_t)7;

    for (mblock_t *b = malloc_head; b; b = b->next) {
        if (!b->free || b->size < size) continue;

        if (b->size >= size + MBLOCK_HEADER + 16) {
            mblock_t *rest = (mblock_t *)((uint8_t *)b + MBLOCK_HEADER + size);
            rest->size = b->size - size - MBLOCK_HEADER;
            rest->free = 1;
            rest->next = b->next;
            b->next = rest;
            b->size = size;
        }

        b->free = 0;
        return (uint8_t *)b + MBLOCK_HEADER;
    }

    /*  Nothing fitted. Ask the kernel for more address space.
     *
     *  We request at least 4 KiB regardless of the size asked for, because
     *  sbrk is a system call and doing one per malloc would be absurd -- the
     *  whole point of a user-space allocator is to make the common case not
     *  involve the kernel at all.                                            */
    size_t want = size + MBLOCK_HEADER;
    if (want < 4096) want = 4096;

    uint8_t *mem = (uint8_t *)sbrk((int)want);
    if (mem == (uint8_t *)-1) return NULL;

    mblock_t *b = (mblock_t *)mem;
    b->size = want - MBLOCK_HEADER;
    b->free = 0;
    b->next = NULL;

    if (!malloc_head) {
        malloc_head = b;
    } else {
        mblock_t *last = malloc_head;
        while (last->next) last = last->next;
        last->next = b;
    }

    /*  Split off the remainder so the extra bytes we asked for are usable.   */
    if (b->size >= size + MBLOCK_HEADER + 16) {
        mblock_t *rest = (mblock_t *)((uint8_t *)b + MBLOCK_HEADER + size);
        rest->size = b->size - size - MBLOCK_HEADER;
        rest->free = 1;
        rest->next = b->next;
        b->next = rest;
        b->size = size;
    }

    return mem + MBLOCK_HEADER;
}

void free(void *ptr)
{
    if (!ptr) return;

    mblock_t *b = (mblock_t *)((uint8_t *)ptr - MBLOCK_HEADER);
    b->free = 1;

    /*  Coalesce forwards only, and only with an immediate neighbour. A
     *  singly-linked list cannot merge backwards without a scan, which is one
     *  of the reasons the kernel's heap uses a doubly-linked one. The cost of
     *  this shortcut is that a free-in-reverse-order pattern fragments; the
     *  benefit is eight bytes per allocation.                                 */
    while (b->next && b->next->free &&
           (uint8_t *)b + MBLOCK_HEADER + b->size == (uint8_t *)b->next) {
        b->size += MBLOCK_HEADER + b->next->size;
        b->next  = b->next->next;
    }
}

void *calloc(size_t count, size_t size)
{
    if (count && size > (size_t)0xFFFFFFFFu / count) return NULL;

    size_t total = count * size;
    void  *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return NULL; }

    mblock_t *b = (mblock_t *)((uint8_t *)ptr - MBLOCK_HEADER);
    if (b->size >= size) return ptr;

    void *fresh = malloc(size);
    if (!fresh) return NULL;

    memcpy(fresh, ptr, b->size);
    free(ptr);
    return fresh;
}

/* ===========================================================================
 *  run -- fork, exec, wait
 * =========================================================================== */
int run(const char *path, char *const argv[])
{
    pid_t pid = fork();

    if (pid < 0) return -1;

    if (pid == 0) {
        /*  The child. execv only returns if it failed, because on success
         *  there is no longer any code here to return to -- the address space
         *  has been replaced. That is why every exec is followed by an error
         *  path and never by an `if`.                                         */
        execv(path, argv);
        fprintf(STDERR_FILENO, "%s: not found\n", path);
        exit(127);      /* 127 is the shell convention for "command not found" */
    }

    int status = 0;
    wait(&status);
    return status;
}
