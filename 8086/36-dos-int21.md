# Chapter 36 — DOS services (`INT 21h`)

[← `.COM` versus `.EXE`](35-com-vs-exe.md) · [Contents](README.md) · [Next: BIOS services →](37-bios-services.md)

---

## Goal

The DOS function interface: how to call it, the conventions it follows, and the forty or so
functions you will actually use — console I/O, file I/O, directories, memory and time — each with a
runnable example.

A complete function list is in [Appendix C](C-interrupt-reference.md). This chapter explains the
ones worth knowing by heart.

---

## 1. The calling convention

```asm
        mov  ah, <function number>
        ; ... set up other registers as the function requires ...
        int  0x21
        ; ... results are in registers ...
```

**`AH` selects the function.** Always. Some functions also use `AL` as a sub-function selector.

### 1.1 What DOS preserves

**Only the segment registers and `SP` are guaranteed.** Any general-purpose register may be
destroyed by any function. In practice most functions preserve most registers, but relying on that
is how you get bugs that appear only on a different DOS version.

**The safe assumption: after `INT 21h`, `AX`, `BX`, `CX`, `DX`, `SI`, `DI` and `BP` are all
undefined** unless the function documents otherwise. Push what you need:

```asm
        push cx
        push si
        mov  ah, 0x09
        int  0x21
        pop  si
        pop  cx
```

### 1.2 The cost

`INT 21h` is 51 clocks before DOS's handler starts, plus the handler itself. A single character
output is on the order of 200–1000 clocks depending on DOS version and redirection.

**Do not call DOS per character in a loop that matters.** Chapter 45 writes to the video buffer
directly and is about fifty times faster.

---

## 2. Error reporting

Most DOS functions that can fail use the carry-flag convention (Chapter 30 §2.2):

```asm
        int  0x21
        jc   .error             ; CF = 1 means failure; AX holds the error code
```

The common error codes in `AX`:

| Code | Meaning |
|------|---------|
| 1 | invalid function number |
| 2 | file not found |
| 3 | path not found |
| 4 | too many open files |
| 5 | access denied |
| 6 | invalid handle |
| 8 | insufficient memory |
| 15 | invalid drive |

---

## 3. Console output

### 3.1 Function 02h — write one character

```
   In:   AH = 02h,  DL = the character
   Out:  AL = the character written
```

```asm
        mov  dl, 'A'
        mov  ah, 0x02
        int  0x21
```

**Destroys `AL`.** In a loop that uses `AL`, save it.

Interprets control characters: `0x0D` returns the cursor, `0x0A` advances a line, `0x08` backspaces,
`0x07` beeps, `0x09` tabs.

**Output can be redirected** (`prog > file.txt`), because it writes to standard output.

### 3.2 Function 09h — write a `$`-terminated string

```
   In:   AH = 09h,  DS:DX = the string
```

```asm
        mov  dx, msg
        mov  ah, 0x09
        int  0x21
msg:    db   'Hello$'
```

**The `$` is not printed.** It is a terminator inherited from CP/M, and it means you cannot print a
literal `$` with this function — use 02h for that.

Far faster than calling 02h per character, because DOS loops internally.

### 3.3 Function 40h — write to a handle

The modern interface. Writes an arbitrary number of bytes, including zeros and `$`.

```
   In:   AH = 40h,  BX = handle,  CX = byte count,  DS:DX = buffer
   Out:  AX = bytes actually written;  CF = 1 on error
```

Standard handles:

| Handle | Stream |
|--------|--------|
| 0 | standard input |
| 1 | standard output |
| 2 | standard error |
| 3 | standard auxiliary (COM1) |
| 4 | standard printer |

```asm
        mov  bx, 1              ; stdout
        mov  cx, msglen
        mov  dx, msg
        mov  ah, 0x40
        int  0x21

msg:    db   'Hello, world!', 0x0D, 0x0A
msglen  equ  $ - msg
```

**Use handle 2 for error messages** — it is not redirected by `>`, so errors still reach the screen
when output goes to a file.

---

## 4. Console input

### 4.1 Function 01h — read a character, with echo

```
   In:   AH = 01h
   Out:  AL = the character.  WAITS for a keypress.
```

```asm
        mov  ah, 0x01
        int  0x21               ; AL = the key, and it appears on screen
```

**Extended keys return two values.** For a function key or arrow, `AL` is `0x00` and you must call
again to get the scan code:

```asm
        mov  ah, 0x01
        int  0x21
        or   al, al
        jnz  .normal            ; a normal character
        mov  ah, 0x01
        int  0x21               ; AL = the extended scan code
        ; F1 = 0x3B, F2 = 0x3C, Up = 0x48, Down = 0x50, Left = 0x4B, Right = 0x4D
.normal:
```

### 4.2 Function 08h — read a character, no echo

Identical to 01h but the character is not displayed. Use it for passwords and for menu keys you want
to handle yourself.

### 4.3 Function 0Bh — check for a keypress without waiting

```
   In:   AH = 0Bh
   Out:  AL = 0xFF if a key is waiting, 0x00 if not.  Does NOT read it.
```

The polling idiom:

```asm
.loop:
        ; ... do work ...
        mov  ah, 0x0B
        int  0x21
        or   al, al
        jz   .loop              ; no key — keep working

        mov  ah, 0x08           ; now read it
        int  0x21
```

### 4.4 Function 0Ah — read a buffered line

```
   In:   AH = 0Ah,  DS:DX = a buffer
   Buffer layout:
        [0] = maximum characters to accept (you set this)
        [1] = characters actually read     (DOS sets this)
        [2..] = the text, terminated by 0x0D
```

```asm
; Read a line of up to 80 characters.
        mov  dx, inbuf
        mov  ah, 0x0A
        int  0x21

        ; the text is at inbuf+2, and inbuf[1] is its length
        mov  cl, [inbuf+1]
        xor  ch, ch             ; CX = length

inbuf:  db   81                 ; maximum (including the CR)
        db   0                  ; DOS fills this in
        times 81 db 0           ; the text
```

DOS provides full line editing — backspace, Ctrl-C, insert — for free. This is the right way to read
a line of input.

**The buffer must be one byte larger than the maximum**, because DOS appends the `0x0D`.

---

## 5. File handling — the handle interface

DOS 2.0 introduced file *handles*, which replaced the CP/M-derived FCB interface. Use handles.

### 5.1 Function 3Ch — create a file

```
   In:   AH = 3Ch,  CX = attributes,  DS:DX = an ASCIIZ pathname
   Out:  AX = the handle;  CF = 1 on error
```

Attributes: `0x00` normal, `0x01` read-only, `0x02` hidden, `0x04` system, `0x20` archive.

```asm
        mov  dx, filename
        xor  cx, cx             ; normal attributes
        mov  ah, 0x3C
        int  0x21
        jc   .error
        mov  [handle], ax

filename: db 'OUTPUT.TXT', 0    ; ASCIIZ — zero-terminated, not $-terminated
handle:   dw 0
```

**Note the zero terminator.** File functions use ASCIIZ strings, not `$`-terminated ones. Mixing them
up is a standard mistake.

**3Ch truncates an existing file to zero length.** To open without truncating, use 3Dh.

### 5.2 Function 3Dh — open an existing file

```
   In:   AH = 3Dh,  AL = mode,  DS:DX = ASCIIZ pathname
   Out:  AX = handle;  CF = 1 on error
```

Modes: `0` read, `1` write, `2` read/write.

```asm
        mov  dx, filename
        mov  al, 0              ; read only
        mov  ah, 0x3D
        int  0x21
        jc   .not_found
        mov  [handle], ax
```

### 5.3 Function 3Fh — read

```
   In:   AH = 3Fh,  BX = handle,  CX = bytes to read,  DS:DX = buffer
   Out:  AX = bytes actually read (0 = end of file);  CF = 1 on error
```

```asm
        mov  bx, [handle]
        mov  cx, 512
        mov  dx, buffer
        mov  ah, 0x3F
        int  0x21
        jc   .error
        ; AX = how many bytes we got. Less than CX means end of file.
```

**`AX = 0` means end of file.** That is how you detect it — not an error code.

### 5.4 Function 40h — write

Already seen in §3.3; the same function writes to a file handle.

```asm
        mov  bx, [handle]
        mov  cx, count
        mov  dx, buffer
        mov  ah, 0x40
        int  0x21
```

**`AX < CX` with no carry means the disk is full.** Check it.

### 5.5 Function 3Eh — close

```
   In:   AH = 3Eh,  BX = handle
```

```asm
        mov  bx, [handle]
        mov  ah, 0x3E
        int  0x21
```

**Always close.** DOS flushes its buffers on close; a program that exits without closing may lose
the last partial sector.

### 5.6 Function 42h — seek

```
   In:   AH = 42h,  AL = origin,  BX = handle,  CX:DX = signed offset
   Out:  DX:AX = the new position;  CF = 1 on error
```

Origins: `0` from the start, `1` from the current position, `2` from the end.

```asm
; Find the file's size by seeking to the end.
        mov  bx, [handle]
        xor  cx, cx
        xor  dx, dx
        mov  ax, 0x4202         ; AH = 42h, AL = 2 (from the end)
        int  0x21
        ; DX:AX = the size
```

### 5.7 Function 41h — delete

```
   In:   AH = 41h,  DS:DX = ASCIIZ pathname
```

### 5.8 Function 56h — rename

```
   In:   AH = 56h,  DS:DX = old name,  ES:DI = new name (both ASCIIZ)
```

---

## 6. Directories

| Function | Action | Input |
|----------|--------|-------|
| `39h` | create directory | `DS:DX` = ASCIIZ path |
| `3Ah` | remove directory | `DS:DX` = ASCIIZ path |
| `3Bh` | change directory | `DS:DX` = ASCIIZ path |
| `47h` | get current directory | `DL` = drive (0 = current), `DS:SI` = 64-byte buffer |
| `0Eh` | select drive | `DL` = drive (0 = A:) |
| `19h` | get current drive | returns `AL` (0 = A:) |

```asm
; Print the current directory.
        mov  ah, 0x19           ; get current drive
        int  0x21
        add  al, 'A'
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        mov  dl, ':'
        mov  ah, 0x02
        int  0x21
        mov  dl, '\'
        mov  ah, 0x02
        int  0x21

        xor  dl, dl             ; current drive
        mov  si, pathbuf
        mov  ah, 0x47
        int  0x21
        ; pathbuf now holds an ASCIIZ path, WITHOUT the drive or leading backslash
```

---

## 7. Finding files

### 7.1 Functions 4Eh and 4Fh — find first / find next

```
   4Eh:  AH = 4Eh,  CX = attribute mask,  DS:DX = ASCIIZ wildcard pattern
   4Fh:  AH = 4Fh   (continues the previous search)
   Out:  CF = 1 when there are no more matches
```

Results go into the **Disk Transfer Area**, a 43-byte block whose address you set with function 1Ah.
Its layout:

| Offset | Size | Contents |
|--------|------|----------|
| `0x00` | 21 | reserved (DOS's search state) |
| `0x15` | 1 | file attribute |
| `0x16` | 2 | time |
| `0x18` | 2 | date |
| `0x1A` | 4 | file size |
| `0x1E` | 13 | **ASCIIZ filename** |

```asm
; List every .TXT file in the current directory.
        mov  dx, dta            ; set the DTA
        mov  ah, 0x1A
        int  0x21

        mov  dx, pattern
        mov  cx, 0              ; normal files only
        mov  ah, 0x4E           ; find first
        int  0x21
        jc   .done

.next:
        mov  dx, dta + 0x1E     ; the filename
        call print_asciiz
        mov  ah, 0x4F           ; find next
        int  0x21
        jnc  .next
.done:

pattern: db  '*.TXT', 0
dta:     times 43 db 0
```

---

## 8. Memory

| Function | Action | In | Out |
|----------|--------|----|----|
| `48h` | allocate | `BX` = paragraphs wanted | `AX` = segment; on failure `BX` = largest available |
| `49h` | free | `ES` = segment to free | |
| `4Ah` | resize | `ES` = segment, `BX` = new size in paragraphs | |

```asm
; Ask for 4 KiB (256 paragraphs).
        mov  bx, 256
        mov  ah, 0x48
        int  0x21
        jc   .no_memory
        mov  [block], ax        ; the segment of the new block
```

**A `.COM` program owns all of memory**, so `48h` always fails until you shrink your own block:

```asm
; Keep 8 KiB for ourselves, release the rest.
        mov  bx, 512            ; 512 paragraphs = 8 KiB
        mov  ah, 0x4A
        int  0x21
```

This matters for TSRs and for programs that load a child process.

---

## 9. Date, time and miscellaneous

| Function | Action | Returns |
|----------|--------|---------|
| `2Ah` | get date | `CX` = year, `DH` = month, `DL` = day, `AL` = weekday (0 = Sunday) |
| `2Bh` | set date | |
| `2Ch` | get time | `CH` = hour, `CL` = minute, `DH` = second, `DL` = hundredths |
| `2Dh` | set time | |
| `30h` | get DOS version | `AL` = major, `AH` = minor |
| `25h` | set interrupt vector | `AL` = number, `DS:DX` = handler |
| `35h` | get interrupt vector | `AL` = number; returns `ES:BX` |
| `4Ch` | terminate | `AL` = exit code |
| `31h` | terminate and stay resident | `AL` = exit code, `DX` = paragraphs to keep |
| `4Bh` | load and execute a program | |

```asm
; ---------------------------------------------------------------
; Print the time as HH:MM:SS.
;
; Note the PUSHes. Function 2Ch returns the time in CX and DX, and
; the very next thing we do is call function 02h, which may destroy
; both. §1.1 is not a theoretical warning — this is where it bites.
; ---------------------------------------------------------------
show_time:
        mov  ah, 0x2C
        int  0x21               ; CH = hour, CL = min, DH = sec
        push dx                 ; save seconds BEFORE any other DOS call
        push cx                 ; save hours and minutes

        mov  al, ch             ; hours
        call print_two
        mov  dl, ':'
        mov  ah, 0x02
        int  0x21

        pop  cx                 ; recover hours/minutes
        push cx
        mov  al, cl             ; minutes
        call print_two
        mov  dl, ':'
        mov  ah, 0x02
        int  0x21

        pop  cx                 ; discard
        pop  dx                 ; recover seconds
        mov  al, dh
        call print_two
        ret

; print_two — print AL (0-59) as exactly two decimal digits
print_two:
        push ax
        xor  ah, ah
        mov  bl, 10
        div  bl                 ; AL = tens, AH = units
        push ax
        mov  dl, al
        add  dl, '0'
        mov  ah, 0x02
        int  0x21
        pop  ax
        mov  dl, ah
        add  dl, '0'
        mov  ah, 0x02
        int  0x21
        pop  ax
        ret
```

---

## 10. Worked program — a file copier

```asm
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
```

### 10.1 Notes

**`mov ax, 0x3D00`** sets `AH = 0x3D` and `AL = 0x00` in one three-byte instruction instead of two
two-byte ones. Use this whenever both halves are constants.

**`or ax, ax` / `jz`** after the read detects end of file. `AX = 0` is not an error; there is no
carry.

**`cmp ax, cx` / `jne`** after the write detects a full disk. DOS reports this by writing fewer
bytes than asked *without* setting carry, which is easy to miss.

**The buffer is 4 KiB** and lives at the end of the file, so the `.COM` file is about 4 KiB larger
than it needs to be. A production version would use `resb` in a `.bss` section, or shrink the memory
block and use the space above it.

**Every exit path goes through `.exit`**, which prints a message and terminates. That structure —
one exit point, error messages selected by `DX` — keeps the error handling from swamping the logic.

---

## 11. The functions worth memorising

```
   02h  write character      DL = char
   09h  write $-string       DS:DX
   0Ah  read buffered line   DS:DX = buffer with max in [0]
   0Bh  check for keypress   AL = FF if waiting
   01h  read char with echo  AL = char
   08h  read char no echo    AL = char

   3Ch  create file          DS:DX = ASCIIZ, CX = attrs  -> AX = handle
   3Dh  open file            AL = mode                   -> AX = handle
   3Eh  close                BX = handle
   3Fh  read                 BX, CX, DS:DX  -> AX = bytes read (0 = EOF)
   40h  write                BX, CX, DS:DX  -> AX = bytes written
   41h  delete               DS:DX = ASCIIZ
   42h  seek                 AL = origin, BX, CX:DX -> DX:AX = position

   25h  set interrupt vector AL = number, DS:DX = handler
   35h  get interrupt vector AL = number -> ES:BX
   2Ch  get time             CH:CL = h:m, DH:DL = s:hundredths
   4Ch  terminate            AL = exit code
```

---

## 12. Summary

```
  AH = function number, then INT 21h
  only the segment registers and SP are guaranteed preserved
  errors: CF = 1 and an error code in AX

  strings for DISPLAY are $-terminated (function 09h)
  strings for FILES   are ASCIIZ, zero-terminated
  mixing them up is a standard mistake

  handles: 0 stdin · 1 stdout · 2 stderr · 3 aux · 4 printer
  use handle 2 for errors so they survive redirection

  3Fh returning AX = 0 means END OF FILE, not an error
  40h returning AX < CX with no carry means DISK FULL

  INT 21h costs 51 clocks before DOS even starts.
  Never call it per character in a loop that matters.
```

---

## Exercises

**36.1** Which register selects the DOS function? Which registers does DOS guarantee to preserve?

**36.2** Write the three instructions that print the character in `BL`. Why can you not simply
`mov ah, 2` / `int 21h`?

**36.3** Print a literal `$` character. Why can function 09h not do it?

**36.4** Write a loop that reads characters with function 08h until Enter (`0x0D`) is pressed,
echoing each one with function 02h.

**36.5** Set up the buffer for function 0Ah to read a line of up to 40 characters, and write the code
that reads it and reports the length.

**36.6** Write the code that opens `DATA.TXT` for reading, or prints an error and exits if it does
not exist.

**36.7** After `INT 21h` function 3Fh, `AX = 0` and `CF = 0`. What does that mean?

**36.8** After function 40h, `AX = 100` but `CX` was 512, and `CF = 0`. What happened?

**36.9** What is the difference between a `$`-terminated string and an ASCIIZ string, and which
functions take which?

**36.10** Write the code that reads the current time and prints it as `HH:MM:SS`, being careful about
registers DOS may destroy.

**36.11** Why does a `.COM` program have to call function 4Ah before function 48h can succeed?

**36.12** Write the code that lists every `*.ASM` file in the current directory.

**36.13** Explain why the file copier in §10 checks both `CF` and `AX` after the write.

**36.14** Rewrite the "print a string" part of any earlier program to use function 40h instead of
09h. What are the two advantages?

Answers in [Appendix H](H-exercise-solutions.md#chapter-36).

---

[← `.COM` versus `.EXE`](35-com-vs-exe.md) · [Contents](README.md) · [Next: BIOS services →](37-bios-services.md)
