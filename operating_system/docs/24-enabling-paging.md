# Chapter 24 — Enabling paging

[← Paging theory](23-paging-theory.md) · [Contents](README.md) · [Next: The higher half →](25-higher-half.md)

> 📖 **Line by line:** [paging.c](line-by-line/nimbus-paging.md)

---

## Goal

Build a real page directory and switch to it. Walk the two levels, allocate tables on demand, map
ranges, and set `CR0.WP` so that the kernel obeys its own permissions.

Paging is already on — [`boot.asm`](../nimbus/boot/boot.asm) enabled it before any C ran — so this
chapter is about replacing the throwaway directory with one we can actually change. Chapter 25
explains the boot directory itself.

---

## 1. What we are replacing

`boot.asm` left us running on a directory made of four 4 MiB pages: 16 MiB identity-mapped (since
removed) and the same 16 MiB at `0xC0000000`.

It got us here and it is wrong in two ways:

**Everything is writable and executable.** One permission for 16 MiB, including `.text` and
`.rodata`.

**It has no page tables at all.** With `PS` set, there is no second level, so nothing finer than
4 MiB can ever be changed. We cannot map one page, cannot unmap a guard page, cannot make `.rodata`
read-only.

So `paging_init` builds a 4 KiB-granular directory and switches to it.

---

## 2. The chicken and egg

To edit a page table you must write to it. To write to it you need a virtual address for it. But the
table is a physical frame the allocator just handed you, and nothing maps it yet.

This is a genuine problem with three standard answers, and
[`paging.c`](../nimbus/mm/paging.c) states all three before picking one:

> **1. Direct map all of physical memory into the kernel half.** Simple, constant-time, and what we
> do. The cost is address space: our kernel half is 1 GiB, we spend 256 MiB of it on the map, and RAM
> beyond that is unusable.
>
> **2. Recursive page tables:** point one directory entry at the directory itself, so the tables
> appear in the address space at a computable address. Elegant, costs 4 MiB of address space and
> nothing else, and genuinely hard to reason about the first ten times.
>
> **3. A temporary mapping:** keep one page of address space reserved, map the frame there, edit it,
> unmap. Works with any amount of RAM, needs a lock, and costs a TLB flush per edit.

### 2.1 The direct map

```c
#define DIRECT_MAP_SIZE (256u * MiB)
```

Physical frame `0x1234000` is always readable at virtual `0xC1234000`, in every address space, for
the life of the machine.

```c
#define V2P(a) ((paddr_t)((uintptr_t)(a) - KERNEL_VIRTUAL_BASE))
#define P2V(a) ((void *)((uintptr_t)(a) + KERNEL_VIRTUAL_BASE))
```

Two macros, one subtraction each.

> These are not general address translation — they work because the kernel window is a simple
> constant offset, and they are wrong for any page mapped anywhere else.

That caveat matters. `V2P` on a *user* address is meaningless; `paging_virt_to_phys` does the real
walk. Using `V2P` where you needed the walk is a bug that produces a plausible-looking wrong number.

### 2.2 The recursive trick, for when you meet it

Worth understanding even though we do not use it, because half the kernels you will read do.

Point directory entry 1023 at the directory's own physical address. Now:

- Virtual `0xFFFFF000`–`0xFFFFFFFF` is the directory itself.
- Virtual `0xFFC00000 + (dir_ix << 12)` is page table `dir_ix`.

The MMU, walking an address in that range, uses the directory as a page table, which lands it back in
the directory. One entry, 4 MiB of address space, and every table in the current address space is
reachable at a computable address.

The catch: it only works for the **current** address space. Editing another process's tables — which
`fork` must do — needs a second recursive slot, or a temporary mapping, or the direct map.

That is exactly why we chose the direct map: `paging_clone_directory` reads the source's tables and
writes the destination's, in one function, with neither being current.

---

## 3. Walking, and building as we go

```c
uint32_t *paging_get_entry(page_directory_t *dir, vaddr_t virt, bool create)
{
    uint32_t di = dir_index(virt);
    uint32_t ti = table_index(virt);

    uint32_t pde = dir->entries[di];

    if (!(pde & PTE_PRESENT)) {
        if (!create) return NULL;

        paddr_t table = alloc_zeroed_frame();

        dir->entries[di] = table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        pde = dir->entries[di];
    }

    uint32_t *table = (uint32_t *)P2V(pde & PTE_FRAME_MASK);
    return &table[ti];
}
```

The heart of the file. Returns a pointer to the page table entry for `virt`, allocating a table if
needed.

**`create` as a parameter** rather than two functions, because almost every caller knows which it
wants and the two versions would differ by four lines. `paging_map` passes true; `paging_unmap` and
`paging_virt_to_phys` pass false, because building a table just to discover a page is not mapped
would allocate a frame on every failed lookup.

**Zeroed, always.** `alloc_zeroed_frame` because a nonzero entry with `P` set by accident is a
mapping to a random physical frame, and the CPU will happily use it.

**The `P2V`** is where the direct map pays for itself. One addition, and the table is writable.

### 3.1 The permissive directory entry

```c
        dir->entries[di] = table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
```

Chapter 23, §4: effective permission is the AND of both levels, so an open directory entry with a
tight table entry gives the table entry's answer.

The advantage is that we never have to widen a directory entry later. The cost is that a directory
entry tells you nothing about what is inside it.

---

## 4. Mapping

```c
void paging_map(page_directory_t *dir, vaddr_t virt, paddr_t phys, uint32_t flags)
{
    uint32_t *pte = paging_get_entry(dir, virt, true);

    if (*pte & PTE_PRESENT) {
        LOG_WARN("paging_map: %08x was already mapped to %08x, now %08x",
                 virt, *pte & PTE_FRAME_MASK, phys);
    }

    *pte = (phys & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;

    paging_invalidate(virt);
}
```

Four lines of work and two lines of care.

**The overwrite warning.** Replacing a live mapping leaks the frame it pointed at and is almost
always a bug in the caller. A warning rather than a panic because there are legitimate cases —
remapping a page with different permissions — but it should be loud enough to notice.

**The masks.** `phys & PTE_FRAME_MASK` and `flags & PTE_FLAGS_MASK`. A caller passing an unaligned
physical address would otherwise set flag bits, and a caller passing a flags word with high bits set
would corrupt the frame address. Both are cheap and both catch real mistakes.

**`paging_invalidate`.** Chapter 23, §5.2. Every single mapping change, no exceptions. The source
says why:

> The MMU caches translations in the TLB and does *not* notice that you changed the table. Until this
> instruction runs, the CPU may keep using the previous translation for this address — including "not
> present", which means the mapping you just created appears not to work.

### 4.1 Ranges

```c
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
```

The workhorse: allocate frames *and* map them. This is what grows a heap, creates a user stack, or
reserves a region.

**Start rounds down, end rounds up.** A request for 100 bytes at `0x1FFF` covers two pages, because
it straddles a boundary. Rounding both the same way would map one page and leave the second byte
unmapped — a fault at a plausible-looking address.

**Already-present pages are skipped,** not overwritten. That makes the function idempotent, which
matters because the heap calls it repeatedly on overlapping ranges as it grows.

---

## 5. `paging_init`

```c
void paging_init(void)
{
    kernel_directory = &kernel_directory_storage;

    kernel_directory->phys    = alloc_zeroed_frame();
    kernel_directory->entries = (uint32_t *)P2V(kernel_directory->phys);
```

### 5.1 A static struct, because the heap does not exist

```c
static page_directory_t kernel_directory_storage;
```

`paging_init` runs before `heap_init` — the heap needs paging to map its arena. So the kernel's
directory descriptor is a static, and only *later* directories come from `kmalloc`:

```c
page_directory_t *paging_new_directory(void)
{
    page_directory_t *dir = (page_directory_t *)kmalloc(sizeof(page_directory_t));
```

The same bootstrapping shape as the PMM's fixed-size bitmap (Chapter 22, §3.1). The bottom of a
kernel is full of these.

### 5.2 A dependency between two files

```c
    kernel_directory->phys    = alloc_zeroed_frame();
    kernel_directory->entries = (uint32_t *)P2V(kernel_directory->phys);
```

`P2V` on a frame we just allocated — but the direct map does not exist yet. We are still running on
the *boot* directory, whose higher-half mapping covers the first 16 MiB.

So this works only because the frame allocator hands out low memory first. The source is explicit:

> That is a real dependency between two files and it is why `BOOT_PAGES` in `boot.asm` is 4 and not 1.

With `BOOT_PAGES = 1`, only 4 MiB is mapped, and on a machine where the kernel and initrd push the
first free frame past 4 MiB, `paging_init` page-faults on its first write. That is a bug that
appears when the initrd grows.

### 5.3 Building the direct map

```c
    size_t ram = pmm_total_frames() * PAGE_SIZE;
    size_t mapped = MIN(ram, DIRECT_MAP_SIZE);

    for (paddr_t phys = 0; phys < mapped; phys += PAGE_SIZE) {
        vaddr_t virt = KERNEL_VIRTUAL_BASE + phys;

        uint32_t flags = PTE_PRESENT | PTE_WRITABLE;

        uint32_t *pte = paging_get_entry(kernel_directory, virt, true);
        *pte = (phys & PTE_FRAME_MASK) | flags;
    }
```

Up to 65,536 entries across 64 page tables — 256 KiB of page tables to describe 256 MiB. A 0.1%
overhead, which is the standard cost of paging.

**No `PTE_USER`, anywhere.** That missing bit is the entire kernel/user boundary (Chapter 23, §3.3).

**4 KiB pages, despite the overhead.** Four 4 MiB pages would describe the same memory with no tables
at all. We use 4 KiB because this is the mapping that must later be *changed* page by page — to mark
`.rodata` read-only, to unmap a guard page, to make a page table's own frame non-writable.

**`paging_get_entry` calls itself into existence.** Each new directory entry allocates a table from
the PMM, and reaching that table uses `P2V` — which works because we are still on the boot directory.
The new directory is fully built before we switch to it.

### 5.4 Read-only text

```c
    for (vaddr_t v = (vaddr_t)__text_start; v < (vaddr_t)__rodata_end; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(kernel_directory, v, false);
        if (pte && (*pte & PTE_PRESENT))
            *pte &= ~PTE_WRITABLE;
    }
```

`.text` and `.rodata` do not need to be writable.

This turns "a wild pointer overwrote an instruction" — which produces a crash somewhere else
entirely, minutes later, in code that is now different from what you compiled — into a page fault at
the offending store, with the address in `CR2`.

It works because [`link.ld`](../nimbus/link.ld) aligns every section to 4 KiB:

```ld
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
```

Page permissions are per-page, so a `.text` sharing a page with `.data` could not have its own.
Chapter 9, §3.4 planted that `ALIGN(4K)` for this moment.

### 5.5 `CR0.WP`

```c
    uint32_t cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x00010000;                       /* bit 16 = WP */
    __asm__ volatile ("movl %0, %%cr0" :: "r"(cr0));
```

Without `WP`, ring 0 may write to a read-only page — a 386 compatibility behaviour (Chapter 23,
§3.2). With it, the kernel obeys its own page permissions.

Two things depend on it:

- The read-only `.text` above means something.
- Copy-on-write works for pages the *kernel* touches on a process's behalf. Without `WP`, a `read()`
  that writes into a COW page would succeed silently, modifying the shared copy and corrupting the
  other process.

That second one is the real reason. It is also why the bit is set here rather than being mentioned as
a nicety.

### 5.6 The switch

```c
    paging_switch_directory(kernel_directory);
```

```c
void paging_switch_directory(page_directory_t *dir)
{
    current_directory = dir;
    write_cr3(dir->phys);
}
```

One instruction, and the machine is now running on our directory.

It works because the new directory maps the kernel at the same addresses the old one did. If it did
not, the instruction after `mov cr3` would fetch from an unmapped address and triple-fault — which is
the classic way to get a reboot with no output at exactly this line.

`dir->phys`, not `dir->entries`. `CR3` takes a **physical** address, and this is the single most
common place to pass the wrong one. That is exactly why the struct carries both:

```c
typedef struct page_directory {
    uint32_t *entries;        /* virtual address: 1024 entries we can write   */
    paddr_t   phys;           /* physical address: what goes into CR3         */
} page_directory_t;
```

> recomputing `V2P()` every time is exactly the kind of thing that works until the day the directory
> is allocated outside the kernel window.

---

## 6. Address spaces

```c
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
```

Entries 0–767 empty; entries 768–1023 copied from the kernel directory.

### 6.1 Shared, not copied

The loop copies **the entries**, which are pointers to page tables. Both directories then point at
the *same* tables.

That is the important bit, and the source spells out why:

> a change to a kernel mapping made in one address space is visible in all of them, automatically,
> because they share the tables rather than copies of them. Get that wrong — copy the entries
> instead of sharing the tables — and a kmalloc that happens to grow the heap becomes visible only to
> the process that did it.

Picture it: process A calls `write()`, the kernel's heap grows, `paging_map_range` adds entries to a
kernel page table. If B had its own copy of that table, B's next syscall would fault on the new heap
memory — and only sometimes, depending on which process happened to grow the heap.

Sharing the tables makes it automatic. It is also what lets a syscall run without reloading `CR3`.

---

## 7. Running it

```
[    0.000] inf  paging: direct map 128 MiB at c0000000, kernel text read-only
```

### 7.1 Checking the map by hand

```c
    kprintf("kernel_directory phys = %08x\n", kernel_directory->phys);
    kprintf("V2P(0xC0100000)  = %08x  (expect 00100000)\n", V2P(0xC0100000));
    kprintf("walk(0xC0100000) = %08x\n",
            paging_virt_to_phys(kernel_directory, 0xC0100000));
    kprintf("walk(0xC00B8000) = %08x  (expect 000b8000)\n",
            paging_virt_to_phys(kernel_directory, 0xC00B8000));
    kprintf("walk(0x00100000) = %08x  (expect ffffffff, unmapped)\n",
            paging_virt_to_phys(kernel_directory, 0x00100000));
```

```
kernel_directory phys = 00500000
V2P(0xC0100000)  = 00100000  (expect 00100000)
walk(0xC0100000) = 00100000
walk(0xC00B8000) = 000b8000  (expect 000b8000)
walk(0x00100000) = ffffffff  (expect ffffffff, unmapped)
```

The macro and the walk agree for kernel addresses. The identity mapping is gone. That last line is
the proof that a null-ish pointer will fault rather than reading the IVT.

### 7.2 In the QEMU monitor

```
(qemu) info tlb
c0000000: 0000000000000000 -------UWP
c0001000: 0000000000001000 -------UWP
...
c0100000: 0000000000100000 -------U-P      <- read-only!
```

`info tlb` dumps the current mappings. `W` present means writable; its absence on `c0100000` is the
read-only `.text` from §5.4.

Thirty seconds, and it confirms the whole chapter.

### 7.3 Proving `WP` works

```c
    volatile uint32_t *code = (volatile uint32_t *)kmain;
    *code = 0xDEADBEEF;
```

```
PAGE FAULT at c01013f0  (eip=c010157a)
  protection violation, write, ring 0
  ...
*** KERNEL PANIC ***
page fault in kernel mode at c01013f0, eip=c010157a
```

Before `CR0.WP`, that store would have succeeded and corrupted `kmain`. Try it with the `WP` line
commented out and watch the machine behave strangely instead of stopping.

---

## 8. What could go wrong

| Symptom | Cause |
|---|---|
| Triple fault on `mov cr3` | The new directory does not map the kernel at its current addresses |
| Triple fault in `paging_init` | `P2V` on a frame above `BOOT_PAGES × 4 MiB` |
| Mapping "does not work" | Missing `invlpg` |
| Works sometimes | Also missing `invlpg` — the TLB happened not to hold it |
| Page fault writing `.text` | Expected, after §5.4 — that is the feature |
| Kernel changes invisible to a process | Kernel page tables copied instead of shared |
| `CR3` faults immediately | Passed `entries` instead of `phys` |
| Random corruption after fork | `V2P` used where `paging_virt_to_phys` was needed |

---

## 9. Exercises

🟢 **24.1** Remove `paging_invalidate` from `paging_map` and map something you have just unmapped.
How many attempts before it misbehaves?

🟢 **24.2** Comment out the `CR0.WP` line and run the `.text` write from §7.3. Then disassemble
`kmain` in the monitor and see the corruption.

🟢 **24.3** Print the number of page tables allocated by `paging_init`. Confirm it matches
256 MiB / 4 MiB.

🟡 **24.4** Change `DIRECT_MAP_SIZE` to 16 MiB and run with `-m 128M`. What breaks, and where?

🟡 **24.5** Make `paging_map` panic instead of warning on an overwrite, then boot. If it fires, you
have found a real bug; if not, argue about which is the better default.

🟡 **24.6** Set `BOOT_PAGES` to 1 in `boot.asm` and grow the initrd until `paging_init` faults.
Confirm the mechanism in §5.2.

🔴 **24.7** Implement recursive page tables alongside the direct map: point entry 1023 at the
directory, and write `paging_get_entry_recursive`. Confirm both give the same answers, then work out
what `paging_clone_directory` would have to do with only the recursive version available.

🔴 **24.8** Map `.rodata` read-only *and* non-executable. You cannot — plain 32-bit paging has no NX
bit. Write down what enabling PAE would involve: 64-bit entries, a third level, and every `uint32_t *`
in this file becoming `uint64_t *`.

---

## What we covered

- What the boot directory is and why 4 MiB pages make it unchangeable.
- The chicken-and-egg of editing a table you cannot address, and the three standard answers.
- The direct map, `V2P`/`P2V`, and the caveat that they are not general translation.
- Recursive page tables in enough detail to recognise them, and the reason they are not enough for
  `fork`.
- `paging_get_entry` as the heart of the file: create-on-demand, zeroed frames, permissive directory
  entries.
- Masks on both operands of a mapping, and a warning on overwrite.
- Range mapping that rounds outwards at both ends and is idempotent.
- A static directory struct because the heap needs paging, and a cross-file dependency on
  `BOOT_PAGES`.
- 4 KiB pages for a mapping that must later change, and read-only `.text` that only means something
  because of `CR0.WP`.
- Sharing kernel page tables between address spaces, and the bug you get from copying them.
- Three ways to verify: a hand walk, `info tlb`, and a deliberate fault.

[Chapter 25](25-higher-half.md) goes back to `boot.asm` and explains the twenty hardest lines in the
kernel: how a kernel gets from the address it was loaded at to the address it was linked for.

---

[← Paging theory](23-paging-theory.md) · [Contents](README.md) · [Next: The higher half →](25-higher-half.md)
