; gcd.asm — greatest common divisor by Euclid's algorithm
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  ax, [a]
        mov  bx, [b]

        ; --- Euclid: while a != b, subtract the smaller from the larger ---
.loop:
        cmp  ax, bx
        je   .done              ; equal -> that is the GCD
        ja   .a_bigger
        sub  bx, ax             ; b > a
        jmp  .loop
.a_bigger:
        sub  ax, bx             ; a > b
        jmp  .loop
.done:
        mov  [result], ax

        print msg
        mov  ax, [a]
        call print_udec
        print andmsg
        mov  ax, [b]
        call print_udec
        print eqmsg
        mov  ax, [result]
        call print_udec
        newline
        exit 0

a:       dw   1071
b:       dw   462
result:  dw   0
msg:     db   'gcd($'
andmsg:  db   ', $'
eqmsg:   db   ') = $'
