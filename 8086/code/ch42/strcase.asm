; strcase.asm — convert to upper and lower case
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        mov  di, buf
        call strcpy_local
        mov  si, buf
        call strupper
        print umsg
        mov  si, buf
        call puts
        newline

        mov  si, str
        mov  di, buf
        call strcpy_local
        mov  si, buf
        call strlower
        print lmsg
        mov  si, buf
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strupper — convert the ASCIIZ string at DS:SI to upper case, in place
; ---------------------------------------------------------------
strupper:
        push si
.next:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, 'a'
        jb   .skip
        cmp  al, 'z'
        ja   .skip
        and  al, 0xDF           ; clear bit 5
        mov  [si], al
.skip:
        inc  si
        jmp  .next
.done:
        pop  si
        ret

; ---------------------------------------------------------------
; strlower — convert to lower case, in place
; ---------------------------------------------------------------
strlower:
        push si
.next:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, 'A'
        jb   .skip
        cmp  al, 'Z'
        ja   .skip
        or   al, 0x20           ; set bit 5
        mov  [si], al
.skip:
        inc  si
        jmp  .next
.done:
        pop  si
        ret

strcpy_local:
        cld
.next:  lodsb
        stosb
        or   al, al
        jnz  .next
        ret

puts:
        cld
.next:  lodsb
        or   al, al
        jz   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:  ret

str:    db   'MiXeD CaSe 123!', 0
buf:    times 64 db 0
umsg:   db   'Upper: $'
lmsg:   db   'Lower: $'
