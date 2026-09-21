; serecho.asm — a simple serial terminal
; nasm -f bin serecho.asm -o serecho.com
;
; Characters typed on the keyboard go out of the serial port;
; characters arriving on the serial port appear on the screen.
; Esc quits.
;
; Hardware: 8251A at ports 0x00 (data) and 0x02 (control),
;           8253 counter 1 supplying TxC/RxC at baud × 16.
        cpu  8086
        org  0x100
%include "macros.inc"

DATA    equ  0x00
CTRL    equ  0x02
T_CTRL  equ  0x43
T_CNT1  equ  0x41

start:
        call init_baud
        call init_8251

        print banner

.loop:
        ; --- anything arrived on the serial line? ---
        in   al, CTRL
        test al, 0x02           ; RxRDY
        jz   .check_errors
        in   al, DATA
        mov  dl, al
        mov  ah, 0x02
        int  0x21               ; show it

.check_errors:
        in   al, CTRL
        test al, 0x38           ; PE | OE | FE
        jz   .check_key
        call report_error

.check_key:
        ; --- anything typed? ---
        mov  ah, 0x0B           ; DOS: check the keyboard, no wait
        int  0x21
        or   al, al
        jz   .loop

        mov  ah, 0x08           ; read it, no echo
        int  0x21
        cmp  al, 27             ; Esc?
        je   .done

        call putc_serial
        jmp  .loop

.done:
        print byemsg
        exit 0

; ---------------------------------------------------------------
; init_baud — 8253 counter 1, mode 3, for 9600 baud × 16
;             from a 1.8432 MHz input.
; ---------------------------------------------------------------
init_baud:
        mov  al, 0x76           ; counter 1, LSB/MSB, mode 3, binary
        out  T_CTRL, al
        mov  ax, 12             ; 1,843,200 / 153,600
        out  T_CNT1, al
        mov  al, ah
        out  T_CNT1, al
        ret

; ---------------------------------------------------------------
init_8251:
        xor  al, al
        out  CTRL, al
        call io_delay
        out  CTRL, al
        call io_delay
        out  CTRL, al
        call io_delay
        mov  al, 0x40           ; internal reset
        out  CTRL, al
        call io_delay
        mov  al, 0x4E           ; mode: 8-N-1, x16
        out  CTRL, al
        call io_delay
        mov  al, 0x37           ; command: RTS, ER, RxE, DTR, TxEN
        out  CTRL, al
        ret

; ---------------------------------------------------------------
putc_serial:
        push ax
        mov  ah, al
.wait:
        in   al, CTRL
        test al, 0x01           ; TxRDY
        jz   .wait
        mov  al, ah
        out  DATA, al
        pop  ax
        ret

; ---------------------------------------------------------------
; report_error — print which error, then clear it.
; ---------------------------------------------------------------
report_error:
        push ax
        push dx
        mov  ah, al             ; keep the status

        test ah, 0x08
        jz   .not_pe
        print pemsg
.not_pe:
        test ah, 0x10
        jz   .not_oe
        print oemsg
.not_oe:
        test ah, 0x20
        jz   .not_fe
        print femsg
.not_fe:
        mov  al, 0x37           ; a command word with ER set clears them
        out  CTRL, al
        pop  dx
        pop  ax
        ret

io_delay:
        jmp  short $+2
        jmp  short $+2
        ret

banner:  db  '8251A terminal — 9600 8-N-1. Esc to quit.', 0x0D, 0x0A, '$'
byemsg:  db  0x0D, 0x0A, 'Closed.', 0x0D, 0x0A, '$'
pemsg:   db  '[parity]$'
oemsg:   db  '[overrun]$'
femsg:   db  '[framing]$'
