; quad.asm — solve ax² + bx + c = 0 using the 8087
; nasm -f bin quad.asm -o quad.com
;
; Runs under DOSBox, which emulates an x87.
        cpu  8086
        fpu  8087
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        finit                   ; initialise the 8087

        print banner

        ; --- discriminant: d = b² − 4ac ---
        fld   qword [b]
        fmul  st0, st0          ; ST0 = b²
        fld   qword [a]
        fmul  qword [c]         ; ST0 = ac, ST1 = b²
        fadd  st0, st0          ; ST0 = 2ac
        fadd  st0, st0          ; ST0 = 4ac
        fsubp st1, st0          ; ST0 = b² − 4ac, and pop
        fst   qword [disc]      ; keep a copy

        ; --- is it negative? ---
        ftst                    ; compare ST0 with 0
        fstsw word [status]
        fwait
        mov   ax, [status]
        sahf
        jb    .complex          ; CF = 1 means ST0 < 0

        ; --- real roots ---
        fsqrt                   ; ST0 = sqrt(d)
        fst   qword [sq]

        ; --- root1 = (−b + sqrt(d)) / (2a) ---
        fld   qword [b]
        fchs                    ; ST0 = −b, ST1 = sqrt(d)
        fadd  st0, st1          ; ST0 = −b + sqrt(d)
        fld   qword [a]
        fadd  st0, st0          ; ST0 = 2a
        fdivp st1, st0          ; ST0 = (−b + sqrt(d)) / (2a)
        fstp  qword [root1]     ; store and pop

        ; --- root2 = (−b − sqrt(d)) / (2a) ---
        fld   qword [b]
        fchs
        fsub  qword [sq]        ; ST0 = −b − sqrt(d)
        fld   qword [a]
        fadd  st0, st0
        fdivp st1, st0
        fstp  qword [root2]

        ; --- report ---
        print r1msg
        fld   qword [root1]
        call  print_real
        newline
        print r2msg
        fld   qword [root2]
        call  print_real
        newline
        exit 0

.complex:
        print cmsg
        exit 0

; ---------------------------------------------------------------
; print_real — print ST0 to three decimal places, then pop it.
;
;   Method: multiply by 1000, round to an integer, store as a
;   32-bit integer, then print it with a decimal point inserted.
;   That avoids any floating-point-to-string conversion.
;
;   Destroys: AX, BX, CX, DX
; ---------------------------------------------------------------
print_real:
        ; --- handle the sign ---
        ftst
        fstsw word [status]
        fwait
        mov   ax, [status]
        sahf
        jae   .positive
        putc  '-'
        fabs
.positive:
        fmul  qword [thousand]
        frndint                 ; round to the nearest integer
        fistp dword [itemp]     ; store as a 32-bit integer and pop
        fwait

        mov   ax, [itemp]
        mov   dx, [itemp+2]     ; DX:AX = the scaled value

        ; --- whole part ---
        mov   bx, 1000
        div   bx                ; AX = whole, DX = the fraction
        push  dx
        call  print_udec
        putc  '.'

        ; --- three fractional digits, zero padded ---
        pop   ax
        mov   bx, 100
        xor   dx, dx
        div   bx
        add   al, '0'
        putc  al
        mov   ax, dx
        mov   bx, 10
        xor   dx, dx
        div   bx
        add   al, '0'
        putc  al
        mov   al, dl
        add   al, '0'
        putc  al
        ret

; ---------------------------------------------------------------
a:         dq  1.0
b:         dq  -5.0
c:         dq  6.0
disc:      dq  0.0
sq:        dq  0.0
root1:     dq  0.0
root2:     dq  0.0
thousand:  dq  1000.0
itemp:     dd  0
status:    dw  0

banner: db  'Solving x^2 - 5x + 6 = 0', 0x0D, 0x0A, '$'
r1msg:  db  'Root 1: $'
r2msg:  db  'Root 2: $'
cmsg:   db  'The roots are complex.', 0x0D, 0x0A, '$'
