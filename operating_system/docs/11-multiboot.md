# Chapter 11 — Multiboot, and a second way to boot

[← Hello, VGA](10-vga-hello.md) · [Contents](README.md) · [Next: A real VGA driver →](12-vga-driver.md)

> 📖 **Line by line:** [boot.asm](line-by-line/nimbus-boot.md)

---

## Goal

Start Nimbus. Write the eight bytes of header that let QEMU or GRUB load our kernel with no
bootloader of our own, understand the contract that creates, and get to a `kmain` that prints.

This chapter also answers the question the last one ended on: why, having spent six chapters writing
a bootloader, are we not going to use it?

---

## 1. Why not use Spark's bootloader?

Three reasons, in increasing order of honesty.

**It would buy nothing.** Spark exists to teach you how a machine boots. You have written a boot
sector, done CHS arithmetic, opened the A20 gate and set `CR0.PE`. Doing it a second time teaches
nothing new and adds 600 lines to every build.

**Multiboot gives us things our bootloader does not.** Chiefly the memory map — Chapter 21 needs to
know how much RAM exists and which parts are usable, and getting that from `int 0x15 E820` means
collecting it in real mode and stashing it somewhere (Chapter 7, §4). Multiboot hands it over in a
struct. It also hands over *modules*, which is how the initrd arrives in Chapter 40.

**The iteration loop is faster.** `qemu -kernel nimbus.elf` boots the ELF file directly. No image
build, no `objcopy`, no size limits, no "did I remember to rebuild the image". When you are
recompiling every ninety seconds for a week, that matters more than it sounds.

Chapter 48 covers making Nimbus boot from Spark's loader if you want the satisfaction. It is about
forty lines: parse the ELF, collect E820, and construct a multiboot info struct by hand.

---

## 2. What Multiboot is

A specification published in 1995 by the GRUB authors, to end the situation where every kernel needed
its own bootloader and every bootloader knew about specific kernels.

The contract has two halves.

**Our half:** put a 12-byte header in the first 8 KiB of the kernel image, 4-byte aligned, containing
a magic number, some flags, and a checksum.

**Their half:** if that header is found, the loader promises to hand over a machine in a specific
state, with specific information in specific registers.

GRUB implements it. QEMU implements enough of it that `-kernel` works. So do most other bootloaders.
Our kernel does not care which one loaded it.

---

## 3. The header

```nasm
MB_ALIGN     equ 1 << 0                     ; align loaded modules on 4 KiB
MB_MEMINFO   equ 1 << 1                     ; give us the memory map
MB_FLAGS     equ MB_ALIGN | MB_MEMINFO
MB_MAGIC     equ 0x1BADB002
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)

section .multiboot.data
align 4
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
```

Twelve bytes.

**`0x1BADB002`.** "1 BAD B002", which is a joke about `BOOT`. It is the value the loader scans for.

**The flags** say what we want. Bit 0 asks for modules to be page-aligned, which matters because the
initrd will be mapped and an unaligned module means the first page contains something else. Bit 1
asks for the memory map. There are more — bit 2 requests video mode information, which a graphical
kernel would want — and each one we set is a promise the loader must keep.

**The checksum** is defined so that `magic + flags + checksum ≡ 0 (mod 2³²)`. Unsigned overflow makes
this work: `-(a + b)` as a 32-bit value is exactly what you add to `a + b` to get zero.

Its purpose is to make a false positive nearly impossible. `0x1BADB002` could appear by chance in a
large binary; `0x1BADB002` followed by a value and its negation could not.

### 3.1 It must be found

The loader scans the first 8 KiB of the file for the magic number on a 4-byte boundary. So the header
has to be *near the start of the file*, which is a statement about the file layout, not about
addresses.

That is what [`link.ld`](../nimbus/link.ld) guarantees:

```ld
    . = KERNEL_LOAD_ADDR;
    __kernel_phys_start = .;

    .multiboot.data : {
        *(.multiboot.data)
    }
```

First output section, so first bytes of the file. If this ends up 9 KiB in — which it would if
`.text` came first and the kernel were large — QEMU refuses with:

```
qemu: could not load kernel 'nimbus.elf': Invalid multiboot header
```

which is at least a clear message, and is the one error in this chapter that tells you exactly what
is wrong.

---

## 4. What the loader promises

From the specification, the state on entry to our `_start`:

| Thing | State |
|---|---|
| CPU mode | 32-bit protected mode |
| `CS` | a flat 32-bit ring 0 code segment |
| `DS`, `ES`, `FS`, `GS`, `SS` | flat 32-bit data segments |
| A20 | enabled |
| Paging | **off** |
| `EFLAGS.IF` | **clear** — interrupts disabled |
| `EAX` | `0x2BADB002` |
| `EBX` | physical address of a `multiboot_info_t` |
| `ESP` | **undefined** |
| `EFLAGS.DF` | undefined |

Compare that with the BIOS handover in Chapter 4, §3. Everything Chapters 5–8 did has been done for
us: real mode is behind us, A20 is open, a GDT exists.

Two rows are worth dwelling on.

**`ESP` is undefined.** We cannot `call` anything, `push` anything, or execute a single line of C
until we set it. That is why the first few instructions of
[`boot.asm`](../nimbus/boot/boot.asm) are all register-to-register work.

**`EAX` and `EBX` are the only places the boot information exists.** Nothing below the stack setup
may clobber them. The file says so in a comment, and it is a real constraint — an early version of
that file used `EAX` as scratch for the `CR4` read and lost the magic number.

### 4.1 The GDT we are given

The loader's GDT is flat and correct, and we replace it in Chapter 15 anyway. The specification is
explicit that its GDT may be anywhere and may be reclaimed, so a kernel that keeps using it is a
kernel whose descriptor table might get overwritten by its own memory allocator. The failure would
be a triple fault at an unpredictable moment.

Replace it early. Chapter 15 does.

---

## 5. The info structure

`EBX` points at this, in physical memory below 1 MiB:

```c
typedef struct multiboot_info {
    uint32_t flags;

    uint32_t mem_lower;      /* KiB of conventional memory below 1 MiB */
    uint32_t mem_upper;      /* KiB above 1 MiB -- NOT total RAM       */

    uint32_t boot_device;
    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;
    ...
    uint32_t mmap_length;
    uint32_t mmap_addr;
    ...
} PACKED multiboot_info_t;
```

### 5.1 `flags` is not decoration

**Each bit of `flags` says whether the corresponding field is valid.** A field whose bit is clear
contains whatever was in that memory.

This is the single biggest source of "works in QEMU, hangs under GRUB" bugs, because the two
bootloaders fill in different subsets. [`multiboot.h`](../nimbus/include/nimbus/multiboot.h) names
every bit, and [`pmm.c`](../nimbus/mm/pmm.c) checks before reading:

```c
    if (!(mbi->flags & MB_INFO_MEM_MAP))
        panic("pmm: the bootloader gave us no memory map (flags=%08x)\n"
              "     Nimbus cannot guess how much RAM exists.", mbi->flags);
```

A panic with the flags printed, rather than a silent walk through uninitialised memory.

### 5.2 `mem_upper` is a trap

`mem_lower` and `mem_upper` look like "how much RAM is there" and are not.

`mem_upper` is the size of the *first contiguous region* above 1 MiB, in KiB. On a machine with a
memory hole — and almost every real machine has one, for PCI address space — it undercounts,
sometimes by a lot.

Use the memory map. That is what it is for, and Chapter 21 walks it properly.

### 5.3 Modules

```c
typedef struct multiboot_module {
    uint32_t mod_start;      /* physical address of the first byte  */
    uint32_t mod_end;        /* physical address one past the last  */
    uint32_t cmdline;
    uint32_t pad;
} PACKED multiboot_module_t;
```

A "module" is any file the bootloader was told to load alongside the kernel. QEMU's `-initrd` becomes
module 0; GRUB's `module /initrd.tar` line does the same.

The bootloader loads it, tells us where, and takes no further interest. Chapter 40 parses it as a tar
archive.

Two things to be careful about, both of which [`pmm.c`](../nimbus/mm/pmm.c) handles:

- The module's memory is described by the memory map as *available*. If we do not reserve it, the
  frame allocator will hand out frames containing our initrd.
- So is the `multiboot_info_t` itself, and the memory map array it points at. Same problem.

```c
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

Four reservations, and forgetting any of them produces corruption minutes later and a long way from
the cause.

---

## 6. `_start`

The full early path is Chapter 25's subject — it sets up paging and jumps to the higher half — so
here is only the part that is about multiboot:

```nasm
_start:
    ; EAX = 0x2BADB002, EBX = &multiboot_info, ESP = undefined

    mov ecx, cr4
    or  ecx, 0x00000010             ; CR4.PSE
    mov cr4, ecx

    mov ecx, boot_page_directory
    mov cr3, ecx

    mov ecx, cr0
    or  ecx, 0x80000000             ; CR0.PG
    mov cr0, ecx

    lea ecx, [higher_half]
    jmp ecx

higher_half:
    ...
    mov esp, stack_top
    xor ebp, ebp

    push ebx                        ; arg 2: multiboot info, PHYSICAL
    push eax                        ; arg 1: the magic number
    call kmain
```

`ECX` for everything, because `EAX` and `EBX` hold the boot information and must survive until the
two pushes.

The arguments are pushed in reverse order — cdecl puts the first argument at the lowest address
(Chapter 3, §5.1) — so `kmain(magic, mbi_physical)`.

### 6.1 Checking the magic

```c
void kmain(uint32_t magic, uint32_t mbi_physical)
{
    serial_init(COM1);
    serial_puts("\n\n=== Nimbus starting ===\n");

    vga_init();
    printk_enable_console();
    banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("bad multiboot magic %08x (expected %08x).\n"
              "Boot with `qemu-system-i386 -kernel nimbus.elf`, or from GRUB.",
              magic, MULTIBOOT_BOOTLOADER_MAGIC);

    multiboot_info_t *mbi = (multiboot_info_t *)P2V(mbi_physical);
```

Note the order: **serial first, then screen, then the check.** If we panicked before initialising
output, the panic would be invisible.

The check itself matters because if `EAX` is wrong we were not loaded by a multiboot loader, which
means `EBX` is not a memory map — it is whatever the real loader happened to leave there. Reading it
produces nonsense that looks almost plausible: a `mmap_length` of a few million, a `mmap_addr`
pointing into the BIOS, and a memory map walk that runs for a long time and then faults.

Failing here, with a message that names the fix, is worth the four lines.

### 6.2 `P2V`

```c
    multiboot_info_t *mbi = (multiboot_info_t *)P2V(mbi_physical);
```

`EBX` is a *physical* address. By the time C runs, paging is on and the identity mapping has been
removed, so that number is not a usable pointer — dereferencing it is a page fault.

`P2V` adds `0xC0000000`, converting to the kernel's direct-map window. Chapter 25 explains the
window; for now, the rule is: **anything the bootloader gave us is physical, and everything our code
uses is virtual.**

[`types.h`](../nimbus/include/nimbus/types.h) gives the two a different name for this reason:

```c
typedef uint32_t            paddr_t;   /* physical: what goes on the bus       */
typedef uint32_t            vaddr_t;   /* virtual:  what your code dereferences*/
```

They are the same type and the compiler will not stop you mixing them. The names are for the reader.

---

## 7. Building and booting

```bat
build nimbus
run nimbus
```

which runs:

```
qemu-system-i386 -m 128M -serial stdio -no-reboot -no-shutdown \
                 -kernel bin/nimbus.elf -initrd bin/initrd.tar
```

No disk image. No bootloader. QEMU reads the ELF file, finds the multiboot header, loads each
`PT_LOAD` segment at its physical address, and jumps to the entry point.

### 7.1 Checking the header is where it should be

```bash
$ i686-elf-readelf -S bin/nimbus.elf | head -8
Section Headers:
  [Nr] Name              Type      Addr     Off    Size   Flg
  [ 1] .multiboot.data   PROGBITS  00100000 001000 001004  A
  [ 2] .multiboot.text   PROGBITS  00102000 003000 000040  AX
  [ 3] .text             PROGBITS  c0103000 004000 004a20  AX
```

`.multiboot.data` at file offset `0x1000` — 4 KiB in, well within the 8 KiB limit.

And the first bytes:

```bash
$ i686-elf-objdump -s -j .multiboot.data bin/nimbus.elf | head -3
Contents of section .multiboot.data:
 100000 02b0ad1b 03000000 fb4f52e2  .........OR.
```

`02 b0 ad 1b` is `0x1BADB002` little-endian. `03 00 00 00` is the flags. `fb 4f 52 e2` is the
checksum, and `0x1BADB002 + 3 + 0xE2524FFB = 0x100000000`, which is 0 in 32 bits. ✓

### 7.2 Booting under GRUB instead

Not needed, and worth knowing. Create `grub.cfg`:

```
menuentry "Nimbus" {
    multiboot /boot/nimbus.elf
    module    /boot/initrd.tar
    boot
}
```

and build an ISO:

```bash
mkdir -p iso/boot/grub
cp bin/nimbus.elf bin/initrd.tar iso/boot/
cp grub.cfg iso/boot/grub/
grub-mkrescue -o nimbus.iso iso
qemu-system-i386 -cdrom nimbus.iso -serial stdio
```

`grub-mkrescue` needs `xorriso` and is awkward on Windows, which is why the default path is
`-kernel`. But this is the route to booting on real hardware, and it is worth doing once before
Chapter 47.

---

## 8. What a Multiboot 2 kernel would look like

Multiboot 2, from 2010, fixes several things: it supports UEFI, 64-bit kernels, and a proper
tag-based info structure that can be extended without breaking old kernels.

The differences that matter:

| | Multiboot 1 | Multiboot 2 |
|---|---|---|
| Magic | `0x1BADB002` | `0xE85250D6` |
| Header | fixed 12 bytes | variable, a list of tags |
| Info struct | fixed layout with a flags word | a list of tags, walk until the end tag |
| UEFI | no | yes |
| 64-bit | no | yes |

The tag-based design is strictly better — a bootloader can add information without any kernel
needing to change, and a kernel can skip tags it does not understand. The fixed struct with a flags
word cannot be extended past 32 fields.

We use version 1 because QEMU's `-kernel` implements it, because every example in every book and
tutorial uses it, and because the extra concepts in version 2 are bookkeeping rather than ideas.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| `qemu: Invalid multiboot header` | The header is not in the first 8 KiB, not 4-byte aligned, or the checksum is wrong |
| `qemu: could not load kernel` | The ELF is 64-bit, or has no `PT_LOAD` segments |
| Panics on the magic check | Booted without `-kernel`, or the entry point ran before `_start` |
| Boots, then faults reading `mbi` | Forgot `P2V`, or read a field whose flag bit is clear |
| Works with `-kernel`, fails under GRUB | Read a field GRUB does not fill in. Check `flags`. |
| Random corruption after a while | Did not reserve the multiboot structures or the module |

That last row is the expensive one, because the corruption appears far from the cause. If Nimbus
starts behaving strangely after a few seconds of uptime and the initrd files are garbage, the
reservations in §5.3 are the first thing to check.

---

## 10. Exercises

🟢 **11.1** Corrupt the checksum by one and run it. Read QEMU's error.

🟢 **11.2** Print `mbi->flags` in binary (`%b` is supported by our `kprintf`) and identify which
fields QEMU filled in. Compare with what GRUB provides if you built the ISO.

🟢 **11.3** Print `mem_lower` and `mem_upper`, and compare `mem_upper` KiB with the `-m` value you
passed. Explain the difference.

🟡 **11.4** Set the video mode bit (bit 2) in `MB_FLAGS` and add the six extra header fields the
specification requires. What does QEMU do with them?

🟡 **11.5** Remove the reservation of `mods_addr` in `pmm_init` and run until something breaks.
Predict what will be corrupted first.

🟡 **11.6** Pass a kernel command line with `-append "debug quiet"` and print it. Remember `P2V` and
the flag bit.

🔴 **11.7** Write the forty lines that make Spark's bootloader load Nimbus: parse the ELF program
headers, collect `int 0x15 E820` into a buffer, build a `multiboot_info_t` by hand, set `EAX` and
`EBX`, and jump. This closes the loop on Part I.

---

## What we covered

- Why Nimbus uses multiboot rather than the bootloader we wrote: no new learning, better information,
  a much faster build.
- The twelve-byte header, the checksum that makes false positives impossible, and the linker section
  that keeps it in the first 8 KiB.
- The state the loader promises, including the two rows that constrain `_start`: undefined `ESP`,
  and boot information living only in `EAX` and `EBX`.
- Why we replace the loader's GDT rather than keeping it.
- `flags` as a validity mask, `mem_upper` as a trap, and modules as the way the initrd arrives.
- The four regions that must be reserved or the frame allocator will hand them out.
- `P2V`, and the discipline of naming physical and virtual addresses differently.
- How to verify the header is where you think, in three commands.

[Chapter 12](12-vga-driver.md) writes a VGA driver properly: scrolling, the hardware cursor, and the
port protocol that drives a 1987 graphics adapter through a two-register keyhole.

---

[← Hello, VGA](10-vga-hello.md) · [Contents](README.md) · [Next: A real VGA driver →](12-vga-driver.md)
