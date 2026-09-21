; add8c.asm — add two bytes, correctly handling a result over 255
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  al, [num1]
        add  al, [num2]         ; CF = 1 if the true sum exceeds 255
        mov  ah, 0              ; assume it fits...
        adc  ah, 0              ; ...then add the carry into AH.
                                ;   AH becomes 1 exactly when CF was 1.
                                ;   AX is now the true 16-bit sum.
        print msg
        call print_udec
        newline
        exit 0

num1:   db   200
num2:   db   100
msg:    db   '200 + 100 = $'
