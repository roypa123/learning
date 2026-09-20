/* ===========================================================================
 *  nimbus/mm/heap.c  --  kmalloc and kfree
 * ===========================================================================
 *
 *  A doubly-linked list of blocks in address order, each with a header. First
 *  fit on allocate; split if the leftover is worth keeping. Coalesce with both
 *  neighbours on free. This is the allocator from K&R chapter 8, with the
 *  addition of magic numbers and a growable arena.
 *
 *  Three properties make it the right first allocator:
 *
 *    * You can hold the whole thing in your head. When memory corruption
 *      happens -- and it will -- you can walk the list by hand in GDB.
 *    * Coalescing is exact, so it cannot fragment in the pathological way a
 *      non-coalescing free list can.
 *    * It fails loudly. Every block carries a magic number, so a double free,
 *      a free of a non-pointer, and a buffer overrun into the next header are
 *      all caught at the moment they are noticed rather than three subsystems
 *      later.
 *
 *  What it is bad at: allocation is O(number of blocks), and a kernel that
 *  allocates thousands of small objects spends real time walking. The standard
 *  answer is a slab allocator -- a separate pool per object size, with no
 *  search at all -- and Chapter 27 sketches one.
 *
 *  Explained in: docs/27-kernel-heap.md
 *  Line by line: docs/line-by-line/nimbus-heap.md
 * =========================================================================== */

#include <nimbus/heap.h>
#include <nimbus/paging.h>
#include <nimbus/pmm.h>
#include <nimbus/kernel.h>
#include <nimbus/string.h>
#include <nimbus/io.h>

static heap_block_t *heap_head = NULL;   /* lowest-addressed block            */
static vaddr_t       heap_top  = 0;      /* first unmapped byte of the arena  */
static size_t        heap_size = 0;

/*  Payloads are 8-byte aligned. The x86 does not require it for correctness,
 *  but an 8-byte aligned pointer is what `double` and `long long` want, and
 *  more importantly it means a header always starts on a boundary we can
 *  recognise when reading a memory dump.                                      */
#define HEAP_ALIGN      8
#define HEADER_SIZE     ((size_t)sizeof(heap_block_t))

/*  Below this, splitting is not worth it: the leftover would be a block whose
 *  header is bigger than its payload, and a heap full of those is a heap with
 *  no memory in it.                                                           */
#define MIN_SPLIT       (HEADER_SIZE + 16)

static inline void *block_payload(heap_block_t *b)
{
    return (void *)((uint8_t *)b + HEADER_SIZE);
}

static inline heap_block_t *payload_block(void *p)
{
    return (heap_block_t *)((uint8_t *)p - HEADER_SIZE);
}

/* ---------------------------------------------------------------------------
 *  Growing the arena
 *
 *  The heap occupies a fixed virtual range starting at KHEAP_START, and grows
 *  by mapping fresh physical frames onto the next slice of it. Virtual address
 *  space is free -- reserving 64 MiB of it costs nothing until pages are
 *  mapped -- which is why the heap can be contiguous in virtual memory while
 *  being scattered across physical frames.
 * ------------------------------------------------------------------------- */
static bool heap_grow(size_t needed)
{
    size_t chunk = ALIGN_UP(needed + HEADER_SIZE, 64 * KiB);

    if (heap_top + chunk > KHEAP_START + KHEAP_MAX) {
        LOG_ERR("heap: refusing to grow past %u MiB", KHEAP_MAX / MiB);
        return false;
    }

    paging_map_range(kernel_directory, heap_top, chunk, PTE_WRITABLE);

    heap_block_t *block = (heap_block_t *)heap_top;
    block->magic = HEAP_MAGIC_FREE;
    block->size  = chunk - HEADER_SIZE;
    block->next  = NULL;
    block->prev  = NULL;

    /*  Append to the end of the address-ordered list, then coalesce -- so that
     *  growing next to an existing free block produces one big block rather
     *  than two adjacent ones that can never satisfy a large request between
     *  them. This single line is why a heap that grows repeatedly does not
     *  fragment at its own chunk boundaries.                                  */
    if (!heap_head) {
        heap_head = block;
    } else {
        heap_block_t *last = heap_head;
        while (last->next) last = last->next;
        last->next  = block;
        block->prev = last;

        if (last->magic == HEAP_MAGIC_FREE &&
            (uint8_t *)last + HEADER_SIZE + last->size == (uint8_t *)block) {
            last->size += HEADER_SIZE + block->size;
            last->next = NULL;
        }
    }

    heap_top  += chunk;
    heap_size += chunk;
    return true;
}

void heap_init(void)
{
    heap_head = NULL;
    heap_top  = KHEAP_START;
    heap_size = 0;

    if (!heap_grow(KHEAP_INITIAL))
        panic("heap: could not create the initial arena");

    LOG_INFO("heap: %u KiB at %08x, header %u bytes",
             (uint32_t)(heap_size / KiB), KHEAP_START, (uint32_t)HEADER_SIZE);
}

/* ---------------------------------------------------------------------------
 *  kmalloc
 * ------------------------------------------------------------------------- */
void *kmalloc(size_t size)
{
    if (size == 0) return NULL;

    size = ALIGN_UP(size, HEAP_ALIGN);

    uint32_t flags = irq_save();

    for (int attempt = 0; attempt < 2; attempt++) {
        for (heap_block_t *b = heap_head; b; b = b->next) {
            if (b->magic != HEAP_MAGIC_FREE) {
                if (b->magic != HEAP_MAGIC_USED)
                    panic("heap: corrupt block at %p (magic %08x)\n"
                          "      Something wrote past the end of the block before it.",
                          (void *)b, b->magic);
                continue;
            }
            if (b->size < size) continue;

            /*  Split, if the remainder is big enough to be a usable block.
             *  Otherwise hand over the whole thing, wasting up to MIN_SPLIT
             *  bytes -- internal fragmentation, deliberately traded against
             *  the external fragmentation that tiny blocks would cause.       */
            if (b->size >= size + MIN_SPLIT) {
                heap_block_t *rest =
                    (heap_block_t *)((uint8_t *)b + HEADER_SIZE + size);

                rest->magic = HEAP_MAGIC_FREE;
                rest->size  = b->size - size - HEADER_SIZE;
                rest->next  = b->next;
                rest->prev  = b;

                if (b->next) b->next->prev = rest;
                b->next = rest;
                b->size = size;
            }

            b->magic = HEAP_MAGIC_USED;
            irq_restore(flags);
            return block_payload(b);
        }

        /*  Nothing fitted. Grow once and try again; a second failure is real. */
        if (attempt == 0 && !heap_grow(size))
            break;
    }

    irq_restore(flags);
    LOG_ERR("kmalloc(%u) failed", (uint32_t)size);
    return NULL;
}

void *kcalloc(size_t count, size_t size)
{
    /*  Overflow check. `count * size` wrapping is a classic way to allocate 8
     *  bytes and then write 4 GiB into them, and it is the reason calloc takes
     *  two arguments instead of one.                                          */
    if (count != 0 && size > (size_t)0xFFFFFFFFu / count) return NULL;

    size_t total = count * size;
    void  *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

/* ---------------------------------------------------------------------------
 *  kfree
 * ------------------------------------------------------------------------- */
void kfree(void *ptr)
{
    if (!ptr) return;                      /* free(NULL) is defined as a no-op */

    heap_block_t *b = payload_block(ptr);

    if (b->magic == HEAP_MAGIC_FREE)
        panic("kfree(%p): this block is already free (double free)", ptr);

    if (b->magic != HEAP_MAGIC_USED)
        panic("kfree(%p): not a heap pointer (magic %08x)\n"
              "      Either it never came from kmalloc, or the header was overwritten.",
              ptr, b->magic);

    uint32_t flags = irq_save();

    b->magic = HEAP_MAGIC_FREE;

    /*  Coalesce forwards. The adjacency test is not optional: two blocks can
     *  be neighbours in the list without being neighbours in memory, because
     *  the arena grows in chunks that may not touch.                          */
    if (b->next && b->next->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b + HEADER_SIZE + b->size == (uint8_t *)b->next) {

        heap_block_t *n = b->next;
        b->size += HEADER_SIZE + n->size;
        b->next  = n->next;
        if (n->next) n->next->prev = b;

        /*  Poison the absorbed header. If anything still holds a pointer to
         *  it, the next use will trip the magic check instead of quietly
         *  walking a list that no longer exists.                              */
        n->magic = 0xDEADBEEF;
    }

    /*  ...and backwards, by doing the same thing from the previous block.     */
    if (b->prev && b->prev->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b->prev + HEADER_SIZE + b->prev->size == (uint8_t *)b) {

        heap_block_t *p = b->prev;
        p->size += HEADER_SIZE + b->size;
        p->next  = b->next;
        if (b->next) b->next->prev = p;
        b->magic = 0xDEADBEEF;
    }

    irq_restore(flags);
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr)   return kmalloc(size);
    if (!size)  { kfree(ptr); return NULL; }

    heap_block_t *b = payload_block(ptr);
    if (b->magic != HEAP_MAGIC_USED)
        panic("krealloc(%p): not a live allocation", ptr);

    if (b->size >= size) return ptr;       /* it already fits */

    void *fresh = kmalloc(size);
    if (!fresh) return NULL;

    memcpy(fresh, ptr, b->size);
    kfree(ptr);
    return fresh;
}

/* ---------------------------------------------------------------------------
 *  kmalloc_aligned
 *
 *  Over-allocate, then position the payload at the next aligned address and
 *  place a header immediately before it. The block in front of it stays a
 *  perfectly ordinary free block, so the list remains consistent and kfree
 *  needs no special case.
 * ------------------------------------------------------------------------- */
void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (alignment <= HEAP_ALIGN) return kmalloc(size);

    /*  Worst case we waste alignment-1 bytes plus one header. For a page
     *  aligned allocation that is 4 KiB of slack, which is why page-sized
     *  objects such as page tables come straight from the PMM instead.        */
    size_t padded = size + alignment + HEADER_SIZE;

    uint8_t *raw = (uint8_t *)kmalloc(padded);
    if (!raw) return NULL;

    uintptr_t aligned = ALIGN_UP((uintptr_t)raw + HEADER_SIZE, alignment);

    heap_block_t *original = payload_block(raw);
    heap_block_t *header   = (heap_block_t *)(aligned - HEADER_SIZE);

    if ((uint8_t *)header != raw - HEADER_SIZE) {
        /*  Shrink the original block so it ends exactly where our new header
         *  begins, and splice the new one into the list after it.             */
        size_t front = (size_t)((uint8_t *)header - (uint8_t *)original) - HEADER_SIZE;

        header->magic = HEAP_MAGIC_USED;
        header->size  = original->size - front - HEADER_SIZE;
        header->next  = original->next;
        header->prev  = original;

        if (original->next) original->next->prev = header;
        original->next  = header;
        original->size  = front;
        original->magic = HEAP_MAGIC_FREE;
    }

    return (void *)aligned;
}

/* ---------------------------------------------------------------------------
 *  Diagnostics
 * ------------------------------------------------------------------------- */
void heap_stats(size_t *total, size_t *used, size_t *freebytes, size_t *blocks)
{
    size_t u = 0, f = 0, n = 0;

    for (heap_block_t *b = heap_head; b; b = b->next) {
        n++;
        if (b->magic == HEAP_MAGIC_USED) u += b->size;
        else                             f += b->size;
    }

    if (total)     *total     = heap_size;
    if (used)      *used      = u;
    if (freebytes) *freebytes = f;
    if (blocks)    *blocks    = n;
}

bool heap_validate(void)
{
    heap_block_t *prev = NULL;

    for (heap_block_t *b = heap_head; b; prev = b, b = b->next) {
        if (b->magic != HEAP_MAGIC_USED && b->magic != HEAP_MAGIC_FREE) {
            LOG_ERR("heap_validate: bad magic %08x at %p", b->magic, (void *)b);
            return false;
        }
        if (b->prev != prev) {
            LOG_ERR("heap_validate: broken back-link at %p", (void *)b);
            return false;
        }
        if ((vaddr_t)b < KHEAP_START || (vaddr_t)b >= heap_top) {
            LOG_ERR("heap_validate: block %p outside the arena", (void *)b);
            return false;
        }
    }
    return true;
}

void heap_dump(void)
{
    size_t total, used, freebytes, blocks;
    heap_stats(&total, &used, &freebytes, &blocks);

    kprintf("heap: %u KiB arena, %u KiB used, %u KiB free, %u blocks\n",
            (uint32_t)(total / KiB), (uint32_t)(used / KiB),
            (uint32_t)(freebytes / KiB), (uint32_t)blocks);
}
