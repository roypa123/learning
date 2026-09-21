; strcmp.asm — compare two strings
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, s1
        mov  di, s2
        call strcmp
        call report

        mov  si, s1
        mov  di, s3
        call strcmp
        call report

        mov  si, s1
        mov  di, s1
        call strcmp
        call report
        exit 0

; ---------------------------------------------------------------
; strcmp — compare the ASCIIZ strings at DS:SI and ES:DI
;   Out:       AX = -1 if SI < DI, 0 if equal, +1 if SI > DI
;   Destroys:  SI, DI, BL, flags
; ---------------------------------------------------------------
strcmp:
        cld
.next:
        mov  bl, [si]           ; keep the source byte; CMPSB advances SI
        cmpsb                   ; compare [SI] with [DI], both advance
        jne  .differ
        or   bl, bl             ; both were the terminator?
        jnz  .next              ;   no — continue
        xor  ax, ax             ; equal
        ret
.differ:
        jb   .less              ; UNSIGNED: characters are unsigned
        mov  ax, 1
        ret
.less:
        mov  ax, -1
        ret

report:
        or   ax, ax
        js   .less
        jz   .equal
        print gtmsg
        ret
.equal: print eqmsg
        ret
.less:  print ltmsg
        ret

s1:     db   'apple', 0
s2:     db   'banana', 0
s3:     db   'apple pie', 0
ltmsg:  db   'first < second', 0x0D, 0x0A, '$'
eqmsg:  db   'first = second', 0x0D, 0x0A, '$'
gtmsg:  db   'first > second', 0x0D, 0x0A, '$'
