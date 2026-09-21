; probe.asm — test whether ports 0x40 and 0x140 are the same device
; Uses the 8253 counter 0 latch, which is readable. Adapt to your hardware.
        org  0x100

        mov  dx, 0x43           ; 8253 control port
        mov  al, 0x36           ; counter 0, LSB then MSB, mode 3, binary
        out  dx, al

        mov  dx, 0x40           ; counter 0 data port
        mov  al, 0x34
        out  dx, al             ; LSB
        mov  al, 0x12
        out  dx, al             ; MSB -> counter 0 loaded with 0x1234

        mov  dx, 0x143          ; suspected alias of the control port
        mov  al, 0x00           ; latch counter 0
        out  dx, al

        mov  dx, 0x140          ; suspected alias of the data port
        in   al, dx
        mov  bl, al
        in   al, dx
        mov  bh, al             ; BX = the latched count

        ; If BX is close to 0x1234 (counting down), 0x140 IS 0x40.
        ...
