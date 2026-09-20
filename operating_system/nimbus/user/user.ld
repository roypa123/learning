/* ===========================================================================
 *  nimbus/user/user.ld  --  where a user program lives
 * ===========================================================================
 *
 *  Much simpler than the kernel's script, because a user program has no load
 *  address / run address split: the kernel maps it exactly where it is linked.
 *
 *  0x08048000 is the traditional base for a 32-bit ELF executable on Linux,
 *  and we copy it for the reason it was chosen: it leaves the bottom 128 MiB
 *  of the address space unmapped, so a null pointer -- and anything within
 *  128 MiB of one, which covers `p->field` and `array[i]` for any sane struct
 *  or index -- faults instead of silently reading real memory.
 *
 *  Explained in: docs/45-user-libc.md
 * =========================================================================== */

ENTRY(_start)

SECTIONS
{
    . = 0x08048000;

    /* crt0's code must come first so that the ELF entry point is _start. In
     * fact the entry point is taken from the header, not from the layout, so
     * this is belt and braces -- but having _start at the lowest address makes
     * a disassembly read in the order the program runs. */
    .text ALIGN(4K) : {
        *(.text.start)
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* The kernel's ELF loader sets the initial program break to the page after
     * the last segment, so this symbol is where the heap begins. It is not
     * read by anything -- sbrk gets the value from the kernel -- but it is
     * useful in a disassembly and in GDB. */
    _end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
