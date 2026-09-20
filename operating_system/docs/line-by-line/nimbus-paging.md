# Line by line: `nimbus/mm/paging.c`

[Index](README.md) · [Chapters 23–26, 28](../23-paging-theory.md)

---

## The direct map

```c
#define DIRECT_MAP_SIZE (256u * MiB)
```
⚠️ Must match `DIRECT_MAP_LIMIT` in `pmm.c`.

The design decision that shapes the file: physical frame `0x1234000` is always readable at virtual
`0xC1234000`, in every address space, for the life of the machine.

Solves the chicken-and-egg: to edit a page table you must write to it, and to write to it you need a
virtual address — but it is a frame the allocator just handed you and nothing maps it yet.

```c
#define V2P(a) ((paddr_t)((uintptr_t)(a) - KERNEL_VIRTUAL_BASE))
#define P2V(a) ((void *)((uintptr_t)(a) + KERNEL_VIRTUAL_BASE))
```
⚠️ **Not general address translation.** They work because the kernel window is a constant offset, and
they are wrong for any page mapped anywhere else. `V2P` on a *user* address is meaningless;
`paging_virt_to_phys` does the real walk.

---

## Storage

```c
static page_directory_t kernel_directory_storage;
```
⚠️ Static, because `paging_init` runs before `heap_init` — the heap needs paging to map its arena.

Only *later* directories come from `kmalloc`. The same bootstrapping shape as the PMM's fixed bitmap.

---

## `alloc_zeroed_frame`

```c
static paddr_t alloc_zeroed_frame(void)
{
    paddr_t frame = pmm_alloc_frame();
    if (frame == PMM_NO_FRAME)
        panic("paging: out of physical memory");

    memset(P2V(frame), 0, PAGE_SIZE);
    return frame;
}
```
⚠️ Zeroed, always. A nonzero entry with `P` set by accident is a mapping to a random physical frame,
and the CPU will happily use it.

The panic is correct *here* — this is called while building a page table and there is no sensible way
to fail halfway — and wrong in general. A real kernel has an out-of-memory killer.

---

## `paging_get_entry`

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
The heart of the file.

`create` as a parameter rather than two functions: `paging_map` passes true; `paging_unmap` and
`paging_virt_to_phys` pass false, because building a table just to discover a page is not mapped
would allocate a frame on every failed lookup.

⚠️ **Permissive directory entries.** Effective permission is the AND of both levels, so an open
directory entry with a tight table entry gives the table entry's answer.

The advantage: we never have to widen a directory entry when one page inside it becomes writable. The
cost: a directory entry tells you nothing about what is inside it.

`P2V` is where the direct map pays for itself — one addition and the table is writable.

---

## `paging_map`

```c
    if (*pte & PTE_PRESENT) {
        LOG_WARN("paging_map: %08x was already mapped to %08x, now %08x", ...);
    }
```
Overwriting a live mapping leaks the frame it pointed at. A warning rather than a panic because
remapping with different permissions is legitimate.

```c
    *pte = (phys & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;
```
⚠️ **Both masks.** An unaligned physical address would set flag bits; a flags word with high bits set
would corrupt the frame address.

```c
    paging_invalidate(virt);
```
⚠️ **Every single mapping change, no exceptions.**

The MMU caches translations and does *not* notice you changed the table. Until this runs, the CPU may
keep using the previous translation — including "not present", which means the mapping you just
created appears not to work.

Forgetting it produces the worst class of bug: **it works most of the time.** The TLB is 64 entries;
if the address happens not to be cached, everything is fine. It becomes wrong under load,
non-deterministically, and never while you are watching.

---

## `paging_map_range`

```c
    vaddr_t start = ALIGN_DOWN(virt, PAGE_SIZE);
    vaddr_t end   = ALIGN_UP(virt + length, PAGE_SIZE);
```
⚠️ Start rounds down, end rounds up. A request for 100 bytes at `0x1FFF` covers **two** pages.
Rounding both the same way maps one and leaves the second byte unmapped.

```c
        if (*pte & PTE_PRESENT) continue;
```
Idempotent, which matters because the heap calls it repeatedly on overlapping ranges as it grows.

---

## `paging_init`

```c
    kernel_directory->phys    = alloc_zeroed_frame();
    kernel_directory->entries = (uint32_t *)P2V(kernel_directory->phys);
```
⚠️ `P2V` on a frame we just allocated — but the direct map does not exist yet. We are still on the
*boot* directory, whose higher-half mapping covers the first 16 MiB.

This works only because the frame allocator hands out low memory first.

> That is a real dependency between two files and it is why `BOOT_PAGES` in `boot.asm` is 4 and not 1.

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
Up to 65,536 entries across 64 page tables — 256 KiB to describe 256 MiB, a 0.1% overhead.

⚠️ **No `PTE_USER` anywhere.** That one missing bit is the entire kernel/user boundary.

4 KiB pages despite the overhead, because this mapping must later be *changed* page by page — to mark
`.rodata` read-only, to unmap a guard page.

```c
    for (vaddr_t v = (vaddr_t)__text_start; v < (vaddr_t)__rodata_end; v += PAGE_SIZE) {
        uint32_t *pte = paging_get_entry(kernel_directory, v, false);
        if (pte && (*pte & PTE_PRESENT))
            *pte &= ~PTE_WRITABLE;
    }
```
Turns "a wild pointer overwrote an instruction" — a crash somewhere else entirely, minutes later —
into a page fault at the offending store with the address in `CR2`.

Works because `link.ld` aligns every section to 4 KiB.

```c
    uint32_t cr0;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x00010000;
    __asm__ volatile ("movl %0, %%cr0" :: "r"(cr0));
```
⚠️ `CR0.WP`, bit 16. Without it ring 0 may write to a read-only page — a 386 compatibility behaviour.

Two things depend on it: the read-only `.text` above means something, and copy-on-write works for
pages the *kernel* touches on a process's behalf. Without it a `read()` into a COW page succeeds
silently and corrupts the other process.

```c
    paging_switch_directory(kernel_directory);
```
⚠️ Works because the new directory maps the kernel at the same addresses the old one did. If it did
not, the instruction after `mov cr3` would fetch from unmapped memory — the classic reboot with no
output at exactly this line.

---

## `paging_switch_directory`

```c
    write_cr3(dir->phys);
```
⚠️ `phys`, not `entries`. `CR3` takes a physical address, and this is the single most common place to
pass the wrong one — which is exactly why the struct carries both.

---

## `paging_new_directory`

```c
    for (uint32_t i = KERNEL_PAGE_NUMBER; i < 1024; i++)
        dir->entries[i] = kernel_directory->entries[i];
```
⚠️ **The entries are copied; the tables are shared.**

Each entry is a *pointer to a page table*. After the loop both directories point at the same physical
frames.

> a change to a kernel mapping made in one address space is visible in all of them, automatically
> [...] Get that wrong — copy the entries instead of sharing the tables — and a kmalloc that happens
> to grow the heap becomes visible only to the process that did it.

It is also what lets a syscall run without reloading `CR3`, and what makes the `mov cr3` mid-function
in `schedule()` safe.

---

## `paging_clone_directory`

```c
    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
```
⚠️ **User half only.** The kernel half came from `paging_new_directory` and is shared.

```c
            dst_table[ti] = new_frame | (src_table[ti] & PTE_FLAGS_MASK);
```
New frame, **old flags**. Copying the address without the flags gives the child a writable copy of
the parent's read-only `.text` — a silent loss of protection rather than a crash.

```c
            memcpy(P2V(new_frame),
                   P2V(src_table[ti] & PTE_FRAME_MASK),
                   PAGE_SIZE);
```
Reads the parent's frame and writes the child's **without either address space being current**. The
direct map earning its keep, and precisely the case recursive page tables could not handle.

---

## `paging_free_directory`

```c
    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
```
⚠️ **User half only, again**, and this one is spectacular if wrong:

> Freeing the kernel tables would unmap the kernel from every other process, since they share them —
> and the machine would die on the next system call anywhere in the system.

One process exits and the entire machine stops, in an unrelated process, in a syscall.

---

## `page_fault_handler`

```c
    uint32_t addr = read_cr2();
```
⚠️ **The very first statement.** `CR2` is volatile — a second fault overwrites it — so it must be read
before anything that could itself fault, which means before any `kprintf`.

```c
    if (user && !present && current_task) {
        vaddr_t bottom = current_task->user_stack_bottom;

        if (addr < bottom && addr + 64 * KiB >= bottom &&
            addr >= USER_STACK_TOP - USER_STACK_SIZE * 8) {
```
Three conditions distinguishing a growing stack from a wild pointer: below the current bottom, within
64 KiB of it, and within an overall limit.

The 64 KiB window exists because a function with a large local array can move `ESP` a long way in one
instruction — which is what `-fstack-clash-protection` exists to probe around.

```c
            paging_map(current_directory, newpage, alloc_zeroed_frame(),
                       PTE_WRITABLE | PTE_USER);
            current_task->user_stack_bottom = newpage;
            return;
        }
```
⚠️ `return` is enough. A page fault is a *fault*, not a trap: the CPU pushed the address of the
faulting instruction, so `iret` re-executes it and the translation now succeeds.

That one property is what makes demand paging possible at all.

```c
    if (addr < PAGE_SIZE)
        kprintf("  (address is in the first page: this is a null pointer dereference)\n");
```
Naming the common case, because an address below `0x1000` covers `p->field` and `array[i]` for any
sane struct or index.

```c
    if (user) {
        task_exit(-11);
    }
    panic("page fault in kernel mode at %08x, eip=%08x", addr, regs->eip);
```
−11 because SIGSEGV is signal 11 on every Unix.

⚠️ A handler that logs and returns without changing anything spins forever at 100%, printing the same
line. It must either fix something or not return.

---

[Index](README.md) · [Chapters 23–26, 28](../23-paging-theory.md)
