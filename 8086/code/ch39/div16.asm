; div16.asm — divide, with a guard against both failure modes
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  bx, [divisor]
        or   bx, bx
        jz   .div_by_zero       ; guard 1: divisor must not be zero

        mov  ax, [dividend]
        xor  dx, dx             ; zero-extend to 32 bits: DX:AX = dividend
                                ; guard 2: with DX = 0 and a 16-bit divisor,
                                ;   the quotient always fits, so no INT 0.
        div  bx                 ; AX = quotient, DX = remainder

        mov  [quot], ax
        mov  [rem], dx

        print qmsg
        mov  ax, [quot]
        call print_udec
        newline

        print rmsg
        mov  ax, [rem]
        call print_udec
        newline
        exit 0

.div_by_zero:
        print zmsg
        exit 1

dividend: dw   50000
divisor:  dw   7
quot:     dw   0
rem:      dw   0
qmsg:     db   'Quotient:  $'
rmsg:     db   'Remainder: $'
zmsg:     db   'Division by zero!', 0x0D, 0x0A, '$'
