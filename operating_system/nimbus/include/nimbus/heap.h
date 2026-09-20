/* ===========================================================================
 *  nimbus/include/nimbus/heap.h  --  kmalloc and kfree
 * ===========================================================================
 *
 *  The PMM hands out 4 KiB frames. Almost nothing the kernel allocates is
 *  4 KiB: a task struct is 200 bytes, a VFS node is 100, a filename is 14. The
 *  heap sits on top of the PMM and turns pages into arbitrary-sized blocks.
 *
 *  Design: a single doubly-linked list of blocks, each with an 16-byte header,
 *  first-fit allocation, splitting on allocate and coalescing on free. That is
 *  the same algorithm K&R publish in chapter 8 of *The C Programming
 *  Language*, and it is a genuinely reasonable choice for a kernel heap that
 *  sees a few thousand live objects. Chapter 27 measures its fragmentation and
 *  explains what slab allocators do instead.
 *
 *  Explained in: docs/27-kernel-heap.md
 * =========================================================================== */
#ifndef NIMBUS_HEAP_H
#define NIMBUS_HEAP_H

#include <nimbus/types.h>

/*  Where the kernel heap lives in the virtual address space. It sits above the
 *  kernel image inside the higher half, and grows upwards by mapping fresh
 *  frames as it needs them.                                                   */
#define KHEAP_START     0xD0000000u
#define KHEAP_INITIAL   (1 * MiB)
#define KHEAP_MAX       (64 * MiB)

/*  Magic numbers in the block header. They cost 4 bytes per allocation and
 *  they catch, immediately and with a message, three of the four classic heap
 *  bugs: freeing a pointer that was never allocated, freeing one twice, and
 *  writing past the end of a block into the next header. The fourth -- writing
 *  past the end into the *data* of the next block -- needs guard pages, which
 *  Chapter 26 adds.                                                           */
#define HEAP_MAGIC_USED 0xA110C8EDu     /* "allocated" */
#define HEAP_MAGIC_FREE 0xF2EEB10Cu     /* "free block" */

typedef struct heap_block {
    uint32_t            magic;
    size_t              size;       /* payload bytes, not counting this header */
    struct heap_block  *next;
    struct heap_block  *prev;
} heap_block_t;

void   heap_init(void);

void  *kmalloc(size_t size);
void  *kcalloc(size_t count, size_t size);
void  *krealloc(void *ptr, size_t size);
void   kfree(void *ptr);

/*  Page-aligned allocation, for things the hardware requires to be aligned:
 *  page directories, page tables, DMA buffers.                                */
void  *kmalloc_aligned(size_t size, size_t alignment);

/*  Statistics and a consistency check. heap_validate() walks the whole list
 *  verifying magics and link symmetry; call it from a suspicious code path and
 *  it will tell you which allocation was corrupted before the crash.          */
void   heap_stats(size_t *total, size_t *used, size_t *free, size_t *blocks);
void   heap_dump(void);
bool   heap_validate(void);

#endif /* NIMBUS_HEAP_H */
