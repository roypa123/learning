; showbin.asm — print a 16-bit value in binary
; nasm -f bin showbin.asm -o showbin.com
;
;   BX -> the value being displayed
;   CX -> bit counter
;   DL -> the character to print
        org  0x100

start:
        mov  dx, prefix
        mov  ah, 0x09
        int  0x21

        mov  bx, [value]
        mov  cx, 16

.next:
        rol  bx, 1              ; the top bit moves into CF — and back into bit 0,
                                ;   so BX is unchanged after all 16 rotations
        mov  dl, '0'
        jnc  .emit
        mov  dl, '1'
.emit:
        mov  ah, 0x02
        int  0x21               ; DOS: print the character in DL

        ; insert a space every four bits for readability
        mov  ax, cx
        dec  ax
        and  ax, 3              ; zero on iterations 13, 9, 5, 1
        jnz  .skip
        cmp  cx, 1              ; but not after the very last bit
        je   .skip
        mov  dl, ' '
        mov  ah, 0x02
        int  0x21
.skip:
        loop .next

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

value:  dw   0xB6D3
prefix: db   'Binary: $'
crlf:   db   0x0D, 0x0A, '$'
