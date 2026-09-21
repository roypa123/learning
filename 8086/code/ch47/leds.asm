; leds.asm — read eight switches on Port B, show them on eight LEDs on Port A
; nasm -f bin leds.asm -o leds.com
;
; Hardware: 8255 at ports 0x00-0x06.
;   PA7-PA0 -> LEDs through 330-ohm resistors to ground (1 = lit)
;   PB7-PB0 <- switches to ground, with 10k pull-ups (closed = 0)
        cpu  8086
        org  0x100
%include "macros.inc"

PORTA   equ  0x00
PORTB   equ  0x02
PORTC   equ  0x04
CTRL    equ  0x06

start:
        ; --- configure: A output, B input, C output, all mode 0 ---
        mov  al, 0x82
        out  CTRL, al

.loop:
        in   al, PORTB          ; read the switches
        not  al                 ; switches pull LOW when closed, so invert
        out  PORTA, al          ; drive the LEDs

        ; --- quit if a key has been pressed ---
        mov  ah, 0x0B           ; DOS: check the keyboard without waiting
        int  0x21
        or   al, al
        jz   .loop

        ; --- turn the LEDs off before leaving ---
        xor  al, al
        out  PORTA, al
        exit 0
