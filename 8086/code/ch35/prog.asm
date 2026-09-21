; prog.asm — an .EXE program
; nasm -f obj prog.asm -o prog.obj
; wlink file prog.obj format dos name prog.exe
        cpu  8086

        segment code
..start:                        ; NASM's special entry-point label
        mov  ax, data
        mov  ds, ax             ; MUST do this
        mov  ax, stack
        mov  ss, ax
        mov  sp, stacktop

        mov  dx, msg
        mov  ah, 0x09
        int  0x21

        mov  ax, 0x4C00
        int  0x21

        segment data
msg:    db   'Hello from an EXE!', 0x0D, 0x0A, '$'

        segment stack stack
        resb 256
stacktop:
