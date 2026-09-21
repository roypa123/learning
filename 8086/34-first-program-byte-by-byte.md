# Chapter 34 — Your first program, byte by byte

[← NASM and program structure](33-nasm-directives.md) · [Contents](README.md) · [Next: `.COM` versus `.EXE` →](35-com-vs-exe.md)

---

## Goal

Take one small program and account for **every single byte** — what it is, why the assembler chose
it, where it sits in memory, and what the processor does with it. Then trace the execution
instruction by instruction, register by register, with the stack drawn at each step.

Nothing is assumed. If you have read Parts I–III, everything here is a consolidation; if you skipped
ahead, this chapter is where it all becomes concrete.

---

## 1. The program

```asm
; hello.asm — the complete first program
; nasm -f bin hello.asm -o hello.com -l hello.lst
        cpu  8086
        org  0x100

start:
        mov  ah, 0x09           ; DOS function 09h: print a $-terminated string
        mov  dx, msg            ; DS:DX must point at the string
        int  0x21               ; call DOS

        mov  ah, 0x4C           ; DOS function 4Ch: terminate
        mov  al, 0              ; exit code 0
        int  0x21               ; call DOS — does not return

msg:    db   'Hello, 8086!', 0x0D, 0x0A, '$'
```

Assemble it:

```
nasm -f bin hello.asm -o hello.com -l hello.lst
```

The result is **28 bytes**. Section 3 accounts for all 28.

---

## 2. The listing file

Open `hello.lst`:

```
     1                                  ; hello.asm — the complete first program
     2                                  ; nasm -f bin hello.asm -o hello.com -l hello.lst
     3                                          cpu  8086
     4                                          org  0x100
     5
     6                                  start:
     7 00000000 B409                            mov  ah, 0x09
     8 00000002 BA0D01                          mov  dx, msg
     9 00000005 CD21                            int  0x21
    10
    11 00000007 B44C                            mov  ah, 0x4C
    12 00000009 B000                            mov  al, 0
    13 0000000B CD21                            int  0x21
    14
    15 0000000D 48656C6C6F2C20383038-           msg:    db   'Hello, 8086!', 0x0D, 0x0A, '$'
    16 00000017 36210D0A24
```

Four columns:

| Column | Meaning |
|--------|---------|
| `1`, `7`, `15` | source line number |
| `00000000`, `00000002` | **offset within the output file**, in hex |
| `B409`, `BA0D01` | **the bytes generated** |
| the rest | your source text |

Note lines 3 and 4 generate **no bytes at all**. `cpu` and `org` are directives.

**Run-time address = file offset + `0x100`**, because of `org 0x100`. So line 7's instruction is at
file offset 0 and run-time offset `0x100`.

---

## 3. Every byte

```
file    run-time   bytes           source
offset  offset
------  --------   -------------   -------------------------
 0x00    0x100     B4 09           mov  ah, 0x09
 0x02    0x102     BA 0D 01        mov  dx, msg
 0x05    0x105     CD 21           int  0x21
 0x07    0x107     B4 4C           mov  ah, 0x4C
 0x09    0x109     B0 00           mov  al, 0
 0x0B    0x10B     CD 21           int  0x21
 0x0D    0x10D     48 65 6C 6C 6F 2C 20 38 30 38 36 21    'Hello, 8086!'
 0x19    0x119     0D              carriage return
 0x1A    0x11A     0A              line feed
 0x1B    0x11B     24              '$'
------
total: 0x1C = 28 bytes
```

Now justify each one.

### 3.1 `B4 09` — `mov ah, 0x09`

**`B4`** is the opcode. From Chapter 20 §6, the pattern `0xB0 + reg` is "move an immediate byte into
an 8-bit register":

```
   0xB0 + reg   where reg is the 3-bit register number, w = 0 (8-bit)

   reg = 000  AL   ->  B0
   reg = 001  CL   ->  B1
   reg = 010  DL   ->  B2
   reg = 011  BL   ->  B3
   reg = 100  AH   ->  B4     <- this one
   reg = 101  CH   ->  B5
   reg = 110  DH   ->  B6
   reg = 111  BH   ->  B7
```

`AH` is register 100 in the 8-bit encoding (Chapter 20 §3.1), so `0xB0 + 4 = 0xB4`.

**`09`** is the immediate byte — the value being moved.

**No ModR/M byte**, because the register is already encoded in the opcode. That is what makes this
form two bytes instead of three.

**Why `0x09`?** It selects DOS function 09h, "display string". Chapter 36 §4.

### 3.2 `BA 0D 01` — `mov dx, msg`

**`BA`** is `0xB8 + reg` — "move an immediate *word* into a 16-bit register":

```
   reg = 000  AX   ->  B8
   reg = 001  CX   ->  B9
   reg = 010  DX   ->  BA     <- this one
   reg = 011  BX   ->  BB
```

`DX` is register 010, so `0xB8 + 2 = 0xBA`.

**`0D 01`** is the immediate word, **low byte first** (Chapter 2 §7). It represents `0x010D`.

**Where did `0x010D` come from?** The label `msg` sits at file offset `0x0D`. `org 0x100` told the
assembler that file offset 0 will be at run-time offset `0x100`. So:

```
   msg  =  0x100 + 0x0D  =  0x010D
```

**Change anything above `msg` and this number changes.** Add one `nop` at the top and the operand
becomes `0x010E`. That is why you write `msg` and not a literal.

### 3.3 `CD 21` — `int 0x21`

**`CD`** is the opcode for `INT imm8` (Chapter 31 §4).

**`21`** is the interrupt type number.

Two bytes. Note that `INT 3` would be the single byte `CC`, a special case (Chapter 31 §4.1).

### 3.4 `B4 4C` — `mov ah, 0x4C`

Same form as §3.1. `0xB4` = move immediate to `AH`; `0x4C` = DOS function 4Ch, "terminate with
return code".

### 3.5 `B0 00` — `mov al, 0`

`0xB0 + 000` = move immediate to `AL`. `0x00` is the exit code.

**`B4` and `B0` differ by exactly 4** because `AH` is register 100 (binary) = 4 and `AL` is register
000 = 0. That is the whole explanation of a detail that looks arbitrary in a hex dump.

### 3.6 `CD 21` — the second `int 0x21`

Identical bytes to §3.3. The *function* differs because `AH` differs.

### 3.7 The string

```
   48 65 6C 6C 6F 2C 20 38 30 38 36 21
   H  e  l  l  o  ,     8  0  8  6  !
```

Twelve ASCII bytes. `0x48` is `'H'`, `0x65` is `'e'`, `0x20` is a space, `0x2C` is a comma. Full
table in [Appendix D](D-ascii-table.md).

**`0D`** is carriage return — moves the cursor to column 0.
**`0A`** is line feed — moves the cursor down one line.

Both are needed on DOS. Printing only `0A` moves down without returning to the left margin, so the
next line starts in the middle of the screen. This is the difference between `\n` on Unix and
`\r\n` on DOS.

**`24`** is `'$'`, the terminator DOS function 09h looks for. It is not printed.

---

## 4. The hex dump

```
Windows PowerShell :  Format-Hex hello.com
macOS / Linux      :  xxd hello.com
```

```
00000000: b409 ba0d 01cd 21b4 4cb0 00cd 2148 656c  ......!.L...!Hel
00000010: 6c6f 2c20 3830 3836 210d 0a24            lo, 8086!..$
```

Read it against §3: `b4 09 ba 0d 01 cd 21 b4 4c b0 00 cd 21` is the 13 bytes of code, then
`48 65 6c ...` is the string.

**The ASCII column on the right** shows why hex dumps are useful: text is immediately visible and
code is not. A dot means "not a printable character".

---

## 5. What DOS does before the first instruction runs

You type `hello` and press Enter. Between that and `mov ah, 0x09`, quite a lot happens.

### 5.1 The load

1. `COMMAND.COM` parses the line, finds `HELLO.COM` on disk.
2. It calls DOS's EXEC function (`INT 21h`, `AH = 4Bh`).
3. DOS finds a free block of memory — say it starts at paragraph `0x0700`, physical `0x07000`.
4. DOS builds a **Program Segment Prefix** — a 256-byte control block — at `0700:0000`.
5. DOS reads the entire file into `0700:0100`, immediately after the PSP.
6. DOS sets the registers (§5.3) and jumps to `0700:0100`.

### 5.2 Memory after loading

```
   physical   offset   contents
   ─────────  ──────   ──────────────────────────────────────
   0x07000    0x0000   ┌────────────────────────────┐
                       │  PSP — 256 bytes           │
                       │  0x00: INT 20h instruction │
                       │  0x02: top of memory       │
                       │  0x2C: environment segment │
                       │  0x5C: FCB 1               │
                       │  0x80: command-tail length │
                       │  0x81: command tail text   │
   0x07100    0x0100   ├────────────────────────────┤
                       │  B4 09    mov ah, 9        │
                       │  BA 0D 01 mov dx, 010D     │
                       │  CD 21    int 21h          │
                       │  B4 4C    mov ah, 4Ch      │
                       │  B0 00    mov al, 0        │
                       │  CD 21    int 21h          │
   0x0710D    0x010D   ├────────────────────────────┤
                       │  'Hello, 8086!' 0D 0A '$'  │
   0x0711C    0x011C   ├────────────────────────────┤
                       │                            │
                       │  free                      │
                       │                            │
                       │            ↓ stack grows   │
   0x16FFE    0xFFFE   └────────────────────────────┘  <- SP
```

Chapter 35 covers the PSP field by field.

### 5.3 Registers at entry

| Register | Value | Why |
|----------|-------|-----|
| `CS` | `0x0700` | the PSP segment |
| `DS` | `0x0700` | **the same** — this is what makes `.COM` work |
| `ES` | `0x0700` | the same |
| `SS` | `0x0700` | the same |
| `IP` | `0x0100` | the entry point |
| `SP` | `0xFFFE` | top of the 64 KiB segment |
| `AX` | drive-validity codes | see Chapter 35 §3.4 |
| everything else | **undefined** | do not assume zero |

**All four segment registers equal.** That is the whole `.COM` model: one 64 KiB segment containing
code, data and stack. It is why `mov dx, msg` works without setting `DS` first — `DS` already points
at the right segment.

**`SP = 0xFFFE` and DOS has pushed one word there**: a zero, so that a `RET` at the end of your
program would jump to offset 0 — the PSP — where DOS has placed an `INT 20h` instruction, which
terminates the program. That is the old CP/M-compatible exit route. We use `INT 21h`/`4Ch` instead,
which is better because it can return an exit code.

---

## 6. Executing it, instruction by instruction

### Step 0 — entry

```
   CS = 0700   IP = 0100   DS = ES = SS = 0700   SP = FFFE
   AX = ????   BX = ????   CX = ????   DX = ????

   physical address being fetched = 0700 × 16 + 0100 = 0x07100
```

The BIU fetches from `0x07100` and fills the queue with `B4 09 BA 0D 01 CD`.

### Step 1 — `mov ah, 0x09` at `0x100`

```
   before:  AX = ????
   after:   AX = 09??       (only AH changed)
            IP = 0x0102
   flags:   UNCHANGED — MOV never touches flags
   clocks:  4
```

`AL` keeps whatever DOS left in it. That is fine here; function 09h ignores `AL`.

### Step 2 — `mov dx, msg` at `0x102`

```
   before:  DX = ????
   after:   DX = 0x010D
            IP = 0x0105
   flags:   unchanged
   clocks:  4
```

`DS` is already `0x0700`, so `DS:DX` = `0700:010D` = physical `0x0710D` — the `'H'` of `'Hello'`.

### Step 3 — `int 0x21` at `0x105`

This is where the interesting part happens. The processor:

```
   1.  push FLAGS              SP: FFFE -> FFFC,  [SS:FFFC] = flags
   2.  IF = 0
   3.  TF = 0
   4.  push CS                 SP: FFFC -> FFFA,  [SS:FFFA] = 0x0700
   5.  push IP                 SP: FFFA -> FFF8,  [SS:FFF8] = 0x0107
   6.  IP <- word at physical 0x00084       (= 4 × 0x21)
   7.  CS <- word at physical 0x00086
```

The pushed `IP` is `0x0107` — the address of the instruction **after** the `INT`, which is
`mov ah, 0x4C`.

The stack now:

```
   0xFFF8  ┌──────────┐  <- SP
           │  0x0107  │  IP
   0xFFFA  ├──────────┤
           │  0x0700  │  CS
   0xFFFC  ├──────────┤
           │  flags   │
   0xFFFE  ├──────────┤
           │  0x0000  │  the word DOS pushed at load time
           └──────────┘
```

`CS:IP` now points into DOS's `INT 21h` handler, somewhere in the DOS kernel.

### Step 4 — inside DOS

DOS's handler:

1. Reads `AH` = `0x09` and dispatches to its "display string" routine.
2. Reads `DS:DX` = `0700:010D`.
3. Loops: read a byte; if it is `'$'`, stop; otherwise write it to standard output (which goes
   through `INT 29h` on some DOS versions, or directly to the BIOS via `INT 10h` function 0Eh).
4. Executes `IRET`.

The screen now shows `Hello, 8086!` and the cursor is at the start of the next line.

### Step 5 — the `IRET` returns

```
   1.  pop IP      IP = 0x0107,  SP: FFF8 -> FFFA
   2.  pop CS      CS = 0x0700,  SP: FFFA -> FFFC
   3.  pop FLAGS   flags restored, SP: FFFC -> FFFE
```

`SP` is back to `0xFFFE` — exactly where it was. **The stack is balanced.** Execution resumes at
`0700:0107`.

Note that DOS may have changed `AX`, `BX`, `CX`, `DX`, `SI`, `DI` and `BP` — the only guarantee is
`CS:IP`, `SS:SP` and the segment registers. Chapter 36 §2 lists what each function preserves.

### Step 6 — `mov ah, 0x4C` at `0x107`

```
   AX = 4C??
   IP = 0x0109
```

### Step 7 — `mov al, 0` at `0x109`

```
   AX = 0x4C00
   IP = 0x010B
```

Note this could have been one instruction: `mov ax, 0x4C00` (`B8 00 4C`) — three bytes instead of
four, and 4 clocks instead of 8. Most programs write it that way, and the rest of this book does.

### Step 8 — `int 0x21` at `0x10B`

The same interrupt sequence as Step 3. DOS reads `AH = 0x4C`, which is "terminate with return code
`AL`".

DOS:

1. Closes any files the program opened.
2. Frees the memory block.
3. Restores the `INT 22h`/`23h`/`24h` vectors from the PSP.
4. Returns to `COMMAND.COM`, which prints its prompt.

**This `INT 21h` never returns.** The stack is irrelevant from this point.

---

## 7. The execution summary

| Step | Address | Bytes | Instruction | Clocks | Registers after |
|------|---------|-------|-------------|--------|-----------------|
| 1 | `0x100` | `B4 09` | `mov ah, 9` | 4 | `AH = 09` |
| 2 | `0x102` | `BA 0D 01` | `mov dx, 010D` | 4 | `DX = 010D` |
| 3 | `0x105` | `CD 21` | `int 21h` | 51 + DOS | screen written |
| 4 | `0x107` | `B4 4C` | `mov ah, 4Ch` | 4 | `AH = 4C` |
| 5 | `0x109` | `B0 00` | `mov al, 0` | 4 | `AL = 00` |
| 6 | `0x10B` | `CD 21` | `int 21h` | 51 + DOS | program ends |

Your program's own instructions cost **67 clocks** — 13.4 µs. Everything else is DOS.

---

## 8. Six things you can now change and predict

### 8.1 Change the message

```asm
msg:    db   'Goodbye!', 0x0D, 0x0A, '$'
```

`'Goodbye!'` is 8 characters instead of 12, so the file becomes 24 bytes. `msg` is still at `0x010D`
because nothing *above* it changed — the `BA 0D 01` is unchanged.

### 8.2 Add an instruction before `msg`

```asm
        nop                     ; one byte, 0x90
        mov  ah, 0x09
        ...
```

Now everything shifts by one: `msg` is at `0x010E`, and the `mov dx` operand becomes `0E 01`. Check
the listing to confirm.

### 8.3 Use the combined exit

```asm
        mov  ax, 0x4C00         ; B8 00 4C — three bytes
        int  0x21
```

Saves one byte and 4 clocks. `msg` moves down to `0x010C`.

### 8.4 Remove the `$`

The program prints `Hello, 8086!` followed by whatever bytes happen to follow in memory, until DOS
finds a `0x24` somewhere. Since the program ends at `0x11C` and beyond that is uninitialised memory,
the output is garbage of unpredictable length.

### 8.5 Remove `org 0x100`

The assembler computes `msg` as `0x000D` instead of `0x010D`, so `mov dx, 0x000D` and `DS:DX` points
at offset `0x0D` — inside the PSP. DOS prints whatever is there, which on most systems is part of
the `INT 22h` vector copy. Garbage, but *different* garbage from §8.4.

### 8.6 Jump into the data

```asm
        mov  ah, 0x09
        mov  dx, msg
        int  0x21
        ; no exit — execution falls through into msg
msg:    db   'Hello, 8086!', 0x0D, 0x0A, '$'
```

Execution continues past the `INT 21h` into the string, and the processor decodes `48 65 6C ...` as
instructions:

```
   48        dec ax
   65        (a segment prefix on later chips; on an 8086, an undefined byte)
   6C 6C     ...
```

The result is undefined behaviour and almost certainly a crash. **This is why data goes after an
unconditional exit** (Chapter 33 §7.1).

---

## 9. Single-stepping it

In DOSBox-X's debugger, or FreeDOS `DEBUG`:

```
C:\> debug hello.com
-u 100 10c                      ; unassemble
0700:0100 B409          MOV     AH,09
0700:0102 BA0D01        MOV     DX,010D
0700:0105 CD21          INT     21
0700:0107 B44C          MOV     AH,4C
0700:0109 B000          MOV     AL,00
0700:010B CD21          INT     21

-r                              ; show registers
AX=0000  BX=0000  CX=001C  DX=0000  SP=FFFE  BP=0000  SI=0000  DI=0000
DS=0700  ES=0700  SS=0700  CS=0700  IP=0100   NV UP EI PL NZ NA PO NC
0700:0100 B409          MOV     AH,09

-t                              ; trace one instruction
AX=0900  BX=0000  ...  IP=0102
                                ; AH is now 09

-t
AX=0900  ...  DX=010D  IP=0105
                                ; DX is now 010D

-d 10d 11c                      ; dump the string
0700:010D  48 65 6C 6C 6F 2C 20 38-30 38 36 21 0D 0A 24     Hello, 8086!..$

-p                              ; PROCEED over the INT (step over)
Hello, 8086!
AX=0924  ...  IP=0107
                                ; note AL changed — DOS left 0x24 in it

-q
```

Two things to notice in that session.

**`CX = 0x001C` at entry** — 28, the file size. DOS leaves it there, and it is one of the few
register values you can rely on.

**Use `-p` (proceed), not `-t` (trace), on an `INT`.** `-t` steps *into* DOS, and you will spend the
next ten minutes single-stepping the DOS kernel.

---

## 10. Building it yourself, without NASM

You now know enough to write the file by hand. In DOS `DEBUG`:

```
C:\> debug
-a 100                          ; assemble at offset 100
0AF3:0100 mov ah,9
0AF3:0102 mov dx,10d
0AF3:0105 int 21
0AF3:0107 mov ah,4c
0AF3:0109 mov al,0
0AF3:010B int 21
0AF3:010D                       ; blank line to stop

-e 10d 'Hello, 8086!' 0d 0a '$' ; enter the data
-r cx                           ; set the file length
CX 0000
:1c                             ; 28 bytes
-n hello2.com                   ; name the file
-w                              ; write it
Writing 0001C bytes
-q

C:\> hello2.com
Hello, 8086!
```

That is how programs were written before assemblers were common, and it is an instructive
half-hour. Note `-r cx` sets the length: `DEBUG` writes `CX` bytes starting from offset `0x100`.

---

## 11. Summary

```
  28 bytes, and every one is accounted for:

    B4 09        B0+reg form, reg=100 (AH), w=0        -> mov ah, 09h
    BA 0D 01     B8+reg form, reg=010 (DX), w=1        -> mov dx, 010Dh
                 operand low byte first: 0D 01 = 010Dh
                 010Dh = org 0x100 + file offset 0x0D of `msg`
    CD 21        INT imm8                              -> int 21h
    B4 4C        mov ah, 4Ch
    B0 00        B0+000 (AL)                           -> mov al, 0
    CD 21        int 21h
    48 65 ...    ASCII 'Hello, 8086!'
    0D 0A        CR LF — DOS needs BOTH
    24           '$', the terminator DOS 09h stops at

  B4 and B0 differ by 4 because AH is register 100 and AL is register 000

  at entry:  CS = DS = ES = SS = the PSP segment,  IP = 0x100,  SP = 0xFFFE
             CX = the file size;  everything else undefined

  INT 21h pushes FLAGS, CS, IP (6 bytes), clears IF and TF,
     and loads CS:IP from physical 0x84 (= 4 × 0x21)
  IRET pops them back and SP returns to exactly where it was

  your program's own instructions cost 67 clocks — 13 µs. DOS costs the rest.
```

---

## Exercises

**34.1** Add `nop` as the first instruction. Predict the new file size, the new value of the
`mov dx` operand, and the new run-time address of `msg`. Then check with the listing.

**34.2** Replace `mov ah, 0x4C` / `mov al, 0` with `mov ax, 0x4C00`. What bytes does that generate,
how many bytes does the file lose, and where is `msg` now?

**34.3** Why is `B4` the opcode for `mov ah, imm8` and `B0` the opcode for `mov al, imm8`?

**34.4** The operand of `mov dx, msg` is stored as `0D 01`. Explain both the order and the value.

**34.5** How many bytes does `int 0x21` generate? How many would `int 3` generate, and why is it
different?

**34.6** List everything the processor pushes on the stack when `int 0x21` executes, in order, with
the value of `SP` after each.

**34.7** At entry, `SP = 0xFFFE` and there is a zero word at `[SS:FFFE]`. What is it for?

**34.8** Delete the `'$'` and describe precisely what the program does. Now delete `org 0x100`
instead, and describe what it does. Why are the two failures different?

**34.9** Write a version that prints two separate lines using two `INT 21h` calls, and give the
complete byte listing you expect before assembling it.

**34.10** Using `DEBUG`, assemble and run the program by hand. What must `CX` be set to before `-w`,
and why?

**34.11** In the `DEBUG` session in §9, `AX` became `0x0924` after the first `INT 21h`. Where did
the `0x24` come from?

**34.12** Why does the chapter tell you to use `-p` rather than `-t` on an `INT` instruction?

**34.13** The program's own instructions take 67 clocks. Estimate what fraction of the program's
total run time that is, and say what the rest is spent on.

Answers in [Appendix H](H-exercise-solutions.md#chapter-34).

---

[← NASM and program structure](33-nasm-directives.md) · [Contents](README.md) · [Next: `.COM` versus `.EXE` →](35-com-vs-exe.md)
