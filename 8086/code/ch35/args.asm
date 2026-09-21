; args.asm — print the command-line arguments
; nasm -f bin args.asm -o args.com
;
; Run as:   args.com hello world
        cpu  8086
        org  0x100

start:
        mov  cl, [0x80]         ; length of the command tail
        xor  ch, ch
        jcxz .none              ; nothing typed after the program name

        mov  dx, tailmsg
        mov  ah, 0x09
        int  0x21

        mov  si, 0x81           ; the text starts here
.next:
        mov  dl, [si]
        mov  ah, 0x02           ; DOS: print the character in DL
        int  0x21
        inc  si
        loop .next

        mov  dx, crlf
        mov  ah, 0x09
        int  0x21
        jmp  .size

.none:
        mov  dx, nonemsg
        mov  ah, 0x09
        int  0x21

.size:
        ; --- report how much memory we own ---
        mov  dx, memmsg
        mov  ah, 0x09
        int  0x21

        mov  ax, [0x02]         ; segment past our block
        mov  bx, cs
        sub  ax, bx             ; paragraphs we own
        mov  cl, 4
        shr  ax, cl             ; ÷16 -> KiB  (paragraphs × 16 / 1024 = ÷ 64)
        shr  ax, 1
        shr  ax, 1              ; total ÷ 64
        call print_dec

        mov  dx, kbmsg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

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
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
tailmsg:  db   'Command tail:$'
nonemsg:  db   'No arguments given.', 0x0D, 0x0A, '$'
memmsg:   db   'Memory owned: $'
kbmsg:    db   ' KiB', 0x0D, 0x0A, '$'
crlf:     db   0x0D, 0x0A, '$'
