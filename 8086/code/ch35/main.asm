; main.asm
        cpu  8086
        org  0x100
start:
        call print_string
        mov  ax, 0x4C00
        int  0x21

%include "strings.inc"
%include "math.inc"

msg:    db   'Hello$'
