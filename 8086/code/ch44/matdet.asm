; matdet.asm — determinant of a 3×3 matrix
;
;   | a b c |
;   | d e f |  =  a(ei − fh) − b(di − fg) + c(dh − eg)
;   | g h i |
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print mmsg
        mov  si, m
        mov  bx, 3
        mov  bp, 3
        call show_rect

        call det3

        print dmsg
        call print_sdec
        newline
        exit 0

; ---------------------------------------------------------------
; det3 — determinant of the 3×3 matrix at `m`
;
;   Out:       AX = the determinant (16-bit; may overflow for large
;              inputs — see Exercise 44.12)
;   Destroys:  BX, CX, DX
;
;   Element offsets, row-major:
;       a=0  b=2  c=4
;       d=6  e=8  f=10
;       g=12 h=14 i=16
; ---------------------------------------------------------------
det3:
        ; --- term 1: a × (e×i − f×h) ---
        mov  ax, [m+8]          ; e
        imul word [m+16]        ; DX:AX = e × i
        mov  cx, ax             ; keep the low word
        mov  ax, [m+10]         ; f
        imul word [m+14]        ; f × h
        sub  cx, ax             ; CX = ei − fh
        mov  ax, [m+0]          ; a
        imul cx                 ; a × (ei − fh)
        mov  bx, ax             ; BX accumulates the determinant

        ; --- term 2: − b × (d×i − f×g) ---
        mov  ax, [m+6]          ; d
        imul word [m+16]        ; d × i
        mov  cx, ax
        mov  ax, [m+10]         ; f
        imul word [m+12]        ; f × g
        sub  cx, ax             ; CX = di − fg
        mov  ax, [m+2]          ; b
        imul cx
        sub  bx, ax             ; minus

        ; --- term 3: + c × (d×h − e×g) ---
        mov  ax, [m+6]          ; d
        imul word [m+14]        ; d × h
        mov  cx, ax
        mov  ax, [m+8]          ; e
        imul word [m+12]        ; e × g
        sub  cx, ax             ; CX = dh − eg
        mov  ax, [m+4]          ; c
        imul cx
        add  bx, ax             ; plus

        mov  ax, bx
        ret

m:      dw   6, 1, 1
        dw   4, -2, 5
        dw   2, 8, 7
mmsg:   db   'Matrix:', 0x0D, 0x0A, '$'
dmsg:   db   'Determinant = $'
