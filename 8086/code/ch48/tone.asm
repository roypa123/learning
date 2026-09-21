; tone.asm — play tones on the PC speaker using counter 2
; nasm -f bin tone.asm -o tone.com
;
; Counter 2's OUT drives the speaker, gated by port 0x61 bits 0 and 1.
;   bit 0 = counter 2 GATE
;   bit 1 = speaker data enable
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

TIMER0  equ  0x40
TIMER2  equ  0x42
TCTRL   equ  0x43
SPKPORT equ  0x61

start:
        print msg

        ; --- a scale ---
        mov  si, notes
.next:
        lodsw                   ; AX = the frequency
        or   ax, ax
        jz   .done
        call play_note
        jmp  .next

.done:
        call speaker_off
        print donemsg
        exit 0

; ---------------------------------------------------------------
; play_note — sound AX Hz for about a quarter of a second.
;   Destroys:  nothing
; ---------------------------------------------------------------
play_note:
        push ax
        push bx
        push cx
        push dx

        ; --- N = 1193182 / frequency ---
        mov  bx, ax
        mov  dx, 0x0012         ; DX:AX = 1,193,182 = 0x1234DE
        mov  ax, 0x34DE
        div  bx                 ; AX = the divisor N
        mov  bx, ax             ; keep it

        ; --- programme counter 2, mode 3 (square wave), binary ---
        mov  al, 1011_0110b     ; SC=10 (counter 2), RW=11, mode 3, binary
        out  TCTRL, al
        mov  ax, bx
        out  TIMER2, al         ; low byte
        mov  al, ah
        out  TIMER2, al         ; high byte

        ; --- turn the speaker on ---
        in   al, SPKPORT
        or   al, 0x03           ; set bits 0 and 1
        out  SPKPORT, al

        mov  ax, 250
        call delay_ms

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; speaker_off — disconnect the speaker.
; ---------------------------------------------------------------
speaker_off:
        push ax
        in   al, SPKPORT
        and  al, 0xFC           ; clear bits 0 and 1
        out  SPKPORT, al
        pop  ax
        ret

; ---------------------------------------------------------------
; delay_ms — wait roughly AX milliseconds using the BIOS tick.
;   Resolution is 55 ms, so anything shorter rounds up to one tick.
; ---------------------------------------------------------------
delay_ms:
        push ax
        push bx
        push cx
        push es
        mov  bx, 55
        xor  dx, dx
        div  bx                 ; AX = ticks (rounded down)
        or   ax, ax
        jnz  .have_ticks
        mov  ax, 1              ; always wait at least one tick
.have_ticks:
        mov  cx, ax

        mov  ax, 0x0040
        mov  es, ax
        mov  bx, [es:0x6C]
.wait:
        mov  ax, [es:0x6C]
        sub  ax, bx
        cmp  ax, cx
        jb   .wait

        pop  es
        pop  cx
        pop  bx
        pop  ax
        ret

; --- a C major scale, in Hz, terminated by zero ---
notes:  dw   262, 294, 330, 349, 392, 440, 494, 523, 0

msg:      db 'Playing a scale...', 0x0D, 0x0A, '$'
donemsg:  db 'Done.', 0x0D, 0x0A, '$'
