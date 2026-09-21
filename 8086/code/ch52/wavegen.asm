; wavegen.asm — generate sawtooth, triangle, square and sine waves
; nasm -f bin wavegen.asm -o wavegen.com
        cpu  8086
        org  0x100
%include "macros.inc"

PORTA   equ  0x00
CTRL    equ  0x06

start:
        mov  al, 0x80           ; all 8255 ports output
        out  CTRL, al

        call build_sine

        print menu

.getkey:
        xor  ah, ah
        int  0x16
        cmp  al, '1'
        je   .sawtooth
        cmp  al, '2'
        je   .triangle
        cmp  al, '3'
        je   .square
        cmp  al, '4'
        je   .sine
        cmp  al, 27
        je   .done
        jmp  .getkey

; ---------------------------------------------------------------
; A sawtooth: ramp 0 to 255, then snap back.
; ---------------------------------------------------------------
.sawtooth:
        xor  al, al
.saw_next:
        out  PORTA, al
        inc  al
        jnz  .saw_next          ; wraps to 0 and repeats
        call check_key
        jnc  .sawtooth
        jmp  .getkey

; ---------------------------------------------------------------
; A triangle: ramp up, then ramp down.
; ---------------------------------------------------------------
.triangle:
        xor  al, al
.tri_up:
        out  PORTA, al
        inc  al
        cmp  al, 255
        jb   .tri_up
.tri_down:
        out  PORTA, al
        dec  al
        jnz  .tri_down
        call check_key
        jnc  .triangle
        jmp  .getkey

; ---------------------------------------------------------------
; A square wave: alternate between 0 and 255.
; ---------------------------------------------------------------
.square:
        mov  al, 0
        out  PORTA, al
        call half_period
        mov  al, 255
        out  PORTA, al
        call half_period
        call check_key
        jnc  .square
        jmp  .getkey

; ---------------------------------------------------------------
; A sine wave, from the table built at start-up.
; ---------------------------------------------------------------
.sine:
        xor  si, si
.sin_next:
        mov  al, [sintab + si]
        out  PORTA, al
        inc  si
        cmp  si, 256
        jb   .sin_next
        call check_key
        jnc  .sine
        jmp  .getkey

.done:
        xor  al, al
        out  PORTA, al          ; leave the output at zero
        exit 0

; ---------------------------------------------------------------
; build_sine — fill a 256-entry sine table.
;
;   No floating point and no trigonometry: build a quarter wave by
;   successive approximation of the circle equation, then mirror it.
;
;   For a quarter sine we use  y = 127 × sin(90° × i/64), which we
;   approximate from a circle: for x from 0 to 63,
;       y = sqrt(63² − (63−x)²)  scaled
;   giving a quarter of an ellipse — close enough to a sine for a
;   waveform demonstration, and computable with integer maths only.
; ---------------------------------------------------------------
build_sine:
        push ax
        push bx
        push cx
        push dx
        push di

        mov  di, sintab
        xor  cx, cx             ; CX = the index 0..255
.next:
        ; --- map the index to a quarter and a quadrant ---
        mov  ax, cx
        and  ax, 0x3F           ; position within the quarter, 0-63
        mov  bx, cx
        shr  bx, 1
        shr  bx, 1
        shr  bx, 1
        shr  bx, 1
        shr  bx, 1
        shr  bx, 1              ; BX = the quadrant 0-3

        ; rising quarters use AX, falling quarters use 63-AX
        test bl, 1
        jz   .have_pos
        mov  dx, 63
        sub  dx, ax
        mov  ax, dx
.have_pos:
        call quarter_sine       ; AL = 0..127 for the quarter

        ; quadrants 2 and 3 are the negative half
        test bl, 2
        jz   .positive
        mov  ah, 128
        sub  ah, al
        mov  al, ah
        jmp  .store
.positive:
        add  al, 128
.store:
        mov  [di], al
        inc  di
        inc  cx
        cmp  cx, 256
        jb   .next

        pop  di
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; quarter_sine — approximate 127 × sin(90° × AX/63) for AX = 0..63
;   using the circle  y = sqrt(63² − (63−x)²), scaled by 2.
; ---------------------------------------------------------------
quarter_sine:
        push bx
        push cx
        push dx
        mov  bx, 63
        sub  bx, ax             ; BX = 63 − x
        mov  ax, bx
        imul bx                 ; AX = (63−x)²
        mov  bx, ax
        mov  ax, 63*63
        sub  ax, bx             ; AX = 63² − (63−x)²
        call isqrt              ; AX = the integer square root, 0..63
        shl  ax, 1              ; scale to 0..126
        pop  dx
        pop  cx
        pop  bx
        ret

; ---------------------------------------------------------------
; isqrt — integer square root of AX, by successive subtraction of
;   odd numbers:  n² = 1 + 3 + 5 + ... + (2n−1)
;   Out: AX = floor(sqrt(input))
; ---------------------------------------------------------------
isqrt:
        push bx
        push cx
        mov  bx, 1              ; the next odd number
        xor  cx, cx             ; the running root
.next:
        cmp  ax, bx
        jb   .done
        sub  ax, bx
        add  bx, 2
        inc  cx
        jmp  .next
.done:
        mov  ax, cx
        pop  cx
        pop  bx
        ret

; ---------------------------------------------------------------
half_period:
        push cx
        mov  cx, 500
.wait:  loop .wait
        pop  cx
        ret

check_key:
        push ax
        mov  ah, 0x01
        int  0x16
        jz   .none
        pop  ax
        stc
        ret
.none:
        pop  ax
        clc
        ret

sintab:  times 256 db 0
menu:    db  'Waveform generator', 0x0D, 0x0A
         db  '  1 sawtooth  2 triangle  3 square  4 sine  Esc quit', 0x0D, 0x0A, '$'
