/* ===========================================================================
 *  spark/link.ld  --  where every byte of the kernel goes
 * ===========================================================================
 *
 *  A linker script answers one question: given a pile of .o files full of
 *  named sections, what address does each byte end up at?
 *
 *  On a hosted system you never write one, because the toolchain ships a
 *  default that matches what the OS loader expects. We *are* the loader, and
 *  what we expect is: the kernel starts at 0x100000, and the first instruction
 *  in the file is `_start`, because stage2.asm does a bare `jmp 0x100000` with
 *  no ELF parsing whatsoever.
 *
 *  Explained in: docs/09-freestanding-c.md
 *  Line by line: docs/line-by-line/spark-link.md
 * =========================================================================== */

/* Which symbol is the entry point. For the raw binary this is documentation
 * only -- nothing reads it -- but GDB uses it, and `ld` warns if it is
 * missing, which catches a misspelled _start before it costs you an hour. */
ENTRY(_start)

SECTIONS
{
    /* The location counter. Every byte placed from here on is assigned an
     * address starting at 1 MiB. See stage2.asm: KERNEL_PHYS. */
    . = 0x00100000;

    /* ---- .text: executable code -----------------------------------------
     *
     * `*(.text.entry)` comes first and alone, which is the whole reason that
     * section exists. Without it, the linker is free to order the input files
     * however it likes, and `jmp 0x100000` would land in the middle of
     * vga_scroll(). With it, the first byte of the image is provably the first
     * byte of _start.
     *
     * The `*` means "from any input file". You could write
     * `kernel/entry.o(.text.entry)` to be explicit; the wildcard is safer
     * against a renamed build directory.
     */
    .text ALIGN(4K) :
    {
        __text_start = .;
        *(.text.entry)
        *(.text .text.*)
        __text_end = .;
    }

    /* ---- .rodata: constants ----------------------------------------------
     * String literals live here. On a real system this section is mapped
     * read-only and executing it faults; we have no paging yet (Chapter 24),
     * so the separation is currently only a convention. Keeping it anyway
     * means Chapter 25 can mark it read-only by changing one line.
     */
    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    /* ---- .data: initialised globals -------------------------------------- */
    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    /* ---- .bss: zero-initialised globals ----------------------------------
     *
     * These occupy address space but no file space -- the type is NOBITS. The
     * two symbols are what entry.asm uses to zero the region; they are defined
     * by the linker, and in C you would declare them as `extern char
     * __bss_start[];` (an array, not a pointer -- the *address* of the symbol
     * is the value you want, and a pointer declaration would make the compiler
     * load from it instead).
     *
     * Our 16 KiB kernel stack lives in here, which is why .bss is much larger
     * than the handful of globals in kernel.c would suggest.
     */
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    /* The first address past the kernel. Chapter 22's physical memory manager
     * uses exactly this symbol to know which frames are already spoken for. */
    __kernel_end = .;

    /* ---- Throw away what we cannot use -----------------------------------
     * GCC emits .comment (a version string), .eh_frame (unwind tables for C++
     * exceptions and backtraces) and .note.* sections. Nothing here consumes
     * them. Discarding them explicitly keeps the binary honest -- and stops
     * .eh_frame from landing between .text and .rodata, which would put 200
     * bytes of unwind data at 0x100000 if the entry section trick ever broke.
     */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
