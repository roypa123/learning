; tracer.asm — single-step a section of code, printing IP at each step
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- install our INT 1 handler ---
        mov  ah, 0x35
        mov  al, 0x01
        int  0x21               ; ES:BX = the old handler
        mov  [old_off], bx
        mov  [old_seg], es

        push ds
        mov  dx, step_handler
        push cs
        pop  ds
        mov  ax, 0x2501
        int  0x21
        pop  ds

        ; --- turn on single-stepping ---
        pushf
        pop  ax
        or   ax, 0x0100         ; set TF (bit 8)
        push ax
        popf                    ; the NEXT instruction will trap

        ; --- the code being traced ---
        mov  ax, 1
        mov  bx, 2
        add  ax, bx
        mov  cx, ax
        nop

        ; --- turn it off ---
        pushf
        pop  ax
        and  ax, 0xFEFF         ; clear TF
        push ax
        popf

        ; --- restore the old handler ---
        push ds
        mov  dx, [old_off]
        mov  ax, [old_seg]
        mov  ds, ax
        mov  ax, 0x2501
        int  0x21
        pop  ds

        print donemsg
        exit 0

; ---------------------------------------------------------------
; step_handler — called after every instruction while TF = 1.
;
;   The stack on entry:   [SP+0] = IP,  [SP+2] = CS,  [SP+4] = FLAGS
;   of the interrupted instruction's SUCCESSOR.
; ---------------------------------------------------------------
step_handler:
        push bp
        mov  bp, sp
        push ax
        push bx
        push cx
        push dx
        push ds

        push cs
        pop  ds                 ; our data segment

        mov  ax, [bp+2]         ; the saved IP  (BP+0 is the saved BP)
        call print_hex16
        putc ' '

        pop  ds
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        pop  bp
        iret

old_off:  dw  0
old_seg:  dw  0
donemsg:  db  0x0D, 0x0A, 'Trace complete.', 0x0D, 0x0A, '$'
