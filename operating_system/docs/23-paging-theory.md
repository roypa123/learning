# Chapter 23 — Paging theory

[← The PMM](22-pmm.md) · [Contents](README.md) · [Next: Enabling paging →](24-enabling-paging.md)

---

## Goal

Understand what the MMU does with an address before we make it do it. Two levels, ten bits each, a
twelve-bit offset, and about a dozen flag bits — plus the cache that will bite you.

No code runs in this chapter. Chapter 24 turns it on.

---

## 1. The idea

Every address your program uses is fake.

When a program loads from `0x08049000`, the CPU hands that number to the memory management unit,
which walks a tree of tables the kernel built, finds a real physical address, and fetches from there
instead.

That one indirection buys an implausible number of things at once:

**Isolation.** Two processes both use address `0x08049000` and get different memory, because their
page tables differ. Neither can reach the other's memory, because there is no number either could
write down that would get there.

**The illusion of a large machine.** A program is linked as though it owns 4 GiB starting at a fixed
address, on a machine with 128 MiB and eleven other programs running.

**Permissions per page.** Code read-only, data non-executable, kernel pages invisible to userland. A
bug that scribbles over a function pointer faults instead of succeeding.

**Tricks.** Copy-on-write makes `fork()` nearly free. Demand paging means a 200 MB binary starts
instantly. Memory-mapped files make a file look like an array. Guard pages turn a stack overflow into
a clean fault.

All of it from one lookup.

---

## 2. Two levels

```
 virtual address  31          22 21          12 11              0
                 +--------------+--------------+----------------+
                 | directory ix |   table ix   |     offset     |
                 +--------------+--------------+----------------+
                       10 bits       10 bits        12 bits
```

```
    CR3 -> page directory (1024 entries, one 4 KiB frame)
             entry[dir_ix] -> page table (1024 entries, one 4 KiB frame)
                                entry[tbl_ix] -> physical frame
                                                   + offset
```

1024 × 1024 × 4096 = exactly 4 GiB.

That is not a coincidence. The sizes were chosen so that a directory is exactly one page, a table is
exactly one page, and the three fields tile the whole address space with nothing left over. 12 bits
of offset because a page is 4 KiB; 20 bits left; split 10/10 because each table then holds 1024
four-byte entries, which is 4096 bytes, which is one page.

The design is unusually tidy for x86.

### 2.1 Why two levels and not one

A single-level table mapping all 4 GiB would need 1,048,576 entries at four bytes each — **4 MiB per
process**, allocated up front, whether or not the process uses any of it.

With two levels, a process that uses 8 MiB of address space needs: one directory (4 KiB) plus two
page tables (8 KiB) = 12 KiB. The other 1022 directory entries are marked not-present and cost
nothing.

The cost is an extra memory access per translation — which is what the TLB (§5) exists to eliminate.

### 2.2 Doing the walk by hand

Virtual address `0xC0103F00`:

```
    0xC0103F00 = 1100 0000 0001 0000 0011 1111 0000 0000

    directory index = bits 31-22 = 1100000000       = 768
    table index     = bits 21-12 = 0100000011 -> wait, recount:

    0xC0103F00 >> 22          = 0x300  = 768
    (0xC0103F00 >> 12) & 0x3FF = 0x103 = 259
    0xC0103F00 & 0xFFF         = 0xF00 = 3840
```

So: directory entry 768, table entry 259, offset 3840.

```c
static inline uint32_t dir_index(vaddr_t v)   { return v >> 22; }
static inline uint32_t table_index(vaddr_t v) { return (v >> 12) & 0x3FF; }
```

Directory entry 768 is `0xC0000000 >> 22`, which is why
[`paging.h`](../nimbus/include/nimbus/paging.h) defines:

```c
#define KERNEL_VIRTUAL_BASE 0xC0000000u
#define KERNEL_PAGE_NUMBER  (KERNEL_VIRTUAL_BASE >> 22)   /* = 768 */
```

Entries 768–1023 are the kernel's quarter of the address space: 256 entries × 4 MiB = 1 GiB.

---

## 3. An entry, bit by bit

Directory entries and table entries have the same format, with one exception (`PS`).

```
 31                                    12 11  9 8 7 6 5 4 3 2 1 0
+----------------------------------------+-----+-+-+-+-+-+-+-+-+-+
|        physical frame address          | AVL |G|S|D|A|C|W|U|R|P|
+----------------------------------------+-----+-+-+-+-+-+-+-+-+-+
```

```c
#define PTE_PRESENT     0x001u
#define PTE_WRITABLE    0x002u
#define PTE_USER        0x004u
#define PTE_WRITETHROUGH 0x008u
#define PTE_NOCACHE     0x010u
#define PTE_ACCESSED    0x020u
#define PTE_DIRTY       0x040u
#define PTE_PAGE_SIZE   0x080u
#define PTE_GLOBAL      0x100u
#define PTE_COW         0x200u          /* ours, software-only */
```

**The top 20 bits are the frame address.** A frame is 4 KiB aligned, so its low 12 bits are always
zero — which is exactly why there is room for flags. This is the same trick as `CR3`, and it is why
page tables must be page-aligned: their addresses would otherwise not fit.

### 3.1 Bit 0 — `PRESENT`

The whole mechanism. Zero means "fault on any access", which is the basis of:

- unmapped memory (a genuine error)
- demand paging (map it in the handler and retry)
- swapping (the other 31 bits can hold a disk location, since the CPU ignores them when P is clear)
- guard pages

An entry with `P = 0` is entirely software-defined apart from that one bit. That is a deliberate and
very useful piece of architecture.

### 3.2 Bit 1 — `WRITABLE`

Clear means read-only. A write faults.

With one wrinkle: **in ring 0, this bit is ignored unless `CR0.WP` is set.** That is a 386-era
compatibility behaviour, and it means a kernel that does not set `WP` can write to its own read-only
pages without noticing.

[`paging.c`](../nimbus/mm/paging.c) sets it:

```c
    cr0 |= 0x00010000;                       /* bit 16 = WP */
```

and Chapter 24, §5 explains what it buys: read-only `.text` that actually is, and copy-on-write that
works for pages the kernel touches on a process's behalf.

### 3.3 Bit 2 — `USER`

**This bit is the wall.**

Clear means only ring 0 may touch the page. Set means ring 3 may.

Every kernel page has it clear. That single bit is what stops a user program reading kernel memory —
not segmentation (Chapter 15, §3.2), which describes a flat 4 GiB for everyone.

```c
        uint32_t flags = PTE_PRESENT | PTE_WRITABLE;
        /*  No PTE_USER anywhere in the kernel half. This one missing bit is
         *  the entire boundary between a user process and the kernel: with it
         *  set, any program could read /etc/shadow out of the buffer cache. */
```

### 3.4 Bits 3–4 — caching

`PWT` (write-through) and `PCD` (cache disable).

`PCD` matters for MMIO. A device register the CPU has cached is a register you read once and never
see change again, and a write that sits in a write-back cache is a command the device never receives.

Any page mapping device memory needs `PTE_NOCACHE`. The VGA framebuffer is an exception — it is RAM
on the card and caching it is both safe and much faster.

### 3.5 Bits 5–6 — `ACCESSED` and `DIRTY`

**The CPU sets these; software clears them.**

`A` is set on any access. `D` is set on a write (table entries only).

They are the hardware support for page replacement. When memory runs out and you must choose a page
to evict, `A` tells you whether it has been used recently, and `D` tells you whether it must be
written back to disk or can simply be dropped.

The classic algorithm is the **clock** (or second-chance): walk the pages in a circle; if `A` is set,
clear it and move on; if `A` is clear, evict. It approximates least-recently-used with one bit and no
bookkeeping.

Nimbus has no swap, so we clear these and ignore them. They are the mechanism you would build on.

### 3.6 Bit 7 — `PS`, and the one difference between the two levels

In a **directory entry**, `PS = 1` means this entry maps a single 4 MiB page directly, with no page
table. The top 10 bits of the entry are the frame address and bits 12–21 of the virtual address
become part of the offset.

In a **table entry**, bit 7 is `PAT`, a cache-type selector. Different meaning, same bit.

4 MiB pages need `CR4.PSE`, which every CPU since the Pentium has.
[`boot.asm`](../nimbus/boot/boot.asm) uses them for exactly this reason:

```nasm
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep
```

`0x83` is `PS | WRITABLE | PRESENT`. Four entries, 16 MiB mapped, no page tables assembled by hand.

### 3.7 Bit 8 — `GLOBAL`

A global page survives a `CR3` reload — its TLB entry is not flushed.

That is a real optimisation for the kernel's own mappings, which are identical in every address
space: a context switch reloads `CR3` and would otherwise throw away every kernel translation,
guaranteeing a TLB miss on the first kernel instruction after every switch.

It needs `CR4.PGE`, and it needs care: a global entry can only be flushed with `invlpg` or by
clearing `PGE`, so a mistake is persistent.

Nimbus does not use it. Exercise 23.6.

### 3.8 Bits 9–11 — ours

Ignored by the hardware and free for software. We use one:

```c
#define PTE_COW         0x200u
```

to mark a page as copy-on-write (Chapter 34). Linux uses these three bits for a similar set of
software flags.

Three bits is not many. Real kernels keep most per-page state in a separate array indexed by frame
number — Linux's `struct page`, one per physical frame, about 64 bytes each.

---

## 4. Effective permissions are the AND

For a two-level walk, the permission checks are:

```
    effective_writable = directory.W AND table.W
    effective_user     = directory.U AND table.U
```

Both levels must permit the access.

That gives two design options and [`paging.c`](../nimbus/mm/paging.c) picks one:

```c
        dir->entries[di] = table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
```

**Permissive directory entries, enforcement in the table.** The advantage is that we never have to go
back and widen a directory entry when one page inside it becomes writable or user-accessible.

The cost, and the source says it:

> The cost is that a directory entry tells you nothing about what is inside it. Some kernels tighten
> the directory entry instead and gain a cheap "is any page in this 4 MiB user-accessible" check.

Either is defensible. Picking one and being consistent is what matters, because mixing them produces
mappings that are silently narrower than intended.

---

## 5. The TLB, and the bug it causes

Walking two levels means two extra memory reads per access. That would triple the cost of every load
and store, so the CPU caches translations in the **Translation Lookaside Buffer**.

A typical TLB holds 64 entries for 4 KiB pages, sometimes split into instruction and data TLBs, with
a second level of a few hundred entries. Hit rates above 99% are normal, which is why paging is
nearly free.

### 5.1 The rule

**The MMU does not notice that you changed a page table.**

Modify an entry and the CPU may keep using the cached translation — including a cached "not present",
which means the mapping you just created appears not to work.

There is no coherence mechanism. Software must invalidate.

```c
static ALWAYS_INLINE void paging_invalidate(vaddr_t virt)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}
```

One page. And the blunt version:

```c
void paging_flush_tlb(void)
{
    write_cr3(read_cr3());
}
```

Reloading `CR3` flushes every non-global entry.

### 5.2 Every mapping change

```c
void paging_map(page_directory_t *dir, vaddr_t virt, paddr_t phys, uint32_t flags)
{
    ...
    *pte = (phys & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;

    paging_invalidate(virt);
}
```

Every single one. `paging_unmap` too, and `paging_free_range`, and the loop in `elf_load` that
tightens permissions after copying.

Forgetting it produces the worst class of bug in this book: **it works most of the time.** The TLB is
64 entries; if the address you changed happens not to be cached, everything is fine. It becomes wrong
only when the address was recently used — which is to say, under load, non-deterministically, and
never while you are watching.

### 5.3 `invlpg` versus reloading `CR3`

`invlpg` is the scalpel: one entry, a few cycles.

`CR3` is the sledgehammer: everything, and the next few thousand memory accesses all miss.

Use `invlpg` for a single mapping change. Use `CR3` when you have changed many — as
[`boot.asm`](../nimbus/boot/boot.asm) does after zeroing four directory entries covering 16 MiB:

```nasm
    mov ecx, cr3
    mov cr3, ecx
```

There is a crossover point, somewhere around a dozen entries, where the full flush is cheaper than
the individual invalidations. Nobody measures it; both are correct.

### 5.4 The shootdown problem

On a multiprocessor, invalidating on *this* CPU does nothing about the same stale entry cached on
another. The kernel must send an inter-processor interrupt to every CPU that might have it, and wait
for them all to acknowledge.

That is a **TLB shootdown**, it costs microseconds, and it is one of the main reasons unmapping
memory is expensive on a large machine. It is also why `munmap` is slower than `mmap`.

Chapter 48. On one CPU, `invlpg` is the whole story.

---

## 6. What a page fault tells you

Vector 14, and two pieces of evidence.

**`CR2`** holds the virtual address that was accessed. Set by the CPU, and **volatile** — a second
fault overwrites it. So it must be read before anything that could itself fault, which in practice
means before any `kprintf`:

```c
void page_fault_handler(registers_t *regs)
{
    uint32_t addr = read_cr2();
    ...
```

**The error code** is a bitmask, and it is *not* the same layout as a selector error (Chapter 16,
§8.2):

| Bit | 0 | 1 |
|---|---|---|
| 0 | the page was not present | a protection violation |
| 1 | it was a read | it was a write |
| 2 | the CPU was in ring 0 | ring 3 |
| 3 | — | a reserved bit was set in a table entry |
| 4 | — | the fault was an instruction fetch |

```c
    bool present   = (regs->err_code & 0x1) != 0;
    bool write     = (regs->err_code & 0x2) != 0;
    bool user      = (regs->err_code & 0x4) != 0;
    bool reserved  = (regs->err_code & 0x8) != 0;
    bool fetch     = (regs->err_code & 0x10) != 0;
```

Bit 3 is worth knowing about: it means you wrote a 1 into a bit the architecture says must be zero,
which on a plain 32-bit entry means bits you did not think you were setting. It almost always means a
corrupted page table rather than a mapping mistake.

**A page fault is not necessarily an error.** In a mature kernel most faults are expected: demand
paging, stack growth, copy-on-write, memory-mapped files. The handler's job is to tell those apart
from the genuine mistakes, and Chapter 26 is entirely about that.

---

## 7. What the other levels look like

For context, because you will meet them.

**PAE** (Physical Address Extension) makes entries 64 bits and adds a third level, giving 36-bit
physical addresses — 64 GiB. The virtual space is still 4 GiB, so a kernel using it needs temporary
mappings to reach high memory. It was how 32-bit servers coped from 1999 to about 2008.

**x86-64 long mode** has four levels: PML4, PDPT, PD, PT, nine bits each, plus a 12-bit offset — 48
bits of virtual address, or 256 TiB. Newer CPUs add a fifth level for 57 bits.

The structure is identical in kind. More levels, wider entries, the same flags in the same positions
plus `NX` (no-execute) at bit 63. Everything in this chapter transfers.

---

## 8. Exercises

🟢 **23.1** Decompose `0x08049ABC`, `0xC0000000` and `0xFFFFF000` into directory index, table index
and offset.

🟢 **23.2** How much memory do the page tables for a process using 100 MiB of contiguous address
space occupy? What if the 100 MiB is scattered in 4 KiB pieces across the whole 3 GiB?

🟢 **23.3** An entry reads `0x0010F027`. What frame does it point at, and what are its permissions?

🟡 **23.4** Work out what happens if a directory entry is user-accessible but the table entry is not,
and vice versa. Then find the line in `paging_get_entry` that makes one of those cases impossible.

🟡 **23.5** Write the page fault error code decoder from §6 as a function, and print a sentence
rather than a bitmask. Compare with the one in `page_fault_handler`.

🟡 **23.6** Enable `CR4.PGE` and mark the kernel's direct map global. Measure the effect on a
syscall-heavy workload once Part IV exists — you will need `rdtsc` (Exercise 18.8).

🔴 **23.7** Implement the clock page-replacement algorithm on paper, then write the code that walks
the directory clearing `A` bits and counting how many were set. Run it every second and watch which
parts of the address space are hot.

🔴 **23.8** Read Intel SDM Volume 3, Chapter 4, sections 4.3 ("32-Bit Paging") and 4.10 ("Caching
Translation Information"). It is about thirty pages and it is the authoritative version of this
chapter.

---

## What we covered

- One indirection, and the five entirely different things it buys.
- Two levels of ten bits, why the numbers tile 4 GiB exactly, and why one level would cost 4 MiB per
  process.
- Every flag bit: the one that is the whole mechanism, the one that is the wall, the two the CPU
  writes for you, the one that means different things at different levels, and the three that are
  yours.
- `CR0.WP`, and the 386 compatibility behaviour that makes read-only kernel pages meaningless
  without it.
- Effective permissions as the AND of both levels, and the two consistent designs you can pick from.
- The TLB: why paging is nearly free, why the MMU does not notice your edits, and why forgetting
  `invlpg` produces a bug that works most of the time.
- `invlpg` versus `CR3`, and the shootdown that makes this hard on a multiprocessor.
- `CR2` as volatile evidence, and a five-bit error code that is not the one from Chapter 16.
- Where PAE and x86-64 differ, which is in size and not in kind.

[Chapter 24](24-enabling-paging.md) turns it on — and covers the instruction after which every
address in the machine is a lie.

---

[← The PMM](22-pmm.md) · [Contents](README.md) · [Next: Enabling paging →](24-enabling-paging.md)
