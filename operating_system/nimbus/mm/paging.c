/* ===========================================================================
 *  nimbus/mm/paging.c  --  virtual memory
 * ===========================================================================
 *
 *  The one design decision that shapes this whole file: the kernel keeps a
 *  *direct map* of physical memory at 0xC0000000. Physical frame at 0x1234000
 *  is always readable at virtual 0xC1234000, for the entire life of the
 *  machine, in every address space.
 *
 *  Why that matters. To edit a page table you must write to it, and to write
 *  to it you need a virtual address for it -- but the page table is a physical
 *  frame the allocator just handed you, and nothing maps it yet. That is a
 *  genuine chicken-and-egg problem and there are three standard answers:
 *
 *    1. Direct map all of physical memory into the kernel half. Simple,
 *       constant-time, and what we do. The cost is address space: our kernel
 *       half is 1 GiB, we spend 256 MiB of it on the map, and RAM beyond that
 *       is unusable.
 *    2. Recursive page tables: point one directory entry at the directory
 *       itself, so the tables appear in the address space at a computable
 *       address. Elegant, costs 4 MiB of address space and nothing else, and
 *       genuinely hard to reason about the first ten times. Chapter 28 works
 *       through the arithmetic.
 *    3. A temporary mapping: keep one page of address space reserved, map the
 *       frame there, edit it, unmap. Works with any amount of RAM, needs a
 *       lock, and costs a TLB flush per edit.
 *
 *  Explained in: docs/23-paging-theory.md through docs/28-address-spaces.md
 *  Line by line: docs/line-by-line/nimbus-paging.md
 * =========================================================================== */

#include <nimbus/paging.h>
#include <nimbus/pmm.h>
#include <nimbus/heap.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/io.h>
#include <nimbus/isr.h>
#include <nimbus/task.h>

/*  How much physical memory the direct map covers. Chosen so that the map
 *  (0xC0000000..0xCFFFFFFF) ends below the kernel heap at 0xD0000000.         */
#define DIRECT_MAP_SIZE (256u * MiB)

page_directory_t *kernel_directory  = NULL;
page_directory_t *current_directory = NULL;

/*  The kernel's own directory descriptor cannot be kmalloc'd: the heap does
 *  not exist when paging_init() runs, because the heap needs paging.          */
static page_directory_t kernel_directory_storage;

static inline uint32_t dir_index(vaddr_t v)   { return v >> 22; }
static inline uint32_t table_index(vaddr_t v) { return (v >> 12) & 0x3FF; }

static inline uint32_t read_cr2(void)
{
    uint32_t v;
    __asm__ volatile ("movl %%cr2, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(paddr_t v)
{
    __asm__ volatile ("movl %0, %%cr3" :: "r"(v) : "memory");
}

static inline uint32_t read_cr3(void)
{
    uint32_t v;
    __asm__ volatile ("movl %%cr3, %0" : "=r"(v));
    return v;
}

void paging_flush_tlb(void)
{
    /*  Writing CR3 flushes every TLB entry that is not marked global. It is
     *  the sledgehammer: correct always, and expensive, because the next few
     *  thousand memory accesses all miss the TLB. Use paging_invalidate() for
     *  a single page.                                                         */
    write_cr3(read_cr3());
}

/* ---------------------------------------------------------------------------
 *  A fresh, zeroed frame
 *
 *  Every page table and every page directory must start at zero, because a
 *  nonzero entry with the present bit set by accident is a mapping to a random
 *  physical frame, and the CPU will happily use it.
 * ------------------------------------------------------------------------- */
static paddr_t alloc_zeroed_frame(void)
{
    paddr_t frame = pmm_alloc_frame();
    if (frame == PMM_NO_FRAME)
        panic("paging: out of physical memory");

    memset(P2V(frame), 0, PAGE_SIZE);
    return frame;
}

/* ---------------------------------------------------------------------------
 *  paging_get_entry -- walk the two levels, optionally building as we go
 * ------------------------------------------------------------------------- */
uint32_t *paging_get_entry(page_directory_t *dir, vaddr_t virt, bool create)
{
    uint32_t di = dir_index(virt);
    uint32_t ti = table_index(virt);

    uint32_t pde = dir->entries[di];

    if (!(pde & PTE_PRESENT)) {
        if (!create) return NULL;

        paddr_t table = alloc_zeroed_frame();

        /*  Directory entries are permissive: PRESENT | WRITABLE | USER. The
         *  *effective* permission for an access is the AND of the directory
         *  entry's and the table entry's, so leaving the directory open and
         *  enforcing per-page in the table means we never have to go back and
         *  widen a directory entry when one page inside it becomes writable.
         *
         *  The cost is that a directory entry tells you nothing about what is
         *  inside it. Some kernels tighten the directory entry instead and
         *  gain a cheap "is any page in this 4 MiB user-accessible" check. */
        dir->entries[di] = table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        pde = dir->entries[di];
    }

    uint32_t *table = (uint32_t *)P2V(pde & PTE_FRAME_MASK);
    return &table[ti];
}

void paging_map(page_directory_t *dir, vaddr_t virt, paddr_t phys, uint32_t flags)
{
    uint32_t *pte = paging_get_entry(dir, virt, true);

    if (*pte & PTE_PRESENT) {
        /*  Overwriting a live mapping leaks the frame it pointed at and is
         *  almost always a bug in the caller. Loud is better than subtle.     */
        LOG_WARN("paging_map: %08x was already mapped to %08x, now %08x",
                 virt, *pte & PTE_FRAME_MASK, phys);
    }

    *pte = (phys & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;

    /*  The MMU caches translations in the TLB and does *not* notice that you
     *  changed the table. Until this instruction runs, the CPU may keep using
     *  the previous translation for this address -- including "not present",
     *  which means the mapping you just created appears not to work.          */
    paging_invalidate(virt);
}

void paging_unmap(page_directory_t *dir, vaddr_t virt)
{
    uint32_t *pte = paging_get_entry(dir, virt, false);
    if (!pte || !(*pte & PTE_PRESENT)) return;

    *pte = 0;
    paging_invalidate(virt);
}

paddr_t paging_virt_to_phys(page_directory_t *dir, vaddr_t virt)
{
    uint32_t *pte = paging_get_entry(dir, virt, false);
    if (!pte || !(*pte & PTE_PRESENT)) return PMM_NO_FRAME;

    /*  Do not forget the offset within the page. Returning only the frame
     *  address is a bug that works for every page-aligned test case.          */
    return (*pte & PTE_FRAME_MASK) | (virt & PAGE_MASK);
}

void paging_map_range(page_directory_t *dir, vaddr_t virt, size_t length, uint32_t flags)
{
    vaddr_t start = ALIGN_DOWN(virt, PAGE_SIZE);
    vaddr_t end   = ALIGN_UP(virt + length, PAGE_SIZE);

    for (vaddr_t v = start; v < end; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(dir, v, true);
        if (*pte & PTE_PRESENT) continue;
        *pte = (alloc_zeroed_frame() & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;
        paging_invalidate(v);
    }
}

void paging_alloc_range(page_directory_t *dir, vaddr_t virt, size_t length, uint32_t flags)
{
    paging_map_range(dir, virt, length, flags);
}

void paging_free_range(page_directory_t *dir, vaddr_t virt, size_t length)
{
    vaddr_t start = ALIGN_DOWN(virt, PAGE_SIZE);
    vaddr_t end   = ALIGN_UP(virt + length, PAGE_SIZE);

    for (vaddr_t v = start; v < end; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(dir, v, false);
        if (!pte || !(*pte & PTE_PRESENT)) continue;

        pmm_free_frame(*pte & PTE_FRAME_MASK);
        *pte = 0;
        paging_invalidate(v);
    }
}

/* ---------------------------------------------------------------------------
 *  paging_init
 *
 *  boot.asm left us running on a throwaway directory made of four 4 MiB pages.
 *  It got us here and it is wrong in two ways: it maps far too much as
 *  writable and executable, and it has no page tables, so nothing finer than
 *  4 MiB can ever be changed. We build the real one.
 * ------------------------------------------------------------------------- */
void paging_init(void)
{
    kernel_directory = &kernel_directory_storage;

    /*  The directory itself is one frame. We are still running on the boot
     *  directory, whose higher-half mapping covers the first 16 MiB of
     *  physical memory -- and the frame allocator hands out low memory first,
     *  so P2V() on this frame is reachable. That is a real dependency between
     *  two files and it is why BOOT_PAGES in boot.asm is 4 and not 1.         */
    kernel_directory->phys    = alloc_zeroed_frame();
    kernel_directory->entries = (uint32_t *)P2V(kernel_directory->phys);

    /* ---- The direct map ------------------------------------------------------
     *
     * Map physical [0, min(RAM, 256 MiB)) at virtual [0xC0000000, ...).
     *
     * With 4 KiB pages this is up to 65,536 page table entries across 64 page
     * tables, costing 256 KiB of page tables to describe 256 MiB of memory --
     * a 0.1% overhead, which is the standard cost of paging and the reason 4
     * MiB pages exist for exactly this case.
     *
     * We use 4 KiB pages anyway, because this is the mapping that must later
     * be *changed* page by page: to mark .rodata read-only, to unmap a guard
     * page, to make the frame holding a page table itself non-writable.
     */
    size_t ram = pmm_total_frames() * PAGE_SIZE;
    size_t mapped = MIN(ram, DIRECT_MAP_SIZE);

    for (paddr_t phys = 0; phys < mapped; phys += PAGE_SIZE) {
        vaddr_t virt = KERNEL_VIRTUAL_BASE + phys;

        uint32_t flags = PTE_PRESENT | PTE_WRITABLE;
        /*  No PTE_USER anywhere in the kernel half. This one missing bit is
         *  the entire boundary between a user process and the kernel: with it
         *  set, any program could read /etc/shadow out of the buffer cache. */

        uint32_t *pte = paging_get_entry(kernel_directory, virt, true);
        *pte = (phys & PTE_FRAME_MASK) | flags;
    }

    /* ---- Tighten the kernel's own mappings -----------------------------------
     *
     * .text and .rodata do not need to be writable. Making them read-only
     * turns "a wild pointer overwrote an instruction" -- which produces a
     * crash somewhere else entirely, minutes later -- into a page fault at the
     * offending store, with the address in CR2.
     *
     * This needs CR0.WP, set below, or the CPU ignores the read-only bit for
     * ring 0 accesses.
     */
    for (vaddr_t v = (vaddr_t)__text_start; v < (vaddr_t)__rodata_end; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(kernel_directory, v, false);
        if (pte && (*pte & PTE_PRESENT))
            *pte &= ~PTE_WRITABLE;
    }

    /* ---- CR0.WP --------------------------------------------------------------
     *
     * Write Protect. Without it, ring 0 may write to a read-only page --
     * a 386-era compatibility behaviour. With it, the kernel obeys its own
     * page permissions, which is what makes the read-only .text above mean
     * anything, and what makes copy-on-write work for pages the *kernel*
     * touches on behalf of a process.
     */
    uint32_t cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x00010000;                       /* bit 16 = WP */
    __asm__ volatile ("movl %0, %%cr0" :: "r"(cr0));

    paging_switch_directory(kernel_directory);

    LOG_INFO("paging: direct map %u MiB at %08x, kernel text read-only",
             (uint32_t)(mapped / MiB), KERNEL_VIRTUAL_BASE);
}

void paging_switch_directory(page_directory_t *dir)
{
    current_directory = dir;
    write_cr3(dir->phys);
}

/* ---------------------------------------------------------------------------
 *  A new, empty address space
 *
 *  The kernel half is *shared*, not copied: entries 768..1023 of every
 *  directory point at the same page tables. That is what makes a system call
 *  cheap -- the kernel is already mapped, at the same addresses, so entering
 *  it needs no CR3 reload and no TLB flush.
 *
 *  It also means a change to a kernel mapping made in one address space is
 *  visible in all of them, automatically, because they share the tables rather
 *  than copies of them. Get that wrong -- copy the entries instead of sharing
 *  the tables -- and a kmalloc that happens to grow the heap becomes visible
 *  only to the process that did it.
 * ------------------------------------------------------------------------- */
page_directory_t *paging_new_directory(void)
{
    page_directory_t *dir = (page_directory_t *)kmalloc(sizeof(page_directory_t));
    if (!dir) return NULL;

    dir->phys    = alloc_zeroed_frame();
    dir->entries = (uint32_t *)P2V(dir->phys);

    for (uint32_t i = KERNEL_PAGE_NUMBER; i < 1024; i++)
        dir->entries[i] = kernel_directory->entries[i];

    return dir;
}

/* ---------------------------------------------------------------------------
 *  Cloning, for fork()
 *
 *  Every user page is copied. That is honest, simple, and wasteful: fork()
 *  followed immediately by exec() -- which is what a shell does for every
 *  command -- copies an entire address space and then throws it away.
 *
 *  Copy-on-write fixes it: mark every page read-only in both copies, set our
 *  software COW bit, and do the copy in the page fault handler only when
 *  someone writes. Chapter 34 implements it; this version is the one to
 *  understand first, because the COW version is this plus a fault handler and
 *  a reference count, and both of those are easier to follow once you have
 *  seen what they are replacing.
 * ------------------------------------------------------------------------- */
page_directory_t *paging_clone_directory(page_directory_t *src)
{
    page_directory_t *dst = paging_new_directory();
    if (!dst) return NULL;

    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
        if (!(src->entries[di] & PTE_PRESENT)) continue;

        uint32_t *src_table = (uint32_t *)P2V(src->entries[di] & PTE_FRAME_MASK);

        paddr_t   dst_table_phys = alloc_zeroed_frame();
        uint32_t *dst_table      = (uint32_t *)P2V(dst_table_phys);

        dst->entries[di] = dst_table_phys | (src->entries[di] & PTE_FLAGS_MASK);

        for (uint32_t ti = 0; ti < 1024; ti++) {
            if (!(src_table[ti] & PTE_PRESENT)) continue;

            paddr_t new_frame = pmm_alloc_frame();
            if (new_frame == PMM_NO_FRAME)
                panic("fork: out of memory cloning address space");

            memcpy(P2V(new_frame),
                   P2V(src_table[ti] & PTE_FRAME_MASK),
                   PAGE_SIZE);

            dst_table[ti] = new_frame | (src_table[ti] & PTE_FLAGS_MASK);
        }
    }

    return dst;
}

void paging_free_directory(page_directory_t *dir)
{
    if (!dir || dir == kernel_directory) return;

    /*  Only the user half. Freeing the kernel tables would unmap the kernel
     *  from every other process, since they share them -- and the machine
     *  would die on the next system call anywhere in the system.              */
    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
        if (!(dir->entries[di] & PTE_PRESENT)) continue;

        uint32_t *table = (uint32_t *)P2V(dir->entries[di] & PTE_FRAME_MASK);
        for (uint32_t ti = 0; ti < 1024; ti++)
            if (table[ti] & PTE_PRESENT)
                pmm_free_frame(table[ti] & PTE_FRAME_MASK);

        pmm_free_frame(dir->entries[di] & PTE_FRAME_MASK);
    }

    pmm_free_frame(dir->phys);
    kfree(dir);
}

/* ---------------------------------------------------------------------------
 *  The page fault handler
 *
 *  A page fault is not necessarily an error. In a mature kernel most faults
 *  are entirely expected: a demand-paged executable, a stack that needs to
 *  grow, a copy-on-write page being written for the first time, a file being
 *  read through a memory mapping. The handler's job is to tell those apart
 *  from the genuine mistakes.
 *
 *  Two pieces of evidence:
 *
 *    CR2        the virtual address that was accessed. Set by the CPU, and
 *               *volatile* -- a second fault overwrites it, so it must be read
 *               before anything else that could fault, which in practice means
 *               before any kprintf.
 *
 *    error code bit 0  0 = the page was not present, 1 = a protection violation
 *               bit 1  0 = it was a read, 1 = a write
 *               bit 2  0 = the CPU was in ring 0, 1 = ring 3
 *               bit 3  a reserved bit was set in a page table entry
 *               bit 4  the fault was an instruction fetch
 * ------------------------------------------------------------------------- */
void page_fault_handler(registers_t *regs)
{
    uint32_t addr = read_cr2();

    bool present   = (regs->err_code & 0x1) != 0;
    bool write     = (regs->err_code & 0x2) != 0;
    bool user      = (regs->err_code & 0x4) != 0;
    bool reserved  = (regs->err_code & 0x8) != 0;
    bool fetch     = (regs->err_code & 0x10) != 0;

    /* ---- Stack growth --------------------------------------------------------
     *
     * A user program's stack starts as a single page and grows down. When it
     * runs off the bottom, the access faults on an address just below the
     * lowest mapped stack page -- and the right answer is not "kill it", it is
     * "map another page and retry".
     *
     * The guard is a distance check: an address within 64 KiB below the
     * current stack bottom is plausibly the stack; anything further is a wild
     * pointer that happens to point downwards. Real kernels use the same trick
     * with a configurable limit, which is what `ulimit -s` sets.
     */
    if (user && !present && current_task) {
        vaddr_t bottom = current_task->user_stack_bottom;

        if (addr < bottom && addr + 64 * KiB >= bottom && addr >= USER_STACK_TOP - USER_STACK_SIZE * 8) {
            vaddr_t newpage = ALIGN_DOWN(addr, PAGE_SIZE);

            paging_map(current_directory, newpage, alloc_zeroed_frame(),
                       PTE_WRITABLE | PTE_USER);
            current_task->user_stack_bottom = newpage;

            LOG_DEBUG("stack grew to %08x for pid %d", newpage, current_task->pid);
            return;                       /* retry the faulting instruction */
        }
    }

    /* ---- Everything else is a real fault ------------------------------------- */
    kprintf("\nPAGE FAULT at %08x  (eip=%08x)\n", addr, regs->eip);
    kprintf("  %s, %s, ring %d%s%s\n",
            present ? "protection violation" : "page not present",
            write ? "write" : "read",
            user ? 3 : 0,
            reserved ? ", RESERVED BIT SET IN A PAGE TABLE" : "",
            fetch ? ", instruction fetch" : "");

    if (addr < PAGE_SIZE)
        kprintf("  (address is in the first page: this is a null pointer dereference)\n");

    if (user) {
        kprintf("  killing pid %d\n", current_task ? current_task->pid : -1);
        isr_dump_registers(regs);
        task_exit(-11);                   /* what SIGSEGV would be */
    }

    isr_dump_registers(regs);
    panic("page fault in kernel mode at %08x, eip=%08x", addr, regs->eip);
}
