; panel.asm — an 8279 keypad and display
; nasm -f bin panel.asm -o panel.com
;
; Reads digits from the keypad and shows them on an eight-digit
; display, shifting left as each is entered.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

DATA    equ  0x08
CMD     equ  0x0A

start:
        call init_8279
        print banner

        ; --- clear the display buffer ---
        cld
        mov  di, digits
        mov  cx, 8
        mov  al, 0x00           ; blank
        rep  stosb
        call refresh

.loop:
        call kbd_read
        jc   .check_quit

        ; --- convert the key code to a character ---
        and  al, 0x3F           ; strip the shift and control bits
        cmp  al, 16
        jae  .loop              ; out of range for our table
        mov  bx, keytab
        xlat                    ; AL = the character

        cmp  al, 'C'
        je   .clear
        cmp  al, '0'
        jb   .loop
        cmp  al, '9'
        ja   .loop

        ; --- shift the display left and append ---
        sub  al, '0'
        call shift_in
        call refresh
        jmp  .loop

.clear:
        mov  di, digits
        mov  cx, 8
        mov  al, 0x00
        rep  stosb
        call refresh
        jmp  .loop

.check_quit:
        mov  ah, 0x0B           ; the PC keyboard, for quitting
        int  0x21
        or   al, al
        jz   .loop
        exit 0

; ---------------------------------------------------------------
; shift_in — shift the display buffer left and put digit AL at the
;            right-hand end.
; ---------------------------------------------------------------
shift_in:
        push ax
        push cx
        push si
        push di
        mov  ah, al             ; keep the new digit

        cld
        mov  si, digits + 1
        mov  di, digits
        mov  cx, 7
        rep  movsb              ; shift everything one place left

        mov  bx, segtab
        mov  al, ah
        xlat                    ; AL = the segment pattern
        mov  [digits + 7], al

        pop  di
        pop  si
        pop  cx
        pop  ax
        ret

; ---------------------------------------------------------------
; refresh — write the eight patterns to the 8279's display RAM.
;
;   With auto-increment, one command and eight writes does it — and
;   the 8279 refreshes the display for ever afterwards, with no
;   further processor involvement at all.
; ---------------------------------------------------------------
refresh:
        push ax
        push cx
        push si
        mov  al, 0x90           ; write display RAM, auto-increment, address 0
        out  CMD, al
        mov  si, digits
        mov  cx, 8
        cld
.next:
        lodsb
        out  DATA, al
        loop .next
        pop  si
        pop  cx
        pop  ax
        ret

; ---------------------------------------------------------------
kbd_read:
        in   al, CMD
        and  al, 0x0F
        jz   .none
        mov  al, 0x40           ; read the FIFO
        out  CMD, al
        in   al, DATA
        clc
        ret
.none:
        stc
        ret

; ---------------------------------------------------------------
init_8279:
        mov  al, 0x00
        out  CMD, al
        call io_delay
        mov  al, 0x34
        out  CMD, al
        call io_delay
        mov  al, 0xD1
        out  CMD, al
        push cx
        mov  cx, 500
.w:     loop .w
        pop  cx
        ret

io_delay:
        jmp  short $+2
        jmp  short $+2
        ret

; ---------------------------------------------------------------
; Common-cathode seven-segment patterns, 0-9, then blank.
segtab: db  0x3F, 0x06, 0x5B, 0x4F, 0x66
        db  0x6D, 0x7D, 0x07, 0x7F, 0x6F
        db  0x00                ; 10 = blank

; Keypad matrix positions to characters.
keytab: db  '7','8','9','/'
        db  '4','5','6','*'
        db  '1','2','3','-'
        db  '0','.','=','C'

digits: times 8 db 0
banner: db  '8279 panel — type on the keypad, C clears.', 0x0D, 0x0A
        db  'Press any PC key to quit.', 0x0D, 0x0A, '$'
