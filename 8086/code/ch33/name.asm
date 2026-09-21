; name.asm — what it does
; nasm -f bin name.asm -o name.com
        cpu  8086
        org  0x100

start:
        ; ---- code ----

        mov  ax, 0x4C00         ; DOS: terminate, exit code 0
        int  0x21

; ---- data ----
msg:    db   'Hello$'
