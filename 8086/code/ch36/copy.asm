; copy.asm — copy one file to another
; nasm -f bin copy.asm -o copy.com
;
; Usage:  copy.com SOURCE.TXT DEST.TXT
;
;   This uses a 4 KiB buffer and loops until 3Fh returns zero bytes.
        cpu  8086
        org  0x100

start:
        call parse_args
        jc   .usage

        ; --- open the source ---
        mov  dx, src_name
        mov  ax, 0x3D00         ; AH = 3Dh, AL = 0 (read)
        int  0x21
        jc   .no_source
        mov  [src_handle], ax

        ; --- create the destination ---
        mov  dx, dst_name
        xor  cx, cx             ; normal attributes
        mov  ah, 0x3C
        int  0x21
        jc   .no_dest
        mov  [dst_handle], ax

        ; --- the copy loop ---
.copy:
        mov  bx, [src_handle]
        mov  cx, BUFSIZE
        mov  dx, buffer
        mov  ah, 0x3F           ; read
        int  0x21
        jc   .read_error
        or   ax, ax
        jz   .done              ; 0 bytes = end of file

        mov  cx, ax             ; write exactly what we read
        mov  bx, [dst_handle]
        mov  dx, buffer
        mov  ah, 0x40           ; write
        int  0x21
        jc   .write_error
        cmp  ax, cx
        jne  .disk_full         ; wrote fewer bytes than asked

        jmp  .copy

.done:
        mov  bx, [src_handle]
        mov  ah, 0x3E
        int  0x21
        mov  bx, [dst_handle]
        mov  ah, 0x3E
        int  0x21

        mov  dx, okmsg
        jmp  .exit

.usage:       mov dx, usagemsg  
              jmp .exit
.no_source:   mov dx, nosrcmsg  
              jmp .exit
.no_dest:     mov dx, nodstmsg  
              jmp .exit
.read_error:  mov dx, readmsg   
              jmp .exit
.write_error: mov dx, writemsg  
              jmp .exit
.disk_full:   mov dx, fullmsg

.exit:
        mov  ah, 0x09
        int  0x21
        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; parse_args — split the command tail into two ASCIIZ filenames.
;
;   Out:       CF = 0 on success, CF = 1 if fewer than two names
;   Destroys:  AX, CX, SI, DI
; ---------------------------------------------------------------
parse_args:
        mov  si, 0x81           ; the command tail
        mov  cl, [0x80]
        xor  ch, ch
        jcxz .fail

        mov  di, src_name
        call skip_spaces
        jcxz .fail
        call copy_word          ; copy until a space
        cmp  di, src_name       ; did we copy anything?
        je   .fail

        mov  di, dst_name
        call skip_spaces
        jcxz .fail
        call copy_word
        cmp  di, dst_name
        je   .fail

        clc
        ret
.fail:
        stc
        ret

; skip_spaces — advance SI past spaces; CX = remaining count
skip_spaces:
        jcxz .out
.next:
        cmp  byte [si], ' '
        jne  .out
        inc  si
        loop .next
.out:
        ret

; copy_word — copy non-space characters from SI to DI, then append 0
copy_word:
        jcxz .out
.next:
        mov  al, [si]
        cmp  al, ' '
        je   .out
        cmp  al, 0x0D
        je   .out
        mov  [di], al
        inc  si
        inc  di
        loop .next
.out:
        mov  byte [di], 0       ; ASCIIZ terminator
        ret

; ---------------------------------------------------------------
BUFSIZE     equ  4096

src_handle: dw   0
dst_handle: dw   0
src_name:   times 80 db 0
dst_name:   times 80 db 0

okmsg:      db   'Copied.', 0x0D, 0x0A, '$'
usagemsg:   db   'Usage: copy SOURCE DEST', 0x0D, 0x0A, '$'
nosrcmsg:   db   'Cannot open the source file.', 0x0D, 0x0A, '$'
nodstmsg:   db   'Cannot create the destination file.', 0x0D, 0x0A, '$'
readmsg:    db   'Read error.', 0x0D, 0x0A, '$'
writemsg:   db   'Write error.', 0x0D, 0x0A, '$'
fullmsg:    db   'Disk full.', 0x0D, 0x0A, '$'

buffer:     times BUFSIZE db 0
