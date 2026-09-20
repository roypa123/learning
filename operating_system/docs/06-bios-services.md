# Chapter 6 — BIOS services: text and disk

[← The boot sector](05-boot-sector.md) · [Contents](README.md) · [Next: Protected mode →](07-protected-mode.md)

> 📖 **Line by line:** [boot.asm](line-by-line/spark-boot.md)

---

## Goal

Write `disk_read`, the routine that turns a sector number into bytes in memory. It is thirty
instructions and it contains the single most common bug in boot sector code, three separate things
that can silently half-succeed, and a conversion that most people get wrong the first time.

By the end of this chapter Spark can read its own disk.

---

## 1. What the BIOS is, from software's point of view

The firmware installed handlers in the interrupt vector table before it jumped to us (Chapter 4,
§2). Calling one is `int N` with arguments in registers — an ABI where the "function number" goes in
`AH` and the return convention is "carry flag set means failure".

It is a library with a strange calling convention, and it is the only library we have.

Two things about it shape everything in this chapter.

**It is from 1981.** The interfaces were designed for a machine with one floppy drive and 16 KiB of
RAM. `int 0x13` speaks Cylinder/Head/Sector because in 1981 that is what a disk *was*; the idea of a
flat sector number came later.

**It disappears at the end of Chapter 7.** The moment `CR0.PE` is set, none of this works. So the
order of operations in a bootloader is forced: everything you need from the BIOS, you take first.

---

## 2. CHS: how disks were addressed

A physical disk is a stack of platters. Each platter surface has a read/write head. Each surface is
divided into concentric tracks, and each track into sectors.

```
        cylinder 0     cylinder 1     cylinder 2
       +------------+ +------------+ +------------+
head 0 |  18 sectors| |  18 sectors| |  18 sectors|   <- top of platter
       +------------+ +------------+ +------------+
head 1 |  18 sectors| |  18 sectors| |  18 sectors|   <- bottom of platter
       +------------+ +------------+ +------------+
```

A *cylinder* is all the tracks at the same radius across every surface — so called because the heads
move together, and the set of tracks they can reach without moving forms a cylinder.

To name a sector you give three numbers: which cylinder, which head, which sector within that track.
A 1.44 MiB floppy is 80 cylinders × 2 heads × 18 sectors × 512 bytes = 1,474,560 bytes, which is
exactly the size [`mkimage.ps1`](../tools/mkimage.ps1) produces. That is not a coincidence: QEMU
infers the geometry from the file size, so an image of any other size gets a different geometry and
our arithmetic stops matching.

### LBA, and why we still have to convert

Logical Block Addressing numbers the sectors 0, 1, 2, … and lets the drive worry about where they
physically are. It is obviously better, and every disk since about 1994 works this way internally
regardless of what the interface says.

But `int 0x13, AH=0x02` predates it, so we convert:

```
    sector   = (LBA % SECTORS_PER_TRACK) + 1
    head     = (LBA / SECTORS_PER_TRACK) % HEADS
    cylinder = (LBA / SECTORS_PER_TRACK) / HEADS
```

**Sectors are numbered from 1.** Cylinders and heads count from 0; sectors count from 1. There is no
sector 0 in CHS addressing. This inconsistency has existed since the IBM PC and has caused an
uncountable number of off-by-one errors — including, if you skip the `inc`, one that reads the last
sector of the previous track and produces plausible-looking wrong data.

---

## 3. The conversion, in assembly

```nasm
disk_read:
    pusha
    mov [dr_count], dh          ; stash the count; AL is about to be needed

    xor dx, dx                  ; DX:AX = LBA (zero-extended to 32 bits)
    mov cx, SECTORS_PER_TRACK
    div cx                      ; AX = LBA / 18, DX = LBA % 18
    inc dx                      ; sectors are numbered from 1, not 0
    mov [dr_sector], dl

    xor dx, dx                  ; DX:AX = LBA / 18
    mov cx, HEADS
    div cx                      ; AX = cylinder, DX = head
    mov [dr_cylinder], al
    mov [dr_head], dl
```

### 3.1 The bug everyone writes

```nasm
    xor dx, dx
```

`div cx` divides the 32-bit value `DX:AX` by `CX`. Not `AX` by `CX` — `DX:AX`.

If `DX` holds anything left over from earlier code, the dividend is `DX × 65536 + AX`, which is
enormous, and the quotient does not fit in `AX`. The CPU raises a **divide error** — exception 0 —
and in real mode that means jumping through the interrupt vector table to whatever the BIOS installed
at vector 0, which is usually a hang.

So `xor dx, dx` before every 16-bit `div` where the dividend is really 16 bits. Every time. It is
two bytes and it is the difference between working and a boot that stops with no message.

### 3.2 Why two divides and not one

The first `div` gives us the sector within the track (`DX`) and the track number (`AX`). The second
splits the track number into head and cylinder.

The intermediate value `AX = LBA / 18` is "which track, counting across all surfaces". Dividing
*that* by the number of heads gives the cylinder, and the remainder is which head. Each `div`
consumes the remainder of the previous one, which is why `DX` must be re-zeroed in between.

### 3.3 `BX` survives

The destination buffer is in `ES:BX`, set up by the caller. `div` writes `AX` and `DX` and touches
nothing else, so `BX` is still correct when we make the BIOS call. That is worth checking rather than
assuming — a version of this routine that used `BX` as scratch would need to save it, and the bug
would be that the sector lands at the wrong address.

---

## 4. Making the call

```nasm
    mov di, 3                   ; three attempts
.attempt:
    mov ah, 0x02                ; function: read sectors into memory
    mov al, [dr_count]          ; AL = number of sectors
    mov ch, [dr_cylinder]       ; CH = cylinder (low 8 bits)
    mov cl, [dr_sector]         ; CL = sector (bits 0-5), cylinder high bits 6-7
    mov dh, [dr_head]           ; DH = head
    mov dl, [boot_drive]        ; DL = drive
    int 0x13
    jnc .verify
```

Six registers, and one of them is doing two jobs.

### 4.1 `CL` holds two fields

```
     7  6  5  4  3  2  1  0
    +-----+-----------------+
    | cyl |     sector      |
    | 9:8 |      1..63      |
    +-----+-----------------+
```

The low six bits are the sector number, 1–63. The top two bits are cylinder bits 8 and 9, because
`CH` only holds eight and the format needed ten.

Ten bits of cylinder, eight of head, six of sector: 1024 × 256 × 63 × 512 bytes = **8.4 GB**, the
famous limit that hard disks hit in 1997 and which `int 0x13` extensions (`AH = 0x42`) exist to get
past.

Our cylinder count is 80, so it fits in `CH` and the top two bits of `CL` stay zero. That is why the
code writes `mov [dr_cylinder], al` and never touches the high bits — correct for a floppy, and a
bug waiting to happen on anything bigger. Exercise 6.4 fixes it.

### 4.2 The retry loop

```nasm
    jnc .verify

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13                    ; AH=0 : reset the disk controller
    dec di
    jnz .attempt
    jmp disk_error
```

Floppy reads fail at random. Not often, but often enough that every BIOS-era bootloader retries, and
the conventional count is three.

`AH = 0x00` resets the disk system, which on a real floppy re-seeks the head to track 0. Most
transient failures are a mis-seek, so re-seeking and trying again genuinely fixes them.

In QEMU this never triggers. On real hardware, with a real floppy, it is the difference between
booting and not.

### 4.3 The check that is not the carry flag

```nasm
.verify:
    cmp al, [dr_count]
    jne disk_error
```

`int 0x13` returns the number of sectors **actually read** in `AL`. A short read — ask for four, get
two — does not set `CF`.

So a loader that checks only the carry flag will believe it has stage 2 in memory when it has half of
stage 2 in memory, and jump into it. The symptom is a machine that executes the first half of a
program and then runs off the end into zeros.

Two instructions to catch it. Note the comment in the source:

```nasm
    ; AL still holds the returned count here -- nothing between the int 0x13
    ; and this compare touches it.
```

That comment is there because an earlier draft of this routine had `mov al, [dr_count]` before the
compare, which made the test compare a value against itself and always pass. Being explicit about
which register holds what across a jump is worth the line.

---

## 5. Why one sector at a time in stage 2

[`stage2.asm`](../spark/boot/stage2.asm) reads the kernel like this:

```nasm
    mov cx, KERNEL_SECTORS      ; 64
.load_one:
    mov dh, 1                   ; exactly one sector
    call disk_read
    inc ax
    mov dx, es
    add dx, 32                  ; advance ES by 512 bytes
    mov es, dx
    loop .load_one
```

Sixty-four BIOS calls instead of one. The reason is in the comment:

> a CHS read may not cross a track boundary, and a track here is 18 sectors. Some BIOSes handle it,
> many silently return a short read.

A track is 18 sectors. Reading 64 sectors starting at LBA 5 crosses three track boundaries. The
specification does not require the BIOS to handle that, and the failure mode — a short read — is
exactly the one that `.verify` catches but that a naive loader would not.

One sector per call is slower. On a real floppy it is a visible pause. It is also always correct, and
at this stage of a project that is worth vastly more.

### The segment advance

```nasm
    mov dx, es
    add dx, 32
    mov es, dx
```

512 bytes is 32 paragraphs (Chapter 2, §3.1). We advance `ES` rather than `BX` because `BX` is 16
bits and `0x0000 + 512 × 128` wraps to zero — sector 129 would be written over sector 1, silently.
Advancing the segment keeps `BX` at zero forever and scales past 64 KiB.

This is the sort of thing that only bites when the kernel grows past 64 KiB, which is to say three
months after you wrote the bug.

---

## 6. `int 0x10`, and the other video functions

We use exactly one:

```nasm
    mov ah, 0x0E
    mov al, 'X'
    int 0x10
```

The others worth knowing, because they occasionally save you:

| `AH` | Function |
|---|---|
| `0x00` | Set video mode. `AL = 0x03` is 80×25 colour text; `AL = 0x13` is 320×200 256-colour graphics. |
| `0x01` | Set cursor shape, including "invisible" |
| `0x02` | Set cursor position, `DH` = row, `DL` = column |
| `0x03` | Read cursor position |
| `0x06` | Scroll a window up |
| `0x09` | Write a character with an attribute, without advancing |
| `0x0E` | Teletype output — the one we use |
| `0x13` | Write a whole string |

`AH = 0x00` with `AL = 0x03` is a useful emergency measure: it resets the display to a known text
mode and clears it, which can rescue a screen that an earlier experiment left in a graphics mode.

`AH = 0x13` would let us print a string with one call instead of a loop, but it takes the length in
`CX` and the position in `DH:DL`, so the setup is longer than the loop it replaces. In 510 bytes,
the loop wins.

---

## 7. `int 0x13` extensions, which we do not use

The modern interface, available on hard disks since about 1996:

```nasm
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, disk_packet
    int 0x13

disk_packet:
    db 0x10         ; size of this packet
    db 0            ; reserved
    dw 4            ; number of sectors
    dw 0x7E00       ; destination offset
    dw 0x0000       ; destination segment
    dq 1            ; the LBA, 64 bits
```

No geometry, no conversion, a 64-bit sector number, and it crosses track boundaries because there are
no tracks as far as the interface is concerned.

We do not use it in Spark for one reason: **floppies do not support it.** `AH = 0x41` checks
availability, and on `-fda` it reports not-present. Since Spark boots from a floppy image so that the
CHS arithmetic has something real to work on, `AH = 0x02` is the only option.

If you change Spark to boot from `-hda`, switching to `AH = 0x42` removes `disk_read`'s two divides
and the entire geometry question. Exercise 6.6.

---

## 8. Running it

With both stages present:

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
```

Those last two lines are the proof. `Spark: -> stage2` is printed by stage 1 after a successful,
verified four-sector read. `Spark: stage2 at 0x7E00` is printed by *stage 2*, which means the bytes
on the disk at LBA 1 are now executing.

### Watching the reads

```bat
run spark
```
won't show you the disk traffic. This will:

```bash
qemu-system-i386 -fda bin/spark.img -boot a -d int -D bin/qemu.log
```

and then in the log, look for `int 13` entries:

```
   13: v=13 e=0000 i=1 cpl=0 IP=0000:7c5e pc=00007c5e SP=0000:7bfa
   AX=0204 BX=7e00 CX=0002 DX=0000 ...
```

`AX=0204` is `AH=0x02, AL=4` — read four sectors. `CX=0002` is cylinder 0, sector 2. `DX=0000` is
head 0, drive 0. `BX=7e00` is the destination.

LBA 1 → cylinder 0, head 0, sector 2. Check it against the formula: `1 % 18 = 1`, `+1 = 2`. ✓

Being able to read that line and confirm the arithmetic is the whole point of this chapter.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| Hangs immediately after `stage1` | `div` overflowed — `DX` was not zeroed |
| "DISK ERROR" always | Wrong drive in `DL`, or the image geometry does not match the constants |
| "DISK ERROR" sometimes | Genuinely flaky media, and the retry loop ran out |
| Jumps to `0x7E00` and crashes | Short read not detected — check the `cmp al` |
| Reads the wrong data | Off by one in the sector: the `inc dx` is missing |
| Works for 64 KiB, then corrupts | `BX` wrapped: the segment advance is missing |
| Works in QEMU, fails on hardware | Multi-track read, or `IF` clear during `int 0x13` |

---

## 10. Exercises

🟢 **6.1** Compute the CHS triple for LBA 0, 17, 18, 35, 36 and 1000 on a 1.44 MiB floppy. Check LBA
18 carefully — it is the first one that crosses a track.

🟢 **6.2** Remove the `inc dx` and boot. What gets loaded instead of stage 2, and why does it not
produce an error?

🟢 **6.3** Remove the `cmp al, [dr_count]` check, then build an image where `stage2.bin` is only two
sectors long but `STAGE2_SECTORS` still says four. Describe what happens.

🟡 **6.4** The cylinder is written only to `CH`, so cylinders above 255 are silently wrong. Fix it:
put bits 8–9 of the cylinder into the top two bits of `CL`. Then work out the largest LBA the fixed
version can address.

🟡 **6.5** Instrument the retry loop: print a `.` on each retry. Then use QEMU's
`-drive if=floppy,file=...,werror=...` options, or simply corrupt the image, to see it fire.

🟡 **6.6** Convert Spark to boot from `-hda` using `int 0x13 AH=0x42`. You will need to build the
disk address packet, remove both divides, and change `run.bat`. Note how much shorter `disk_read`
becomes.

🔴 **6.7** Make stage 1 read *only one* sector of stage 2, check a magic number and a length at the
start of it, and then read the rest based on that length. This is option 2 from Chapter 5, §4, and it
is how a real loader avoids compile-time constants.

---

## What we covered

- The BIOS as a library with a 1981 calling convention, and the deadline after which it stops
  existing.
- CHS geometry, the LBA conversion, and the fact that sectors count from 1 while everything else
  counts from 0.
- `div` divides `DX:AX`, so `xor dx, dx` is mandatory — the most common boot sector bug there is.
- `CL` carrying two fields, the 8.4 GB limit that falls out of it, and where our code is quietly
  wrong for large disks.
- Retries with a controller reset, and why QEMU never shows you that they matter.
- The short-read check, which the carry flag does not give you.
- Why stage 2 reads one sector at a time, and why the destination advances by segment rather than
  by offset.
- Reading `-d int` output well enough to verify the CHS arithmetic against the log.

[Chapter 7](07-protected-mode.md) is the mode switch: the A20 gate, a Global Descriptor Table built
by hand, and the one instruction after which the machine is a different computer.

---

[← The boot sector](05-boot-sector.md) · [Contents](README.md) · [Next: Protected mode →](07-protected-mode.md)
