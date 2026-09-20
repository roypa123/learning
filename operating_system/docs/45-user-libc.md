# Chapter 45 — A C library for Nimbus

[← The ELF loader](44-elf-loader.md) · [Contents](README.md) · [Next: The shell →](46-shell.md)

> 📖 **Line by line:** [crt0.asm + libc.c](line-by-line/nimbus-libc.md)

---

## Goal

Build what runs before `main`, and the library it needs. Syscall stubs, a `printf` that batches, a
`malloc` built on `sbrk`, and an honest look at what a real libc adds on top.

> There is no magic in a C library: `printf` is `vsnprintf` plus `write()`, `malloc` is a free list
> plus `sbrk()`, and `fopen` is `open()` plus a buffer.

---

## 1. What runs before `main`

Every C program on every system has a `crt0`, and almost nobody has read one.

> The kernel does not call main. It cannot: main's signature is a C convention, the kernel has no
> idea whether this program even has a main, and there is no way to "call" into a process that is not
> running yet. What the kernel does is far blunter — it points EIP at the ELF header's entry point
> and irets. Whatever is at that address is the program.

```nasm
_start:
    mov eax, [esp]                  ; argc
    mov ebx, [esp + 4]              ; argv

    xor ebp, ebp

    push ebx
    push eax
    call main
    add esp, 8

    push eax
    call exit

.hang:
    jmp .hang
```

Three jobs.

### 1.1 Turn the kernel's convention into C's

The kernel left the stack as (Chapter 34, §3.4):

```
    [esp]      argc
    [esp+4]    argv
```

cdecl wants them pushed right to left. Four instructions.

> That layout is a choice our kernel made, not a law. Linux puts argc, then argv, then a NULL, then
> envp, then a NULL, then the auxiliary vector — and a real crt0 has to walk past all of it to find
> envp.

The auxiliary vector is how the kernel tells the dynamic linker things like the page size, the
address of the program headers, and a random seed for stack canaries. `LD_SHOW_AUXV=1` on Linux
prints it.

### 1.2 Terminate the frame chain

```nasm
    xor ebp, ebp
```

> so a backtrace stops at `_start` rather than walking into whatever the kernel left on the stack
> below us.

Same reason as `entry.asm` (Chapter 9, §1.3), one privilege level down.

### 1.3 Turn `main`'s return into `exit`

```nasm
    call main
    add esp, 8

    push eax
    call exit
```

> main returned. Its value is in EAX, which is exactly where exit() wants its argument to come from —
> push it and call.
>
> This is why `int main()` without a return statement is defined to return 0 in C99 and undefined
> before it: without that rule, the exit status is whatever happened to be in EAX, which is usually
> the return value of the last function main called.

That explains a rule most C programmers know and cannot justify. C99 added the implicit `return 0`
specifically because the alternative was "whatever `printf` returned".

### 1.4 The hang that should never run

```nasm
    ; exit() never returns. If it somehow did, do not fall off the end of the
    ; program into unmapped memory -- stop here, visibly.
.hang:
    jmp .hang
```

Same principle as `kernel_thread_exit` (Chapter 29, §8.2) and the unreachable `panic` in `task_exit`
(Chapter 34, §4.4): an impossible path gets a defined behaviour anyway.

---

## 2. Syscall stubs

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

One template, four arities. Chapter 33, §1 covered the convention and the `"memory"` clobber.

```c
int   open(const char *path, int flags) { return syscall2(SYS_OPEN, (uint32_t)path, (uint32_t)flags); }
int   close(int fd)                     { return syscall1(SYS_CLOSE, (uint32_t)fd); }
pid_t fork(void)                        { return syscall0(SYS_FORK); }
```

Twenty-three one-line wrappers. This is genuinely what a C library's system call layer is: a cast and
an `int 0x80`.

### 2.1 The one that is not a pure wrapper

```c
void *sbrk(int increment)
{
    int32_t r = syscall1(SYS_SBRK, (uint32_t)increment);
    if (r < 0 && r > -4096) return (void *)-1;      /* an -errno, not an address */
    return (void *)r;
}
```

Chapter 33, §1.2: errors come back as small negative numbers. `sbrk` returns an *address*, so the
wrapper has to distinguish "a very high address" from "-ENOMEM".

The `> -4096` bound is the convention: no errno exceeds 4095, and no valid address falls in
`0xFFFFF000`–`0xFFFFFFFF` for a user process.

glibc does exactly this, in a macro called `INLINE_SYSCALL_ERROR_RETURN_VALUE`.

---

## 3. printf

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

The **same formatter** as the kernel — `lib/printf.c` is compiled into both — with a different sink
(Chapter 14, §2).

### 3.1 Batching is not an optimisation

> Every `write()` is a system call — an `int 0x80`, a privilege change, a dispatch, a VFS call. One
> per character would make `printf` hundreds of times slower than the formatting it wraps.

A syscall is roughly 250 cycles (Chapter 33, §2.1). Formatting a character is maybe 10. So
unbatched, 96% of `printf`'s time is the kernel transition.

256 bytes on the stack, flushed when full and at the end.

### 3.2 The buffering everyone eventually trips over

This is why `printf` output can appear out of order with respect to unbuffered writes:

```c
    printf("one\n");
    write(STDOUT_FILENO, "two\n", 4);
```

Ours flushes at the end of each `printf`, so this is fine. **glibc does not** — it buffers across
calls, flushing on a newline when stdout is a terminal and only when the buffer fills when it is a
pipe.

Which is why:

```bash
$ ./prog            # prints interleaved correctly
$ ./prog | cat      # prints all the printf output at the end
```

and why `stdbuf -o0` exists. Adding cross-call buffering to ours would reproduce the behaviour
exactly. Exercise 45.5.

---

## 4. malloc

```c
typedef struct mblock {
    size_t         size;
    struct mblock *next;
    int            free;
} mblock_t;

static mblock_t *malloc_head = NULL;
```

> The same first-fit free list as the kernel heap, minus the growable arena and plus `sbrk`. About
> sixty lines, and it is genuinely what malloc was until the 1990s.

### 4.1 Growing

```c
    size_t want = size + MBLOCK_HEADER;
    if (want < 4096) want = 4096;

    uint8_t *mem = (uint8_t *)sbrk((int)want);
    if (mem == (uint8_t *)-1) return NULL;
```

> We request at least 4 KiB regardless of the size asked for, because sbrk is a system call and doing
> one per malloc would be absurd — the whole point of a user-space allocator is to make the common
> case not involve the kernel at all.

That sentence is the *reason malloc exists*. The kernel could expose an allocator directly; it does
not, because a syscall per allocation would cost 250 cycles for a 16-byte object.

### 4.2 Forward-only coalescing

```c
void free(void *ptr)
{
    if (!ptr) return;

    mblock_t *b = (mblock_t *)((uint8_t *)ptr - MBLOCK_HEADER);
    b->free = 1;

    while (b->next && b->next->free &&
           (uint8_t *)b + MBLOCK_HEADER + b->size == (uint8_t *)b->next) {
        b->size += MBLOCK_HEADER + b->next->size;
        b->next  = b->next->next;
    }
}
```

> Coalesce forwards only, and only with an immediate neighbour. A singly-linked list cannot merge
> backwards without a scan, which is one of the reasons the kernel's heap uses a doubly-linked one.
> The cost of this shortcut is that a free-in-reverse-order pattern fragments; the benefit is eight
> bytes per allocation.

Concretely: allocate A, B, C; free C, then B, then A. Each free merges forward with the already-free
next block, so it works. Free A, then B, then C and nothing merges, because each block's *previous*
neighbour is the free one.

The kernel's version (Chapter 27, §7) merges both ways because it has `prev`.

### 4.3 The `while` versus the kernel's two `if`s

The kernel coalesces forward once and backward once. This one loops forward.

Both are correct; the loop handles three or more consecutive free blocks in one pass, which can
happen here because `free` does not merge backwards and so free blocks accumulate.

### 4.4 No magic numbers

The kernel's heap has them (Chapter 27, §4); this one does not.

That is a deliberate asymmetry. A corrupted user heap kills one process; a corrupted kernel heap
kills the machine. The four bytes and the check are worth it in one place and arguably not in the
other.

It also means a double free in userland corrupts the list silently, which is exactly the bug class
that made `malloc` hardening a research area. Exercise 45.6.

### 4.5 What a modern malloc adds

> size classes (so small allocations do not search), per-thread arenas (so two threads do not contend
> on one lock), `mmap` for large blocks (so a 10 MiB allocation can be returned to the OS
> independently), and a great deal of care about the fact that `sbrk` can only shrink from the top.

That last one is the structural limitation. `sbrk` moves one boundary, so memory can only be returned
to the kernel if the *last* thing allocated is the first thing freed.

Which is why every real `malloc` keeps freed memory rather than returning it, and why a process's RSS
rarely goes down.

---

## 5. `run`: the three-line idiom

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

> `execv` only returns if it failed, because on success there is no longer any code here to return to
> — the address space has been replaced. That is why every exec is followed by an error path and
> never by an `if`.

Chapter 34, §6.2. And 127 is the shell convention for "command not found", which is why `echo $?`
after a typo gives 127 on any Unix.

This is `system()` without the shell, and it is worth noticing how small it is.

---

## 6. getline

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

> The console hands us the newline, because that is what a terminal read returns. Strip it: every
> caller wants the line, not the line plus a character it has to remember to remove.

Chapter 20, §3.1: the kernel keeps the newline in the buffer, libc strips it.

> Two layers, two conventions, each right for its level. Conflating them is how you end up with a
> shell that prints a blank line after every command.

`n <= 0` returns −1 for both EOF (0) and error (negative), which conflates them. POSIX `getline`
distinguishes; ours does not, and the shell treats both as "stop".

---

## 7. What we are missing

| Missing | Why | Would need |
|---|---|---|
| `stdio` (`FILE *`, `fopen`) | fd-based I/O is enough here | A `FILE` struct, buffering, `fseek`, `ungetc` |
| Cross-call buffering | §3.2 | A per-stream buffer and a flush policy |
| `errno` | We return `-errno` directly | A global, set by every stub |
| `atexit` | No static destructors | A table of function pointers, called by `exit` |
| Static constructors | Nothing needs them | `.init_array` walked by crt0 |
| `environ`/`getenv` | No environment | argv+envp on the stack, and a shell that sets it |
| Locales, wide chars | No | A great deal |
| `math.h` | No FPU in userland | FPU state saving in the kernel first |
| Threads | No threads in the kernel | Everything |

### 7.1 `errno` specifically

```c
    int fd = open("nope", O_RDONLY);
    if (fd < 0) {
        /* fd is -ENOENT. On a real system it would be -1 and errno == ENOENT. */
    }
```

Our stubs return the negative errno directly. POSIX returns −1 and sets a global.

The global is a genuine nuisance — it has to be thread-local, which is why glibc's `errno` is a macro
expanding to `*__errno_location()` — and the direct return is cleaner.

It is also incompatible with every piece of C code ever written. Adding it is ten lines and makes
ported code work. Exercise 45.4.

### 7.2 Static constructors

```c
__attribute__((constructor)) static void init(void) { ... }
```

GCC puts the function pointer in `.init_array`, and crt0 is supposed to walk it before `main`. C++
static object constructors use the same mechanism.

Our `user.ld` does not even keep the section, so a constructor would be silently discarded. Fifteen
lines in `crt0.asm` and one in the linker script. Exercise 45.7.

---

## 8. Building it

```make
USER_LIB = bin/user/crt0.o bin/user/libc.o bin/user/string.o bin/user/printf.o

bin/user/sh: nimbus/user/sh.c $(USER_LIB) nimbus/user/user.ld
	$(CC) $(CFLAGS) -c $< -o bin/user/sh.o
	$(LD) -m elf_i386 -T nimbus/user/user.ld -o $@ $(USER_LIB) bin/user/sh.o
```

**`crt0.o` first on the link line.** Not because the linker script demands it — `user.ld` has
`*(.text.start)` first as belt and braces — but because it is the convention and because a
disassembly then reads in the order the program runs.

Note `string.o` and `printf.o` come from `nimbus/lib/`, the *same* source files the kernel uses:

```make
bin/user/string.o: nimbus/lib/string.c
	$(CC) $(CFLAGS) -c $< -o $@
```

compiled twice with different flags — the kernel's copy gets `-DNIMBUS_KERNEL`. Two object files,
one source.

### 8.1 The same flags as the kernel

```make
CFLAGS   = -std=gnu11 -m32 -ffreestanding -nostdlib -fno-builtin ...
```

Userland is built with `-ffreestanding` and `-nostdlib` too, which surprises people: it is a *user*
program, so surely it is hosted?

No — "hosted" means a C library exists, and the only one that exists is the one we are building. The
program is freestanding with respect to the toolchain even though it runs under an operating system.

---

## 9. Running it

```
nimbus> echo hello world
hello world
nimbus> sleep 1000
slept 1010 ms (asked for 1000)
nimbus> forktest
parent pid is 4
...
```

Three programs, all linked against the same four object files, all working.

### 9.1 Looking at the binary

```bash
$ i686-elf-objdump -d bin/user/echo | head -20

08048080 <_start>:
 8048080:  8b 04 24       mov    eax,DWORD PTR [esp]
 8048083:  8b 5c 24 04    mov    ebx,DWORD PTR [esp+0x4]
 8048087:  31 ed          xor    ebp,ebp
 8048089:  53             push   ebx
 804808a:  50             push   eax
 804808b:  e8 40 00 00 00 call   80480d0 <main>
 8048090:  83 c4 08       add    esp,0x8
 8048093:  50             push   eax
 8048094:  e8 17 01 00 00 call   80481b0 <exit>
 8048099:  eb fe          jmp    8048099 <_start+0x19>
```

`eb fe` is `jmp $` — the hang from §1.4, and one of the byte sequences worth recognising
(Chapter 3, §7).

```bash
$ i686-elf-objdump -d bin/user/echo | grep -A4 "<write>:"

080481c0 <write>:
 80481c0:  53             push   ebx
 80481c1:  8b 54 24 14    mov    edx,DWORD PTR [esp+0x14]
 80481c5:  8b 4c 24 10    mov    ecx,DWORD PTR [esp+0x10]
 80481c9:  8b 5c 24 0c    mov    ebx,DWORD PTR [esp+0xc]
 80481cd:  b8 01 00 00 00 mov    eax,0x1
 80481d2:  cd 80          int    0x80
```

Arguments into `EBX`, `ECX`, `EDX`; the call number into `EAX`; `int 0x80`. Six instructions, and
that is a system call.

### 9.2 Sizes

```
sh                       6436 bytes
ls                       5120 bytes
echo                     4896 bytes
true                     4688 bytes
```

`true` is `int main(void) { return 0; }` and it is 4688 bytes, because it links the whole of
`libc.o`, `string.o` and `printf.o` whether it uses them or not.

**The linker's unit is the object file**, not the function. `printf` is in `printf.o`, so linking
anything from that file brings all of it.

The fix is `-ffunction-sections -fdata-sections` when compiling and `--gc-sections` when linking,
which puts every function in its own section and discards the unreferenced ones. Exercise 45.8; it
takes `true` to about 600 bytes.

---

## 10. Exercises

🟢 **45.1** Disassemble `_start` in any binary and match it against `crt0.asm` line by line.

🟢 **45.2** Write a `main` with no `return` and print the exit status from the shell. Then add
`return 0` and compare.

🟢 **45.3** Count the syscalls `echo hello` makes, using the trace from Exercise 33.1.

🟡 **45.4** Add `errno`: a global, set by every stub, with the stubs returning −1. Then update all
the utilities and add `perror`.

🟡 **45.5** Add cross-call buffering to `printf` with a line-buffered policy for terminals and
block-buffered for pipes. Reproduce the `./prog | cat` reordering from §3.2, then add `fflush`.

🟡 **45.6** Add magic numbers to the userland `malloc` and confirm a double free is caught.

🟡 **45.7** Support `.init_array`: keep the section in `user.ld`, and walk it in `crt0.asm` before
`main`. Test with `__attribute__((constructor))`.

🔴 **45.8** Add `-ffunction-sections -fdata-sections --gc-sections` and measure every binary before
and after.

🔴 **45.9** Implement `stdio`: a `FILE` struct with a buffer and a mode, `fopen`/`fclose`/`fgets`/
`fputs`/`fprintf`/`fseek`, and the three standard streams. Then rewrite `cat` to use it.

---

## What we covered

- Why the kernel cannot call `main`, and the three jobs `crt0` does instead.
- The argv layout as a choice, and what Linux's auxiliary vector carries.
- `xor ebp, ebp`, and `main`'s return becoming `exit`'s argument — which explains C99's implicit
  `return 0`.
- Twenty-three one-line syscall wrappers, and the one that has to distinguish an address from an
  error.
- The same formatter as the kernel with a batching sink, and why unbatched `printf` would be 96%
  kernel transition.
- The buffering behaviour that makes `./prog | cat` reorder output, and why `stdbuf` exists.
- `malloc` as a free list plus `sbrk`, requesting 4 KiB minimum because a syscall per allocation
  would defeat the point.
- Forward-only coalescing, what it costs, and why the kernel's version differs.
- No magic numbers in userland, deliberately, and what that gives up.
- `sbrk`'s single boundary, and why RSS rarely goes down.
- Nine things a real libc has that ours does not, with what each would take.
- Userland built `-ffreestanding` even though it runs under an OS, and why that is not a
  contradiction.
- A 4688-byte `true`, and the linker granularity that explains it.

[Chapter 46](46-shell.md) writes the program that makes all of it visible.

---

[← The ELF loader](44-elf-loader.md) · [Contents](README.md) · [Next: The shell →](46-shell.md)
