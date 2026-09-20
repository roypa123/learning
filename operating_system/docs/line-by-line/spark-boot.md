# Line by line: `spark/boot/boot.asm`

[Index](README.md) · [Chapter 5](../05-boot-sector.md) · [Chapter 6](../06-bios-services.md)

The 512-byte boot sector. Every byte.

---

## Directives

```nasm
[BITS 16]
```
Emit 16-bit encodings. The same mnemonic assembles differently in 16- and 32-bit modes; get this
wrong and every instruction after the first is misaligned garbage.

```nasm
[ORG 0x7C00]
```
Assume this load address when computing label addresses. Does not move anything.

⚠️ Only correct when `DS` is 0, because the CPU computes `DS × 16 + offset`. The far jump below is
what guarantees that.

---

## Constants

```nasm
STAGE2_SEG      equ 0x0000
STAGE2_OFF      equ 0x7E00
```
`0x7E00` is the byte immediately after our 512.

```nasm
STAGE2_LBA      equ 1
STAGE2_SECTORS  equ 4
```
⚠️ Compile-time constants, not read from the image. If `stage2.bin` grows past 2 KiB, stage 1 loads
four sectors of a five-sector program and jumps into it. `mkimage.ps1`'s overlap check catches the
image side; nothing catches this side. Exercise 5.5.

```nasm
SECTORS_PER_TRACK equ 18
HEADS             equ 2
```
1.44 MiB floppy geometry. ⚠️ Must match the image size exactly — QEMU infers geometry from the file
size, so an image that is not 1,474,560 bytes gets different numbers and every CHS read lands
elsewhere.

---

## Entry

```nasm
start:
    jmp 0x0000:.canonical
.canonical:
```
The BIOS may jump as `0000:7C00` or `07C0:0000` — both physical `0x7C00`, differing only in `CS`.
A far jump forces `CS = 0`, which is what `ORG` needs.

Five bytes. Removes a class of bug that manifests as "works in QEMU, reboots on my laptop".

```nasm
    cli
```
`SS:SP` is undefined right now. An interrupt would push three values onto an address we do not
control. The real-mode timer fires 18.2 times a second, so the window is small and real.

```nasm
    xor ax, ax
```
Two bytes. `mov ax, 0` is three, and at this scale that matters.

```nasm
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
```
Cannot write an immediate into a segment register, hence the trip through `AX`.

`DS = 0` so `[label]` means physical `0x7C00 + label`. `ES = 0` because `int 0x13` reads into
`ES:BX`. `SS:SP` puts the stack just below us, growing *down* into 30 KiB of free memory — away from
the code, which grows up.

```nasm
    sti
```
Not undoing the `cli` pointlessly: that window is closed.

🔧 Required. `int 0x13` on a real floppy controller waits for an interrupt from the drive, and with
`IF` clear it waits forever. This is the most common reason a boot sector that works in QEMU hangs on
hardware.

```nasm
    cld
```
The BIOS is not required to leave `DF` clear, and `print` uses `lodsb`. With `DF` set it walks
backwards and prints garbage.

```nasm
    mov [boot_drive], dl
```
The BIOS passes the boot drive in `DL`: `0x00` floppy, `0x80` first hard disk. ⚠️ `div` clobbers `DX`
within twenty instructions, so this must be early.

---

## Loading stage 2

```nasm
    mov ax, STAGE2_SEG
    mov es, ax
    mov bx, STAGE2_OFF
    mov ax, STAGE2_LBA
    mov dh, STAGE2_SECTORS
    call disk_read
```
Set up `disk_read`'s four inputs. Note `ES:BX` is the buffer and `AX` is reloaded after `ES` is set —
order matters because both pass through `AX`.

```nasm
    jmp STAGE2_SEG:STAGE2_OFF
```
Far, so `CS = 0` for stage 2's own `[ORG 0x7E00]`.

`DL` still holds the boot drive — `print` and `disk_read` both bracket with `pusha`/`popa` — and
stage 2 claims it in its first instructions.

---

## `disk_read`

```nasm
disk_read:
    pusha
    mov [dr_count], dh
```
`AL` is about to be needed for the sector count, so stash it.

```nasm
    xor dx, dx
```
⚠️ **The most common boot sector bug.** `div cx` divides `DX:AX`, not `AX`. Leftover `DX` makes the
dividend enormous, the quotient overflows `AX`, and the CPU raises `#DE` — which in real mode jumps
through the IVT to whatever the BIOS installed, usually a hang.

```nasm
    mov cx, SECTORS_PER_TRACK
    div cx
```
`AX = LBA / 18` (the track index), `DX = LBA % 18`.

```nasm
    inc dx
    mov [dr_sector], dl
```
⚠️ Sectors count from **1**. Cylinders and heads count from 0. No sector 0 exists in CHS. Omitting
this reads the last sector of the previous track — plausible-looking wrong data.

```nasm
    xor dx, dx
    mov cx, HEADS
    div cx
    mov [dr_cylinder], al
    mov [dr_head], dl
```
Zero `DX` again — the first `div` left a remainder there. Second divide splits the track index into
cylinder and head.

⚠️ `AL` only, so cylinders above 255 are silently wrong. Correct for an 80-cylinder floppy; broken
for anything larger. The fix is bits 8–9 into the top of `CL`. Exercise 6.4.

`BX` survives: `div` writes only `AX` and `DX`.

```nasm
    mov di, 3
.attempt:
```
🔧 Three attempts. Floppy reads fail at random often enough that every BIOS-era loader retries.
Never triggers in QEMU.

```nasm
    mov ah, 0x02
    mov al, [dr_count]
    mov ch, [dr_cylinder]
    mov cl, [dr_sector]
    mov dh, [dr_head]
    mov dl, [boot_drive]
    int 0x13
```
`CL` carries two fields: bits 0–5 the sector (1–63), bits 6–7 cylinder bits 8–9. Ten bits of
cylinder, eight of head, six of sector = the 8.4 GB limit.

```nasm
    jnc .verify
```
Carry clear = success.

```nasm
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    dec di
    jnz .attempt
    jmp disk_error
```
`AH = 0` resets the disk system; on a real floppy it re-seeks to track 0, which fixes the most common
transient failure.

```nasm
.verify:
    cmp al, [dr_count]
    jne disk_error
```
⚠️ `int 0x13` returns the number of sectors **actually** read in `AL`, and a short read does **not**
set CF. A loader that checks only the carry flag jumps into half-loaded code.

`AL` still holds the returned count here — nothing between the `int 0x13` and this compare touches
it.

```nasm
    popa
    ret
```

---

## `print`

```nasm
print:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
```
`AH = 0x0E` is teletype output. `BH` is the display page; `BL` is the colour, ignored in text mode
but set anyway because some BIOSes read it.

```nasm
.next:
    lodsb
```
`AL = [DS:SI]; SI++`, one byte. Depends on `cld`.

```nasm
    test al, al
    jz .done
```
`test` ANDs and discards, keeping flags. One byte shorter than `cmp al, 0`.

```nasm
    int 0x10
    jmp .next
.done:
    popa
    ret
```

---

## `hang`

```nasm
hang:
    cli
    hlt
    jmp hang
```
`hlt` parks the CPU in a low-power state; `jmp $` would spin it at 100%. With `IF` clear no maskable
interrupt will ever wake it; the `jmp` catches an NMI.

---

## Data

```nasm
boot_drive:  db 0
dr_count:    db 0
dr_sector:   db 0
dr_head:     db 0
dr_cylinder: db 0
```
Five bytes of state. Registers are too scarce to hold them across the `div`s.

```nasm
msg_stage1:  db "Spark: stage1", 13, 10, 0
```
⚠️ Every character costs a byte of the 510. This string is 16 bytes.

---

## The signature

```nasm
times 510 - ($ - $$) db 0
```
`$` is the current address; `$$` is `0x7C00` because of `ORG`; `$ - $$` is how many bytes have been
emitted.

A negative count is `error: TIMES value -13 is negative` — the sector is 13 bytes too big, and the
only fix is less code. The strings are usually the place to look.

```nasm
dw 0xAA55
```
Little-endian, so the bytes on disk are `55 AA`. Both spellings appear in documentation.

⚠️ Without it the BIOS prints "no bootable device" and you spend twenty minutes assuming your code
crashed. `mkimage.ps1` checks it at build time for exactly this reason.

---

[Index](README.md) · [Chapter 5](../05-boot-sector.md) · [Chapter 6](../06-bios-services.md)
