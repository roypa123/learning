; mask.asm — demonstrate the interrupt mask register
; nasm -f bin mask.asm -o mask.com
;
; Reads and displays the PIC mask, then disables the keyboard for
; three seconds, then re-enables it. Type during the pause and
; nothing appears until it ends.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

PIC_CMD  equ  0x20
PIC_DATA equ  0x21

start:
        ; --- show the current mask ---
        print maskmsg
        in   al, PIC_DATA
        mov  [saved], al
        xor  ah, ah
        call print_hex16
        newline
        call explain_mask

        ; --- disable the keyboard (IRQ1) ---
        print offmsg
        in   al, PIC_DATA
        or   al, 0000_0010b     ; set bit 1 -> IRQ1 masked
        out  PIC_DATA, al

        mov  ax, 3
        call delay_seconds

        ; --- restore ---
        mov  al, [saved]
        out  PIC_DATA, al
        print onmsg

        ; --- show the IRR and ISR ---
        print irrmsg
        mov  al, 0x0A           ; OCW3: read IRR
        out  PIC_CMD, al
        in   al, PIC_CMD
        xor  ah, ah
        call print_hex16
        newline

        print isrmsg
        mov  al, 0x0B           ; OCW3: read ISR
        out  PIC_CMD, al
        in   al, PIC_CMD
        xor  ah, ah
        call print_hex16
        newline
        exit 0

; ---------------------------------------------------------------
; explain_mask — print which IRQs are enabled, from AL.
; ---------------------------------------------------------------
explain_mask:
        push ax
        push bx
        push cx
        mov  bl, [saved]
        xor  cx, cx
.next:
        mov  al, bl
        mov  ah, cl
        push cx
        mov  cl, ah
        shr  al, cl
        pop  cx
        test al, 1
        jnz  .masked
        print enmsg
        mov  ax, cx
        call print_udec
        newline
.masked:
        inc  cx
        cmp  cx, 8
        jb   .next
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
delay_seconds:
        push ax
        push bx
        push cx
        push es
        mov  bx, 1165
        mul  bx
        mov  cl, 6
        shr  ax, cl
        mov  cx, ax
        mov  ax, 0x0040
        mov  es, ax
        mov  bx, [es:0x6C]
.wait:
        mov  ax, [es:0x6C]
        sub  ax, bx
        cmp  ax, cx
        jb   .wait
        pop  es
        pop  cx
        pop  bx
        pop  ax
        ret

saved:    db  0
maskmsg:  db  'Current IMR: 0x$'
enmsg:    db  '  enabled: IRQ$'
offmsg:   db  'Keyboard disabled for three seconds — type now...', 0x0D, 0x0A, '$'
onmsg:    db  'Keyboard re-enabled.', 0x0D, 0x0A, '$'
irrmsg:   db  'IRR: 0x$'
isrmsg:   db  'ISR: 0x$'
