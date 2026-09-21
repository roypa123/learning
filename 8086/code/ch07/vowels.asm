; vowels.asm — count vowels in a string, print the total
; nasm -f bin vowels.asm -o vowels.com
;
; register allocation
;   SI -> current position in the text        (DS:SI, advanced by LODSB)
;   AL -> the character just read
;   BX -> current position in the vowel table
;   DL -> the table character being compared
;   CX -> the running count
;   AX -> scratch at the end, for the division
        org  0x100

start:
        mov  si, text           ; SI = source pointer, relative to DS
        xor  cx, cx             ; CX = 0. XOR is 2 bytes; MOV CX,0 is 3.

.next:
        lodsb                   ; AL <- [DS:SI]; SI <- SI+1
        or   al, al             ; sets ZF if AL is zero — the terminator
        jz   .done
        or   al, 0x20           ; force lower case: bit 5 is the case bit

        mov  bx, vowels         ; BX = start of the vowel table
.scan:
        mov  dl, [bx]           ; DL = this table entry (DS:BX)
        or   dl, dl             ; end of table?
        jz   .next              ;   yes — this character was not a vowel
        cmp  al, dl
        je   .hit
        inc  bx
        jmp  .scan
.hit:
        inc  cx                 ; one more vowel
        jmp  .next

.done:
        ; CX holds the count (< 100). Split it into two decimal digits.
        mov  ax, cx             ; DIV works on AX, not CX
        mov  bl, 10
        div  bl                 ; AL <- AX/10 (tens), AH <- AX mod 10 (units)
        add  ax, 0x3030         ; +0x30 to each half: binary -> ASCII digit
        mov  [result], al       ; tens digit
        mov  [result+1], ah     ; units digit

        mov  dx, result
        mov  ah, 0x09
        int  0x21               ; print it

        mov  ax, 0x4C00
        int  0x21               ; exit

text:   db   'The quick brown fox jumps over the lazy dog', 0
vowels: db   'aeiou', 0
result: db   '00', 0x0D, 0x0A, '$'
