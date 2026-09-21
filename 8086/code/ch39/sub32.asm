; sub32.asm — subtract two 32-bit numbers
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]
        sub  ax, [num2]         ; CF = borrow out
        mov  [result], ax
        mov  ax, [num1+2]
        sbb  ax, [num2+2]       ; − the borrow
        mov  [result+2], ax

        print msg
        mov  dx, [result+2]
        mov  ax, [result]
        call print_hex32
        newline
        exit 0

num1:   dd   0x00040000
num2:   dd   0x00000001
result: dd   0
msg:    db   '00040000 - 00000001 = 0x$'
