; first.asm - prints a message and exits
; Assemble:  nasm -f bin first.asm -o first.com
; Run:       first.com      (inside DOSBox)

        org  0x100              ; DOS loads a .COM image at offset 0x100

start:
        mov  ah, 0x09           ; DOS service 09h = print $-terminated string
        mov  dx, msg            ; DS:DX must point at the string
        int  0x21               ; invoke DOS

        mov  ah, 0x4C           ; DOS service 4Ch = terminate program
        mov  al, 0              ; exit code 0
        int  0x21               ; invoke DOS - does not return

msg:    db   'The toolchain works.', 0x0D, 0x0A, '$'
