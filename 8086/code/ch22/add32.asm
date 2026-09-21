; add32.asm — add two 32-bit numbers and print the result in hex
; nasm -f bin add32.asm -o add32.com
;
;   DX:AX holds the running 32-bit value in several places
        org  0x100

start:
        ; --- the addition ---
        mov  ax, [num1]         ; low word of num1
        add  ax, [num2]         ; + low word of num2, CF = carry
        mov  [result], ax       ; MOV does not touch CF
        mov  ax, [num1+2]       ; high word
        adc  ax, [num2+2]       ; + high word + carry
        mov  [result+2], ax

        ; --- print it as 8 hex digits, high word first ---
        mov  ax, [result+2]
        call print_hex16
        mov  ax, [result]
        call print_hex16

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; print_hex16 — print AX as four hex digits.
;   Destroys nothing the caller cares about (AX, BX, CX, DX saved).
; ---------------------------------------------------------------
print_hex16:
        push ax
        push bx
        push cx
        push dx

        mov  cx, 4              ; four digits
        mov  bx, ax             ; keep the value in BX; AX is needed for DOS
.digit:
        rol  bx, 1              ; rotate the top nibble down to the bottom
        rol  bx, 1              ; (8086 has no ROL bx,4)
        rol  bx, 1
        rol  bx, 1
        mov  dl, bl
        and  dl, 0x0F           ; isolate the nibble
        add  dl, '0'
        cmp  dl, '9'
        jbe  .emit
        add  dl, 7              ; 'A'..'F' are 7 past '9'+1
.emit:
        mov  ah, 0x02           ; DOS: print the character in DL
        int  0x21
        loop .digit

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
num1:   dd   0x0001FFFF
num2:   dd   0x00020001
result: dd   0
crlf:   db   0x0D, 0x0A, '$'
