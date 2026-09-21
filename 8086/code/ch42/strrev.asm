; strrev.asm — reverse a string in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print bmsg
        mov  si, str
        call puts
        newline

        mov  si, str
        call strrev

        print amsg
        mov  si, str
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strrev — reverse the ASCIIZ string at DS:SI, in place
;   Destroys:  AX, CX, SI, DI
; ---------------------------------------------------------------
strrev:
        push si
        ; --- find the length ---
        mov  di, si
        cld
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb
        not  cx
        dec  cx                 ; CX = the length
        pop  si

        cmp  cx, 2
        jb   .done              ; 0 or 1 characters — nothing to do

        mov  di, si
        add  di, cx
        dec  di                 ; DI -> the last character
        shr  cx, 1              ; swap only half of them

.swap:
        mov  al, [si]
        mov  ah, [di]
        mov  [si], ah
        mov  [di], al
        inc  si
        dec  di
        loop .swap
.done:
        ret

str:    db   'Stressed was I ere I saw desserts', 0
bmsg:   db   'Before: $'
amsg:   db   'After:  $'
