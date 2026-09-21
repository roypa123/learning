; strchr.asm — find the first occurrence of a character
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        mov  al, 'o'
        call strchr
        jc   .nf
        print foundmsg
        mov  ax, si
        sub  ax, str            ; convert the address to an index
        call print_udec
        newline
        exit 0
.nf:
        print nfmsg
        exit 1

; ---------------------------------------------------------------
; strchr — find AL in the ASCIIZ string at DS:SI
;   Out:       CF = 0 and SI -> the match, or CF = 1
;   Destroys:  AH, SI
; ---------------------------------------------------------------
strchr:
        cld
        mov  ah, al             ; keep the target in AH
.next:
        lodsb                   ; AL = the current character, SI advances
        cmp  al, ah
        je   .found
        or   al, al
        jnz  .next
        stc                     ; hit the terminator without a match
        ret
.found:
        dec  si                 ; LODSB advanced past it — back up
        clc
        ret

str:      db   'Hello, world!', 0
foundmsg: db   "First 'o' at index $"
nfmsg:    db   'Not found.', 0x0D, 0x0A, '$'
