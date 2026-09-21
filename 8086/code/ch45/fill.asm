; fill.asm — fill the screen with a colour, three ways
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax

        ; --- method 1: REP STOSB, one byte at a time ---
        cld
        xor  di, di
        mov  al, 1              ; blue
        mov  cx, 64000
        rep  stosb              ; 9 + 64000 × 10 = 640,009 clocks
        call pause

        ; --- method 2: REP STOSW, two pixels at a time ---
        xor  di, di
        mov  ax, 0x0404         ; red in both bytes
        mov  cx, 32000
        rep  stosw              ; 9 + 32000 × 10 = 320,009 clocks — HALF
        call pause

        ; --- method 3: horizontal bands ---
        xor  di, di
        xor  bl, bl             ; BL = the colour
        mov  dx, 200            ; 200 rows
.row:
        mov  al, bl
        mov  ah, bl
        mov  cx, 160            ; 320 bytes = 160 words
        rep  stosw
        inc  bl                 ; a different colour each row
        dec  dx
        jnz  .row
        call pause

        mov  ax, 0x0003
        int  0x10
        exit 0

pause:
        push ax
        xor  ah, ah
        int  0x16
        pop  ax
        ret
