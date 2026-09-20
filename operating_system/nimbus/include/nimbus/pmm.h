/* ===========================================================================
 *  nimbus/include/nimbus/pmm.h  --  the physical memory manager
 * ===========================================================================
 *
 *  The PMM answers exactly one question: "give me a free 4 KiB frame of
 *  physical RAM", and accepts exactly one statement: "I am done with this
 *  frame". It knows nothing about virtual addresses, processes or objects. It
 *  is the bottom of the memory stack and everything else is built on it.
 *
 *  Our implementation is a bitmap: one bit per 4 KiB frame, 1 = used. For
 *  4 GiB of RAM that is 1,048,576 bits = 128 KiB of bitmap, which is 0.003% of
 *  the memory it manages. Allocation is a linear scan for a clear bit, which
 *  is O(n) and would be a disaster in a real kernel; Chapter 22 explains what
 *  buddy allocators and free lists do about it, and why a bitmap is still the
 *  right first implementation.
 *
 *  Explained in: docs/22-pmm.md
 * =========================================================================== */
#ifndef NIMBUS_PMM_H
#define NIMBUS_PMM_H

#include <nimbus/types.h>
#include <nimbus/multiboot.h>

#define PAGE_SIZE       4096u
#define PAGE_SHIFT      12
#define PAGE_MASK       (PAGE_SIZE - 1)

/*  A frame number is a physical address divided by 4096. Using frame numbers
 *  rather than addresses in the allocator's internals means the arithmetic
 *  cannot silently produce an unaligned address.                              */
#define ADDR_TO_FRAME(a) ((uint32_t)(a) >> PAGE_SHIFT)
#define FRAME_TO_ADDR(f) ((paddr_t)(f) << PAGE_SHIFT)

/*  Returned by pmm_alloc_frame() when there is no memory left. Zero would be a
 *  bad sentinel: physical address 0 is a real, valid frame (it holds the
 *  interrupt vector table), and a kernel that treats 0 as failure will one day
 *  hand out frame 0 and then refuse to free it.                               */
#define PMM_NO_FRAME    ((paddr_t)0xFFFFFFFFu)

void    pmm_init(multiboot_info_t *mbi);

paddr_t pmm_alloc_frame(void);
void    pmm_free_frame(paddr_t frame);

/*  Contiguous allocation, needed for DMA buffers and for the page directory's
 *  own storage. Much more likely to fail than single-frame allocation once the
 *  system has been running a while, which is the whole reason DMA allocators
 *  exist as a separate thing in real kernels.                                 */
paddr_t pmm_alloc_frames(size_t count);
void    pmm_free_frames(paddr_t first, size_t count);

/*  Mark a region as permanently unavailable. Used for the kernel image itself,
 *  the multiboot structures, the initrd, and everything below 1 MiB.          */
void    pmm_reserve_region(paddr_t start, size_t length);

size_t  pmm_total_frames(void);
size_t  pmm_used_frames(void);
size_t  pmm_free_frames_count(void);
void    pmm_dump_stats(void);

#endif /* NIMBUS_PMM_H */
