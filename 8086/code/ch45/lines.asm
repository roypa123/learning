; lines.asm — fast horizontal and vertical lines
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10
        mov  ax, 0xA000
        mov  es, ax
        call build_rowtab

        ; --- a border ---
        mov  ax, 0              ; x1
        mov  bx, 319            ; x2
        mov  cx, 0              ; y
        mov  dl, 15
        call hline
        mov  cx, 199
        call hline
        mov  ax, 0              ; x
        mov  bx, 0              ; y1
        mov  cx, 199            ; y2
        call vline
        mov  ax, 319
        call vline

        ; --- some coloured bars ---
        mov  bp, 20             ; y
        mov  dl, 1
.bar:
        mov  ax, 40
        mov  bx, 280
        mov  cx, bp
        call hline
        add  bp, 10
        inc  dl
        cmp  bp, 190
        jb   .bar

        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; hline — horizontal line from (AX, CX) to (BX, CX) in colour DL
;   Uses REP STOSB, so it is as fast as a fill.
;   Destroys: nothing
; ---------------------------------------------------------------
hline:
        push ax
        push bx
        push cx
        push di

        cmp  ax, bx             ; make sure AX <= BX
        jbe  .ordered
        xchg ax, bx
.ordered:
        push ax
        mov  si, cx
        shl  si, 1
        mov  di, [rowtab + si]  ; y × 320
        pop  ax
        add  di, ax             ; + x1

        mov  cx, bx
        sub  cx, ax
        inc  cx                 ; CX = the pixel count

        mov  al, dl             ; STOSB stores AL
        cld
        rep  stosb

        pop  di
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; vline — vertical line from (AX, BX) to (AX, CX) in colour DL
;   Strides by 320 bytes per row, so no REP is possible.
;   Destroys: nothing
; ---------------------------------------------------------------
vline:
        push ax
        push bx
        push cx
        push di

        cmp  bx, cx
        jbe  .ordered
        xchg bx, cx
.ordered:
        push ax
        mov  si, bx
        shl  si, 1
        mov  di, [rowtab + si]
        pop  ax
        add  di, ax

        mov  ax, cx
        sub  ax, bx
        inc  ax
        mov  cx, ax             ; CX = the pixel count
.next:
        mov  [es:di], dl
        add  di, 320            ; down one row
        loop .next

        pop  di
        pop  cx
        pop  bx
        pop  ax
        ret

; ... build_rowtab and rowtab from Program 45.1 ...
