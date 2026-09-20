/* ===========================================================================
 *  nimbus/include/nimbus/multiboot.h  --  what the bootloader tells us
 * ===========================================================================
 *
 *  Multiboot 1 is a contract between a bootloader and a kernel, published in
 *  1995 so that you would not have to write a bootloader for every kernel and
 *  a kernel for every bootloader. GRUB implements it. QEMU implements enough
 *  of it that `qemu-system-i386 -kernel nimbus.elf` works with no bootloader
 *  at all, which is why Nimbus uses it: Spark already taught you how to write
 *  the bootloader, and repeating that work here would buy nothing.
 *
 *  Our side of the contract is eight bytes of header in the first 8 KiB of the
 *  kernel image. Their side is: the CPU is in 32-bit protected mode, A20 is
 *  on, paging is off, interrupts are off, EAX holds the magic number 0x2BADB002
 *  and EBX holds the physical address of the structure below.
 *
 *  Explained in: docs/11-multiboot.md
 * =========================================================================== */
#ifndef NIMBUS_MULTIBOOT_H
#define NIMBUS_MULTIBOOT_H

#include <nimbus/types.h>

#define MULTIBOOT_HEADER_MAGIC   0x1BADB002u   /* what we put in the image  */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u /* what the loader puts in EAX */

/*  Header flags we request.                                                   */
#define MULTIBOOT_PAGE_ALIGN     0x00000001u   /* align loaded modules to 4 KiB */
#define MULTIBOOT_MEMORY_INFO    0x00000002u   /* please give us the memory map */

/*  Which fields of multiboot_info_t are actually valid -- the loader sets one
 *  flag bit per field, and reading a field whose bit is clear gives you
 *  whatever was in that memory. This is the number one source of "it works in
 *  QEMU and hangs under GRUB".                                                */
#define MB_INFO_MEMORY           0x00000001u   /* mem_lower / mem_upper      */
#define MB_INFO_BOOTDEV          0x00000002u
#define MB_INFO_CMDLINE          0x00000004u
#define MB_INFO_MODS             0x00000008u   /* mods_count / mods_addr     */
#define MB_INFO_AOUT_SYMS        0x00000010u
#define MB_INFO_ELF_SHDR         0x00000020u
#define MB_INFO_MEM_MAP          0x00000040u   /* mmap_length / mmap_addr    */
#define MB_INFO_DRIVE_INFO       0x00000080u
#define MB_INFO_CONFIG_TABLE     0x00000100u
#define MB_INFO_BOOT_LOADER_NAME 0x00000200u
#define MB_INFO_APM_TABLE        0x00000400u
#define MB_INFO_VBE_INFO         0x00000800u
#define MB_INFO_FRAMEBUFFER_INFO 0x00001000u

typedef struct multiboot_info {
    uint32_t flags;

    uint32_t mem_lower;      /* KiB of conventional memory below 1 MiB (~639) */
    uint32_t mem_upper;      /* KiB of memory above 1 MiB -- NOT total RAM    */

    uint32_t boot_device;
    uint32_t cmdline;        /* physical address of a NUL-terminated string   */

    uint32_t mods_count;     /* loaded modules: our initrd arrives this way   */
    uint32_t mods_addr;      /* physical address of an array of multiboot_module */

    uint32_t syms[4];        /* a.out symbol table or ELF section headers     */

    uint32_t mmap_length;    /* bytes in the memory map                       */
    uint32_t mmap_addr;      /* physical address of the first mmap entry      */

    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_color_info[6];
} PACKED multiboot_info_t;

/*  One entry of the memory map. The layout is odd: `size` is the number of
 *  bytes in the entry *not counting the size field itself*, so you advance to
 *  the next entry with `p = (mmap_entry*)((uint8_t*)p + p->size + 4)` and not
 *  with `p++`. Entries are not required to be the same length and on some
 *  firmware they are not.                                                     */
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} PACKED multiboot_mmap_entry_t;

#define MULTIBOOT_MEMORY_AVAILABLE        1   /* usable RAM                   */
#define MULTIBOOT_MEMORY_RESERVED         2   /* do not touch                 */
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3   /* usable once ACPI tables read */
#define MULTIBOOT_MEMORY_NVS              4   /* ACPI non-volatile storage    */
#define MULTIBOOT_MEMORY_BADRAM           5   /* firmware says it is broken   */

typedef struct multiboot_module {
    uint32_t mod_start;      /* physical address of the first byte            */
    uint32_t mod_end;        /* physical address one past the last byte       */
    uint32_t cmdline;        /* physical address of the module's string       */
    uint32_t pad;
} PACKED multiboot_module_t;

#endif /* NIMBUS_MULTIBOOT_H */
