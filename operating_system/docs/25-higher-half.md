# Chapter 25 — The higher half

[← Enabling paging](24-enabling-paging.md) · [Contents](README.md) · [Next: Page faults →](26-page-faults.md)

> 📖 **Line by line:** [boot.asm](line-by-line/nimbus-boot.md) · [link.ld](line-by-line/nimbus-link.md)

---

## Goal

Explain the twenty hardest lines in the kernel: how a kernel gets from the address it was *loaded*
at to the address it was *linked* for, while executing.

This is [`boot.asm`](../nimbus/boot/boot.asm) and [`link.ld`](../nimbus/link.ld), and it is the one
place where the build system and the code have to conspire.

---

## 1. Why the kernel lives at 3 GiB

The kernel is linked at `0xC0100000` and every process gets `0x00000000`–`0xBFFFFFFF` to itself.

```
    0xFFFFFFFF  +------------------+
                |                  |
                |   kernel   1 GiB |  shared by every process
                |                  |
    0xC0000000  +------------------+
                |                  |
                |                  |
                |   user     3 GiB |  different in every process
                |                  |
                |                  |
    0x00000000  +------------------+
```

Three reasons, and the third is the real one.

**A system call needs no `CR3` reload.** The kernel is mapped at the same addresses in every address
space (Chapter 24, §6.1). Entering the kernel changes privilege level and nothing else — no TLB
flush, no address space switch. A syscall costs a few hundred cycles instead of a few thousand.

**The kernel can read user memory directly.** `copy_from_user` is a `memcpy` after a bounds check,
because the user's pages are mapped right there in the low half. The alternative — a kernel in its
own address space — means every user pointer has to be walked and temporarily mapped.

**Interrupt handlers work in any context.** An interrupt can arrive while any process is running. The
handler runs in whatever address space was current, and it needs the kernel's code, stack and data to
be mapped. Sharing the top gigabyte makes that automatic.

The split at 3 GiB is Linux's traditional choice, for the reason that 1 GiB is enough kernel address
space to direct-map a reasonable amount of RAM (Chapter 24, §2.1) while 3 GiB is more than any 32-bit
program can usefully consume.

---

## 2. The problem

We want the kernel to *run* at `0xC0100000`. The bootloader *loads* it at `0x00100000` with paging
off.

So for the first few instructions the kernel is at an address it was not linked for. Any absolute
reference — a global variable, a call to another function, a string constant — points 3 GiB too high,
into memory that does not exist.

```nasm
    mov eax, some_global        ; assembles as mov eax, 0xC0104000
                                ; ...which is not mapped yet
```

Three ways out:

**Write position-independent early code.** Every reference becomes `label - 0xC0000000`, by hand, and
one missed subtraction is a triple fault. This is what many tutorials do and it is error-prone.

**Relocate at runtime.** Parse your own ELF relocations and patch yourself. Real, and far too much
work for twenty instructions.

**Split the kernel into two linker sections.** Early code linked low, everything else linked high.
This is what we do, and it means the early code needs no fudge factor at all — so there is no
subtraction to forget.

---

## 3. The linker script does the work

```ld
    . = KERNEL_LOAD_ADDR;               /* 0x00100000 */
    __kernel_phys_start = .;

    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }

    . += KERNEL_VIRTUAL_BASE;           /* jump up by 3 GiB */
    __kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        ...
    }
```

### 3.1 LMA and VMA

Two addresses matter for every byte of a kernel, and for most programs they are the same number so
nobody ever learns the difference:

- **LMA**, the Load Memory Address — where the loader puts the bytes.
- **VMA**, the Virtual Memory Address — where the code expects to be.

`AT(...)` is how a linker script says they differ: the section is *addressed* here and *stored*
there.

```ld
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
```

"Address `.text` wherever the location counter is (`0xC0101000`), but store it 3 GiB lower
(`0x00101000`)."

The ELF program header then has `p_vaddr = 0xC0101000` and `p_paddr = 0x00101000`, and a multiboot
loader loads by `p_paddr`.

### 3.2 The two low sections

`.multiboot.data` and `.multiboot.text` have **no `AT()`**. Their LMA and VMA are both
`0x00100000`-ish, which is correct, because they run before paging and must be addressed where they
are loaded.

This is the whole trick. `boot.asm` puts its early code and its page directory in those sections, so:

```nasm
    mov ecx, boot_page_directory
    mov cr3, ecx
```

**`boot_page_directory` is already the physical address.** No subtraction, no fudge factor, no
opportunity to forget one. The comment in the source says exactly that:

> This is the entire payoff of the two-section split: no `- 0xC0000000` fudge factor, and therefore
> no chance of forgetting one.

### 3.3 `. +=` rather than `. =`

```ld
    . += KERNEL_VIRTUAL_BASE;
```

Not `. = 0xC0100000`.

The `+=` preserves however far the low sections got. If `boot.asm` grows past 8 KiB, the high
sections move up with it. Writing an absolute address would silently create an overlap the day the
early code grew.

### 3.4 Verifying it

```bash
$ i686-elf-readelf -l bin/nimbus.elf

Program Headers:
  Type    Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD    0x001000 0x00100000 0x00100000 0x01044 0x01044 RWE 0x1000
  LOAD    0x003000 0xc0103000 0x00103000 0x06a20 0x06a20 R E 0x1000
  LOAD    0x00a000 0xc010a000 0x0010a000 0x00c40 0x05c40 RW  0x1000
```

Two columns. The first `LOAD` has `VirtAddr == PhysAddr` — the low sections. The other two differ by
exactly `0xC0000000`.

That is the check to run after any change to `link.ld`, and it takes five seconds.

---

## 4. The twenty instructions

```nasm
section .multiboot.text

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

section .text

higher_half:
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  0], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  4], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  8], 0
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE + 12], 0

    mov ecx, cr3
    mov cr3, ecx

    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    mov esp, stack_top
    xor ebp, ebp

    push ebx
    push eax
    call kmain
```

### 4.1 `ECX` for everything

`EAX` holds the multiboot magic and `EBX` the info pointer, and they must survive until the two
pushes at the bottom. `ECX` is the only scratch register used anywhere in this code.

The source flags it:

> "ESP is undefined" is the one that bites: we cannot call anything, push anything, or use a single C
> function until we set up a stack. Until then EAX and EBX are the only places the boot information
> can live, so nothing below is allowed to clobber them.

An early draft of this file used `EAX` for the `CR4` read and lost the magic number, which presented
as a panic about a bad bootloader.

### 4.2 The boot page directory

```nasm
align 4096
global boot_page_directory
boot_page_directory:
    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep

    times (KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0

    %assign i 0
    %rep BOOT_PAGES
        dd (i << 22) | 0x83
    %assign i i+1
    %endrep

    times (1024 - KERNEL_PAGE_NUMBER - BOOT_PAGES) dd 0
```

Assembled at build time, not built at runtime. 4 + 764 + 4 + 252 = 1024 entries.

**`0x83`** is `PS | WRITABLE | PRESENT`: a 4 MiB page. Using 4 KiB pages here would mean assembling
four page tables by hand in assembly, for a mapping we are about to throw away.

**Entries 0–3** identity-map the first 16 MiB. Needed for exactly three instructions — the ones
between enabling paging and jumping high — and deleted immediately afterwards.

**Entries 768–771** map the same 16 MiB at `0xC0000000`.

**`align 4096`** because `CR3` holds only the top 20 bits of the address. A misaligned directory is
not merely slow; it is unrepresentable.

**Why 16 MiB and not 4?** Chapter 24, §5.2: `paging_init` calls `P2V` on frames the allocator hands
out, and those must be within the boot mapping. With the kernel plus a growing initrd, 4 MiB runs out.

### 4.3 The three instructions that matter

```nasm
    mov cr3, ecx
    ...
    mov cr0, ecx        ; <- paging is on NOW
    lea ecx, [higher_half]
    jmp ecx
```

After `mov cr0, ecx`, every address goes through the MMU. `EIP` is somewhere around `0x00101000`,
which is covered by directory entry 0 — **the identity mapping is the only reason the next
instruction can be fetched at all.**

Then:

```nasm
    lea ecx, [higher_half]
    jmp ecx
```

An **absolute indirect** jump, not a relative one. And the source explains why:

> `jmp higher_half` would assemble to a relative displacement and keep EIP down here in the low
> mapping; loading the absolute address into a register first forces EIP to become `0xC01xxxxx`,
> which is what lets us unmap the low addresses.

`higher_half` is in `.text`, which is linked at `0xC0103000`-ish, so `lea` loads that value.
`jmp ecx` sets `EIP` to it. From this instruction on we are executing in the higher half.

This is the single most important instruction in the file.

### 4.4 Removing the identity map

```nasm
    mov dword [boot_page_directory + KERNEL_VIRTUAL_BASE +  0], 0
    ...
    mov ecx, cr3
    mov cr3, ecx
```

`boot_page_directory` is a low address, so `+ KERNEL_VIRTUAL_BASE` reaches it through the *high*
window — both windows point at the same physical frame, so `0xC0101000` and `0x00101000` are the same
bytes.

Then reload `CR3`, because four entries changed and the TLB does not notice (Chapter 23, §5).

**Why remove it at all?** Because a stray write to a small address — the classic `*(int *)0 = 1` —
would otherwise quietly overwrite the interrupt vector table instead of faulting. Leaving the
identity map in place means null pointer bugs are silent.

After this, `0x00000000`–`0x00FFFFFF` is unmapped and will stay that way until a user process is
created.

---

## 5. The consequence you meet immediately

**Every fixed physical address a driver needs must go through `P2V`.**

```c
void vga_init(void)
{
    vga_buffer   = (volatile uint16_t *)P2V(VGA_PHYS);
```

Writing the constant `0xB8000` here is a page fault — in `vga_init`, which runs before there is any
way to *print* a fault. The machine simply stops.

The same applies to the multiboot info pointer (Chapter 11, §6.2), the initrd module address
(Chapter 40), and anything else the firmware or bootloader handed us as a number.

The discipline that prevents it is naming the two kinds of address differently:

```c
typedef uint32_t            paddr_t;
typedef uint32_t            vaddr_t;
```

They are the same type and the compiler will not stop you mixing them. The names are for the reader,
and every function signature in the memory code says which it wants.

---

## 6. Running it

```
=== Nimbus starting ===
```

If that line appears, the higher half worked. It is printed by `serial_init` + `serial_puts` from C
code running at `0xC01xxxxx`, which means: the page directory was right, paging came on, the absolute
jump landed high, the identity map was removed without killing us, `.bss` was cleared, and the stack
was valid enough for `call kmain`.

### 6.1 Confirming the addresses

```c
    kprintf("kmain is at      %p\n", (void *)kmain);
    kprintf("stack is around  %p\n", &magic);
    kprintf("kernel phys      %08x - %08x\n",
            (uint32_t)__kernel_phys_start, (uint32_t)__kernel_phys_end);
```

```
kmain is at      0xc0104a20
stack is around  0xc010cfe0
kernel phys      00100000 - 0010f000
```

Code and stack above 3 GiB; the physical image at 1 MiB. Both true at once, which is the whole point
of the chapter.

### 6.2 In GDB

```bash
make debug
```
```
$ i686-elf-gdb bin/nimbus.elf
(gdb) target remote :1234
(gdb) break *0x00101000
(gdb) continue
(gdb) info registers eip
eip  0x101000
(gdb) stepi 8
(gdb) info registers eip
eip  0xc0103012
```

Watching `EIP` jump from `0x101000` to `0xc0103012` across the `jmp ecx` is the clearest possible
demonstration, and it takes a minute.

---

## 7. What could go wrong

| Symptom | Cause |
|---|---|
| Triple fault at `mov cr0` | Directory not 4 KiB aligned, or `CR3` given a wrong address |
| Triple fault at the `jmp` | Higher-half entries missing or wrong |
| Triple fault just after the jump | Identity map removed too early, or removed via the wrong address |
| Faults later, in `paging_init` | `BOOT_PAGES` too small |
| `readelf` shows VirtAddr == PhysAddr everywhere | `AT()` missing from `link.ld` |
| QEMU: "Invalid multiboot header" | `.multiboot.data` is not in the first 8 KiB |
| Works, then faults in `vga_init` | Missing `P2V` on a physical constant |
| Null pointer writes do not fault | Identity map not removed |

The first three are all "instant reboot with no output", which is why `-no-reboot -d int` is the tool
(Chapter 47).

---

## 8. Exercises

🟢 **25.1** Change `jmp ecx` to `jmp higher_half` and boot. Explain the failure precisely.

🟢 **25.2** Comment out the four `mov dword [...], 0` lines and then write to address 0 from
`kmain`. What happens, and what *should* happen?

🟢 **25.3** Make the mistake on purpose: change `vga_init` to use `0xB8000` directly. The machine
will stop with no output. Now find it with `-d int`. This is worth doing once, because you will
recognise the symptom instantly thereafter.

🟡 **25.4** Remove `AT()` from one section in `link.ld` and look at `readelf -l`. Then boot it.

🟡 **25.5** Change `KERNEL_VIRTUAL_BASE` to `0x80000000` (a 2 GiB/2 GiB split). You will need to
change it in `paging.h`, `link.ld` and `boot.asm`, and `KERNEL_PAGE_NUMBER` follows. Confirm it
boots, then work out what it costs the user half and what it buys the direct map.

🟡 **25.6** Set `BOOT_PAGES` to 8 and confirm from `readelf -x .multiboot.data` that eight entries
are present at each end of the directory.

🔴 **25.7** Make the kernel position-independent instead: remove the two-section split, write the
early code with explicit `- KERNEL_VIRTUAL_BASE` on every symbol, and get it booting. Count how many
subtractions you needed and decide which approach you prefer.

🔴 **25.8** Reclaim the low sections. Once `kmain` is running, `.multiboot.text` is dead code
occupying a page of physical memory. Add symbols around it and free those frames in `pmm_init`.

---

## What we covered

- Why the kernel lives at 3 GiB: cheap syscalls, direct access to user memory, and interrupt handlers
  that work in any address space.
- The problem: linked for one address, loaded at another, executing in between.
- Three solutions, and why the linker-section split is the one with no fudge factor to forget.
- LMA versus VMA, `AT()`, and the `readelf -l` check that confirms them.
- `. +=` rather than `. =`, so growing the early code cannot create a silent overlap.
- `ECX` as the only scratch register, because `EAX` and `EBX` carry the boot information.
- A page directory assembled at build time with `%rep`, why 4 MiB pages, why 4 KiB alignment, and why
  16 MiB rather than 4.
- The three instructions between paging-on and the jump, which run only because of the identity map.
- `lea` + `jmp ecx` rather than a relative jump — the most important instruction in the file.
- Removing the identity map through the high window, and why leaving it makes null pointer bugs
  silent.
- The `P2V` discipline that follows, and the failure that stops the machine before it can print.

[Chapter 26](26-page-faults.md) makes the page fault handler do useful work: growing stacks, demand
paging, guard pages, and telling an expected fault from a bug.

---

[← Enabling paging](24-enabling-paging.md) · [Contents](README.md) · [Next: Page faults →](26-page-faults.md)
