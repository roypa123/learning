# Line by line: `nimbus/mm/pmm.c`

[Index](README.md) · [Chapter 21](../21-memory-map.md) · [Chapter 22](../22-pmm.md)

One bit per 4 KiB frame.

---

## Sizing

```c
#define MAX_FRAMES      (4u * 1024 * 1024 * 1024 / PAGE_SIZE)   /* 1,048,576 */
#define BITMAP_BYTES    (MAX_FRAMES / 8)                        /* 131,072   */

static uint8_t  frame_bitmap[BITMAP_BYTES];
```
128 KiB of `.bss`, sized for 4 GiB regardless of how much RAM exists. 0.003% overhead.

⚠️ Fixed rather than dynamic because a dynamically sized bitmap needs somewhere to live, and the only
allocator that could provide it is this one.

> Bootstrapping problems are why the lowest layer of a kernel is usually the one with a fixed-size
> array in it.

```c
#define DIRECT_MAP_LIMIT (256u * MiB)
```
⚠️ **Must match `DIRECT_MAP_SIZE` in `paging.c`.** Two constants that have to agree, living apart
because `pmm.c` must not depend on `paging.c`.

---

## Bit operations

```c
static inline void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame >> 3] |= (uint8_t)(1 << (frame & 7));
}
```
`frame >> 3` is the byte index, `frame & 7` the bit within it. The compiler would generate the same
code from `/8` and `%8`; the shift form matches how you think about it.

---

## `pmm_reserve_region` / `pmm_free_region`

```c
void pmm_reserve_region(paddr_t start, size_t length)
{
    uint32_t first = ADDR_TO_FRAME(ALIGN_DOWN(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_UP(start + length, PAGE_SIZE));
```

```c
static void pmm_free_region(paddr_t start, size_t length)
{
    uint32_t first = ADDR_TO_FRAME(ALIGN_UP(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_DOWN(start + length, PAGE_SIZE));
```
⚠️ **They round opposite ways.**

Reserving rounds **outwards**: a region occupying one byte of a frame claims the whole frame.

Freeing rounds **inwards**: a region must cover a whole frame before it is considered free.

Both err towards "not available". Getting either backwards lets the allocator hand out the frame
containing the last hundred bytes of the kernel, and the corruption appears ten seconds later
somewhere unrelated.

---

## `pmm_init`

```c
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
```
⚠️ **All ones — everything used.** Then free only what the map explicitly lists, then re-reserve what
is spoken for.

This is the opposite of what feels natural and it is the important decision in the file. Any region
the memory map fails to mention — a hole, an MMIO window, RAM the firmware is hiding — stays
unavailable by default.

The `0xA0000`–`0xEFFFF` video/ROM region is absent from QEMU's map entirely. "Free everything, then
reserve what is listed" would hand out the framebuffer.

> The failure mode of being too cautious is wasting memory. The failure mode of the opposite is
> handing a driver a frame that is really a device register.

```c
    if (!(mbi->flags & MB_INFO_MEM_MAP))
        panic("pmm: the bootloader gave us no memory map (flags=%08x)\n"
              "     Nimbus cannot guess how much RAM exists.", mbi->flags);
```
Flags checked before any field is read. Chapter 11, §5.1 — reading a field whose bit is clear gives
whatever was in that memory.

The flags are printed, which makes the panic actionable.

```c
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)P2V(mbi->mmap_addr);
```
⚠️ `mmap_addr` is **physical**. Dereferencing it directly is a page fault.

```c
        entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + 4);
```
⚠️ **Not `entry++`.** `size` counts the bytes *after* the size field, and entries are not required to
be the same length — most bootloaders emit 20-byte bodies, some 24.

`entry++` works in QEMU and fails on firmware you do not have.

```c
            if (base < 0x100000000ULL) {
                if (base + len > 0x100000000ULL)
                    len = 0x100000000ULL - base;
```
Anything above 4 GiB is unreachable without PAE, and `len` is 64 bits so the arithmetic would
silently wrap.

---

## The reservations

```c
    pmm_reserve_region(0, 1 * MiB);
```
The IVT, the BIOS data area, the EBDA, video memory, option ROMs, the BIOS. Some of it is genuinely
free RAM — it is where an SMP trampoline must go, because other cores boot in real mode — and the map
is not always honest about which is which.

```c
    pmm_reserve_region((paddr_t)__kernel_phys_start,
                       (size_t)(__kernel_phys_end - __kernel_phys_start));
```
⚠️ **Physical** symbols from `link.ld`, so no conversion that might be wrong. If they are wrong, the
allocator hands out frames containing the running kernel.

Check once with `readelf -s | grep kernel_phys`.

```c
    pmm_reserve_region(V2P((uintptr_t)mbi), sizeof(multiboot_info_t));
    if (mbi->flags & MB_INFO_MEM_MAP)
        pmm_reserve_region(mbi->mmap_addr, mbi->mmap_length);
```
⚠️ The multiboot structures live in low memory the map describes as *available* — and we are reading
them while walking that same map.

```c
    if (mbi->flags & MB_INFO_MODS) {
        multiboot_module_t *mods = (multiboot_module_t *)P2V(mbi->mods_addr);
        pmm_reserve_region(mbi->mods_addr,
                           mbi->mods_count * sizeof(multiboot_module_t));
        for (uint32_t i = 0; i < mbi->mods_count; i++)
            pmm_reserve_region(mods[i].mod_start,
                               mods[i].mod_end - mods[i].mod_start);
    }
```
⚠️ Both the module *array* and each module's *contents*. Miss the second and the initrd's files
become garbage a few seconds after boot.

Five reservations. Missing any one is corruption far from the cause.

```c
    if ((uint64_t)total_frames * PAGE_SIZE > (uint64_t)DIRECT_MAP_LIMIT) {
        uint32_t usable = ADDR_TO_FRAME(DIRECT_MAP_LIMIT);
        for (uint32_t f = usable; f < total_frames; f++)
            bitmap_set(f);
        LOG_WARN("pmm: ignoring RAM above %u MiB (direct map limit)", ...);
    }
```
A frame above the direct map has no kernel virtual address, so `P2V` on it would write page table
entries into unmapped memory.

Rather than half-support such frames, we refuse to hand them out — and say so.

```c
    used_frames = 0;
    for (uint32_t f = 0; f < total_frames; f++)
        if (bitmap_test(f)) used_frames++;
```
⚠️ The running counter is meaningless during init, because "everything used" initially means all
4 GiB of frames the bitmap can describe, most of which do not exist.

Recount once `total_frames` is known. Three lines, and the difference between a sensible report and
one saying 1,016,076 of 32,768 frames are in use.

---

## `pmm_alloc_frame`

```c
    uint32_t flags = irq_save();
```
⚠️ Callable from interrupt context — the page fault handler allocates.

`irq_save` rather than `cli`, because the caller might already have had interrupts off.

```c
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? search_hint : 0;
        uint32_t stop  = (pass == 0) ? total_frames : search_hint;
```
Two passes: from the hint to the end, then from the beginning to the hint.

```c
            if (frame_bitmap[byte] == 0xFF) continue;
```
Eight frames in one comparison. On a nearly-full system this is the difference between scanning a
megabyte of bitmap and 128 KiB.

```c
                search_hint = frame + 1;
```
Without the hint, allocate-allocate-allocate rescans the same full region every time — O(n²) for n
allocations.

```c
    return PMM_NO_FRAME;
```
`0xFFFFFFFF`, not 0.

⚠️ Zero would be a bad sentinel: **physical address 0 is a real, valid frame** holding the interrupt
vector table. A kernel that treats 0 as failure will one day hand it out and then refuse to free it.

`0xFFFFFFFF` cannot collide with a real return value because it is not page-aligned.

---

## `pmm_free_frame`

```c
    if (frame >= MAX_FRAMES)
        panic("pmm_free_frame: %08x is not a valid physical address", addr);
```
Almost always a *virtual* address that should have gone through `V2P`. The panic naming the value
makes it a thirty-second fix.

```c
    if (!bitmap_test(frame))
        panic("pmm_free_frame: frame %u (%08x) was already free", frame, addr);
```
⚠️ A double free means the frame is now free twice. Two subsystems will be handed the same memory,
and the corruption gets blamed on whichever is unluckier.

There is no recovery — the allocator's state is already wrong — so the only question is whether you
find out here or in half an hour somewhere unrelated.

```c
    if (frame < search_hint) search_hint = frame;
```
Pull the hint back so a freed frame is reused promptly rather than after a full wrap.

---

## `pmm_alloc_frames`

```c
    for (uint32_t frame = 0; frame < total_frames; frame++) {
        if (bitmap_test(frame)) { run_len = 0; continue; }
```
Contiguous allocation, for DMA buffers — a device follows physical addresses and knows nothing about
page tables.

No hint: a run could start anywhere.

⚠️ Gets **worse the longer the system runs**. There may be 60 MiB free and no four consecutive
frames. That is external fragmentation, and it is why serious kernels reserve a DMA pool at boot when
memory is still contiguous.

---

[Index](README.md) · [Chapter 21](../21-memory-map.md) · [Chapter 22](../22-pmm.md)
