; aaademo.asm — add two ASCII digits and print the two-digit result
; nasm -f bin aaademo.asm -o aaademo.com
        org  0x100

start:
        mov  ah, 0              ; AAA increments AH, so start it at zero
        mov  al, [d1]           ; '8' = 0x38
        add  al, [d2]           ; '7' = 0x37 -> AL = 0x6F
        aaa                     ; (0x6F AND 0x0F) = 0xF > 9
                                ;   AL = 0x6F + 6 = 0x75
                                ;   AH = 1
                                ;   AL = 0x75 AND 0x0F = 5
                                ; AX = 0x0105, CF = 1
        add  ax, 0x3030         ; AX = 0x3135 = '1','5'

        mov  [out_hi], ah
        mov  [out_lo], al

        mov  dx, outmsg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

d1:     db   '8'
d2:     db   '7'
outmsg: db   '8 + 7 = '
out_hi: db   '0'
out_lo: db   '0'
        db   0x0D, 0x0A, '$'
