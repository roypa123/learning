# Line by line: `spark/link.ld`

[Index](README.md) · [Chapter 9](../09-freestanding-c.md)

A linker script answers one question: given a pile of `.o` files full of named sections, what address
does each byte end up at?

---

```ld
ENTRY(_start)
```
Records the entry point in the ELF header.

For the flat binary this is documentation only — stage 2 jumps to a hardcoded `0x100000`. It matters
for two other things: GDB uses it, and `ld` warns if the symbol is missing, which catches a
misspelled `_start` before it costs an hour.

---

```ld
SECTIONS
{
    . = 0x00100000;
```
`.` is the **location counter** — the address the next byte will be assigned.

1 MiB because everything below it is a minefield: the IVT, the BIOS data area, video memory at
`0xA0000`, option ROMs, and the BIOS at `0xF0000`. Above it there is nothing but RAM.

Must match `KERNEL_PHYS` in `stage2.asm`. ⚠️ Nothing checks that; a mismatch is a triple fault at the
`jmp`.

---

```ld
    .text ALIGN(4K) :
    {
        __text_start = .;
```
The name before the brace is the **output** section. `__text_start = .;` defines a symbol whose value
is the counter's current value — the counter is readable as well as writable.

`ALIGN(4K)` rounds up to a page boundary. Does nothing useful in Spark and everything in Chapter 25:
page permissions are per page, so a section sharing a page with another cannot have its own.

```ld
        *(.text.entry)
```
⚠️ **The most important line in the file.**

`*(...)` means "these input sections, from any input file". The `*` is a filename wildcard.

`.text.entry` alone, first, so the first byte of the image is provably `_start`. Without it the
linker may order the files however it likes and `jmp 0x100000` lands in the middle of `vga_scroll`.

There is no error if this goes wrong — the build succeeds and the machine triple-faults.

Two alternatives: list `entry.o` first on the command line (undocumented ordering behaviour, breaks
if the Makefile is reordered), or name the file explicitly in the script (brittle against build
layout changes). The section convention is what Linux uses, as `.head.text`.

```ld
        *(.text .text.*)
        __text_end = .;
    }
```
`.text.*` catches `-ffunction-sections` output, where every function gets its own section.

---

```ld
    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }
```
String literals live here.

Separating it from `.data` is currently only a convention — Spark has no paging — but keeping it
means the Chapter 25 version can mark it read-only by changing one line.

---

```ld
    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }
```
Initialised globals. Their values are stored in the file.

---

```ld
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }
```
Zero-initialised globals. Type `NOBITS`: address space but no file space.

`__bss_start` and `__bss_end` are what `entry.asm` uses to zero the region.

⚠️ In C these are declared as **arrays**, never pointers:

```c
extern char __bss_start[];       /* right: the ADDRESS of the symbol */
extern char *__bss_start;        /* wrong: emits a LOAD from that address */
```

A linker-defined symbol has no storage. `extern char *x` tells the compiler a pointer is *stored*
there, so it emits a load and you get the first four bytes of `.bss` interpreted as an address.

```ld
        *(COMMON)
```
An archaeological curiosity that will bite once. A *tentative definition* — `int counter;` at file
scope, no initialiser, no `static` — historically went into a `COMMON` section that the linker merged
across translation units.

GCC 10 changed the default to `-fno-common`, making these duplicate-symbol errors as they always
should have been. Old code and assembly can still produce them, and a script that does not place them
gets a link error naming a symbol you did not know existed.

The 16 KiB stack is in here, which is why `.bss` is much larger than the handful of globals in
`kernel.c` would suggest.

---

```ld
    __kernel_end = .;
```
The first address past the kernel. Nimbus's physical memory manager uses exactly this symbol to know
which frames are already spoken for.

---

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
- **`.eh_frame`** — DWARF unwind tables for C++ exceptions and backtraces. Nothing here consumes
  them.
- **`.note.*`** — build IDs, ABI tags, GNU property notes.

Discarding keeps the binary honest, and — the real reason — stops `.eh_frame` landing *between*
`.text` and `.rodata`. If the `.text.entry` trick ever broke, 200 bytes of unwind data at `0x100000`
would make the failure much more confusing.

---

## Checking it

```bash
$ i686-elf-readelf -S bin/spark/kernel.elf
  [Nr] Name      Type      Addr     Off    Size   Flg
  [ 1] .text     PROGBITS  00100000 001000 000420  AX
  [ 2] .rodata   PROGBITS  00101000 002000 000180  A
  [ 3] .data     PROGBITS  00102000 003000 000004  WA
  [ 4] .bss      NOBITS    00103000 004000 004010  WA
```

`.text` at `0x100000`. `.bss` is `NOBITS` with a size of `0x4010` — 16 KiB of stack plus a few
globals — and contributes nothing to the file.

```bash
$ i686-elf-readelf -s bin/spark/kernel.elf | grep _start
     5: 00100000     0 NOTYPE  GLOBAL DEFAULT    1 _start
```

⚠️ **`_start` at `0x100000`.** The check worth running after any change to this file. If it is not
`0x100000`, nothing else will work and the symptom will be uninformative.

---

## Two outputs

```make
bin/spark/kernel.elf: ... spark/link.ld
	$(LD) -m elf_i386 -T spark/link.ld -o $@ ...

$(SPARK_KERNEL): bin/spark/kernel.elf
	$(OBJCOPY) -O binary $< $@
```

The ELF keeps symbols and DWARF, which is what lets GDB show source. The flat binary is what stage 2
can load.

`objcopy -O binary` takes every section with the `ALLOC` flag and contents, writes them at their
addresses relative to the lowest, and drops the rest. `.bss` is `NOBITS` so it contributes nothing —
which is why the flat binary is small despite the 16 KiB stack.

⚠️ It does **not** tell you if the result is too big. Hence the Makefile's size check against
`KERNEL_SECTORS * 512`.

---

[Index](README.md) · [Chapter 9](../09-freestanding-c.md)
