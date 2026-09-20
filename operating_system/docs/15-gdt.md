# Chapter 15 — The GDT, properly

[← printf](14-printf.md) · [Contents](README.md) · [Next: The IDT and exceptions →](16-idt-exceptions.md)

> 📖 **Line by line:** [gdt.c / cpu.asm](line-by-line/nimbus-gdt.md)

---

## Goal

Build the descriptor table Nimbus will use for the rest of its life. Six descriptors: null, kernel
code, kernel data, user code, user data, and a TSS.

Chapter 7 built a two-descriptor GDT good enough to enter protected mode. This one adds the two
things that make userland possible, and it is worth understanding fully because a wrong bit here
produces a triple fault at an unpredictable moment weeks later.

---

## 1. Why replace the bootloader's table

The multiboot loader gave us a flat, correct GDT (Chapter 11, §4). Three reasons to throw it away:

**We do not know where it is.** The specification says it may be anywhere and may be reclaimed. The
frame allocator (Chapter 22) will start handing out physical memory, and if one of those frames
contains the live GDT the machine triple-faults the next time any segment register is loaded — which
might be a second later or a minute later.

**It has no ring 3 descriptors.** Userland is impossible without a code segment whose DPL is 3.

**It has no TSS.** Taking an interrupt *from* ring 3 is impossible without one, because the CPU has
nowhere to look for a kernel stack.

So `gdt_init()` is the second thing `kmain` does after output, and it must happen before `idt_init`,
because an IDT gate names a code segment *in the GDT*.

---

## 2. A descriptor, bit by bit

```c
typedef struct gdt_entry {
    uint16_t limit_low;      /* limit bits 0..15                              */
    uint16_t base_low;       /* base  bits 0..15                              */
    uint8_t  base_mid;       /* base  bits 16..23                             */
    uint8_t  access;         /* P | DPL(2) | S | Type(4)                      */
    uint8_t  granularity;    /* G | D/B | L | AVL | limit bits 16..19         */
    uint8_t  base_high;      /* base  bits 24..31                             */
} PACKED gdt_entry_t;
```

Eight bytes. `PACKED` is mandatory — without it GCC pads `access` and `granularity` out to alignment
boundaries and the CPU reads eight bytes of something else entirely.

The layout is interleaved for the reason Chapter 7, §2.2 gave: the 80286's descriptors were six
bytes with a 24-bit base and 16-bit limit, and the 386 bolted the extra byte of base onto the end and
stuffed four more limit bits into a spare nibble.

### 2.1 Setting one

```c
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[index].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[index].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[index].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[index].granularity |= (uint8_t)(flags & 0xF0);

    gdt[index].access      = access;
}
```

Note that `granularity` is assigned and then OR'd. The first assignment puts the limit's top nibble
in the low four bits; the second adds the flags in the top four. Writing it as one expression would
work; writing it as two makes it obvious that the byte holds two unrelated things.

The `& 0x0F` and `& 0xF0` masks are not decoration. A caller passing a flags byte with low bits set
would corrupt the limit, and the failure would be a segment whose limit is nearly right.

### 2.2 The access byte

```c
#define GDT_PRESENT     0x80    /* P: this descriptor is valid                */
#define GDT_DPL0        0x00    /* descriptor privilege level 0 (kernel)      */
#define GDT_DPL3        0x60    /* descriptor privilege level 3 (user)        */
#define GDT_SEGMENT     0x10    /* S=1: a code or data segment, not a gate    */
#define GDT_EXEC        0x08    /* code segment (executable)                  */
#define GDT_DIRECTION   0x04    /* data: expand-down. code: conforming.       */
#define GDT_RW          0x02    /* code: readable. data: writable.            */
#define GDT_ACCESSED    0x01    /* the CPU sets this; we leave it clear       */
```

Two of these deserve more than a comment.

**Bit 2, `GDT_DIRECTION`, means two different things.**

For a *data* segment it is "expand-down": the limit becomes a *lower* bound rather than an upper one,
so the segment covers everything from `limit` to `0xFFFFFFFF`. This exists for stack segments — a
stack that grows down wants to grow into the segment, not out of it. Nobody has used it since flat
memory models became universal, and setting it by accident produces a segment covering the memory you
did *not* want.

For a *code* segment it is "conforming": code in a conforming segment can be called from a *lower*
privilege level and continues to run at the caller's privilege. That is a mechanism for shared
library code in a segmented system, and it is a security hazard in any other context. Ours are
non-conforming.

**Bit 0, `GDT_ACCESSED`, is set by the CPU**, the first time the descriptor is loaded. We leave it
clear. It exists so that a segment-swapping OS could tell which descriptors had been used recently —
the segmentation-era equivalent of the page table Accessed bit.

It matters in one practical way: **the GDT must be writable memory.** If you place it in a read-only
page (which Chapter 24 does for `.rodata`), the first segment load faults trying to set this bit.
Ours is in `.bss`, which is writable.

### 2.3 The granularity byte

```c
#define GDT_GRAN_4K     0x80    /* G: the limit counts 4 KiB pages, not bytes */
#define GDT_SIZE_32     0x40    /* D/B: 32-bit default operand size           */
#define GDT_LONG_MODE   0x20    /* L: 64-bit code. Not us.                    */
```

`G = 1` with limit `0xFFFFF` gives `0xFFFFF × 4096 + 4095 = 0xFFFFFFFF` — exactly 4 GiB.

`D/B = 1` makes 32-bit the default operand and address size in this segment. With it clear, the CPU
treats the segment as 16-bit code and every 32-bit instruction needs a prefix byte — which means
running 32-bit code in a `D = 0` segment executes something completely different.

`L = 1` is 64-bit mode, and `L` and `D` must not both be set. Not our problem.

---

## 3. The six descriptors

```c
void gdt_init(void)
{
    gdt_set_entry(0, 0, 0, 0, 0);

    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_EXEC | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL0 | GDT_SEGMENT | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL3 | GDT_SEGMENT | GDT_EXEC | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);

    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_DPL3 | GDT_SEGMENT | GDT_RW,
                  GDT_GRAN_4K | GDT_SIZE_32);
    ...
}
```

All four real segments describe **the same 4 GiB**. Base 0, limit 4 GiB, flat.

### 3.1 So why four?

Because the privilege level lives in the descriptor.

Ring 3 code must run with a `CS` whose DPL is 3. Ring 0 code must run with a `CS` whose DPL is 0. The
CPU checks this on every `iret` and every far transfer. So we need two code descriptors and two data
descriptors that are identical except for two bits.

**The segments are not there to divide memory. They are there to carry two bits.**

### 3.2 The user segments span the kernel too

This surprises people and it is correct.

Our ring 3 descriptors have base 0 and limit 4 GiB, which includes the kernel's higher half at
`0xC0000000`. A user program can *name* kernel addresses.

What stops it reading them is **paging**, not segmentation: the `PTE_USER` bit is clear on every
kernel page (Chapter 24, §3). That is the wall.

The alternative — shrinking the user segment limit to `0xC0000000` — is the 1990s answer. It works,
it is one more bit of defence, and it breaks the moment you want a higher-half kernel that user code
must be able to *call into* via a gate, or a shared page mapped at a high address. Every modern
kernel uses flat user segments and relies on paging.

Chapter 32, §5 revisits this with the concrete attack in mind.

---

## 4. The TSS

```c
typedef struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;       /* <-- the ring 0 stack pointer. This one matters.   */
    uint32_t ss0;        /* <-- the ring 0 stack segment. So does this.       */
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} PACKED tss_entry_t;
```

104 bytes, of which we use eight.

### 4.1 What it was for

The 386 designed the Task State Segment for **hardware task switching**. One instruction — a far jump
to a TSS selector — would save every register into the current TSS and load them from another.
Process switching in one opcode.

Nobody uses it. Two reasons:

**It is slower than doing it by hand.** A hardware task switch saves and restores all 104 bytes
unconditionally. Our `switch_context` (Chapter 30) saves five registers, because the calling
convention already handled the rest. Measurements on a 486 put the hardware switch at roughly 300
cycles against about 50 for the software version.

**It cannot be extended.** The TSS has no room for FPU, SSE or AVX state, which is where most of the
per-task register state lives on a modern CPU. And x86-64 removed hardware task switching entirely.

### 4.2 What it is for now

Two fields.

**`esp0` and `ss0`.** When the CPU takes an interrupt while running in ring 3, it must switch to a
ring 0 stack — the user stack cannot be trusted and may not even be mapped. The only place it will
look for that stack's address is `ss0:esp0` in the current TSS.

Without a TSS, an interrupt from ring 3 has nowhere to push its frame, and the result is a double
fault, then a triple fault.

**`iomap_base`.**

```c
    tss.iomap_base = sizeof(tss_entry_t);
```

The TSS can carry an I/O permission bitmap — one bit per port — allowing a user process to access
specific ports. Setting the base to the size of the structure means "the bitmap is past the end",
which the CPU interprets as "no permissions".

So every `in` and `out` from ring 3 raises a general protection fault. That is what we want: a user
program that can write to port `0x64` can reboot the machine, and one that can write to `0x3F2` can
issue disk commands.

### 4.3 The descriptor

```c
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss_entry_t) - 1,
                  GDT_PRESENT | GDT_DPL3 | 0x09,
                  0x00);
```

A **system** descriptor, not a segment descriptor. `S = 0` — note that `GDT_SEGMENT` is absent — and
the type field means something different: `0x9` is "available 32-bit TSS".

The base is the address of our one TSS. The limit is its size minus one, and unlike the flat segments
this limit is *enforced*: a TSS limit smaller than 103 causes an invalid-TSS exception.

`G = 0`, so the limit is in bytes.

**DPL 3** looks wrong. It is not, and the reason is worth getting right: the DPL of a TSS descriptor
controls who may load it with a far jump (hardware task switching, which we never do) — it is not
checked when the CPU reads `esp0` on an interrupt. Setting it to 3 is what every kernel does and what
every tutorial copies; DPL 0 also works on real hardware and in QEMU.

### 4.4 Loading it

```nasm
tss_flush:
    mov ax, 0x2B                   ; SEL_TSS, with RPL 3 in the low bits
    ltr ax
    ret
```

`ltr` loads the Task Register. Selector `0x2B` is descriptor 5 (`0x28`) with RPL 3.

The comment in [`cpu.asm`](../nimbus/boot/cpu.asm) is honest about this:

> RPL 3 looks wrong and is not. [...] The value 0x2B is what every kernel uses because the TSS
> descriptor has DPL 3 so that `iret` to ring 3 is legal. Setting it to 0x28 also works on real
> hardware and on QEMU.

---

## 5. `esp0` and the bug you get without it

```c
void tss_set_kernel_stack(uint32_t esp0)
{
    tss.ss0  = SEL_KDATA;
    tss.esp0 = esp0;
}
```

Called from the scheduler on every switch:

```c
    tss_set_kernel_stack(next->kernel_stack);
```

Here is why it matters, concretely.

There is **one TSS for the whole machine**. When process A makes a system call, the CPU reads `esp0`
and pushes A's trap frame there. When the scheduler switches to process B and B makes a system call,
the CPU reads `esp0` again — and if nobody updated it, it is still pointing at A's kernel stack.

So B's trap frame lands on top of A's. A is blocked in the middle of a system call with its saved
registers on that stack. When A is next scheduled and returns from its call, it restores registers
that B overwrote.

The symptom: two processes that both make system calls corrupt each other, and the crash happens in
whichever one returns second, in a function that did nothing wrong. It is an excellent bug and it
takes a long time to find.

One line in the scheduler, and Chapter 31 puts it there.

---

## 6. Installing the table

```c
    gdt_pointer.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_pointer.base  = (uint32_t)&gdt;

    gdt_flush(&gdt_pointer);
    tss_flush();
```

> ⚠️ **The limit is size minus one.** Six descriptors is 48 bytes, so the limit is 47. Writing 48
> creates a seventh descriptor made of whatever follows the array, and the fault it eventually causes
> points anywhere but at this line.

```nasm
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10                   ; SEL_KDATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.reload_cs            ; SEL_KCODE
.reload_cs:
    ret
```

`lgdt` loads the register and changes nothing else. The CPU keeps using the descriptors it cached
when the segment registers were last loaded — from the *old* table, which we are about to stop
maintaining. Until every segment register is reloaded, we are running on a GDT that may no longer
exist.

Five `mov`s and a far jump. `CS` is the one that cannot be assigned (Chapter 2, §2.4), so the far
jump to the very next instruction is the idiom.

### 6.1 The `ret` after a far jump

```nasm
    jmp 0x08:.reload_cs
.reload_cs:
    ret
```

The `jmp` is far but the `ret` is near — it pops four bytes, the return address `gdt_flush` was
called with.

That is correct because a far *jump* pushes nothing. A far *call* would push both `CS` and `EIP` and
need a `retf`. Mixing them up produces a return to a garbage address, which is the kind of bug that
teaches you to read the encoding rather than the mnemonic.

### 6.2 The address is virtual

```c
    gdt_pointer.base  = (uint32_t)&gdt;
```

`&gdt` is a kernel virtual address — something like `0xC0104060`, because `gdt` is in `.bss` and
Nimbus runs in the higher half.

`lgdt` takes a *linear* address, and with our flat segments linear equals virtual, so this is
correct. Paging is already on and the higher half is mapped, so the CPU can walk to it.

This is worth checking rather than assuming, because Chapter 7's GDT base was a *physical* address
(paging was off) and the same-looking line means something different there. If you ever move the GDT
out of the kernel's direct-mapped window, this line needs `paging_virt_to_phys`.

---

## 7. Verifying it

```c
    LOG_INFO("gdt: %u descriptors at %p, tss at %p",
             (uint32_t)GDT_ENTRIES, (void *)gdt, (void *)&tss);
```

```
[    0.000] inf  gdt: 6 descriptors at 0xc0104060, tss at 0xc01040a0
```

And in the QEMU monitor (Ctrl-Alt-2):

```
(qemu) info registers
...
GDT=     c0104060 0000002f
IDT=     c0104100 000007ff
CS =0008 00000000 ffffffff 00cf9a00 DPL=0 CS32 [-R-]
SS =0010 00000000 ffffffff 00cf9200 DPL=0 DS   [-WA]
TR =002b c01040a0 00000067 00008900 DPL=0 TSS32-avail
```

Read that carefully, because it confirms almost everything in this chapter:

- `GDT= c0104060 0000002f` — base matches the log, limit is `0x2f` = 47 = 48 − 1. ✓
- `CS =0008 00000000 ffffffff` — selector 8, base 0, limit 4 GiB. ✓
- `00cf9a00` is the raw descriptor's upper dword: `9a` is the access byte
  (`10011010` — present, DPL 0, segment, exec, readable) and `cf` is granularity
  (`11001111` — G, D, limit high nibble). ✓
- `TR =002b c01040a0 00000067` — the task register holds `0x2B`, base matches the TSS in the log,
  and the limit is `0x67` = 103 = 104 − 1. ✓

`info registers` after any change to this file is a thirty-second check that catches most mistakes.

---

## 8. What could go wrong

| Symptom | Cause |
|---|---|
| Triple fault inside `gdt_flush` | The base or limit is wrong; the far jump found nothing valid |
| Triple fault on the first `push` after | `SS` not reloaded, or the data descriptor is malformed |
| Faults only when userland starts | `SEL_UCODE`/`SEL_UDATA` wrong, or RPL bits missing |
| Double fault on the first syscall | No TSS loaded, or `esp0` is zero |
| Two processes corrupt each other | `tss_set_kernel_stack` not called on switch |
| Works until memory pressure | Still using the bootloader's GDT and it got reclaimed |
| Fault the first time a segment loads | The GDT is in read-only memory; the CPU cannot set the Accessed bit |

---

## 9. Exercises

🟢 **9.1** Change the GDT limit to `sizeof(gdt)` instead of `sizeof(gdt) - 1` and boot. Explain why
it still works, and construct a case where it would not.

🟢 **9.2** Set `GDT_DPL3` on the *kernel* code descriptor and boot. Where does it fail, and why not
immediately?

🟡 **9.3** Add a seventh descriptor: a data segment with base `0xC0000000` and limit 1 GiB. Load it
into `GS` and access `[gs:0]`. What address does that reach, and what does it contain?

🟡 **9.4** Decode `00cf9200` from the `info registers` output above, bit by bit, and confirm every
field against §2.

🟡 **9.5** Remove `mov ss, ax` from `gdt_flush` and find the exact instruction that faults. Use
`-d int` to identify it.

🔴 **9.6** Implement a working I/O permission bitmap: extend the TSS with a 8192-byte bitmap, set
`iomap_base` correctly, and clear the bits for ports `0x3F8`–`0x3FF`. Then write a user program that
does `outb(0x3F8, 'X')` and watch it work — a user process driving a serial port directly, with no
system call.

---

## What we covered

- Three reasons the bootloader's GDT has to go, one of which is a bug that appears minutes later.
- A descriptor bit by bit: the interleaved layout, the two meanings of bit 2, and the Accessed bit
  that forces the table into writable memory.
- Four flat segments that all describe the same 4 GiB, and why: they exist to carry two privilege
  bits, not to divide memory.
- Why user segments span the kernel, and what actually stops a user program reading it.
- The TSS: 104 bytes for hardware task switching nobody uses, two fields that make userland possible,
  and an `iomap_base` that denies port access by being out of range.
- `esp0`, and the two-process corruption bug you get without updating it on every switch.
- `lgdt` changes nothing until the segment registers are reloaded, and `CS` needs a far jump.
- Reading `info registers` to verify base, limit, access byte and TSS in thirty seconds.

[Chapter 16](16-idt-exceptions.md) builds the interrupt descriptor table and handles the thirty-two
exceptions the CPU can raise — which is when the kernel finally gets to see its own mistakes.

---

[← printf](14-printf.md) · [Contents](README.md) · [Next: The IDT and exceptions →](16-idt-exceptions.md)
