/* ===========================================================================
 *  nimbus/include/nimbus/paging.h  --  virtual memory
 * ===========================================================================
 *
 *  Two-level paging on 32-bit x86, in one picture:
 *
 *      virtual address  31          22 21          12 11              0
 *                      +--------------+--------------+----------------+
 *                      | directory ix |   table ix   |     offset     |
 *                      +--------------+--------------+----------------+
 *                            10 bits       10 bits        12 bits
 *
 *      CR3 -> page directory (1024 entries, one 4 KiB frame)
 *               entry[dir_ix] -> page table (1024 entries, one 4 KiB frame)
 *                                  entry[tbl_ix] -> physical frame
 *                                                     + offset
 *
 *  1024 * 1024 * 4096 = exactly 4 GiB, which is not a coincidence: the sizes
 *  were chosen so that a directory and a table are each exactly one page, and
 *  so that the whole thing tiles the address space with nothing left over.
 *
 *  Explained in: docs/23-paging-theory.md, docs/24-enabling-paging.md,
 *                docs/25-higher-half.md, docs/28-address-spaces.md
 * =========================================================================== */
#ifndef NIMBUS_PAGING_H
#define NIMBUS_PAGING_H

#include <nimbus/types.h>
#include <nimbus/isr.h>

/* ---------------------------------------------------------------------------
 *  The higher half
 *
 *  The kernel lives at virtual 0xC0000000 and above, mapped to physical
 *  0x00000000 and above. Userland gets everything below. This split is why
 *  every process can have its own address space while the kernel stays at the
 *  same addresses in all of them -- the top 256 page directory entries are
 *  shared between every process, so a system call does not need to change CR3.
 *
 *  3 GiB for user, 1 GiB for kernel is the traditional Linux split and it is
 *  traditional because 1 GiB is enough kernel address space to map a
 *  reasonable amount of RAM directly, and 3 GiB is more than any 32-bit
 *  program can usefully consume.
 * ------------------------------------------------------------------------- */
#define KERNEL_VIRTUAL_BASE 0xC0000000u
#define KERNEL_PAGE_NUMBER  (KERNEL_VIRTUAL_BASE >> 22)   /* = 768 */

/*  Convert between the two, for memory inside the identity-mapped kernel
 *  window only. These are not general address translation -- they work because
 *  the kernel window is a simple constant offset, and they are wrong for any
 *  page mapped anywhere else. paging_virt_to_phys() does the real walk.       */
#define V2P(a) ((paddr_t)((uintptr_t)(a) - KERNEL_VIRTUAL_BASE))
#define P2V(a) ((void *)((uintptr_t)(a) + KERNEL_VIRTUAL_BASE))

/* ---------------------------------------------------------------------------
 *  Entry flag bits. The same bits mean the same things in a directory entry
 *  and in a table entry, with one exception (PS).
 * ------------------------------------------------------------------------- */
#define PTE_PRESENT     0x001u  /* 0 here means "fault on any access"          */
#define PTE_WRITABLE    0x002u  /* 0 = read-only (for ring 3; see WP below)    */
#define PTE_USER        0x004u  /* 1 = ring 3 may touch it. This is the wall.  */
#define PTE_WRITETHROUGH 0x008u
#define PTE_NOCACHE     0x010u  /* essential for MMIO: caching a device
                                   register means reading a stale value forever */
#define PTE_ACCESSED    0x020u  /* set by the CPU on any access; we clear it   */
#define PTE_DIRTY       0x040u  /* set by the CPU on a write (table entries)   */
#define PTE_PAGE_SIZE   0x080u  /* directory entries only: this maps 4 MiB     */
#define PTE_GLOBAL      0x100u  /* survives a CR3 reload; needs CR4.PGE        */

/*  Bits 9, 10, 11 are ignored by the hardware and free for software use. We
 *  use one to mark a page as copy-on-write (Chapter 34).                      */
#define PTE_COW         0x200u

#define PTE_FRAME_MASK  0xFFFFF000u
#define PTE_FLAGS_MASK  0x00000FFFu

/*  A page directory. We carry the physical address alongside the virtual one
 *  because CR3 needs the physical and our code needs the virtual, and
 *  recomputing V2P() every time is exactly the kind of thing that works until
 *  the day the directory is allocated outside the kernel window.              */
typedef struct page_directory {
    uint32_t *entries;        /* virtual address: 1024 entries we can write   */
    paddr_t   phys;           /* physical address: what goes into CR3         */
} page_directory_t;

extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;

void  paging_init(void);
void  paging_switch_directory(page_directory_t *dir);

/*  Map one virtual page to one physical frame with the given flags. Allocates
 *  a page table if the directory entry is empty.                              */
void  paging_map(page_directory_t *dir, vaddr_t virt, paddr_t phys, uint32_t flags);
void  paging_unmap(page_directory_t *dir, vaddr_t virt);

/*  Map a range, and allocate the frames for it too. The workhorse: this is
 *  what grows a heap, loads an ELF segment, or creates a user stack.          */
void  paging_map_range(page_directory_t *dir, vaddr_t virt, size_t length, uint32_t flags);
void  paging_alloc_range(page_directory_t *dir, vaddr_t virt, size_t length, uint32_t flags);
void  paging_free_range(page_directory_t *dir, vaddr_t virt, size_t length);

paddr_t paging_virt_to_phys(page_directory_t *dir, vaddr_t virt);
uint32_t *paging_get_entry(page_directory_t *dir, vaddr_t virt, bool create);

/*  A fresh address space: the kernel half shared, the user half empty.        */
page_directory_t *paging_new_directory(void);

/*  A copy of an address space, for fork(). Kernel pages are shared; user pages
 *  are copied (Chapter 34 turns this into copy-on-write).                     */
page_directory_t *paging_clone_directory(page_directory_t *src);
void  paging_free_directory(page_directory_t *dir);

/*  Invalidate one TLB entry. The MMU caches translations, and changing a page
 *  table entry does NOT invalidate the cache -- the CPU will keep using the
 *  old translation, possibly for a very long time, with no diagnostic. Every
 *  single mapping change must be followed by one of these.                    */
static ALWAYS_INLINE void paging_invalidate(vaddr_t virt)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

void  paging_flush_tlb(void);
void  page_fault_handler(registers_t *regs);

#endif /* NIMBUS_PAGING_H */
