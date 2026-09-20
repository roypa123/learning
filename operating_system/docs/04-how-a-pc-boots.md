# Chapter 4 — How a PC boots

[← Assembly](03-assembly-crash-course.md) · [Contents](README.md) · [Next: The boot sector →](05-boot-sector.md)

---

## Goal

Trace every step between pressing the power button and the first instruction of code you wrote. By
the end you will know exactly what state the machine is in when our boot sector starts, why it is in
that state, and what the firmware has and has not done for us.

This is the last chapter before we write an operating system.

---

## 1. Power on: the reset vector

The instant the power supply asserts *power good*, the CPU comes out of reset in a defined state:

| Register | Value | Meaning |
|---|---|---|
| `CS` | `0xF000` | code segment |
| `EIP` | `0x0000FFF0` | instruction pointer |
| `CS` base (hidden) | `0xFFFF0000` | **not** `0xF000 × 16` |
| `CR0` | `0x60000000` | `PE` clear: real mode |
| `EFLAGS` | `0x00000002` | interrupts disabled |
| everything else | undefined | genuinely undefined |

The third row is the strange one. In real mode the base should be `CS × 16 = 0x000F0000`, and the
first instruction would be at `0x000FFFF0`. It is not. The CPU starts with a hidden base of
`0xFFFF0000`, so the first instruction is fetched from **`0xFFFFFFF0`** — sixteen bytes below the top
of the 4 GiB address space.

This is called the reset vector, and the reason for it is a compatibility problem in reverse. The
8086 fetched its first instruction from `0xFFFF0`, the top of *its* 1 MiB space. When the 286 and
386 widened the address bus, the firmware ROM moved to the top of the larger space — but the CPU
still has to start in real mode for compatibility. So Intel starts it with a segment base that real
mode could never produce, and the first far jump the firmware executes loads a normal `CS` and the
oddity disappears forever.

Sixteen bytes is not much room. What lives there is a far jump into the real firmware:

```nasm
    jmp far 0xF000:0xE05B       ; five bytes, and then padding
```

---

## 2. What the firmware does

The sixteen bytes at the reset vector belong to a ROM chip on the motherboard. What it does next
takes a second or two and is, in rough order:

**1. Bring up the CPU.** Microcode updates, cache configuration, and on a multi-core machine,
parking every core but one. The other cores sit in a halt loop until software wakes them — which is
why an SMP kernel has to send them a specific interrupt sequence to start them (Chapter 48).

**2. Initialise memory.** DRAM is not usable at power-on. The firmware reads the SPD EEPROM on each
DIMM, works out the timings, trains the memory controller, and runs a short test. Until this is
finished there is no RAM at all — early firmware runs out of cache configured as RAM, a mode with the
excellent name "cache-as-RAM".

**3. Enumerate and initialise devices.** Walk the PCI bus, assign resources, run option ROMs. The
video card's ROM is what puts the display into text mode 3 (80×25 colour) and installs the `int 0x10`
handler; the disk controller's installs `int 0x13`. This is why our boot sector can print and read
sectors without knowing anything about the hardware.

**4. Build the tables software will need.** The interrupt vector table at `0x0000`, the BIOS data
area at `0x0400`, the E820 memory map that `int 0x15` will report, and on modern machines the ACPI
tables.

**5. Find something to boot.**

---

## 3. Finding something to boot

The firmware works through its configured boot order, and for each device it does the same thing:

1. Read the first sector — 512 bytes, LBA 0, also called the Master Boot Record.
2. Check that the last two bytes are `0x55` then `0xAA`.
3. If they are, copy the 512 bytes to physical address `0x00007C00` and jump to it.
4. If not, move to the next device.

That is the entire contract. Three facts, no negotiation:

- **512 bytes.** Not 513. Everything your boot sector does must fit, minus the two signature bytes
  and any strings.
- **`0x7C00`.** Fixed since the IBM PC 5150 in 1981.
- **`0x55 0xAA` at offset 510.** Stored little-endian, hence `dw 0xAA55` in the source and
  `55 AA` on the disk.

### Why 0x7C00?

The number looks arbitrary and is not. The original IBM PC had a 32 KiB minimum configuration —
`0x0000`–`0x7FFF`. IBM wanted the boot sector as high as possible, to leave the maximum contiguous
free space below it, but needed room above it for the sector itself (512 bytes) plus a stack and some
scratch (another 512). `0x8000 - 0x0200 - 0x0200 = 0x7C00`.

A decision about a 32 KiB machine, made in 1981, that every PC on earth still honours.

### What state we start in

This is the important part of the chapter, because it is the precondition for every line of
Chapter 5:

| Thing | State |
|---|---|
| CPU mode | 16-bit real mode |
| `CS:IP` | points at `0x7C00`, but may be `0000:7C00` **or** `07C0:0000` |
| `DL` | the drive number we were loaded from. `0x00` = floppy, `0x80` = first hard disk |
| `DS`, `ES`, `SS`, `SP` | **undefined.** Not zero. Undefined. |
| `EFLAGS.IF` | usually set, but not guaranteed |
| `EFLAGS.DF` | not guaranteed clear |
| A20 gate | disabled, so memory above 1 MiB wraps |
| Interrupt vector table | valid, at `0x0000` |
| Everything above 1 MiB | unreachable |
| Everything above 512 bytes of our program | not loaded yet |

Three of those rows are why [`boot.asm`](../spark/boot/boot.asm) starts the way it does:

```nasm
start:
    jmp 0x0000:.canonical       ; force CS = 0, because it might be 0x07C0
.canonical:
    cli                         ; because IF might be set and SS:SP is garbage
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; a stack, growing down away from our code
    sti
    cld                         ; because DF might be set and print uses lodsb
    mov [boot_drive], dl        ; before anything clobbers DL
```

Every one of those lines is defending against a row in that table. The far jump is not superstition —
some BIOSes really do jump as `07C0:0000`, and `[ORG 0x7C00]` produces wrong addresses if they do.

---

## 4. BIOS services

Until we leave real mode, the firmware is a library. The calls we use:

### `int 0x10` — video

```nasm
    mov ah, 0x0E        ; function: teletype output
    mov al, 'X'         ; the character
    mov bh, 0           ; page number
    int 0x10
```

Function `0x0E` prints one character at the cursor and advances it, handling scrolling, `\r` and
`\n`. It is the entire user interface of a boot sector.

### `int 0x13` — disk

Two functions matter.

**`AH = 0x02`, read sectors in CHS mode:**

| Register | In |
|---|---|
| `AL` | sector count |
| `CH` | cylinder (low 8 bits) |
| `CL` | sector (bits 0–5), cylinder high bits (6–7) |
| `DH` | head |
| `DL` | drive |
| `ES:BX` | destination buffer |

Returns with `CF` clear on success, and — importantly — `AL` holding the number of sectors *actually*
read. A short read does not set `CF`, which is why [`boot.asm`](../spark/boot/boot.asm) checks:

```nasm
.verify:
    cmp al, [dr_count]
    jne disk_error
```

A loader that only checks `CF` will happily jump into a half-loaded kernel.

**`AH = 0x42`, extended read using LBA.** Takes a "disk address packet" in `DS:SI` and speaks flat
sector numbers with no geometry. Cleaner, and available on any hard disk since about 1996 — but not
on floppies, which is why Spark uses CHS.

### `int 0x15, AX = 0xE820` — the memory map

Returns a list of memory regions, one per call, with a type: available, reserved, ACPI-reclaimable,
or bad. It is the only reliable way to learn how much RAM exists and which parts you may use.

Nimbus does not call it, because our bootloader is QEMU's multiboot loader and multiboot hands us the
same information in a structure — see [`multiboot.h`](../nimbus/include/nimbus/multiboot.h). But it
is the same data from the same source, and Chapter 21 covers the format.

### `int 0x15, AX = 0x2401` — enable A20

One of the three ways to open the A20 gate. Chapter 7 covers all three and why we try them in order.

### The catch

**Every one of these stops working the moment we set `CR0.PE`.** The interrupt vector table at
`0x0000` is meaningless in protected mode, and the BIOS routines are 16-bit code that assumes
real-mode segmentation.

So the rule for any bootloader: **collect everything you need from the firmware before switching
modes.** That is why [`stage2.asm`](../spark/boot/stage2.asm) loads the kernel off the disk *first*
and enters protected mode *last*. Get the order wrong and you have a 32-bit kernel with no way to
read the disk it came from.

---

## 5. Why two stages?

512 bytes is not enough. Our stage 1 is already 400 bytes for something that only prints two strings
and reads four sectors, and it cannot do any of:

- enable A20 (three methods, ~100 bytes)
- build a GDT (24 bytes of table plus the code)
- parse a partition table or a filesystem
- load a kernel larger than the remaining free space in one segment
- print a useful error message

So the division of labour is:

**Stage 1** (512 bytes, LBA 0): set up a known CPU state, read stage 2 off the disk, jump to it.
Nothing else.

**Stage 2** (2 KiB here, unlimited in principle): everything hard. Load the kernel, enable A20,
build a GDT, switch to protected mode, jump to the kernel.

Real bootloaders do the same thing with more stages. GRUB has three: a 512-byte `boot.img`, a
`core.img` of about 30 KiB that understands filesystems, and then the modules it loads from `/boot`.
The reason is always the same — the first stage has no room to be clever, so its only job is to load
something that does.

---

## 6. UEFI, and what we are pretending

Everything above describes BIOS booting. On any machine made after roughly 2012 the firmware is UEFI,
and UEFI does something quite different:

| | BIOS | UEFI |
|---|---|---|
| Finds the bootloader by | reading sector 0 | reading a file from a FAT32 partition |
| Bootloader format | 512 raw bytes | a PE32+ executable, like a `.exe` |
| CPU mode at handover | 16-bit real mode | 64-bit long mode, paging already on |
| Services | interrupt-based (`int 0x10`) | a table of function pointers |
| Memory map | `int 0x15 E820` | `GetMemoryMap()` |
| Partition scheme | MBR, 4 partitions, 2 TiB limit | GPT, 128 partitions, 8 ZiB |

UEFI is, honestly, better in almost every respect. It hands you a 64-bit machine with a working
memory map and a console, and the bootloader is a normal C program.

**Our code runs because of the Compatibility Support Module** — a UEFI component that emulates a BIOS
— and because QEMU defaults to SeaBIOS, a genuine BIOS implementation. Both are increasingly
disabled by default on real hardware; Intel announced the end of CSM support in 2020.

So why teach BIOS booting?

**Because the point is not to write a bootloader.** The point is to see, with nothing hidden, how a
machine goes from executing nothing to executing your code, and to meet real mode, segmentation, the
A20 gate and the mode switch — all of which are still there underneath UEFI, and all of which appear
in the Intel manual you will be reading for the rest of the book.

A UEFI version of Chapter 5 would be: write a C program, link it as PE32+, call
`gBS->AllocatePages`, call `GetMemoryMap`, call `ExitBootServices`, jump to your kernel. It is
shorter and it teaches almost nothing about the machine.

Chapter 48 sketches what a UEFI port of Nimbus would involve. It is a weekend, and it is a good
weekend, once the kernel exists.

---

## 7. The whole sequence, end to end

Putting it together, here is every step from power to our C code, with the chapter that covers each:

```
  power good
      |
  CPU resets: real mode, CS:EIP -> 0xFFFFFFF0
      |
  firmware: memory training, PCI, option ROMs, IVT, E820        <- this chapter
      |
  firmware reads LBA 0, checks 0x55AA, copies to 0x7C00, jumps
      |
  [our code starts here]
      |
  stage 1: canonical CS, segments, stack, save DL               <- Chapter 5
  stage 1: int 0x13 reads 4 sectors to 0x7E00                   <- Chapter 6
  stage 1: jmp 0x0000:0x7E00
      |
  stage 2: int 0x13 reads 64 sectors to 0x10000                 <- Chapter 8
  stage 2: enable A20 (BIOS, then port 0x92, then the 8042)     <- Chapter 7
  stage 2: lgdt, set CR0.PE, far jump                           <- Chapter 7
      |
  [32-bit protected mode]
      |
  stage 2: reload segment registers, set ESP
  stage 2: rep movsd, 0x10000 -> 0x100000
  stage 2: jmp 0x100000
      |
  entry.asm: zero .bss, set ESP, zero EBP, call kmain           <- Chapter 9
      |
  kmain: write to 0xB8000                                        <- Chapter 10
```

Fourteen steps. Six of them are ours, and the next six chapters are one step each.

---

## 8. What Nimbus does instead

Worth stating now so the difference is clear when we get there.

Nimbus does **not** use our bootloader. It is a multiboot kernel, loaded by QEMU's `-kernel` flag or
by GRUB, and the handover is:

- the CPU is already in 32-bit protected mode
- A20 is already enabled
- a flat GDT is already installed
- paging is off, interrupts are off
- `EAX` = `0x2BADB002`, `EBX` = the address of an info structure with the memory map in it

In other words, the bootloader has already done everything Chapters 5–8 do. That is deliberate:
Spark exists to teach you that work, and repeating it for Nimbus would buy nothing but a longer
build. Chapter 11 covers the multiboot contract, and Chapter 48 explains how to make Nimbus boot
from our own bootloader if you want the satisfaction — it is about forty lines.

---

## 9. Exercises

🟢 **4.1** Why does the CPU start with a hidden `CS` base of `0xFFFF0000` rather than
`CS × 16`? What would break if Intel had used the normal calculation?

🟢 **4.2** The boot signature is written `dw 0xAA55` but appears on disk as `55 AA`. Explain, and
say which byte is at offset 510.

🟡 **4.3** List every register whose value is guaranteed when the boot sector starts. Then list
three that are commonly assumed to be zero and are not. For each, name the line of
[`boot.asm`](../spark/boot/boot.asm) that defends against it.

🟡 **4.4** Boot `hello.img` from Chapter 1 with `-d int -D log.txt` and find, in the log, the
`int 0x13` call in which SeaBIOS reads the boot sector. What LBA does it read, and where does it put
it?

🟡 **4.5** Explain why a bootloader must read the disk before entering protected mode, and describe
what a bootloader that got the order wrong would experience.

🔴 **4.6** Read the first three pages of the UEFI specification's "Boot Manager" chapter, and write
down the three ways UEFI's handover state differs from BIOS's in a way that would change
`entry.asm`.

---

## What we covered

- The reset vector at `0xFFFFFFF0`, and the hidden segment base that real mode could not produce.
- What firmware does in the two seconds before your code: memory training, PCI, option ROMs, the
  IVT, E820.
- The three-clause contract: 512 bytes, `0x7C00`, `0x55 0xAA` — and why `0x7C00` is what it is.
- The exact machine state at handover, including the three registers people assume are zero.
- The BIOS services we use, and the fact that all of them die the instant we set `CR0.PE`.
- Why two stages, and what real bootloaders do about the same constraint.
- How UEFI differs, why we teach BIOS anyway, and what we are relying on to make it work.

[Chapter 5](05-boot-sector.md) writes the 512 bytes. Every byte of them.

---

[← Assembly](03-assembly-crash-course.md) · [Contents](README.md) · [Next: The boot sector →](05-boot-sector.md)
