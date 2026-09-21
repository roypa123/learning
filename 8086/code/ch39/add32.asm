; add32.asm — add two 32-bit numbers
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [num1]         ; low word of num1
        add  ax, [num2]         ; + low word of num2; CF = carry out
        mov  [result], ax       ; MOV does not disturb CF

        mov  ax, [num1+2]       ; high word
        adc  ax, [num2+2]       ; + high word + the carry
        mov  [result+2], ax

        print msg
        mov  dx, [result+2]
        mov  ax, [result]
        call print_hex32
        newline
        exit 0

num1:   dd   0x0001FFFF
num2:   dd   0x00020001
result: dd   0
msg:    db   '0001FFFF + 00020001 = 0x$'
