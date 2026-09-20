# Line by line: `nimbus/boot/boot.asm`

[Index](README.md) · [Chapter 11](../11-multiboot.md) · [Chapter 25](../25-higher-half.md)

The twenty hardest lines in the kernel.

---

## Constants

```nasm
MB_ALIGN     equ 1 << 0
MB_MEMINFO   equ 1 << 1
MB_FLAGS     equ MB_ALIGN | MB_MEMINFO
MB_MAGIC     equ 0x1BADB002
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)
```
`0x1BADB002` is "1 BAD B002", a joke about `BOOT`.

Each flag we set is a **promise the loader must keep**. Bit 0 asks for page-aligned modules — which
matters because the initrd will be mapped and an unaligned module means the first page contains
something else. Bit 1 asks for the memory map.

The checksum is defined so the three sum to zero mod 2³². `-(a + b)` as a 32-bit value is exactly
what you add to get there. Its purpose is to make a false positive impossible: the magic alone could
appear by chance in a large binary; the magic followed by a value and its negation could not.

```nasm
KERNEL_VIRTUAL_BASE equ 0xC0000000
KERNEL_PAGE_NUMBER  equ (KERNEL_VIRTUAL_BASE >> 22)   ; = 768
BOOT_PAGES   equ 4
```
⚠️ `BOOT_PAGES` is 4, not 1. `paging_init` calls `P2V` on frames the allocator hands out, and those
must be inside the boot mapping. With 1, a machine where the kernel plus initrd push the first free
frame past 4 MiB page-faults in `paging_init` — a bug that appears the day the initrd grows.

---

## `.multiboot.data`

```nasm
section .multiboot.data

align 4
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
```
⚠️ Must be within the first 8 KiB of the *file* and 4-byte aligned. `link.ld` places this section
first for exactly that. If it ends up 9 KiB in, QEMU refuses with "Invalid multiboot header" — which
is at least a clear message.

```nasm
align 4096
global boot_page_directory
boot_page_directory:
```
⚠️ 4 KiB alignment is not an optimisation. `CR3` holds only the top 20 bits of the address; a
misaligned directory is unrepresentable.

```nasm
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep
```
Entries 0–3: identity map the first 16 MiB.

`0x83` = `PS | WRITABLE | PRESENT`. The `PS` bit means this entry maps a 4 MiB page directly, with no
second level — which is why no page tables are assembled by hand.

Needed for exactly three instructions, and deleted immediately afterwards.

```nasm
    times (KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0
```
Entries 4–767: not present.

```nasm
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep

    times (1024 - KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0
```
Entries 768–771: the same 16 MiB at `0xC0000000`. Then 252 empty.

4 + 764 + 4 + 252 = 1024. The whole directory, assembled at build time, no runtime code.

---

## `.multiboot.text`

```nasm
section .multiboot.text

global _start
extern kmain
extern __bss_start
extern __bss_end
```
⚠️ A separate section from `.text`, and `link.ld` places it at a **low** address with no `AT()`,
because it runs before paging and must be addressed where it is loaded.

```nasm
_start:
```
State on entry, per the specification: 32-bit protected mode, flat segments, A20 on, paging off,
interrupts off, `EAX = 0x2BADB002`, `EBX` = physical address of the info structure, **`ESP`
undefined**.

⚠️ `ESP` undefined means no `call`, no `push`, no C — until §"Stack" below. And `EAX`/`EBX` are the
only places the boot information exists, so nothing may clobber them. Hence `ECX` for everything.

```nasm
    mov ecx, cr4
    or  ecx, 0x00000010
    mov cr4, ecx
```
`CR4.PSE`, bit 4. Enables 4 MiB pages. Every CPU since the Pentium has it.

```nasm
    mov ecx, boot_page_directory
    mov cr3, ecx
```
⚠️ **`boot_page_directory` is already the physical address**, because `link.ld` placed
`.multiboot.data` low.

That is the entire payoff of the two-section split: no `- 0xC0000000` fudge factor, and therefore no
chance of forgetting one.

```nasm
    mov ecx, cr0
    or  ecx, 0x80000000
    mov cr0, ecx
```
`CR0.PG`, bit 31. From this instruction on, every address goes through the MMU.

`EIP` is around `0x00101000`, covered by directory entry 0 — **the identity mapping is the only
reason the next instruction can be fetched**.

```nasm
    lea ecx, [higher_half]
    jmp ecx
```
⚠️ **The most important instruction in the file.**

An absolute **indirect** jump. `jmp higher_half` would assemble to a relative displacement and keep
`EIP` in the low mapping; loading the absolute address into a register first forces `EIP` to become
`0xC01xxxxx`, which is what lets us unmap the low addresses.

`higher_half` is in `.text`, linked at `0xC0103000`-ish, so `lea` loads that value.

---

## `.text`

```nasm
section .text

higher_half:
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  0], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  4], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  8], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE + 12], 0
```
`boot_page_directory` is a low address, so `+ KERNEL_VIRTUAL_BASE` reaches it through the **high**
window — both windows point at the same physical frame.

Why remove it: a stray write to a small address — the classic `*(int *)0 = 1` — would otherwise
quietly overwrite the interrupt vector table instead of faulting. Leaving the identity map in makes
null pointer bugs silent.

```nasm
    mov ecx, cr3
    mov cr3, ecx
```
Four entries changed and the TLB does not notice. Reloading `CR3` is the blunt instrument, and here
it is correct because 16 MiB of mappings changed.

```nasm
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb
```
A multiboot loader is supposed to zero the difference between a segment's file size and memory size,
and both GRUB and QEMU do. Doing it ourselves costs eight instructions and removes a dependency on
someone else's correctness for a failure — uninitialised globals — that would present weeks later.

No stack needed: `rep stosb` touches only `EDI`, `ECX` and `EAX`. Just as well, because the stack is
inside the region being cleared.

```nasm
    mov esp, stack_top
    xor ebp, ebp
```
`stack_top` is the high end. `xor ebp, ebp` terminates the frame chain.

```nasm
    push ebx
    push eax
    call kmain
```
Reverse order, because cdecl puts the first argument at the lowest address. So
`kmain(magic, mbi_physical)`.

`EAX` and `EBX` survived because `ECX` did all the work.

```nasm
    cli
.hang:
    hlt
    jmp .hang
```
`kmain` must not return. A returning `kmain` is a bug, and a silent one is a bug you will chase for
an hour.

---

## The stack

```nasm
section .bss
align 16
global stack_bottom
global stack_top
stack_bottom:
    resb 16384
stack_top:
```
16 KiB in `.bss`, costing nothing in the image.

This is the stack the kernel runs on until `task_init` — after which it becomes the idle task's
stack, claimed by:

```c
    extern char stack_top[];
    t->kernel_stack = (uint32_t)stack_top;
```

⚠️ No guard page below it. A kernel stack overflow silently corrupts whatever `.bss` variable the
linker placed underneath. Chapter 26, exercise 26.6.

---

## Verifying

```bash
$ i686-elf-readelf -S bin/nimbus.elf | head -6
  [ 1] .multiboot.data   PROGBITS  00100000 001000 001004  A
  [ 2] .multiboot.text   PROGBITS  00102000 003000 000040  AX
  [ 3] .text             PROGBITS  c0103000 004000 004a20  AX
```
`.multiboot.data` at file offset `0x1000` — 4 KiB in, well within the limit. The low sections have
`Addr` matching their file position; `.text` does not.

```bash
$ i686-elf-objdump -s -j .multiboot.data bin/nimbus.elf | head -3
 100000 02b0ad1b 03000000 fb4f52e2
```
`02 b0 ad 1b` is `0x1BADB002`. `03 00 00 00` is the flags. `fb 4f 52 e2` is the checksum, and
`0x1BADB002 + 3 + 0xE2524FFB = 0x100000000` — zero in 32 bits. ✓

In GDB:

```
(gdb) break *0x00101000
(gdb) info registers eip
eip  0x101000
(gdb) stepi 8
(gdb) info registers eip
eip  0xc0103012
```
`EIP` jumping across the `jmp ecx` is the clearest demonstration available.

---

[Index](README.md) · [Chapter 11](../11-multiboot.md) · [Chapter 25](../25-higher-half.md)
