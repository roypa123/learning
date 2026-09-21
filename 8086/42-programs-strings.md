# Chapter 42 — Programs: strings

[← Sorting and searching](41-programs-sorting-searching.md) · [Contents](README.md) · [Next: Number conversion →](43-programs-number-conversion.md)

---

## Goal

Twelve string programs: length, copy, concatenate, compare, search, reverse, palindrome test, case
conversion, word count, token split, character frequency, and a substring replace.

All assume `macros.inc` and `io.inc` from Chapter 39 §0.

---

## 1. Two string conventions

```asm
dollar_str:  db  'Hello$'          ; $-terminated — for DOS function 09h
asciiz_str:  db  'Hello', 0        ; zero-terminated — for everything else
counted_str: db  5, 'Hello'        ; length-prefixed — for fixed-size records
```

**This chapter uses zero-terminated (ASCIIZ) strings**, because they are what file functions need
(Chapter 36 §5.1) and what C uses. Where a program prints with function 09h it converts, or uses a
routine that prints character by character.

### 1.1 The fundamental loop

```asm
        cld
        mov  si, str
.next:  lodsb                   ; AL = the next character, SI advances
        or   al, al             ; zero terminator?
        jz   .done
        ; ... process AL ...
        jmp  .next
.done:
```

`lodsb` + `or al, al` + `jz` is three instructions and 12 + 3 + 4 = 19 clocks per character. Every
program below is a variation on it.

---

## Program 42.1 — String length

```asm
; strlen.asm — length of a zero-terminated string
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        call strlen
        print msg
        mov  ax, cx
        call print_udec
        newline
        exit 0

; ---------------------------------------------------------------
; strlen — length of the ASCIIZ string at DS:SI
;   Out:       CX = the length
;   Destroys:  AL, SI
; ---------------------------------------------------------------
strlen:
        cld
        xor  cx, cx
.next:
        lodsb
        or   al, al
        jz   .done
        inc  cx
        jmp  .next
.done:
        ret

str:    db   'The quick brown fox', 0
msg:    db   'Length: $'
```

**Output:** `Length: 19`

### 42.1a — The `SCASB` version

```asm
; ---------------------------------------------------------------
; strlen_fast — the same, using REPNE SCASB.
;   In:        ES:DI = the string
;   Out:       CX = the length
; ---------------------------------------------------------------
strlen_fast:
        push di
        cld
        xor  al, al             ; scan for the zero byte
        mov  cx, 0xFFFF         ; scan at most 65535 bytes
        repne scasb
        not  cx                 ; CX = 0xFFFF - remaining = examined count
        dec  cx                 ; don't count the terminator
        pop  di
        ret
```

**How the arithmetic works.** `REPNE SCASB` decrements `CX` once per byte examined, including the
terminator. Starting from `0xFFFF`, after examining *k* bytes `CX = 0xFFFF − k`. `NOT CX` computes
`0xFFFF − CX = k`, and `DEC CX` removes the terminator, giving the length.

For a 19-character string: 20 bytes examined, `CX = 0xFFEB`, `NOT` gives 20, `DEC` gives 19. ✔

| Version | Clocks per character |
|---------|---------------------|
| `LODSB` loop | 12 + 3 + 4 + 2 + 15 = **36** |
| `REPNE SCASB` | **15** |

---

## Program 42.2 — String copy

```asm
; strcpy.asm — copy a string
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, source
        mov  di, dest
        call strcpy

        print msg
        mov  si, dest
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strcpy — copy the ASCIIZ string at DS:SI to ES:DI, terminator included
;   Destroys:  AL, SI, DI
; ---------------------------------------------------------------
strcpy:
        cld
.next:
        lodsb                   ; AL <- [DS:SI], SI++
        stosb                   ; [ES:DI] <- AL, DI++
        or   al, al             ; was that the terminator?
        jnz  .next              ;   no — keep going (and it HAS been copied)
        ret

; ---------------------------------------------------------------
; puts — print the ASCIIZ string at DS:SI
;   Destroys:  AL, DL, SI, AH
; ---------------------------------------------------------------
puts:
        cld
.next:
        lodsb
        or   al, al
        jz   .done
        mov  dl, al
        mov  ah, 0x02
        int  0x21
        jmp  .next
.done:
        ret

source: db   'Copy me exactly.', 0
dest:   times 64 db 0
msg:    db   'Copied: $'
```

**Output:** `Copied: Copy me exactly.`

### Explanation

**The test comes *after* the store.** `stosb` writes the byte, then `or al, al` checks whether it was
the terminator. That order means the zero is copied — which is what you want, and what a
test-before-store version would get wrong.

**Three instructions per character**, 12 + 11 + 3 + 16 = 42 clocks. There is no faster way for an
unknown-length string, because `REP MOVSB` needs the count in advance. If you know the length,
`REP MOVSW` is three times faster (Chapter 29 §3.3).

---

## Program 42.3 — Concatenate

```asm
; strcat.asm — append one string to another
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  di, buffer
        mov  si, part1
        call strcpy_di          ; buffer = part1

        mov  di, buffer
        mov  si, part2
        call strcat             ; buffer = buffer + part2

        mov  di, buffer
        mov  si, part3
        call strcat

        print msg
        mov  si, buffer
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strcat — append DS:SI to the ASCIIZ string at ES:DI
;   Destroys:  AL, SI, DI
; ---------------------------------------------------------------
strcat:
        cld
        ; --- first, find the end of the destination ---
        push ax
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb             ; DI ends up one PAST the terminator
        dec  di                 ; back onto the terminator
        pop  ax
        ; --- now copy, overwriting that terminator ---
strcpy_di:
        cld
.next:
        lodsb
        stosb
        or   al, al
        jnz  .next
        ret

buffer: times 128 db 0
part1:  db   'Hello, ', 0
part2:  db   'cruel ', 0
part3:  db   'world!', 0
msg:    db   'Result: $'
```

**Output:** `Result: Hello, cruel world!`

### Explanation

**`dec di` after the scan.** `REPNE SCASB` stops with `DI` pointing one byte past the match, so
without the decrement the second string would start after the terminator and the result would look
truncated (Chapter 29 §3.1).

**`strcat` falls through into `strcpy_di`.** That is deliberate: once `DI` points at the terminator,
appending *is* copying. Sharing the tail saves code and makes the relationship obvious.

---

## Program 42.4 — Compare

```asm
; strcmp.asm — compare two strings
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, s1
        mov  di, s2
        call strcmp
        call report

        mov  si, s1
        mov  di, s3
        call strcmp
        call report

        mov  si, s1
        mov  di, s1
        call strcmp
        call report
        exit 0

; ---------------------------------------------------------------
; strcmp — compare the ASCIIZ strings at DS:SI and ES:DI
;   Out:       AX = -1 if SI < DI, 0 if equal, +1 if SI > DI
;   Destroys:  SI, DI, BL, flags
; ---------------------------------------------------------------
strcmp:
        cld
.next:
        mov  bl, [si]           ; keep the source byte; CMPSB advances SI
        cmpsb                   ; compare [SI] with [DI], both advance
        jne  .differ
        or   bl, bl             ; both were the terminator?
        jnz  .next              ;   no — continue
        xor  ax, ax             ; equal
        ret
.differ:
        jb   .less              ; UNSIGNED: characters are unsigned
        mov  ax, 1
        ret
.less:
        mov  ax, -1
        ret

report:
        or   ax, ax
        js   .less
        jz   .equal
        print gtmsg
        ret
.equal: print eqmsg
        ret
.less:  print ltmsg
        ret

s1:     db   'apple', 0
s2:     db   'banana', 0
s3:     db   'apple pie', 0
ltmsg:  db   'first < second', 0x0D, 0x0A, '$'
eqmsg:  db   'first = second', 0x0D, 0x0A, '$'
gtmsg:  db   'first > second', 0x0D, 0x0A, '$'
```

**Output:**

```
first < second
first < second
first = second
```

`'apple'` < `'banana'` because `'a'` < `'b'`. `'apple'` < `'apple pie'` because the shorter string's
terminator (0) is less than `' '` (0x20).

### Explanation

**`mov bl, [si]` before `CMPSB`** captures the source character, because `CMPSB` advances `SI` and we
need to know afterwards whether we just compared two terminators.

**`jb` not `jl`.** Characters are unsigned. With `jl`, any character above `0x7F` — the accented
letters and box-drawing characters of code page 437 — would compare as negative.

---

## Program 42.5 — Find a character

```asm
; strchr.asm — find the first occurrence of a character
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        mov  al, 'o'
        call strchr
        jc   .nf
        print foundmsg
        mov  ax, si
        sub  ax, str            ; convert the address to an index
        call print_udec
        newline
        exit 0
.nf:
        print nfmsg
        exit 1

; ---------------------------------------------------------------
; strchr — find AL in the ASCIIZ string at DS:SI
;   Out:       CF = 0 and SI -> the match, or CF = 1
;   Destroys:  AH, SI
; ---------------------------------------------------------------
strchr:
        cld
        mov  ah, al             ; keep the target in AH
.next:
        lodsb                   ; AL = the current character, SI advances
        cmp  al, ah
        je   .found
        or   al, al
        jnz  .next
        stc                     ; hit the terminator without a match
        ret
.found:
        dec  si                 ; LODSB advanced past it — back up
        clc
        ret

str:      db   'Hello, world!', 0
foundmsg: db   "First 'o' at index $"
nfmsg:    db   'Not found.', 0x0D, 0x0A, '$'
```

**Output:** `First 'o' at index 4`

`'Hello, world!'` — `H`(0) `e`(1) `l`(2) `l`(3) `o`(4). ✔

**Note the search for the terminator comes after the match test.** That way `strchr` with `AL = 0`
correctly finds the terminator, which is how C's `strchr` behaves.

---

## Program 42.6 — Reverse a string in place

```asm
; strrev.asm — reverse a string in place
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        print bmsg
        mov  si, str
        call puts
        newline

        mov  si, str
        call strrev

        print amsg
        mov  si, str
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strrev — reverse the ASCIIZ string at DS:SI, in place
;   Destroys:  AX, CX, SI, DI
; ---------------------------------------------------------------
strrev:
        push si
        ; --- find the length ---
        mov  di, si
        cld
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb
        not  cx
        dec  cx                 ; CX = the length
        pop  si

        cmp  cx, 2
        jb   .done              ; 0 or 1 characters — nothing to do

        mov  di, si
        add  di, cx
        dec  di                 ; DI -> the last character
        shr  cx, 1              ; swap only half of them

.swap:
        mov  al, [si]
        mov  ah, [di]
        mov  [si], ah
        mov  [di], al
        inc  si
        dec  di
        loop .swap
.done:
        ret

str:    db   'Stressed was I ere I saw desserts', 0
bmsg:   db   'Before: $'
amsg:   db   'After:  $'
```

**Output:**

```
Before: Stressed was I ere I saw desserts
After:  stressed was I ere I saw desserts
```

(Which is nearly the same — that is the point of the example sentence.)

---

## Program 42.7 — Palindrome test

```asm
; palin.asm — is a string a palindrome, ignoring case and punctuation?
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        call is_palindrome
        jc   .no
        print yesmsg
        exit 0
.no:
        print nomsg
        exit 1

; ---------------------------------------------------------------
; is_palindrome — test the ASCIIZ string at DS:SI
;   Out:       CF = 0 if it is, CF = 1 if not
;   Destroys:  AX, BX, CX, SI, DI
;
;   Ignores anything that is not a letter, and ignores case.
; ---------------------------------------------------------------
is_palindrome:
        push si
        ; --- find the end ---
        mov  di, si
        cld
        xor  al, al
        mov  cx, 0xFFFF
        repne scasb
        sub  di, 2              ; back past the terminator to the last character
        pop  si

.loop:
        cmp  si, di
        jae  .yes               ; pointers met or crossed -> palindrome

        ; --- advance SI to the next letter ---
.skip_left:
        cmp  si, di
        jae  .yes
        mov  al, [si]
        call is_letter
        jnc  .got_left
        inc  si
        jmp  .skip_left
.got_left:

        ; --- retreat DI to the previous letter ---
.skip_right:
        cmp  si, di
        jae  .yes
        mov  bl, [di]
        push ax
        mov  al, bl
        call is_letter
        mov  bl, al
        pop  ax
        jnc  .got_right
        dec  di
        jmp  .skip_right
.got_right:

        ; --- compare, case-insensitively ---
        or   al, 0x20           ; force lower case (both are known letters)
        or   bl, 0x20
        cmp  al, bl
        jne  .no

        inc  si
        dec  di
        jmp  .loop

.yes:
        clc
        ret
.no:
        stc
        ret

; ---------------------------------------------------------------
; is_letter — CF = 0 if AL is A-Z or a-z, CF = 1 otherwise
;   Preserves AL.
; ---------------------------------------------------------------
is_letter:
        cmp  al, 'A'
        jb   .no
        cmp  al, 'Z'
        jbe  .yes
        cmp  al, 'a'
        jb   .no
        cmp  al, 'z'
        jbe  .yes
.no:    stc
        ret
.yes:   clc
        ret

str:     db   'A man, a plan, a canal: Panama', 0
yesmsg:  db   'It is a palindrome.', 0x0D, 0x0A, '$'
nomsg:   db   'Not a palindrome.', 0x0D, 0x0A, '$'
```

**Output:** `It is a palindrome.`

### Explanation

**Two pointers converging**, skipping non-letters on each side independently. The `cmp si, di` /
`jae` test appears three times because either skip loop can run the pointers together.

**`or al, 0x20` for case folding** is safe here *because* `is_letter` has already confirmed the
character is a letter (Chapter 25 §7.1).

---

## Program 42.8 — Case conversion

```asm
; strcase.asm — convert to upper and lower case
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, str
        mov  di, buf
        call strcpy_local
        mov  si, buf
        call strupper
        print umsg
        mov  si, buf
        call puts
        newline

        mov  si, str
        mov  di, buf
        call strcpy_local
        mov  si, buf
        call strlower
        print lmsg
        mov  si, buf
        call puts
        newline
        exit 0

; ---------------------------------------------------------------
; strupper — convert the ASCIIZ string at DS:SI to upper case, in place
; ---------------------------------------------------------------
strupper:
        push si
.next:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, 'a'
        jb   .skip
        cmp  al, 'z'
        ja   .skip
        and  al, 0xDF           ; clear bit 5
        mov  [si], al
.skip:
        inc  si
        jmp  .next
.done:
        pop  si
        ret

; ---------------------------------------------------------------
; strlower — convert to lower case, in place
; ---------------------------------------------------------------
strlower:
        push si
.next:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, 'A'
        jb   .skip
        cmp  al, 'Z'
        ja   .skip
        or   al, 0x20           ; set bit 5
        mov  [si], al
.skip:
        inc  si
        jmp  .next
.done:
        pop  si
        ret

strcpy_local:
        cld
.next:  lodsb
        stosb
        or   al, al
        jnz  .next
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

str:    db   'MiXeD CaSe 123!', 0
buf:    times 64 db 0
umsg:   db   'Upper: $'
lmsg:   db   'Lower: $'
```

**Output:**

```
Upper: MIXED CASE 123!
Lower: mixed case 123!
```

**The range checks are essential.** Without them, `or al, 0x20` would turn `'['` (`0x5B`) into `'{'`
(`0x7B`), and `and al, 0xDF` would turn `'{'` into `'['`. Digits and punctuation must be left alone.

---

## Program 42.9 — Word count

```asm
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
```

**Output:**

```
Words:      17
Characters: 89
Lines:      3
```

### Explanation

**The state flag in `BP`.** A word is counted when we transition from "between words" to "inside a
word". Counting every non-space character would give the character count; counting every space would
be wrong for runs of spaces. The flag is what makes the transition detectable.

**`BP` used as a general register** — legal, because we never write `[bp]` (Chapter 7 §3.2).

---

## Program 42.10 — Split into tokens

```asm
; tokens.asm — split a string on spaces and print each token on its own line
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        mov  si, text
        xor  bx, bx             ; BX = token count
        cld

.find_token:
        ; --- skip leading separators ---
.skip:
        mov  al, [si]
        or   al, al
        jz   .done
        cmp  al, ' '
        jne  .found_start
        inc  si
        jmp  .skip

.found_start:
        inc  bx
        mov  di, si             ; DI -> the start of this token

        ; --- find the end ---
.scan:
        mov  al, [si]
        or   al, al
        jz   .emit
        cmp  al, ' '
        je   .emit
        inc  si
        jmp  .scan

.emit:
        push si
        push ax
        ; print from DI up to (but not including) SI
        mov  ax, bx
        call print_udec
        print colon
        mov  cx, si
        sub  cx, di             ; CX = the token length
.putc:
        mov  dl, [di]
        push ax
        mov  ah, 0x02
        int  0x21
        pop  ax
        inc  di
        loop .putc
        newline
        pop  ax
        pop  si

        or   al, al
        jz   .done
        inc  si                 ; step past the separator
        jmp  .find_token

.done:
        print tmsg
        mov  ax, bx
        call print_udec
        newline
        exit 0

text:   db   '  alpha beta   gamma delta  ', 0
colon:  db   ': $'
tmsg:   db   'Total tokens: $'
```

**Output:**

```
1: alpha
2: beta
3: gamma
4: delta
Total tokens: 4
```

**Runs of spaces are handled correctly** because `.skip` consumes all of them before a token starts,
and leading and trailing spaces produce no empty tokens.

---

## Program 42.11 — Character frequency

```asm
; charfreq.asm — count how often each letter appears
        cpu  8086
        org  0x100
%include "macros.inc"
%include "io.inc"

start:
        ; --- clear the 26 counters ---
        cld
        mov  di, counts
        mov  cx, 26
        xor  ax, ax
        rep  stosw

        ; --- count ---
        mov  si, text
.next:
        lodsb
        or   al, al
        jz   .report
        or   al, 0x20           ; fold to lower case
        cmp  al, 'a'
        jb   .next
        cmp  al, 'z'
        ja   .next
        sub  al, 'a'            ; 0..25
        xor  ah, ah
        mov  bx, ax
        shl  bx, 1              ; ×2 — counters are words
        inc  word [counts+bx]
        jmp  .next

        ; --- report only the non-zero counts ---
.report:
        xor  bx, bx
.loop:
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        or   ax, ax
        jz   .skip
        mov  al, bl
        add  al, 'a'
        putc al
        print colon
        mov  si, bx
        shl  si, 1
        mov  ax, [counts+si]
        call print_udec
        newline
.skip:
        inc  bx
        cmp  bx, 26
        jb   .loop
        exit 0

text:   db   'the quick brown fox jumps over the lazy dog', 0
counts: times 26 dw 0
colon:  db   ': $'
```

Every letter appears at least once — it is a pangram — so all 26 lines are printed.

**`or al, 0x20` before the range check** is safe here because anything that is not a letter fails the
subsequent `cmp al, 'a'` / `cmp al, 'z'` test and is skipped. Digits become themselves plus 0x20
only if bit 5 was clear, which for `'0'`–`'9'` (`0x30`–`0x39`) it already is — so they are unchanged,
and then rejected by the range check.

---

## Program 42.12 — Replace a substring

```asm
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
```

**Output:**

```
Before: the cat sat on the mat, the cat was fat
After:  the dog sat on the mat, the dog was fat
Replacements: 2
```

### Explanation

**Equal lengths only.** Replacing a 3-character pattern with a 5-character one would require moving
the rest of the string, which needs a second buffer or a backwards copy. Exercise 42.13 asks for it.

**`BX` walks the text during the comparison, not `SI`.** That way `SI` still points at the candidate
position if the match fails, and `.no_match` only has to `inc si`. A version that advanced `SI` would
have to push and pop it around every comparison.

**Three memory variables instead of three registers.** This routine needs a text pointer, a pattern
pointer, a replacement pointer, a pattern length, a comparison counter and a match counter — six
live values, against the 8086's four usable pointer registers. Memory variables cost 6 EA clocks per
access and make the code readable; stack juggling costs the same and does not. Exercise 42.14 asks
for the stack-frame version, which is the right answer when the routine must be reentrant.

---

## Exercises

**42.1** Rewrite `strlen` to count until a `$` rather than a zero. Which DOS functions need each
form?

**42.2** Write `strncpy` — copy at most *n* characters, padding with zeros if the source is shorter.

**42.3** Modify `strcmp` to be case-insensitive.

**42.4** Write `strstr` — find the first occurrence of one string inside another, returning a
pointer or `CF = 1`.

**42.5** Write `strrchr` — find the *last* occurrence of a character.

**42.6** Program 42.5 searches for the terminator after testing for a match. What does `strchr` with
`AL = 0` do, and why is that the right behaviour?

**42.7** Write a routine that trims leading and trailing spaces from a string, in place.

**42.8** Modify the word counter to also report the longest word and its length.

**42.9** Write a routine that counts how many times one string occurs inside another, including
overlapping occurrences.

**42.10** Program 42.8's case conversion checks ranges before folding. Remove the checks and give an
input where the result is wrong.

**42.11** Write a routine that converts a string to "title case" — first letter of each word upper,
the rest lower.

**42.12** Rewrite `strcpy` using `REP MOVSB` after first measuring the length with `REPNE SCASB`. Is
it faster than the `LODSB`/`STOSB` version? Show the arithmetic for a 20-character string.

**42.13** Extend Program 42.12 to handle a replacement of a *different* length.

**42.14** Rewrite `replace_all` using a stack frame with `BP`, so the pattern address, replacement
address and length are locals rather than juggled registers.

**42.15** Write a routine that checks whether two strings are anagrams of each other.

Answers in [Appendix H](H-exercise-solutions.md#chapter-42).

---

[← Sorting and searching](41-programs-sorting-searching.md) · [Contents](README.md) · [Next: Number conversion →](43-programs-number-conversion.md)
