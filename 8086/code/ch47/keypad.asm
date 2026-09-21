; keypad.asm — scan a 4x4 matrix keypad
;
; Hardware: PA3-PA0 drive the rows (active low, open-drain or via
;           resistors), PB3-PB0 read the columns with pull-ups.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

PORTA   equ  0x00
PORTB   equ  0x02
CTRL    equ  0x06

start:
        mov  al, 0x82           ; A output, B input
        out  CTRL, al

.loop:
        call scan_keypad
        jc   .no_key            ; CF = 1 means nothing pressed

        ; AL = 0-15; print it as a hex digit
        mov  bx, hextab
        xlat
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        call debounce

.no_key:
        mov  ah, 0x0B
        int  0x21
        or   al, al
        jz   .loop
        exit 0

; ---------------------------------------------------------------
; scan_keypad — scan the 4x4 matrix.
;
;   Out:       CF = 0 and AL = the key code 0-15, or CF = 1
;   Destroys:  AH, BX, CX
;
;   Method: drive one row low at a time, read the columns. A pressed
;   key connects its row to its column, pulling that column low.
; ---------------------------------------------------------------
scan_keypad:
        mov  cl, 0              ; CL = the row number
        mov  ch, 0xFE           ; CH = the row pattern: one bit low
.row:
        mov  al, ch
        out  PORTA, al          ; drive this row low
        call short_delay        ; let the lines settle

        in   al, PORTB
        and  al, 0x0F           ; only the four column bits
        cmp  al, 0x0F
        jne  .pressed           ; some column went low -> a key in this row

        rol  ch, 1              ; move the low bit to the next row
        inc  cl
        cmp  cl, 4
        jb   .row

        stc                     ; nothing pressed
        ret

.pressed:
        ; --- work out which column ---
        mov  ah, 0              ; AH = the column number
.findcol:
        test al, 1
        jz   .gotcol            ; this bit is low -> this column
        shr  al, 1
        inc  ah
        cmp  ah, 4
        jb   .findcol
        stc
        ret

.gotcol:
        ; key code = row × 4 + column
        mov  al, cl
        shl  al, 1
        shl  al, 1              ; × 4
        add  al, ah
        clc
        ret

short_delay:
        push cx
        mov  cx, 100
.wait:  loop .wait
        pop  cx
        ret

; ---------------------------------------------------------------
; debounce — wait until no key is pressed, then pause.
;   Without this, one press registers dozens of times.
; ---------------------------------------------------------------
debounce:
        push ax
.wait_release:
        call scan_keypad
        jnc  .wait_release      ; still pressed
        call short_delay
        call short_delay
        pop  ax
        ret

hextab: db   '0123456789ABCDEF'
