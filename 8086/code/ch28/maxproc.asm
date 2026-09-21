; maxproc.asm — find the larger of two values using a stack-parameter procedure
; nasm -f bin maxproc.asm -o maxproc.com
        org  0x100

start:
        push word [val1]        ; parameter 1
        push word [val2]        ; parameter 2
        call max16
        add  sp, 4              ; caller cleans up

        call print_dec          ; AX holds the result

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; max16(a, b) -> the larger, treating both as SIGNED
;
;   In:        a at [BP+6], b at [BP+4]
;   Out:       AX
;   Destroys:  AX, flags
;   Cleanup:   caller
; ---------------------------------------------------------------
max16:
        push bp
        mov  bp, sp

        mov  ax, [bp+6]         ; a
        cmp  ax, [bp+4]         ; compare with b
        jge  .done              ; signed comparison — a >= b, keep a
        mov  ax, [bp+4]         ; otherwise take b
.done:
        pop  bp
        ret

; ---------------------------------------------------------------
; print_dec — print the SIGNED value in AX as decimal, then CR LF
;
;   In:        AX
;   Destroys:  nothing
; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx

        or   ax, ax
        jns  .positive
        push ax                 ; remember that it was negative
        mov  dl, '-'
        mov  ah, 0x02
        int  0x21
        pop  ax
        neg  ax
.positive:
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

val1:   dw   -500
val2:   dw   1234
