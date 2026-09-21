; freq.asm — count how often each value 0-9 appears
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- zero the counters ---
        cld
        mov  di, counts
        mov  cx, 10
        xor  ax, ax
        rep  stosw              ; the fastest way to clear memory

        ; --- count ---
        mov  si, data
        mov  cx, datalen
.next:
        lodsb                   ; AL = the next value, SI advances
        cmp  al, 9
        ja   .skip              ; ignore anything out of range
        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — counters are words
        inc  word [counts+bx]   ; the counter for this value
.skip:
        loop .next

        ; --- report ---
        xor  bx, bx             ; BX = the value being reported
.report:
        mov  ax, bx
        call print_udec
        print colon
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        call print_udec
        newline
        inc  bx
        cmp  bx, 10
        jb   .report
        exit 0

data:    db   3,7,3,1,9,3,7,0,1,3,5,7,9,9,3
datalen  equ  $ - data
counts:  times 10 dw 0
colon:   db   ': $'
