# Chapter 48 — Where to go next

[← Debugging](47-debugging.md) · [Contents](README.md)

---

## 1. What you have

Two operating systems, about 9,600 lines, and a fairly complete map of a small computer.

**Spark** is 600 lines: a boot sector you wrote, a second stage, the A20 gate, a GDT, `CR0.PE`, and a
C kernel printing to video memory. It does nothing useful and it means nothing about booting a
computer is mysterious to you.

**Nimbus** is the rest:

| | |
|---|---|
| Boot | Multiboot, a higher-half kernel, paging on before the first C instruction |
| Console | VGA with scrolling and a hardware cursor; a serial log that survives a crash |
| CPU tables | GDT with ring 3 segments and a TSS; a 256-entry IDT |
| Interrupts | 32 exceptions, the 8259s remapped, 16 IRQ lines, spurious detection |
| Time | PIT at 100 Hz, uptime, preemption |
| Input | PS/2 scancodes, modifiers, a ring buffer, a line discipline |
| Memory | A bitmap frame allocator, two-level paging, a direct map, `kmalloc` with coalescing |
| Faults | Stack growth, guard pages, and killing the right thing |
| Processes | Task structs, a fourteen-instruction context switch, round robin with priorities |
| Userland | Ring 3, `int 0x80`, validated pointers, `fork`/`exec`/`exit`/`wait` |
| Blocking | Wait channels, sleep, semaphores, mutexes |
| Storage | ATA PIO, MBR partitions, a VFS, a tar initrd, read/write FAT16 |
| Files | Three-level descriptors, `dup2`, pipes |
| Programs | An ELF loader, a C library, a shell, and six utilities |

More importantly, you have the *shape*. When you read about a kernel feature now, you know where it
would go and roughly what it would cost.

---

## 2. The eight things Nimbus deliberately lacks

Each of these was named in the chapter it belongs to. Here they are together, with what each would
actually take.

### 2.1 x86-64 and long mode

**What changes:** four levels of page tables instead of two, 64-bit entries, `NX` at bit 63, a
different calling convention (arguments in registers), `syscall`/`sysret` instead of `int 0x80`, no
segmentation to speak of, and `iret` replaced by `sysret` for the fast path.

**What does not:** every idea in Part III and IV. The page table walk is the same operation with more
steps.

**The work:** a 32-bit stub that sets up an initial page table, enables PAE and `EFER.LME`, and
long-jumps into 64-bit code. Then a rewrite of every assembly file, every `uint32_t` that is really
an address, and the syscall convention.

**A weekend for the boot path, a week for the rest.** The single largest payoff is `NX`, which you
cannot have on plain 32-bit paging (Chapter 44, §5.2).

### 2.2 SMP

**What changes:** everything in Chapter 36.

Start with the honest statement from §7 of that chapter:

> making this kernel SMP-safe is not a matter of adding locks. It is a matter of re-examining every
> global, every read-modify-write, and every assumption that "interrupts off" means "alone".

The mechanics:

1. **Find the other CPUs.** Parse the ACPI MADT (or the older MP tables) — a few hundred lines of
   table walking.
2. **Start them.** Each application processor boots in *real mode* at a physical address you specify,
   via an INIT-SIPI-SIPI sequence through the local APIC. So you need a trampoline in low memory that
   re-does Chapters 7 and 25 for each core.
3. **Replace the PIC with the APIC.** Per-CPU local APICs for timers and IPIs, an I/O APIC for device
   routing.
4. **Per-CPU data.** `current_task` becomes per-CPU, reached through `GS` — which is exactly what
   Chapter 16, §6.2 warned about when it saved only `DS`.
5. **Real spinlocks.** Chapter 36, §4.1: three lines in one file.
6. **TLB shootdowns.** Chapter 23, §5.4: an IPI to every CPU that might hold a stale entry, and wait
   for acknowledgement.
7. **Audit every global.** This is the actual work.

**A month**, and the first two weeks are the ACPI parser.

### 2.3 A network stack

An entire second book, genuinely.

The layers: a driver (Intel e1000 is the friendliest), Ethernet framing, ARP, IPv4, ICMP, UDP, TCP,
and then DNS and something that uses it.

TCP alone is: sequence numbers, the three-way handshake, retransmission with exponential backoff,
window management, congestion control, the TIME_WAIT state, and Nagle's algorithm. RFC 793 plus
thirty years of amendments.

**The good news:** a driver plus Ethernet plus ARP plus ICMP is a weekend, and at the end the machine
answers `ping`. That is a genuinely satisfying milestone and it needs none of TCP.

The e1000 needs PCI enumeration first — §2.8.

### 2.4 USB

The most complicated interface on a modern PC, by a wide margin.

Three host controller standards (UHCI, OHCI, EHCI) plus XHCI for USB 3, each a complete driver. Then
the USB protocol layer: enumeration, descriptors, endpoints, four transfer types. Then a class driver
per device type — HID for keyboards, mass storage for disks.

**The minimum useful thing** is XHCI plus HID, which gets you a keyboard on a machine with no PS/2
port. That is a large fraction of modern laptops, so it is the difference between "runs on real
hardware" and "runs on hardware from 2012".

**A month**, and it is the least rewarding month on this list.

### 2.5 A journaling filesystem

Chapter 42, §7 laid out the six ways a FAT volume can be corrupted by a power cut.

A journal fixes all six: write intentions to a log, commit, then perform. After a crash, replay or
discard.

**The work:** a reserved log region, a transaction API (`begin`, `log_write`, `commit`), routing
every metadata write through it, and replay at mount. About 400 lines on top of FAT16, and it is one
of the most instructive things on this list because the hard part is deciding *which* writes need to
be in the journal.

Alternatively, implement ext2 — no journal, but inodes, indirect blocks, and a much better design to
read. Then ext3 is ext2 plus a journal, which is exactly the exercise above.

### 2.6 Signals

Chapter 46, §10.1. **The single largest missing feature in Nimbus.**

- A pending mask and a handler table per process.
- Delivery on the return path from a syscall or interrupt.
- A signal frame built on the user stack.
- `sigreturn` to unwind it.
- Default actions: terminate, ignore, stop, continue.
- Process groups and a controlling terminal, so Ctrl-C reaches the foreground job.

**About 300 lines**, and it unlocks job control, proper Ctrl-C, `SIGPIPE`, `SIGSEGV` handlers, and
`alarm`.

Do this one first. Everything else in userland wants it.

### 2.7 Dynamic linking

Chapter 44, §10.

The kernel's part is small: notice `PT_INTERP`, load *that* instead, and jump to it with the original
program's headers on the stack via the auxiliary vector.

The rest is a userland program — the dynamic linker — that maps libraries, resolves symbols through
the `.dynamic` section, fills in the GOT, and finally jumps to the real entry point.

**Worth doing for the understanding**, less so for the benefit: static linking is faster to start and
simpler, and for a system with nine programs there is nothing to share.

### 2.8 PCI

Not on the original list, and it is the prerequisite for most of the others.

Enumeration is genuinely easy: two I/O ports (`0xCF8`/`0xCFC`), a bus/device/function address, and a
256-byte configuration space per function. Walking every bus and printing vendor and device IDs is
**about 80 lines** and is the single best return on effort in this chapter.

From there: base address registers tell you where a device's registers are, and the interrupt line
tells you which IRQ it uses. That is what a real ATA DMA driver, an e1000, or an AHCI (SATA) driver
all need first.

**Do this before §2.3 or the DMA exercise in Chapter 37.**

---

## 3. Six projects, in order of ratio

If you want one thing to do next, the ordering by value over effort:

**1. PCI enumeration (80 lines, an evening).** §2.8. Unlocks everything else, and `lspci`-style
output from your own kernel is a good moment.

**2. The block cache (Chapter 38, §4.1, an evening).** 64 buffers with LRU. Turns 68 sector writes
into 6 (Chapter 42, §8.2), and the measurement before and after is immediate.

**3. Signals (300 lines, a weekend).** §2.6. The biggest userland unlock.

**4. Copy-on-write fork (Chapter 28, exercise 28.7, a weekend).** A per-frame refcount and a fault
handler case. Measurable with `rdtsc`, and it is how `fork` is supposed to work.

**5. A driver for something real (a weekend each).** e1000 and ping. Or AHCI, for a disk interface
that is not from 1986. Or XHCI+HID if you want to boot a modern laptop.

**6. x86-64 (a week).** §2.1. Everything you know transfers; the payoff is `NX` and a modern
architecture.

---

## 4. Two harder directions

### 4.1 A different architecture

**RISC-V** is the obvious one. The privileged specification is 100 pages against Intel's 5,000; there
are three privilege modes with a clean design; the page table format is Sv39, three levels, and much
more regular than x86's; and QEMU's `virt` machine is well documented.

Porting Nimbus is: a new boot path (SBI rather than multiboot), a new trap mechanism (`stvec`,
`scause`, `sepc` instead of an IDT), new page tables, a new context switch, and new drivers
(virtio rather than ATA). The scheduler, the VFS, FAT16, the heap and the ELF loader are unchanged.

**That is the point.** The split between "architecture" and "policy" in this codebase is not
theoretical, and a port is how you find out where you got it wrong.

**ARM** — a Raspberry Pi — is the other option, and is more work because the peripherals are less
documented.

### 4.2 A different kernel structure

Nimbus is a **monolithic** kernel: drivers and filesystems run in ring 0, in the kernel's address
space.

A **microkernel** puts them in userland processes and communicates by message passing. The kernel
provides only address spaces, threads, and IPC.

The tradeoff is real and the arguments have been going on since 1992:

- A driver bug kills one process instead of the machine.
- Components can be restarted.
- IPC costs more than a function call — which is what the debate is actually about.

L4 showed the IPC cost can be a few hundred cycles, and seL4 is formally verified. It is a genuinely
different way to think about the same problem, and implementing one after this book is the clearest
way to understand what a kernel *is*.

---

## 5. What to read

**Specifications, which you have been using:**

- *Intel SDM Volume 3* (System Programming Guide). Appendix F explains how to navigate it.
- *ATA/ATAPI Command Set*, for §2.8's DMA.
- The Multiboot specification. Twenty pages.
- *Microsoft FAT32 File System Specification*. Thirty-four pages and it covers FAT12/16/32.
- The ELF specification (the System V gABI). Sixty pages.

**Books:**

- Tanenbaum, *Modern Operating Systems*. The standard textbook. Read the chapters corresponding to
  the parts you have just written.
- Bovet and Cesati, *Understanding the Linux Kernel*. Old (2.6) and still the clearest guide to how a
  real kernel is put together.
- *The Design and Implementation of the FreeBSD Operating System*. A different tradition, and worth
  reading for the contrast.
- Lions, *Commentary on UNIX 6th Edition*. The whole kernel, annotated, in 9,000 lines — about the
  size of Nimbus, and written in 1976.

**Source:**

- **xv6** (MIT). A teaching kernel in about 6,000 lines, with a book. The closest thing to Nimbus in
  the world, done differently. Read it after this — the differences are the interesting part.
- **Linux 0.11**. Linus's kernel from 1991, about 10,000 lines. Recognisable as Linux, small enough
  to read entirely.
- **Plan 9**. What Unix's designers did next. Every resource is a file served over a protocol, which
  makes the VFS idea general in a way Unix never did.
- **seL4**. A formally verified microkernel. Not light reading, and the proof is the point.

**Communities:**

- The [OSDev wiki](https://wiki.osdev.org). The reference everyone uses. Accurate on the hardware,
  occasionally out of date on the toolchain.
- `/r/osdev`, and the OSDev forums. Read the "beginner mistakes" threads.

---

## 6. The thing that is actually worth having

Not the code. The code is 9,600 lines and much of it is a simplified version of something better.

What is worth having is that **a computer is no longer opaque**.

When your laptop pauses, you know it might be a page fault reading from disk, and you know what the
handler is doing. When a program segfaults, you know exactly which bit of which page table entry was
clear. When `fork` is slow, you know it is copying page tables and you know what copy-on-write would
change. When a filesystem corrupts after a power cut, you know which of six windows it was in.

That is not knowledge you can get from reading, and it is not knowledge that goes away.

The distance between "an operating system is magic" and "an operating system is about ten thousand
lines of ordinary code, most of which I have now written" is the whole of this book.

---

## 7. A last exercise

🔴 **48.1** Boot Nimbus on real hardware.

Chapter 47, §9. Build the ISO, write it to a USB stick, and boot a machine with a PS/2 port or a
USB keyboard that the firmware emulates as PS/2.

It will not work the first time. §9.2 of that chapter lists ten reasons, and every one of them works
in QEMU.

When it does work, there is a prompt on a screen, driven by a kernel you wrote, on a machine that has
no other software on it at all.

That is worth doing.

---

[← Debugging](47-debugging.md) · [Contents](README.md)
