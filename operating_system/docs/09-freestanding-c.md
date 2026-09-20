# Chapter 9 — Freestanding C and the linker script

[← Stage 2](08-stage2-loader.md) · [Contents](README.md) · [Next: Hello, VGA →](10-vga-hello.md)

> 📖 **Line by line:** [entry.asm](line-by-line/spark-kernel.md) · [link.ld](line-by-line/spark-link.md)

---

## Goal

Get from a `jmp 0x100000` to a running C function. Three things have to be true before C can execute,
none of them are true when we arrive, and establishing them is what
[`entry.asm`](../spark/kernel/entry.asm) is for. Then we have to make sure the linker puts that file
first, which is what [`link.ld`](../spark/link.ld) is for.

This chapter is about the seam between assembly and C, and about the one build tool most programmers
never configure.

---

## 1. What C needs before it can run

C looks like a portable language and it is, but only relative to an environment that provides three
things. On a hosted system the loader and the C runtime provide them and you never notice.

### 1.1 A stack

The first thing any non-trivial C function does is push something. Local variables, saved registers,
return addresses, function arguments — all of it lives on the stack, addressed through `ESP`.

Stage 2 set `ESP = 0x90000` before jumping to us, so technically there *is* a stack. We replace it
anyway, for two reasons: the kernel should own its own stack at an address the linker knows about,
and `0x90000` is inside the region a physical memory manager will one day want to hand out.

### 1.2 A zeroed `.bss`

The C standard promises that a global without an initialiser starts at zero:

```c
static int counter;              /* guaranteed 0 */
static char buffer[4096];        /* guaranteed all zeros */
```

The compiler relies on that promise. It does not emit code to zero `counter`; it places it in `.bss`
and assumes someone else did.

On a hosted system, the ELF loader does — it maps `.bss` from `/dev/zero`. We are the loader. If we
skip it, globals contain whatever was in RAM at power-on, which on real hardware is genuinely random
and on QEMU is usually zero — meaning the bug works fine in the emulator and fails on metal.

### 1.3 A terminated frame-pointer chain

Less critical, and free. `EBP` normally points at the previous stack frame, forming a chain a
debugger can walk. At the top of the chain something has to be zero, or the walker keeps following
whatever garbage `EBP` held and prints invented stack frames.

`xor ebp, ebp` before the first `call`. One instruction, and Chapter 47's backtrace depends on it.

---

## 2. `entry.asm`

```nasm
[BITS 32]

global _start
extern kmain
extern __bss_start
extern __bss_end

section .text.entry

_start:
    cli

    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    mov esp, stack_top
    xor ebp, ebp

    call kmain

.halt:
    cli
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
```

Twelve instructions. Each block is one of the three requirements plus the aftermath.

### 2.1 `section .text.entry`

A section name that exists for exactly one purpose: so the linker script can name it and place it
first. Chapter 8, §4.2 — stage 2 does `jmp 0x100000` with no ELF parsing, so the first byte of the
image must be `_start`, and a normal `.text` gives no guarantee about ordering.

### 2.2 Zeroing `.bss`

```nasm
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb
```

`__bss_start` and `__bss_end` are defined by the linker script, not by any source file. In assembly
they are used directly as addresses; in C you would declare them as arrays, never as pointers:

```c
extern char __bss_start[];       /* right: the ADDRESS of the symbol */
extern char *__bss_start;        /* wrong: loads FROM that address */
```

That distinction catches people constantly. A linker-defined symbol has no storage — the linker
assigns it an address and nothing else. `extern char *x` tells the compiler there is a pointer
*stored* at that address, so it emits a load, and you get whatever bytes happen to be at the start of
`.bss` interpreted as a pointer.

`rep stosb` needs no stack, which is fortunate, because the stack is inside the region being cleared.
It touches only `EDI`, `ECX` and `EAX`.

### 2.3 The stack

```nasm
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
```

16 KiB in `.bss`, so it costs nothing in the binary — `resb` reserves space without storing bytes
(Chapter 3, §2.5).

`stack_top` is the label *after* the reservation, because the stack grows downwards and `ESP` starts
at the high end. Setting `ESP = stack_bottom` is a mistake that works for a surprising number of
function calls before it destroys something below it.

`align 16` because the System V ABI wants 16-byte stack alignment at call boundaries. GCC can emit
SSE instructions for struct copies even when you have asked it not to use SSE for arithmetic, and
`movaps` on a misaligned address faults.

**There is no guard page.** Overflow this stack and it grows silently into whatever `.bss` variable
the linker placed just below `stack_bottom`. Chapter 26 adds a guard page to Nimbus's task stacks;
Spark does not have one, and 16 KiB is enough that it does not matter for a kernel with no recursion.

### 2.4 The halt loop

```nasm
.halt:
    cli
    hlt
    jmp .halt
```

`kmain` is not supposed to return. If it does, this catches it rather than executing whatever bytes
follow.

Chapter 5, §7 covered why `hlt` rather than `jmp $`: low power instead of a spinning core.

---

## 3. What a linker script does

A linker takes object files full of *named sections* and produces an output file in which every byte
has an address. The script answers: which sections, in what order, at what addresses.

On a hosted system you never write one, because the toolchain ships a default that matches what the
OS loader expects. We are the loader, and what we expect is: **start at `0x100000`, and `_start`
first.**

### 3.1 The script

```ld
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .text ALIGN(4K) :
    {
        __text_start = .;
        *(.text.entry)
        *(.text .text.*)
        __text_end = .;
    }

    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
```

### 3.2 `ENTRY(_start)`

Records the entry point in the ELF header. For the flat binary this is documentation only — nothing
reads it, because stage 2 jumps to a hardcoded address. It matters for two other things: GDB uses it,
and `ld` warns if the symbol is missing, which catches a misspelled `_start` before it costs an hour.

### 3.3 The location counter

```ld
    . = 0x00100000;
```

`.` is the location counter — the address that the next byte will be assigned. Setting it here says
"everything from now on is addressed starting at 1 MiB".

It can be read as well as written, which is what makes `__text_start = .;` work: it defines a symbol
whose value is whatever the counter currently holds.

### 3.4 Output sections and input sections

```ld
    .text ALIGN(4K) :
    {
        *(.text.entry)
        *(.text .text.*)
    }
```

The name before the brace is the *output* section. Inside, `*(...)` means "the named input sections,
from any input file". `*` is a filename wildcard — you could write `kernel/entry.o(.text.entry)` to
be explicit, and the wildcard is safer against a renamed build directory.

`ALIGN(4K)` rounds the location counter up to a 4096-byte boundary before placing the section. That
does nothing useful yet and everything useful in Chapter 25: page permissions are per-page, so a
section that shares a page with another section cannot have its own permissions.

### 3.5 The line that matters most

```ld
        *(.text.entry)
        *(.text .text.*)
```

`.text.entry` alone, first. Without it the linker is free to order input files however it likes, and
the first byte of the image could be the middle of `vga_scroll`.

There is no error if this goes wrong. The build succeeds, the image is well-formed, and the machine
jumps into a function prologue somewhere. You get a triple fault with no obvious cause.

Two other ways to achieve the same thing:

1. **List the object file first on the link command line.** Works with GNU `ld`'s default ordering,
   is undocumented behaviour, and breaks if anyone reorders the Makefile.
2. **Name the file explicitly in the script:** `bin/spark/entry.o(.text)`. Explicit, and brittle
   against build layout changes.

The `.text.entry` convention is what Linux uses (`.head.text`), and it is the one that survives
someone reorganising the build.

### 3.6 `*(COMMON)`

An archaeological curiosity that will bite you once. A *tentative definition* — `int counter;` at
file scope with no initialiser and no `static` — historically went into a special `COMMON` section
that the linker merged across translation units, so the same definition in two files was one
variable rather than a duplicate-symbol error.

GCC 10 changed the default to `-fno-common`, making these errors as they always should have been. But
old code, and assembly, can still produce `COMMON` symbols, and a script that does not place them
gets a link error naming a symbol you did not know existed. One line prevents it.

### 3.7 `/DISCARD/`

```ld
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
```

Sections we cannot use:

- **`.comment`** — a version string GCC embeds.
- **`.eh_frame`** — DWARF unwind tables, for C++ exceptions and backtraces. Nothing here consumes
  them.
- **`.note.*`** — build IDs, ABI tags, GNU property notes.

Discarding them keeps the binary honest, and — the real reason — stops `.eh_frame` landing *between*
`.text` and `.rodata`. If the `.text.entry` trick ever broke, having 200 bytes of unwind data at
`0x100000` would make the failure much more confusing.

---

## 4. Two outputs: ELF and flat

```make
bin/spark/kernel.elf: bin/spark/entry.o bin/spark/kernel.o spark/link.ld
	$(LD) -m elf_i386 -T spark/link.ld -o $@ bin/spark/entry.o bin/spark/kernel.o

$(SPARK_KERNEL): bin/spark/kernel.elf
	$(OBJCOPY) -O binary $< $@
```

Link to ELF, then flatten with `objcopy`.

**Why both.** The ELF file keeps the symbol table and the DWARF debug info, which is what lets you
run:

```bash
i686-elf-gdb bin/spark/kernel.elf
(gdb) target remote :1234
(gdb) break kmain
```

and see source. The flat binary is what stage 2 can actually load.

**What `objcopy -O binary` does:** takes every section that has the `ALLOC` flag and contents, writes
them at their addresses relative to the lowest one, and drops everything else. `.bss` is `NOBITS` —
it has an address but no contents — so it contributes nothing to the file. That is why the flat
binary is small even though the stack is 16 KiB.

**What it does not do:** it does not tell you if the result is too big. Hence the Makefile's size
check (Chapter 8, §4.3).

### Looking at the result

```bash
$ i686-elf-readelf -S bin/spark/kernel.elf
Section Headers:
  [Nr] Name      Type      Addr     Off    Size   Flg
  [ 1] .text     PROGBITS  00100000 001000 000420  AX
  [ 2] .rodata   PROGBITS  00101000 002000 000180  A
  [ 3] .data     PROGBITS  00102000 003000 000004  WA
  [ 4] .bss      NOBITS    00103000 004000 004010  WA
```

`.text` at `0x100000` — exactly where stage 2 jumps. `.bss` is `NOBITS` with a size of `0x4010`
(16 KiB of stack plus a few globals) and contributes nothing to the file.

```bash
$ i686-elf-readelf -s bin/spark/kernel.elf | grep -E "_start|bss"
     5: 00100000     0 NOTYPE  GLOBAL DEFAULT    1 _start
    12: 00103000     0 NOTYPE  GLOBAL DEFAULT    4 __bss_start
    13: 00107010     0 NOTYPE  GLOBAL DEFAULT    4 __bss_end
```

`_start` at `0x100000`. That is the check worth doing the first time and after any change to the
script — if it is not `0x100000`, nothing else will work and the symptom will be uninformative.

---

## 5. What "freestanding" really means

Chapter 1 covered the flags. Here is what they add up to, and the one thing that surprises everyone.

`-ffreestanding` tells GCC that no hosted C environment exists. Concretely:

- `main` is not special. It has no implicit `return 0`, and it is not the entry point.
- The standard library is not assumed to exist — `<stdio.h>` and friends are not available.
- Only the *freestanding* headers are guaranteed: `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`,
  `<limits.h>`, `<stdarg.h>`, `<float.h>`, `<iso646.h>`.

**What it does not do:** it does not stop GCC emitting calls to `memcpy`, `memmove`, `memset` and
`memcmp`. The C standard explicitly requires a freestanding implementation to provide those four, so
GCC assumes they exist.

This is why [`kernel.c`](../spark/kernel/kernel.c) has no struct assignments and no array
initialisations of any size — with only 600 lines, avoiding them is easier than providing the four
functions. Nimbus does provide them, in [`lib/string.c`](../nimbus/lib/string.c), and Chapter 12 is
where that first becomes necessary.

`-fno-builtin` is the related flag and it is subtler. Without it, GCC recognises a function *named*
`memcpy` and may replace its body with a call to `memcpy` — which is to say, to itself. Infinite
recursion, discovered at runtime, as a stack overflow.

---

## 6. Writing C with nothing underneath

A few habits that Spark's kernel demonstrates and that every kernel needs.

### 6.1 Types, spelled out

```c
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
```

`<stdint.h>` is freestanding and we could include it. Writing them out once is worth doing, because
every one of these is a promise about the machine, and on 32-bit x86 the promises are: `int` is 32
bits, `long` is **32** bits (not 64), and a pointer is 32 bits.

That `long` is a real trap for anyone coming from 64-bit Linux, where `long` is 64 bits. Code that
uses `long` for a pointer-sized integer works there and breaks here.

### 6.2 `volatile` for hardware

```c
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)
```

Without `volatile`, the compiler is entitled to notice that nothing reads the VGA buffer and delete
the writes, or to hoist a store out of a loop, or to combine several. All of those are wrong for
memory-mapped I/O. Chapter 12 has the full treatment.

### 6.3 Signedness, once

```c
static inline uint16_t vga_cell(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}
```

`char` is signed on x86 GCC. A byte like `0xDB` is the negative number −37, and sign-extending it
into 16 bits gives `0xFFDB` — which would overwrite the colour nibbles and print a blinking white
block.

`(uint8_t)` first, then `(uint16_t)`. It is a two-cast idiom that appears whenever a `char` becomes a
wider integer, and forgetting it produces bugs that only show up for characters above 127.

### 6.4 No `assert`, no `errno`, no allocation

Spark has none of these. Nimbus builds all three, and it is worth noticing in which order: `panic`
and `ASSERT` come in Chapter 14, before the memory manager, because a kernel that cannot say what
went wrong is a kernel you cannot debug.

---

## 7. Running it

At this point `kmain` exists but does nothing visible — Chapter 10 writes the VGA code. To prove the
handover works, a two-line `kmain` is enough:

```c
void kmain(void)
{
    *(volatile uint16_t *)0xB8000 = 0x0F41;   /* white 'A' on black */
    for (;;) __asm__ volatile ("hlt");
}
```

```bat
build spark
run spark
```

An `A` in the top-left corner, over whatever the BIOS left on screen.

That single character proves: the kernel was loaded, copied to 1 MiB, jumped to at the right byte,
`.bss` was cleared without crashing, `ESP` was valid enough for `call kmain` to work, and C code is
executing in 32-bit protected mode.

### If the `A` does not appear

| Symptom | Check |
|---|---|
| Triple fault immediately | `_start` is not at `0x100000` — run `readelf -s` and look |
| Triple fault after a few instructions | `.bss` zeroing ran off the end; check `__bss_end` |
| Nothing, no fault | The kernel was not copied; check `rep movsd` in stage 2 |
| Character in the wrong place | The BIOS left the cursor elsewhere — expected, we write absolutely |
| Reboots on hardware, fine in QEMU | `.bss` was not zeroed and QEMU's RAM happened to be zero |

---

## 8. Exercises

🟢 **9.1** Remove `*(.text.entry)` from the linker script, rebuild, and run `readelf -s` to see what
ended up at `0x100000`. Then boot it.

🟢 **9.2** Change `mov esp, stack_top` to `mov esp, stack_bottom`. Does it boot? How many function
calls does it survive, and what does it corrupt?

🟡 **9.3** Delete the `.bss` zeroing and add `static int counter;` with `counter++` in `kmain`.
Print it. Run in QEMU (where memory starts zeroed) and then with `-m 32M` after running something
else — can you get a non-zero starting value?

🟡 **9.4** Add a `.rodata` section test: put a `const char msg[] = "hi";` in `kernel.c` and use
`readelf -S` to confirm it landed in `.rodata` and not `.data`. Then make it non-`const` and watch it
move.

🟡 **9.5** Write the C declaration for `__bss_start` two ways — as `extern char x[]` and as
`extern char *x` — and print both values. Explain the difference in the output.

🔴 **9.6** Add a `.init_array` section to the linker script and write the loop in `entry.asm` that
calls every function pointer in it. This is how C++ static constructors and GCC's
`__attribute__((constructor))` work, and it is about fifteen lines.

---

## What we covered

- The three preconditions for C — a stack, a zeroed `.bss`, a terminated frame chain — and why
  nobody establishes them for us.
- `entry.asm`: `rep stosb` needing no stack, `stack_top` versus `stack_bottom`, and 16-byte
  alignment.
- Linker-defined symbols, and why `extern char x[]` and `extern char *x` are not the same thing.
- What a linker script is: sections in, addresses out. The location counter, output versus input
  sections, `ALIGN(4K)`, `COMMON` and `/DISCARD/`.
- The `.text.entry` trick, the two alternatives, and why this one survives a build reorganisation.
- ELF for debugging, flat binary for loading, and what `objcopy -O binary` keeps.
- What `-ffreestanding` does and — more importantly — the four functions it does *not* free you from.
- Kernel C habits: explicit widths, `volatile`, and the two-cast idiom for `char`.

[Chapter 10](10-vga-hello.md) finishes Spark: the VGA text buffer, attribute bytes, scrolling, and
the first thing you wrote appearing on a screen you drove yourself.

---

[← Stage 2](08-stage2-loader.md) · [Contents](README.md) · [Next: Hello, VGA →](10-vga-hello.md)
