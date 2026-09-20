# Line by line: `spark/boot/stage2.asm`

[Index](README.md) · [Chapter 7](../07-protected-mode.md) · [Chapter 8](../08-stage2-loader.md)

2 KiB. Loads the kernel, opens A20, builds a GDT, and switches the machine from 16 bits to 32.

---

## Constants

```nasm
KERNEL_LBA        equ 5
KERNEL_SECTORS    equ 64
```
⚠️ 32 KiB reserved. The Makefile checks `kernel.bin` against this and fails the build if it is
larger — the failure would otherwise be a half-loaded kernel and a hang.

```nasm
KERNEL_SEG        equ 0x1000
KERNEL_PHYS       equ 0x100000
```
Staged at 64 KiB because real mode cannot reach 1 MiB and A20 is shut at load time. Moved to 1 MiB
after the mode switch.

```nasm
CODE_SEG          equ 0x08
DATA_SEG          equ 0x10
```
Byte offsets into the GDT, not indices.

---

## Entry

```nasm
stage2_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
```
Stage 1 already did this. Repeated because this file must be correct on its own terms.

```nasm
    mov [boot_drive_s2], dl
```
`DL` survived from stage 1 — both its routines bracket with `pusha`/`popa`. Claim it before `DX`
becomes scratch in the load loop.

---

## Loading the kernel

```nasm
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx
    mov ax, KERNEL_LBA
    mov cx, KERNEL_SECTORS
```
`ES:BX = 0x1000:0x0000`. `AX` is the LBA counter, `CX` the `loop` counter.

```nasm
.load_one:
    mov dh, 1
    call disk_read
```
⚠️ **One sector per call.** A CHS read may not cross a track boundary, a track is 18 sectors, and 64
sectors from LBA 5 crosses three. Some BIOSes handle it; many return a short read — which
`disk_read`'s `cmp al` reports as an error rather than as data.

Sixty-four BIOS calls. Slow, and always correct.

```nasm
    inc ax
```
`disk_read` preserves everything (`pusha`/`popa`), so `AX` and `CX` survive with no saving at the
call site.

```nasm
    mov dx, es
    add dx, 32
    mov es, dx
```
⚠️ 512 bytes = 32 paragraphs. Advancing `ES` rather than adding 512 to `BX`, because `BX` is 16 bits
and would wrap after 128 sectors — writing sector 129 over sector 1, silently.

Our kernel is 64 sectors so the wrap would not happen. The bug would appear the month the kernel grew
past 64 KiB.

`DX` is scratch here and `disk_read` reads the drive from memory rather than `DL`, which is what
makes this safe.

```nasm
    loop .load_one
```

---

## `check_a20`

```nasm
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    cli
```
Saves everything, including flags. `cli` because an interrupt handler running mid-test would see two
bytes of memory holding test patterns.

```nasm
    xor ax, ax
    mov es, ax
    mov di, 0x0500
```
`ES:DI = 0x0000:0x0500` = physical `0x000500`. The first byte past the BIOS data area — but "past"
is not "unused", hence the save/restore below.

```nasm
    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510
```
`DS:SI` = `0xFFFF × 16 + 0x510` = `0x100500`, which wraps to `0x000500` when A20 is held low.

```nasm
    mov al, [es:di]
    push ax
    mov al, [ds:si]
    push ax
```
Save both bytes.

```nasm
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
    cmp byte [es:di], 0xFF
```
Write different values to the two addresses, read one back. If the high write landed on the low
address, they are aliased.

```nasm
    pop ax
    mov [ds:si], al
    pop ax
    mov [es:di], al
```
Restore in reverse order — stack discipline.

```nasm
    mov ax, 0
    je .exit
    mov ax, 1
.exit:
```
Equal means aliased means A20 **off**, return 0. Note `mov ax, 0` does not affect flags, so the `je`
still sees the `cmp`'s result.

---

## Opening A20

```nasm
    call check_a20
    test ax, ax
    jnz .a20_done
```
🔧 Some BIOSes enable it for us. Testing first costs nothing.

```nasm
    mov ax, 0x2401
    int 0x15
    call check_a20
```
The polite method. We do not trust its return value — we re-test.

```nasm
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
```
Port `0x92` bit 1 is A20.

⚠️ **Bit 0 is FAST RESET.** Writing 1 reboots the machine instantly. `and al, 0xFE` is not optional
in a read-modify-write.

```nasm
    call enable_a20_keyboard
```
The 1984 method, if the first two failed.

```nasm
    mov si, msg_a20_fail
    call print
    jmp hang
```
All four failed. Stop with a message rather than loading a kernel that will land at address 0.

---

## `enable_a20_keyboard`

```nasm
    call .wait_in
    mov al, 0xAD
    out 0x64, al
```
Disable the keyboard so it cannot interfere while we drive the controller.

```nasm
    mov al, 0xD0
    out 0x64, al
    call .wait_out
    in al, 0x60
    push ax
```
Read the controller's output port.

```nasm
    mov al, 0xD1
    out 0x64, al
    call .wait_in
    pop ax
    or al, 2
    out 0x60, al
```
**Bit 1 of that port is the A20 line**, because in 1984 the 8042 was the only chip with a spare
output pin.

```nasm
.wait_in:
    in al, 0x64
    test al, 2
    jnz .wait_in
```
Status bit 1 = input buffer full. The 8042 runs at about 8 MHz and needs time between bytes.

```nasm
.wait_out:
    in al, 0x64
    test al, 1
    jz .wait_out
```
Bit 0 = output buffer full.

---

## The GDT

```nasm
align 8
gdt_start:
gdt_null:
    dq 0
```
Required to be all zeros. Makes selector 0 invalid, so an uninitialised segment register faults
rather than addressing something plausible.

```nasm
gdt_code:
    dw 0xFFFF       ; limit 0:15
    dw 0x0000       ; base 0:15
    db 0x00         ; base 16:23
    db 10011010b    ; P=1 DPL=00 S=1 Type=1010
    db 11001111b    ; G=1 D=1 L=0 AVL=0 | limit 16:19
    db 0x00         ; base 24:31
```
Access byte: present, ring 0, code/data segment, executable, non-conforming, readable, not accessed.

Granularity byte: `G=1` scales the limit by 4 KiB so `0xFFFFF` becomes 4 GiB; `D=1` means 32-bit
default operand size.

⚠️ `D=0` would make the CPU treat this as a 16-bit segment and every 32-bit instruction would need a
prefix — which is to say, execute something else entirely.

```nasm
gdt_data:
    ...
    db 10010010b    ; Type=0010: data, expand-up, writable
```
Bit 3 clear (not executable), bit 2 is expand-down rather than conforming, bit 1 is writable.

```nasm
gdt_descriptor:
    dw gdt_end - gdt_start - 1
```
⚠️ **Minus one.** The CPU stores the offset of the last valid byte. Writing 24 for a 24-byte table
creates a fourth descriptor made of whatever follows.

```nasm
    dd gdt_start
```
Must be a **linear** address. `DS` is 0 and `ORG` is `0x7E00`, so the label's value is already
linear. It would not be if `DS` were non-zero.

---

## The switch

```nasm
    cli
```
Permanent this time. The real-mode IVT is meaningless in protected mode and we have no IDT; any
interrupt — including the timer at 18.2 Hz — would triple-fault.

```nasm
    lgdt [gdt_descriptor]
```
Loads the register. Changes no segment register; the CPU keeps using cached real-mode descriptors.

```nasm
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
```
Protected mode is on **now**. But `CS` still holds a real-mode value and the prefetch queue holds
instructions decoded under the old rules.

```nasm
    jmp CODE_SEG:protected_entry
```
🔧 The seam. Reloads `CS` from the GDT — the only way, since `CS` is not assignable — and flushes the
prefetch. Without a far jump the behaviour is undefined.

---

## 32-bit

```nasm
[BITS 32]
protected_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
```
⚠️ Every segment register except `CS` still holds a real-mode value, now an invalid selector. Forget
`SS` and the next `push` faults; forget `DS` and the next memory access does. Both present as a
triple fault immediately after the switch.

```nasm
    mov esp, 0x90000
```
576 KiB: below the 640 KiB line, above everything loaded, clear of the BIOS data area.

```nasm
    mov esi, KERNEL_SEG * 16
    mov edi, KERNEL_PHYS
    mov ecx, KERNEL_SECTORS * 512 / 4
    cld
    rep movsd
```
8192 dwords from `0x10000` to `0x100000`.

`cld` again — thirty instructions and a BIOS call have happened since the last one.

```nasm
    jmp KERNEL_PHYS
```
The first byte at `0x100000` is `_start`, guaranteed by `link.ld`'s `*(.text.entry)`.

---

## Padding

```nasm
times (4 * 512) - ($ - $$) db 0
```
⚠️ Exactly 4 sectors, so the image layout in `mkimage.ps1` stays true. Growing past this is a build
error from the `times`, and `mkimage`'s overlap check catches the other direction.

---

[Index](README.md) · [Chapter 7](../07-protected-mode.md) · [Chapter 8](../08-stage2-loader.md)
