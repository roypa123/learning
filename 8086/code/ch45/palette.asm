; palette.asm — display all 256 colours
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax

        ; --- a 16 × 16 grid of 18 × 11 pixel blocks ---
        cld
        xor  bl, bl             ; BL = the colour
        xor  bp, bp             ; BP = the grid row
.gridrow:
        mov  si, 0              ; SI = the grid column
.gridcol:
        ; top-left of this block
        mov  ax, bp
        mov  dx, 11
        mul  dx                 ; y = row × 11
        mov  dx, 320
        mul  dx                 ; AX = y × 320  (DX:AX, but it fits)
        mov  di, ax
        mov  ax, si
        mov  dx, 18
        mul  dx
        add  di, ax             ; + column × 18

        ; draw an 18 × 11 block
        mov  al, bl
        mov  cx, 11
.blockrow:
        push cx
        push di
        mov  cx, 18
        rep  stosb
        pop  di
        add  di, 320
        pop  cx
        loop .blockrow

        inc  bl
        inc  si
        cmp  si, 16
        jb   .gridcol
        inc  bp
        cmp  bp, 16
        jb   .gridrow

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0
