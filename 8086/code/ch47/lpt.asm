; lpt.asm — write patterns to the LPT1 data register
;
; LPT1 at 0x378:
;   0x378  data register     (8 output bits — like Port A)
;   0x379  status register   (5 input bits — like Port B)
;   0x37A  control register  (4 bidirectional bits)
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

LPT_DATA    equ  0x378
LPT_STATUS  equ  0x379
LPT_CTRL    equ  0x37A

start:
        ; --- a running light on the data pins ---
        mov  cx, 8
        mov  al, 1
.next:
        push ax
        mov  dx, LPT_DATA
        out  dx, al             ; the 16-bit port needs DX

        print pmsg
        pop  ax
        push ax
        xor  ah, ah
        call print_hex16
        newline

        call delay
        pop  ax
        shl  al, 1
        loop .next

        ; --- read the status register ---
        print smsg
        mov  dx, LPT_STATUS
        in   al, dx
        xor  ah, ah
        call print_hex16
        newline

        xor  al, al
        mov  dx, LPT_DATA
        out  dx, al
        exit 0

delay:
        push cx
        push dx
        mov  dx, 4
.outer: xor  cx, cx
.inner: loop .inner
        dec  dx
        jnz  .outer
        pop  dx
        pop  cx
        ret

pmsg:   db   'Data pins: 0x$'
smsg:   db   'Status:    0x$'
