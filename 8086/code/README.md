# Source code

Every complete, named program from the book, extracted verbatim from the chapters and organised by
chapter number. 97 programs in 34 directories.

---

## Building

From this directory:

```
nasm -f bin -I. ch34/hello.asm -o ch34/hello.com -l ch34/hello.lst
```

The `-I.` matters: several programs `%include "macros.inc"` or `%include "io.inc"`, which live here
rather than in the chapter directories.

Then run the `.com` file inside DOSBox. Chapter 1 sets the toolchain up.

### Build scripts

`build.bat` (Windows) and `build.sh` (macOS, Linux) assemble one program and report its size:

```
build ch34\hello                 (Windows)
./build.sh ch34/hello            (Unix)
```

`buildall.sh` assembles everything and reports which programs fail, which is the quickest way to
check a fresh NASM installation.

---

## The shared includes

| File | Contents |
|------|----------|
| `macros.inc` | `exit`, `print`, `putc`, `newline`, `waitkey`, `gotoxy`, `cls`, `save`, `restore` — Chapter 38 §8 |
| `io.inc` | `print_udec`, `print_sdec`, `print_hex16`, `print_hex32`, `read_udec` — Chapter 39 §0 |

Every macro preserves the registers it touches, so they compose freely.

---

## What runs where

| Chapters | Where it runs |
|----------|---------------|
| 0–46 | **DOSBox** — these are ordinary DOS programs |
| 47, 52, 53 | a **trainer board** with an 8255 at ports `0x00`–`0x06`. `ch47/lpt.asm` is the PC-compatible variant and does run under DOSBox |
| 48 | **DOSBox** — it emulates the PC's 8253 at `0x40`–`0x43` |
| 49 | **DOSBox** for the masking and EOI examples; re-initialising the PIC needs real hardware |
| 50 | a trainer board with an 8251A; §9 of the chapter gives a PC UART version |
| 54 | **DOSBox** — it emulates an x87 |

Each Part V chapter says explicitly at the top whether its programs run under DOSBox.

---

## A note on verification

These listings were written and hand-checked against the Intel documentation, not assembled by the
author's toolchain — NASM was not installed on the machine the book was written on. Every encoding,
clock count and flag effect was derived by hand.

If you find a program that does not assemble or does not behave as the chapter says, the chapter is
wrong and worth correcting. Run `buildall.sh` first; it will find any syntax errors immediately.
