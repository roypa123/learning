; popcount.asm — count the 1 bits in a 16-bit value and print the count
; nasm -f bin popcount.asm -o popcount.com
;
;   BX -> the value being examined
;   CX -> loop counter (16 bits to check)
;   DL -> the running count
        org  0x100

start:
        mov  bx, [value]
        mov  cx, 16
        xor  dl, dl             ; DL = 0, the count

.next:
        test bx, 1              ; is the bottom bit set?
        jz   .skip              ;   no
        inc  dl                 ;   yes — count it
.skip:
        shr  bx, 1              ; move the next bit down
        loop .next

        ; --- print DL as two decimal digits ---
        mov  al, dl
        xor  ah, ah
        mov  bl, 10
        div  bl                 ; AL = tens, AH = units
        add  ax, 0x3030
        mov  [out_hi], al
        mov  [out_lo], ah

        mov  dx, msg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

value:  dw   0xB6D3             ; 1011 0110 1101 0011
msg:    db   'Bits set: '
out_hi: db   '0'
out_lo: db   '0'
        db   0x0D, 0x0A, '$'
