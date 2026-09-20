/* ===========================================================================
 *  nimbus/mm/pmm.c  --  the physical memory manager
 * ===========================================================================
 *
 *  One bit per 4 KiB frame: 1 = in use, 0 = free. That is the entire data
 *  structure, and for 4 GiB of RAM it costs 128 KiB -- three thousandths of
 *  one percent of the memory it tracks.
 *
 *  Allocation is a linear scan for a zero bit. That is O(total memory) in the
 *  worst case and genuinely bad: with 4 GiB of RAM nearly full, one allocation
 *  reads a megabyte of bitmap. The fix is a free list, or a buddy allocator,
 *  or at minimum a "search from where you left off" hint -- we implement the
 *  hint, because it is four lines and removes the pathological case, and
 *  Chapter 22 explains what the other two would buy.
 *
 *  Explained in: docs/22-pmm.md
 *  Line by line: docs/line-by-line/nimbus-pmm.md
 * =========================================================================== */

#include <nimbus/pmm.h>
#include <nimbus/paging.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/io.h>

/*  Enough bits for 4 GiB. A dynamically sized bitmap would be more elegant and
 *  would need somewhere to live -- and the only allocator that could provide
 *  that somewhere is this one. Bootstrapping problems are why the lowest layer
 *  of a kernel is usually the one with a fixed-size array in it.              */
#define MAX_FRAMES      (4u * 1024 * 1024 * 1024 / PAGE_SIZE)   /* 1,048,576   */
#define BITMAP_BYTES    (MAX_FRAMES / 8)                        /* 131,072     */

/*  Must match DIRECT_MAP_SIZE in mm/paging.c. Two constants that have to agree
 *  is a smell; they live apart because pmm.c must not depend on paging.c, and
 *  the assertion that catches a mismatch is in paging_init().                 */
#define DIRECT_MAP_LIMIT (256u * MiB)

static uint8_t  frame_bitmap[BITMAP_BYTES];
static uint32_t total_frames = 0;
static uint32_t used_frames  = 0;
static uint32_t search_hint  = 0;

/*  Provided by link.ld. These are *physical* addresses, which is what the
 *  frame allocator deals in, and they are why the kernel does not hand out
 *  frames containing itself.                                                  */
extern char __kernel_phys_start[];
extern char __kernel_phys_end[];

static inline void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame >> 3] |= (uint8_t)(1 << (frame & 7));
}

static inline void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame >> 3] &= (uint8_t)~(1 << (frame & 7));
}

static inline bool bitmap_test(uint32_t frame)
{
    return (frame_bitmap[frame >> 3] & (1 << (frame & 7))) != 0;
}

/* ---------------------------------------------------------------------------
 *  Reserving
 *
 *  Note the rounding: the start rounds *down* and the end rounds *up*. A
 *  region that occupies even one byte of a frame claims the whole frame,
 *  because frames are the unit of allocation and half a frame cannot be handed
 *  out. Rounding the other way would let the allocator give away the frame
 *  containing the last 100 bytes of the kernel, which is the kind of bug that
 *  corrupts something ten seconds later and a long way away.
 * ------------------------------------------------------------------------- */
void pmm_reserve_region(paddr_t start, size_t length)
{
    if (length == 0) return;

    uint32_t first = ADDR_TO_FRAME(ALIGN_DOWN(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_UP(start + length, PAGE_SIZE));

    for (uint32_t f = first; f < last && f < MAX_FRAMES; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }
}

static void pmm_free_region(paddr_t start, size_t length)
{
    uint32_t first = ADDR_TO_FRAME(ALIGN_UP(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_DOWN(start + length, PAGE_SIZE));

    for (uint32_t f = first; f < last && f < MAX_FRAMES; f++) {
        if (bitmap_test(f)) {
            bitmap_clear(f);
            used_frames--;
        }
    }
}

/* ---------------------------------------------------------------------------
 *  pmm_init
 *
 *  The order here is the important part, and it is the opposite of what feels
 *  natural. We start by marking *everything* as used, then free only what the
 *  firmware explicitly told us is available RAM, then re-reserve the parts of
 *  that RAM which are already spoken for.
 *
 *  Starting from "all used" rather than "all free" means that any region the
 *  memory map failed to mention -- a hole, an MMIO window, a chunk of RAM the
 *  firmware is hiding -- stays unavailable by default. The failure mode of
 *  being too cautious is that you waste memory. The failure mode of the
 *  opposite is handing a driver a frame that is really a device register.
 * ------------------------------------------------------------------------- */
void pmm_init(multiboot_info_t *mbi)
{
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
    total_frames = 0;

    /*  The running counter is meaningless until we know how much RAM there is,
     *  because "everything is used" here means all 4 GiB of frames the bitmap
     *  can describe, most of which do not exist. We recount from the bitmap at
     *  the end of this function, once total_frames is known, and the counter
     *  is authoritative from then on.                                         */
    used_frames = 0;

    if (!(mbi->flags & MB_INFO_MEM_MAP))
        panic("pmm: the bootloader gave us no memory map (flags=%08x)\n"
              "     Nimbus cannot guess how much RAM exists.", mbi->flags);

    /* ---- Walk the memory map ------------------------------------------------
     *
     * The entries are variable length. `size` counts the bytes *after* the
     * size field, so advancing by `size + 4` is correct and `p++` is not --
     * that would work only if every entry were exactly sizeof(entry), which
     * the specification does not promise and some firmware does not do.
     */
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)P2V(mbi->mmap_addr);
    uintptr_t end = (uintptr_t)P2V(mbi->mmap_addr) + mbi->mmap_length;

    kprintf("physical memory map:\n");

    while ((uintptr_t)entry < end) {
        const char *kind =
            entry->type == MULTIBOOT_MEMORY_AVAILABLE        ? "available" :
            entry->type == MULTIBOOT_MEMORY_RESERVED         ? "reserved"  :
            entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE ? "acpi"      :
            entry->type == MULTIBOOT_MEMORY_NVS              ? "nvs"       :
                                                               "bad";

        kprintf("  %08x - %08x  %s\n",
                (uint32_t)entry->addr,
                (uint32_t)(entry->addr + entry->len - 1),
                kind);

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            /*  Ignore anything above 4 GiB. On a 32-bit kernel without PAE we
             *  cannot address it, and a 64-bit length field means the
             *  arithmetic would silently wrap if we tried.                    */
            uint64_t base = entry->addr;
            uint64_t len  = entry->len;

            if (base < 0x100000000ULL) {
                if (base + len > 0x100000000ULL)
                    len = 0x100000000ULL - base;

                pmm_free_region((paddr_t)base, (size_t)len);

                uint32_t top_frame = ADDR_TO_FRAME(ALIGN_DOWN((paddr_t)(base + len), PAGE_SIZE));
                if (top_frame > total_frames) total_frames = top_frame;
            }
        }

        entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + 4);
    }

    /* ---- Take back what is already in use -----------------------------------
     *
     * Everything below 1 MiB. Some of it is genuinely free RAM, and a real
     * kernel reclaims it -- it is where you must put the trampoline code that
     * starts the other CPUs, because they boot in real mode. But it also holds
     * the interrupt vector table, the BIOS data area, video memory, option
     * ROMs and the BIOS itself, and the memory map is not always honest about
     * which is which. 1 MiB is a cheap price for not having to care.
     */
    pmm_reserve_region(0, 1 * MiB);

    /*  The kernel image. These symbols are physical addresses precisely so
     *  that this line can be written without a conversion that might be
     *  wrong.                                                                 */
    pmm_reserve_region((paddr_t)__kernel_phys_start,
                       (size_t)(__kernel_phys_end - __kernel_phys_start));

    /*  The multiboot structures themselves, which live somewhere in low memory
     *  that the map calls "available". Overwriting them before we have read
     *  the module list is a classic and very confusing bug.                   */
    pmm_reserve_region(V2P((uintptr_t)mbi), sizeof(multiboot_info_t));
    if (mbi->flags & MB_INFO_MEM_MAP)
        pmm_reserve_region(mbi->mmap_addr, mbi->mmap_length);

    /*  Any modules the bootloader loaded -- for us, the initrd.               */
    if (mbi->flags & MB_INFO_MODS) {
        multiboot_module_t *mods = (multiboot_module_t *)P2V(mbi->mods_addr);
        pmm_reserve_region(mbi->mods_addr,
                           mbi->mods_count * sizeof(multiboot_module_t));
        for (uint32_t i = 0; i < mbi->mods_count; i++)
            pmm_reserve_region(mods[i].mod_start,
                               mods[i].mod_end - mods[i].mod_start);
    }

    /* ---- Cap at the size of the kernel's direct map --------------------------
     *
     * mm/paging.c maps physical memory into the kernel half at 0xC0000000, and
     * that window is 256 MiB wide. A frame above the window has no permanent
     * kernel virtual address, so the moment the page-table code called P2V()
     * on it we would write page table entries into unmapped memory.
     *
     * Rather than half-support such frames, we refuse to hand them out at all.
     * The honest cost is that a machine with 4 GiB of RAM runs Nimbus in
     * 256 MiB. The alternative is recursive page tables or a temporary mapping
     * window, which Chapter 28 describes and which you should reach for the
     * day this limit actually gets in your way.
     */
    if ((uint64_t)total_frames * PAGE_SIZE > (uint64_t)DIRECT_MAP_LIMIT) {
        uint32_t usable = ADDR_TO_FRAME(DIRECT_MAP_LIMIT);
        for (uint32_t f = usable; f < total_frames; f++)
            bitmap_set(f);
        LOG_WARN("pmm: ignoring RAM above %u MiB (direct map limit)",
                 DIRECT_MAP_LIMIT / MiB);
    }

    /*  Recount, now that total_frames is known. From here on every alloc and
     *  free keeps the counter in step, and heap_validate()-style consistency
     *  checks can compare the two.                                            */
    used_frames = 0;
    for (uint32_t f = 0; f < total_frames; f++)
        if (bitmap_test(f)) used_frames++;

    search_hint = ADDR_TO_FRAME(1 * MiB);

    kprintf("physical memory: %u MiB total, %u MiB free\n",
            (total_frames * PAGE_SIZE) / MiB,
            ((total_frames - used_frames) * PAGE_SIZE) / MiB);
}

/* ---------------------------------------------------------------------------
 *  Allocation
 * ------------------------------------------------------------------------- */
paddr_t pmm_alloc_frame(void)
{
    uint32_t flags = irq_save();

    /*  Scan from the hint, then wrap. The hint means a sequence of
     *  allocate-allocate-allocate does not rescan the same full region every
     *  time, which turns O(n) per call into O(1) amortised for the common
     *  pattern.                                                               */
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? search_hint : 0;
        uint32_t stop  = (pass == 0) ? total_frames : search_hint;

        for (uint32_t byte = start >> 3; byte < (stop + 7) >> 3; byte++) {
            if (frame_bitmap[byte] == 0xFF) continue;   /* all eight are taken */

            for (uint32_t bit = 0; bit < 8; bit++) {
                uint32_t frame = (byte << 3) + bit;
                if (frame >= total_frames) break;
                if (bitmap_test(frame)) continue;

                bitmap_set(frame);
                used_frames++;
                search_hint = frame + 1;
                irq_restore(flags);
                return FRAME_TO_ADDR(frame);
            }
        }
    }

    irq_restore(flags);
    return PMM_NO_FRAME;
}

void pmm_free_frame(paddr_t addr)
{
    uint32_t frame = ADDR_TO_FRAME(addr);

    if (frame >= MAX_FRAMES)
        panic("pmm_free_frame: %08x is not a valid physical address", addr);

    uint32_t flags = irq_save();

    /*  Double free. This is worth a panic rather than a warning: the frame is
     *  now on the free list twice, so two different subsystems are about to be
     *  handed the same memory, and the resulting corruption will be blamed on
     *  whichever of them is unluckier.                                        */
    if (!bitmap_test(frame))
        panic("pmm_free_frame: frame %u (%08x) was already free", frame, addr);

    bitmap_clear(frame);
    used_frames--;

    if (frame < search_hint) search_hint = frame;

    irq_restore(flags);
}

/* ---------------------------------------------------------------------------
 *  Contiguous allocation
 *
 *  Needed for exactly two things: a DMA buffer, because a device follows
 *  physical addresses and knows nothing about page tables, and occasionally a
 *  structure the hardware requires to be contiguous.
 *
 *  It is much more likely to fail than single-frame allocation, and gets worse
 *  the longer the system runs, because the free frames become scattered. That
 *  is external fragmentation, and it is the reason serious kernels keep a
 *  separate pool for DMA rather than hoping.
 * ------------------------------------------------------------------------- */
paddr_t pmm_alloc_frames(size_t count)
{
    if (count == 0) return PMM_NO_FRAME;
    if (count == 1) return pmm_alloc_frame();

    uint32_t flags = irq_save();

    uint32_t run_start = 0;
    uint32_t run_len   = 0;

    for (uint32_t frame = 0; frame < total_frames; frame++) {
        if (bitmap_test(frame)) {
            run_len = 0;
            continue;
        }
        if (run_len == 0) run_start = frame;
        run_len++;

        if (run_len == count) {
            for (uint32_t f = run_start; f < run_start + count; f++) {
                bitmap_set(f);
                used_frames++;
            }
            irq_restore(flags);
            return FRAME_TO_ADDR(run_start);
        }
    }

    irq_restore(flags);
    return PMM_NO_FRAME;
}

void pmm_free_frames(paddr_t first, size_t count)
{
    for (size_t i = 0; i < count; i++)
        pmm_free_frame(first + i * PAGE_SIZE);
}

size_t pmm_total_frames(void)      { return total_frames; }
size_t pmm_used_frames(void)       { return used_frames; }
size_t pmm_free_frames_count(void) { return total_frames - used_frames; }

void pmm_dump_stats(void)
{
    kprintf("pmm: %u/%u frames used (%u MiB / %u MiB), hint at frame %u\n",
            used_frames, total_frames,
            (used_frames * PAGE_SIZE) / MiB,
            (total_frames * PAGE_SIZE) / MiB,
            search_hint);
}
