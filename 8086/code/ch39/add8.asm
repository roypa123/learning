; add8.asm — add two 8-bit numbers and print the result
; nasm -f bin add8.asm -o add8.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  al, [num1]         ; AL = the first number
        add  al, [num2]         ; AL = AL + the second; flags set

        mov  ah, 0              ; zero-extend AL into AX for printing
                                ;   (the sum of two bytes can exceed 255,
                                ;    but here AL has already wrapped)
        mov  [result], al

        print msg
        mov  al, [result]
        mov  ah, 0
        call print_udec
        newline
        exit 0

num1:   db   87
num2:   db   56
result: db   0
msg:    db   '87 + 56 = $'
