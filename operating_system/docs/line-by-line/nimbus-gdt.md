# Line by line: `kernel/gdt.c` and `boot/cpu.asm` (GDT part)

[Index](README.md) · [Chapter 15](../15-gdt.md)

---

## Storage

```c
static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_pointer;
static tss_entry_t tss;
```
⚠️ Plain globals in `.bss`, because the CPU reads the GDT on **every segment register load** for the
entire life of the machine. It cannot be on a stack or in the heap.

⚠️ And it must be **writable**: the CPU sets the Accessed bit (bit 0 of the access byte) the first
time a descriptor is loaded. A GDT in a read-only page faults on the first segment load.

---

## `gdt_set_entry`

```c
    gdt[index].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[index].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].base_high   = (uint8_t)((base >> 24) & 0xFF);
```
A 32-bit base in three fields at offsets 2, 4 and 7. The 286's descriptors were six bytes with a
24-bit base; the 386 bolted the extra byte onto the end rather than redesigning.

```c
    gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[index].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[index].granularity |= (uint8_t)(flags & 0xF0);
```
A 20-bit limit in a 16-bit field plus the low nibble of a byte that otherwise holds flags.

Assigned then OR'd, in two statements, to make it obvious the byte holds two unrelated things.

⚠️ The `& 0x0F` and `& 0xF0` masks are not decoration. A flags byte with low bits set would corrupt
the limit, producing a segment whose size is *nearly* right.

---

## The descriptors

```c
    gdt_set_entry(0, 0, 0, 0, 0);
```
Required to be all zeros. Makes selector 0 invalid, so a segment register left uninitialised — which
contains 0 — faults rather than addressing something plausible.

```c
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_EXEC | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);
```
Kernel code. `0x9A` access, `0xCF` granularity.

`GDT_GRAN_4K` with limit `0xFFFFF` gives `0xFFFFF × 4096 + 4095` = exactly 4 GiB.

⚠️ `GDT_SIZE_32` (`D/B`) must be set. Clear, the CPU treats the segment as 16-bit code and every
32-bit instruction needs a prefix — which means it executes something else entirely.

```c
    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);
```
Kernel data. `GDT_EXEC` absent.

⚠️ Bit 2 (`GDT_DIRECTION`) means two different things. For data it is "expand-down" — the limit
becomes a *lower* bound. For code it is "conforming" — callable from a lower privilege level, and
running at the caller's privilege. Setting either by accident is a real problem; both are clear here.

```c
    gdt_set_entry(3, ... GDT_DPL3 ... GDT_EXEC | GDT_RW, ...);
    gdt_set_entry(4, ... GDT_DPL3 ... GDT_RW, ...);
```
User code and data. **Identical to the kernel's except for two bits.**

All four describe the same 4 GiB. The segments are not there to divide memory; they are there to
carry the privilege level.

⚠️ The user segments span the kernel's higher half. A user program can *name* `0xC0100000`. What
stops it reading is `PTE_USER`, not segmentation.

---

## The TSS

```c
    memset(&tss, 0, sizeof(tss));
    tss.ss0  = SEL_KDATA;
    tss.esp0 = 0;
```
104 bytes, of which we use two fields. `esp0` is filled in per task by the scheduler.

```c
    tss.iomap_base = sizeof(tss_entry_t);
```
⚠️ Pointing the I/O permission bitmap **past the end** of the structure means "no bitmap", which makes
every `in` and `out` from ring 3 a general protection fault.

That is the behaviour we want: a user program that can write to port `0x64` can reboot the machine.

```c
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss_entry_t) - 1,
                  GDT_PRESENT | GDT_DPL3 | 0x09,
                  0x00);
```
A **system** descriptor — note `GDT_SEGMENT` is absent, so `S = 0` — and the type field means
something different: `0x9` is "available 32-bit TSS".

⚠️ `G = 0`, so the limit is in bytes, and unlike the flat segments it is **enforced**: a limit below
103 causes an invalid-TSS exception.

DPL 3 is what every kernel uses. It controls who may load the TSS with a far jump (hardware task
switching, which we never do); it is not checked when the CPU reads `esp0`. DPL 0 also works.

---

## Installing

```c
    gdt_pointer.limit = (uint16_t)(sizeof(gdt) - 1);
```
⚠️ **Minus one.** Six descriptors is 48 bytes, so the limit is 47. Writing 48 creates a seventh
descriptor made of whatever follows the array.

```c
    gdt_pointer.base  = (uint32_t)&gdt;
```
`lgdt` takes a **linear** address, and with flat segments linear equals virtual. Correct here; would
need `paging_virt_to_phys` if the GDT were ever outside the kernel's direct-mapped window.

⚠️ Note this differs from Spark's, where paging was off and the value was physical. Same-looking
line, different meaning.

```c
    gdt_flush(&gdt_pointer);
    tss_flush();
```

---

## `gdt_flush` (cpu.asm)

```nasm
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
```
`[esp]` is the return address; `[esp+4]` is argument 1.

⚠️ `lgdt` loads the register and changes **nothing else**. The CPU keeps using descriptors cached
when the segment registers were last loaded — from the *old* table, which we are about to stop
maintaining.

```nasm
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
```
Five reloads. Forget `SS` and the next `push` faults.

```nasm
    jmp 0x08:.reload_cs
.reload_cs:
    ret
```
`CS` is the one that cannot be assigned — there is no `mov cs, ax`. A far jump to the next
instruction is the idiom, and it also flushes the pipeline.

⚠️ The `jmp` is far but the `ret` is **near**: it pops four bytes, the return address `gdt_flush` was
called with. Correct because a far *jump* pushes nothing. A far *call* would need `retf`.

---

## `tss_flush`

```nasm
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret
```
`0x2B` is descriptor 5 (`0x28`) with RPL 3.

The comment in the source:

> RPL 3 looks wrong and is not. The CPU checks the *descriptor's* DPL when deciding whether ring 3
> may use a gate; the RPL in the selector we load here is not checked against anything, and the value
> 0x2B is what every kernel uses.

---

## `tss_set_kernel_stack`

```c
void tss_set_kernel_stack(uint32_t esp0)
{
    tss.ss0  = SEL_KDATA;
    tss.esp0 = esp0;
}
```
Two stores, called on **every context switch**.

⚠️ There is one TSS for the whole machine. When a process makes a system call the CPU reads `esp0`
and pushes its trap frame there. If nobody updated it between tasks, process B's frame lands on
process A's kernel stack, over A's saved registers while A is blocked mid-syscall.

The crash then happens in whichever returns second, in a function that did nothing wrong.

`ss0` is re-set every time even though it never changes — two stores instead of one, and one fewer
invariant to maintain.

---

## Verifying

```
(qemu) info registers
GDT=     c0104060 0000002f
CS =0008 00000000 ffffffff 00cf9a00 DPL=0 CS32 [-R-]
SS =0010 00000000 ffffffff 00cf9200 DPL=0 DS   [-WA]
TR =002b c01040a0 00000067 00008900 DPL=0 TSS32-avail
```

- `0000002f` = 47 = 48 − 1 ✓
- `CS` base 0, limit `ffffffff` ✓
- `00cf9a00`: `9a` is the access byte, `cf` the granularity ✓
- `TR` limit `0x67` = 103 = 104 − 1 ✓

Thirty seconds, and it confirms the whole file.

---

[Index](README.md) · [Chapter 15](../15-gdt.md)
