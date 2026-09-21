; replace.asm — replace every occurrence of one substring with another
;   of the SAME length (which keeps it simple and in place)
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print bmsg
        mov  si, text
        call puts
        newline

        mov  si, text
        call replace_all

        print amsg
        mov  si, text
        call puts
        newline
        print cmsg
        mov  ax, cx
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; replace_all — replace every occurrence of the ASCIIZ pattern at
;   [pat_ptr] with the equal-length ASCIIZ string at [rep_ptr],
;   inside the ASCIIZ string at DS:SI.
;
;   Out:       CX = the number of replacements
;   Destroys:  AX, BX, DX, SI, DI
;
;   The three parameters live in memory variables rather than
;   registers. That is deliberate: the 8086 does not have enough
;   registers for this routine, and named memory variables are far
;   clearer than stack juggling. See Exercise 42.14 for the stack
;   frame version.
; ---------------------------------------------------------------
replace_all:
        xor  cx, cx             ; replacement count

        ; --- measure the pattern, once ---
        mov  di, [pat_ptr]
        xor  dx, dx
.measure:
        cmp  byte [di], 0
        je   .measured
        inc  di
        inc  dx
        jmp  .measure
.measured:
        mov  [pat_len], dx
        or   dx, dx
        jz   .out               ; an empty pattern would loop for ever

.scan:
        cmp  byte [si], 0
        je   .out               ; end of the text

        ; --- does the pattern match at SI? ---
        mov  di, [pat_ptr]
        mov  bx, si             ; BX walks the text during the comparison,
        mov  dx, [pat_len]      ;   leaving SI pointing at the candidate
.cmp:
        mov  al, [bx]
        cmp  al, [di]
        jne  .no_match
        inc  bx
        inc  di
        dec  dx
        jnz  .cmp

        ; --- matched: overwrite in place with the replacement ---
        mov  di, [rep_ptr]
        mov  bx, si
        mov  dx, [pat_len]
.copy:
        mov  al, [di]
        mov  [bx], al
        inc  di
        inc  bx
        dec  dx
        jnz  .copy

        add  si, [pat_len]      ; skip past what we just replaced
        inc  cx
        jmp  .scan

.no_match:
        inc  si
        jmp  .scan

.out:
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

text:     db   'the cat sat on the mat, the cat was fat', 0
find:     db   'cat', 0
repl:     db   'dog', 0
pat_ptr:  dw   find
rep_ptr:  dw   repl
pat_len:  dw   0
bmsg:     db   'Before: $'
amsg:     db   'After:  $'
cmsg:     db   'Replacements: $'
