# Chapter 5 — The 512-byte boot sector

[← How a PC boots](04-how-a-pc-boots.md) · [Contents](README.md) · [Next: BIOS services →](06-bios-services.md)

> 📖 **Line by line:** [boot.asm](line-by-line/spark-boot.md)

---

## Goal

Write [`spark/boot/boot.asm`](../spark/boot/boot.asm) — the first code on the machine that is ours.
By the end of this chapter the BIOS will load our 512 bytes, they will print a message, read four
more sectors off the disk, and jump into them.

Every byte is accounted for. There are only 510 of them to spend.

---

## 1. The budget

This is the constraint that shapes everything:

```
    512 bytes total
  -   2 bytes for the 0x55AA signature
  ------
    510 bytes for code, data and strings
```

That is it. There is no linker script that can make it bigger, no compression, no "just this once".
Our finished stage 1 is about 400 bytes, and it only prints two strings and reads four sectors.

The implication is a design rule: **stage 1 does the minimum that makes stage 2 possible, and
nothing else.** No filesystem parsing, no A20, no GDT, no error recovery beyond a retry loop. Every
one of those goes in stage 2, where there is room.

---

## 2. The two directives at the top

```nasm
[BITS 16]                       ; assemble 16-bit instructions: we are in real mode
[ORG 0x7C00]                    ; ...and every label is an address relative to 0x7C00
```

**`[BITS 16]`** tells NASM to emit 16-bit encodings. The same mnemonic assembles differently in 16-
and 32-bit modes — `mov ax, 1` is three bytes in 16-bit and four in 32-bit, with an operand-size
prefix. Get this wrong and every instruction after the first is misaligned garbage.

**`[ORG 0x7C00]`** is the more interesting one. It does not *move* anything; it tells the assembler
what address to assume the code will be loaded at, so that when it computes the address of a label it
adds `0x7C00`.

Without it, `mov si, msg_stage1` would load the offset of `msg_stage1` from the start of the file —
say `0x0130`. The BIOS loads us at `0x7C00`, so the string is really at `0x7D30`, and we would print
whatever happens to be at `0x0130`, which is in the interrupt vector table. With `ORG`, the
instruction assembles as `mov si, 0x7D30` and is correct.

> ⚠️ **`ORG` and `DS` must agree.** `ORG 0x7C00` only produces correct addresses when `DS` is 0,
> because the CPU computes `DS × 16 + offset`. If `DS` were `0x07C0`, every address would be
> `0x7C00` too high. That is what the next block is for.

---

## 3. Establishing a known state

```nasm
start:
    jmp 0x0000:.canonical
.canonical:

    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    sti
    cld

    mov [boot_drive], dl
```

Nine instructions, and each one defends against a specific thing the BIOS does not promise. Taking
them in order.

### 3.1 The far jump

```nasm
    jmp 0x0000:.canonical
```

From Chapter 4: the BIOS may jump to us as `0000:7C00` or as `07C0:0000`. Both are physical `0x7C00`
and they differ only in `CS`. `ORG 0x7C00` is only correct for the first.

A far jump loads both `CS` and `IP` from an explicit pair, so after this instruction `CS` is
definitely 0 regardless of how we arrived. It costs five bytes and removes a class of bug that
manifests as "works in QEMU, reboots on my laptop".

Note that we cannot simply `mov cs, ax` — Chapter 2, §2.4: `CS` is not assignable. A far jump is the
only tool.

### 3.2 `cli`

Interrupts off. Right now `SS:SP` is whatever the BIOS left, which is undefined. If a hardware
interrupt arrives before we set up a stack, the CPU pushes three values onto an address we do not
control — possibly over the interrupt vector table, possibly over our own code.

The window is short but it is real: the timer interrupt fires 18.2 times a second in real mode, so
there is roughly a one in a hundred thousand chance per boot. That is a bug that appears once a
month, which is worse than one that appears every time.

### 3.3 Segments and stack

```nasm
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
```

`xor ax, ax` rather than `mov ax, 0`: two bytes instead of three. At this scale that matters.

We cannot write an immediate into a segment register — `mov ds, 0` does not assemble — so the value
goes via `AX`. Three segment registers get zero:

- **`DS = 0`** so that `[label]` means physical `0x7C00 + label`, matching `ORG`.
- **`ES = 0`** because `int 0x13` reads into `ES:BX`, and we want the data where we think.
- **`SS = 0`** with `SP = 0x7C00`.

The stack deserves a moment. It grows *downwards* from `0x7C00`, into the ~30 KiB of free memory
below us (Chapter 2, §3.3). Our code occupies `0x7C00`–`0x7DFF` and grows *upwards*. They grow away
from each other, so no amount of stack usage can reach the code.

Putting the stack just above the code instead — say `SP = 0x8000` — would work until stage 2 loaded
at `0x7E00` and the stack grew down into it. Small decisions like this one are why boot sectors have
a conventional layout.

### 3.4 `sti`

Interrupts back on. This looks like undoing the `cli`, and it is — the `cli` protected exactly the
window in which `SS:SP` was undefined, and that window is now closed.

Why turn them on at all? Because the BIOS services we are about to call are not guaranteed to work
with interrupts disabled. `int 0x13` on a real floppy controller waits for an interrupt from the
drive, and with `IF` clear it waits forever. This is not hypothetical; it is the single most common
reason a boot sector that works in QEMU hangs on real hardware.

### 3.5 `cld`

Clear the direction flag, so `lodsb` in our `print` routine counts upwards.

The BIOS is under no obligation to leave `DF` clear. If it is set, `print` walks *backwards* through
memory from the start of the string, printing whatever it finds until it hits a zero byte — which
produces a screen of garbage and no clue why.

One byte. Always worth it.

### 3.6 Saving `DL`

```nasm
    mov [boot_drive], dl
```

The BIOS passes the drive it booted from in `DL`: `0x00` for the first floppy, `0x80` for the first
hard disk. We must read from the *same* drive — a boot sector that hardcodes `0x00` works from a
floppy and fails from a USB stick.

`DL` is a scratch register and `div` will clobber `DX` within twenty instructions, so this has to
happen early. It is the last thing in the prologue for exactly that reason.

---

## 4. Where things live

```nasm
STAGE2_SEG      equ 0x0000
STAGE2_OFF      equ 0x7E00
STAGE2_LBA      equ 1
STAGE2_SECTORS  equ 4

SECTORS_PER_TRACK equ 18
HEADS             equ 2
```

`equ` defines an assembly-time constant — no memory, no runtime cost, just a name.

The image layout these describe:

```
    LBA 0        boot.bin        512 bytes, this file
    LBA 1..4     stage2.bin      2 KiB, padded to exactly 4 sectors
    LBA 5..68    kernel.bin      32 KiB, padded to exactly 64 sectors
```

`0x7E00` is the byte immediately after our 512 bytes. Stage 2 loads there because it is the first
free address and because keeping the two stages adjacent makes the memory map easy to hold in your
head.

### The honest limitation

These are **compile-time constants**, not something read from the image. If stage 2 grows past 2 KiB,
stage 1 will load four sectors of a five-sector program and jump into it, and the failure will be a
hang with no message.

Three ways to do better, in increasing order of effort:

1. **Have the image builder patch the count in.** `mkimage` knows how big `stage2.bin` is; it could
   write that number into a known offset of `boot.bin`. About ten lines, and it removes the whole
   class of problem.
2. **Put a small header at the start of stage 2** with a magic number and a length, and have stage 1
   read one sector, check the magic, then read the rest.
3. **Parse a filesystem.** What GRUB does, and what makes its first stage need a second stage just
   to understand ext4.

We use constants, and [`mkimage.ps1`](../tools/mkimage.ps1) refuses to build an image where the
pieces overlap — which catches the failure at build time rather than at boot time. That is the
cheapest version of safety available and it is worth the twenty lines it costs.

Exercise 5.5 implements option 1.

---

## 5. Loading stage 2

```nasm
    mov ax, STAGE2_SEG
    mov es, ax                  ; ES:BX = destination buffer
    mov bx, STAGE2_OFF
    mov ax, STAGE2_LBA          ; AX = first LBA to read
    mov dh, STAGE2_SECTORS      ; DH = how many sectors
    call disk_read

    mov si, msg_jump
    call print

    jmp STAGE2_SEG:STAGE2_OFF
```

Set up the arguments, call, print, jump. `disk_read` is the subject of Chapter 6; for now, assume it
works or hangs with a message.

The final `jmp` is far, loading `CS = 0` and `IP = 0x7E00`. Stage 2 has its own `[ORG 0x7E00]` and
therefore needs `CS` to be 0 for the same reason we did.

Note what we do *not* pass to stage 2: nothing. `DL` still holds the boot drive — both `print` and
`disk_read` bracket themselves with `pusha`/`popa`, so nothing has touched it — and stage 2 claims it
in its first few instructions. Passing information between stages through a register that neither
side documents is fragile, which is why
[`stage2.asm`](../spark/boot/stage2.asm) says so explicitly in a comment.

---

## 6. `print`

```nasm
print:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    popa
    ret
```

Eleven instructions, and it is the entire user interface of the boot sector.

**`pusha` / `popa`** save and restore all eight general-purpose registers. In 400 bytes of code it
would be tempting to skip this and document which registers the routine clobbers — but the cost is
two bytes and the benefit is that callers never have to think about it. At this scale, two bytes for
"one fewer thing to get wrong" is the right trade.

**`AH = 0x0E`** selects teletype output (Chapter 4, §4). `BH` is the display page, `BL` the colour in
graphics modes — ignored in text mode, but set to something sane anyway, because some BIOS
implementations do read it.

**`lodsb`** is `AL = [DS:SI]; SI++`, in one byte. This is why `cld` mattered: with `DF` set, `SI`
would decrement.

**`test al, al` / `jz`** is "if (al == 0)". `test` ANDs the operands, discards the result, and keeps
the flags — so `test al, al` sets `ZF` exactly when `AL` is zero. It is one byte shorter than
`cmp al, 0` and does not need an immediate.

The loop is therefore: load a byte, stop if it is NUL, print it, repeat. A C `strlen`-shaped loop in
seven instructions.

---

## 7. `hang`

```nasm
hang:
    cli
    hlt
    jmp hang
```

Three instructions that stop the machine properly.

`cli` clears `IF`. `hlt` halts the CPU until an interrupt arrives — and since `IF` is clear, no
maskable interrupt ever will. The `jmp` catches the one case that can wake it: a non-maskable
interrupt, which ignores `IF`.

Why not just `jmp $`? Because `hlt` puts the processor in a low-power state, while a tight jump loop
runs it at 100%. On a laptop that is the difference between silence and a fan spinning up over a
machine that has stopped doing anything. It also makes the state obvious in a QEMU monitor: a halted
CPU is a deliberate stop, a spinning one might be a runaway loop.

---

## 8. The signature

```nasm
times 510 - ($ - $$) db 0       ; pad with zeros up to offset 510
dw 0xAA55                       ; ...and the two magic bytes
```

`$` is the address of the current line. `$$` is the address of the start of the section — `0x7C00`,
because of `ORG`. So `$ - $$` is "how many bytes have been emitted so far", and `510 - ($ - $$)` is
how many zeros are needed to reach offset 510.

`times N db 0` emits `N` zero bytes.

If the code is too big, `N` is negative and NASM stops with:

```
boot.asm:200: error: TIMES value -13 is negative
```

Thirteen bytes over. There is nothing to do but make the code smaller, and the usual candidates are
the strings — every character of every message costs a byte, and `"Spark: stage1"` plus CRLF plus NUL
is sixteen of them.

`dw 0xAA55` writes the word little-endian, so the bytes on disk are `55 AA`. Both spellings appear in
documentation; they are the same two bytes.

---

## 9. Building and looking at it

```bat
build spark
```

or by hand:

```bash
nasm -f bin spark/boot/boot.asm -o bin/spark/boot.bin
```

`-f bin` is a *flat binary*: no ELF header, no sections, no symbol table. Just the bytes, in order.
That is the only thing that can work here — the BIOS copies 512 bytes and jumps to the first one; it
will not parse a program header table.

Check the size:

```bash
$ wc -c bin/spark/boot.bin
512 bin/spark/boot.bin
```

Always exactly 512, because of the `times` line. And look at the end:

```bash
$ xxd bin/spark/boot.bin | tail -3
000001c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000001d0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000001e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa  ..............U.
```

`55 aa` in the last two bytes. If it is not there, the BIOS will not boot the image and will say
something unhelpful about no bootable device.

And the beginning:

```bash
$ xxd bin/spark/boot.bin | head -2
00000000: ea05 7c00 00fa 31c0 8ed8 8ec0 8ed0 bc00  ..|...1.........
00000010: 7cfb fc88 1636 7cbe 5c7c e839 0031 c08e  |....6|.\|.9.1..
```

`ea 05 7c 00 00` is the far jump: opcode `ea`, then `IP = 0x7C05`, then `CS = 0x0000`. `fa` is `cli`.
`31 c0` is `xor ax, ax`. Being able to read the first few bytes of a boot sector by eye is a genuinely
useful skill when you are trying to work out whether the image on disk is the one you built.

---

## 10. Running it

At this point stage 2 does not exist yet, so `build spark` will fail at the image step. To test just
this file, temporarily replace the `jmp STAGE2_SEG:STAGE2_OFF` with `jmp hang` and build a bare
image:

```bash
nasm -f bin spark/boot/boot.asm -o test.img
qemu-system-i386 -fda test.img -boot a
```

Expected:

```
SeaBIOS (version ...)
Booting from Floppy...
Spark: stage1
Spark: DISK ERROR
```

The disk error is correct — there is nothing at LBA 1 in a 512-byte image, and `disk_read` said so
rather than jumping into nothing. That message appearing is the proof that everything up to it
worked: the far jump, the segment setup, `print`, `lodsb`, and the BIOS call.

If the image is padded out to a full 1.44 MiB, the read succeeds (reading 2 KiB of zeros) and the
machine jumps to `0x7E00`, executes 2 KiB of `00 00` — which disassembles to `add [bx+si], al` — and
eventually wanders off. Chapter 3, §7 noted that a disassembly full of `add [eax],al` means you are
looking at zeroed memory; this is where that first becomes useful.

---

## 11. What could go wrong

| Symptom | Cause |
|---|---|
| "No bootable device" | Signature missing, or the image is not exactly a multiple of 512 |
| Blank screen, no output | `CS` is not 0 and `ORG` is producing wrong addresses — check the far jump |
| Garbage characters | `DF` is set: `cld` missing, or something cleared it |
| Prints once then reboots | The stack is overwriting the code. Check `SP`. |
| "DISK ERROR" every time | Wrong `boot_drive`, or the image geometry does not match the CHS constants |
| Works in QEMU, hangs on hardware | Almost always `sti` missing before an `int 0x13` |

That last row is worth remembering. QEMU's emulated floppy controller completes instantly and does
not need the interrupt; a real one does.

---

## 12. Exercises

🟢 **5.1** Add a third message printed after the disk read succeeds but before the jump. Count how
many bytes it costs you (`wc -c` before and after — the file is always 512, so compare the number of
padding zeros).

🟢 **5.2** Delete the `cld` and set `DF` deliberately with `std` before the first `print`. Predict
what appears on screen, then run it.

🟢 **5.3** Change `mov sp, 0x7C00` to `mov sp, 0x7E00` and explain what will break once stage 2 is
loaded. (It will not break immediately — that is the interesting part.)

🟡 **5.4** Rewrite `print` to use `mov al, [si]` and `inc si` instead of `lodsb`. How many more bytes
is it? Is the `cld` still needed?

🟡 **5.5** Implement option 1 from §4: have [`mkimage.ps1`](../tools/mkimage.ps1) write the actual
size of `stage2.bin`, in sectors, into a known byte of `boot.bin`, and have stage 1 read that byte
instead of using `STAGE2_SECTORS`. This removes a whole class of silent failure.

🟡 **5.6** The boot sector is about 400 bytes. Find 50 bytes to cut without removing functionality.
(Hints: the strings; `pusha`/`popa` in `print`; the retry count.)

🔴 **5.7** Write a boot sector that prints the value of `DL` in hexadecimal and then hangs, and boot
it from `-fda`, `-hda` and a USB image. Confirm the three drive numbers, and explain why hardcoding
`0x00` would have worked for exactly one of them.

---

## What we covered

- The 510-byte budget, and the design rule it forces: stage 1 does the minimum and nothing else.
- `[BITS 16]` and `[ORG 0x7C00]`, and why `ORG` is only correct when `DS` is 0.
- Nine instructions of prologue, each defending against one thing the BIOS does not promise: the
  ambiguous `CS`, the undefined stack, `IF`, `DF`, and the `DL` that is about to be clobbered.
- Why the stack goes at `0x7C00` and grows *away* from the code.
- `print` in eleven instructions, and why `lodsb` depends on `cld`.
- `hlt` in a loop versus `jmp $`, and why the difference is a laptop fan.
- `times 510 - ($ - $$) db 0`, what the error means when it goes negative, and how to read the first
  and last bytes of the output by eye.

[Chapter 6](06-bios-services.md) writes `disk_read`: CHS geometry, the `div` that everyone gets
wrong, retries, and why checking the carry flag is not enough.

---

[← How a PC boots](04-how-a-pc-boots.md) · [Contents](README.md) · [Next: BIOS services →](06-bios-services.md)
