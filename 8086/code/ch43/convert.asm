; convert.asm — read a number and show it in every base
; nasm -f bin convert.asm -o convert.com
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
.again:
        print prompt
        mov  si, inbuf
        call read_line
        cmp  byte [inbuf], 0
        je   .quit

        mov  si, inbuf
        call dec_to_bin
        jc   .bad
        mov  [value], ax

        ; --- decimal ---
        print dmsg
        mov  ax, [value]
        call print_udec
        newline

        ; --- hexadecimal ---
        print hmsg
        mov  ax, [value]
        mov  di, outbuf
        push ds
        pop  es
        call bin_to_hex
        mov  si, outbuf
        call puts
        newline

        ; --- binary ---
        print bmsg
        mov  ax, [value]
        mov  di, outbuf
        call bin_to_bits
        mov  si, outbuf
        call puts
        newline

        ; --- packed BCD ---
        print cmsg
        mov  ax, [value]
        call bin_to_bcd
        jc   .bcd_too_big
        mov  di, outbuf
        call bin_to_hex
        mov  si, outbuf
        call puts
        newline
        jmp  .again
.bcd_too_big:
        print bigmsg
        jmp  .again

.bad:
        print badmsg
        jmp  .again
.quit:
        exit 0

; ---------------------------------------------------------------
; read_line — read a line into the ASCIIZ buffer at DS:SI
; ---------------------------------------------------------------
read_line:
        push si
.next:
        mov  ah, 0x01
        int  0x21
        cmp  al, 0x0D
        je   .done
        mov  [si], al
        inc  si
        jmp  .next
.done:
        mov  byte [si], 0
        newline
        pop  si
        ret

puts:
        cld
.next:  lodsb
        or   al, al
        jz   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:  ret

; ... bin_to_hex, bin_to_bits, bin_to_bcd, dec_to_bin from above ...

value:   dw   0
inbuf:   times 16 db 0
outbuf:  times 20 db 0
prompt:  db   0x0D, 0x0A, 'Number (Enter to quit): $'
dmsg:    db   'Decimal: $'
hmsg:    db   'Hex:     0x$'
bmsg:    db   'Binary:  $'
cmsg:    db   'BCD:     0x$'
badmsg:  db   'Not a valid number.', 0x0D, 0x0A, '$'
bigmsg:  db   '(exceeds 9999)', 0x0D, 0x0A, '$'
