; strcat.asm — append one string to another
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  di, buffer
        mov  si, part1
        call strcpy_di          ; buffer = part1

        mov  di, buffer
        mov  si, part2
        call strcat             ; buffer = buffer + part2

        mov  di, buffer
        mov  si, part3
        call strcat

        print msg
        mov  si, buffer
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strcat — append DS:SI to the ASCIIZ string at ES:DI
;   Destroys:  AL, SI, DI
; ---------------------------------------------------------------
strcat:
        cld
        ; --- first, find the end of the destination ---
        push ax
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb             ; DI ends up one PAST the terminator
        dec  di                 ; back onto the terminator
        pop  ax
        ; --- now copy, overwriting that terminator ---
strcpy_di:
        cld
.next:
        lodsb
        stosb
        or   al, al
        jnz  .next
        ret

buffer: times 128 db 0
part1:  db   'Hello, ', 0
part2:  db   'cruel ', 0
part3:  db   'world!', 0
msg:    db   'Result: $'
