; palin.asm — is a string a palindrome, ignoring case and punctuation?
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        call is_palindrome
        jc   .no
        print yesmsg
        exit 0
.no:
        print nomsg
        exit 1

; ---------------------------------------------------------------
; is_palindrome — test the ASCIIZ string at DS:SI
;   Out:       CF = 0 if it is, CF = 1 if not
;   Destroys:  AX, BX, CX, SI, DI
;
;   Ignores anything that is not a letter, and ignores case.
; ---------------------------------------------------------------
is_palindrome:
        push si
        ; --- find the end ---
        mov  di, si
        cld
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb
        sub  di, 2              ; back past the terminator to the last character
        pop  si

.loop:
        cmp  si, di
        jae  .yes               ; pointers met or crossed -> palindrome

        ; --- advance SI to the next letter ---
.skip_left:
        cmp  si, di
        jae  .yes
        mov  al, [si]
        call is_letter
        jnc  .got_left
        inc  si
        jmp  .skip_left
.got_left:

        ; --- retreat DI to the previous letter ---
.skip_right:
        cmp  si, di
        jae  .yes
        mov  bl, [di]
        push ax
        mov  al, bl
        call is_letter
        mov  bl, al
        pop  ax
        jnc  .got_right
        dec  di
        jmp  .skip_right
.got_right:

        ; --- compare, case-insensitively ---
        or   al, 0x20           ; force lower case (both are known letters)
        or   bl, 0x20
        cmp  al, bl
        jne  .no

        inc  si
        dec  di
        jmp  .loop

.yes:
        clc
        ret
.no:
        stc
        ret

; ---------------------------------------------------------------
; is_letter — CF = 0 if AL is A-Z or a-z, CF = 1 otherwise
;   Preserves AL.
; ---------------------------------------------------------------
is_letter:
        cmp  al, 'A'
        jb   .no
        cmp  al, 'Z'
        jbe  .yes
        cmp  al, 'a'
        jb   .no
        cmp  al, 'z'
        jbe  .yes
.no:    stc
        ret
.yes:   clc
        ret

str:     db   'A man, a plan, a canal: Panama', 0
yesmsg:  db   'It is a palindrome.', 0x0D, 0x0A, '$'
nomsg:   db   'Not a palindrome.', 0x0D, 0x0A, '$'
