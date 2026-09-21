; fact.asm — compute n! iteratively
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print prompt
        call read_udec
        jc   .bad
        cmp  ax, 8              ; 8! = 40320 fits in 16 bits; 9! does not
        ja   .too_big
        mov  [n], ax
        newline

        ; --- the computation ---
        mov  cx, [n]
        mov  ax, 1              ; 0! = 1
        jcxz .done              ; guard: CX = 0 would loop 65536 times
.next:
        mul  cx                 ; DX:AX = AX × CX.  We ignore DX because
                                ;   we checked n <= 8 above.
        loop .next              ; CX counts down: n, n-1, ... 1
.done:
        mov  [result], ax

        print rmsg
        mov  ax, [n]
        call print_udec
        print emsg
        mov  ax, [result]
        call print_udec
        newline
        exit 0

.bad:
        print badmsg
        exit 1
.too_big:
        print bigmsg
        exit 1

n:       dw   0
result:  dw   0
prompt:  db   'Enter n (0-8): $'
rmsg:    db   '$'
emsg:    db   '! = $'
badmsg:  db   0x0D, 0x0A, 'Not a number.', 0x0D, 0x0A, '$'
bigmsg:  db   0x0D, 0x0A, 'Too large — 9! exceeds 16 bits.', 0x0D, 0x0A, '$'
