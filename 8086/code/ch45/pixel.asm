; pixel.asm — plot some pixels
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        mov  ax, 0x0013
        int  0x10               ; enter mode 13h

        mov  ax, 0xA000
        mov  es, ax             ; ES -> the video buffer
        call build_rowtab

        ; --- plot a diagonal line of pixels ---
        xor  cx, cx             ; CX = the loop counter
.next:
        mov  ax, cx             ; x = cx
        mov  bx, cx             ; y = cx
        mov  dl, 15             ; colour: white
        call putpixel
        inc  cx
        cmp  cx, 200
        jb   .next

        ; --- wait for a key, then restore text mode ---
        xor  ah, ah
        int  0x16
        mov  ax, 0x0003
        int  0x10
        exit 0

; ---------------------------------------------------------------
; putpixel — plot a pixel.
;
;   In:        AX = x (0-319), BX = y (0-199), DL = colour
;              ES must already point at 0xA000
;   Destroys:  nothing
;
;   No bounds checking — see Exercise 45.1.
; ---------------------------------------------------------------
putpixel:
        push ax
        push bx
        push di

        shl  bx, 1              ; ×2 for the word table
        mov  di, [rowtab + bx]  ; y × 320
        add  di, ax             ; + x
        mov  [es:di], dl        ; the pixel IS this byte

        pop  di
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
; build_rowtab — fill rowtab[y] with y × 320
; ---------------------------------------------------------------
build_rowtab:
        push ax
        push cx
        push di
        mov  di, rowtab
        xor  ax, ax
        mov  cx, 200
.next:
        mov  [di], ax
        add  ax, 320
        inc  di
        inc  di
        loop .next
        pop  di
        pop  cx
        pop  ax
        ret

rowtab: times 200 dw 0
