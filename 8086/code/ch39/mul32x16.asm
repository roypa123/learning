; mul32x16.asm — multiply a 32-bit value by a 16-bit value
;
;   Let the 32-bit value be H:L (H = high word, L = low word) and the
;   multiplier be M.
;
;      (H×65536 + L) × M  =  H×M×65536  +  L×M
;
;   L×M gives a 32-bit partial product.
;   H×M gives another, shifted up by 16 bits.
;   Add them with the shift accounted for.
;
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- L × M ---
        mov  ax, [val]          ; low word L
        mul  word [mult]        ; DX:AX = L × M
        mov  [res], ax          ; the low word of the result is final
        mov  bx, dx             ; BX = the carry into the next 16 bits

        ; --- H × M ---
        mov  ax, [val+2]        ; high word H
        mul  word [mult]        ; DX:AX = H × M
        add  ax, bx             ; add the carry from the first product
        adc  dx, 0              ; propagate into the top word
        mov  [res+2], ax
        mov  [res+4], dx        ; the result is 48 bits wide

        print msg
        mov  ax, [res+4]
        call print_hex16
        mov  ax, [res+2]
        call print_hex16
        mov  ax, [res]
        call print_hex16
        newline
        exit 0

val:    dd   0x00012345
mult:   dw   0x1000
res:    times 3 dw 0            ; 48 bits
msg:    db   '00012345 * 1000 = 0x$'
