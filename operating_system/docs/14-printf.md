# Chapter 14 — printf from nothing

[← The serial port](13-serial-port.md) · [Contents](README.md) · [Next: The GDT →](15-gdt.md)

---

## Goal

Write the formatter. It is the single most useful function in a kernel, it is completely
self-contained, and building it teaches two things worth knowing: how varargs actually work on x86,
and why an output callback is a better interface than a buffer.

One implementation, in [`lib/printf.c`](../nimbus/lib/printf.c), serves `snprintf`, `kprintf`,
`klog`, `panic` and userland's `printf`.

---

## 1. Varargs, and why they work

```c
void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
```

On 32-bit x86 with cdecl, this is almost embarrassingly simple, and understanding why makes the whole
mechanism obvious.

Arguments are pushed right to left, so they end up in memory in left-to-right order at increasing
addresses:

```
    kprintf("%d %s", 42, "hi");

    high    "hi"          <- [ebp + 16]
            42            <- [ebp + 12]
            "%d %s"       <- [ebp + 8]
            return addr   <- [ebp + 4]
    low     saved ebp     <- [ebp]
```

So "the arguments after `fmt`" is simply "the memory after `fmt`", and a `va_list` is a pointer.

```c
typedef __builtin_va_list   va_list;
#define va_start(v, l)      __builtin_va_start(v, l)
#define va_arg(v, t)        __builtin_va_arg(v, t)
#define va_end(v)           __builtin_va_end(v)
```

`va_start(ap, fmt)` sets the pointer to just past `fmt`. `va_arg(ap, int)` reads four bytes and
advances four. `va_end` does nothing.

We use GCC's builtins rather than `<stdarg.h>` — which is a freestanding header and would work —
because they *are* the implementation of that header, and using them directly removes one include
from the build.

> 🔧 **On x86-64 this is not true.** The first six integer arguments go in registers, floats go in
> different registers, and `va_list` becomes a struct with two indices and a pointer to a spill area
> the prologue had to create. It is about forty lines of ABI. The 32-bit simplicity is a genuine
> pedagogical advantage.

### 1.1 Default argument promotions

```c
        case 'c': {
            char c = (char)va_arg(ap, int);   /* char is promoted to int */
```

Anything passed through `...` is promoted: `char` and `short` become `int`, `float` becomes `double`.
So `va_arg(ap, char)` is undefined behaviour — you must read an `int` and narrow it yourself.

This is one of the rules that C programmers know and cannot quite say why. The reason is exactly this
stack layout: everything occupies at least four bytes, so reading a `char` would advance the pointer
by one and desynchronise every argument after it.

### 1.2 Why `__attribute__((format(printf, n, m)))` matters

```c
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
```

This makes GCC type-check the format string exactly as it does for the real `printf`. Passing a
`uint64_t` to `%d` becomes a warning instead of two garbage columns of hex at 3 a.m.

In a kernel there is no runtime to catch a mismatch, and a `%s` given an integer dereferences it as a
pointer — a page fault inside the logger, which is the worst place for one. One attribute, and the
whole class goes away at compile time.

---

## 2. The design: an output callback

```c
typedef void (*fmt_out_fn)(void *ctx, char c);

int fmt_vformat(fmt_out_fn out, void *ctx, const char *fmt, va_list ap);
```

The formatter never writes to a buffer. It calls `out(ctx, c)` for each character.

That one indirection gives us four things from one implementation:

| Sink | `ctx` | Used by |
|---|---|---|
| Buffer, counting past the end | a `buf_sink_t` | `snprintf` |
| Screen + serial | `NULL` | `kprintf` |
| Serial only | `NULL` | `klog`, `panic` |
| Batched `write()` syscall | an fd and a buffer | userland `printf` |

The obvious alternative — format into a fixed buffer, then output the buffer — is what most hobby
kernels do, and it has two problems. It imposes a maximum line length, and it puts a few hundred
bytes on the stack of every caller, including interrupt handlers whose stacks you would rather keep
shallow.

The callback costs one indirect call per character. On a path that already spends a microsecond per
character talking to a UART, that is not measurable.

### 2.1 The buffer sink, and the truncation rule

```c
static void buf_putc(void *ctx, char c)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    if (s->pos + 1 < s->size)
        s->buf[s->pos] = c;
    s->pos++;
}
```

Note that `pos` keeps incrementing past the end.

`snprintf` must return the length it *would* have produced, not the length it wrote. That is what
lets a caller detect truncation:

```c
    if (snprintf(buf, sizeof(buf), ...) >= (int)sizeof(buf))
        /* it did not fit */
```

and it is what makes the "measure, then allocate" idiom work:

```c
    int n = snprintf(NULL, 0, fmt, ...);
    char *p = kmalloc(n + 1);
    snprintf(p, n + 1, fmt, ...);
```

This is the one place where the standard's design is subtle and correct, and where the obvious
implementation is wrong.

### 2.2 The userland sink batches

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

Every `write()` is a system call — an `int 0x80`, a privilege change, a dispatch, a VFS call. One per
character would make `printf` hundreds of times slower than the formatting it wraps.

256 bytes on the stack, flushed when full and at the end. This is exactly what `stdio`'s buffering
does, and it is why `printf` output can appear out of order with respect to unbuffered writes.

---

## 3. Converting a number

```c
static int utoa(uint64_t value, unsigned base, bool upper, char *buf, int bufsize)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = bufsize;

    if (value == 0) {
        buf[--i] = '0';
        return i;
    }
    while (value && i > 0) {
        buf[--i] = digits[value % base];
        value /= base;
    }
    return i;
}
```

Division produces digits least-significant-first, so we fill the buffer **backwards from the end**
and return the index where the digits start. No reversal step, no second buffer.

`char digits[72]` at the call site: the longest possible output is a 64-bit value in binary, which is
64 digits. Sizing it for the decimal case and then adding `%b` later is a stack overflow waiting for
one input.

The `value == 0` case is separate because the loop produces nothing for zero. Every hand-written
integer-to-string has this bug once.

### 3.1 The negation that overflows

```c
        case 'd':
        case 'i': {
            int64_t v = ...;
            bool neg = v < 0;
            uint64_t mag = neg ? ((uint64_t)(-(v + 1)) + 1) : (uint64_t)v;
```

`-v` is undefined behaviour when `v` is `INT64_MIN`, because the positive counterpart does not exist
in the signed range. On x86 it wraps to itself, so a naive implementation prints `-9223372036854775808`
as `-9223372036854775808`... eventually, after doing signed division on a negative number, which is
also implementation-defined for the sign of the remainder.

`-(v + 1) + 1` computed in unsigned is well defined for every input including the awkward one. It is
the standard idiom and it is worth recognising.

### 3.2 64-bit division on a 32-bit machine

`value % base` and `value /= base` on a `uint64_t` compile to calls to `__udivdi3` and `__umoddi3`,
which live in `libgcc` (Chapter 1, §4).

Nimbus does not link `libgcc`, so this path would fail to link — except that we only ever pass 64-bit
values when `%llu` is used, and nothing in the kernel does. The build succeeds because the compiler
proves the 64-bit path unreachable for the call sites that exist.

That is fragile. If you add a `%llu` call, the link breaks with `undefined reference to __udivdi3`,
and the fix is either `-lgcc` in the link line or avoiding the conversion. Exercise 14.6.

---

## 4. Padding, and the rule everyone gets backwards

```c
static void format_number(fmt_state_t *st, uint64_t value, unsigned base,
                          bool negative, int width, int precision, unsigned flags)
{
    ...
    bool zero_pad = (flags & FLAG_ZERO) && !(flags & FLAG_LEFT) && precision < 0;

    if (!zero_pad && !(flags & FLAG_LEFT)) emit_pad(st, ' ', pad);

    if (sign) emit(st, sign);
    for (int i = 0; i < prefix_len; i++) emit(st, prefix[i]);

    if (zero_pad) emit_pad(st, '0', pad);
    emit_pad(st, '0', zeros);

    for (int i = 0; i < ndigits; i++) emit(st, digits[start + i]);

    if (flags & FLAG_LEFT) emit_pad(st, ' ', pad);
}
```

**Zero padding goes after the sign and the `0x`. Space padding goes before.**

```
    printf("%08x", 255)     ->  000000ff
    printf("%8x",  255)     ->        ff
    printf("%#08x", 255)    ->  0x0000ff        <- 0x first, then zeros
    printf("%+08d", 42)     ->  +0000042        <- sign first, then zeros
    printf("%-8d", 42)      ->  42
```

Getting it backwards produces `00000x ff`, which is the kind of thing you only notice at 3 a.m. after
staring at a column of addresses.

Two more rules encoded in that function:

**An explicit precision cancels zero padding.** `%08.3d` of 5 is `     005` — the precision supplies
the zeros, the width supplies spaces. The standard says so and it is why `precision < 0` is in the
`zero_pad` condition.

**`%.0d` of zero prints nothing at all.** An obscure rule, and the reason `printf("%.*d", 0, 0)` is
the canonical way to print an empty field:

```c
    if (precision == 0 && value == 0) ndigits = 0;
```

---

## 5. The specifier state machine

```c
    while (*fmt) {
        if (*fmt != '%') { emit(&st, *fmt++); continue; }
        fmt++;

        /* flags */
        unsigned flags = 0;
        for (;;) {
            if      (*fmt == '-') flags |= FLAG_LEFT;
            else if (*fmt == '0') flags |= FLAG_ZERO;
            else if (*fmt == '+') flags |= FLAG_PLUS;
            else if (*fmt == ' ') flags |= FLAG_SPACE;
            else if (*fmt == '#') flags |= FLAG_ALT;
            else break;
            fmt++;
        }

        /* width */
        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { flags |= FLAG_LEFT; width = -width; }
            fmt++;
        } else {
            while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        }

        /* precision */
        int precision = -1;
        if (*fmt == '.') { ... }

        /* length modifier */
        int longness = 0;
        while (*fmt == 'l') { longness++; fmt++; }
        if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }
        if (*fmt == 'z') { fmt++; }

        char conv = *fmt++;
        switch (conv) { ... }
    }
```

Flags, then width, then precision, then length, then the conversion character. That order is fixed by
the standard, and the parser reads left to right with no backtracking.

**A negative `*` width means left-align.** `printf("%*d", -8, 42)` is the same as `printf("%-8d", 42)`.
Three lines, and it is in the standard.

**`precision = -1` means "not specified"**, which is different from `precision = 0`. The `%.0d` rule
above depends on the distinction.

### 5.1 Length modifiers on a 32-bit target

```c
        int longness = 0;
        while (*fmt == 'l') { longness++; fmt++; }
```

On 32-bit x86, `int` and `long` are both 32 bits, so `%ld` and `%d` read the same thing. We accept
`l` and ignore it, purely so that code written for a 64-bit host compiles unchanged.

`ll` is real and must not be ignored: a 64-bit argument occupies two stack slots, and reading it as
32 bits leaves the other half in place for the *next* conversion to pick up. Every subsequent
argument comes out wrong, which is a spectacular and very confusing failure.

`h` and `hh` are no-ops because of the default argument promotions (§1.1) — a `short` was already
promoted to `int` before it reached us.

### 5.2 Two conversions worth their existence

```c
        case 'b': {   /* not standard C */
            uint64_t v = ...;
            format_number(&st, v, 2, false, width, precision, flags);
            break;
        }
```

`%b` is not standard C and is invaluable in a kernel. When you are staring at a GDT access byte, an
IDT type field, a page table entry or a PIC mask, binary is the representation that matches the
documentation:

```c
    LOG_INFO("pte = %#034b", *pte);
```

```
pte = 0b0000000000010000000000110000011
```

```c
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            format_number(&st, (uint64_t)v, 16, false, 8, 8, FLAG_ZERO | FLAG_ALT);
            break;
        }
```

`%p` ignores the caller's width and precision and always prints `0x` plus eight digits. A pointer
column that changes width is a pointer column you cannot scan, and the whole reason to print
pointers in a kernel log is to scan them.

---

## 6. Two defensive choices

```c
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
```

A null pointer to `%s` is a bug. Printing `(null)` rather than page-faulting inside the logger has
saved more debugging sessions than it has hidden bugs — because the fault would happen *while
reporting another problem*, replacing a useful message with a useless one.

```c
        default:
            emit(&st, '%');
            emit(&st, conv);
            break;
```

An unknown conversion is echoed verbatim. The alternative — swallowing it — also swallows the
corresponding argument, which desynchronises every conversion after it. Echoing makes the typo
visible in the output:

```
    kprintf("value: %q\n", 42);     ->  value: %q
```

---

## 7. Wiring it up

```c
void kvprintf(const char *fmt, va_list ap)
{
    fmt_vformat(sink_both, NULL, fmt, ap);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
```

Four lines each. Providing both a `...` version and a `va_list` version is the standard pattern and
it matters: a function that wants to wrap `kprintf` cannot forward `...`, only a `va_list`. That is
why the C library has `printf` and `vprintf`, `sprintf` and `vsprintf`, and so on throughout.

---

## 8. Testing it

There is no test framework yet. A function called once from `kmain` does the job:

```c
static void printf_selftest(void)
{
    kprintf("decimal    |%d|%5d|%-5d|%05d|\n",      42, 42, 42, 42);
    kprintf("negative   |%d|%5d|%-5d|%05d|\n",      -42, -42, -42, -42);
    kprintf("extremes   |%d|%u|\n",                 (int)0x80000000, 0xFFFFFFFFu);
    kprintf("hex        |%x|%X|%#x|%08x|\n",        0xdeadbeef, 0xdeadbeef,
                                                    0xdeadbeef, 0xff);
    kprintf("pointer    |%p|%p|\n",                 (void *)0xB8000, NULL);
    kprintf("string     |%s|%10s|%-10s|%.3s|\n",    "abc", "abc", "abc", "abcdef");
    kprintf("null       |%s|\n",                    (char *)NULL);
    kprintf("char       |%c|%3c|\n",                'x', 'x');
    kprintf("binary     |%b|%#010b|\n",             0x8E, 0x8E);
    kprintf("percent    |%%|\n");
    kprintf("unknown    |%q|\n");
    kprintf("star       |%*d|%-*d|\n",              6, 42, 6, 42);
}
```

Expected:

```
decimal    |42|   42|42   |00042|
negative   |-42|  -42|-42  |-0042|
extremes   |-2147483648|4294967295|
hex        |deadbeef|DEADBEEF|0xdeadbeef|000000ff|
pointer    |0x000b8000|0x00000000|
string     |abc|       abc|abc       |abc|
null       |(null)|
char       |x|  x|
binary     |10001110|0b10001110|
percent    |%|
unknown    |%q|
star       |    42|42    |
```

Check `|%05d|` of −42 carefully: `-0042`, with the sign before the zeros. That is the rule from §4
and it is the one most implementations get wrong.

And check `extremes`: `%d` of `0x80000000` must be `-2147483648`, which is the negation case from
§3.1.

---

## 9. What we left out, and what it would cost

**`%f`, `%e`, `%g`.** Floating point formatting. Genuinely hard — correct rounding for
shortest-representation output is a published algorithm (Grisu, then Ryu) and a few hundred lines —
and worse, it needs the FPU, which means saving and restoring FPU state across context switches,
which means `FXSAVE` and a 512-byte per-task area. A kernel that uses floating point at all is a
kernel that has made a significant design decision. Linux's `vsnprintf` has no `%f`.

**Positional arguments** (`%1$s`). Useful for translations, useless here.

**Wide characters** (`%ls`). We have no locale and no wide char type.

**Thread safety.** Our `kprintf` is not reentrant: two tasks calling it simultaneously interleave
characters. On a uniprocessor with `kprintf` called from task context this is rare and harmless; from
an interrupt handler mid-`kprintf` it happens. The fix is a spinlock around the whole format call,
which Chapter 36 could add — at the cost that `panic()` could then deadlock on a lock held by the
code that crashed. Real kernels handle this with a lock that `panic` forcibly breaks.

---

## 10. Exercises

🟢 **14.1** Add `%S` that prints a size in human-readable units: `1536` → `1.5 KiB`. Use it in
`pmm_dump_stats`.

🟢 **14.2** Run the self-test with `%05d` of −42 and check your output against §8. If it prints
`0-042`, find the line in `format_number` that is wrong.

🟢 **14.3** Pass an `int` to `%s` (with the format attribute temporarily removed) and observe the
page fault. Then put the attribute back and see the compile-time warning.

🟡 **14.4** Implement `%n`, which stores the number of characters written so far into an `int *`
argument. Then read about why it was removed from Windows' CRT and is disabled in glibc's hardened
mode.

🟡 **14.5** Add a `hexdump_sink` so that `fmt_vformat` can produce a hex dump of arbitrary memory
with the same padding machinery. Use it to dump the GDT.

🟡 **14.6** Add a `%llu` call somewhere and watch the link fail with `__udivdi3`. Fix it two ways:
once by adding `-lgcc`, once by writing a `udivmod64` helper by hand. Compare the code size.

🔴 **14.7** Make `kprintf` reentrant. Add a spinlock, then work out what `panic()` must do about it —
and implement that too.

---

## What we covered

- Varargs on 32-bit cdecl: arguments are contiguous in memory, so `va_list` is a pointer and
  `va_arg` is a load and an add.
- Default argument promotions, and the stack-layout reason behind them.
- The `format(printf, ...)` attribute, which turns a class of 3 a.m. bugs into compile errors.
- The output-callback design, and the four sinks it gives us for free.
- Why `snprintf` must count past the end of its buffer.
- Backwards digit generation, the `INT64_MIN` negation, and the 64-bit division that needs `libgcc`.
- Zero padding after the sign, space padding before it — and the two obscure standard rules that
  fall out of `precision == -1`.
- The specifier state machine in parse order, and why `ll` cannot be ignored on a 32-bit target.
- `%b` and `%p`, and why both earn their place in a kernel.
- `(null)` and echoing unknown conversions: two defensive choices about failing usefully.

[Chapter 15](15-gdt.md) replaces the bootloader's descriptor table with our own, and adds the two
things it lacks: ring 3 segments, and a TSS.

---

[← The serial port](13-serial-port.md) · [Contents](README.md) · [Next: The GDT →](15-gdt.md)
