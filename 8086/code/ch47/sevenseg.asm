; sevenseg.asm — drive four multiplexed seven-segment digits
;
; Hardware:
;   Port A = the segment pattern (a-g and the decimal point)
;   Port B bits 3-0 = the digit enables (common cathode, active low)
;
;   Multiplexing: light one digit at a time, cycling fast enough
;   that persistence of vision makes all four look continuous.
        cpu  8086
        org  0x100
%include "macros.inc"

PORTA   equ  0x00
PORTB   equ  0x02
CTRL    equ  0x06

start:
        mov  al, 0x80           ; all ports output
        out  CTRL, al

        mov  word [value], 1234

.refresh:
        call show_number

        mov  ah, 0x0B
        int  0x21
        or   al, al
        jz   .refresh

        xor  al, al
        out  PORTA, al
        mov  al, 0x0F
        out  PORTB, al          ; all digits off
        exit 0

; ---------------------------------------------------------------
; show_number — display [value] on four digits, once.
;
;   Splits the value into four decimal digits, then lights each in
;   turn for about 2 ms. Call it repeatedly to keep the display lit.
; ---------------------------------------------------------------
show_number:
        push ax
        push bx
        push cx
        push dx

        ; --- split into digits, least significant first ---
        mov  ax, [value]
        mov  bx, 10
        mov  di, digits
        mov  cx, 4
.split:
        xor  dx, dx
        div  bx                 ; DX = this digit, AX = the rest
        mov  [di], dl
        inc  di
        loop .split

        ; --- light each digit in turn ---
        mov  cx, 4
        mov  si, digits
        mov  bl, 1110b          ; digit 0 enable, active low
.digit:
        ; blank first, to avoid ghosting between digits
        mov  al, 0
        out  PORTA, al
        mov  al, bl
        out  PORTB, al          ; select this digit

        mov  al, [si]           ; the digit value 0-9
        push bx
        mov  bx, segtab
        xlat                    ; AL = the segment pattern
        pop  bx
        out  PORTA, al          ; light it

        call digit_delay

        inc  si
        rol  bl, 1              ; move the low bit to the next digit
        loop .digit

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

digit_delay:
        push cx
        mov  cx, 2000           ; roughly 2 ms at 5 MHz
.wait:  loop .wait
        pop  cx
        ret

; ---------------------------------------------------------------
; Segment patterns for a COMMON-CATHODE display.
;   bit 0 = a, 1 = b, 2 = c, 3 = d, 4 = e, 5 = f, 6 = g, 7 = dp
;   A 1 lights the segment.
;
;        a
;      ─────
;    f│     │b
;     │  g  │
;      ─────
;    e│     │c
;     │     │
;      ─────   • dp
;        d
; ---------------------------------------------------------------
segtab: db   0x3F    ; 0 = a b c d e f
        db   0x06    ; 1 = b c
        db   0x5B    ; 2 = a b d e g
        db   0x4F    ; 3 = a b c d g
        db   0x66    ; 4 = b c f g
        db   0x6D    ; 5 = a c d f g
        db   0x7D    ; 6 = a c d e f g
        db   0x07    ; 7 = a b c
        db   0x7F    ; 8 = all
        db   0x6F    ; 9 = a b c d f g

value:  dw   0
digits: times 4 db 0
