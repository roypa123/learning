# Operating Systems From Scratch — the book

Forty-seven chapters. Two operating systems. Nothing hidden.

Read it in order. Each chapter assumes the one before it, and the code in the repository is
written so that it builds and boots at the end of every single chapter — you are never looking
at half a subsystem that will only work three chapters later.

---

## Part 0 — Foundations

Before we can boot anything we need to agree on what an operating system is, get a toolchain that
can produce bare-metal binaries, and learn the small amount of x86 that the rest of the book
leans on.

| # | Chapter | What you get |
|---|---|---|
| 0 | [Introduction: what an operating system actually is](00-introduction.md) | The whole map: kernel vs. OS, the four hard problems, what we build and why |
| 1 | [Setting up the toolchain](01-setup.md) | NASM, an i686-elf cross-compiler, QEMU, and a first boot |
| 2 | [The x86 machine](02-the-x86-machine.md) | Registers, memory, the bus, real mode, segmentation, I/O ports |
| 3 | [x86 assembly for OS writers](03-assembly-crash-course.md) | The 40 instructions we actually use, calling conventions, NASM syntax |
| 4 | [How a PC boots](04-how-a-pc-boots.md) | Power, reset vector, the BIOS, the boot sector, and where UEFI fits |

## Part I — Spark: the basic operating system

Six chapters that take you from an empty file to a C kernel running in 32-bit protected mode on
a machine you booted yourself.

| # | Chapter | What you get |
|---|---|---|
| 5 | [The 512-byte boot sector](05-boot-sector.md) | `boot.asm`, every byte of it, and the `0xAA55` signature |
| 6 | [BIOS services: text and disk](06-bios-services.md) | `int 0x10`, `int 0x13`, printing and loading sectors in real mode |
| 7 | [A20, the GDT, and protected mode](07-protected-mode.md) | The three things that stand between you and 32 bits |
| 8 | [Stage 2: loading the kernel](08-stage2-loader.md) | A bigger loader, disk geometry, and handing over control |
| 9 | [Freestanding C and the linker script](09-freestanding-c.md) | `-ffreestanding`, `link.ld`, sections, why `main` is not special |
| 10 | [Hello, VGA — Spark is done](10-vga-hello.md) | `0xB8000`, attribute bytes, and the first thing you wrote appearing on screen |

## Part II — Talking to the machine

Nimbus starts here. By the end of this part the kernel has a console, a log, interrupts, a clock
and a keyboard — everything you need to *observe* the machine, which is what makes the rest
possible.

| # | Chapter | What you get |
|---|---|---|
| 11 | [Multiboot, and a second way to boot](11-multiboot.md) | The Multiboot header, `qemu -kernel`, and the info struct |
| 12 | [A real VGA text driver](12-vga-driver.md) | Scrolling, colour, the hardware cursor, `\n` `\t` `\b` |
| 13 | [The serial port](13-serial-port.md) | COM1, the UART registers, and the log that survives a triple fault |
| 14 | [printf from nothing](14-printf.md) | `vsnprintf`, varargs on x86, `%d %u %x %s %c %p`, and padding |
| 15 | [The GDT, properly](15-gdt.md) | Descriptors bit by bit, flat mode, ring 0 and ring 3 segments, `lgdt` |
| 16 | [Interrupts I: the IDT and CPU exceptions](16-idt-exceptions.md) | Gate descriptors, the 32 exceptions, error codes, a panic screen |
| 17 | [Interrupts II: the PIC and IRQs](17-pic-irqs.md) | The 8259s, remapping, EOI, spurious interrupts, a handler registry |
| 18 | [The PIT: time and preemption](18-pit-timer.md) | Channel 0, divisors, 100 Hz, uptime, and the tick that will drive the scheduler |
| 19 | [The PS/2 keyboard](19-keyboard.md) | Scancode set 1, make/break, modifiers, keymaps, a ring buffer |
| 20 | [A kernel console](20-kernel-console.md) | Line editing, backspace, history, and the first interactive prompt |

## Part III — Memory

The part where most hobby kernels die. We go slowly: what memory exists, how to track it, what
paging really does, and how to move a running kernel to a different address without it noticing.

| # | Chapter | What you get |
|---|---|---|
| 21 | [What memory is there?](21-memory-map.md) | E820, the Multiboot memory map, holes, MMIO, and the 640K story |
| 22 | [The physical memory manager](22-pmm.md) | A bitmap frame allocator, `pmm_alloc_frame`, and reserving the kernel |
| 23 | [Paging theory](23-paging-theory.md) | Page directories, page tables, the MMU walk, the TLB, flags bit by bit |
| 24 | [Enabling paging](24-enabling-paging.md) | Identity mapping, `CR0.PG`, and the instruction after which addresses lie |
| 25 | [The higher half](25-higher-half.md) | Moving the kernel to `0xC0000000`, the boot page directory, and why |
| 26 | [The page fault handler](26-page-faults.md) | `CR2`, the error code, demand paging, growing stacks, guard pages |
| 27 | [The kernel heap](27-kernel-heap.md) | `kmalloc`/`kfree`, block headers, splitting, coalescing, alignment |
| 28 | [Address spaces](28-address-spaces.md) | Per-process page directories, cloning, copy-on-write, and `CR3` switches |

## Part IV — Processes

| # | Chapter | What you get |
|---|---|---|
| 29 | [What a process is](29-what-is-a-process.md) | The task struct, kernel stacks, PIDs, states, and the process table |
| 30 | [The context switch](30-context-switch.md) | Twenty instructions that swap one universe for another, explained one at a time |
| 31 | [The scheduler](31-scheduler.md) | Round robin, time slices, priorities, the idle task, preemption from IRQ0 |
| 32 | [Ring 3](32-usermode.md) | Privilege levels, the TSS, `esp0`, the fake `iret` frame, and what ring 3 cannot do |
| 33 | [System calls](33-syscalls.md) | `int 0x80`, the syscall table, argument passing, and never trusting a user pointer |
| 34 | [fork, exec, exit, wait](34-fork-exec.md) | Making one process into two, and replacing a process with a program |
| 35 | [Blocking and wait queues](35-blocking.md) | `sleep`, waiting on the keyboard, and why a spinning kernel is a broken kernel |
| 36 | [Synchronisation](36-synchronisation.md) | The races we already have, `cli`/`sti`, spinlocks, mutexes, semaphores |

## Part V — Storage and filesystems

| # | Chapter | What you get |
|---|---|---|
| 37 | [The ATA PIO driver](37-ata-driver.md) | IDENTIFY, LBA28, the status register dance, reading and writing sectors |
| 38 | [Partitions and a block cache](38-block-layer.md) | The MBR, partition tables, a buffer cache, write-back |
| 39 | [A virtual filesystem](39-vfs.md) | `vfs_node`, the operations table, mounting, and path resolution |
| 40 | [The initrd](40-initrd.md) | A tar ramdisk, `mkinitrd`, and files before you have a disk driver |
| 41 | [FAT16, read](41-fat16-read.md) | The boot record, the FAT, clusters, directory entries, long names |
| 42 | [FAT16, write](42-fat16-write.md) | Allocating clusters, growing files, creating and deleting entries |
| 43 | [File descriptors and pipes](43-fds-and-pipes.md) | The fd table, `open`/`read`/`write`/`close`/`lseek`, `dup2`, and a pipe |

## Part VI — Userland

| # | Chapter | What you get |
|---|---|---|
| 44 | [The ELF loader](44-elf-loader.md) | Headers, program headers, mapping segments, and jumping into a program |
| 45 | [A C library for Nimbus](45-user-libc.md) | `crt0.s`, syscall stubs, `malloc`, `printf`, `string.h` for userland |
| 46 | [The shell and the utilities](46-shell.md) | Parsing a command line, `fork`+`exec`+`wait`, redirection, pipelines, `ls cat echo ps` |

## Part VII — Afterwards

| # | Chapter | What you get |
|---|---|---|
| 47 | [Debugging an operating system](47-debugging.md) | QEMU monitor, GDB on `:1234`, triple faults, `-d int`, bisecting a boot hang |
| 48 | [Where to go next](48-where-next.md) | x86-64 and long mode, SMP, real drivers, networking, a journaling FS, POSIX |

---

## The line-by-line reference

The chapters explain code in the order that makes it teachable. The
[line-by-line](line-by-line/) folder explains it in the order it appears in the file, with no
narrative — use it when you are reading the source and want an annotation for the line in front
of you.

- Spark: [boot.asm](line-by-line/spark-boot.md) · [stage2.asm](line-by-line/spark-stage2.md) · [kernel.c](line-by-line/spark-kernel.md) · [link.ld](line-by-line/spark-link.md)
- Boot: [boot.s](line-by-line/nimbus-boot.md) · [gdt.c / gdt_flush.s](line-by-line/nimbus-gdt.md) · [idt.c / isr.s](line-by-line/nimbus-idt.md)
- Memory: [pmm.c](line-by-line/nimbus-pmm.md) · [paging.c](line-by-line/nimbus-paging.md) · [heap.c](line-by-line/nimbus-heap.md)
- Tasks: [task.c](line-by-line/nimbus-task.md) · [switch.s](line-by-line/nimbus-switch.md) · [sched.c](line-by-line/nimbus-sched.md) · [syscall.c](line-by-line/nimbus-syscall.md)
- Storage: [ata.c](line-by-line/nimbus-ata.md) · [vfs.c](line-by-line/nimbus-vfs.md) · [fat16.c](line-by-line/nimbus-fat16.md)
- Userland: [elf.c](line-by-line/nimbus-elf.md) · [crt0.s + libc](line-by-line/nimbus-libc.md) · [sh.c](line-by-line/nimbus-sh.md)

---

## Appendices

- [A. The x86 instruction subset we use](appendix-a-instructions.md)
- [B. I/O port map](appendix-b-ports.md)
- [C. Every struct in Nimbus, with its layout](appendix-c-structs.md)
- [D. The full syscall table](appendix-d-syscalls.md)
- [E. Glossary](appendix-e-glossary.md)
- [F. Reading the Intel manuals without drowning](appendix-f-manuals.md)

---

## Conventions used in the book

**Sizes.** `KiB`, `MiB`, `GiB` are powers of two. When the book writes `KB` it means the same
thing and is being sloppy for readability; there is no decimal-kilobyte anywhere in this project.

**Numbers.** Hexadecimal is written `0xB8000` in C and in prose. Assembly listings use NASM
syntax, so hex is also `0xB8000` there. Binary is written `0b1010` or with underscores for
readability: `0b1001_1010`.

**Addresses.** A *physical* address is what goes on the memory bus. A *virtual* (or *linear*)
address is what your code uses after paging is on. Until Chapter 24 they are the same thing and
the book will say so; after Chapter 24 every address in the book is labelled.

**"The manual"** means the *Intel 64 and IA-32 Architectures Software Developer's Manual*,
Volume 3 (System Programming Guide) unless another volume is named. Appendix F explains how to
navigate it.

**Danger blocks** look like this:

> ⚠️ **This will triple-fault.** If you get this wrong the machine reboots in a loop with no
> output at all. Chapter 47 explains how to see what happened.

**Hardware quirk blocks** look like this:

> 🔧 **On real hardware.** QEMU is forgiving here; a 2009 ThinkPad is not. The difference is …

---

[Start reading → Chapter 0: Introduction](00-introduction.md)
