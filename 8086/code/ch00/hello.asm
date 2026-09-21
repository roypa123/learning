; hello.asm — the traditional first program
        org  0x100                  ; a .COM file is loaded at offset 0x100

start:  mov  dx, msg                ; DS:DX -> the string to print
        mov  ah, 0x09               ; DOS function 09h: write string
        int  0x21                   ; call DOS
        mov  ax, 0x4C00             ; function 4Ch, exit code 0
        int  0x21                   ; call DOS: terminate

msg:    db   'Hello, 8086!', 0x0D, 0x0A, '$'
