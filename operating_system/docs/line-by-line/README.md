# Line-by-line reference

[Book contents](../README.md)

---

The chapters explain code in the order that makes it teachable. These files explain it in the order
it appears in the file, with no narrative.

Use them when you are reading the source and want an annotation for the line in front of you. Use the
chapters when you want to know why the line exists.

---

## Spark — the basic OS

| File | Source | Chapter |
|---|---|---|
| [spark-boot.md](spark-boot.md) | [`spark/boot/boot.asm`](../../spark/boot/boot.asm) | 5, 6 |
| [spark-stage2.md](spark-stage2.md) | [`spark/boot/stage2.asm`](../../spark/boot/stage2.asm) | 7, 8 |
| [spark-kernel.md](spark-kernel.md) | [`spark/kernel/entry.asm`](../../spark/kernel/entry.asm), [`kernel.c`](../../spark/kernel/kernel.c) | 9, 10 |
| [spark-link.md](spark-link.md) | [`spark/link.ld`](../../spark/link.ld) | 9 |

## Nimbus — boot and CPU tables

| File | Source | Chapter |
|---|---|---|
| [nimbus-boot.md](nimbus-boot.md) | [`nimbus/boot/boot.asm`](../../nimbus/boot/boot.asm) | 11, 25 |
| [nimbus-link.md](nimbus-link.md) | [`nimbus/link.ld`](../../nimbus/link.ld) | 25 |
| [nimbus-gdt.md](nimbus-gdt.md) | [`kernel/gdt.c`](../../nimbus/kernel/gdt.c), [`boot/cpu.asm`](../../nimbus/boot/cpu.asm) | 15 |
| [nimbus-idt.md](nimbus-idt.md) | [`kernel/idt.c`](../../nimbus/kernel/idt.c), [`isr.c`](../../nimbus/kernel/isr.c), [`boot/isr.asm`](../../nimbus/boot/isr.asm) | 16, 17 |

## Nimbus — memory

| File | Source | Chapter |
|---|---|---|
| [nimbus-pmm.md](nimbus-pmm.md) | [`mm/pmm.c`](../../nimbus/mm/pmm.c) | 21, 22 |
| [nimbus-paging.md](nimbus-paging.md) | [`mm/paging.c`](../../nimbus/mm/paging.c) | 23–26, 28 |
| [nimbus-heap.md](nimbus-heap.md) | [`mm/heap.c`](../../nimbus/mm/heap.c) | 27 |

## Nimbus — tasks

| File | Source | Chapter |
|---|---|---|
| [nimbus-task.md](nimbus-task.md) | [`kernel/task.c`](../../nimbus/kernel/task.c) | 29, 32, 34 |
| [nimbus-switch.md](nimbus-switch.md) | [`boot/cpu.asm`](../../nimbus/boot/cpu.asm) | 30, 32 |
| [nimbus-sched.md](nimbus-sched.md) | [`kernel/sched.c`](../../nimbus/kernel/sched.c) | 31, 35 |
| [nimbus-syscall.md](nimbus-syscall.md) | [`kernel/syscall.c`](../../nimbus/kernel/syscall.c) | 33 |

## Nimbus — storage

| File | Source | Chapter |
|---|---|---|
| [nimbus-ata.md](nimbus-ata.md) | [`drivers/ata.c`](../../nimbus/drivers/ata.c) | 37 |
| [nimbus-vfs.md](nimbus-vfs.md) | [`fs/vfs.c`](../../nimbus/fs/vfs.c), [`fd.c`](../../nimbus/fs/fd.c) | 39, 43 |
| [nimbus-fat16.md](nimbus-fat16.md) | [`fs/fat16.c`](../../nimbus/fs/fat16.c) | 41, 42 |

## Nimbus — userland

| File | Source | Chapter |
|---|---|---|
| [nimbus-elf.md](nimbus-elf.md) | [`kernel/elf.c`](../../nimbus/kernel/elf.c) | 44 |
| [nimbus-libc.md](nimbus-libc.md) | [`user/crt0.asm`](../../nimbus/user/crt0.asm), [`libc.c`](../../nimbus/user/libc.c) | 45 |
| [nimbus-sh.md](nimbus-sh.md) | [`user/sh.c`](../../nimbus/user/sh.c) | 46 |

---

## Conventions

**Line references** are `file.c:NN` and point at the version in this repository. They drift when the
source changes; the surrounding quoted code does not.

**⚠️** marks a line where a mistake is silent — the build succeeds, the machine may even boot, and
the failure appears somewhere else.

**🔧** marks a line where QEMU and real hardware differ.

---

[Book contents](../README.md)
