# Chapter 21 — What memory is there?

[← The console](20-kernel-console.md) · [Contents](README.md) · [Next: The physical memory manager →](22-pmm.md)

---

## Goal

Answer a question the kernel has never had to ask: how much RAM exists, and which parts of it may we
use? The answer is not "all of it starting at zero", and the ways in which it is not are the subject
of this chapter.

This starts Part III, which is where most hobby kernels die. We go slowly.

---

## 1. Why this is a question at all

It would be convenient if physical address space were RAM from 0 to *N*. It is not, for three
reasons.

**Devices live in it.** The VGA framebuffer at `0xA0000`, PCI device windows somewhere above
`0xE0000000`, the local APIC at `0xFEE00000`. Reading those addresses reads a device register, not
memory. Writing them does something.

**Firmware claims parts of it.** ACPI tables, runtime services, SMM (System Management Mode) code
that the operating system is not allowed to see, and on many machines a region reserved for the
integrated graphics.

**Some of it is a hole.** Between 640 KiB and 1 MiB there is no RAM at all, for reasons from 1981.
Above 3 GiB on a 32-bit machine there is usually a hole for PCI address space, which is why a
machine with 4 GiB of RAM shows 3.2 GiB.

So the kernel has to be *told*, and the thing that tells it is the firmware.

---

## 2. The first megabyte, in detail

Chapter 2, §3.3 gave the sketch. Here it is properly, because Part III's allocator has to avoid all
of it.

```
0x00000 - 0x003FF   1 KiB    Interrupt Vector Table (real mode)
0x00400 - 0x004FF   256 B    BIOS Data Area
0x00500 - 0x07BFF   ~30 KiB  free conventional memory
0x07C00 - 0x07DFF   512 B    where a boot sector is loaded
0x07E00 - 0x9FBFF   ~607 KiB free conventional memory
0x9FC00 - 0x9FFFF   1 KiB    Extended BIOS Data Area (usually)
0xA0000 - 0xBFFFF   128 KiB  video memory
0xC0000 - 0xC7FFF   32 KiB   video BIOS ROM
0xC8000 - 0xEFFFF   160 KiB  option ROMs, memory-mapped devices
0xF0000 - 0xFFFFF   64 KiB   system BIOS ROM
```

Three notes.

**The EBDA moves.** Its base is stored as a paragraph number at `0x040E`, and its size varies by
machine. Some firmware puts it at `0x9FC00`, some at `0x9E000`. A kernel that assumes it is not there
can overwrite data the SMM handler still uses — which produces failures with no relationship to
anything you did.

**The 640 KiB line is `0xA0000`.** That is the boundary between conventional memory and the video
window, and it is where "640 KB" comes from. Nobody said it ought to be enough for anybody; it was
the space IBM left below the hardware.

**Everything from `0xA0000` to `0xFFFFF` is device or ROM.** Not RAM. There *is* RAM behind some of
it on modern chipsets — "shadow RAM", used to copy the slow ROM into fast memory — but reaching it
requires chipset-specific configuration and it is not worth 384 KiB.

Our response is blunt:

```c
    pmm_reserve_region(0, 1 * MiB);
```

Some of that is genuinely free RAM, and a real kernel reclaims it — it is where you must put the
trampoline that starts the other CPUs, because they boot in real mode. But the memory map is not
always honest about which is which, and 1 MiB is a cheap price for not having to care.

---

## 3. E820, and where the map comes from

The firmware builds the map during POST and reports it through `int 0x15, AX = 0xE820`. Each call
returns one entry and a continuation value; you loop until the continuation comes back zero.

An entry is 20 or 24 bytes:

```
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t extended_attributes;   /* the optional extra 4 bytes */
```

with types:

| Type | Name | Meaning |
|---|---|---|
| 1 | Available | usable RAM |
| 2 | Reserved | do not touch |
| 3 | ACPI reclaimable | usable *after* the ACPI tables have been read |
| 4 | ACPI NVS | non-volatile storage; must be preserved across sleep |
| 5 | Bad | firmware says this RAM is broken |

Nimbus does not call `int 0x15` — it is a protected-mode kernel and the BIOS is gone (Chapter 7, §4).
Multiboot hands us the same data, from the same source, because the bootloader made the call before
switching modes.

---

## 4. The Multiboot memory map

```c
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} PACKED multiboot_mmap_entry_t;
```

Same fields, one extra: `size`.

### 4.1 The advance that is not `p++`

```c
    entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + 4);
```

`size` is the number of bytes in the entry **not counting the size field itself**, so advancing takes
`size + 4`.

And entries are not required to be the same length. Most bootloaders emit 20-byte bodies, some emit
24 to include the extended attributes, and the specification permits either — even mixed within one
map.

So `entry++` works on QEMU and fails on some firmware, which is the worst kind of bug: it works
everywhere you test and fails on hardware you do not have.

### 4.2 Walking it

```c
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)P2V(mbi->mmap_addr);
    uintptr_t end = (uintptr_t)P2V(mbi->mmap_addr) + mbi->mmap_length;

    while ((uintptr_t)entry < end) {
        const char *kind =
            entry->type == MULTIBOOT_MEMORY_AVAILABLE        ? "available" :
            entry->type == MULTIBOOT_MEMORY_RESERVED         ? "reserved"  :
            entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE ? "acpi"      :
            entry->type == MULTIBOOT_MEMORY_NVS              ? "nvs"       :
                                                               "bad";

        kprintf("  %08x - %08x  %s\n",
                (uint32_t)entry->addr,
                (uint32_t)(entry->addr + entry->len - 1),
                kind);
        ...
        entry = (multiboot_mmap_entry_t *)((uintptr_t)entry + entry->size + 4);
    }
```

`P2V` because `mmap_addr` is physical (Chapter 11, §6.2).

Printing the whole map at boot is worth the eight lines. It is the first thing you will want when a
machine behaves differently from QEMU, and it takes one screen.

---

## 5. What it actually looks like

QEMU with `-m 128M`:

```
physical memory map:
  00000000 - 0009FBFF  available
  0009FC00 - 0009FFFF  reserved
  000F0000 - 000FFFFF  reserved
  00100000 - 07FDFFFF  available
  07FE0000 - 07FFFFFF  reserved
  FFFC0000 - FFFFFFFF  reserved
physical memory: 127 MiB total, 123 MiB free
```

Six entries. Read them:

- `00000000–0009FBFF` — conventional memory, 639 KiB. Note it stops at `0x9FC00`, not `0xA0000`: the
  EBDA is the next entry.
- `0009FC00–0009FFFF` — the EBDA, 1 KiB, reserved.
- `000F0000–000FFFFF` — the BIOS ROM.
- `00100000–07FDFFFF` — the main block, from 1 MiB to just under 128 MiB. This is where everything
  lives.
- `07FE0000–07FFFFFF` — 128 KiB at the top, reserved. This is where SeaBIOS put its ACPI tables.
- `FFFC0000–FFFFFFFF` — the firmware ROM at the top of the 32-bit address space, which is where the
  reset vector points (Chapter 4, §1).

Note what is **not** in the map: `0xA0000–0xEFFFF`, the video memory and option ROM region. It is not
listed as reserved; it is simply absent.

That is why §6's approach matters.

### 5.1 A real machine

An eight-year-old laptop with 8 GiB, booted as a 32-bit kernel:

```
  00000000 - 00057FFF  available
  00058000 - 00058FFF  reserved
  00059000 - 0009DFFF  available
  0009E000 - 0009FFFF  reserved
  00100000 - 1FFFFFFF  available
  20000000 - 201FFFFF  reserved
  20200000 - 40003FFF  available
  40004000 - 40004FFF  reserved
  40005000 - C9EF5FFF  available
  C9EF6000 - CA0BDFFF  acpi
  CA0BE000 - CA31FFFF  nvs
  CA320000 - CFFFFFFF  reserved
  D0000000 - DFFFFFFF  reserved      <- PCI
  FED00000 - FED03FFF  reserved      <- HPET
  FEE00000 - FEE00FFF  reserved      <- local APIC
```

Sixteen entries, four kinds, and holes scattered through what looks like it should be contiguous RAM.
A one-kilobyte reserved region at `0x58000`, in the middle of conventional memory, because the
firmware put something there.

A kernel that assumes "RAM is everything from 1 MiB to `mem_upper`" will hand out `0x20000000` and
whatever is there will stop working.

---

## 6. Start from "all used"

```c
void pmm_init(multiboot_info_t *mbi)
{
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
    total_frames = 0;
    used_frames = 0;
    ...
```

The bitmap starts **all ones**: every frame marked used. Then we free only what the map explicitly
says is available, then re-reserve the parts of that which are already spoken for.

This is the opposite of what feels natural, and it is the important design decision in the chapter.

Starting from "all used" means that any region the memory map failed to mention — a hole, an MMIO
window, a chunk the firmware is hiding — stays unavailable **by default**.

**The failure mode of being too cautious is wasting memory. The failure mode of the opposite is
handing a driver a frame that is really a device register.** Those are not comparable.

The `0xA0000–0xEFFFF` gap in §5 is the concrete case: it is absent from the map, so a
"free everything, then reserve what is listed" approach would hand out the VGA framebuffer.

### 6.1 The rounding rule

```c
void pmm_reserve_region(paddr_t start, size_t length)
{
    uint32_t first = ADDR_TO_FRAME(ALIGN_DOWN(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_UP(start + length, PAGE_SIZE));
    ...
}

static void pmm_free_region(paddr_t start, size_t length)
{
    uint32_t first = ADDR_TO_FRAME(ALIGN_UP(start, PAGE_SIZE));
    uint32_t last  = ADDR_TO_FRAME(ALIGN_DOWN(start + length, PAGE_SIZE));
    ...
}
```

Note that the two round **opposite ways**.

Reserving rounds outwards: a region occupying one byte of a frame claims the whole frame.

Freeing rounds inwards: a region must cover a whole frame before that frame is considered free.

Both err towards "not available", for the reason in §6. Getting either backwards lets the allocator
hand out a frame containing the last hundred bytes of the kernel, and the corruption appears ten
seconds later somewhere unrelated.

---

## 7. What has to be reserved

```c
    pmm_reserve_region(0, 1 * MiB);

    pmm_reserve_region((paddr_t)__kernel_phys_start,
                       (size_t)(__kernel_phys_end - __kernel_phys_start));

    pmm_reserve_region(V2P((uintptr_t)mbi), sizeof(multiboot_info_t));
    if (mbi->flags & MB_INFO_MEM_MAP)
        pmm_reserve_region(mbi->mmap_addr, mbi->mmap_length);

    if (mbi->flags & MB_INFO_MODS) {
        multiboot_module_t *mods = (multiboot_module_t *)P2V(mbi->mods_addr);
        pmm_reserve_region(mbi->mods_addr,
                           mbi->mods_count * sizeof(multiboot_module_t));
        for (uint32_t i = 0; i < mbi->mods_count; i++)
            pmm_reserve_region(mods[i].mod_start,
                               mods[i].mod_end - mods[i].mod_start);
    }
```

Five reservations. Miss any and the frame allocator hands out memory that is in use.

**The first megabyte** — §2.

**The kernel image** — using the *physical* symbols from the linker script:

```ld
    __kernel_end = .;
    __kernel_phys_end = . - KERNEL_VIRTUAL_BASE;
```

These are physical addresses precisely so that this line can be written without a conversion that
might be wrong. The comment in `link.ld` is worth repeating:

> If those two symbols are wrong, the frame allocator will hand out frames containing the running
> kernel, and the failure will look like random corruption minutes later.

Check them once with `readelf -s | grep kernel_phys`.

**The multiboot info struct and the memory map array.** They live in low memory that the map
describes as *available*, and we are reading them while walking that same map. Overwriting them
before the module list has been read is a classic and very confusing bug.

**The modules.** The initrd. Same argument.

---

## 8. The 4 GiB cap, and one that is ours

```c
            if (base < 0x100000000ULL) {
                if (base + len > 0x100000000ULL)
                    len = 0x100000000ULL - base;
                ...
            }
```

Anything above 4 GiB is unreachable on a 32-bit kernel without PAE, and `len` is 64 bits so the
arithmetic would silently wrap if we tried.

PAE — Physical Address Extension — gets you 64 GiB by making page table entries 64 bits and adding a
third level. It was how 32-bit servers used more than 4 GiB for a decade, and it is genuinely
unpleasant: the kernel still only has a 4 GiB *virtual* space, so accessing high memory needs
temporary mappings.

And then there is a limit of our own:

```c
    if ((uint64_t)total_frames * PAGE_SIZE > (uint64_t)DIRECT_MAP_LIMIT) {
        uint32_t usable = ADDR_TO_FRAME(DIRECT_MAP_LIMIT);
        for (uint32_t f = usable; f < total_frames; f++)
            bitmap_set(f);
        LOG_WARN("pmm: ignoring RAM above %u MiB (direct map limit)",
                 DIRECT_MAP_LIMIT / MiB);
    }
```

256 MiB. Chapter 23 explains why: the kernel keeps a direct map of physical memory at `0xC0000000`,
that window is 256 MiB wide, and a frame outside it has no permanent kernel virtual address — so the
page-table code could not write to it.

Rather than half-support such frames, we refuse to hand them out. The honest cost is that a machine
with 4 GiB runs Nimbus in 256 MiB, and the comment says so.

---

## 9. Counting, and a bookkeeping subtlety

```c
    used_frames = 0;
    for (uint32_t f = 0; f < total_frames; f++)
        if (bitmap_test(f)) used_frames++;
```

The running counter is meaningless during `pmm_init`, because "everything is used" initially means
all 4 GiB of frames the bitmap can describe — most of which do not exist.

So we recount from the bitmap once `total_frames` is known. From then on every alloc and free keeps
the counter in step.

It is an unglamorous three lines and it is the difference between a memory report that makes sense
and one that says 1,016,076 of 32,768 frames are in use.

---

## 10. Running it

```
physical memory map:
  00000000 - 0009FBFF  available
  0009FC00 - 0009FFFF  reserved
  000F0000 - 000FFFFF  reserved
  00100000 - 07FDFFFF  available
  07FE0000 - 07FFFFFF  reserved
  FFFC0000 - FFFFFFFF  reserved
physical memory: 127 MiB total, 123 MiB free
```

123 of 127 MiB free. The missing 4 MiB is the first megabyte, the kernel image, and the initrd —
which is exactly what §7 reserved.

### 10.1 Sanity checks

Worth doing once, by hand, the first time:

```c
    kprintf("kernel: %08x - %08x (%u KiB)\n",
            (uint32_t)__kernel_phys_start, (uint32_t)__kernel_phys_end,
            (uint32_t)(__kernel_phys_end - __kernel_phys_start) / 1024);
```

```
kernel: 00100000 - 0010c000 (48 KiB)
```

Compare with the file:

```bash
$ ls -l bin/nimbus.elf
-rwxr-xr-x 1 user user 53240 bin/nimbus.elf
```

The ELF is bigger than the loaded image because of the symbol table and DWARF sections, which are not
loaded. `readelf -l` shows the actual `PT_LOAD` sizes.

And check the arithmetic: 127 MiB = 32,512 frames. 123 MiB free = 31,488. Difference 1024 frames =
4 MiB. 1 MiB reserved below, 48 KiB kernel, ~2 MiB initrd, rounded up — plausible.

Doing this once builds the habit of checking that the numbers are the numbers they should be, which
is the single most useful habit in Part III.

---

## 11. Exercises

🟢 **21.1** Run with `-m 32M`, `-m 128M` and `-m 512M` and compare the maps. Where does the extra
memory appear, and does the number of entries change?

🟢 **21.2** Remove the `pmm_reserve_region(0, 1 * MiB)` and print the first ten frames the allocator
hands out. Then work out what would break.

🟢 **21.3** Print `entry->size` for each entry. Is it 20 or 24 in your QEMU?

🟡 **21.4** Reclaim conventional memory properly: instead of reserving the whole first megabyte,
reserve only `0x00000–0x00FFF` (the IVT and BDA), the EBDA (read its base from `0x040E`), and
`0xA0000–0xFFFFF`. How much do you get back, and what would you use it for?

🟡 **21.5** Change `pmm_free_region` to round outwards like `pmm_reserve_region` does. Boot and see
what happens — you may need `-m 33M` to get a region whose end is not page-aligned.

🟡 **21.6** Handle ACPI-reclaimable memory: treat it as reserved at boot, then add a function that
frees it, and call it from the shell. Confirm the free count rises.

🔴 **21.7** Write the `int 0x15 E820` loop in Spark's stage 2, stash the results at a fixed address,
and have a 32-bit kernel read them. This is what the bootloader did for us, and doing it yourself
shows exactly how much multiboot is saving.

---

## What we covered

- Physical address space is not RAM from 0 to *N*: devices, firmware and holes all live in it.
- The first megabyte in detail, including the EBDA whose base you must read rather than assume.
- E820, its five region types, and the fact that Multiboot is relaying exactly that data.
- The variable-length map entry, and why `entry++` works in QEMU and fails on hardware.
- Two real memory maps, and the absent `0xA0000` region that makes the next point matter.
- Starting from "all used" and freeing only what is listed — and the asymmetry of the two failure
  modes that justifies it.
- Reserve rounds outwards, free rounds inwards, both erring the same way.
- The five things that must be reserved or the allocator hands out live memory.
- The 4 GiB architectural cap and our own 256 MiB direct-map cap, with the reason for each.
- Recounting the bitmap once the size is known, and checking the resulting numbers by hand.

[Chapter 22](22-pmm.md) turns this map into an allocator: one bit per frame, a search hint, and the
two functions everything else in the kernel is built on.

---

[← The console](20-kernel-console.md) · [Contents](README.md) · [Next: The physical memory manager →](22-pmm.md)
