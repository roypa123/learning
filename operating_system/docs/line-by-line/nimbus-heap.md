# Line by line: `nimbus/mm/heap.c`

[Index](README.md) · [Chapter 27](../27-kernel-heap.md)

---

## Constants

```c
#define KHEAP_START     0xD0000000u
#define KHEAP_INITIAL   (1 * MiB)
#define KHEAP_MAX       (64 * MiB)
```
⚠️ `0xD0000000` sits immediately above the 256 MiB direct map. Change `DIRECT_MAP_SIZE` in
`paging.c` and this must move with it.

Virtual address space is free — reserving 64 MiB costs nothing until pages are mapped — which is why
`KHEAP_MAX` can be generous.

```c
#define HEAP_MAGIC_USED 0xA110C8EDu     /* "allocated" */
#define HEAP_MAGIC_FREE 0xF2EEB10Cu     /* "free block" */
```
Four bytes per allocation, catching three of the four classic heap bugs at the moment they happen:

| Bug | Caught by |
|---|---|
| freeing a pointer never allocated | magic is neither value |
| freeing one twice | magic is already `FREE` |
| writing past the end into the next header | magic is corrupted |
| writing past the end into the next block's **data** | **not caught** — needs guard pages |

```c
#define HEAP_ALIGN      8
#define MIN_SPLIT       (HEADER_SIZE + 16)
```
Split only if the remainder would be at least 32 bytes.

⚠️ Below that, hand over the whole block and waste up to 31 bytes. That is internal fragmentation,
deliberately traded against external: a free block too small for any request is waste *plus* a list
node to walk.

---

## The header

```c
typedef struct heap_block {
    uint32_t            magic;
    size_t              size;       /* payload, not counting this header */
    struct heap_block  *next;
    struct heap_block  *prev;
} heap_block_t;
```
Sixteen bytes. For a 14-byte filename that is more overhead than payload — the standard complaint,
and why slab allocators exist.

```c
static inline heap_block_t *payload_block(void *p)
{
    return (heap_block_t *)((uint8_t *)p - HEADER_SIZE);
}
```
⚠️ The header sits immediately before the payload, so `kfree(p)` finds it by subtraction — no lookup,
no table. And writing one byte *before* a `kmalloc`ed pointer corrupts it.

⚠️ **Doubly linked and in address order**, both deliberately. Address order means memory neighbours
are list neighbours, so merging is local. `prev` is what lets `kfree` merge backwards — which the
userland `malloc`, being singly linked, cannot do.

---

## `heap_grow`

```c
    size_t chunk = ALIGN_UP(needed + HEADER_SIZE, 64 * KiB);
```
64 KiB at a time. Sixteen pages per call amortises the walk and the TLB invalidations.

```c
    if (heap_top + chunk > KHEAP_START + KHEAP_MAX) {
        LOG_ERR("heap: refusing to grow past %u MiB", KHEAP_MAX / MiB);
        return false;
    }
```

```c
    paging_map_range(kernel_directory, heap_top, chunk, PTE_WRITABLE);
```
⚠️ `kernel_directory`, not `current_directory`. The heap is in the shared kernel half, so mapping it
there makes it visible in every address space (Ch. 28, §2.2).

No `PTE_USER`.

```c
        if (last->magic == HEAP_MAGIC_FREE &&
            (uint8_t *)last + HEADER_SIZE + last->size == (uint8_t *)block) {
            last->size += HEADER_SIZE + block->size;
            last->next = NULL;
        }
```
⚠️ **This single line is why a heap that grows repeatedly does not fragment at its own chunk
boundaries.**

Without it, a heap that has grown five times has five 16-byte headers at 64 KiB intervals, each
splitting what should be one large free region. A request for 100 KiB fails with 300 KiB free.

---

## `kmalloc`

```c
    size = ALIGN_UP(size, HEAP_ALIGN);
    uint32_t flags = irq_save();
```
⚠️ Callable from interrupt context — the page fault handler allocates.

```c
            if (b->magic != HEAP_MAGIC_FREE) {
                if (b->magic != HEAP_MAGIC_USED)
                    panic("heap: corrupt block at %p (magic %08x)\n"
                          "      Something wrote past the end of the block before it.",
                          (void *)b, b->magic);
                continue;
            }
```
Validating while walking. One comparison per block, and it turns a corrupted list into a panic **at
the corrupted block** rather than a wild pointer dereference three blocks later.

The message names the likely cause, which saves the reader a deduction.

```c
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
```
Four links to update, and the `if (b->next)` guard for the tail case.

**First fit.** Best fit searches the whole list for the smallest adequate block, wastes less per
allocation, and — counterintuitively — fragments *worse*, because it leaves a trail of slivers too
small for anything.

```c
        if (attempt == 0 && !heap_grow(size))
            break;
```
Two attempts: walk, grow, walk again. Growing *inside* the loop risks an unbounded loop if the
request is larger than the chunk size.

---

## `kcalloc`

```c
    if (count != 0 && size > (size_t)0xFFFFFFFFu / count) return NULL;
```
⚠️ Overflow check. `count * size` wrapping is the classic way to allocate 8 bytes and then write
4 GiB into them, and it is the reason `calloc` takes two arguments instead of one.

---

## `kfree`

```c
    if (!ptr) return;
```
`free(NULL)` is defined as a no-op.

```c
    if (b->magic == HEAP_MAGIC_FREE)
        panic("kfree(%p): this block is already free (double free)", ptr);

    if (b->magic != HEAP_MAGIC_USED)
        panic("kfree(%p): not a heap pointer (magic %08x)\n"
              "      Either it never came from kmalloc, or the header was overwritten.",
              ptr, b->magic);
```
⚠️ **Two distinct messages**, because the two bugs have different causes and different fixes. A single
"invalid free" would make you work out which.

```c
    if (b->next && b->next->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b + HEADER_SIZE + b->size == (uint8_t *)b->next) {
```
⚠️ **The adjacency test is the most important line in the function.**

Two blocks can be neighbours *in the list* without being neighbours *in memory*, because the arena
grows in chunks that may not touch.

Merging without it produces a block whose size spans a gap — and the gap is unmapped memory. The next
allocation from that block hands out addresses that page-fault, in the kernel, a long way from the
cause.

```c
        n->magic = 0xDEADBEEF;
```
Poisoning. If anything still holds a pointer to the absorbed header — a stale `next`, a cached block
pointer — the next use trips the magic check instead of quietly walking a list that no longer exists.

```c
    if (b->prev && b->prev->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b->prev + HEADER_SIZE + b->prev->size == (uint8_t *)b) {
```
Backwards too, which the doubly-linked list makes possible. Three adjacent free blocks become one.

---

## `krealloc`

```c
    if (b->size >= size) return ptr;
```
Already fits. No shrink — the block keeps its size, wasting the difference.

```c
    memcpy(fresh, ptr, b->size);
```
`b->size`, not `size`: copy what was there, not what was asked for.

---

## `kmalloc_aligned`

```c
    size_t padded = size + alignment + HEADER_SIZE;
    uint8_t *raw = (uint8_t *)kmalloc(padded);
    uintptr_t aligned = ALIGN_UP((uintptr_t)raw + HEADER_SIZE, alignment);
```
Over-allocate, position the payload at the next aligned address, put a header immediately before it.

```c
        original->size  = front;
        original->magic = HEAP_MAGIC_FREE;
```
The block in front stays an ordinary **free** block, so the list remains consistent and `kfree` needs
no special case.

⚠️ Waste is up to `alignment - 1` plus a header. For a page-aligned request that is 4 KiB of slack,
which is why page-sized objects come straight from the PMM instead.

---

## `heap_validate`

```c
    for (heap_block_t *b = heap_head; b; prev = b, b = b->next) {
        if (b->magic != HEAP_MAGIC_USED && b->magic != HEAP_MAGIC_FREE) { ... }
        if (b->prev != prev) { ... }
        if ((vaddr_t)b < KHEAP_START || (vaddr_t)b >= heap_top) { ... }
    }
```
Three invariants: magic, link symmetry, bounds.

Dropping `ASSERT(heap_validate())` into the top and bottom of a suspect function is one of the most
effective debugging moves in Part III — it tells you which allocation was corrupted *before* the
crash.

---

## What a slab allocator would change

A pool per object *type*: fixed-size slots, free slots chained through their own memory, **no header
at all**, allocation is "take the head of the free list".

O(1), no search, no split, no coalesce — and objects can be kept *constructed*, so allocating a
`task_t` skips the setup.

The cost is a cache per type and a mechanism to grow and shrink them. Linux's `kmem_cache_create` is
exactly this, and most kernel allocations go through one.

---

[Index](README.md) · [Chapter 27](../27-kernel-heap.md)
