# Line by line: `nimbus/user/crt0.asm` and `user/libc.c`

[Index](README.md) · [Chapter 45](../45-user-libc.md)

---

# `crt0.asm`

```nasm
global _start
extern main
extern exit
```
`_start`, not `main`.

> The kernel does not call main. It cannot: main's signature is a C convention, the kernel has no
> idea whether this program even has a main, and there is no way to "call" into a process that is not
> running yet. What the kernel does is far blunter — it points EIP at the ELF header's entry point
> and irets. Whatever is at that address is the program.

```nasm
    mov eax, [esp]                  ; argc
    mov ebx, [esp + 4]              ; argv
```
The layout `task_exec_regs` built:

```
    [esp]      argc
    [esp+4]    argv
```

⚠️ **A choice our kernel made, not a law.**

> Linux puts argc, then argv, then a NULL, then envp, then a NULL, then the auxiliary vector — and a
> real crt0 has to walk past all of it to find envp.

The auxiliary vector is how the kernel tells the dynamic linker the page size, the address of the
program headers, and a random seed for stack canaries. `LD_SHOW_AUXV=1` prints it.

```nasm
    xor ebp, ebp
```
⚠️ Terminates the frame-pointer chain:

> so a backtrace stops at `_start` rather than walking into whatever the kernel left on the stack
> below us.

Same reason as `entry.asm` and `boot.asm`, one privilege level down.

```nasm
    push ebx
    push eax
    call main
    add esp, 8
```
cdecl: right to left, caller cleans up.

```nasm
    push eax
    call exit
```
⚠️ `main`'s return value is already in `EAX`, which is exactly where `exit`'s argument comes from.

> This is why `int main()` without a return statement is defined to return 0 in C99 and undefined
> before it: without that rule, the exit status is whatever happened to be in EAX, which is usually
> the return value of the last function main called.

A rule most C programmers know and cannot justify. C99 added the implicit `return 0` because the
alternative was "whatever `printf` returned".

```nasm
.hang:
    jmp .hang
```
`exit` never returns. If it somehow did, stop visibly rather than falling off the end into unmapped
memory.

`eb fe` in a disassembly — one of the byte sequences worth recognising.

---

# `libc.c`

## The syscall stubs

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
One template, four arities.

⚠️ **`"memory"` is not optional.**

```c
    buf[0] = 'x';  read(fd, buf, 10);  if (buf[0] == 'x') ...
```

Without it, GCC believes `int $0x80` cannot touch `buf`, keeps `'x'` in a register, and compiles the
comparison to `true`.

⚠️ `volatile` for the same class of reason: without it the block is a pure function of its inputs and
may be deleted, hoisted or reused.

```c
int   open(const char *path, int flags) { return syscall2(SYS_OPEN, (uint32_t)path, (uint32_t)flags); }
int   close(int fd)                     { return syscall1(SYS_CLOSE, (uint32_t)fd); }
pid_t fork(void)                        { return syscall0(SYS_FORK); }
```
Twenty-three one-line wrappers. This is genuinely what a C library's system call layer is: a cast and
an `int 0x80`.

```c
void exit(int status)
{
    syscall1(SYS_EXIT, (uint32_t)status);
    for (;;) { }
}
```
The infinite loop is unreachable and keeps GCC's `noreturn` attribute satisfied.

---

## `sbrk` — the one that is not a pure wrapper

```c
void *sbrk(int increment)
{
    int32_t r = syscall1(SYS_SBRK, (uint32_t)increment);
    if (r < 0 && r > -4096) return (void *)-1;      /* an -errno, not an address */
    return (void *)r;
}
```
⚠️ Errors come back as small negative numbers, but `sbrk` returns an **address** — so the wrapper has
to distinguish "a very high address" from `-ENOMEM`.

The `> -4096` bound is the convention: no errno exceeds 4095, and no valid user address falls in
`0xFFFFF000`–`0xFFFFFFFF`.

glibc does exactly this, in a macro called `INLINE_SYSCALL_ERROR_RETURN_VALUE`.

---

## `printf`

```c
typedef struct {
    int    fd;
    char   buf[256];
    size_t pos;
} fd_sink_t;

static void fd_putc(void *ctx, char c)
{
    fd_sink_t *s = (fd_sink_t *)ctx;
    s->buf[s->pos++] = c;
    if (s->pos == sizeof(s->buf)) fd_flush(s);
}
```
⚠️ **Batching is not an optimisation, it is the difference between usable and not.**

> Every `write()` is a system call — an `int 0x80`, a privilege change, a dispatch, a VFS call. One
> per character would make `printf` hundreds of times slower than the formatting it wraps.

A syscall is ~250 cycles; formatting a character is ~10. Unbatched, 96% of `printf`'s time is the
kernel transition.

```c
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
```
**The same formatter as the kernel** — `lib/printf.c` compiled into both — with a different sink.

⚠️ Flushed at the end of **each call**. glibc does not: it buffers across calls, flushing on a newline
when stdout is a terminal and only when full when it is a pipe.

Which is why:

```bash
$ ./prog            # interleaves correctly
$ ./prog | cat      # all the printf output at the end
```

and why `stdbuf -o0` exists.

---

## `getline`

```c
int getline(char *buf, size_t max)
{
    ssize_t n = read(STDIN_FILENO, buf, max - 1);
    if (n <= 0) return -1;

    buf[n] = '\0';

    if (n > 0 && buf[n - 1] == '\n') buf[--n] = '\0';

    return (int)n;
}
```
⚠️ The kernel keeps the newline in the buffer; libc strips it.

> Two layers, two conventions, each right for its level. Conflating them is how you end up with a
> shell that prints a blank line after every command.

`n <= 0` conflates EOF (0) and error (negative). POSIX `getline` distinguishes; ours does not, and the
shell treats both as "stop".

---

## `malloc`

```c
typedef struct mblock {
    size_t         size;
    struct mblock *next;
    int            free;
} mblock_t;
```
Twelve bytes, singly linked.

⚠️ **No magic numbers**, unlike the kernel heap. A deliberate asymmetry: a corrupted user heap kills
one process; a corrupted kernel heap kills the machine.

It also means a double free in userland corrupts the list silently — exactly the bug class that made
`malloc` hardening a research area.

```c
    size_t want = size + MBLOCK_HEADER;
    if (want < 4096) want = 4096;

    uint8_t *mem = (uint8_t *)sbrk((int)want);
    if (mem == (uint8_t *)-1) return NULL;
```
⚠️ **4 KiB minimum**, and this is the *reason malloc exists*:

> sbrk is a system call and doing one per malloc would be absurd — the whole point of a user-space
> allocator is to make the common case not involve the kernel at all.

The kernel could expose an allocator directly; it does not, because a syscall per allocation would
cost 250 cycles for a 16-byte object.

```c
    if (b->size >= size + MBLOCK_HEADER + 16) {
        mblock_t *rest = (mblock_t *)((uint8_t *)b + MBLOCK_HEADER + size);
        rest->size = b->size - size - MBLOCK_HEADER;
        rest->free = 1;
        rest->next = b->next;
        b->next = rest;
        b->size = size;
    }
```
Split the remainder so the extra bytes we asked for are usable. Appears twice — once on the free-list
path, once after growing.

```c
void free(void *ptr)
{
    mblock_t *b = (mblock_t *)((uint8_t *)ptr - MBLOCK_HEADER);
    b->free = 1;

    while (b->next && b->next->free &&
           (uint8_t *)b + MBLOCK_HEADER + b->size == (uint8_t *)b->next) {
        b->size += MBLOCK_HEADER + b->next->size;
        b->next  = b->next->next;
    }
}
```
⚠️ **Forwards only.**

> A singly-linked list cannot merge backwards without a scan, which is one of the reasons the
> kernel's heap uses a doubly-linked one. The cost of this shortcut is that a free-in-reverse-order
> pattern fragments; the benefit is eight bytes per allocation.

Concretely: allocate A, B, C; free C, B, A and each merges forward. Free A, B, C and nothing merges.

⚠️ The adjacency test is here too, for the same reason as the kernel's: `sbrk` regions need not be
contiguous.

A `while`, not two `if`s — three or more consecutive free blocks merge in one pass, which happens
because `free` does not merge backwards and so free blocks accumulate.

```c
void *calloc(size_t count, size_t size)
{
    if (count && size > (size_t)0xFFFFFFFFu / count) return NULL;
```
The overflow check, same as `kcalloc`.

---

## `run`

```c
int run(const char *path, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execv(path, argv);
        fprintf(STDERR_FILENO, "%s: not found\n", path);
        exit(127);
    }

    int status = 0;
    wait(&status);
    return status;
}
```
⚠️ **`execv` only returns on failure**, because on success there is no longer any code here to return
to — the address space has been replaced.

> That is why every exec is followed by an error path and never by an `if`.

127 is the shell convention for "command not found", which is why `echo $?` after a typo gives 127 on
any Unix.

This is `system()` without the shell.

---

## What is missing

| Missing | Why | Would need |
|---|---|---|
| `stdio` (`FILE *`) | fd I/O is enough | A struct, buffering, `fseek`, `ungetc` |
| Cross-call buffering | see `printf` above | A per-stream buffer and a flush policy |
| `errno` | we return `-errno` directly | A global, set by every stub |
| `atexit` | no static destructors | A table called by `exit` |
| `.init_array` | nothing needs it | A loop in crt0 and a line in `user.ld` |
| `environ` / `getenv` | no environment | envp on the stack, and a shell that sets it |
| `math.h` | no FPU in userland | FPU state saving in the kernel first |

⚠️ On `errno`: ours return the negative errno directly, which is cleaner than a global that has to be
thread-local. It is also incompatible with every piece of C code ever written.

---

## Build notes

```make
bin/user/string.o: nimbus/lib/string.c
	$(CC) $(CFLAGS) -c $< -o $@
```
⚠️ The **same source files** the kernel uses, compiled twice with different flags — the kernel's copy
gets `-DNIMBUS_KERNEL`. Two object files, one source.

```make
CFLAGS = -std=gnu11 -m32 -ffreestanding -nostdlib ...
```
⚠️ Userland is built `-ffreestanding` too, which surprises people.

"Hosted" means a C library exists, and the only one that exists is the one being built. The program
is freestanding with respect to the *toolchain* even though it runs under an operating system.

```
true                     4688 bytes
```
⚠️ `int main(void) { return 0; }` is 4688 bytes, because **the linker's unit is the object file**, not
the function. `printf` is in `printf.o`, so linking anything from that file brings all of it.

`-ffunction-sections -fdata-sections` plus `--gc-sections` takes it to about 600.

---

[Index](README.md) · [Chapter 45](../45-user-libc.md)
