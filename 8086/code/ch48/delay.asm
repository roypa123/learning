; delay.asm — a delay measured with the hardware timer
; nasm -f bin delay.asm -o delay.com
;
; Runs under DOSBox. Uses the BIOS tick counter at 0040:006C, which
; the timer's counter 0 drives at 18.2065 Hz.
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print startmsg

        mov  ax, 5
        call delay_seconds

        print donemsg
        exit 0

; ---------------------------------------------------------------
; delay_seconds — wait AX seconds.
;
;   The BIOS tick counter at 0040:006C increments 18.2065 times a
;   second. Ticks needed = seconds × 18.2065, approximated as
;   seconds × 1193 / 65.536 ... in integer terms, × 18 plus a
;   correction. Good to about 1%.
;
;   Destroys:  nothing
; ---------------------------------------------------------------
delay_seconds:
        push ax
        push bx
        push cx
        push dx
        push es

        ; --- ticks = seconds × 18.2065, done as (seconds × 1165) / 64 ---
        mov  bx, 1165
        mul  bx                 ; DX:AX = seconds × 1165
        mov  cl, 6
        shr  ax, cl             ; ÷ 64   (DX is zero for sane inputs)
        mov  cx, ax             ; CX = the tick count to wait

        ; --- read the starting tick ---
        mov  ax, 0x0040
        mov  es, ax
        cli
        mov  bx, [es:0x6C]      ; the low word is enough for short waits
        sti

.wait:
        cli
        mov  ax, [es:0x6C]
        sti
        sub  ax, bx             ; elapsed ticks (wraps correctly)
        cmp  ax, cx
        jb   .wait

        pop  es
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

startmsg: db 'Waiting five seconds...', 0x0D, 0x0A, '$'
donemsg:  db 'Done.', 0x0D, 0x0A, '$'
