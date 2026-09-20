# Chapter 47 — Debugging an operating system

[← The shell](46-shell.md) · [Contents](README.md) · [Next: Where to go next →](48-where-next.md)

---

## Goal

The chapter you should have skimmed in Part I. Kernel bugs fail differently from application bugs,
and the techniques are different enough to be worth writing down.

---

## 1. Why it is different

In application programming a mistake produces an exception with a stack trace, and the runtime tells
you where.

In kernel programming a mistake produces:

- a machine that reboots instantly, forever, with no output;
- or a machine that hangs, with no output;
- or a machine that works fine and corrupts something that fails twenty seconds later in unrelated
  code.

There is no runtime to catch you, no message, and frequently no relationship between where the fault
appears and where the bug is.

So the strategy is different. It is **not** "run it and read the error". It is:

1. Make the machine tell you things it would not tell you by default.
2. Reduce the search space before you start looking.
3. Have a known-good version to compare against.

---

## 2. The tools, in order of value

### 2.1 The serial log

Chapter 13, and it is first for a reason.

```bash
qemu-system-i386 ... -serial file:bin/serial.log
```

It survives a reboot, it is greppable, and it is diffable. **Diffing a boot that worked against one
that did not is the single most effective technique in this chapter.**

```bash
$ diff good.log bad.log
 [    0.010] inf  heap: 1024 KiB at d0000000, header 16 bytes
 [    0.010] inf  irq: line 0 claimed
-[    0.011] inf  keyboard: ps/2 set 1, us layout
-[    0.012] inf  vfs: root created
```

The last line in the bad log is the last thing that worked. That bounds the problem to whatever
happens next.

Which is why `LOG_INFO` at the end of every subsystem's init is worth the line:

```c
    LOG_INFO("gdt: %u descriptors at %p, tss at %p", ...);
    LOG_INFO("idt: 256 vectors at %p (49 populated)", ...);
    LOG_INFO("pic: remapped to vectors %u-%u and %u-%u", ...);
```

### 2.2 `-no-reboot`

```bash
qemu-system-i386 ... -no-reboot -no-shutdown
```

A triple fault resets the CPU. Without this flag, QEMU reboots and the screen is gone before you can
read it.

With it, the machine stops and whatever was on screen stays there. This is in `run.bat` by default
and should be in yours.

### 2.3 `-d int`

```bash
qemu-system-i386 ... -d int -D bin/qemu.log
```

Logs every interrupt and exception QEMU delivers.

```
   13: v=0d e=0018 i=0 cpl=0 IP=0008:c0101a47 pc=c0101a47 SP=0010:c0106fc0
```

| Field | Meaning |
|---|---|
| `v=0d` | vector 13, general protection |
| `e=0018` | error code `0x18` |
| `i=0` | 0 = hardware/exception, 1 = software `int` |
| `cpl=0` | the privilege level it interrupted |
| `IP=0008:c0101a47` | `CS:EIP` of the faulting instruction |
| `SP=0010:c0106fc0` | `SS:ESP` |

**The output is enormous** — 100 lines a second from the timer alone. `grep -v "v=20"` drops it.

This is *the* tool for a triple fault. §3.

### 2.4 GDB

```bash
make debug
```
```bash
$ i686-elf-gdb bin/nimbus.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

`-S -s` stops the CPU before the first instruction and listens on port 1234, so you can break on
`_start` if you need to.

A `.gdbinit` worth having:

```
target remote :1234
symbol-file bin/nimbus.elf
set disassembly-flavor intel

define hook-stop
  info registers eip esp ebp eflags
end

define pagewalk
  set $dir = $arg0 >> 22
  set $tbl = ($arg0 >> 12) & 0x3ff
  printf "dir[%d] tbl[%d] off[0x%x]\n", $dir, $tbl, $arg0 & 0xfff
end
```

Caveats:

**Use `i686-elf-gdb`.** A 64-bit `gdb` mostly works on a 32-bit target and gets confused about
register widths at exactly the wrong moments.

**Symbols are virtual.** Break on `kmain` and GDB uses `0xC01...`, which only works after paging.
For `_start`, break on the physical address: `break *0x101000`.

**Userland has different symbols.** `symbol-file bin/user/sh` replaces the kernel's. Use
`add-symbol-file` to have both, and remember their address ranges do not overlap.

### 2.5 The QEMU monitor

Ctrl-Alt-2 in the QEMU window, or `-monitor stdio`.

```
(qemu) info registers
(qemu) info tlb
(qemu) info mem
(qemu) xp /16xb 0x7c00        physical memory
(qemu) x  /16xb 0xc0100000    virtual memory, through the current page tables
(qemu) info pic
(qemu) info irq
```

`info registers` (Chapter 15, §7) verifies the GDT, IDT and TR in thirty seconds.

`info tlb` (Chapter 24, §7.2) dumps the current mappings with permissions.

`xp` versus `x` is the distinction that matters: physical versus virtual. After Chapter 25 they are
very different.

You cannot use `-serial stdio` and `-monitor stdio` together.

### 2.6 `kprintf`, and the Heisenbug it creates

The oldest technique, and still the one you will use most.

The thing to know:

> heavy logging slows the kernel down. That turns out to be a feature the first time a race condition
> disappears when you add a printf — which tells you, immediately and for free, that the bug is
> timing-dependent.

A bug that vanishes when you add logging is a race. That is diagnostic information, not an
inconvenience, and it narrows the search enormously.

---

## 3. Triple faults

The characteristic kernel failure: instant reboot, no output.

### 3.1 What it is

A fault occurred. Handling it caused a second fault — a **double fault**, vector 8. Handling *that*
caused a third. The CPU gives up and asserts the reset line.

So a triple fault means three things went wrong, and **the first one is the one you want.**

### 3.2 Finding it

```bash
qemu-system-i386 ... -no-reboot -d int -D bin/qemu.log
$ tail -30 bin/qemu.log
```

```
    0: v=0e e=0002 i=0 cpl=0 IP=0008:c0104a12 pc=c0104a12 SP=0010:c0106f80 CR2=00000000
    1: v=08 e=0000 i=0 cpl=0 IP=0008:c0104a12 pc=c0104a12 SP=0010:c0106f80
check_exception old: 0x8 new 0xd
```

Read it bottom-up:

- `check_exception old: 0x8 new 0xd` — a `#GP` while handling a double fault. That is the triple.
- `v=08` — the double fault.
- `v=0e`, `CR2=00000000` — **the first fault**: a page fault at address 0, at `c0104a12`.

Then:

```bash
$ i686-elf-objdump -d bin/nimbus.elf | grep -B5 c0104a12
```

and you have the instruction.

### 3.3 The common causes

| First exception | Likely cause |
|---|---|
| `v=0d` after `lgdt` | GDT limit off by one, or base wrong |
| `v=0d` after `mov cr0` | Far jump selector wrong |
| `v=0d` on a `push` | `SS` not reloaded |
| `v=0e` with a plausible address | Missing `P2V`, or the identity map removed too early |
| `v=08` with no prior fault | No valid kernel stack — `esp0` zero or no TSS |
| Nothing logged at all | The GDT or IDT base is garbage; `lgdt`/`lidt` loaded nonsense |

That last row is the nastiest: the CPU cannot even *deliver* the first exception, so nothing is
logged. If `-d int` shows nothing, suspect the descriptor tables themselves.

### 3.4 The bisect

When the log gives nothing, bisect by `hlt`:

```c
    gdt_init();
    kprintf("A\n"); for(;;) hlt();      /* move this line down */
    idt_init();
    irq_init();
```

Crude, fast, and it bounds the problem in about four rebuilds. Combined with `git stash` it is often
quicker than reasoning.

---

## 4. Hangs

The machine is alive and not progressing.

### 4.1 Where is it?

```
(qemu) info registers
EIP=c0104b20
```

```bash
$ i686-elf-objdump -d bin/nimbus.elf | grep -A3 c0104b20
c0104b20:  eb fe    jmp    c0104b20
```

`eb fe` is `jmp $` (Chapter 3, §7) — a deliberate stop.

If `EIP` is inside `ata_wait_busy`, the disk is not responding. If it is inside `sched_block`'s
`hlt`, nobody is waking us.

### 4.2 The five usual causes

**Missing EOI** (Chapter 17, §5). The PIC is blocking everything at or below that priority. Symptom:
"everything stops after exactly one keypress".

**Missing wakeup** (Chapter 35, §6.1). A task blocked forever. `ps` shows `blocked`; the wait channel
column (Chapter 35, §6.3) says on what.

**Interrupts disabled** somewhere that blocks or spins. `timer_spin_ms` with `IF` clear waits forever
on a counter that cannot advance (Chapter 18, §7.1).

**Recursive spinlock**, which our detector catches (Chapter 36, §4.2) — so if you have a hang rather
than a panic, it is not this.

**`hlt` with `IF` clear**, which is a permanent stop. Hence `sti; hlt` everywhere.

### 4.3 The diagnostic that answers most of them

```c
void debug_state(void)
{
    kprintf("IF=%d  ticks=%u\n", irqs_enabled(), (uint32_t)timer_ticks());
    kprintf("PIC: irr=%04x isr=%04x\n", pic_get_irr(), pic_get_isr());
    task_dump_all();
    pmm_dump_stats();
    heap_dump();
}
```

Bind it to a key in the keyboard handler, and you can dump the machine's state at any moment.

`isr` non-zero with no handler running means a missing EOI. `ticks` not advancing means the timer is
not firing. `ps` full of `blocked` means a missing wakeup.

---

## 5. Corruption

Something wrote where it should not, and the crash is elsewhere.

### 5.1 The detectors we already have

| Detector | Catches | Chapter |
|---|---|---|
| Heap magic numbers | Double free, bad pointer, overrun into a header | 27, §4 |
| `heap_validate()` | Corrupt list, at the block | 27, §10 |
| PMM double-free panic | Two owners of one frame | 22, §7 |
| `ASSERT` | Any invariant you state | 14 |
| Read-only `.text` + `CR0.WP` | A wild pointer into code | 24, §5.4 |
| Guard pages | Stack overflow | 26, §3.3 |

The last two are worth emphasising: they turn corruption into a *fault at the store*, with the
address in `CR2`, instead of a crash somewhere else later.

### 5.2 Bisecting corruption in time

```c
    ASSERT(heap_validate());
```

Drop it into a suspect function, at the top and the bottom. Then move it. Four rebuilds bound the
window.

The same idea with a canary:

```c
static uint32_t canary = 0xC0FFEE;
...
    ASSERT(canary == 0xC0FFEE);
```

Place it next to the structure you suspect. If it changes, something overran its neighbour.

### 5.3 Watchpoints

```
(gdb) watch *(uint32_t *)0xc0113f40
(gdb) continue

Hardware watchpoint 2: *(uint32_t *) 0xc0113f40
Old value = 0
New value = 305419896
0xc0104f12 in fat_write (...) at fs/fat16.c:312
```

The x86 has four debug registers, so four hardware watchpoints. They stop the CPU at the instruction
that wrote, which is exactly what you want.

This is the single most effective tool against corruption and it is underused because most people do
not know GDB can do it on a bare-metal target.

---

## 6. Wrong behaviour

The machine works and does the wrong thing. The easiest category, and the techniques are ordinary.

**The syscall trace** (Chapter 33, §7.1):

```
[    2.451] dbg  syscall 1(00000001, 0804b0a0, 00000006) from pid 4
```

`strace` in four lines, and it usually shows the problem directly — a wrong fd, a wrong length, a
pointer that is obviously not a string.

**The VFS dispatch trace** (Chapter 39, §8.2) tells you which implementation ran.

**Cross-check against a reference implementation.** This is the one people forget:

```bash
$ mdir -i bin/disk.img         # does mtools agree about the directory?
$ fsck.fat -v bin/disk.img     # is the volume consistent?
$ tar -tvf bin/initrd.tar      # does GNU tar accept our archive?
$ i686-elf-readelf -l bin/user/sh   # do our segments match?
```

Four tools, four formats, and each one is an independent implementation that will disagree with you
when you are wrong.

A filesystem driver that can only read what it wrote has not implemented FAT16.

---

## 7. A stack backtrace

The thing you miss most from userland, and it is sixty lines.

```c
void backtrace(uint32_t ebp)
{
    kprintf("backtrace:\n");

    for (int depth = 0; depth < 16 && ebp; depth++) {
        uint32_t *frame = (uint32_t *)ebp;

        if (ebp < KERNEL_VIRTUAL_BASE || ebp > 0xFFFFF000) break;

        uint32_t ret = frame[1];
        if (!ret) break;

        kprintf("  [%d] %08x\n", depth, ret);
        ebp = frame[0];
    }
}
```

Each stack frame holds the previous `EBP` at `[ebp]` and the return address at `[ebp+4]`
(Chapter 3, §5.3).

Three things make it work:

**`-fno-omit-frame-pointer`** in `CFLAGS`, or there is no chain to walk.

**`xor ebp, ebp`** in `boot.asm` and `crt0.asm`, so the walk terminates.

**The bounds check**, so a corrupted `EBP` does not fault inside the backtrace — which would be a
fault while reporting a fault.

Called from `panic` and from the exception handler:

```c
    isr_dump_registers(regs);
    backtrace(regs->ebp);
```

```
backtrace:
  [0] c0104f12
  [1] c0103a80
  [2] c0102110
  [3] c0101a47
```

Then symbolise:

```bash
$ i686-elf-addr2line -f -e bin/nimbus.elf c0104f12 c0103a80 c0102110
fat_write
vfs_write
sys_write
```

Or do it in the kernel by embedding a symbol table. Exercise 47.6.

---

## 8. Reproducibility

Kernel bugs are frequently timing-dependent, and a bug you cannot reproduce is one you cannot fix.

**Fix the seed.** `-rtc base=2020-01-01` makes the clock deterministic.

**Change the timing deliberately.** `-icount shift=N` makes QEMU execute a deterministic number of
instructions per virtual tick, which makes races reproducible.

**Vary the timing deliberately.** Add a random delay in a suspect path:

```c
    if ((timer_ticks() & 7) == 0) timer_spin_ms(1);
```

A race that appears one time in a thousand appears half the time with a well-placed delay. That is
how you turn "it sometimes hangs" into "it hangs".

**Record and replay.** QEMU supports `-record` and `-replay`, which capture all non-determinism and
let you replay it exactly — including under GDB. It is the most powerful debugging feature QEMU has
and almost nobody uses it.

---

## 9. Real hardware

Everything above assumes QEMU. Eventually you will want metal, and it is different.

### 9.1 Getting it there

```bash
mkdir -p iso/boot/grub
cp bin/nimbus.elf bin/initrd.tar iso/boot/
cp grub.cfg iso/boot/grub/
grub-mkrescue -o nimbus.iso iso
dd if=nimbus.iso of=/dev/sdX bs=4M
```

Chapter 11, §7.2. Test the ISO in QEMU first.

### 9.2 What will break

| Difference | Symptom |
|---|---|
| Real `int 0x13` needs `IF` set | Spark hangs on a disk read |
| Multi-track CHS reads short | Kernel half-loaded (Ch. 6, §5) |
| ATA needs one DRQ per sector | Reads return garbage (Ch. 37, §6.1) |
| Absent devices read `0xFF`, not 0 | Detection hangs (Ch. 37, §5.1) |
| A20 method differs | Boot fails at the mode switch (Ch. 7, §1.4) |
| Spurious IRQ 7 occurs | Keyboard dies after minutes (Ch. 17, §4) |
| `io_wait` actually needed | Interrupts at the wrong vectors (Ch. 17, §2.2) |
| `.bss` not zero at power-on | Uninitialised globals (Ch. 9, §1.2) |
| Memory map has holes | Frames handed out that are devices (Ch. 21, §5.1) |
| No serial port | No log |

Every one of those is flagged in its chapter, and every one of them works in QEMU.

### 9.3 Debugging without a screen

A USB-to-serial adapter and `screen /dev/ttyUSB0 115200`. Five pounds, and it is the whole reason
Chapter 13 came before almost everything else.

If the machine has no serial port, the POST card at port `0x80` is the fallback: `outb(0x80, n)`
displays `n` on a two-digit LED, and you can bisect with it. It is what firmware engineers used
before serial was universal.

---

## 10. A checklist

When something breaks:

1. **Does the serial log stop somewhere?** Diff against a working boot.
2. **`-no-reboot -d int`.** Find the *first* exception.
3. **`objdump | grep <eip>`.** What instruction?
4. **`info registers`.** Are the GDT, IDT and TR what you expect?
5. **`info tlb`.** Is the address mapped, with the permissions you meant?
6. **`ps`.** Is anything blocked, and on what?
7. **`heap_validate()`.** Is the heap intact?
8. **`git diff` against the last version that worked.**
9. **Add a `hlt` and bisect.**
10. **Does an independent tool agree?** `fsck.fat`, `readelf`, `tar`.

Number 8 is not a substitute for understanding. At 1 a.m. it is a substitute for despair.

---

## 11. Exercises

🟢 **47.1** Cause a triple fault deliberately — `lidt` a garbage pointer, then `int 3`. Find the
first exception in the log.

🟢 **47.2** Cause a hang with `hlt` after `cli`, then find `EIP` in the monitor.

🟢 **47.3** Set a hardware watchpoint on `current_task` and watch every switch.

🟡 **47.4** Implement `backtrace` from §7 and call it from `panic`.

🟡 **47.5** Add the `debug_state` dump from §4.3, triggered by a key.

🟡 **47.6** Embed a symbol table: post-process `nimbus.elf` into a sorted array of (address, name),
link it in, and make `backtrace` print names.

🔴 **47.7** Implement single-stepping: set `EFLAGS.TF` in a trap frame, handle vector 1, and print
each instruction's address. Then add a `step` command to the kernel shell.

🔴 **47.8** Add a kernel GDB stub: implement the remote serial protocol over COM2 so that a host GDB
can debug the kernel *without* QEMU's help — which is how you debug on real hardware.

---

## What we covered

- Three failure modes that have no equivalent in application programming, and the strategy that
  follows.
- Six tools in order of value, starting with the log that survives a reboot and diffs cleanly.
- Reading `-d int` output field by field.
- Triple faults: what they are, why the first fault is the one you want, six common causes, and the
  case where nothing is logged at all.
- Five causes of a hang, and one diagnostic dump that distinguishes them.
- Six corruption detectors already in the kernel, bisecting in time with `heap_validate`, and
  hardware watchpoints.
- Cross-checking against `mtools`, `fsck.fat`, GNU tar and `readelf` — four independent
  implementations that will tell you when you are wrong.
- A sixty-line backtrace, and the three things that make it work.
- Making timing bugs reproducible by changing the timing on purpose.
- Ten differences between QEMU and real hardware, each flagged in its own chapter.
- A ten-step checklist.

[Chapter 48](48-where-next.md) is the last one: what you have, and what is on the other side of it.

---

[← The shell](46-shell.md) · [Contents](README.md) · [Next: Where to go next →](48-where-next.md)
