; divmod.asm — divide two 16-bit numbers, print quotient and remainder
; nasm -f bin divmod.asm -o divmod.com
        org  0x100

start:
        mov  ax, [dividend]
        xor  dx, dx             ; zero-extend: DX:AX = dividend (UNSIGNED)
        mov  bx, [divisor]

        or   bx, bx             ; guard against division by zero
        jz   .divzero

        div  bx                 ; AX = quotient, DX = remainder
        mov  [quot], ax
        mov  [rem], dx

        mov  dx, qmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, [quot]
        call print_dec

        mov  dx, rmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, [rem]
        call print_dec

        mov  ax, 0x4C00
        int  0x21

.divzero:
        mov  dx, zmsg
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C01
        int  0x21

; ---------------------------------------------------------------
; print_dec — print the unsigned 16-bit value in AX as decimal,
;             followed by CR LF.
;
; Method: repeatedly divide by 10, pushing each remainder, then pop
;         them back in reverse order. The stack reverses the digits
;         for us, which is why this is the standard idiom.
; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx

        mov  bx, 10
        xor  cx, cx             ; CX counts the digits pushed

.divide:
        xor  dx, dx             ; DX:AX = the remaining value
        div  bx                 ; AX = value/10, DX = value mod 10
        push dx                 ; save the digit
        inc  cx
        or   ax, ax             ; anything left?
        jnz  .divide

.output:
        pop  dx                 ; digits come back most significant first
        add  dl, '0'            ; binary -> ASCII
        mov  ah, 0x02           ; DOS: print the character in DL
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
dividend: dw  1000
divisor:  dw  7
quot:     dw  0
rem:      dw  0
qmsg:     db  'Quotient:  $'
rmsg:     db  'Remainder: $'
zmsg:     db  'Division by zero!', 0x0D, 0x0A, '$'
