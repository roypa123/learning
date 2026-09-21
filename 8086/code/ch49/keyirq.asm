; keyirq.asm — count keystrokes via an IRQ1 handler
; nasm -f bin keyirq.asm -o keyirq.com
;
; Chains to the BIOS INT 09h handler so normal keyboard processing
; continues. The BIOS handler sends the EOI, so ours must not.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- save the old INT 09h vector ---
        mov  ax, 0x3509
        int  0x21               ; ES:BX = the old handler
        mov  [old_off], bx
        mov  [old_seg], es

        ; --- install ours ---
        push ds
        mov  dx, key_handler
        push cs
        pop  ds
        mov  ax, 0x2509
        int  0x21
        pop  ds

        print msg

        ; --- wait for 20 keystrokes ---
.wait:
        mov  ax, [count]
        cmp  ax, 20
        jb   .wait

        ; --- restore the old vector BEFORE anything else ---
        push ds
        mov  dx, [old_off]
        mov  ax, [old_seg]
        mov  ds, ax
        mov  ax, 0x2509
        int  0x21
        pop  ds

        print donemsg
        mov  ax, [count]
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; key_handler — called on every keyboard interrupt (IRQ1).
;
;   Increments a counter, then CHAINS to the original BIOS handler,
;   which reads the scan code, buffers it and sends the EOI.
;
;   Because we chain, we must NOT send an EOI ourselves.
; ---------------------------------------------------------------
key_handler:
        push ax
        push ds
        push cs
        pop  ds

        inc  word [count]

        pop  ds
        pop  ax

        ; --- chain: simulate an INT to the old handler ---
        pushf                   ; the old handler's IRET expects flags here
        call far [cs:old_vec]   ; ... then CS and IP, which CALL FAR pushes
        iret

; The old vector, as a far pointer for CALL FAR to use.
old_vec:
old_off:  dw  0
old_seg:  dw  0

count:    dw  0
msg:      db  'Press 20 keys...', 0x0D, 0x0A, '$'
donemsg:  db  'Keystrokes counted: $'
