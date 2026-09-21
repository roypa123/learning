; tokens.asm — split a string on spaces and print each token on its own line
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, text
        xor  bx, bx             ; BX = token count
        cld

.find_token:
        ; --- skip leading separators ---
.skip:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, ' '
        jne  .found_start
        inc  si
        jmp  .skip

.found_start:
        inc  bx
        mov  di, si             ; DI -> the start of this token

        ; --- find the end ---
.scan:
        mov  al, [si]
        or   al, al
        jz   .emit
        cmp  al, ' '
        je   .emit
        inc  si
        jmp  .scan

.emit:
        push si
        push ax
        ; print from DI up to (but not including) SI
        mov  ax, bx
        call print_udec
        print colon
        mov  cx, si
        sub  cx, di             ; CX = the token length
.putc:
        mov  dl, [di]
        push ax
        mov  ah, 0x02
        int  0x21
        pop  ax
        inc  di
        loop .putc
        newline
        pop  ax
        pop  si

        or   al, al
        jz   .done
        inc  si                 ; step past the separator
        jmp  .find_token

.done:
        print tmsg
        mov  ax, bx
        call print_udec
        newline
        exit 0

text:   db   '  alpha beta   gamma delta  ', 0
colon:  db   ': $'
tmsg:   db   'Total tokens: $'
