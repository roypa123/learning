# Chapter 8 — Stage 2: loading the kernel

[← Protected mode](07-protected-mode.md) · [Contents](README.md) · [Next: Freestanding C →](09-freestanding-c.md)

> 📖 **Line by line:** [stage2.asm](line-by-line/spark-stage2.md)

---

## Goal

Finish [`spark/boot/stage2.asm`](../spark/boot/stage2.asm): load the kernel off the disk, get it to
1 MiB, and jump into it. Chapter 7 covered A20 and the mode switch; this chapter covers everything
around them — the ordering constraints, the segment arithmetic, and the copy.

It also covers the disk image itself, which is the one thing in this project that nobody writes and
everybody has to get right.

---

## 1. The ordering constraint

Stage 2 does four things and the order is not negotiable:

```
    1. load the kernel                  <- needs the BIOS
    2. enable A20                       <- needs to be before anything touches >1 MiB
    3. build a GDT and set CR0.PE       <- destroys the BIOS
    4. copy to 1 MiB and jump           <- needs A20 and protected mode
```

Step 1 must come before step 3, because `int 0x13` stops working at step 3.

Step 2 must come before step 4, because without A20, writing to `0x100000` writes to `0x000000`.

Step 1 cannot load *directly* to 1 MiB, for two separate reasons that are worth keeping apart:

- **Real mode cannot address it.** The maximum real-mode address is `0x10FFEF`, and `ES:BX` with a
  16-bit `ES` cannot name `0x100000` at all — the segment would have to be `0x10000`, which does not
  fit in 16 bits.
- **A20 is off at that point anyway.** Even if you could name the address, the write would land at
  `0x000000`.

So the kernel is staged somewhere real mode *can* reach, and moved up after the switch. That is what
§4 does.

---

## 2. The disk image

Before the code, the thing the code assumes.

```
    LBA 0        boot.bin       512 bytes         stage 1
    LBA 1..4     stage2.bin     2 KiB             stage 2
    LBA 5..68    kernel.bin     32 KiB            the kernel
    LBA 69..2879                                  unused
    -----------------------------------------------------
    total        1,474,560 bytes = 80 × 2 × 18 × 512
```

A raw disk image is not a container format. There is no header, no index, no metadata — it is a
byte-for-byte picture of what the sectors contain, and the only structure it has is the structure
the boot process agreed on in advance.

That agreement lives in three places: `boot.asm`'s `STAGE2_LBA`, `stage2.asm`'s `KERNEL_LBA`, and
[`mkimage.ps1`](../tools/mkimage.ps1)'s command line. If any two of them disagree, the machine
reboots with no message.

### 2.1 Why exactly 1,474,560 bytes

QEMU infers floppy geometry from the file size. Hand it 1,474,560 bytes and it presents 80 cylinders,
2 heads, 18 sectors — which is exactly what `SECTORS_PER_TRACK` and `HEADS` in our code say.

Hand it 1,000,000 bytes and it guesses something else, our CHS arithmetic stops matching the
geometry, and reads land on the wrong sectors. The failure is not an error; it is wrong data.

This is why `mkimage.ps1` takes the size as a required parameter and pads to it, rather than just
concatenating the pieces.

### 2.2 The builder, and the two checks it does

```powershell
foreach ($part in $Parts) {
    ...
    for ($s = 0; $s -lt $sectors; $s++) {
        $idx = $lba + $s
        if ($owner[$idx]) {
            throw ("Sector {0}: '$file' overlaps '{1}'. ..." -f $idx, $owner[$idx])
        }
        $owner[$idx] = $file
    }
    ...
}

if ($image[510] -ne 0x55 -or $image[511] -ne 0xAA) {
    throw ("Sector 0 does not end with the 0x55 0xAA boot signature ...")
}
```

**Overlap detection.** Every sector is tracked. If `stage2.bin` grows past 2 KiB, it will occupy LBA
5 as well — which is where the kernel goes — and the build fails with a message naming both files.
Without this check the build succeeds, the image is silently wrong, and the symptom is a hang.

**Signature check.** The boot sector must end in `55 AA` or the BIOS will refuse the image and say
something unhelpful about no bootable device. Checking here means the error arrives at build time,
attached to an explanation.

Both checks exist because the failures they catch are *silent*. That is the criterion for whether a
build-time check is worth writing: not "is this likely", but "how long would it take to diagnose".

---

## 3. Loading the kernel

```nasm
    mov ax, KERNEL_SEG          ; 0x1000
    mov es, ax                  ; ES:BX = 0x1000:0x0000 = physical 0x10000
    xor bx, bx
    mov ax, KERNEL_LBA          ; 5
    mov cx, KERNEL_SECTORS      ; 64

.load_one:
    mov dh, 1                   ; exactly one sector
    call disk_read

    inc ax                      ; next LBA

    mov dx, es
    add dx, 32
    mov es, dx

    loop .load_one
```

### 3.1 Why `0x10000`

64 KiB. It is above stage 2 at `0x7E00`, above the 32 KiB we reserved for the stack below `0x7C00`,
and it leaves room for a 512 KiB kernel before hitting the video memory hole at `0xA0000`.

It is also segment-aligned — `0x1000:0x0000` — which means `BX` starts at zero and the segment
arithmetic in the loop is clean.

### 3.2 One sector at a time

Chapter 6, §5 covered this: a CHS read may not cross a track boundary, a track is 18 sectors, and 64
sectors starting at LBA 5 crosses three of them. Some BIOSes handle it; many return a short read,
which `disk_read`'s `cmp al` would catch as an error rather than as data.

Sixty-four BIOS calls. Slow, and correct.

### 3.3 The segment advance, again

```nasm
    mov dx, es
    add dx, 32
    mov es, dx
```

512 bytes is 32 paragraphs. We advance `ES` rather than adding 512 to `BX` because `BX` is 16 bits:
after 128 sectors it would wrap to zero and sector 129 would land on top of sector 1.

Our kernel is 64 sectors, so the wrap would not actually happen — but the bug would appear the day
the kernel grew past 64 KiB, which is a month later and in a completely different part of the
project. Writing it the scalable way costs three instructions.

Note that `DX` is used as scratch here, and `disk_read` needs `DL` for the drive. It works because
`disk_read` reads the drive from memory (`[boot_drive_s2]`) rather than from `DL`, and because
`pusha`/`popa` means the call does not disturb `DX` either. Both of those are things to check rather
than assume; a version of `disk_read` that took the drive in `DL` would break this loop.

### 3.4 Registers across the loop

`disk_read` brackets itself with `pusha`/`popa`, so `AX`, `CX`, `BX` and `ES` all survive the call.
That is what lets the loop keep `AX` as the LBA counter and `CX` as the `loop` counter with no saving
and restoring at the call site.

It is worth noticing how much simplicity comes from that one decision. A `disk_read` that clobbered
registers would need the caller to push and pop around every call, and in a 2 KiB budget that adds
up.

---

## 4. The copy to 1 MiB

```nasm
[BITS 32]
protected_entry:
    mov ax, DATA_SEG
    mov ds, ax
    ...
    mov esp, 0x90000

    mov esi, KERNEL_SEG * 16    ; 0x10000
    mov edi, KERNEL_PHYS        ; 0x100000
    mov ecx, KERNEL_SECTORS * 512 / 4
    cld
    rep movsd

    jmp KERNEL_PHYS
```

`rep movsd` copies `ECX` doublewords from `[ESI]` to `[EDI]`. 64 sectors × 512 bytes ÷ 4 = 8192
doublewords.

This is the fastest memory copy on x86 and it is one instruction. `cld` first, because `DF` decides
the direction and we want upwards — and although we set `cld` in the prologue, that was thirty
instructions and a BIOS call ago.

### 4.1 Why 1 MiB

Everything below it is a minefield (Chapter 2, §3.3): the interrupt vector table, the BIOS data area,
video memory at `0xA0000`, option ROMs, and the BIOS itself at `0xF0000`. Above it there is nothing
but RAM.

It is the first address on a PC where a kernel can live without negotiating with anything, and it has
been the conventional kernel load address since the early 1990s. Linux loads there. Multiboot
specifies a minimum of 1 MiB. Nimbus's [`link.ld`](../nimbus/link.ld) says
`KERNEL_LOAD_ADDR = 0x00100000`.

### 4.2 The bare `jmp`

```nasm
    jmp KERNEL_PHYS
```

A near jump to an absolute address. The first instruction of `kernel.bin` is the first byte at
`0x100000`, and [`link.ld`](../spark/link.ld) guarantees that byte is `_start`:

```
    .text ALIGN(4K) :
    {
        __text_start = .;
        *(.text.entry)          <- this section, alone, first
        *(.text .text.*)
        __text_end = .;
    }
```

Chapter 9 covers why that line exists and what happens without it. The short version: without
`*(.text.entry)` the linker is free to order the input files however it likes, and `jmp 0x100000`
lands in the middle of whatever function happened to be placed first.

### 4.3 Why not parse ELF?

Stage 2 could read the ELF header, walk the program headers, and place each segment at its `p_vaddr`.
That is what GRUB does, and it is maybe sixty lines.

We do not, because:

- **The budget.** Stage 2 is 2 KiB, and sixty lines of ELF parsing plus the error handling is a
  meaningful fraction of it.
- **It buys nothing here.** Spark's kernel is one contiguous blob at a fixed address. ELF parsing
  matters when segments have different addresses and permissions, which is Chapter 44's problem, not
  this one.
- **The flattening is free.** `objcopy -O binary` strips the headers at build time, and we keep
  `kernel.elf` alongside for GDB.

The tradeoff is that the kernel must be position-fixed and contiguous, and that a kernel larger than
`KERNEL_SECTORS` fails silently. The Makefile checks the second one:

```make
	@size=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@); \
	 if [ $$size -gt 32768 ]; then \
	   echo "*** kernel.bin is $$size bytes; stage2 only loads 32768."; \
	   echo "*** Raise KERNEL_SECTORS in spark/boot/stage2.asm."; \
	   exit 1; \
	 fi
```

Same principle as the overlap check: the failure is silent, so catch it at build time.

---

## 5. The complete stage 2, in order

Putting the whole file together:

```
  stage2_start:
      set DS/ES/SS = 0, SP = 0x7C00, cld        <- re-establish, do not assume
      save DL into boot_drive_s2                <- before DX becomes scratch
      print "stage2 at 0x7E00"

  load the kernel:
      ES:BX = 0x1000:0x0000
      64 × { read one sector; ES += 32 }
      print "kernel loaded"

  A20:                                          <- Chapter 7 §1
      test; try BIOS; test; try port 0x92; test; try 8042; test
      hang with a message if all four fail

  protected mode:                               <- Chapter 7 §3
      cli
      lgdt
      CR0.PE = 1
      far jmp to 32-bit code

  [BITS 32] protected_entry:
      reload DS/ES/FS/GS/SS
      ESP = 0x90000
      rep movsd : 0x10000 -> 0x100000, 32 KiB
      jmp 0x100000
```

Seventy or so instructions of actual work, in 2 KiB, and the machine goes from "16-bit, 1 MiB, BIOS
available" to "32-bit, 4 GiB, on its own".

---

## 6. Running it

```bat
build spark
run spark
```

```
SeaBIOS (version ...)
Booting from Floppy...
Spark: stage1
Spark: -> stage2
Spark: stage2 at 0x7E00
Spark: loading kernel...
Spark: kernel loaded
Spark: A20 via port 0x92
Spark: A20 enabled
Spark: entering protected mode
```

then the screen clears and the Spark banner appears (Chapter 10).

### Watching the copy happen

Stop the machine just after the mode switch and look at memory directly. In QEMU, press
Ctrl-Alt-2 for the monitor:

```
(qemu) xp /8xb 0x10000
0000000000010000: 0xfa 0xbf 0x00 0x40 0x10 0xc0 0xb9 0x00

(qemu) xp /8xb 0x100000
0000000000100000: 0xfa 0xbf 0x00 0x40 0x10 0xc0 0xb9 0x00
```

The same eight bytes at both addresses: the kernel at its staging area and at its final home.
`fa` is `cli`, which is the first instruction of
[`entry.asm`](../spark/kernel/entry.asm). Seeing your own code at the address you expect, by eye, is
worth the thirty seconds.

And to confirm A20:

```
(qemu) xp /1xb 0x000500
0000000000000500: 0x00
(qemu) xp /1xb 0x100500
0000000000100500: 0x00
```

Write to one and re-read the other — if they differ, A20 is open.

---

## 7. What could go wrong

| Symptom | Cause |
|---|---|
| Hangs after "loading kernel..." | Disk error on a later sector; check `KERNEL_SECTORS` against the image size |
| Reboots right after "entering protected mode" | GDT problem — see Chapter 7, §6 |
| Runs, but the screen is garbage | The kernel was not copied, or was copied from the wrong address |
| Runs the first time, hangs after a rebuild | The kernel outgrew 32 KiB; the Makefile check catches this |
| `mkimage` says "overlaps" | Stage 2 outgrew 2 KiB; raise `STAGE2_SECTORS` in both files |
| Everything works, then random corruption | `BX` wrapped — the segment advance is wrong |

The "raise `STAGE2_SECTORS` in both files" row is worth dwelling on. The constant appears in
`boot.asm` (how many to load) and implicitly in `mkimage`'s command line (where the kernel starts).
Changing one and not the other produces an image where stage 1 loads four sectors of a six-sector
stage 2. The overlap check catches the *other* direction; this one it cannot.

That is the cost of compile-time constants, and Exercise 5.5 is the fix.

---

## 8. Exercises

🟢 **8.1** Change `KERNEL_SECTORS` to 32 and rebuild. The kernel is smaller than that, so it still
works — explain why, and then explain what would happen if it were 8.

🟢 **8.2** Move the A20 code to *after* the mode switch. Predict what happens before running it.

🟡 **8.3** Replace the one-sector-at-a-time loop with a single 64-sector read, and see whether your
QEMU handles it. Then work out whether you could safely read 18 at a time, and what the first LBA of
each read would be.

🟡 **8.4** Add a progress indicator: print a `.` every 8 sectors. Where does it have to go so that
it does not disturb `AX`, `BX`, `CX` or `ES`?

🟡 **8.5** Load the kernel to `0x20000` instead of `0x10000` and adjust the copy. What else has to
change? What is the highest staging address that still works in real mode?

🔴 **8.6** Teach stage 2 to parse ELF: read the program headers from `kernel.elf` (build without the
`objcopy` step), and copy each `PT_LOAD` segment to its `p_paddr`. Measure how much bigger stage 2
gets. This is the work Chapter 44 does for user programs, and doing it here first makes that chapter
easy.

---

## What we covered

- The ordering constraint: BIOS work first, A20 before anything above 1 MiB, mode switch last.
- The two separate reasons the kernel cannot be loaded directly to 1 MiB.
- The disk image as an agreement between three files, and the two build-time checks that catch the
  silent failures.
- Why the image must be exactly 1,474,560 bytes.
- One sector per read, and the segment advance that scales past 64 KiB.
- `rep movsd` to 1 MiB, and why 1 MiB.
- Why we flatten the kernel rather than parsing ELF, and what that costs.
- Reading memory in the QEMU monitor to confirm the copy and the A20 gate by eye.

[Chapter 9](09-freestanding-c.md) is the other half of the handover: what a linker script does, why
`_start` has to be first, and what "freestanding" actually means to the compiler.

---

[← Protected mode](07-protected-mode.md) · [Contents](README.md) · [Next: Freestanding C →](09-freestanding-c.md)
