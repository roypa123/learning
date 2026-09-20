; =============================================================================
;  spark/boot/stage2.asm  --  Stage 2: from real mode to a 32-bit C kernel
; =============================================================================
;
;  Stage 1 loaded these 2048 bytes to physical 0x7E00 and jumped here. We are
;  still in 16-bit real mode, but we now have room to breathe, so this is where
;  the interesting work happens:
;
;      1. Load the kernel image off the disk (it is too big for stage 1).
;      2. Enable the A20 gate, without which memory above 1 MiB is invisible.
;      3. Build a Global Descriptor Table describing a flat 4 GiB address space.
;      4. Set CR0.PE and far-jump, which is the actual switch to protected mode.
;      5. Copy the kernel up to 1 MiB and jump into it.
;
;  Step 4 is a one-way door: BIOS services (int 0x10, int 0x13) stop working
;  the instant we set that bit, because they are 16-bit code that assumes real
;  mode segmentation. Everything the kernel needs from the BIOS must be
;  collected *before* the switch. That is why the disk read comes first.
;
;  Explained in: docs/07-protected-mode.md and docs/08-stage2-loader.md
;  Line by line: docs/line-by-line/spark-stage2.md
; =============================================================================

[BITS 16]
[ORG 0x7E00]

KERNEL_LBA        equ 5         ; kernel starts at sector 5 of the image
KERNEL_SECTORS    equ 64        ; 64 * 512 = 32 KiB reserved for the kernel
KERNEL_SEG        equ 0x1000    ; stage the kernel at 0x1000:0x0000 = 0x10000
KERNEL_PHYS       equ 0x100000  ; ...then move it here, to 1 MiB, once we can
                                ;    address memory that high

SECTORS_PER_TRACK equ 18
HEADS             equ 2

CODE_SEG          equ 0x08      ; byte offset of gdt_code within the GDT
DATA_SEG          equ 0x10      ; byte offset of gdt_data within the GDT

; =============================================================================
stage2_start:
    ; Stage 1 left DS/ES/SS at 0 and SP at 0x7C00. Re-establish it anyway:
    ; this file must be correct even if you later load it from somewhere else.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    ; Stage 1 never touched DL after the BIOS handed it over -- both `print`
    ; and `disk_read` bracket themselves with pusha/popa -- so the boot drive
    ; number is still sitting there. Claim it before we start using DX as a
    ; scratch register.
    mov [boot_drive_s2], dl

    mov si, msg_hello
    call print

; -----------------------------------------------------------------------------
;  1. Load the kernel to 0x10000
; -----------------------------------------------------------------------------
;  Why not read all 64 sectors with a single int 0x13 call? Because a CHS read
;  may not cross a track boundary, and a track here is 18 sectors. Some BIOSes
;  handle it, many silently return a short read, and a short read means you
;  jump into a kernel whose second half is whatever was in RAM at power-on.
;
;  One sector per call is slower -- 64 BIOS calls instead of 4 -- and on a real
;  floppy that is a visible pause. It is also always correct, which at this
;  stage of the project is worth vastly more than the speed.
; -----------------------------------------------------------------------------
    mov si, msg_loading
    call print

    mov ax, KERNEL_SEG
    mov es, ax                  ; ES:BX = 0x1000:0x0000
    xor bx, bx
    mov ax, KERNEL_LBA          ; AX = LBA of the next sector to read
    mov cx, KERNEL_SECTORS      ; CX = loop counter for `loop`

.load_one:
    mov dh, 1                   ; exactly one sector
    call disk_read              ; preserves every register (pusha/popa)

    inc ax                      ; next LBA

    ; Advance the destination by 512 bytes. We cannot just add 512 to BX,
    ; because BX is 16 bits and would wrap after 64 KiB -- silently writing
    ; sector 129 over sector 1. Instead we advance the *segment* by 512/16 = 32
    ; paragraphs, which keeps BX at 0 forever and scales past 64 KiB.
    mov dx, es
    add dx, 32
    mov es, dx

    loop .load_one              ; CX--, jump if CX != 0

    mov si, msg_ok
    call print

; -----------------------------------------------------------------------------
;  2. The A20 gate
; -----------------------------------------------------------------------------
;  The 8086 had 20 address lines. A real-mode address seg*16+off can reach
;  0x10FFEF, which needs 21 bits, so on an 8086 it silently wrapped to 0x0000EF.
;  Enough software depended on that wrap that when the 286 arrived with 24
;  address lines, IBM added a gate on line A20 that forces it to zero, and left
;  it *off* at boot for compatibility.
;
;  So: until we open this gate, every odd megabyte of physical memory is
;  aliased onto the even one below it. Loading a kernel to 1 MiB with A20 shut
;  writes it to address 0 instead, over the interrupt vector table.
;
;  There are three ways to open it and no way to know in advance which the
;  machine supports, so we try them in increasing order of brutality and check
;  after each one.
; -----------------------------------------------------------------------------
    call check_a20
    test ax, ax
    jnz .a20_done               ; some BIOSes already enabled it for us

    mov si, msg_a20_bios
    call print
    mov ax, 0x2401              ; int 0x15, AX=0x2401: "enable A20"
    int 0x15
    call check_a20
    test ax, ax
    jnz .a20_done

    mov si, msg_a20_fast
    call print
    in al, 0x92                 ; System Control Port A, on every PS/2 and later
    or al, 0x02                 ; bit 1 = A20 enable
    and al, 0xFE                ; bit 0 = FAST RESET. Writing 1 here reboots the
                                ; machine instantly. Mask it off, always.
    out 0x92, al
    call check_a20
    test ax, ax
    jnz .a20_done

    mov si, msg_a20_kbd
    call print
    call enable_a20_keyboard    ; the original, slowest, most compatible way
    call check_a20
    test ax, ax
    jnz .a20_done

    mov si, msg_a20_fail
    call print
    jmp hang

.a20_done:
    mov si, msg_a20_ok
    call print

; -----------------------------------------------------------------------------
;  3 + 4. Protected mode
; -----------------------------------------------------------------------------
    mov si, msg_pmode
    call print

    cli                         ; From here on there is no going back. The real
                                ; mode IVT at 0x0000 is meaningless in protected
                                ; mode and we have no IDT yet, so any interrupt
                                ; at all -- including the timer, which is
                                ; ticking 18.2 times a second -- would be a
                                ; triple fault. Interrupts stay off until the
                                ; kernel installs an IDT.

    lgdt [gdt_descriptor]       ; tell the CPU where our descriptor table is

    mov eax, cr0
    or  eax, 1                  ; CR0 bit 0 = PE, Protection Enable
    mov cr0, eax                ; <-- protected mode is on *now*

    ; ...but CS still holds a real-mode segment value, and the CPU has already
    ; decoded and cached the next few instructions using real-mode rules. The
    ; only way to reload CS is a far jump, and doing one here also flushes that
    ; prefetch. This instruction is the seam between two different machines.
    jmp CODE_SEG:protected_entry

; =============================================================================
;  Real-mode subroutines
; =============================================================================

; ---- disk_read: identical to the one in stage 1 ----------------------------
;   in: AX = LBA, DH = count, ES:BX = buffer
disk_read:
    pusha
    mov [dr_count], dh

    xor dx, dx
    mov cx, SECTORS_PER_TRACK
    div cx
    inc dx
    mov [dr_sector], dl

    xor dx, dx
    mov cx, HEADS
    div cx
    mov [dr_cylinder], al
    mov [dr_head], dl

    mov di, 3
.attempt:
    mov ah, 0x02
    mov al, [dr_count]
    mov ch, [dr_cylinder]
    mov cl, [dr_sector]
    mov dh, [dr_head]
    mov dl, [boot_drive_s2]
    int 0x13
    jnc .ok
    xor ah, ah
    mov dl, [boot_drive_s2]
    int 0x13
    dec di
    jnz .attempt
    mov si, msg_diskerr
    call print
    jmp hang
.ok:
    popa
    ret

; ---- check_a20: does address 0x100000 alias address 0x000000? ---------------
;   out: AX = 1 if A20 is enabled (no aliasing), 0 if it is not
;
;  We write two different bytes to two addresses that are the same address if
;  and only if A20 is disabled, then read one back. The original contents are
;  saved and restored, because one of those addresses is in the BIOS data area.
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    cli

    xor ax, ax
    mov es, ax
    mov di, 0x0500              ; ES:DI = 0x0000:0x0500 = physical 0x000500

    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510              ; DS:SI = 0xFFFF:0x0510 = 0xFFFF*16 + 0x510
                                ;       = 0x100500, which wraps to 0x000500
                                ;       when A20 is held low.

    mov al, [es:di]             ; save both bytes
    push ax
    mov al, [ds:si]
    push ax

    mov byte [es:di], 0x00      ; write 0x00 low
    mov byte [ds:si], 0xFF      ; write 0xFF high
    cmp byte [es:di], 0xFF      ; did the high write land on the low address?

    pop ax                      ; restore, in reverse order
    mov [ds:si], al
    pop ax
    mov [es:di], al

    mov ax, 0
    je .exit                    ; they aliased -> A20 is OFF -> return 0
    mov ax, 1
.exit:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret

; ---- enable_a20_keyboard: the 1984 method -----------------------------------
;  A20 is wired to a spare output pin of the 8042 keyboard controller, because
;  in 1984 that was the only chip with a pin going spare. This is the reason
;  "enable the A20 line" involves talking to the keyboard.
enable_a20_keyboard:
    cli
    call .wait_in
    mov al, 0xAD                ; 0xAD = disable the keyboard, so it cannot
    out 0x64, al                ;        interfere while we drive the port

    call .wait_in
    mov al, 0xD0                ; 0xD0 = read the controller's output port
    out 0x64, al
    call .wait_out
    in al, 0x60                 ; ...the value arrives on the data port
    push ax

    call .wait_in
    mov al, 0xD1                ; 0xD1 = write the output port
    out 0x64, al
    call .wait_in
    pop ax
    or al, 2                    ; bit 1 of that port *is* the A20 line
    out 0x60, al

    call .wait_in
    mov al, 0xAE                ; re-enable the keyboard
    out 0x64, al
    call .wait_in
    sti
    ret

; Status port 0x64: bit 1 = "input buffer full" (controller is still reading
; the last byte we wrote), bit 0 = "output buffer full" (it has a byte for us).
.wait_in:
    in al, 0x64
    test al, 2
    jnz .wait_in
    ret
.wait_out:
    in al, 0x64
    test al, 1
    jz .wait_out
    ret

; ---- print ------------------------------------------------------------------
print:
    pusha
    mov ah, 0x0E
    mov bh, 0
    mov bl, 7
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    popa
    ret

hang:
    cli
    hlt
    jmp hang

; =============================================================================
;  The Global Descriptor Table
; =============================================================================
;  In protected mode a segment register no longer holds "address / 16". It
;  holds a *selector*: an index into this table. Each 8-byte entry describes a
;  base address, a limit, and a pile of permission bits.
;
;  We want segmentation to stop mattering, so both of our segments describe the
;  same thing: base 0, limit 4 GiB. This is called a "flat" model, and it means
;  a linear address is just an offset, exactly as C expects. Chapter 15 builds
;  a proper GDT with ring 3 segments and a TSS; this one is the minimum that
;  lets us leave real mode.
;
;  Each entry, in the order the bytes appear on disk:
;
;      limit  0:15     low 16 bits of the limit
;      base   0:15     low 16 bits of the base
;      base  16:23     next 8 bits of the base
;      access byte     P DPL S | Type
;      flags + limit   G D/B L AVL | limit 16:19
;      base  24:31     top 8 bits of the base
;
;  The layout is split and out of order because the 386 had to stay
;  byte-compatible with the 286's 6-byte descriptors. There is no better
;  reason than that, and you will meet the same scar tissue in the IDT.
; =============================================================================
align 8
gdt_start:

gdt_null:                       ; selector 0x00
    dq 0                        ; Required to be all zeros. Loading selector 0
                                ; into DS/ES/SS faults, which turns "I forgot
                                ; to set a segment register" into an
                                ; exception instead of silent corruption.

gdt_code:                       ; selector 0x08 -- ring 0, execute/read
    dw 0xFFFF                   ; limit 0:15
    dw 0x0000                   ; base 0:15
    db 0x00                     ; base 16:23
    db 10011010b                ; P=1 DPL=00 S=1 | Type=1010 (code, non-conforming, readable)
    db 11001111b                ; G=1 D=1 L=0 AVL=0 | limit 16:19 = 1111
    db 0x00                     ; base 24:31
                                ; G=1 scales the limit by 4 KiB, so
                                ; 0xFFFFF * 4 KiB = exactly 4 GiB.
                                ; D=1 means 32-bit default operand size.

gdt_data:                       ; selector 0x10 -- ring 0, read/write
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                ; Type=0010 (data, expand-up, writable)
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; limit = size in bytes, MINUS ONE. The CPU
                                ; stores "the offset of the last valid byte",
                                ; so a 24-byte table has limit 23. Writing 24
                                ; here makes the CPU believe in a fourth,
                                ; nonexistent descriptor.
    dd gdt_start                ; base = the *linear* address of the table.
                                ; DS is 0 and ORG is 0x7E00, so the address
                                ; NASM computed for this label is already
                                ; linear. If DS were not 0, it would not be.

; =============================================================================
;  32-bit protected mode starts here
; =============================================================================
[BITS 32]
protected_entry:
    ; CS was loaded by the far jump. Every other segment register still holds
    ; a real-mode value, which is now an invalid selector. Load them all with
    ; the flat data descriptor before touching memory or the stack.
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000            ; a stack at 576 KiB, below the 640 KiB line and
                                ; well clear of both the kernel and the BIOS
                                ; data area. The kernel switches to its own
                                ; stack almost immediately.

    ; ---- Move the kernel from 0x10000 to 0x100000 --------------------------
    ; We could not load it here directly: real-mode addressing tops out at
    ; 0x10FFEF, and A20 was still shut when the load started. Now that we are
    ; in protected mode with a flat 4 GiB segment, 1 MiB is just a number.
    ;
    ; 1 MiB is the traditional home for a kernel because everything below it is
    ; a minefield: the IVT, the BIOS data area, video memory at 0xA0000, option
    ; ROMs, and the BIOS itself at 0xF0000.
    mov esi, KERNEL_SEG * 16    ; 0x10000
    mov edi, KERNEL_PHYS        ; 0x100000
    mov ecx, KERNEL_SECTORS * 512 / 4
    cld
    rep movsd                   ; copy ECX dwords from [ESI] to [EDI]

    jmp KERNEL_PHYS             ; the first instruction of kernel.bin is the
                                ; first byte at 0x100000 -- see link.ld, which
                                ; places entry.asm's code before everything.

; =============================================================================
;  Data
; =============================================================================
[BITS 16]
boot_drive_s2: db 0             ; filled in from DL at stage2_start
dr_count:     db 0
dr_sector:    db 0
dr_head:      db 0
dr_cylinder:  db 0

msg_hello:    db "Spark: stage2 at 0x7E00", 13, 10, 0
msg_loading:  db "Spark: loading kernel...", 13, 10, 0
msg_ok:       db "Spark: kernel loaded", 13, 10, 0
msg_a20_bios: db "Spark: A20 via BIOS", 13, 10, 0
msg_a20_fast: db "Spark: A20 via port 0x92", 13, 10, 0
msg_a20_kbd:  db "Spark: A20 via 8042", 13, 10, 0
msg_a20_ok:   db "Spark: A20 enabled", 13, 10, 0
msg_a20_fail: db "Spark: A20 FAILED", 13, 10, 0
msg_pmode:    db "Spark: entering protected mode", 13, 10, 0
msg_diskerr:  db "Spark: DISK ERROR", 13, 10, 0

; Pad to exactly 4 sectors so the image layout in mkimage.c stays true.
times (4 * 512) - ($ - $$) db 0
