; bcdadd.asm — add two 6-digit packed BCD numbers
; nasm -f bin bcdadd.asm -o bcdadd.com
;
; Numbers are stored LEAST significant byte first, so we can walk
; upwards with the carry propagating naturally.
;
;   SI -> current byte of num1
;   DI -> current byte of num2
;   BX -> current byte of result
;   CX -> byte count
        org  0x100

start:
        mov  si, num1
        mov  di, num2
        mov  bx, result
        mov  cx, 3              ; three bytes = six digits
        clc                     ; no carry into the first byte

.next:
        mov  al, [si]           ; a digit pair from num1
        adc  al, [di]           ; + the pair from num2 + the carry from last time
        daa                     ; make it decimal again; CF = the decimal carry
        mov  [bx], al           ; MOV does not disturb CF

        inc  si                 ; INC does not disturb CF either
        inc  di
        inc  bx
        loop .next              ; LOOP does not disturb CF

        ; --- print the six digits, most significant byte first ---
        mov  si, result + 2
        mov  cx, 3
.show:
        mov  al, [si]
        call print_bcd_byte
        dec  si
        loop .show

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; print_bcd_byte — print the two BCD digits in AL.
; ---------------------------------------------------------------
print_bcd_byte:
        push ax
        push cx
        push dx

        mov  dl, al
        mov  cl, 4
        shr  dl, cl             ; the high nibble
        add  dl, '0'
        mov  ah, 0x02
        int  0x21

        pop  dx
        push dx
        mov  dl, al
        and  dl, 0x0F           ; the low nibble
        add  dl, '0'
        mov  ah, 0x02
        int  0x21

        pop  dx
        pop  cx
        pop  ax
        ret

; ---------------------------------------------------------------
; 123456 + 876544 = 999 - wait: 123456 + 876544 = 1,000,000.
; We store only six digits, so the result is 000000 with a final
; carry - which is exactly what the program will show.
num1:   db   0x56, 0x34, 0x12   ; 123456, least significant byte first
num2:   db   0x44, 0x65, 0x87   ; 876544
result: db   0, 0, 0
crlf:   db   0x0D, 0x0A, '$'
