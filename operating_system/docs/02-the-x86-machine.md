# Chapter 2 — The x86 machine

[← Setup](01-setup.md) · [Contents](README.md) · [Next: Assembly crash course →](03-assembly-crash-course.md)

---

## Goal

Understand the machine well enough to program it directly: what registers exist, how an address is
computed, what the two address spaces are, and what "real mode" means. This chapter is pure theory —
no code compiles, nothing runs — and every subsequent chapter depends on it.

The x86 is a strange architecture and its strangeness is not random. Nearly all of it is the
accumulated cost of forty-five years of never breaking anything. Knowing *which* decade a piece of
weirdness comes from is genuinely useful, because it tells you whether to look for a reason or stop
looking.

---

## 1. A very short history, because it explains everything

| Year | Chip | What it added | What we still live with |
|---|---|---|---|
| 1978 | 8086 | 16-bit, 20 address lines, segment registers | Real mode; the boot process starts here |
| 1982 | 80286 | Protected mode, 24 address lines, descriptors | The GDT; A20; descriptor layout |
| 1985 | 80386 | 32-bit, paging, 4 GiB | Everything in this book |
| 1993 | Pentium | 4 MiB pages, TSC, APIC | `boot.asm`'s page directory |
| 2003 | Opteron | 64-bit long mode | Not used here — see Chapter 48 |

Every machine made since 1985 still boots as an 8086. Your laptop, at power-on, is a 16-bit
processor that can address one megabyte of memory. It will remain one until some software — our
bootloader, in Chapter 7 — explicitly asks it to become a 386. That is not a metaphor; it is
literally the state of the silicon.

The reason is the one that explains almost all x86 weirdness: **IBM shipped a PC in 1981, software
was written for it, and nobody has been willing to break that software since.** Not Intel, not
Microsoft, not AMD. The compatibility is so complete that a genuine MS-DOS 1.0 floppy will boot on a
2024 machine with a compatibility module enabled.

---

## 2. Registers

### 2.1 The general-purpose eight

```
 31                   16 15      8 7       0
+-----------------------+---------+---------+
|                       |   AH    |   AL    |  AX     EAX
+-----------------------+---------+---------+
|                       |   BH    |   BL    |  BX     EBX
+-----------------------+---------+---------+
|                       |   CH    |   CL    |  CX     ECX
+-----------------------+---------+---------+
|                       |   DH    |   DL    |  DX     EDX
+-----------------------+---------+---------+
|                       |        SI         |         ESI
|                       |        DI         |         EDI
|                       |        BP         |         EBP
|                       |        SP         |         ESP
+-----------------------+-------------------+
```

Eight registers. On a 386 and later each is 32 bits, named `EAX` through `ESP`; the low 16 bits of
each are addressable as `AX` through `SP`, and the two halves of the first four are addressable as
`AH`/`AL` and so on.

They overlap. Writing `AL` changes the low byte of `EAX` and leaves the other 24 bits alone. This is
used constantly in assembly and is a frequent source of bugs — a routine that returns a result in
`AL` leaves whatever was in the top of `EAX` untouched, so a caller that reads `EAX` gets garbage
in the high bits.

The names are historical and only loosely meaningful now:

| Register | Name | Still true? |
|---|---|---|
| `EAX` | Accumulator | Yes: many instructions have a shorter encoding using `EAX`, and it holds return values |
| `EBX` | Base | No: it was a memory base register on the 8086, now it is general |
| `ECX` | Counter | Yes: `loop`, `rep` and the shift instructions implicitly use `ECX` |
| `EDX` | Data | Partly: `mul` and `div` use `EDX:EAX` as a 64-bit pair, and `in`/`out` take the port in `DX` |
| `ESI` | Source Index | Yes: `lodsb`, `movsb`, `rep movsd` read from `[DS:ESI]` |
| `EDI` | Destination Index | Yes: those same instructions write to `[ES:EDI]` |
| `EBP` | Base Pointer | By convention: the frame pointer |
| `ESP` | Stack Pointer | Enforced by hardware: `push`, `pop`, `call`, `ret`, and interrupts all use it |

Only `ESP` is special to the hardware in a way you cannot opt out of. The rest are conventions —
strong ones, because the instruction encodings favour them.

### 2.2 EIP

The instruction pointer. You cannot read or write it directly: there is no `mov eax, eip`. It is
changed by `jmp`, `call`, `ret`, `int` and `iret`, and that is the complete list.

The standard trick to read it:

```nasm
    call .here
.here:
    pop eax          ; EAX now holds the address of .here
```

`call` pushes the return address, which is the address of the instruction after it. This matters in
Chapter 5 when position-independent boot code needs to know where it is.

### 2.3 EFLAGS

One register, thirty-two bits, most of them individually meaningful.

```
 31                    21 20 19 18 17 16 15 14 13-12 11 10  9  8  7  6  5  4  3  2  1  0
+------------------------+--+--+--+--+--+--+--+-----+--+--+--+--+--+--+--+--+--+--+--+--+
|        reserved        |ID|VP|VF|AC|VM|RF| 0| NT| IOPL |OF|DF|IF|TF|SF|ZF| 0|AF| 0|PF| 1|CF|
+------------------------+--+--+--+--+--+--+--+-----+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

The ones this book uses:

**`CF` (0), `ZF` (6), `SF` (7), `OF` (11)** — carry, zero, sign, overflow. Set by arithmetic and
tested by the conditional jumps. `cmp a, b` is a subtraction that discards the result and keeps the
flags, which is why `cmp` and `je` are always written as a pair.

**`DF` (10), direction.** Controls whether the string instructions count up or down. `cld` clears it
(up), `std` sets it (down). Leaving it set when a called routine expects it clear is a genuinely
nasty bug, which is why [`boot.asm`](../spark/boot/boot.asm) runs `cld` before doing anything else.

**`IF` (9), interrupt enable.** The single most consequential bit in the kernel. `sti` sets it,
`cli` clears it. While it is clear, maskable hardware interrupts are held pending — the timer does
not tick, the keyboard does not report, and any code that waits for either hangs forever. Chapter 36
is largely about this bit.

Ring 3 cannot change `IF`. `cli` from userland is a general protection fault, which is why
[`cpu.asm`](../nimbus/boot/cpu.asm)'s `enter_usermode` has to set it in the `EFLAGS` value it hands
to `iret` — that is the only place a user process can get interrupts enabled.

**`TF` (8), trap.** Set it and the CPU raises a debug exception after every single instruction. This
is how a single-stepping debugger works, and it is four lines to implement in our kernel (Chapter 47).

**`IOPL` (12–13).** The privilege level required to use `in` and `out`. We leave it at 0, so ring 3
cannot touch I/O ports.

`EFLAGS` cannot be moved to a register directly either. `pushfd` / `popfd` push and pop it, which is
what [`io.h`](../nimbus/include/nimbus/io.h)'s `irq_save` does:

```c
static ALWAYS_INLINE uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
```

Push the flags, pop them into a variable, then disable interrupts. The caller later restores exactly
what was there, which is how nested critical sections work without a counter.

### 2.4 Segment registers

Six of them: `CS`, `DS`, `ES`, `FS`, `GS`, `SS`. Sixteen bits each. What they *mean* changed
completely between real mode and protected mode, and §3 and §4 cover both.

`CS` is the code segment and is used implicitly for every instruction fetch. `SS` is the stack
segment, used implicitly by `push`, `pop` and anything addressed through `ESP` or `EBP`. `DS` is the
default for most data access. `ES` is the destination for string instructions. `FS` and `GS` have no
assigned use and modern kernels use them for per-CPU and per-thread data.

`CS` cannot be loaded with `mov`. The only instructions that change it are `jmp far`, `call far`,
`ret far` and `iret`. That restriction is why [`cpu.asm`](../nimbus/boot/cpu.asm)'s `gdt_flush` ends
with a far jump to the very next instruction, and why entering protected mode requires a far jump
rather than just setting a bit.

### 2.5 Control registers

Four that matter, and they are the kernel's main switches.

**`CR0`** — mode bits.

| Bit | Name | Meaning |
|---|---|---|
| 0 | `PE` | Protection Enable. 0 = real mode, 1 = protected mode. Set in Chapter 7. |
| 16 | `WP` | Write Protect. If 0, ring 0 may write to read-only pages. Set in Chapter 24. |
| 31 | `PG` | Paging. 0 = addresses are physical, 1 = addresses go through the MMU. Set in Chapter 24. |

**`CR2`** — on a page fault, the CPU writes the faulting *virtual address* here. It is the only place
that information exists, and a second fault overwrites it, which is why
[`paging.c`](../nimbus/mm/paging.c)'s handler reads it before doing anything that might fault.

**`CR3`** — the physical address of the current page directory. Writing it switches address spaces
and flushes the TLB. This is the register a context switch changes, and the reason the top 12 bits of
a page directory address must be zero (the low bits hold flags).

**`CR4`** — feature enables. We use bit 4, `PSE`, which allows 4 MiB pages —
[`boot.asm`](../nimbus/boot/boot.asm) sets it so that the early page directory needs no second level.

All four are readable and writable only in ring 0, only with `mov`, and only to or from a
general-purpose register. `mov eax, cr0` and `mov cr0, eax`, nothing else.

---

## 3. Real mode, and how an address used to be computed

The machine boots in real mode, and Spark's first two files run entirely inside it, so it is worth
understanding properly rather than as "the weird bit before the real work".

### 3.1 The problem Intel had in 1978

The 8086 was a 16-bit chip. A 16-bit register addresses 64 KiB. Intel wanted a megabyte, which needs
20 bits. The registers were 16 bits and were not going to get bigger.

Their answer was segmentation:

```
physical address = (segment register × 16) + offset
```

A segment register holds the *paragraph number* — the address divided by 16 — and the offset is added
to it. Both are 16 bits, and the sum can be up to 20 bits.

```
    CS = 0x1000, IP = 0x0250
    physical = 0x1000 × 16 + 0x0250
             = 0x10000 + 0x0250
             = 0x10250
```

Written as `1000:0250`. The colon notation is not decoration; the two halves genuinely are two
registers.

### 3.2 The consequences, all of which we hit

**Addresses are not unique.** `0000:7C00`, `07C0:0000` and `0700:0C00` are all physical `0x7C00`.
This is why [`boot.asm`](../spark/boot/boot.asm) starts with a far jump:

```nasm
    jmp 0x0000:.canonical
.canonical:
```

The BIOS may jump to the boot sector either way. `[ORG 0x7C00]` only generates correct addresses if
`CS` is 0, so the far jump forces the question closed rather than hoping.

**A single segment is 64 KiB.** Anything larger needs segment arithmetic. That is exactly what
[`stage2.asm`](../spark/boot/stage2.asm) does when loading the kernel:

```nasm
    mov dx, es
    add dx, 32          ; 512 bytes = 32 paragraphs
    mov es, dx
```

It advances the *segment* by 32 rather than adding 512 to `BX`, because `BX` is 16 bits and would
wrap after 64 KiB — silently writing sector 129 over sector 1.

**The address space wraps at 1 MiB.** The maximum is `0xFFFF × 16 + 0xFFFF = 0x10FFEF`, which needs
21 bits. The 8086 had 20 address lines, so the top bit was simply lost and the address wrapped to
`0x000000`–`0x00FFEF`.

Software relied on that wrap. When the 286 arrived with 24 address lines and stopped wrapping, that
software broke, so IBM added a gate on address line 20 that forces it to zero — and left it *closed*
at boot. That gate is the A20 line, it is still closed on your machine right now, and Chapter 7 is
partly about opening it.

### 3.3 The real-mode memory map

The first megabyte is not empty. This is what the BIOS leaves behind:

```
0x00000 - 0x003FF   1 KiB    Interrupt Vector Table: 256 entries of segment:offset
0x00400 - 0x004FF   256 B    BIOS Data Area: keyboard buffer, equipment word, ...
0x00500 - 0x07BFF   ~30 KiB  free — and where our stack goes, growing down from 0x7C00
0x07C00 - 0x07DFF   512 B    the boot sector. Our code.
0x07E00 - 0x9FFFF   ~608 KiB free — stage 2 goes at 0x7E00, the kernel at 0x10000
0xA0000 - 0xBFFFF   128 KiB  video memory. Text mode is at 0xB8000.
0xC0000 - 0xC7FFF   32 KiB   video BIOS ROM
0xC8000 - 0xEFFFF            option ROMs: network boot, RAID controllers
0xF0000 - 0xFFFFF   64 KiB   the system BIOS itself
```

Two things follow. First, "640 KB ought to be enough for anybody" was never said by Bill Gates, but
the 640 KiB is real — it is the gap between `0x00000` and `0xA0000`, and it is why DOS programs
fought over "conventional memory" for fifteen years.

Second, this is why a kernel is loaded at 1 MiB. Everything below it is a minefield of things you
must not overwrite, and above it there is nothing but RAM.

### 3.4 The interrupt vector table

At physical `0x0000`, 256 entries of four bytes each: a 16-bit offset and a 16-bit segment.
`int 0x10` reads the two words at `0x10 × 4 = 0x40` and far-jumps there.

The BIOS fills this table in before handing over, which is how our boot sector can call `int 0x10` to
print and `int 0x13` to read the disk without knowing where in ROM those routines live.

It is also why BIOS services stop working the moment we enter protected mode. In protected mode the
table at `0x0000` means nothing — the CPU consults the IDT instead — and the BIOS code itself is
16-bit and assumes real-mode segmentation. Chapter 7 covers what to do about that: gather everything
you need from the BIOS *before* the switch.

---

## 4. Protected mode: the same registers, different meaning

Setting `CR0.PE` changes what a segment register contains. It is no longer a paragraph number; it is
a **selector**, an index into a table of descriptors.

```
 15                                3  2  1  0
+------------------------------------+--+-----+
|            index                   |TI| RPL |
+------------------------------------+--+-----+
```

- **index** — which descriptor, as a byte offset with the low three bits used for the fields below
- **TI** — 0 = look in the GDT, 1 = look in the LDT (we never use an LDT)
- **RPL** — Requested Privilege Level, 0–3

So selector `0x08` is descriptor 1 at ring 0; selector `0x1B` is descriptor 3 (`0x18`) at ring 3.
That is why [`gdt.h`](../nimbus/include/nimbus/gdt.h)'s user selectors look odd:

```c
#define SEL_KCODE       0x08    /* ring 0 code,  descriptor 1 */
#define SEL_UCODE       0x1B    /* ring 3 code,  descriptor 3, RPL 3 */
```

Each descriptor is eight bytes describing a base address, a limit, and permissions. An address is
then:

```
linear address = descriptor.base + offset
```

with a fault if the offset exceeds `descriptor.limit` or the privilege check fails.

Chapter 15 builds the table properly. The summary result: we set every descriptor's base to 0 and
limit to 4 GiB, so `base + offset == offset` and segmentation does nothing. It is a "flat" model, and
every modern OS uses one.

Which raises the question the chapter has to answer: if segmentation does nothing, why have four
segments? Because the privilege level lives *in the descriptor*. Ring 3 code must run with a `CS`
whose DPL is 3. The segments are not there to divide memory. They are there to carry two bits.

---

## 5. Paging, in one page

After Chapter 24 there is a third translation step:

```
logical address  --[segmentation]-->  linear address  --[paging]-->  physical address
   (what your                           (what the MMU                  (what goes on
    code says)                           looks up)                      the bus)
```

With a flat GDT, the first arrow does nothing, so "linear" and "virtual" become the same word and
this book uses "virtual".

The MMU splits a 32-bit virtual address three ways:

```
 31          22 21          12 11              0
+--------------+--------------+----------------+
| directory ix |   table ix   |     offset     |
+--------------+--------------+----------------+
     10 bits        10 bits        12 bits
```

`CR3` points at a page directory of 1024 entries. Entry `directory ix` points at a page table of 1024
entries. Entry `table ix` points at a 4 KiB physical frame. Add the offset.

1024 × 1024 × 4096 = exactly 4 GiB, which is not a coincidence: the sizes were chosen so a directory
and a table are each exactly one page.

Part III is seven chapters on this. For now the thing to hold onto is that **after paging is on,
every address in your code is a lie that the MMU translates**, and the kernel is the only thing that
knows the truth.

---

## 6. The two address spaces

x86 has memory space *and* a separate 64 KiB I/O space, reachable only by the `in` and `out`
instructions. A pointer cannot reach it; there is no way to express it in C.

It exists because in 1978 address pins were expensive and Intel copied the 8080. Sixty-five thousand
addresses seemed like plenty for peripherals — and the addresses IBM chose in 1981 are still in use:

| Ports | Device | Chapter |
|---|---|---|
| `0x20`, `0x21` | Master PIC (interrupt controller) | 17 |
| `0x40`–`0x43` | PIT (timer) | 18 |
| `0x60`, `0x64` | PS/2 keyboard controller | 19 |
| `0x70`, `0x71` | CMOS / real-time clock | — |
| `0x1F0`–`0x1F7` | Primary ATA (disk) | 37 |
| `0x3D4`, `0x3D5` | VGA CRT controller | 12 |
| `0x3F8`–`0x3FF` | COM1 (serial) | 13 |
| `0xA0`, `0xA1` | Slave PIC | 17 |

Modern devices use memory-mapped I/O instead: the device's registers appear at physical addresses,
and you read and write them with ordinary `mov`. That is strictly better — a pointer works, the
compiler understands it, there is no separate 64 KiB limit — and the only reason port I/O survives is
that these eight chips are still emulated by every chipset for compatibility.

Two things MMIO requires that ordinary memory does not, both of which we hit:

**`volatile`.** The compiler must not cache a device register in a register or optimise away a
"pointless" repeated read. [`vga.c`](../nimbus/drivers/vga.c) declares its framebuffer pointer
`volatile` for exactly this reason.

**Cache disabling.** A device register that the CPU caches is a register you read once and then never
see change. That is what `PTE_NOCACHE` in [`paging.h`](../nimbus/include/nimbus/paging.h) is for.

---

## 7. The stack

`push`, `pop`, `call`, `ret` and every interrupt use `ESP`, and the stack **grows downwards** —
`push` subtracts 4 from `ESP` and then stores.

```
high addresses
    ...
    +------------------+  <- where ESP started
    | first thing      |
    +------------------+
    | second thing     |
    +------------------+  <- ESP now
    (free space)
    ...
low addresses
```

Three consequences worth internalising now:

**A stack overflow grows into whatever is below it.** There is no guard by default. In
[`boot.asm`](../spark/boot/boot.asm) the stack starts at `0x7C00` and grows down into 30 KiB of free
memory — deliberately, so that it can never reach the code at `0x7C00` itself. In Nimbus, each task
gets 8 KiB and Chapter 26 adds a guard page.

**A buffer on the stack, overflowed upwards, overwrites the return address.** That is the entire
mechanism of a stack smashing attack, and it works because the return address is at a higher address
than the local variables.

**`call` pushes, `ret` pops.** That is all a function call is at this level. It means you can fake
one: write an address to the stack and execute `ret`, and control goes there. [`cpu.asm`](../nimbus/boot/cpu.asm)'s
`switch_context` does precisely this — it `ret`s into a *different task*, and Chapter 30 is about why
that works.

---

## 8. Alignment, endianness, and other things that will bite

**Little-endian.** The value `0x12345678` stored at address 100 occupies bytes 100–103 as
`78 56 34 12`. The lowest byte comes first.

This is why the boot signature is written `dw 0xAA55` and appears on disk as `55 AA`, and why you
will see the number written both ways in documentation. It also means casting a `uint32_t*` to a
`uint8_t*` and reading `[0]` gives the *least* significant byte, which is occasionally useful and
frequently confusing.

**Unaligned access works, and is slower.** Unlike ARM, x86 lets you read a `uint32_t` from an odd
address. It costs an extra cycle or two, or a fault if `EFLAGS.AC` is set (it is not, by default).
This is why our `memcpy` checks alignment before taking the fast path.

**Structures the hardware defines must be `PACKED`.** A GDT descriptor is exactly eight bytes with
specific fields at specific offsets. Without `__attribute__((packed))`, GCC inserts padding for
alignment and the CPU reads garbage. [`types.h`](../nimbus/include/nimbus/types.h) has the macro and
a warning about not overusing it: packed structs produce slower code and pointers the compiler will
not trust.

---

## 9. Exercises

🟢 **2.1** Given `DS = 0x07C0` and `SI = 0x0010`, what physical address does `[DS:SI]` refer to?
Give another `segment:offset` pair that names the same byte.

🟢 **2.2** The boot sector's stack starts at `SS:SP = 0000:7C00`. After three `push ax` instructions,
what is `SP`, and what physical addresses hold the pushed data?

🟡 **2.3** Selector `0x23` is used for the user data segment. Which descriptor index is that, and
what privilege level does the selector request? Check your answer against
[`gdt.h`](../nimbus/include/nimbus/gdt.h).

🟡 **2.4** Write out the physical addresses that a real-mode program would reach for
`0xFFFF:0x0010` with A20 disabled and with A20 enabled. This is exactly the check
[`stage2.asm`](../spark/boot/stage2.asm)'s `check_a20` performs.

🟡 **2.5** In QEMU, run `hello.img` from Chapter 1 with `-monitor stdio`, then type
`info registers`. Identify `CS`, `EIP`, and the state of `IF` in `EFLAGS`. Explain why `IF` is set
even though we never executed `sti`.

🔴 **2.6** Read Intel SDM Volume 3, Chapter 2 ("System Architecture Overview"), sections 2.1 and 2.5.
It is fifteen pages and it is the authoritative version of this chapter. Appendix F explains how to
navigate the manuals without drowning.

---

## What we covered

- Every x86 still boots as a 16-bit 8086, and will stay one until software asks otherwise.
- The eight general-purpose registers, which of their conventional uses the hardware actually
  enforces, and why `EIP` and `EFLAGS` cannot be read directly.
- `EFLAGS.IF` as the kernel's most consequential bit, and why ring 3 cannot touch it.
- Real mode: `segment × 16 + offset`, the 1 MiB wrap, the A20 gate, and the first-megabyte map that
  explains why kernels live at 1 MiB.
- Protected mode: the same registers holding selectors into a descriptor table, and why we have four
  flat segments rather than one.
- Two address spaces, the port map we will be using, and what MMIO needs that memory does not.
- A downward-growing stack, and the fact that `ret` will go anywhere you tell it to.

[Chapter 3](03-assembly-crash-course.md) teaches the forty instructions this book uses, the NASM
syntax we write them in, and the calling convention that lets assembly and C call each other.

---

[← Setup](01-setup.md) · [Contents](README.md) · [Next: Assembly crash course →](03-assembly-crash-course.md)
