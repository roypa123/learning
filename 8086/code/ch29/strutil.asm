; strutil.asm — length, copy, compare and search, using string instructions
; nasm -f bin strutil.asm -o strutil.com
        org  0x100

start:
        cld                     ; forwards, once, for the whole program
        mov  ax, ds
        mov  es, ax             ; ES = DS (already true for a .COM, but be explicit)

        ; --- length of str1 ---
        mov  di, str1
        call strlen             ; CX = length
        mov  ax, cx
        call print_dec

        ; --- copy str1 to buffer ---
        mov  si, str1
        mov  di, buffer
        call strcpy

        mov  dx, buffer
        mov  ah, 0x09
        int  0x21

        ; --- compare str1 with str2 ---
        mov  si, str1
        mov  di, str2
        call strcmp
        je   .same
        mov  dx, diffmsg
        jmp  .say
.same:  mov  dx, samemsg
.say:   mov  ah, 0x09
        int  0x21

        ; --- find 'o' in str1 ---
        mov  di, str1
        mov  al, 'o'
        call strchr
        jc   .notfound
        mov  dx, foundmsg
        jmp  .say2
.notfound:
        mov  dx, nofoundmsg
.say2:  mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; strlen — length of the $-terminated string at ES:DI
;   Out:       CX = length
;   Destroys:  AL, DI, flags
; ---------------------------------------------------------------
strlen:
        push di
        mov  al, '$'
        mov  cx, 0xFFFF         ; scan at most 65535 bytes
        repne scasb             ; stop when AL matches
        ; CX now = 0xFFFF − (length + 1)
        not  cx                 ; CX = length + 1
        dec  cx                 ; CX = length
        pop  di
        ret

; ---------------------------------------------------------------
; strcpy — copy the $-terminated string at DS:SI to ES:DI
;   Destroys:  AL, SI, DI, flags
; ---------------------------------------------------------------
strcpy:
.next:
        lodsb                   ; AL <- [SI], SI++
        stosb                   ; [DI] <- AL, DI++
        cmp  al, '$'
        jne  .next
        ret

; ---------------------------------------------------------------
; strcmp — compare the $-terminated strings at DS:SI and ES:DI
;   Out:       ZF = 1 if identical; otherwise flags from the first difference
;   Destroys:  AL, SI, DI, flags
; ---------------------------------------------------------------
strcmp:
.next:
        mov  al, [si]
        cmpsb                   ; compares [SI] with [DI], advances both
        jne  .done              ; a difference — flags already set
        cmp  al, '$'            ; end of both strings?
        jne  .next
        ; fell through with AL = '$' and everything equal -> ZF is already 1
.done:
        ret

; ---------------------------------------------------------------
; strchr — find AL in the $-terminated string at ES:DI
;   Out:       CF = 0 and DI -> the match, or CF = 1 if not found
;   Destroys:  CX, DI, flags
; ---------------------------------------------------------------
strchr:
        push ax
        mov  ah, al             ; keep the target in AH
        mov  cx, 0xFFFF
.next:
        mov  al, [di]
        cmp  al, '$'
        je   .notfound
        cmp  al, ah
        je   .found
        inc  di
        loop .next
.notfound:
        pop  ax
        stc                     ; CF = 1 -> not found
        ret
.found:
        pop  ax
        clc                     ; CF = 0 -> DI points at it
        ret

; ---------------------------------------------------------------
; print_dec — print AX as unsigned decimal, then CR LF
; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx
        mov  bx, 10
        xor  cx, cx
.divide:
        xor  dx, dx
        div  bx
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide
.output:
        pop  dx
        add  dl, '0'
        mov  ah, 0x02
        int  0x21
        loop .output
        mov  dl, 0x0D
        mov  ah, 0x02
        int  0x21
        mov  dl, 0x0A
        mov  ah, 0x02
        int  0x21
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
str1:       db   'Hello, world!$'
str2:       db   'Hello, there!$'
buffer:     times 80 db 0
samemsg:    db   'Strings are identical', 0x0D, 0x0A, '$'
diffmsg:    db   'Strings differ', 0x0D, 0x0A, '$'
foundmsg:   db   "Found 'o'", 0x0D, 0x0A, '$'
nofoundmsg: db   "No 'o' found", 0x0D, 0x0A, '$'
