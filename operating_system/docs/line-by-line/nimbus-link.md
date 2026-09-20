# Line by line: `nimbus/link.ld`

[Index](README.md) · [Chapter 25](../25-higher-half.md)

A kernel that is loaded low and runs high.

---

## The two addresses

Two addresses matter for every byte of a kernel, and for most programs they are the same number so
nobody ever learns the difference:

- **LMA**, the Load Memory Address — where the loader puts the bytes.
- **VMA**, the Virtual Memory Address — where the code expects to be.

Nimbus is loaded at physical `0x00100000` and runs at virtual `0xC0100000`.

---

```ld
ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xC0000000;
KERNEL_LOAD_ADDR    = 0x00100000;
```
Script-level variables. They appear in three files — here, `paging.h`, and `boot.asm` — and ⚠️
nothing checks that they agree.

---

## The low part

```ld
    . = KERNEL_LOAD_ADDR;
    __kernel_phys_start = .;

    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }
```
**No `AT()`.** LMA and VMA are both `0x00100000`-ish, which is correct: this code runs before paging
and must be addressed where it is loaded.

That is the entire trick. `boot.asm` puts its early code and its page directory here, so
`mov ecx, boot_page_directory` loads a physical address with no arithmetic.

`.multiboot.data` first, so the multiboot header is within the first 8 KiB of the file.

---

## The jump

```ld
    . += KERNEL_VIRTUAL_BASE;
    __kernel_start = .;
```
⚠️ **`. +=`, not `. =`.**

The `+=` preserves however far the low sections got. Writing `. = 0xC0100000` would silently create
an overlap the day `boot.asm` grew past 4 KiB.

---

## The high part

```ld
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    }
```
`AT(...)` says: address it here, store it there.

`ADDR(.text)` is the section's VMA; subtracting the base gives the LMA. The ELF program header then
has `p_vaddr = 0xC0103000` and `p_paddr = 0x00103000`, and a multiboot loader loads by `p_paddr`.

`ALIGN(4K)` matters here in a way it did not in Spark: `paging_init` marks `.text` and `.rodata`
read-only, and page permissions are per page. A `.text` sharing a page with `.data` could not have
its own.

```ld
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
    .data   ALIGN(4K) : AT(ADDR(.data)   - KERNEL_VIRTUAL_BASE)
    .bss    ALIGN(4K) : AT(ADDR(.bss)    - KERNEL_VIRTUAL_BASE)
```
Same pattern. `.bss` is `NOBITS` so it occupies no file space; the `AT()` on it is harmless.

---

## The symbols

```ld
    __kernel_end = .;
    __kernel_phys_end = . - KERNEL_VIRTUAL_BASE;
```
⚠️ **Both**, because two different consumers want different ones.

`pmm_init` reserves the kernel image and works in *physical* addresses:

```c
    pmm_reserve_region((paddr_t)__kernel_phys_start,
                       (size_t)(__kernel_phys_end - __kernel_phys_start));
```

Providing the physical symbols means that line needs no conversion that might be wrong. The script
says so:

> If those two symbols are wrong, the frame allocator will hand out frames containing the running
> kernel, and the failure will look like random corruption minutes later. They are worth checking
> with `i686-elf-readelf -s nimbus.elf | grep kernel_phys` the first time.

All the symbols defined here:

| Symbol | Used by |
|---|---|
| `__kernel_phys_start` / `_end` | `pmm_init` |
| `__kernel_start` / `_end` | diagnostics |
| `__text_start` / `_end` | `paging_init`, for read-only marking |
| `__rodata_start` / `_end` | same |
| `__data_start` / `_end` | diagnostics |
| `__bss_start` / `_end` | `boot.asm`, for zeroing |

⚠️ In C, declare them as arrays:

```c
extern char __kernel_start[];
```

Never `extern char *x` — that emits a load from the address rather than using it.

---

## Discards

```ld
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
```
Same as Spark. `.eh_frame` in particular would otherwise land between sections and confuse the
layout.

---

## Verifying

```bash
$ i686-elf-readelf -l bin/nimbus.elf

Program Headers:
  Type    Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD    0x001000 0x00100000 0x00100000 0x01044 0x01044 RWE 0x1000
  LOAD    0x003000 0xc0103000 0x00103000 0x06a20 0x06a20 R E 0x1000
  LOAD    0x00a000 0xc010a000 0x0010a000 0x00c40 0x05c40 RW  0x1000
```

⚠️ **The check to run after any change to this file.**

Two columns. The first `LOAD` has `VirtAddr == PhysAddr` — the low sections. The other two differ by
exactly `0xC0000000`.

The third segment's `MemSiz` (`0x5c40`) exceeds its `FileSiz` (`0x00c40`) — that is `.bss`, including
the 16 KiB stack.

```bash
$ i686-elf-readelf -s bin/nimbus.elf | grep kernel_phys
   112: 00100000     0 NOTYPE  GLOBAL DEFAULT  ABS __kernel_phys_start
   113: 0010f000     0 NOTYPE  GLOBAL DEFAULT  ABS __kernel_phys_end
```

Physical addresses, as intended. Compare with the log:

```
kernel: 00100000 - 0010f000 (60 KiB)
```

---

## Changing the split

To move the kernel to `0x80000000` (a 2 GiB/2 GiB split), three files change:

| File | Change |
|---|---|
| `link.ld` | `KERNEL_VIRTUAL_BASE` |
| `include/nimbus/paging.h` | `KERNEL_VIRTUAL_BASE`, and `KERNEL_PAGE_NUMBER` follows |
| `boot/boot.asm` | `KERNEL_VIRTUAL_BASE`, and `KERNEL_PAGE_NUMBER` follows |

⚠️ And check `KHEAP_START` in `heap.h` is still above the direct map and below the top.

Exercise 25.5.

---

[Index](README.md) · [Chapter 25](../25-higher-half.md)
