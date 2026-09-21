; idiv16.asm — signed division, showing why CWD matters
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [dividend]
        cwd                     ; SIGN-extend AX into DX:AX.
                                ;   xor dx,dx would be WRONG for a negative value.
        mov  bx, [divisor]
        idiv bx                 ; AX = quotient, DX = remainder

        push dx
        print qmsg
        call print_sdec
        newline
        pop  ax
        print rmsg
        call print_sdec
        newline
        exit 0

dividend: dw   -100
divisor:  dw   7
qmsg:     db   'Quotient:  $'
rmsg:     db   'Remainder: $'
