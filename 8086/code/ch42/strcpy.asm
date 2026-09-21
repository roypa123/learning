; strcpy.asm — copy a string
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, source
        mov  di, dest
        call strcpy

        print msg
        mov  si, dest
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strcpy — copy the ASCIIZ string at DS:SI to ES:DI, terminator included
;   Destroys:  AL, SI, DI
; ---------------------------------------------------------------
strcpy:
        cld
.next:
        lodsb                   ; AL <- [DS:SI], SI++
        stosb                   ; [ES:DI] <- AL, DI++
        or   al, al             ; was that the terminator?
        jnz  .next              ;   no — keep going (and it HAS been copied)
        ret

; ---------------------------------------------------------------
; puts — print the ASCIIZ string at DS:SI
;   Destroys:  AL, DL, SI, AH
; ---------------------------------------------------------------
puts:
        cld
.next:
        lodsb
        or   al, al
        jz   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:
        ret

source: db   'Copy me exactly.', 0
dest:   times 64 db 0
msg:    db   'Copied: $'
