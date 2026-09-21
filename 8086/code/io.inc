; io.inc — number input and output routines used throughout Part IV
;
%ifndef IO_INC
%define IO_INC

; ---------------------------------------------------------------
; print_udec — print the UNSIGNED 16-bit value in AX as decimal.
;
;   In:        AX = the value
;   Out:       nothing
;   Destroys:  nothing
;
;   Method: divide by 10 repeatedly, pushing each remainder. The
;   stack reverses the digits, which is why this idiom is standard.
; ---------------------------------------------------------------
print_udec:
        push ax
        push bx
        push cx
        push dx

        mov  bx, 10
        xor  cx, cx             ; CX counts the digits pushed
.divide:
        xor  dx, dx             ; DX:AX = the remaining value.
                                ; CLEARING DX EACH TIME IS ESSENTIAL.
        div  bx                 ; AX = value/10, DX = value mod 10
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide

.output:
        pop  dx                 ; digits come back most significant first
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
; print_sdec — print the SIGNED 16-bit value in AX as decimal.
; ---------------------------------------------------------------
print_sdec:
        push ax
        or   ax, ax
        jns  .positive
        push ax
        mov  dl, '-'
        mov  ah, 0x02
        int  0x21
        pop  ax
        neg  ax
.positive:
        call print_udec
        pop  ax
        ret

; ---------------------------------------------------------------
; print_hex16 — print AX as exactly four hex digits.
; ---------------------------------------------------------------
print_hex16:
        push ax
        push bx
        push cx
        push dx

        mov  bx, ax
        mov  cx, 4
.digit:
        rol  bx, 1              ; four ROLs bring the top nibble to the bottom
        rol  bx, 1              ;   (the 8086 has no ROL bx,4)
        rol  bx, 1
        rol  bx, 1
        mov  dl, bl
        and  dl, 0x0F
        add  dl, '0'
        cmp  dl, '9'
        jbe  .emit
        add  dl, 7              ; 'A'..'F' sit 7 past '9'+1
.emit:
        mov  ah, 0x02
        int  0x21
        loop .digit

        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; print_hex32 — print DX:AX as eight hex digits.
; ---------------------------------------------------------------
print_hex32:
        push ax
        mov  ax, dx
        call print_hex16        ; high word first
        pop  ax
        call print_hex16
        ret

; ---------------------------------------------------------------
; read_udec — read an unsigned decimal number from the keyboard.
;
;   Out:       AX = the value, CF = 1 if nothing valid was entered
;   Destroys:  nothing else
; ---------------------------------------------------------------
read_udec:
        push bx
        push cx
        push dx

        xor  bx, bx             ; BX accumulates the value
        xor  cx, cx             ; CX counts valid digits
.next:
        mov  ah, 0x01           ; read with echo
        int  0x21
        cmp  al, 0x0D           ; Enter?
        je   .done
        cmp  al, '0'
        jb   .next              ; ignore anything that is not a digit
        cmp  al, '9'
        ja   .next

        sub  al, '0'            ; ASCII -> value
        mov  ah, 0
        push ax                 ; save the new digit
        mov  ax, bx
        mov  dx, 10
        mul  dx                 ; DX:AX = old value x 10
        mov  bx, ax             ;   (overflow past 65535 is ignored)
        pop  ax
        add  bx, ax             ; + the new digit
        inc  cx
        jmp  .next

.done:
        mov  ax, bx
        or   cx, cx
        jnz  .ok
        stc                     ; no digits entered
        jmp  .out
.ok:
        clc
.out:
        pop  dx
        pop  cx
        pop  bx
        ret

%endif
