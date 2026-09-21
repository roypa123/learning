; ticker.asm — count timer ticks for five seconds, then report
; nasm -f bin ticker.asm -o ticker.com
        org  0x100

start:
        ; --- save the old INT 1Ch vector ---
        mov  ah, 0x35
        mov  al, 0x1C
        int  0x21               ; ES:BX = old handler
        mov  [old_off], bx
        mov  [old_seg], es

        ; --- install ours ---
        push ds
        mov  ah, 0x25
        mov  al, 0x1C
        mov  dx, tick_handler
        push cs
        pop  ds                 ; DS = CS, so DS:DX is our handler
        int  0x21
        pop  ds

        ; --- wait until 91 ticks have accumulated (about 5 seconds) ---
.wait:
        mov  ax, [ticks]        ; the handler updates this
        cmp  ax, 91             ; 18.2 ticks/s × 5 s
        jb   .wait

        ; --- restore the old vector BEFORE doing anything else ---
        push ds
        mov  dx, [old_off]      ; read both while DS still addresses our data
        mov  ax, [old_seg]
        mov  ds, ax             ; DS:DX = the original handler
        mov  ax, 0x251C         ; AH = 25h (set vector), AL = 1Ch
        int  0x21               ;   — set AX AFTER loading DS, or it gets clobbered
        pop  ds

        ; --- report ---
        mov  dx, donemsg
        mov  ah, 0x09
        int  0x21
        mov  ax, [ticks]
        call print_dec

        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; tick_handler — called 18.2 times a second by the BIOS.
;   Must be short, must preserve everything, must IRET.
;   No EOI needed: INT 1Ch is called by the BIOS's own INT 08h
;   handler, which sends the EOI itself.
; ---------------------------------------------------------------
tick_handler:
        push ax
        push ds

        push cs
        pop  ds                 ; our data segment
        inc  word [ticks]

        pop  ds
        pop  ax
        iret

; ---------------------------------------------------------------
print_dec:
        push ax
        push bx
        push cx
        push dx
        mov  bx, 10
        xor  cx, cx
.divide:
        xor  dx, dx
        div  bx
        push dx
        inc  cx
        or   ax, ax
        jnz  .divide
.output:
        pop  dx
        add  dl, '0'
        mov  ah, 0x02
        int  0x21
        loop .output
        mov  dl, 0x0D
        mov  ah, 0x02
        int  0x21
        mov  dl, 0x0A
        mov  ah, 0x02
        int  0x21
        pop  dx
        pop  cx
        pop  bx
        pop  ax
        ret

; ---------------------------------------------------------------
ticks:    dw   0
old_off:  dw   0
old_seg:  dw   0
donemsg:  db   'Ticks counted: $'
