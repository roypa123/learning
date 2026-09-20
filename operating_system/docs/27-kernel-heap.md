# Chapter 27 — The kernel heap

[← Page faults](26-page-faults.md) · [Contents](README.md) · [Next: Address spaces →](28-address-spaces.md)

> 📖 **Line by line:** [heap.c](line-by-line/nimbus-heap.md)

---

## Goal

Build `kmalloc` and `kfree`. A doubly-linked list of blocks in address order, first fit, splitting on
allocate, coalescing on free — and four magic numbers that turn silent corruption into a panic with
a file and a line.

---

## 1. Why the PMM is not enough

The PMM hands out 4 KiB frames. Almost nothing the kernel allocates is 4 KiB:

| Object | Size |
|---|---|
| `task_t` | ~200 bytes |
| `vfs_node_t` | ~120 bytes |
| `file_t` | 16 bytes |
| `page_directory_t` | 8 bytes |
| a filename | 14 bytes |
| a pipe buffer | 4 KiB + 16 |

Allocating a frame for each would waste 95% of memory and run out after a few thousand objects.

So the heap sits on top of the PMM and turns pages into arbitrary-sized blocks:

```
    kmalloc(200)  ->  a 200-byte slice of a page the heap already owns
    heap grows    ->  paging_map_range -> pmm_alloc_frame
```

---

## 2. The design, and why this one

A single doubly-linked list of blocks in address order, each with a header. First-fit allocation.
Split on allocate if the remainder is worth keeping; coalesce with both neighbours on free.

That is the allocator from chapter 8 of *The C Programming Language*, and it is a genuinely
reasonable choice for a kernel heap seeing a few thousand live objects.

Three properties make it the right *first* allocator:

**You can hold it in your head.** When memory corruption happens — and it will — you can walk the
list by hand in GDB.

**Coalescing is exact**, so it cannot fragment in the pathological way a non-coalescing free list
can.

**It fails loudly.** Every block carries a magic number, so a double free, a free of a non-pointer,
and a buffer overrun into the next header are all caught at the moment they are noticed.

What it is bad at: allocation is O(number of blocks). A kernel that allocates thousands of small
objects spends real time walking. §9 covers what to do about that.

---

## 3. The block header

```c
typedef struct heap_block {
    uint32_t            magic;
    size_t              size;       /* payload bytes, not counting this header */
    struct heap_block  *next;
    struct heap_block  *prev;
} heap_block_t;
```

Sixteen bytes per allocation. For a 14-byte filename that is more overhead than payload, which is
the standard complaint about this design and the reason slab allocators exist.

```c
static inline void *block_payload(heap_block_t *b)
{
    return (void *)((uint8_t *)b + HEADER_SIZE);
}

static inline heap_block_t *payload_block(void *p)
{
    return (heap_block_t *)((uint8_t *)p - HEADER_SIZE);
}
```

The header sits immediately before the payload, so `kfree(p)` finds it by subtraction. That is why
`kfree` needs no lookup and no table — and why writing one byte before a `kmalloc`ed pointer corrupts
the header.

### 3.1 Address order, both directions

The list is kept in **address order** and is **doubly linked**. Both matter for coalescing.

Address order means that if two blocks are adjacent in memory, they are adjacent in the list — so
merging is a local operation rather than a search.

Doubly linked means `kfree` can merge with the *previous* block, which a singly-linked list cannot do
without walking from the head. Our userland `malloc` (Chapter 45) is singly linked and coalesces
forwards only, and the difference is exactly eight bytes per block against fragmentation on
free-in-reverse-order patterns.

---

## 4. The magic numbers

```c
#define HEAP_MAGIC_USED 0xA110C8EDu     /* "allocated" */
#define HEAP_MAGIC_FREE 0xF2EEB10Cu     /* "free block" */
```

Four bytes per allocation, and they catch three of the four classic heap bugs at the moment they
happen:

| Bug | How it is caught |
|---|---|
| Freeing a pointer that was never allocated | Magic is neither value |
| Freeing one twice | Magic is already `FREE` |
| Writing past the end of a block into the next header | Magic is corrupted |
| Writing past the end into the next block's *data* | **Not caught** — needs guard pages |

```c
void kfree(void *ptr)
{
    if (!ptr) return;

    heap_block_t *b = payload_block(ptr);

    if (b->magic == HEAP_MAGIC_FREE)
        panic("kfree(%p): this block is already free (double free)", ptr);

    if (b->magic != HEAP_MAGIC_USED)
        panic("kfree(%p): not a heap pointer (magic %08x)\n"
              "      Either it never came from kmalloc, or the header was overwritten.",
              ptr, b->magic);
```

Two distinct messages, because the two bugs have different causes and different fixes. A single
"invalid free" would make you work out which.

### 4.1 Poisoning

```c
        n->magic = 0xDEADBEEF;
```

When a block is absorbed by coalescing, its header is overwritten with a value that is neither magic.

If anything still holds a pointer to it — a stale `next` somewhere, a cached block pointer in a
caller — the next use trips the magic check instead of quietly walking a list that no longer exists.

Poisoning on free is a cheap and very effective habit. Some allocators go further and fill the whole
freed payload with a pattern, which catches use-after-free reads too; the cost is a `memset` per
free.

---

## 5. Growing the arena

```c
#define KHEAP_START     0xD0000000u
#define KHEAP_INITIAL   (1 * MiB)
#define KHEAP_MAX       (64 * MiB)
```

The heap occupies a fixed virtual range and grows by mapping fresh frames onto the next slice of it.

```c
static bool heap_grow(size_t needed)
{
    size_t chunk = ALIGN_UP(needed + HEADER_SIZE, 64 * KiB);

    if (heap_top + chunk > KHEAP_START + KHEAP_MAX) {
        LOG_ERR("heap: refusing to grow past %u MiB", KHEAP_MAX / MiB);
        return false;
    }

    paging_map_range(kernel_directory, heap_top, chunk, PTE_WRITABLE);
    ...
}
```

**Virtual address space is free.** Reserving 64 MiB of it costs nothing until pages are mapped. That
is why the heap can be contiguous in virtual memory while being scattered across physical frames —
and why `KHEAP_MAX` can be generous.

**`0xD0000000`** sits above the 256 MiB direct map ending at `0xD0000000` (Chapter 24, §2.1) and
below the rest of the kernel half. The two constants have to agree, and Chapter 24's
`DIRECT_MAP_SIZE` is what fixes this one.

**64 KiB chunks**, not one page at a time. Sixteen pages per `heap_grow` amortises the cost of the
walk and the TLB invalidations.

### 5.1 Coalescing at the seam

```c
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
```

The new chunk is appended and then merged with the previous block if they are adjacent.

> This single line is why a heap that grows repeatedly does not fragment at its own chunk boundaries.

Without it, a heap that has grown five times has five 16-byte headers sitting at 64 KiB intervals,
each splitting what should be one large free region into pieces. A request for 100 KiB would fail
with 300 KiB free.

---

## 6. Allocation

```c
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
            ...
```

### 6.1 Validating while walking

The magic check inside the allocation loop costs one comparison per block and turns a corrupted list
into a panic *at the corrupted block* rather than a wild pointer dereference three blocks later.

The message names the likely cause — "something wrote past the end of the block before it" — because
that is what a corrupted header almost always means, and saying so saves the reader a deduction.

### 6.2 First fit, and the alternatives

We take the first block that fits.

**Best fit** searches the whole list for the smallest adequate block. It wastes less per allocation
and — counterintuitively — fragments *worse*, because it leaves behind a trail of slivers too small
for anything.

**Worst fit** takes the largest block, on the theory that the remainder stays useful. It is worse
than both in practice.

**Next fit** is first fit that resumes where it stopped, like the PMM's search hint (Chapter 22,
§5.2). It is faster and fragments slightly worse.

First fit is the standard answer and the measurements that established it are from the 1970s. Knuth
covers them in *TAOCP* volume 1.

### 6.3 Splitting

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

```c
#define MIN_SPLIT       (HEADER_SIZE + 16)
```

Split only if the remainder would be at least 32 bytes — a 16-byte header plus a 16-byte payload.

Below that, hand over the whole block and waste up to 31 bytes. That is **internal fragmentation**,
deliberately traded against the **external fragmentation** that a heap full of 4-byte free blocks
would cause. A free block too small for any request is worse than waste: it is waste *plus* a list
node to walk.

### 6.4 Grow once, then fail

```c
        if (attempt == 0 && !heap_grow(size))
            break;
```

Two attempts. Walk the list; if nothing fits, grow and walk again. A second failure is real.

The alternative — growing inside the loop — risks an unbounded loop if `heap_grow` succeeds but the
new chunk still does not satisfy the request, which can happen if the request is larger than the
chunk size.

---

## 7. Freeing and coalescing

```c
    b->magic = HEAP_MAGIC_FREE;

    if (b->next && b->next->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b + HEADER_SIZE + b->size == (uint8_t *)b->next) {

        heap_block_t *n = b->next;
        b->size += HEADER_SIZE + n->size;
        b->next  = n->next;
        if (n->next) n->next->prev = b;
        n->magic = 0xDEADBEEF;
    }

    if (b->prev && b->prev->magic == HEAP_MAGIC_FREE &&
        (uint8_t *)b->prev + HEADER_SIZE + b->prev->size == (uint8_t *)b) {

        heap_block_t *p = b->prev;
        p->size += HEADER_SIZE + b->size;
        p->next  = b->next;
        if (b->next) b->next->prev = p;
        b->magic = 0xDEADBEEF;
    }
```

Merge forwards, then backwards. Order does not matter, and doing both means three adjacent free
blocks become one.

### 7.1 The adjacency test is not optional

```c
        (uint8_t *)b + HEADER_SIZE + b->size == (uint8_t *)b->next
```

Two blocks can be neighbours *in the list* without being neighbours *in memory*, because the arena
grows in chunks that may not touch (§5).

Merging without this test produces a block whose size spans a gap — and the gap is unmapped memory.
The next allocation from that block hands out addresses that page-fault, in the kernel, which is a
panic a long way from the cause.

It is one comparison and it is the most important line in the function.

---

## 8. Aligned allocation

```c
void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (alignment <= HEAP_ALIGN) return kmalloc(size);

    size_t padded = size + alignment + HEADER_SIZE;

    uint8_t *raw = (uint8_t *)kmalloc(padded);
    if (!raw) return NULL;

    uintptr_t aligned = ALIGN_UP((uintptr_t)raw + HEADER_SIZE, alignment);
    ...
```

Over-allocate, position the payload at the next aligned address, and put a header immediately before
it. The block in front stays an ordinary free block, so the list remains consistent and `kfree` needs
no special case.

The waste is up to `alignment - 1` bytes plus a header. For a page-aligned allocation that is 4 KiB
of slack, which is why page-sized objects — page directories, page tables — come straight from the
PMM instead:

```c
    kernel_directory->phys = alloc_zeroed_frame();
```

---

## 9. What a slab allocator would do instead

The complaint about this design: 16 bytes of header for a 16-byte object, and an O(n) search.

A **slab allocator** keeps a separate pool per object *type*:

- A slab is one or more pages, divided into fixed-size slots for one kind of object.
- Free slots are chained through their own memory, so there is no header at all.
- Allocation is: take the head of the free list. O(1), no search, no split, no coalesce.
- Objects can be kept *constructed* — a `task_t` returned to the slab keeps its initialised fields,
  so allocating one skips the setup.

The cost is a cache per type and a mechanism to grow and shrink them. Linux's `kmem_cache_create` is
exactly this, and most kernel allocations go through one.

For Nimbus it would be premature: we have maybe a dozen object types and a few hundred live objects,
and the debuggability of a single list is worth more than the speed. But it is the standard next
step, and Exercise 27.8 builds one for `task_t`.

---

## 10. Diagnostics

```c
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
```

Three invariants: magic, link symmetry, and bounds.

Call it from a suspicious code path and it tells you which allocation was corrupted *before* the
crash. Dropping a `ASSERT(heap_validate())` into the top and bottom of a function you suspect is one
of the most effective debugging moves available in Part III.

```c
void heap_dump(void)
{
    size_t total, used, freebytes, blocks;
    heap_stats(&total, &used, &freebytes, &blocks);

    kprintf("heap: %u KiB arena, %u KiB used, %u KiB free, %u blocks\n", ...);
}
```

Wired into the `mem` command from Chapter 20:

```
nimbus> mem
pmm: 1348/32512 frames used (5 MiB / 127 MiB), hint at frame 1351
heap: 1024 KiB arena, 47 KiB used, 976 KiB free, 23 blocks
```

Run it before and after an operation. If `used` or `blocks` grows and does not come back, something
leaks.

---

## 11. Running it

```
[    0.010] inf  heap: 1024 KiB at d0000000, header 16 bytes
```

### 11.1 A self-test

```c
static void heap_selftest(void)
{
    void *a = kmalloc(100);
    void *b = kmalloc(200);
    void *c = kmalloc(50);

    kprintf("a=%p b=%p c=%p\n", a, b, c);
    ASSERT((uintptr_t)b > (uintptr_t)a);
    ASSERT(heap_validate());

    size_t used_before;
    heap_stats(NULL, &used_before, NULL, NULL);

    kfree(b);
    void *d = kmalloc(200);
    ASSERT(d == b);                       /* first fit reused the hole */

    kfree(a); kfree(c); kfree(d);
    ASSERT(heap_validate());

    size_t blocks;
    heap_stats(NULL, NULL, NULL, &blocks);
    kprintf("after freeing everything: %u blocks\n", (uint32_t)blocks);

    (void)used_before;
    kprintf("heap self-test passed\n");
}
```

```
a=0xd0000010 b=0xd0000080 c=0xd0000160
after freeing everything: 1 blocks
heap self-test passed
```

**One block** at the end. That is the coalescing working: three separate allocations, freed in a
scattered order, merged back into a single free block covering the whole arena.

If it says 3, coalescing is broken and the heap will fragment until it fails.

### 11.2 Watching the checks fire

```c
    void *p = kmalloc(64);
    kfree(p);
    kfree(p);
```

```
*** KERNEL PANIC ***
kfree(0xd0000010): this block is already free (double free)
```

```c
    void *p = kmalloc(64);
    ((uint32_t *)p)[16] = 0;              /* 64 bytes in: the next header */
    kmalloc(32);
```

```
*** KERNEL PANIC ***
heap: corrupt block at 0xd0000050 (magic 00000000)
      Something wrote past the end of the block before it.
```

Both fire at the moment the damage is noticed, with the address. Without the magics, the first would
corrupt the free list and the second would be discovered when someone dereferenced a garbage `next`.

---

## 12. Exercises

🟢 **27.1** Allocate 1000 blocks of random sizes, free every other one, then run `heap_dump`. How
many blocks, and how much free memory is in the largest block?

🟢 **27.2** Remove the adjacency test from the forward coalesce and construct a case that merges
across a chunk boundary. You will need a heap that has grown at least twice.

🟢 **27.3** Set `MIN_SPLIT` to `HEADER_SIZE` (split always) and repeat 27.1. Compare the block count.

🟡 **27.4** Add a `kmalloc` caller tag: store `__builtin_return_address(0)` in the header and add a
`heapleaks` command grouping live blocks by caller. This is the single most useful tool for finding
leaks.

🟡 **27.5** Implement `heap_shrink`: if the last block is free and larger than 64 KiB, unmap its
pages and return them to the PMM. Then work out why a real heap rarely bothers.

🟡 **27.6** Fill freed payloads with `0xDD` and check the pattern on allocate. This catches
use-after-free writes. Measure the cost on a `memcpy`-heavy workload.

🔴 **27.7** Give each allocation a guard page: round every request up to a page, allocate an extra
page after it, and leave it unmapped. Memory usage explodes and every overrun becomes a clean page
fault with the exact address. This is what Valgrind and Electric Fence do, and it is worth having as
a debug build.

🔴 **27.8** Write a slab allocator for `task_t`: a cache of pre-sized slots chained through their own
memory, with no per-object header. Compare allocation time and memory overhead against `kmalloc`.

---

## What we covered

- Why frames are not enough, and where the heap sits in the stack.
- The K&R allocator, and the three properties that make it the right first one.
- A 16-byte header found by subtraction, address-ordered, doubly linked — and what each of those
  three buys.
- Magic numbers that catch three of the four classic heap bugs at the moment they happen, with two
  distinct messages because the causes differ.
- Poisoning absorbed headers so stale pointers trip the check.
- Growing in 64 KiB chunks into free virtual address space, and the coalesce at the seam that stops
  the heap fragmenting at its own boundaries.
- First fit versus best fit, and why best fit fragments worse.
- `MIN_SPLIT`, trading internal fragmentation against external.
- The adjacency test — two blocks can be list neighbours without being memory neighbours.
- Aligned allocation by over-allocating and splicing a second header in.
- What a slab allocator would change, and why it is the standard next step.
- Three invariants in `heap_validate`, and a `mem` command that finds leaks in one line.

[Chapter 28](28-address-spaces.md) finishes Part III: per-process page directories, what `fork` has
to copy, and the difference between sharing a table and copying one.

---

[← Page faults](26-page-faults.md) · [Contents](README.md) · [Next: Address spaces →](28-address-spaces.md)
