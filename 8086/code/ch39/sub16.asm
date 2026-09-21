; sub16.asm — subtract two 16-bit numbers, printing a signed result
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        sub  ax, [num2]         ; CF = borrow, OF = signed overflow

        print msg
        call print_sdec         ; handles the minus sign
        newline
        exit 0

num1:   dw   1500
num2:   dw   4200
msg:    db   '1500 - 4200 = $'
