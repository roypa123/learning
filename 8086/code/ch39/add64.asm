; add64.asm — add two 64-bit numbers held as four words each
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, num1
        mov  di, num2
        mov  bx, result
        mov  cx, 4              ; four words = 64 bits
        clc                     ; no carry into the first word

.next:
        mov  ax, [si]
        adc  ax, [di]           ; add with the carry from the previous word
        mov  [bx], ax           ; MOV preserves CF

        inc  si                 ; INC preserves CF — `add si, 2` would NOT
        inc  si
        inc  di
        inc  di
        inc  bx
        inc  bx
        loop .next              ; LOOP preserves CF

        ; --- print the result, most significant word first ---
        print msg
        mov  ax, [result+6]
        call print_hex16
        mov  ax, [result+4]
        call print_hex16
        mov  ax, [result+2]
        call print_hex16
        mov  ax, [result]
        call print_hex16
        newline
        exit 0

; stored least significant word first
num1:   dw   0xFFFF, 0xFFFF, 0xFFFF, 0x0000
num2:   dw   0x0001, 0x0000, 0x0000, 0x0000
result: times 4 dw 0
msg:    db   '0000FFFFFFFFFFFF + 1 = 0x$'
