; menu.asm — read a digit and dispatch through a jump table
; nasm -f bin menu.asm -o menu.com
        org  0x100

start:
        mov  dx, prompt
        mov  ah, 0x09
        int  0x21

        mov  ah, 0x01           ; DOS: read a character, echo it
        int  0x21               ; AL = the character

        sub  al, '1'            ; '1'..'4' -> 0..3
        cmp  al, 3
        ja   .invalid           ; UNSIGNED: catches both < '1' and > '4',
                                ;   because '0'-'1' wraps to 0xFF

        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — table entries are words
        jmp  [table + bx]       ; dispatch

.invalid:
        mov  dx, badmsg
        jmp  .say

opt1:   mov  dx, msg1
        jmp  .say
opt2:   mov  dx, msg2
        jmp  .say
opt3:   mov  dx, msg3
        jmp  .say
opt4:   mov  dx, msg4

.say:
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C00
        int  0x21

table:  dw   opt1, opt2, opt3, opt4

prompt: db   0x0D, 0x0A, 'Choose 1-4: $'
msg1:   db   0x0D, 0x0A, 'You chose addition.', 0x0D, 0x0A, '$'
msg2:   db   0x0D, 0x0A, 'You chose subtraction.', 0x0D, 0x0A, '$'
msg3:   db   0x0D, 0x0A, 'You chose multiplication.', 0x0D, 0x0A, '$'
msg4:   db   0x0D, 0x0A, 'You chose division.', 0x0D, 0x0A, '$'
badmsg: db   0x0D, 0x0A, 'Not a valid choice.', 0x0D, 0x0A, '$'
