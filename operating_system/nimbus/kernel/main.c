/* ===========================================================================
 *  nimbus/kernel/main.c  --  the boot sequence
 * ===========================================================================
 *
 *  Thirty lines of function calls, in an order that cannot be changed.
 *
 *  Almost every line here depends on the one above it, and the dependencies
 *  are not obvious from the names, so each one says what it needs. Read it as
 *  a dependency graph rather than a list: this is the single most useful page
 *  in the kernel for understanding how the pieces fit together, and it is
 *  worth returning to after each chapter.
 *
 *  Explained in: docs/11-multiboot.md (and every chapter after it adds a line)
 * =========================================================================== */

#define NIMBUS_KERNEL 1

#include <nimbus/types.h>
#include <nimbus/multiboot.h>
#include <nimbus/kernel.h>
#include <nimbus/vga.h>
#include <nimbus/serial.h>
#include <nimbus/gdt.h>
#include <nimbus/idt.h>
#include <nimbus/isr.h>
#include <nimbus/irq.h>
#include <nimbus/timer.h>
#include <nimbus/keyboard.h>
#include <nimbus/console.h>
#include <nimbus/pmm.h>
#include <nimbus/paging.h>
#include <nimbus/heap.h>
#include <nimbus/task.h>
#include <nimbus/sched.h>
#include <nimbus/syscall.h>
#include <nimbus/vfs.h>
#include <nimbus/fs.h>
#include <nimbus/ata.h>
#include <nimbus/io.h>
#include <nimbus/string.h>

void printk_enable_console(void);

static void banner(void)
{
    vga_set_color(VGA_BLACK, VGA_LIGHT_CYAN);
    for (int c = 0; c < VGA_WIDTH; c++) vga_put_at(0, c, ' ', vga_get_color());

    const char *title = " NIMBUS ";
    for (int i = 0; title[i]; i++)
        vga_put_at(0, 2 + i, title[i], vga_get_color());

    const char *sub = "an operating system you wrote";
    for (int i = 0; sub[i]; i++)
        vga_put_at(0, 14 + i, sub[i], vga_get_color());

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_move_cursor(2, 0);
}

/* ---------------------------------------------------------------------------
 *  Mount whatever storage we can find
 *
 *  The initrd first, because it is guaranteed to be there and the shell lives
 *  in it. Then any FAT16 partition on the first disk, mounted at /mnt.
 * ------------------------------------------------------------------------- */
static void mount_filesystems(multiboot_info_t *mbi)
{
    vfs_init();

    /* ---- /dev ---------------------------------------------------------------- */
    console_init();

    vfs_node_t *dev = vfs_make_directory("dev");
    vfs_dir_add(vfs_root, dev);
    vfs_dir_add(dev, console_device_node());

    /* ---- the initrd, as / ----------------------------------------------------
     * The bootloader loaded it as a "module". QEMU puts it there when you pass
     * -initrd; GRUB when you write `module /initrd.tar` in its config.
     */
    if ((mbi->flags & MB_INFO_MODS) && mbi->mods_count > 0) {
        multiboot_module_t *mods = (multiboot_module_t *)P2V(mbi->mods_addr);

        vfs_node_t *initrd = initrd_init(mods[0].mod_start,
                                         mods[0].mod_end - mods[0].mod_start);
        if (initrd) {
            vfs_node_t *bin = vfs_make_directory("bin");
            vfs_dir_add(vfs_root, bin);
            bin->mounted = initrd;
            bin->flags  |= VFS_MOUNTPOINT;
        }
    } else {
        LOG_WARN("no initrd module: there will be nothing to run.\n"
                 "       Pass -initrd initrd.tar to QEMU.");
    }

    /* ---- a real disk, if one is attached ------------------------------------- */
    ata_init();

    ata_device_t *disk = ata_get(0);
    if (disk) {
        uint8_t mbr[512];

        if (ata_read_sectors(disk, 0, 1, mbr) == 0 &&
            mbr[510] == 0x55 && mbr[511] == 0xAA) {

            mbr_partition_t *parts = (mbr_partition_t *)(mbr + 446);

            for (int i = 0; i < 4; i++) {
                if (parts[i].type != 0x04 && parts[i].type != 0x06 &&
                    parts[i].type != 0x0E) continue;

                kprintf("mbr: partition %d, type %02x, %u sectors at LBA %u\n",
                        i, parts[i].type, parts[i].sectors, parts[i].lba_first);

                vfs_node_t *fat = fat16_mount(disk, parts[i].lba_first);
                if (fat) {
                    vfs_node_t *mnt = vfs_make_directory("mnt");
                    vfs_dir_add(vfs_root, mnt);
                    vfs_mount("/mnt", fat);
                }
                break;
            }
        } else {
            /*  No partition table. Try the whole disk as one volume, which is
             *  how a floppy is formatted and how `mkfs.fat image.img` leaves
             *  a raw image.                                                   */
            vfs_node_t *fat = fat16_mount(disk, 0);
            if (fat) {
                vfs_node_t *mnt = vfs_make_directory("mnt");
                vfs_dir_add(vfs_root, mnt);
                vfs_mount("/mnt", fat);
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 *  kmain -- called from boot.asm with the multiboot magic and info pointer
 * ------------------------------------------------------------------------- */
void kmain(uint32_t magic, uint32_t mbi_physical)
{
    /* ---- 1. The log, before anything else -----------------------------------
     * Nothing depends on this and everything benefits from it. If the kernel
     * dies on line 3, the serial log is the only evidence that will exist.
     */
    serial_init(COM1);
    serial_puts("\n\n=== Nimbus starting ===\n");

    /* ---- 2. The screen -------------------------------------------------------
     * Depends on: paging, which boot.asm already enabled -- vga.c reaches the
     * framebuffer through the higher-half window, and would page-fault if it
     * ran before that window existed.
     */
    vga_init();
    printk_enable_console();
    banner();

    /* ---- 3. Check who booted us ---------------------------------------------
     * If EAX is wrong, we were not loaded by a multiboot loader, which means
     * EBX is not a memory map and reading it would produce nonsense that looks
     * almost plausible. Fail here, where the message is clear.
     */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("bad multiboot magic %08x (expected %08x).\n"
              "Boot with `qemu-system-i386 -kernel nimbus.elf`, or from GRUB.",
              magic, MULTIBOOT_BOOTLOADER_MAGIC);

    /*  The pointer is physical; every other line in the kernel wants virtual. */
    multiboot_info_t *mbi = (multiboot_info_t *)P2V(mbi_physical);

    /* ---- 4. Descriptor tables ------------------------------------------------
     * The GDT first, because the IDT's gates name a code segment *in* the GDT.
     * Installing an IDT whose gates point into a table that does not exist yet
     * is a triple fault on the first interrupt.
     */
    gdt_init();
    isr_init();
    idt_init();

    /* ---- 5. Interrupt controller ---------------------------------------------
     * Depends on: the IDT. irq_init remaps the PIC to vectors 32-47 and leaves
     * every line masked, so nothing can fire until a driver claims it.
     *
     * Interrupts stay *off* until step 12. Everything between here and there
     * runs single-threaded with no possibility of preemption, which is why
     * none of it needs a lock.
     */
    irq_init();

    /* ---- 6. Physical memory --------------------------------------------------
     * Depends on: the multiboot memory map. Nothing may allocate before this.
     */
    pmm_init(mbi);

    /* ---- 7. Virtual memory ---------------------------------------------------
     * Depends on: the PMM, for the frames that hold the page tables.
     * Replaces boot.asm's throwaway 4 MiB-page directory with a real one.
     */
    paging_init();

    /* ---- 8. The heap ---------------------------------------------------------
     * Depends on: paging, to map its arena. From this line onwards kmalloc
     * works, and most of the rest of the kernel needs it.
     */
    heap_init();

    /* ---- 9. Time -------------------------------------------------------------
     * Depends on: the IDT and the PIC. Claims IRQ 0 and unmasks it -- but with
     * IF still clear, no tick is delivered until step 12.
     */
    timer_init(TIMER_HZ);

    /* ---- 10. Input -----------------------------------------------------------
     * Depends on: the IDT, the PIC, and the heap (the console allocates its
     * /dev/console node).
     */
    keyboard_init();

    /* ---- 11. Filesystems -----------------------------------------------------
     * Depends on: the heap, for every node it creates, and on the ATA driver
     * for anything on a real disk.
     */
    mount_filesystems(mbi);

    /* ---- 12. Tasks -----------------------------------------------------------
     * task_init adopts the context we are running in as pid 0. After
     * sched_init, the timer interrupt can preempt us.
     */
    task_init();
    syscall_init();
    sched_init();

    pmm_dump_stats();
    heap_dump();

    /* ---- 13. Interrupts on ---------------------------------------------------
     * The moment the machine becomes concurrent. Everything written above this
     * line ran with a guarantee it will never have again.
     */
    sti();

    kprintf("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Nimbus is up. Starting /bin/sh.\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    /* ---- 14. The first user process ------------------------------------------ */
    if (!task_spawn_user("/bin/sh"))
        kprintf("could not start /bin/sh -- dropping to the kernel with no userland.\n");

    /* ---- 15. Become the idle task --------------------------------------------
     *
     * kmain never returns; it turns into pid 0's main loop. `hlt` stops the
     * CPU until the next interrupt, which on an idle machine means the
     * processor draws almost no power -- the difference between a silent
     * laptop and one whose fan is at full speed doing nothing.
     *
     * The `sti` before it is belt and braces: `hlt` with interrupts disabled
     * is a machine that never wakes up, and it is worth being unable to get
     * that wrong.
     */
    for (;;) {
        sti();
        hlt();
    }
}
