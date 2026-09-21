; strlen.asm — length of a zero-terminated string
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        call strlen
        print msg
        mov  ax, cx
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; strlen — length of the ASCIIZ string at DS:SI
;   Out:       CX = the length
;   Destroys:  AL, SI
; ---------------------------------------------------------------
strlen:
        cld
        xor  cx, cx
.next:
        lodsb
        or   al, al
        jz   .done
        inc  cx
        jmp  .next
.done:
        ret

str:    db   'The quick brown fox', 0
msg:    db   'Length: $'
