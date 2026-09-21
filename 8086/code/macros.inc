; macros.inc — common macros used by the programs in this book
;
; Every macro preserves the registers it touches, so they compose freely.
; Include with:   %include "macros.inc"
;
%ifndef MACROS_INC
%define MACROS_INC

; ---- program exit ----
%macro exit 0-1 0
        mov  ax, 0x4C00 + %1
        int  0x21
%endmacro

; ---- print a $-terminated string whose address is %1 ----
%macro print 1
        push ax
        push dx
        mov  dx, %1
        mov  ah, 0x09
        int  0x21
        pop  dx
        pop  ax
%endmacro

; ---- print one character ----
%macro putc 1
        push ax
        push dx
        mov  dl, %1
        mov  ah, 0x02
        int  0x21
        pop  dx
        pop  ax
%endmacro

; ---- carriage return + line feed ----
%macro newline 0
        putc 0x0D
        putc 0x0A
%endmacro

; ---- wait for any key ----
%macro waitkey 0
        push ax
        xor  ah, ah
        int  0x16
        pop  ax
%endmacro

; ---- set the cursor: gotoxy row, column ----
%macro gotoxy 2
        push ax
        push bx
        push dx
        mov  ah, 0x02
        xor  bh, bh
        mov  dh, %1
        mov  dl, %2
        int  0x10
        pop  dx
        pop  bx
        pop  ax
%endmacro

; ---- clear the screen; optional attribute, default light grey on black ----
%macro cls 0-1 0x07
        push ax
        push bx
        push cx
        push dx
        mov  ax, 0x0600
        mov  bh, %1
        xor  cx, cx
        mov  dx, 0x184F
        int  0x10
        pop  dx
        pop  cx
        pop  bx
        pop  ax
%endmacro

; ---- save / restore the six common registers ----
%macro save 0
        push ax
        push bx
        push cx
        push dx
        push si
        push di
%endmacro

%macro restore 0
        pop  di
        pop  si
        pop  dx
        pop  cx
        pop  bx
        pop  ax
%endmacro

%endif
