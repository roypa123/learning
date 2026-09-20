# Chapter 22 — The physical memory manager

[← The memory map](21-memory-map.md) · [Contents](README.md) · [Next: Paging theory →](23-paging-theory.md)

> 📖 **Line by line:** [pmm.c](line-by-line/nimbus-pmm.md)

---

## Goal

Turn the memory map into an allocator. One bit per 4 KiB frame, two functions, and the foundation
that every other memory subsystem in the kernel sits on.

---

## 1. The interface, and its deliberate smallness

```c
paddr_t pmm_alloc_frame(void);
void    pmm_free_frame(paddr_t frame);
```

The PMM answers exactly one question — "give me a free 4 KiB frame of physical RAM" — and accepts
exactly one statement — "I am done with this one".

It knows nothing about virtual addresses, processes, or objects. It is the bottom of the memory
stack, and keeping the interface this narrow is what lets everything above it be simple:

```
    kmalloc / kfree             arbitrary sizes           Chapter 27
        |
    paging_map / paging_alloc   virtual -> physical       Chapter 24
        |
    pmm_alloc_frame             4 KiB of physical RAM     this chapter
        |
    the memory map                                        Chapter 21
```

Each layer uses only the one below. A bug in `kmalloc` cannot corrupt the frame allocator's state,
and a bug in the frame allocator shows up as exactly one thing: two subsystems handed the same
memory.

---

## 2. Why 4 KiB

Because that is the page size the MMU uses (Chapter 23). Allocating in any other unit would mean the
paging code doing arithmetic to combine or split allocations, for no benefit.

It is also a reasonable size in its own right. The tradeoff:

**Smaller pages** mean less waste when you need 100 bytes, but more page table entries — a 4 GiB
address space needs a million of them at 4 KiB, and four thousand at 4 MiB. They also mean more TLB
entries for the same working set, and the TLB is a small fixed-size cache.

**Larger pages** mean fewer table entries and better TLB coverage, but internal fragmentation: a
4 MiB page for a 4 KiB buffer wastes 99.9% of it.

4 KiB was chosen in 1985 and is still the default everywhere. Modern systems support 2 MiB and 1 GiB
pages *as well*, used for things like the kernel's direct map where the waste is zero because the
region is genuinely that large.

---

## 3. The bitmap

```c
#define MAX_FRAMES      (4u * 1024 * 1024 * 1024 / PAGE_SIZE)   /* 1,048,576 */
#define BITMAP_BYTES    (MAX_FRAMES / 8)                        /* 131,072   */

static uint8_t  frame_bitmap[BITMAP_BYTES];
```

One bit per frame, 1 = used. For 4 GiB that is 128 KiB of bitmap — **0.003% of the memory it
manages.**

```c
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
```

`frame >> 3` is `frame / 8`, the byte index. `frame & 7` is `frame % 8`, the bit within it. The
compiler would generate the same code from the division and modulo, and the shift form matches how
you think about it.

### 3.1 The fixed-size array, and the bootstrapping problem

128 KiB of `.bss`, sized for 4 GiB regardless of how much RAM the machine has.

A dynamically sized bitmap would be more elegant and would need somewhere to live — and the only
allocator that could provide that somewhere is this one.

> Bootstrapping problems are why the lowest layer of a kernel is usually the one with a fixed-size
> array in it.

Real kernels solve it with a two-stage boot allocator: a simple bump allocator that carves memory out
of the map, used to allocate the real allocator's structures, then discarded. Linux's `memblock` is
exactly this. It is about 150 lines and it is the right answer for a kernel that cares about the
128 KiB.

---

## 4. Frame numbers, not addresses

```c
#define ADDR_TO_FRAME(a) ((uint32_t)(a) >> PAGE_SHIFT)
#define FRAME_TO_ADDR(f) ((paddr_t)(f) << PAGE_SHIFT)
```

Internally the allocator works in frame numbers — physical address divided by 4096.

The reason is not efficiency; it is that **the arithmetic cannot silently produce an unaligned
address.** A frame number times 4096 is always page-aligned. An address plus 4096 is only aligned if
the original was, and "the original was" is an invariant somebody has to maintain.

It also makes the bitmap indexing direct: frame *n* is bit *n*.

### 4.1 The sentinel that is not zero

```c
#define PMM_NO_FRAME    ((paddr_t)0xFFFFFFFFu)
```

Returned on failure.

Zero would be a bad sentinel, and the reason is worth stating: **physical address 0 is a real, valid
frame.** It holds the interrupt vector table. A kernel that treats 0 as failure will one day hand out
frame 0 — after Exercise 21.4 reclaims low memory, say — and then refuse to free it, or free it
twice.

`0xFFFFFFFF` is not a valid frame address because it is not page-aligned, so it cannot collide with a
real return value.

---

## 5. Allocation

```c
paddr_t pmm_alloc_frame(void)
{
    uint32_t flags = irq_save();

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? search_hint : 0;
        uint32_t stop  = (pass == 0) ? total_frames : search_hint;

        for (uint32_t byte = start >> 3; byte < (stop + 7) >> 3; byte++) {
            if (frame_bitmap[byte] == 0xFF) continue;

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
```

A linear scan for a clear bit, with two optimisations and one piece of locking.

### 5.1 Skip full bytes

```c
            if (frame_bitmap[byte] == 0xFF) continue;
```

Eight frames checked in one comparison. On a nearly-full system this is the difference between
scanning a megabyte of bitmap and scanning 128 KiB of it.

The obvious extension is to compare 32 bits at a time, which is four times better again, and then to
use `bsf` (bit scan forward) to find the first clear bit in a word without looping. Exercise 22.4.

### 5.2 The search hint

```c
                search_hint = frame + 1;
```

Start the next search where this one stopped.

Without it, a sequence of allocations rescans the same full region every time, turning *n*
allocations into O(n²). With it, the common pattern — allocate, allocate, allocate — is O(1)
amortised.

The two-pass loop handles the wrap: scan from the hint to the end, then from the beginning to the
hint.

```c
    if (frame < search_hint) search_hint = frame;
```

in `pmm_free_frame` pulls the hint back when memory is freed below it, so a freed frame is reused
promptly rather than after a full wrap.

### 5.3 Why it is still bad

Worst case this is O(total memory). With 4 GiB nearly full, one allocation reads 128 KiB of bitmap.
The source says so up front:

> That is O(total memory) in the worst case and genuinely bad [...] The fix is a free list, or a
> buddy allocator, or at minimum a "search from where you left off" hint — we implement the hint,
> because it is four lines and removes the pathological case.

**A free list** — link every free frame into a list, using the frame's own memory to store the
pointer — makes allocation O(1). It is about twenty lines. The cost is that you can no longer ask
"is frame *n* free?" without walking the list, which the reservation code in Chapter 21 needs, and
that the list order becomes scattered, hurting locality.

**A buddy allocator** keeps free lists per power-of-two size, splitting and merging on demand.
Allocation and free are both O(log n), and it handles contiguous multi-frame allocation well — which
is §6's problem. Linux uses one. It is about 300 lines and it is the right answer for a real kernel.

A bitmap is still the right *first* implementation: you can hold it in your head, you can dump it,
and when memory corruption happens you can read it by eye.

### 5.4 The locking

```c
    uint32_t flags = irq_save();
    ...
    irq_restore(flags);
```

`pmm_alloc_frame` can be called from task context and from an interrupt handler — the page fault
handler allocates, and page faults arrive as interrupts.

So the bitmap update must be atomic against interrupts. `irq_save`/`irq_restore` rather than
`cli`/`sti`, because the caller might already have had interrupts off and turning them on underneath
would break an invariant it was relying on (Chapter 36).

---

## 6. Contiguous allocation

```c
paddr_t pmm_alloc_frames(size_t count)
{
    ...
    for (uint32_t frame = 0; frame < total_frames; frame++) {
        if (bitmap_test(frame)) { run_len = 0; continue; }
        if (run_len == 0) run_start = frame;
        run_len++;

        if (run_len == count) {
            for (uint32_t f = run_start; f < run_start + count; f++) {
                bitmap_set(f);
                used_frames++;
            }
            ...
            return FRAME_TO_ADDR(run_start);
        }
    }
    ...
}
```

Find *n* consecutive free frames.

Needed for exactly two things: a **DMA buffer**, because a device follows physical addresses and
knows nothing about page tables, and occasionally a structure the hardware requires to be contiguous.

Note it does not use the hint — a run could start anywhere, so the scan is from the beginning.

### 6.1 External fragmentation

This gets *worse the longer the system runs*, and it is the classic demonstration of external
fragmentation.

Allocate and free single frames for a while and the free frames become scattered. There may be 60 MiB
free and no four consecutive frames anywhere. The memory exists; it is just not arranged usefully.

That is why serious kernels keep a **separate pool** for DMA — reserved at boot, when memory is still
contiguous, and never used for anything else. It is also why `dma_alloc_coherent` on Linux can fail
on a machine with gigabytes free.

Our ATA driver uses PIO and never needs this. It is here because the day you write a driver that does
need it, the failure will be mysterious unless you already know this paragraph.

---

## 7. Freeing, and the two panics

```c
void pmm_free_frame(paddr_t addr)
{
    uint32_t frame = ADDR_TO_FRAME(addr);

    if (frame >= MAX_FRAMES)
        panic("pmm_free_frame: %08x is not a valid physical address", addr);

    uint32_t flags = irq_save();

    if (!bitmap_test(frame))
        panic("pmm_free_frame: frame %u (%08x) was already free", frame, addr);

    bitmap_clear(frame);
    used_frames--;

    if (frame < search_hint) search_hint = frame;

    irq_restore(flags);
}
```

Both checks panic rather than returning an error, and that is the right call.

**A double free** means the frame is now free twice. Two different subsystems will be handed the same
memory, and the resulting corruption gets blamed on whichever is unluckier. There is no recovery: the
allocator's state is already wrong, and the only question is whether you find out now, at the line
that caused it, or in half an hour somewhere unrelated.

**An out-of-range address** means the caller is not passing a physical address at all — almost always
a virtual address that should have gone through `V2P`. That is a bug in the caller, and a panic
naming the value makes it a thirty-second fix.

This is the general principle for kernel error handling: **if the invariant is already broken, stop.**
Returning `-EINVAL` from a double free lets the system continue in a state you cannot reason about.

---

## 8. Statistics

```c
void pmm_dump_stats(void)
{
    kprintf("pmm: %u/%u frames used (%u MiB / %u MiB), hint at frame %u\n",
            used_frames, total_frames,
            (used_frames * PAGE_SIZE) / MiB,
            (total_frames * PAGE_SIZE) / MiB,
            search_hint);
}
```

Wired into the mini-shell from Chapter 20, §6.1:

```
nimbus> mem
pmm: 1089/32512 frames used (4 MiB / 127 MiB), hint at frame 1092
heap: 1024 KiB arena, 12 KiB used, 1011 KiB free, 3 blocks
```

Run it before and after an operation and you know immediately whether something leaks. That is the
entire debugging methodology for Part III and it costs six lines.

---

## 9. Running it

```
physical memory map:
  ...
physical memory: 127 MiB total, 123 MiB free
[    0.000] inf  pmm: 1089/32512 frames used
```

### 9.1 A self-test worth running once

```c
static void pmm_selftest(void)
{
    size_t before = pmm_free_frames_count();

    paddr_t a = pmm_alloc_frame();
    paddr_t b = pmm_alloc_frame();
    paddr_t c = pmm_alloc_frame();

    kprintf("allocated %08x %08x %08x\n", a, b, c);
    ASSERT(a != b && b != c && a != c);
    ASSERT((a & PAGE_MASK) == 0);

    pmm_free_frame(b);
    paddr_t d = pmm_alloc_frame();
    kprintf("freed %08x, reallocated %08x\n", b, d);
    ASSERT(d == b);                       /* the hint pulled back */

    pmm_free_frame(a);
    pmm_free_frame(c);
    pmm_free_frame(d);

    ASSERT(pmm_free_frames_count() == before);
    kprintf("pmm self-test passed\n");
}
```

```
allocated 00500000 00501000 00502000
freed 00501000, reallocated 00501000
pmm self-test passed
```

Four properties checked: distinctness, alignment, immediate reuse after a free, and conservation.
That last one — the count returns to where it started — is the one that catches leaks.

### 9.2 Watching it fail

```c
    for (;;) {
        paddr_t f = pmm_alloc_frame();
        if (f == PMM_NO_FRAME) break;
    }
    pmm_dump_stats();
```

```
pmm: 32512/32512 frames used (127 MiB / 127 MiB), hint at frame 32512
```

Exhaustion returns the sentinel rather than crashing. Whether the *caller* handles it is another
matter — `alloc_zeroed_frame` in [`paging.c`](../nimbus/mm/paging.c) panics:

```c
static paddr_t alloc_zeroed_frame(void)
{
    paddr_t frame = pmm_alloc_frame();
    if (frame == PMM_NO_FRAME)
        panic("paging: out of physical memory");
    ...
}
```

which is correct for the paging code — it is called while building a page table, and there is no
sensible way to fail halfway — and wrong in general. A real kernel has an out-of-memory killer, and
Chapter 48 talks about why that is a policy problem rather than a mechanism problem.

---

## 10. What could go wrong

| Symptom | Cause |
|---|---|
| Panic "already free" | Double free, or two subsystems tracking the same frame |
| Panic "not a valid physical address" | A virtual address passed without `V2P` |
| Allocation returns overlapping frames | Bitmap indexing wrong — check the shift/mask |
| Free count grows past total | The counter is not recounted after init (Ch. 21, §9) |
| Kernel corrupts itself after a while | `__kernel_phys_*` wrong, so the kernel is being handed out |
| Corruption in the initrd | Modules not reserved (Ch. 11, §5.3) |
| Works then fails at 256 MiB | The direct-map cap; expected |

---

## 11. Exercises

🟢 **22.1** Add a `frames` shell command that prints the first 64 bytes of the bitmap in binary. Run
it before and after allocating ten frames.

🟢 **22.2** Remove the search hint and time a thousand allocations. Then put it back.

🟢 **22.3** Call `pmm_free_frame` twice on the same address and read the panic.

🟡 **22.4** Optimise the scan: compare 32 bits at a time, and use `__builtin_ctz` (which compiles to
`bsf`) to find the first clear bit without looping. Measure the difference on a nearly-full bitmap.

🟡 **22.5** Implement `pmm_alloc_frame_low()` that only returns frames below 16 MiB — which is what
ISA DMA requires, because the ISA bus has 24 address lines. Then explain why that constraint shaped
the design of every memory allocator written between 1984 and 1995.

🟡 **22.6** Add allocation tracking: a parallel array recording the caller's return address
(`__builtin_return_address(0)`) for every allocated frame. Add a `leaks` command that groups by
caller. This is the single most useful debugging tool in Part III.

🔴 **22.7** Replace the bitmap with a free list stored in the free frames themselves: each free frame
holds a pointer to the next. Allocation and free become O(1). Then work out what to do about
`pmm_reserve_region`, which needs to ask whether a specific frame is free.

🔴 **22.8** Implement a buddy allocator: free lists for orders 0 through 10, splitting on allocate
and merging with the buddy on free. Compare fragmentation against the bitmap by allocating and
freeing randomly for a while and then asking for 16 contiguous frames.

---

## What we covered

- A two-function interface, and the layering it enables.
- Why 4 KiB, and the two directions the tradeoff runs.
- One bit per frame at 0.003% overhead, and the bootstrapping problem that forces a fixed-size array
  at the bottom of the kernel.
- Frame numbers rather than addresses, so the arithmetic cannot produce a misaligned result.
- Why the failure sentinel is not zero.
- Linear scan with a full-byte skip and a search hint; why it is still O(n) and what a free list or
  a buddy allocator would change.
- `irq_save` rather than `cli`, because page faults allocate.
- Contiguous allocation, external fragmentation that worsens with uptime, and why DMA pools are
  reserved at boot.
- Panicking on a double free, and the principle: if the invariant is already broken, stop.
- A four-property self-test, and the conservation check that catches leaks.

[Chapter 23](23-paging-theory.md) is theory: what the MMU actually does with an address, what every
bit of a page table entry means, and why the TLB will bite you.

---

[← The memory map](21-memory-map.md) · [Contents](README.md) · [Next: Paging theory →](23-paging-theory.md)
