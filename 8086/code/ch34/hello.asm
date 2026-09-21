; hello.asm — the complete first program
; nasm -f bin hello.asm -o hello.com -l hello.lst
        cpu  8086
        org  0x100

start:
        mov  ah, 0x09           ; DOS function 09h: print a $-terminated string
        mov  dx, msg            ; DS:DX must point at the string
        int  0x21               ; call DOS

        mov  ah, 0x4C           ; DOS function 4Ch: terminate
        mov  al, 0              ; exit code 0
        int  0x21               ; call DOS — does not return

msg:    db   'Hello, 8086!', 0x0D, 0x0A, '$'
