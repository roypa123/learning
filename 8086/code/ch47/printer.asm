; printer.asm — send a string to a printer using mode 1 handshaking
;
; Port A = data out, mode 1.
;   PC7 = OBF# -> the printer's STROBE# (inverted externally)
;   PC6 = ACK# <- the printer's ACK#
        cpu  8086
        org  0x100
%include "macros.inc"

PORTA   equ  0x00
PORTC   equ  0x04
CTRL    equ  0x06

start:
        ; --- Port A mode 1 output, Port B mode 0 output ---
        ;   bit7=1, bits6,5=01 (A mode 1), bit4=0 (A output),
        ;   bit3=0, bit2=0, bit1=0, bit0=0
        mov  al, 1010_0000b     ; 0xA0
        out  CTRL, al

        mov  si, message
.next:
        lodsb
        or   al, al
        jz   .done

        call send_byte
        jmp  .next

.done:
        exit 0

; ---------------------------------------------------------------
; send_byte — write AL to Port A and wait for the acknowledgement.
;
;   Polls OBF# (PC7). While it is LOW the previous byte has not been
;   acknowledged; when it goes HIGH the 8255 is ready for more.
; ---------------------------------------------------------------
send_byte:
        push ax
.wait:
        in   al, PORTC
        test al, 0x80           ; PC7 = OBF#
        jz   .wait              ; still low -> the printer has not taken it
        pop  ax
        out  PORTA, al          ; writing Port A drives OBF# low again
        ret

message: db 'Hello, printer!', 0x0D, 0x0A, 0
