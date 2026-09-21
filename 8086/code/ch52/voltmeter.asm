; voltmeter.asm — display ADC channel 0 as a voltage
; nasm -f bin voltmeter.asm -o voltmeter.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

PORTA   equ  0x00
PORTB   equ  0x02
PORTC   equ  0x04
CTRL    equ  0x06

start:
        ; --- 8255: A input, B output, C input ---
        ;   bit7=1, A mode 0, A input(1), C upper input(1),
        ;   B mode 0, B output(0), C lower input(1)
        mov  al, 1001_1001b     ; 0x99
        out  CTRL, al

        cls  0x07
        gotoxy 0, 0
        print banner

.loop:
        xor  al, al             ; channel 0
        call adc_read
        jc   .adc_error

        push ax
        gotoxy 3, 0
        print rawmsg
        pop  ax
        push ax
        xor  ah, ah
        call print_udec
        print spaces

        pop  ax
        xor  ah, ah
        call reading_to_mv      ; AX = millivolts

        gotoxy 4, 0
        print voltmsg
        call print_volts
        print spaces

        call adc_delay

        mov  ah, 0x0B
        int  0x21
        or   al, al
        jz   .loop
        exit 0

.adc_error:
        gotoxy 6, 0
        print errmsg
        exit 1

; ---------------------------------------------------------------
; print_volts — print AX millivolts as "n.nnn V"
; ---------------------------------------------------------------
print_volts:
        push ax
        push bx
        push cx
        push dx

        mov  bx, 1000
        xor  dx, dx
        div  bx                 ; AX = volts, DX = millivolts
        push dx
        call print_udec         ; the whole volts
        putc '.'
        pop  ax

        ; --- three digits, zero padded ---
        mov  bx, 100
        xor  dx, dx
        div  bx
        add  al, '0'
        putc al
        mov  ax, dx
        mov  bx, 10
        xor  dx, dx
        div  bx
        add  al, '0'
        putc al
        mov  al, dl
        add  al, '0'
        putc al
        print vmsg

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; reading_to_mv — AL (0-255) to millivolts in AX, Vref = 5 V
;   mV = reading × 5000 / 256 = reading × 625 / 32
; ---------------------------------------------------------------
reading_to_mv:
        push bx
        push cx
        push dx
        xor  ah, ah
        mov  bx, 625
        mul  bx                 ; DX:AX = reading × 625 (max 159,375)
        mov  cl, 5
        shr  ax, cl
        ; fold in the bits that came from DX
        mov  bx, dx
        mov  cl, 11
        shl  bx, cl
        or   ax, bx
        pop  dx
        pop  cx
        pop  bx
        ret

; ---------------------------------------------------------------
adc_delay:
        push cx
        mov  cx, 20000
.wait:  loop .wait
        pop  cx
        ret

; ... adc_read and short_delay from §4.4 ...

banner:  db  'ADC0808 voltmeter — channel 0. Press a key to quit.', 0x0D, 0x0A, '$'
rawmsg:  db  'Raw reading: $'
voltmsg: db  'Voltage:     $'
vmsg:    db  ' V$'
spaces:  db  '      $'
errmsg:  db  'ADC not responding — check the hardware.', 0x0D, 0x0A, '$'
