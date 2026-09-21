; leds.asm — mirror eight switches onto eight LEDs, forever.
; 8255 at ports 0x00 (A), 0x02 (B), 0x04 (C), 0x06 (control).
        org  0x100

PORTA   equ  0x00
PORTB   equ  0x02
CTRL    equ  0x06

start:
        ; Control word 0x82:
        ;   bit 7 = 1   mode-set flag
        ;   bits 6,5 = 00  port A mode 0
        ;   bit 4 = 0   port A output
        ;   bit 3 = 0   port C upper output
        ;   bit 2 = 0   port B mode 0
        ;   bit 1 = 1   port B INPUT
        ;   bit 0 = 0   port C lower output
        mov  al, 0x82
        out  CTRL, al

.loop:
        in   al, PORTB          ; read the switches
        not  al                 ; switches pull low when closed
        out  PORTA, al          ; drive the LEDs

        mov  ah, 0x0B           ; DOS: check for a keypress
        int  0x21
        or   al, al
        jz   .loop              ; no key -> keep going

        mov  ax, 0x4C00
        int  0x21
