; =============================================================================
;  spark/boot/boot.asm  --  Stage 1: the boot sector
; =============================================================================
;
;  This file becomes the first 512 bytes of the disk image. The BIOS loads it
;  to physical address 0x00007C00 and jumps to it with the CPU in 16-bit real
;  mode. That is the entire contract. Everything else is up to us.
;
;  Our job here is deliberately tiny, because 512 bytes minus the 2-byte
;  signature minus a couple of strings is not much room:
;
;      1. Put the CPU into a state we chose, rather than the state the BIOS
;         happened to leave it in (segments, stack, direction flag).
;      2. Remember which drive we were booted from.
;      3. Load stage 2 off that drive into memory.
;      4. Jump to it.
;
;  Explained in: docs/05-boot-sector.md and docs/06-bios-services.md
;  Line by line: docs/line-by-line/spark-boot.md
; =============================================================================

[BITS 16]                       ; assemble 16-bit instructions: we are in real mode
[ORG 0x7C00]                    ; ...and every label is an address relative to 0x7C00

; ---- Where things live -------------------------------------------------------
; The disk image is laid out by tools/mkimage.c like this:
;
;     LBA 0        this file (512 bytes, ends with 0xAA55)
;     LBA 1..4     stage2.bin (padded to exactly 4 sectors)
;     LBA 5..68    kernel.bin (padded to exactly 64 sectors = 32 KiB)
;
; Sector counts are compile-time constants rather than something we read from
; the image. That is a real limitation -- docs/05 discusses the alternatives --
; but it keeps stage 1 small enough to fit, which is the binding constraint.

STAGE2_SEG      equ 0x0000      ; load stage 2 at 0x0000:0x7E00, which is
STAGE2_OFF      equ 0x7E00      ; physical 0x7E00: the byte right after us
STAGE2_LBA      equ 1           ; first sector of stage 2 on the disk
STAGE2_SECTORS  equ 4           ; how many sectors stage 2 occupies

SECTORS_PER_TRACK equ 18        ; 1.44 MiB floppy geometry: 80 cylinders,
HEADS             equ 2         ; 2 heads, 18 sectors per track.

; =============================================================================
;  Entry point
; =============================================================================
start:
    ; The BIOS jumped here. It may have jumped as 0x0000:0x7C00 or as
    ; 0x07C0:0x0000 -- both are physical 0x7C00, but they give CS different
    ; values, and [ORG 0x7C00] only produces correct addresses for the first.
    ; A far jump to an explicit CS:IP pair forces the question closed.
    jmp 0x0000:.canonical
.canonical:

    cli                         ; no interrupts while the stack is undefined:
                                ; an IRQ right now would push onto garbage.

    xor ax, ax                  ; AX = 0 (two bytes; "mov ax, 0" is three)
    mov ds, ax                  ; DS = 0 so [label] means physical 0x7C00+label
    mov es, ax                  ; ES = 0 so ES:BX disk reads land where we think
    mov ss, ax                  ; SS = 0, and...
    mov sp, 0x7C00              ; ...SP just below us. The stack grows *down*
                                ; from 0x7C00 into the 30 KiB of free memory
                                ; below, so it never touches our own code.

    sti                         ; interrupts back on: int 0x10 and int 0x13 are
                                ; BIOS services and some BIOSes need IRQs live.

    cld                         ; string instructions count upwards. The BIOS
                                ; is not required to leave DF clear and lodsb
                                ; in our print routine depends on it.

    mov [boot_drive], dl        ; The BIOS passes the drive it booted from in
                                ; DL: 0x00 = first floppy, 0x80 = first hard
                                ; disk. We must read from the *same* drive, so
                                ; save it before anything clobbers DL.

    mov si, msg_stage1
    call print

; -----------------------------------------------------------------------------
;  Load stage 2
; -----------------------------------------------------------------------------
    mov ax, STAGE2_SEG
    mov es, ax                  ; ES:BX = destination buffer
    mov bx, STAGE2_OFF
    mov ax, STAGE2_LBA          ; AX = first LBA to read
    mov dh, STAGE2_SECTORS      ; DH = how many sectors
    call disk_read              ; ...or it prints an error and hangs

    mov si, msg_jump
    call print

    ; Hand over. DL still holds the boot drive (disk_read preserves it via
    ; pusha/popa) and stage 2 reads it straight back out of memory anyway.
    jmp STAGE2_SEG:STAGE2_OFF

; =============================================================================
;  disk_read  --  read sectors using BIOS int 0x13, AH=0x02 (CHS read)
; =============================================================================
;   in:  AX     = starting LBA (logical block address, 0-based)
;        DH     = sector count (1..127; in practice keep it inside one track)
;        ES:BX  = destination buffer
;   out: buffer filled; on failure, jumps to disk_error and never returns
;   clobbers: nothing (pusha/popa)
;
;  int 0x13 AH=0x02 speaks CHS -- Cylinder, Head, Sector -- because it is older
;  than the idea of a flat sector number. The conversion, for a disk with
;  S sectors per track and H heads, is:
;
;       sector   = (LBA % S) + 1        <- 1-based! sector 0 does not exist
;       head     = (LBA / S) % H
;       cylinder = (LBA / S) / H
;
;  We compute it with two 16-bit divides. "div cx" divides DX:AX by CX, putting
;  the quotient in AX and the remainder in DX, so DX must be zeroed first --
;  forgetting that is the single most common bug in a boot sector.
; =============================================================================
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
    mov [dr_cylinder], al       ; AL: cylinders 0..255 only -- fine for a floppy
    mov [dr_head], dl

    mov di, 3                   ; three attempts. Floppy reads genuinely fail
                                ; at random; the BIOS expects you to retry.
.attempt:
    mov ah, 0x02                ; function: read sectors into memory
    mov al, [dr_count]          ; AL = number of sectors
    mov ch, [dr_cylinder]       ; CH = cylinder (low 8 bits)
    mov cl, [dr_sector]         ; CL = sector (bits 0-5), cylinder high bits 6-7
    mov dh, [dr_head]           ; DH = head
    mov dl, [boot_drive]        ; DL = drive
                                ; ES:BX is already the buffer -- untouched by
                                ; div, which only writes AX and DX.
    int 0x13
    jnc .verify                 ; CF clear = success

    ; Failed. Reset the disk controller and try again. AH=0x00 is "reset",
    ; and on a real floppy it re-seeks the head to track 0, which fixes the
    ; most common transient failure.
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    dec di
    jnz .attempt
    jmp disk_error

.verify:
    ; The BIOS returns the number of sectors *actually* read in AL. A short
    ; read is not signalled with CF, so a loader that only checks CF will
    ; happily jump into half-loaded code. AL still holds the returned count
    ; here -- nothing between the int 0x13 and this compare touches it.
    cmp al, [dr_count]
    jne disk_error
    popa
    ret

disk_error:
    mov si, msg_diskerr
    call print
    jmp hang

; =============================================================================
;  print  --  write a NUL-terminated string via BIOS teletype output
; =============================================================================
;   in:  DS:SI = string
;   int 0x10 AH=0x0E prints AL at the cursor and advances it, handling
;   scrolling, \r and \n for us. BH is the page number, BL the colour in
;   graphics modes; in text mode BL is ignored but should still be sane.
; =============================================================================
print:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
.next:
    lodsb                       ; AL = [DS:SI], SI++  (needs DF clear -- see cld)
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    popa
    ret

hang:
    cli
    hlt                         ; stop the CPU until an interrupt arrives...
    jmp hang                    ; ...and since IF is clear, that is never.

; =============================================================================
;  Data
; =============================================================================
boot_drive:  db 0
dr_count:    db 0
dr_sector:   db 0
dr_head:     db 0
dr_cylinder: db 0

msg_stage1:  db "Spark: stage1", 13, 10, 0
msg_jump:    db "Spark: -> stage2", 13, 10, 0
msg_diskerr: db "Spark: DISK ERROR", 13, 10, 0

; =============================================================================
;  The signature
; =============================================================================
;  $  = the current address
;  $$ = the address of the start of this section (0x7C00, because of ORG)
;  so ($ - $$) is "how many bytes have we emitted so far".
;
;  If this line errors with "times value is negative", the boot sector is too
;  big. There is no way around it: 512 bytes is the hardware limit.

times 510 - ($ - $$) db 0       ; pad with zeros up to offset 510
dw 0xAA55                       ; ...and the two magic bytes the BIOS checks.
                                ; Stored little-endian, so the bytes on disk
                                ; are 0x55 0xAA -- which is why you see the
                                ; number written both ways in the wild.
