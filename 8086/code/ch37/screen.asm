; screen.asm — draw a bordered box with BIOS video calls
; nasm -f bin screen.asm -o screen.com
        cpu  8086
        org  0x100

TOP     equ  5
LEFT    equ  20
WIDTH   equ  40
HEIGHT  equ  10
ATTR    equ  0x1F              ; white on blue

start:
        ; --- clear the screen to blue ---
        mov  ax, 0x0600         ; scroll up 0 lines = clear the window
        mov  bh, ATTR
        xor  cx, cx             ; top-left 0,0
        mov  dx, 0x184F         ; bottom-right 24,79
        int  0x10

        ; --- draw the top edge ---
        mov  dh, TOP
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xC9           ; ╔
        call putchar_attr
        mov  cx, WIDTH - 2
        mov  al, 0xCD           ; ═
        call putrep
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBB           ; ╗
        call putchar_attr

        ; --- the sides ---
        mov  cx, HEIGHT - 2
        mov  dh, TOP + 1
.side:
        push cx
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xBA           ; ║
        call putchar_attr
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBA
        call putchar_attr
        inc  dh
        pop  cx
        loop .side

        ; --- the bottom edge ---
        mov  dh, TOP + HEIGHT - 1
        mov  dl, LEFT
        call gotoxy
        mov  al, 0xC8           ; ╚
        call putchar_attr
        mov  cx, WIDTH - 2
        mov  al, 0xCD
        call putrep
        mov  dl, LEFT + WIDTH - 1
        call gotoxy
        mov  al, 0xBC           ; ╝
        call putchar_attr

        ; --- the title ---
        mov  dh, TOP + 2
        mov  dl, LEFT + 10
        call gotoxy
        mov  si, title
        call puts

        ; --- wait for a key ---
        xor  ah, ah
        int  0x16

        ; --- restore the screen ---
        mov  ax, 0x0003         ; set mode 3 — clears to the default colours
        int  0x10

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; gotoxy — move the cursor.  In: DH = row, DL = column.
;          Preserves everything.
; ---------------------------------------------------------------
gotoxy:
        push ax
        push bx
        mov  ah, 0x02
        xor  bh, bh
        int  0x10
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; putchar_attr — write AL at the cursor with ATTR, without moving it.
; ---------------------------------------------------------------
putchar_attr:
        push ax
        push bx
        push cx
        mov  ah, 0x09
        xor  bh, bh
        mov  bl, ATTR
        mov  cx, 1
        int  0x10
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; putrep — write CX copies of AL at the cursor.
; ---------------------------------------------------------------
putrep:
        push ax
        push bx
        mov  ah, 0x09
        xor  bh, bh
        mov  bl, ATTR
        int  0x10
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; puts — write the zero-terminated string at DS:SI, advancing the
;        cursor.  Uses teletype output so newlines work.
; ---------------------------------------------------------------
puts:
        push ax
        push bx
.next:
        lodsb
        or   al, al
        jz   .done
        mov  ah, 0x0E
        xor  bh, bh
        int  0x10
        jmp  .next
.done:
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
title:  db   'BIOS video demo', 0
