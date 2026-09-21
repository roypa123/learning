# Chapter 33 — NASM and program structure

[← Instruction timing](32-instruction-timing.md) · [Contents](README.md) · [Next: Your first program, byte by byte →](34-first-program-byte-by-byte.md)

---

## Goal

Everything the assembler contributes that is not an instruction: directives, data definitions,
constants, labels, expressions, the location counter, and the differences from MASM that you will
meet in any textbook.

The distinction to hold onto: **instructions become bytes the processor executes; directives tell
the assembler what to do.** `MOV` is an instruction. `ORG` is a directive. `ORG` generates no bytes.

---

## 1. The anatomy of a source line

```
   label:   instruction  operands        ; comment
```

All four parts are optional.

```asm
start:                              ; label alone
        mov  ax, 5                  ; instruction with operands
        ret                         ; instruction alone
; a whole line of comment
count   dw   0                      ; label and a data directive
```

### 1.1 Labels

- Must start with a letter, `_`, `.` or `?`.
- May contain letters, digits, `_`, `$`, `#`, `@`, `~`, `.`, `?`.
- **Case sensitive.** `Count` and `count` are different symbols.
- The colon is optional in NASM but conventional — use it, because without it a mistyped instruction
  silently becomes a label.

```asm
        mov ax, 5
        mvo ax, 5               ; typo. Without colons NASM might read 'mvo' as a label.
```

### 1.2 Local labels

A label beginning with `.` is **local to the preceding non-local label**:

```asm
strlen:
.next:  lodsb                   ; really 'strlen.next'
        or   al, al
        jnz  .next
        ret

strcpy:
.next:  lodsb                   ; a DIFFERENT label — 'strcpy.next'
        stosb
        or   al, al
        jnz  .next
        ret
```

Two procedures can each have a `.next` without colliding. **Use local labels for everything inside a
procedure.** It makes the code readable and removes the need for names like `strcpy_loop_2`.

### 1.3 Comments

`;` to end of line. That is the only comment syntax — `#`, `//` and `/* */` are all errors.

---

## 2. Defining data

### 2.1 The `D` directives — initialised data

| Directive | Size | Name |
|-----------|------|------|
| `db` | 1 byte | define byte |
| `dw` | 2 bytes | define word |
| `dd` | 4 bytes | define doubleword |
| `dq` | 8 bytes | define quadword |
| `dt` | 10 bytes | define ten bytes (8087 extended real) |

```asm
count   db   42                 ; one byte: 2A
total   dw   1000               ; two bytes: E8 03     (little-endian!)
big     dd   0x12345678         ; four bytes: 78 56 34 12
flags   db   10110101b          ; binary
mask    db   0xF0               ; hex
chr     db   'A'                ; 41
```

**Multiple values, comma separated:**

```asm
table   db   1, 2, 3, 4, 5
vowels  db   'a', 'e', 'i', 'o', 'u'
msg     db   'Hello', 0x0D, 0x0A, '$'       ; strings and bytes mix freely
words   dw   0x1234, 0x5678, 0x9ABC
```

**Strings** are just byte sequences:

```asm
s1      db   'Hello'            ; 48 65 6C 6C 6F
s2      db   "Hello"            ; identical — single and double quotes are the same
s3      db   'It''s'            ; escape a quote by doubling it
s4      db   `Line\n`           ; BACKQUOTES enable C escapes: 4C 69 6E 65 0A
```

The backquote form is NASM-specific and useful: `` `\r\n` `` is clearer than `0x0D, 0x0A`.

**A word with a string is not what you expect:**

```asm
        dw   'AB'               ; 42 41 — 'B' then 'A', because of little-endian
        db   'AB'               ; 41 42 — 'A' then 'B'
```

Use `db` for text. Always.

### 2.2 `RESB` and friends — uninitialised data

```asm
buffer  resb 128                ; reserve 128 bytes
values  resw 50                 ; reserve 50 words = 100 bytes
```

**In a flat binary (`-f bin`) these still occupy space in the file**, filled with zeros, unless they
are in a `.bss` section. For a `.COM` program it is simpler to use `times`:

```asm
buffer  times 128 db 0          ; 128 zero bytes, explicitly in the file
```

and place it at the very end, where the file could be truncated if you cared about size.

### 2.3 `TIMES` — repetition

```asm
        times 128 db 0          ; 128 zero bytes
        times 10  dw 0xFFFF     ; 10 words of FFFF
        times 5   nop           ; five NOP instructions — TIMES works on code too
```

The classic use, in a boot sector:

```asm
        times 510-($-$$) db 0   ; pad to 510 bytes
        dw   0xAA55             ; the boot signature
```

`$` and `$$` are explained in §5.

---

## 3. `ORG` — where the code will live

```asm
        org  0x100
```

**`ORG` generates no bytes.** It tells the assembler: "when this program runs, byte 0 of the output
file will be at offset `0x100`."

That affects every address the assembler computes:

```asm
        org  0x100
start:  mov  dx, msg            ; msg is at 0x100 + its file offset
msg:    db   'x$'
```

Without `org 0x100`, `msg` would be computed as its file offset alone, and `DX` would be 0x100 too
low — pointing into the PSP. The program prints garbage. Chapter 1 §10.3.

**Every `.COM` program needs `org 0x100`**, because that is where DOS loads it (Chapter 35 §2).

A boot sector uses `org 0x7C00`. A bare-metal ROM image uses whatever its decoder puts it at.

---

## 4. NASM versus MASM — the differences that matter

Any 8086 textbook you pick up will use MASM syntax. Here is the translation.

### 4.1 Brackets mean "contents of" — consistently

**This is the big one.**

| Meaning | NASM | MASM |
|---------|------|------|
| the **address** of `count` | `mov ax, count` | `mov ax, OFFSET count` |
| the **contents** of `count` | `mov ax, [count]` | `mov ax, count` |

NASM is consistent: brackets always dereference. MASM guesses from the symbol's declared type, which
is why `OFFSET` exists.

> **MASM note.** When you copy code from a textbook, `MOV DX, OFFSET msg` becomes `mov dx, msg`, and
> `MOV AX, total` becomes `mov ax, [total]`.

### 4.2 Sizes must be explicit when ambiguous

```asm
        mov  [bx], 5            ; ✘ NASM: byte or word?
        mov  byte [bx], 5       ; ✔
        mov  word [bx], 5       ; ✔
        inc  byte [count]       ; ✔
```

MASM infers the size from how `count` was declared. NASM does not track types, so you say.

When one operand is a register the size is unambiguous and no keyword is needed:

```asm
        mov  [bx], al           ; clearly a byte
        mov  [bx], ax           ; clearly a word
```

### 4.3 No `ASSUME`, no `.MODEL`

MASM's `ASSUME DS:DATA` tells the assembler which segment register points where, so it can generate
overrides automatically. NASM has no such mechanism: **you write the override yourself**, and you are
responsible for loading the segment registers.

```asm
; MASM
        .MODEL SMALL
        .DATA
msg     DB  'Hi$'
        .CODE
        MOV AX, @DATA
        MOV DS, AX

; NASM equivalent (.COM — one segment, so nothing to set up)
        org  0x100
        mov  dx, msg
msg:    db   'Hi$'
```

### 4.4 Directive spelling

| MASM | NASM |
|------|------|
| `DB` `DW` `DD` | `db` `dw` `dd` (case-insensitive) |
| `EQU` | `equ` |
| `DUP` | `times` |
| `PROC` / `ENDP` | just a label |
| `END start` | nothing — `-f bin` has no entry point |
| `SEGMENT` / `ENDS` | `section` |
| `OFFSET x` | `x` |
| `PTR` (`BYTE PTR [bx]`) | `byte [bx]` |

```asm
; MASM
buffer  DB  100 DUP(0)
; NASM
buffer  times 100 db 0
```

### 4.5 Numbers

| Form | NASM | MASM |
|------|------|------|
| Hex | `0x1F`, `1Fh`, `$1F` | `1Fh` |
| Binary | `0b1010`, `1010b` | `1010b` |
| Octal | `0o17`, `17q` | `17o` |
| Decimal | `31` | `31` |
| Character | `'A'` | `'A'` |

NASM accepts MASM's `1Fh` form, so textbook constants copy over unchanged — **except** that a hex
number starting with a letter needs a leading zero in both: `0FFh`, not `FFh`, because `FFh` looks
like a symbol.

---

## 5. `$` and `$$` — the location counter

```
   $    the address of the START of the current line
   $$   the address of the start of the current section
```

### 5.1 Computing a length

```asm
msg     db   'Hello, world!', 0x0D, 0x0A
msglen  equ  $ - msg            ; 15 — computed at assembly time
```

`$` at that point is the address just past the last byte of `msg`, so `$ - msg` is its length. **This
is the standard idiom** and it means you never count string lengths by hand.

```asm
        mov  cx, msglen         ; the assembler substituted 15
```

### 5.2 Padding

```asm
        times 510-($-$$) db 0   ; fill up to offset 510
```

`$-$$` is "how many bytes have I emitted so far in this section". So `510-($-$$)` is "how many more
to reach 510".

### 5.3 The infinite loop

```asm
        jmp  $                  ; jump to this instruction — EB FE
        jmp  $+2                ; jump to the next instruction — a queue flush (Ch 17 §7)
```

---

## 6. `EQU` and `%define` — constants

### 6.1 `EQU`

```asm
BUFSIZE equ  256
CR      equ  0x0D
LF      equ  0x0A
PORTA   equ  0x00
```

Evaluated **once**, at the point of definition, and it cannot be redefined. Use it for numbers.

```asm
        mov  cx, BUFSIZE
        mov  dl, CR
```

**`EQU` generates no bytes.** `BUFSIZE equ 256` does not put 256 anywhere; it teaches the assembler a
name.

### 6.2 `%define`

A preprocessor macro — textual substitution, evaluated each time it is used:

```asm
%define SCREEN  0xB800
%define CELL(r,c)  (((r)*80+(c))*2)

        mov  ax, SCREEN
        mov  di, CELL(10, 20)   ; expands to (((10)*80+(20))*2) = 1640
```

Use `equ` for simple constants and `%define` when you need parameters. Chapter 38 covers the
preprocessor properly.

### 6.3 Expressions

NASM evaluates arithmetic at assembly time:

```asm
        mov  ax, 5*8+2          ; assembles as MOV AX, 42
        mov  cx, BUFSIZE/2
        mov  bx, table + 4*3    ; the address of the 4th word
        mov  al, 1 << 5         ; 0x20
        and  al, ~(1 << 3)      ; 0xF7
        mov  dx, msg2 - msg1    ; the distance between two labels
```

Operators, in precedence order: `~ -` (unary), `* / % //`, `+ -`, `<< >>`, `&`, `^`, `|`.

Note `/` is **unsigned** division and `//` is signed. For constants that are always positive it makes
no difference.

---

## 7. Sections

```asm
        section .text           ; code
        section .data           ; initialised data
        section .bss            ; uninitialised data
```

**For `-f bin` output these are mostly decorative.** The flat binary format emits everything in the
order it appears, and a `.COM` file has no section structure at all. You can use them for
organisation, but this book's `.COM` programs put code first and data at the end without any
`section` directive — which is simpler and matches what the file actually contains.

They matter for object-file output (`-f obj`, Chapter 38) where the linker uses them.

### 7.1 Data after code, always

```asm
        org  0x100
start:
        ; ... all the code ...
        mov  ax, 0x4C00
        int  0x21

        ; ... all the data ...
msg:    db   'Hello$'
count:  dw   0
```

**Why data goes last:** the processor executes straight through whatever follows the last
instruction. Data placed between instructions would be executed as code. Putting all data after an
unconditional exit is the only safe arrangement in a flat binary.

If you must put data in the middle, jump over it:

```asm
        jmp  .past
table:  db   1, 2, 3, 4
.past:  mov  al, [table]
```

---

## 8. `CPU 8086` — the guard you should always use

```asm
        cpu  8086
```

Tells NASM to **reject any instruction the 8086 cannot execute**:

```asm
        cpu  8086
        shl  ax, 4              ; error: instruction not supported on the 8086
        push 0x1234             ; error
        pusha                   ; error
```

Without it, NASM happily assembles 80186 and later instructions, they run fine under DOSBox, and you
learn the wrong architecture. **Put `cpu 8086` at the top of every source file in this book.**

The programs in `code/` all have it.

---

## 9. The standard program templates

### 9.1 A `.COM` program

```asm
; name.asm — what it does
; nasm -f bin name.asm -o name.com
        cpu  8086
        org  0x100

start:
        ; ---- code ----

        mov  ax, 0x4C00         ; DOS: terminate, exit code 0
        int  0x21

; ---- data ----
msg:    db   'Hello$'
```

That is the whole template, and it is what Part IV uses throughout.

### 9.2 A `.COM` program with procedures

```asm
        cpu  8086
        org  0x100

start:
        call setup
        call do_work
        call cleanup
        mov  ax, 0x4C00
        int  0x21

; ---------------------------------------------------------------
; setup — ...
;   In:        nothing
;   Out:       nothing
;   Destroys:  nothing
; ---------------------------------------------------------------
setup:
        push ax
        ; ...
        pop  ax
        ret

; ... more procedures ...

; ---------------------------------------------------------------
; data
; ---------------------------------------------------------------
msg:    db   'Hello$'
buffer: times 128 db 0
```

### 9.3 The build command

```
nasm -f bin name.asm -o name.com -l name.lst
```

Always pass `-l`. The listing file is how you check what was generated (Chapter 1 §6.3).

---

## 10. Useful NASM command-line options

| Option | Effect |
|--------|--------|
| `-f bin` | flat binary output — what `.COM` needs |
| `-f obj` | 16-bit object file, for linking (Chapter 38) |
| `-o file` | output filename |
| `-l file` | generate a listing file |
| `-D name=value` | define a preprocessor symbol from the command line |
| `-I dir/` | add an include search directory |
| `-E` | run only the preprocessor and print the result |
| `-w+all` | enable all warnings |
| `-O0` | disable optimisation (always use the long form of jumps and immediates) |

`-E` is genuinely useful when a macro is not expanding the way you expect.

`-O0` is useful when hand-assembling: it stops NASM choosing short forms, so the bytes match the
general encoding you predicted.

---

## 11. Common assembler errors and what they mean

| Message | Cause |
|---------|-------|
| `symbol 'x' undefined` | typo, or the label is defined in another file |
| `operation size not specified` | `mov [bx], 5` — add `byte` or `word` |
| `short jump is out of range` | a conditional jump further than ±127 bytes (Chapter 27 §3) |
| `invalid combination of opcode and operands` | e.g. `mov ds, 0x100`, `mov [bx], [si]` |
| `instruction not supported on the 8086` | you have `cpu 8086` and used a later instruction — good |
| `parser: instruction expected` | a stray character, a smart quote pasted from the web, or a `#` comment |
| `comma expected` | usually a missing operand |
| `label or instruction expected at start of line` | indentation problem, or a label with a space before the colon |

---

## 12. Summary

```
  line:  label:  instruction  operands  ; comment      — all parts optional
  labels are CASE SENSITIVE; .labels are local to the previous global label

  db dw dd dq dt      define initialised data
  resb resw resd      reserve uninitialised space
  times N thing       repeat — works on data AND instructions
  equ                 an assembly-time constant, no bytes generated
  %define             preprocessor substitution, can take parameters
  org 0x100           where the code will live at run time — .COM needs this
  cpu 8086            reject later instructions — USE THIS ALWAYS

  $   address of the current line       $$  start of the current section
  msglen equ $ - msg                    the standard length idiom
  times 510-($-$$) db 0                 the standard padding idiom

  NASM vs MASM:
     mov ax, count      -> MASM: contents · NASM: THE ADDRESS
     mov ax, [count]    -> NASM: the contents
     MASM OFFSET count  -> NASM just: count
     MASM BYTE PTR [bx] -> NASM: byte [bx]
     MASM 100 DUP(0)    -> NASM: times 100 db 0
     no ASSUME, no .MODEL — you write segment overrides yourself

  put ALL data after the final exit, or the processor executes it
  build with:  nasm -f bin x.asm -o x.com -l x.lst
```

---

## Exercises

**33.1** What bytes does each of these generate?

```asm
(a) db  0x41, 0x42
(b) dw  0x1234
(c) db  'AB'
(d) dw  'AB'
(e) dd  0x12345678
(f) times 3 db 0xFF
```

**33.2** Write the data definitions for: a 16-byte zeroed buffer, a word initialised to 1000, a
`$`-terminated string reading `Ready`, and a table of the five vowels.

**33.3** Write the `equ` that computes the length of this string automatically:

```asm
msg     db   'The quick brown fox', 0x0D, 0x0A
```

**33.4** What does `org 0x100` generate? What breaks if you omit it from a `.COM` program?

**33.5** Translate to NASM:

```asm
        MOV AX, OFFSET buffer
        MOV BX, count
        MOV BYTE PTR [SI], 0
        buf DB 50 DUP(?)
```

**33.6** Why must `mov [bx], 5` be written `mov byte [bx], 5` or `mov word [bx], 5`?

**33.7** Explain the difference between `equ` and `%define`. Give a case where only `%define` works.

**33.8** What does `times 510-($-$$) db 0` do, and in what kind of program does it appear?

**33.9** Why does data go after the final `int 0x21` rather than before the code?

**33.10** Write the `jmp` that skips over a table placed in the middle of the code.

**33.11** What does `cpu 8086` do, and why does this book insist on it?

**33.12** Two procedures both want a loop label called `next`. Show how local labels solve this.

**33.13** What is the value of `1 << 5`? Of `~(1 << 3)` as a byte? Write the instructions that set
bit 5 and clear bit 3 of `AL` using these forms.

Answers in [Appendix H](H-exercise-solutions.md#chapter-33).

---

[← Instruction timing](32-instruction-timing.md) · [Contents](README.md) · [Next: Your first program, byte by byte →](34-first-program-byte-by-byte.md)
