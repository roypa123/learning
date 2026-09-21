# Chapter 1 — Setting up the toolchain

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: Number systems →](02-number-systems.md)

---

## Goal

Get from an empty machine to a running 8086 program, and to a debugger in which you can watch that
program execute one instruction at a time. Every step has a check. If a check fails, the
troubleshooting section at the end names the likely cause.

By the end of this chapter you will have assembled, run and single-stepped a program, and you will
have a build script you use for the rest of the book.

---

## 1. What we need and why

Four things.

**An assembler** turns the text you write into machine code bytes. We use **NASM**, the Netwide
Assembler. It is free, actively maintained, runs on every desktop OS, and — crucially for us — can
emit a *flat binary*: a file containing exactly the bytes you asked for and nothing else. That is
what a DOS `.COM` file is, and it means you can hex-dump the output and account for every byte.
Chapter 34 does exactly that.

**A machine to run 16-bit code on.** Your actual processor can still execute 8086 instructions, but
your *operating system* will not let a 16-bit real-mode program run: 64-bit Windows removed the
16-bit subsystem entirely, and modern Linux and macOS never had DOS. So we use **DOSBox**, which
emulates a 8086/80286-class PC with DOS. It is free, it is accurate enough for everything in this
book, and it is the way practically everyone runs DOS software now.

**A debugger.** DOSBox ships with a good-enough `DEBUG` clone in some builds; more reliably, we use
**DOSBox-X**, a fork with a real built-in debugger, or the classic `DEBUG.EXE` from FreeDOS. Either
lets you set breakpoints, single-step, and dump registers and memory. Chapter 46 is entirely about
using these.

**A text editor.** Anything that saves plain text with Unix or DOS line endings. VS Code with an
`x86 and x86_64 Assembly` extension gives syntax highlighting; Notepad works; `vim` works.

A fifth thing is optional but worth having: **a linker**, for the multi-file `.EXE` programs of
Chapter 38. NASM can produce `obj` files that the free **JWlink** or Open Watcom's `wlink` turn into
`.EXE`. We defer that to Chapter 35; for now, flat `.COM` files need no linker at all, which is one
reason the book starts with them.

---

## 2. Installing NASM

### 2.1 Windows

1. Go to `https://www.nasm.us/`, follow *Download* to the latest stable release, then the
   `win64/` directory, and take the installer (`nasm-<version>-installer-x64.exe`).
2. Run it. Accept the default install location, typically
   `C:\Program Files\NASM` or `C:\Users\<you>\AppData\Local\bin\NASM`.
3. Add that directory to your `PATH`. The installer usually offers to; if it does not:
   press <kbd>Win</kbd>, type *environment variables*, choose **Edit the system environment
   variables** → **Environment Variables…** → under *User variables* select `Path` → **Edit** →
   **New** → paste the NASM directory → OK out of all three dialogs.
4. **Open a new terminal** — `PATH` changes do not apply to already-open windows.

### 2.2 macOS

```
brew install nasm
```

If you do not have Homebrew, install it from `https://brew.sh/` first. Apple ships its own `nasm` in
some Xcode versions; it is an old Apple fork and will reject syntax we use, so make sure the
Homebrew one comes first in `PATH` (`which -a nasm` shows the order).

### 2.3 Linux

```
sudo apt install nasm          # Debian, Ubuntu, Mint
sudo dnf install nasm          # Fedora, RHEL
sudo pacman -S nasm            # Arch
```

### 2.4 Check

```
nasm -v
```

Expected output, with some version number:

```
NASM version 2.16.03 compiled on Apr 17 2024
```

> **Check 1 passed** when `nasm -v` prints a version. If you get *command not found* or *'nasm' is
> not recognised*, see §10.1.

---

## 3. Installing DOSBox

### 3.1 Which DOSBox

There are three you will see named:

| Build | Use it for |
|-------|-----------|
| **DOSBox** (0.74-3) | The original. Fine for running programs. No debugger in the standard Windows build. |
| **DOSBox-X** | A fork with a proper integrated debugger, better DOS compatibility and a config GUI. **Recommended for this book.** |
| **DOSBox Staging** | A modernised fork focused on gaming. Works, but its debugger is less complete. |

Install **DOSBox-X** from `https://dosbox-x.com/` (Windows installer, macOS `.dmg`, Linux packages
or AppImage). Everything in this book also works on plain DOSBox except the debugger sections of
Chapter 46.

### 3.2 First run

Launch it. You should get a window with a black screen and:

```
Z:\>
```

`Z:` is DOSBox's built-in virtual drive holding its own utilities. It is not your disk.

> **Check 2 passed** when you see the `Z:\>` prompt.

---

## 4. A working directory

Make one folder that will hold everything you write for this book. The examples assume:

```
Windows :  C:\asm86
macOS   :  ~/asm86
Linux   :  ~/asm86
```

Create it, and inside it create a subfolder per chapter as you go:

```
asm86\
  ch01\
  ch34\
  ch39\
  ...
```

### 4.1 Mounting it in DOSBox

DOSBox cannot see your disk until you *mount* a folder as a drive letter:

```
Z:\> mount c c:\asm86
Drive C is mounted as local directory c:\asm86\

Z:\> c:
C:\>
```

On macOS/Linux the middle line is `mount c ~/asm86`.

Typing that every time is tedious, so put it in the config file. Find it with:

```
Z:\> config -writeconf dosbox-x.conf
```

or locate the existing one — DOSBox-X prints its path in the console at startup; typical locations
are `%LOCALAPPDATA%\DOSBox-X\dosbox-x.conf` on Windows and `~/.config/dosbox-x/dosbox-x.conf`
elsewhere. Open it in your editor, scroll to the very bottom, and find:

```ini
[autoexec]
# Lines in this section will be run at startup.
```

Add:

```ini
[autoexec]
mount c c:\asm86
c:
```

Save, restart DOSBox-X, and you should land directly at `C:\>`.

> **Check 3 passed** when DOSBox starts at `C:\>` and `dir` lists your folders.

### 4.2 The one thing that will bite you

DOSBox reads the mounted folder *when it starts*, and caches the directory listing. If you create a
new file on the host while DOSBox is running, DOSBox may not see it. Press
<kbd>Ctrl</kbd>+<kbd>F4</kbd> to force it to re-read the mounted drive. Remember this; you will need
it within the hour.

---

## 5. The first program

Create `C:\asm86\ch01\first.asm` in your editor with exactly this content:

```asm
; first.asm — prints a message and exits
; Assemble:  nasm -f bin first.asm -o first.com
; Run:       first.com      (inside DOSBox)

        org  0x100              ; DOS loads a .COM image at offset 0x100

start:
        mov  ah, 0x09           ; DOS service 09h = print $-terminated string
        mov  dx, msg            ; DS:DX must point at the string
        int  0x21               ; invoke DOS

        mov  ah, 0x4C           ; DOS service 4Ch = terminate program
        mov  al, 0              ; exit code 0
        int  0x21               ; invoke DOS — does not return

msg:    db   'The toolchain works.', 0x0D, 0x0A, '$'
```

Do not worry yet about what any of it means; Chapter 34 explains every byte and Chapter 36 explains
the DOS calls. Right now we are testing the pipeline.

### 5.1 Assemble

In your *host* terminal (not DOSBox), from the folder containing the file:

```
nasm -f bin first.asm -o first.com
```

Reading that command:

| Part | Meaning |
|------|---------|
| `nasm` | the assembler |
| `-f bin` | output **f**ormat = flat **bin**ary: no headers, no relocations, just bytes |
| `first.asm` | the input source file |
| `-o first.com` | **o**utput to this filename |

If it prints nothing at all, it worked. Assemblers, like compilers, are silent on success.

Check the result:

```
Windows :  dir first.com
Unix    :  ls -l first.com
```

You should see a file of **36 bytes**. Not 35, not 512. If the size differs, you mistyped the
source; compare it character by character. Section 5.3 accounts for all 36.

> **Check 4 passed** when `first.com` exists and is 36 bytes.

### 5.2 Run

In DOSBox:

```
C:\> cd ch01
C:\CH01> first.com
The toolchain works.

C:\CH01>
```

> **Check 5 passed** when you see the message. If DOSBox says *Illegal command*, press
> <kbd>Ctrl</kbd>+<kbd>F4</kbd> and try again — see §4.2.

You have now assembled and executed 8086 machine code.

### 5.3 Look at the bytes

This is the habit that makes the rest of the book work. Dump the file:

```
Windows PowerShell :  Format-Hex first.com
macOS / Linux      :  xxd first.com
```

```
00000000: b409 ba0d 01cd 21b4 4cb0 00cd 2154 6865  ......!.L...!The
00000010: 2074 6f6f 6c63 6861 696e 2077 6f72 6b73   toolchain works
00000020: 2e0d 0a24                                 ...$
```

Thirty-six bytes, and every one is accounted for:

| File offset | Bytes | Source line |
|---|---|---|
| `00` | `B4 09` | `mov ah, 0x09` |
| `02` | `BA 0D 01` | `mov dx, msg` |
| `05` | `CD 21` | `int 0x21` |
| `07` | `B4 4C` | `mov ah, 0x4C` |
| `09` | `B0 00` | `mov al, 0` |
| `0B` | `CD 21` | `int 0x21` |
| `0D` | `54 68 65 … 2E` | the 20 characters of the message |
| `21` | `0D 0A 24` | carriage return, line feed, `$` |

Add them up: 2 + 3 + 2 + 2 + 2 + 2 = 13 bytes of code, 20 characters of text, 3 bytes of
terminator = 36.

Three observations, each of which gets a full treatment later.

**The operand of `mov dx, msg` is `0D 01`, meaning `0x010D`, stored low byte first.** That is the
*little-endian* rule, and it applies to every multi-byte value the 8086 puts in memory. Chapter 2 §7
explains it.

**`0x010D` is `0x100 + 13`.** The label `msg` sits 13 bytes into the file, and `org 0x100` told the
assembler that byte 0 of the file will live at offset `0x100` at run time, so `msg` = `0x100 +
0x0D`. Change any instruction above `msg` and this number changes — which is exactly why we write
`msg` and not a hard-coded number.

**The mnemonic is not in the file.** There is no "MOV" anywhere in those 36 bytes. `B4` *is* "move
the following immediate byte into `AH`", and `B0` *is* "move the following immediate byte into
`AL`". Chapter 20 shows why those two opcodes differ by exactly 4, and how to derive any opcode from
the map.

> **Check 6 passed** when your dump shows `b4 09 ba 0d 01 cd 21` as the first seven bytes.

---

## 6. A build script

You will assemble hundreds of times. Automate it.

### 6.1 Windows — `build.bat`

Put this in `C:\asm86\build.bat`:

```bat
@echo off
rem  Usage:  build ch01\first        (no extension)
if "%~1"=="" (
    echo usage: build ^<path\to\source-without-extension^>
    exit /b 1
)
nasm -f bin "%~1.asm" -o "%~1.com" -l "%~1.lst"
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)
echo Built %~1.com
for %%F in ("%~1.com") do echo Size: %%~zF bytes
```

Run as `build ch01\first`.

### 6.2 Unix — `build.sh`

```sh
#!/bin/sh
# Usage: ./build.sh ch01/first
set -e
[ -n "$1" ] || { echo "usage: $0 <path/to/source-without-extension>"; exit 1; }
nasm -f bin "$1.asm" -o "$1.com" -l "$1.lst"
echo "Built $1.com  ($(wc -c < "$1.com") bytes)"
```

`chmod +x build.sh` once, then `./build.sh ch01/first`.

### 6.3 The listing file

Both scripts pass `-l` to produce a **listing file**, and this is the most useful thing in the
toolchain. Open `ch01/first.lst`:

```
     1                                  ; first.asm — prints a message and exits
     2                                          org  0x100
     3
     4                                  start:
     5 00000000 B409                            mov  ah, 0x09
     6 00000002 BA0D01                          mov  dx, msg
     7 00000005 CD21                            int  0x21
     8
     9 00000007 B44C                            mov  ah, 0x4C
    10 00000009 B000                            mov  al, 0
    11 0000000B CD21                            int  0x21
    12
    13 0000000D 546865...                       msg: db 'The toolchain works.', ...
```

Four columns: source line number, **offset within the output file**, the **bytes generated**, and
your source text. Every time you wonder "what did that assemble to?", the answer is in the listing.
Get in the habit of opening it.

Note the offsets are file offsets (starting at 0), while the addresses at run time start at `0x100`
because of `org`. Add `0x100` to convert. Chapter 33 §3 explains `org` fully.

---

## 7. The debugger

### 7.1 DOSBox-X's built-in debugger

Start DOSBox-X, then press <kbd>Alt</kbd>+<kbd>Pause</kbd> (Windows) or select
**Debug → Start DOSBox-X Debugger** from the menu. A second window appears showing registers,
a disassembly, a data pane and a command line.

Load the program without running it:

```
C:\CH01> debug first.com
```

Hmm — plain DOSBox-X does not ship `DEBUG.COM` by default in all builds. Two routes:

**Route A — the DOSBox-X debugger.** In the debugger window's command line:

```
BPINT 21          ; break whenever INT 21h is executed
```

then run `first.com` in the DOS window. Execution stops at the first DOS call with all registers
visible. Useful commands:

| Command | Effect |
|---------|--------|
| `F5` | run / continue |
| `F10` | step over (execute a `CALL` without entering it) |
| `F11` | step into (single instruction) |
| `BP cs:0100` | breakpoint at `CS:0100` |
| `BPINT 21 09` | break only on `INT 21h` with `AH = 09h` |
| `D DS:0` | dump memory at `DS:0000` |
| `SR AX 1234` | set `AX` to `0x1234` |

**Route B — FreeDOS `DEBUG`.** Download the FreeDOS `debug` package
(`https://www.freedos.org/` → software → `debug`), put `DEBUG.EXE` in `C:\asm86\bin\`, and add that
to the DOS `PATH` by putting `set PATH=C:\BIN;%PATH%` in your `[autoexec]` section (after the
mount). Then:

```
C:\CH01> debug first.com
-r                     ; show registers
-u 100 10c             ; unassemble
-t                     ; trace one instruction
-g                     ; go
-q                     ; quit
```

Chapter 46 covers both in full. For now, one exercise:

### 7.2 Single-step your first program

Using whichever debugger you have, execute `first.com` one instruction at a time and watch `AH`
become `0x09`, then `DX` become `0x010D`. Stop before `int 0x21` and look at the memory at `DS:010D`
— you should see the ASCII of your message.

> **Check 7 passed** when you have watched `AH` change from whatever it was to `09`.

This is the single most valuable skill in the book. Everything in Parts III and IV can be understood
by stepping through it and watching the registers.

---

## 8. Editor setup (optional but recommended)

### VS Code

Install the extension **x86 and x86_64 Assembly** (publisher `13xforever`) for syntax highlighting.
Then add a build task — create `.vscode/tasks.json` in `C:\asm86`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "nasm: build .com",
      "type": "shell",
      "command": "nasm",
      "args": [
        "-f", "bin",
        "${file}",
        "-o", "${fileDirname}/${fileBasenameNoExtension}.com",
        "-l", "${fileDirname}/${fileBasenameNoExtension}.lst"
      ],
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": {
        "owner": "nasm",
        "fileLocation": ["absolute"],
        "pattern": {
          "regexp": "^(.*):(\\d+):\\s+(warning|error):\\s+(.*)$",
          "file": 1, "line": 2, "severity": 3, "message": 4
        }
      }
    }
  ]
}
```

Now <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> assembles the open file, and NASM's errors become
clickable.

### Line endings

NASM accepts both `LF` and `CRLF`. DOS tools sometimes do not. If you ever edit a file *inside*
DOSBox with `EDIT`, it writes `CRLF`; that is fine. Do not let your editor strip the final newline
from a source file — some NASM versions warn about it.

---

## 9. What "running on DOSBox" means, and its limits

DOSBox does not emulate an 8086. By default it emulates something closer to a 486, with 386
instructions available, and its *timing is not cycle-accurate*. Three consequences:

1. **A program that uses 386 instructions will run under DOSBox but would not run on a real 8086.**
   This book never uses them, and in Chapter 33 you will add `CPU 8086` to your sources so NASM
   *rejects* anything the real chip could not execute. Do that from Chapter 34 onwards; it is your
   only guard against accidentally learning the wrong architecture.

2. **Cycle counts in Chapter 32 are not measurable in DOSBox.** They are from the Intel datasheet
   and they are correct for real silicon. Treat DOSBox as a functional simulator, not a timing one.
   The `cycles` setting in `dosbox-x.conf` changes speed but not per-instruction ratios.

3. **The hardware in Part V is not present.** DOSBox emulates a PC's 8253, 8259A and 8255-ish
   keyboard controller at the addresses the IBM PC used, so several Part V programs run as written.
   Others — the ADC, the stepper motor — assume a lab trainer board's address map and cannot. Each
   such chapter says explicitly whether its program runs under DOSBox, runs on real hardware only,
   or is paper-only.

To make DOSBox-X behave more like the real thing, set in `dosbox-x.conf`:

```ini
[cpu]
cputype=8086
core=normal
cycles=fixed 1000
```

`cputype=8086` makes the emulated CPU *fault* on 186+ instructions, which is a genuinely useful
safety net. Leave `cycles` alone unless a program runs too fast to see.

---

## 10. Troubleshooting

### 10.1 `nasm: command not found` / `'nasm' is not recognized`

The executable is not on `PATH`. Verify where it actually is:

```
Windows :  where nasm          (if this fails, search: dir /s /b C:\nasm.exe)
Unix    :  which nasm
```

Then either add that folder to `PATH` (§2.1) or call it by full path. Remember to open a **new**
terminal after changing `PATH`.

### 10.2 `first.asm:6: error: symbol 'msg' undefined`

A typo in the label. NASM labels are case sensitive: `msg` and `Msg` are different symbols. Also
check the label definition ends with a colon (`msg:`), or is at the start of a line.

### 10.3 The program prints garbage, or prints the message plus junk

Almost always a missing `$` terminator, or a missing `org 0x100`. DOS function 09h prints until it
meets `$`; with no terminator it prints whatever follows in memory until it happens to find a `0x24`
byte. Without `org 0x100`, `mov dx, msg` loads an address 0x100 too low, and DOS prints whatever is
in the Program Segment Prefix.

### 10.4 The program prints nothing and DOSBox hangs

You probably wrote `int 0x21` without setting `AH`, or set `AH` to a function that waits for input.
<kbd>Ctrl</kbd>+<kbd>F9</kbd> kills DOSBox; <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Home</kbd> reboots
the emulated DOS in DOSBox-X.

### 10.5 `Illegal command: FIRST.COM`

DOSBox has not noticed the new file. <kbd>Ctrl</kbd>+<kbd>F4</kbd>. If it still fails, check you
mounted the right folder (`mount -u c` then re-`mount`), and that the filename obeys DOS's 8.3 rule
— `myfirstprogram.com` becomes something like `MYFIRS~1.COM`, which is confusing but still works.

### 10.6 The `.com` file is 0 bytes

NASM failed but the shell created the output file anyway. Scroll up; there is an error message.

### 10.7 `error: parser: instruction expected`

Usually a line that begins with something NASM does not recognise as a label, instruction or
directive — a stray character, a smart quote pasted from a web page, or a `#` comment (assembly
comments start with `;`).

---

## 11. Your working layout at the end of this chapter

```
asm86\
  build.bat            (or build.sh)
  .vscode\tasks.json   (optional)
  bin\DEBUG.EXE        (optional)
  ch01\
    first.asm
    first.com          36 bytes
    first.lst
```

and a DOSBox-X config whose `[autoexec]` mounts `asm86` as `C:` and whose `[cpu]` section says
`cputype=8086`.

---

## Exercises

**1.1** Change the message in `first.asm` to your own name and rebuild. Predict the new file size
before you check it, then check.

**1.2** Delete the `$` from the end of `msg`, rebuild, and run it. Describe exactly what happens and
explain why, in terms of what DOS function 09h does.

**1.3** Remove the `org 0x100` line, rebuild, and compare the listing file to the original. Which
bytes changed, and by how much? Run it and explain the output.

**1.4** Using the listing file, state the run-time address (not file offset) of the `int 0x21`
instruction on line 11.

**1.5** Write a second program `two.asm` that prints two separate lines by calling function 09h
twice with two different strings. Do not use a loop.

**1.6** Using the debugger, set a breakpoint at the second `int 0x21` in `first.com` and report the
values of `AX`, `DX` and `IP` when it is hit.

**1.7** Set `cputype=8086` in your DOSBox config, then write a one-line program containing the 386
instruction `push 0x1234` — actually a 186 instruction — assemble it with `nasm -f bin`, and
describe what happens when you run it. (NASM will assemble it happily; the emulated CPU will not
like it.)

Answers in [Appendix H](H-exercise-solutions.md#chapter-1).

---

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: Number systems →](02-number-systems.md)
