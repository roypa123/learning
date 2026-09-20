# =============================================================================
#  operating_system/Makefile
# =============================================================================
#
#  Two operating systems, one userland, and the images to boot them.
#
#      make spark      the basic OS -> bin/spark.img   (a 1.44 MiB floppy)
#      make nimbus     the full OS  -> bin/nimbus.elf + bin/initrd.tar
#      make disk       a FAT16 disk image for Nimbus to mount
#      make run        build nimbus and boot it
#      make run-spark  build spark and boot it
#      make debug      boot nimbus stopped, waiting for GDB on :1234
#      make clean
#
#  Windows users without make can use build.bat and run.bat, which do the same
#  things with the same flags.
#
#  Explained in: docs/01-setup.md
# =============================================================================

# ---- The toolchain -----------------------------------------------------------
#
# CROSS is the prefix of a compiler that targets bare-metal i686 rather than
# the machine you are sitting at. Chapter 1 explains at length why this is not
# optional; the short version is that your system gcc produces code that
# expects a C library, an ELF loader and an operating system, all of which we
# are in the middle of writing.
CROSS   ?= i686-elf-
CC       = $(CROSS)gcc
LD       = $(CROSS)ld
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
READELF  = $(CROSS)readelf
NASM    ?= nasm
QEMU    ?= qemu-system-i386

# ---- Flags -------------------------------------------------------------------
#
# Every one of these earns its place:
#
#   -ffreestanding      there is no hosted C environment. Stops GCC assuming
#                       that `main` is special or that the standard library
#                       exists -- though note it does NOT stop GCC calling
#                       memcpy, which is why lib/string.c must provide it.
#   -nostdlib           do not link crt0, libc or libgcc's startup files.
#   -fno-builtin        do not turn our own memcpy into a call to itself.
#   -fno-stack-protector the stack guard needs a canary from thread-local
#                       storage, which needs a thread library, which we do not
#                       have. Leaving it on produces a link error about
#                       __stack_chk_fail.
#   -fno-pic            we want absolute addresses. Position-independent code
#                       needs a GOT that something must relocate.
#   -mno-sse etc        GCC will happily vectorise a struct copy with SSE, and
#                       SSE faults until CR4.OSFXSR is set and an FXSAVE area
#                       exists. Chapter 48 turns it on properly.
#   -Wall -Wextra       non-negotiable in a kernel. There is no runtime to
#                       catch what the compiler could have.
#   -g                  DWARF symbols, so that GDB can show source. Costs
#                       nothing at runtime: it all lives in sections the loader
#                       never reads.
CFLAGS   = -std=gnu11 -m32 -ffreestanding -nostdlib -fno-builtin \
           -fno-stack-protector -fno-pic -fno-omit-frame-pointer \
           -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-red-zone \
           -Wall -Wextra -Wno-unused-parameter -O2 -g \
           -Inimbus/include

ASFLAGS  = -f elf32 -g -F dwarf

# =============================================================================
#  Spark -- the basic operating system
# =============================================================================
SPARK_BOOT   = bin/spark/boot.bin
SPARK_STAGE2 = bin/spark/stage2.bin
SPARK_KERNEL = bin/spark/kernel.bin
SPARK_IMG    = bin/spark.img

.PHONY: spark
spark: $(SPARK_IMG)

bin/spark:
	@mkdir -p bin/spark

# The bootloader is assembled to a *flat binary*, not an ELF file. There is no
# loader to interpret an ELF header at this point in the boot -- the BIOS
# copies 512 raw bytes and jumps to the first one.
$(SPARK_BOOT): spark/boot/boot.asm | bin/spark
	$(NASM) -f bin $< -o $@

$(SPARK_STAGE2): spark/boot/stage2.asm | bin/spark
	$(NASM) -f bin $< -o $@

# The kernel is built as ELF (so we keep symbols for GDB) and then flattened
# with objcopy, because stage 2 does a bare `jmp 0x100000` and cannot parse a
# program header table in the space it has.
bin/spark/entry.o: spark/kernel/entry.asm | bin/spark
	$(NASM) $(ASFLAGS) $< -o $@

bin/spark/kernel.o: spark/kernel/kernel.c | bin/spark
	$(CC) $(CFLAGS) -c $< -o $@

bin/spark/kernel.elf: bin/spark/entry.o bin/spark/kernel.o spark/link.ld
	$(LD) -m elf_i386 -T spark/link.ld -o $@ bin/spark/entry.o bin/spark/kernel.o

$(SPARK_KERNEL): bin/spark/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@); \
	 if [ $$size -gt 32768 ]; then \
	   echo "*** kernel.bin is $$size bytes; stage2 only loads 32768."; \
	   echo "*** Raise KERNEL_SECTORS in spark/boot/stage2.asm."; \
	   exit 1; \
	 fi

$(SPARK_IMG): $(SPARK_BOOT) $(SPARK_STAGE2) $(SPARK_KERNEL)
	powershell -ExecutionPolicy Bypass -File tools/mkimage.ps1 \
	  -Out $@ -Size 1474560 \
	  -Parts "$(SPARK_BOOT):0","$(SPARK_STAGE2):1","$(SPARK_KERNEL):5"

# =============================================================================
#  Nimbus -- the medium operating system
# =============================================================================
KERNEL_ASM = nimbus/boot/boot.asm nimbus/boot/isr.asm nimbus/boot/cpu.asm

KERNEL_SRC = \
	nimbus/kernel/main.c     nimbus/kernel/printk.c   nimbus/kernel/gdt.c     \
	nimbus/kernel/idt.c      nimbus/kernel/isr.c      nimbus/kernel/irq.c     \
	nimbus/kernel/timer.c    nimbus/kernel/console.c  nimbus/kernel/task.c    \
	nimbus/kernel/sched.c    nimbus/kernel/sync.c     nimbus/kernel/syscall.c \
	nimbus/kernel/elf.c                                                       \
	nimbus/mm/pmm.c          nimbus/mm/paging.c       nimbus/mm/heap.c        \
	nimbus/drivers/vga.c     nimbus/drivers/serial.c  nimbus/drivers/keyboard.c \
	nimbus/drivers/ata.c                                                      \
	nimbus/fs/vfs.c          nimbus/fs/fd.c           nimbus/fs/initrd.c      \
	nimbus/fs/fat16.c        nimbus/fs/pipe.c                                 \
	nimbus/lib/string.c      nimbus/lib/printf.c

KERNEL_OBJ = $(patsubst %.asm,bin/nimbus/%.o,$(KERNEL_ASM)) \
             $(patsubst %.c,bin/nimbus/%.o,$(KERNEL_SRC))

NIMBUS_ELF = bin/nimbus.elf

.PHONY: nimbus
nimbus: $(NIMBUS_ELF) bin/initrd.tar

bin/nimbus/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) $< -o $@

bin/nimbus/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DNIMBUS_KERNEL -c $< -o $@

$(NIMBUS_ELF): $(KERNEL_OBJ) nimbus/link.ld
	$(LD) -m elf_i386 -T nimbus/link.ld -o $@ $(KERNEL_OBJ)
	@echo
	@$(READELF) -l $@ | head -20
	@echo "kernel: $$($(READELF) -h $@ | grep Entry)"

# ---- Userland ----------------------------------------------------------------
#
# Each utility is the same coreutils.c compiled with a different -D, linked
# against crt0 and libc. Seven binaries, one source file, one rule.
USER_PROGS = sh ls cat echo hexdump sleep true false forktest

USER_LIB = bin/user/crt0.o bin/user/libc.o bin/user/string.o bin/user/printf.o

bin/user/crt0.o: nimbus/user/crt0.asm
	@mkdir -p bin/user
	$(NASM) $(ASFLAGS) $< -o $@

bin/user/libc.o: nimbus/user/libc.c
	@mkdir -p bin/user
	$(CC) $(CFLAGS) -c $< -o $@

bin/user/string.o: nimbus/lib/string.c
	@mkdir -p bin/user
	$(CC) $(CFLAGS) -c $< -o $@

bin/user/printf.o: nimbus/lib/printf.c
	@mkdir -p bin/user
	$(CC) $(CFLAGS) -c $< -o $@

bin/user/sh: nimbus/user/sh.c $(USER_LIB) nimbus/user/user.ld
	@mkdir -p bin/user
	$(CC) $(CFLAGS) -c $< -o bin/user/sh.o
	$(LD) -m elf_i386 -T nimbus/user/user.ld -o $@ $(USER_LIB) bin/user/sh.o

define USERPROG_RULE
bin/user/$(1): nimbus/user/coreutils.c $$(USER_LIB) nimbus/user/user.ld
	@mkdir -p bin/user
	$$(CC) $$(CFLAGS) -DBUILD_$(shell echo $(1) | tr a-z A-Z) -c $$< -o bin/user/$(1).o
	$$(LD) -m elf_i386 -T nimbus/user/user.ld -o $$@ $$(USER_LIB) bin/user/$(1).o
endef

$(foreach p,$(filter-out sh,$(USER_PROGS)),$(eval $(call USERPROG_RULE,$(p))))

USER_BINS = $(addprefix bin/user/,$(USER_PROGS))

bin/initrd.tar: $(USER_BINS)
	powershell -ExecutionPolicy Bypass -File tools/mkinitrd.ps1 \
	  -Out $@ -Files $(subst $(space),$(comma),$(USER_BINS))

comma := ,
space := $(subst ,, )

# ---- A FAT16 disk for Nimbus to mount ----------------------------------------
#
# 16 MiB, one partition, formatted FAT16. Built with mkfs.fat if you have it
# (WSL, MSYS2, or Linux); otherwise Nimbus simply boots with no /mnt, which is
# fine until Chapter 41.
bin/disk.img:
	@mkdir -p bin
	dd if=/dev/zero of=$@ bs=1M count=16 2>/dev/null
	mkfs.fat -F 16 -n NIMBUS $@
	@echo "disk: 16 MiB FAT16 at $@"

.PHONY: disk
disk: bin/disk.img

# =============================================================================
#  Running
# =============================================================================
#
# -kernel        QEMU's built-in multiboot loader. No bootloader, no image.
# -initrd        loaded as multiboot module 0 -- what fs/initrd.c reads.
# -serial stdio  the kernel's log appears in the terminal you launched from.
# -m 128M        128 MiB of RAM; anything from 32 MiB up works.
# -no-reboot     stop on a triple fault instead of rebooting in a loop, which
#                is the difference between seeing the panic and seeing nothing.
# -d int         add this to log every interrupt and exception QEMU delivers.
QEMU_FLAGS = -m 128M -serial stdio -no-reboot -no-shutdown

.PHONY: run
run: nimbus
	$(QEMU) $(QEMU_FLAGS) -kernel $(NIMBUS_ELF) -initrd bin/initrd.tar \
	  $(if $(wildcard bin/disk.img),-drive file=bin/disk.img$(comma)format=raw,)

.PHONY: run-spark
run-spark: spark
	$(QEMU) -m 32M -no-reboot -fda $(SPARK_IMG) -boot a

# GDB: run this, then in another terminal
#     i686-elf-gdb bin/nimbus.elf -ex "target remote :1234"
# -S stops the CPU before the first instruction, so you can break on _start.
.PHONY: debug
debug: nimbus
	$(QEMU) $(QEMU_FLAGS) -kernel $(NIMBUS_ELF) -initrd bin/initrd.tar -S -s

.PHONY: trace
trace: nimbus
	$(QEMU) $(QEMU_FLAGS) -kernel $(NIMBUS_ELF) -initrd bin/initrd.tar \
	  -d int,cpu_reset -D bin/qemu.log

# =============================================================================
#  Inspection -- the commands you will want at 2 a.m.
# =============================================================================
.PHONY: disasm symbols sections
disasm: $(NIMBUS_ELF)
	$(OBJDUMP) -d -M intel $< | less

sections: $(NIMBUS_ELF)
	$(READELF) -S $<

symbols: $(NIMBUS_ELF)
	$(READELF) -s $< | sort -k2

.PHONY: clean
clean:
	rm -rf bin

.PHONY: help
help:
	@echo "make spark      build the basic OS       -> bin/spark.img"
	@echo "make nimbus     build the full OS        -> bin/nimbus.elf"
	@echo "make disk       create a FAT16 disk      -> bin/disk.img"
	@echo "make run        build and boot nimbus"
	@echo "make run-spark  build and boot spark"
	@echo "make debug      boot nimbus waiting for gdb on :1234"
	@echo "make trace      boot nimbus logging every interrupt to bin/qemu.log"
	@echo "make disasm     disassemble the kernel"
	@echo "make clean"
