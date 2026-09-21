; reverse.asm — reverse a string in place, then print it
; nasm -f bin reverse.asm -o reverse.com
;
;   SI -> left-hand character
;   DI -> right-hand character
;   AL, AH -> the two characters being swapped
        org  0x100

start:
        ; find the end of the string
        mov  si, text
        mov  di, si
.findend:
        cmp  byte [di], '$'
        je   .found
        inc  di
        jmp  .findend
.found:
        dec  di                 ; DI now points at the last real character

.swap:
        cmp  si, di             ; pointers met or crossed?
        jae  .done
        mov  al, [si]
        mov  ah, [di]
        mov  [si], ah
        mov  [di], al
        inc  si
        dec  di
        jmp  .swap

.done:
        mov  dx, text
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

text:   db   'Hello, 8086!', '$'
