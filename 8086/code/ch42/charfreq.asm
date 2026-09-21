; charfreq.asm — count how often each letter appears
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- clear the 26 counters ---
        cld
        mov  di, counts
        mov  cx, 26
        xor  ax, ax
        rep  stosw

        ; --- count ---
        mov  si, text
.next:
        lodsb
        or   al, al
        jz   .report
        or   al, 0x20           ; fold to lower case
        cmp  al, 'a'
        jb   .next
        cmp  al, 'z'
        ja   .next
        sub  al, 'a'            ; 0..25
        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — counters are words
        inc  word [counts+bx]
        jmp  .next

        ; --- report only the non-zero counts ---
.report:
        xor  bx, bx
.loop:
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        or   ax, ax
        jz   .skip
        mov  al, bl
        add  al, 'a'
        putc al
        print colon
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        call print_udec
        newline
.skip:
        inc  bx
        cmp  bx, 26
        jb   .loop
        exit 0

text:   db   'the quick brown fox jumps over the lazy dog', 0
counts: times 26 dw 0
colon:  db   ': $'
