/* ===========================================================================
 *  nimbus/link.ld  --  a kernel that is loaded low and runs high
 * ===========================================================================
 *
 *  Two addresses matter for every byte of a kernel image, and for most
 *  programs they are the same number so nobody ever learns the difference:
 *
 *      LMA, the Load Memory Address    where the loader puts the bytes
 *      VMA, the Virtual Memory Address where the code expects to be
 *
 *  Nimbus is loaded at physical 0x00100000 and runs at virtual 0xC0100000, so
 *  for us they differ by exactly 0xC0000000. `AT(...)` is how a linker script
 *  says so: the section is *addressed* here and *stored* there.
 *
 *  The exception is boot.asm's two sections, which run before paging exists
 *  and so must be addressed where they are loaded. They are placed first, low,
 *  and with no AT() at all.
 *
 *  Explained in: docs/25-higher-half.md
 * =========================================================================== */

ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xC0000000;
KERNEL_LOAD_ADDR    = 0x00100000;

SECTIONS
{
    /* ---- The low part: runs with paging off --------------------------------
     *
     * 1 MiB, because everything below it is claimed by the BIOS, by video
     * memory, or by option ROMs. It is the first address on a PC where a
     * kernel can live without negotiating with anything.
     */
    . = KERNEL_LOAD_ADDR;
    __kernel_phys_start = .;

    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }

    /* ---- Everything else: addressed high, stored low ----------------------
     *
     * The location counter jumps up by 3 GiB. Note the `. += ...` rather than
     * `. = 0xC0100000`: it preserves however far the low sections got, so
     * adding code to boot.asm cannot silently create an overlap.
     */
    . += KERNEL_VIRTUAL_BASE;
    __kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE)
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    __kernel_end = .;
    __kernel_phys_end = . - KERNEL_VIRTUAL_BASE;

    /* pmm_init() reserves everything between __kernel_phys_start and
     * __kernel_phys_end. If those two symbols are wrong, the frame allocator
     * will hand out frames containing the running kernel, and the failure will
     * look like random corruption minutes later. They are worth checking with
     * `i686-elf-readelf -s nimbus.elf | grep kernel_phys` the first time. */

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
