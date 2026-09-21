# Chapter 38 — Macros and modular programs

[← BIOS services](37-bios-services.md) · [Contents](README.md) · [Next: Programs: arithmetic →](39-programs-arithmetic.md)

---

## Goal

NASM's preprocessor — macros, conditional assembly, repetition, includes — and the two ways to build
a program from several files. This is what makes the larger programs in Parts IV and V manageable.

---

## 1. Macro versus procedure

```asm
; A PROCEDURE: one copy of the code, called.
print_char:
        mov  ah, 0x02
        int  0x21
        ret
        ; ... call print_char        3 bytes at each call site + 19+16 clocks

; A MACRO: the code is pasted in at each use.
%macro print_char 0
        mov  ah, 0x02
        int  0x21
%endmacro
        ; ... print_char             4 bytes at each use site, no call overhead
```

| | Procedure | Macro |
|---|-----------|-------|
| Code size | one copy | one copy **per use** |
| Speed | `CALL` + `RET` = 35 clocks overhead | no overhead |
| Parameters | registers, stack | textual substitution |
| Debugging | one place to breakpoint | scattered |
| Recursion | yes | no (though macros can nest) |

**Use a procedure when the body is more than a few instructions or is used many times. Use a macro
when the body is short, or when it must be textually different each time.**

---

## 2. `%macro`

```asm
%macro name parameter_count
        ; body, using %1 %2 %3 ... for the parameters
%endmacro
```

### 2.1 A simple one

```asm
%macro exit 1
        mov  ax, 0x4C00 + %1
        int  0x21
%endmacro

; use:
        exit 0                  ; expands to  mov ax, 0x4C00 / int 0x21
        exit 1                  ; expands to  mov ax, 0x4C01 / int 0x21
```

### 2.2 With several parameters

```asm
%macro print 1
        mov  dx, %1
        mov  ah, 0x09
        int  0x21
%endmacro

%macro setcursor 2
        mov  ah, 0x02
        xor  bh, bh
        mov  dh, %1             ; row
        mov  dl, %2             ; column
        int  0x10
%endmacro

; use:
        print msg
        setcursor 10, 30
```

That reads almost like a high-level language, and it generates exactly the instructions you would
have written.

### 2.3 Local labels inside macros

A macro used twice would define its labels twice — a duplicate-symbol error. Prefix them with `%%`:

```asm
%macro strlen 1
        mov  si, %1
        xor  cx, cx
%%next:                         ; %% makes this unique per expansion
        lodsb
        or   al, al
        jz   %%done
        inc  cx
        jmp  %%next
%%done:
%endmacro
```

Each expansion gets labels named `..@1.next`, `..@2.next` and so on. **Always use `%%` for labels
inside a macro.**

### 2.4 Default parameters

```asm
%macro clear_screen 0-1 0x07     ; 0 to 1 parameters, default 0x07
        mov  ax, 0x0600
        mov  bh, %1
        xor  cx, cx
        mov  dx, 0x184F
        int  0x10
%endmacro

        clear_screen            ; grey on black
        clear_screen 0x1F       ; white on blue
```

### 2.5 A variable number of parameters

```asm
%macro pushall 1-*              ; one or more
  %rep  %0                      ; %0 = the actual parameter count
        push %1
  %rotate 1                     ; shift the parameters left
  %endrep
%endmacro

%macro popall 1-*
  %rep  %0
  %rotate -1                    ; go backwards, so pops reverse the pushes
        pop  %1
  %endrep
%endmacro

; use:
        pushall ax, bx, cx, dx
        ; ... work ...
        popall  ax, bx, cx, dx  ; pops in the right order automatically
```

`%rotate` moves the parameter window. This is genuinely useful — it is `PUSHA`/`POPA` for an 8086.

---

## 3. `%define`, `%assign` and `%strlen`

### 3.1 `%define` — textual substitution

```asm
%define SCREEN      0xB800
%define CELL(r,c)   (((r) * 80 + (c)) * 2)
%define ATTR(bg,fg) ((bg) * 16 + (fg))

        mov  ax, SCREEN
        mov  di, CELL(10, 20)           ; -> (((10)*80+(20))*2) = 1640
        mov  ah, ATTR(1, 15)            ; -> ((1)*16+(15)) = 31 = 0x1F
```

**Always parenthesise the parameters** inside the definition. Without the brackets,
`CELL(a+1, b)` would expand to `((a+1*80+b)*2)` — wrong.

### 3.2 `%define` versus `equ`

```asm
BUFSIZE equ    256              ; evaluated ONCE, here
%define BUFSZ  256              ; substituted textually at each use
```

For a constant, `equ` is clearer and marginally faster to assemble. Use `%define` when you need
parameters or when the value must be re-evaluated.

### 3.3 `%assign` — a preprocessor variable

```asm
%assign counter 0

%macro numbered_label 0
label_%+counter:
%assign counter counter+1
%endmacro
```

Unlike `equ`, `%assign` can be changed. It is how you build tables at assembly time.

---

## 4. Conditional assembly

### 4.1 `%if`

```asm
%define DEBUG 1

%if DEBUG
        call dump_registers
%endif
```

The `call` is assembled only when `DEBUG` is non-zero. Set it to 0 and the instruction does not
exist in the output — no run-time cost at all.

### 4.2 `%ifdef`

```asm
%ifdef VERBOSE
        print startup_msg
%endif
```

Define it on the command line:

```
nasm -f bin -DVERBOSE prog.asm -o prog.com
```

### 4.3 `%ifidn` — compare text

```asm
%macro load 2
  %ifidni %1, ax                ; case-insensitive text comparison
        mov  ax, %2
  %else
        mov  %1, %2
  %endif
%endmacro
```

Rarely needed, but it lets a macro generate different code for different operand types.

### 4.4 A debug-print macro

The standard use — instrumentation you can switch off:

```asm
%define DEBUG 1

%macro dbg 1
  %if DEBUG
        push ax
        push dx
        mov  dx, %%msg
        mov  ah, 0x09
        int  0x21
        pop  dx
        pop  ax
        jmp  %%past
    %%msg:  db %1, 0x0D, 0x0A, '$'
    %%past:
  %endif
%endmacro

; use:
        dbg 'entering main loop'
        ; ...
        dbg 'about to divide'
```

Set `DEBUG` to 0 and every trace disappears from the binary. Note the `jmp %%past` — the string is
stored inline, so execution must skip over it (Chapter 33 §7.1).

---

## 5. `%include` — multi-file programs without a linker

The simplest way to split a `.COM` program.

**`strings.inc`:**

```asm
; strings.inc — string routines
; Included by main.asm; do not assemble separately.

; ---------------------------------------------------------------
; puts — print the $-terminated string at DS:DX
;   Destroys: AX
; ---------------------------------------------------------------
puts:
        mov  ah, 0x09
        int  0x21
        ret

; ---------------------------------------------------------------
; strlen — length of the zero-terminated string at DS:SI, into CX
;   Destroys: AX, SI
; ---------------------------------------------------------------
strlen:
        xor  cx, cx
.next:
        lodsb
        or   al, al
        jz   .done
        inc  cx
        jmp  .next
.done:
        ret
```

**`main.asm`:**

```asm
        cpu  8086
        org  0x100

start:
        mov  dx, msg
        call puts

        mov  si, msg
        call strlen
        ; CX = the length

        mov  ax, 0x4C00
        int  0x21

%include "strings.inc"
%include "numbers.inc"

msg:    db   'Hello$'
```

Build with one command:

```
nasm -f bin main.asm -o main.com
```

**No linker, no `.EXE`, no relocation.** The preprocessor pastes the files together and the whole
thing assembles as one unit.

### 5.1 Where to put the `%include`

**After your code and before your data**, so the included procedures do not sit between the entry
point and the first instruction. The layout that works:

```asm
        org  0x100
start:
        ; ... main code, ending in an exit ...

%include "lib1.inc"             ; procedures
%include "lib2.inc"

; ... data ...
```

### 5.2 Include guards

If two files both `%include "common.inc"`, its contents appear twice and every label is duplicated.
Guard it:

```asm
; common.inc
%ifndef COMMON_INC
%define COMMON_INC

; ... contents ...

%endif
```

---

## 6. Building with a linker

For genuinely large programs, or when mixing with a high-level language, use object files.

### 6.1 The modules

**`main.asm`:**

```asm
        cpu  8086
        segment code
        extern puts, strlen     ; defined elsewhere
        global _start

..start:
        mov  ax, data
        mov  ds, ax
        mov  dx, msg
        call puts
        mov  ax, 0x4C00
        int  0x21

        segment data
msg:    db   'Hello$'

        segment stack stack
        resb 256
```

**`strings.asm`:**

```asm
        cpu  8086
        segment code
        global puts, strlen     ; make these visible to other modules

puts:
        mov  ah, 0x09
        int  0x21
        ret

strlen:
        xor  cx, cx
.next:  lodsb
        or   al, al
        jz   .done
        inc  cx
        jmp  .next
.done:  ret
```

### 6.2 The build

```
nasm -f obj main.asm    -o main.obj
nasm -f obj strings.asm -o strings.obj
wlink file main.obj, strings.obj format dos name prog.exe
```

`global` exports a symbol; `extern` imports one. The linker resolves them.

### 6.3 Which to use

| | `%include` | Linker |
|---|-----------|--------|
| Output | `.COM` | `.EXE` |
| Build | one command | assemble each file, then link |
| Rebuild time | everything, every time | only changed modules |
| Size limit | 64 KiB total | none |
| Name collisions | possible — everything shares one namespace | controlled by `global`/`extern` |
| Extra tools | none | a linker |

**For everything in this book, `%include` is the right answer.** The linker route matters when a
project exceeds 64 KiB or must interoperate with C.

---

## 7. `%rep` — assembly-time repetition

```asm
%rep 10
        nop
%endrep                         ; ten NOPs
```

Building a table:

```asm
; A table of squares, 0..15, computed at assembly time.
squares:
%assign n 0
%rep 16
        dw   n*n
%assign n n+1
%endrep
```

That generates `dw 0, 1, 4, 9, 16, 25, ...` with no run-time cost. A sine table, a CRC table, a
gamma curve — all can be built this way, which is far better than computing them at start-up.

### 7.1 Unrolling a loop

```asm
%macro unrolled_copy 1
  %rep %1
        lodsb
        stosb
  %endrep
%endmacro

        mov  cx, 100
.next:
        unrolled_copy 8         ; eight copies per iteration
        loop .next              ; ... but CX must be 100/8
```

Chapter 32 §4 shows the clock savings. Do not do this until you have measured.

---

## 8. A practical macro library

The macros this book's later programs use. Put them in `macros.inc`.

```asm
; macros.inc — common macros
%ifndef MACROS_INC
%define MACROS_INC

; ---- program exit ----
%macro exit 0-1 0
        mov  ax, 0x4C00 + %1
        int  0x21
%endmacro

; ---- print a $-terminated string ----
%macro print 1
        push ax
        push dx
        mov  dx, %1
        mov  ah, 0x09
        int  0x21
        pop  dx
        pop  ax
%endmacro

; ---- print one character ----
%macro putc 1
        push ax
        push dx
        mov  dl, %1
        mov  ah, 0x02
        int  0x21
        pop  dx
        pop  ax
%endmacro

; ---- newline ----
%macro newline 0
        putc 0x0D
        putc 0x0A
%endmacro

; ---- wait for any key ----
%macro waitkey 0
        push ax
        xor  ah, ah
        int  0x16
        pop  ax
%endmacro

; ---- set the cursor ----
%macro gotoxy 2
        push ax
        push bx
        push dx
        mov  ah, 0x02
        xor  bh, bh
        mov  dh, %1
        mov  dl, %2
        int  0x10
        pop  dx
        pop  bx
        pop  ax
%endmacro

; ---- clear the screen with an attribute ----
%macro cls 0-1 0x07
        push ax
        push bx
        push cx
        push dx
        mov  ax, 0x0600
        mov  bh, %1
        xor  cx, cx
        mov  dx, 0x184F
        int  0x10
        pop  dx
        pop  cx
        pop  bx
        pop  ax
%endmacro

; ---- save / restore the common registers ----
%macro save 0
        push ax
        push bx
        push cx
        push dx
        push si
        push di
%endmacro

%macro restore 0
        pop  di
        pop  si
        pop  dx
        pop  cx
        pop  bx
        pop  ax
%endmacro

%endif
```

Using it:

```asm
        cpu  8086
        org  0x100
%include "macros.inc"

start:
        cls  0x1F
        gotoxy 10, 30
        print msg
        newline
        waitkey
        exit 0

msg:    db   'Hello from a macro!$'
```

Seven lines of main program. Every macro preserves the registers it uses, so they compose freely.

---

## 9. Pitfalls

**Macros that destroy registers silently.** `print msg` looks like a function call, but if it
clobbers `AX` the caller is surprised. **Make every macro preserve what it touches**, as the library
above does — the `push`/`pop` pairs cost clocks but prevent a class of bug that is very hard to
find.

**Forgetting `%%` on labels.** Using the macro twice gives `symbol redefined`. The error message
points at the macro definition, not at the second use, which is confusing the first time.

**Macros that are too clever.** A macro that generates different code depending on its arguments is
hard to debug, because the listing shows the expansion and the source shows the macro. Use `-E` to
see what the preprocessor produced:

```
nasm -E prog.asm > prog.i
```

**Assuming macros are procedures.** A macro cannot recurse, cannot be called indirectly, and
duplicates its code at every use. A 50-instruction macro used 20 times adds 1000 instructions to
your program.

---

## 10. Summary

```
  %macro name N ... %endmacro        parameters are %1 %2 ...
  %%label                            a label unique to each expansion — ALWAYS use this
  %0                                 the actual parameter count
  %rotate n                          shift the parameter window
  0-1 default                        optional parameters with a default
  1-*                                a variable number

  %define NAME value                 textual substitution, can take parameters
  %define F(a,b) ((a)+(b))           ALWAYS parenthesise the parameters
  %assign n expr                     a preprocessor variable that can change
  equ                                an assembly-time constant — prefer for plain numbers

  %if / %ifdef / %ifndef / %else / %endif      conditional assembly
  nasm -DNAME ...                    define a symbol from the command line

  %rep N ... %endrep                 repeat at assembly time — great for tables
  %include "file.inc"                paste a file in — the .COM way to modularise
     guard with %ifndef GUARD / %define GUARD / ... / %endif

  linker route: global/extern + nasm -f obj + wlink, producing an .EXE

  a macro is INLINED: no call overhead, but one copy per use
  a procedure is CALLED: 35 clocks of overhead, but one copy total
  make every macro preserve the registers it touches
  nasm -E shows you what the preprocessor actually produced
```

---

## Exercises

**38.1** Write a macro `addto dest, src` that adds `src` to `dest` using `AX` as scratch, preserving
`AX`.

**38.2** Write a macro `beep` that emits a bell character. How many bytes does each use cost?

**38.3** Why must labels inside a macro be written `%%label`? What error do you get without it?

**38.4** Write a macro `max3 a, b, c` that leaves the largest of three memory words in `AX`.

**38.5** Write a macro `swapmem a, b` that exchanges two memory words, preserving all registers.

**38.6** Convert the `print_dec` procedure from Chapter 23 §8 into a macro. Give one reason this is a
bad idea.

**38.7** Write a `%rep` block that generates a table of the first 20 Fibonacci numbers at assembly
time.

**38.8** Write a `%rep` block that generates a 256-byte table where entry *n* is *n* with its bits
reversed. (Hint: you may need a nested `%rep` and `%assign`.)

**38.9** Write a debug macro that prints a register's value in hex, active only when `DEBUG` is
defined on the command line.

**38.10** What goes wrong if two `%include`d files both include a third? How do you prevent it?

**38.11** Give three differences between a macro and a procedure, and state when you would choose
each.

**38.12** A macro body is 50 instructions and is used 20 times. How much does it add to the program
compared with a procedure? How many clocks does it save?

**38.13** Write `pushall`/`popall` macros that take a variable number of registers and guarantee the
pops reverse the pushes.

Answers in [Appendix H](H-exercise-solutions.md#chapter-38).

---

[← BIOS services](37-bios-services.md) · [Contents](README.md) · [Next: Programs: arithmetic →](39-programs-arithmetic.md)
