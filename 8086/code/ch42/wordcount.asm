; wordcount.asm — count words, characters and lines
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, text
        xor  bx, bx             ; BX = word count
        xor  cx, cx             ; CX = character count
        xor  dx, dx             ; DX = line count
        mov  bp, 0              ; BP = "we are inside a word" flag
        cld

.next:
        lodsb
        or   al, al
        jz   .done
        inc  cx                 ; every byte counts as a character

        cmp  al, 0x0A
        jne  .not_newline
        inc  dx
.not_newline:

        ; --- is this a separator? ---
        cmp  al, ' '
        je   .separator
        cmp  al, 0x09           ; tab
        je   .separator
        cmp  al, 0x0D
        je   .separator
        cmp  al, 0x0A
        je   .separator

        ; --- a word character ---
        or   bp, bp
        jnz  .next              ; already inside a word
        mov  bp, 1              ; a word starts here
        inc  bx
        jmp  .next

.separator:
        xor  bp, bp             ; we are now between words
        jmp  .next

.done:
        print wmsg
        mov  ax, bx
        call print_udec
        newline
        print cmsg
        mov  ax, cx
        call print_udec
        newline
        print lmsg
        mov  ax, dx
        call print_udec
        newline
        exit 0

text:  db   'The quick brown fox', 0x0D, 0x0A
       db   'jumps over the lazy dog.', 0x0D, 0x0A
       db   'Pack my box with five dozen liquor jugs.', 0x0D, 0x0A, 0
wmsg:  db   'Words:      $'
cmsg:  db   'Characters: $'
lmsg:  db   'Lines:      $'
