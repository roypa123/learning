# Operating Systems From Scratch

### Writing a real, bootable operating system in C and x86 assembly, with nothing hidden

This project is a complete course that teaches you what an **operating system** actually is —
not as a diagram in a textbook, but as a pile of bytes that the CPU starts executing at
`0000:7C00` and that ends up running a shell you typed yourself.

We write everything. No Linux kernel, no GRUB (except as an optional convenience), no libc, no
runtime, no framework. The bootloader is ours. The interrupt handlers are ours. The memory
allocator is ours. The filesystem driver is ours. The C library that userland programs link
against is ours. Every byte that ends up in the disk image came from code in this repository
that you will read, understand and be able to change.

We build **two** operating systems.

**Spark** — the basic OS (Part I). About 600 lines. A 512-byte boot sector that you wrote,
which loads a second stage, enables the A20 line, installs a GDT, switches the CPU from 16-bit
real mode into 32-bit protected mode, loads a kernel off the disk and jumps into C code that
prints to the screen by writing directly to video memory at `0xB8000`. It does nothing useful.
It is the most important 600 lines in the book, because after it nothing about booting is magic
any more.

**Nimbus** — the medium OS (Parts II–VI). About 9,000 lines. A real little kernel:

```
nimbus> ls /
bin  dev  etc  home
nimbus> cat /etc/motd
Welcome to Nimbus.
nimbus> ps
 PID  STATE    NAME
   0  running  idle
   1  ready    init
   4  running  sh
nimbus> hexdump /bin/echo | head
00000000  7F 45 4C 46 01 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
```

Everything in that transcript is ours: the keyboard driver that read the characters, the VGA
driver that drew them, the line editor, the `fork`/`exec` that started `ls`, the syscall
interface it used, the virtual filesystem it walked, the FAT16 driver that read the directory,
the ATA driver that pulled the sectors off the emulated disk, the page tables that gave `ls`
its own address space, and the scheduler that switched away from the shell while it ran.

---

## What Nimbus has when you finish

| Subsystem | What we build |
|---|---|
| Boot | Our own two-stage bootloader, **and** a Multiboot header so `qemu -kernel` works |
| Console | VGA text mode driver with scrolling and a hardware cursor; COM1 serial log |
| CPU tables | GDT with user segments, a TSS, a 256-entry IDT |
| Interrupts | All 32 CPU exceptions, 8259 PIC remap, 16 IRQ lines, a handler registry |
| Timers | PIT at 100 Hz, uptime, `sleep()`, preemption |
| Input | PS/2 keyboard: scancode set 1, modifiers, a keymap, a line-editing console |
| Physical memory | Bitmap frame allocator over the Multiboot/E820 memory map |
| Virtual memory | Paging, a higher-half kernel at `0xC0000000`, per-process address spaces |
| Faults | A real page-fault handler: demand paging, stack growth, guard pages, segfaults |
| Heap | `kmalloc`/`kfree`: a first-fit block allocator with splitting and coalescing |
| Processes | Task structs, kernel stacks, a hand-written context switch, `fork`, `exec`, `exit`, `wait` |
| Scheduling | Round robin with priorities, an idle task, blocking and wait queues |
| Usermode | Ring 3, the TSS `esp0` dance, `iret` into userland |
| Syscalls | `int 0x80`, a syscall table, safe copying across the user/kernel boundary |
| Locking | Spinlocks, mutexes, semaphores, and the race conditions that motivate each |
| Storage | ATA PIO driver, MBR partition parsing, a block cache |
| Filesystems | A VFS layer, a tar-based initrd, and a read/write FAT16 driver |
| Files | File descriptors, `open`/`read`/`write`/`close`/`seek`, pipes, `dup2` |
| Userland | ELF loader, a small C library, a shell, and `ls cat echo hexdump ps sleep` |

---

## The book

The book lives in [`docs/`](docs/). Start at **[docs/README.md](docs/README.md)** for the full
table of contents, or jump straight to **[Chapter 0 — Introduction](docs/00-introduction.md)**.

Forty-seven chapters, roughly 500 pages. The structure of every chapter is the same:

1. **Theory first.** What is the hardware actually doing? What does the Intel manual say? What
   problem does this mechanism exist to solve, and what did people do before it existed?
2. **The code, in blocks.** Every block of code in this repository is reproduced in the book and
   explained — what each line does, why it is written that way, what breaks if you change it,
   and which bit of the theory it corresponds to.
3. **Run it.** Every chapter ends with a build you can run in QEMU and a description of what you
   should see. If it does not look like that, the chapter tells you how to find out why.
4. **Exercises**, from five-minute tweaks to "add a feature the book deliberately left out".

The [`docs/line-by-line/`](docs/line-by-line/) folder holds a second, denser pass: every source
file in the repository annotated line by line, for when you want the reference rather than the
narrative.

---

## The layout

```
operating_system/
├── README.md              <- you are here
├── docs/                  <- THE BOOK: start with docs/README.md
│   └── line-by-line/      <- every source file explained line by line
├── setup/                 <- toolchain install guide + helper scripts
├── spark/                 <- Part I: the basic OS (boot sector -> C kernel)
│   ├── boot/              <- boot.asm (stage 1), stage2.asm (stage 2)
│   ├── kernel/            <- the freestanding C kernel
│   └── link.ld            <- the linker script
├── nimbus/                <- Parts II-VI: the medium OS
│   ├── boot/              <- multiboot header, entry, low-level asm
│   ├── kernel/            <- gdt, idt, irq, timer, task, sched, syscall, main
│   ├── drivers/           <- vga, serial, keyboard, ata
│   ├── mm/                <- pmm, paging, heap, vmm
│   ├── fs/                <- vfs, initrd, fat16, pipe
│   ├── lib/               <- string/printf/etc shared by kernel and userland
│   ├── user/              <- userland: libc, sh, ls, cat, echo, hexdump
│   ├── include/nimbus/    <- all headers
│   └── link.ld
├── tools/                 <- host-side tools (mkimage, mkinitrd) in portable C
├── build.bat              <- build everything on Windows
├── run.bat                <- build + boot in QEMU
└── Makefile               <- the same, for make users
```

---

## Getting started

You need three tools: **NASM** (assembler), an **i686-elf cross-compiler** (GCC that targets bare
metal rather than Windows), and **QEMU** (the PC emulator you will boot into). None of them are
installed by default.

[Chapter 1 — Setting up the toolchain](docs/01-setup.md) walks through the install on Windows
step by step, explains *why* a cross-compiler is not optional, and ends with a five-line assembly
program that boots in QEMU so you know the whole chain works before you write anything real.

Once the tools are on your `PATH`:

```bat
build spark          :: build the basic OS
run   spark          :: boot it in QEMU

build nimbus         :: build the full OS + userland + disk image
run   nimbus         :: boot it in QEMU
run   nimbus debug   :: boot it stopped, waiting for GDB on :1234
```

---

## Who this is for

You should be comfortable reading C. You do **not** need to know assembly — Chapter 3 teaches the
subset of x86 we use — and you do not need to have written a kernel, a driver, or anything that
runs without an operating system underneath it.

If you have done the [`compiler_design`](../compiler_design) course, this is the natural sequel:
there you built the thing that produces programs, here you build the thing that runs them.

---

## A promise and a warning

**The promise:** nothing in this book is hand-waved. Where the book says "the CPU pushes EFLAGS,
CS and EIP", it will also show you the stack layout, the exact `iret` that consumes it, and a GDB
session where you watch it happen.

**The warning:** operating systems fail differently from normal programs. There is no exception
message, no stack trace and nobody to catch your error — a wrong bit in a descriptor reboots the
machine in a loop with no output whatsoever. Chapter 45 is entirely about debugging this, and it
is worth skimming early. You will need it.
