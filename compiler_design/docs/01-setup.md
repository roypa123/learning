# Chapter 1 — Setting up: compiler, editor, building

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: C++ for compiler writers →](02-cpp-for-compiler-writers.md)

---

## Goal

Get a working C++17 toolchain, build the first chapter program, and understand what the build command
actually does. Fifteen minutes if all goes well. This chapter is also your reference for the rest of
the book: every later chapter assumes `run <name>` works.

There is one extra requirement compared with an ordinary C++ book: from Part 6 onwards we emit
assembly, so we need an **assembler** and a **linker** too. The good news is that the C++ compiler you
are about to install contains both.

---

## 1. What we need, and why

| Tool | Used for | Needed from |
|------|----------|-------------|
| A C++17 compiler | building our compiler | Chapter 4 |
| An assembler (`as`, via `gcc`) | turning our emitted `.s` files into object files | Chapter 52 |
| A linker (`ld`, via `gcc`) | turning object files into executables | Chapter 52 |
| `objdump` or `dumpbin` | looking at machine code we produced | Chapter 47 (optional) |
| A text editor | everything | now |

Three viable setups. Pick **one**.

### Option A — Windows with g++ (recommended for this book)

We recommend the **WinLibs** build of MinGW-w64, because it is a single ZIP with no installer, it
includes `g++`, `gcc`, `as`, `ld`, `objdump`, `gdb`, and it targets Windows natively.

1. Go to <https://winlibs.com/> and download the latest **UCRT runtime, Win64, GCC** archive (the
   "with POSIX threads" variant is fine; pick the `.zip`).
2. Extract it to `C:\mingw64` — so that `C:\mingw64\bin\g++.exe` exists. Do not put it in a path with
   spaces.
3. Add `C:\mingw64\bin` to your `PATH`:
   * Press Windows, type *environment variables*, open **Edit the system environment variables**.
   * **Environment Variables…** → under *User variables* select `Path` → **Edit** → **New** →
     `C:\mingw64\bin` → OK, OK, OK.
4. **Close every open terminal** (PATH is read when a terminal starts) and open a new one.
5. Check it:

   ```bat
   g++ --version
   ```

   You should see something like `g++ (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders) 14.2.0`.
   Any version 9 or newer is fine; 11 or newer is comfortable.

### Option B — Windows with Visual Studio

1. Install **Visual Studio Community** (free) or just the **Build Tools for Visual Studio**. In the
   installer, tick **Desktop development with C++**.
2. Use the **x64 Native Tools Command Prompt for VS 2022** from the Start menu — not a plain
   `cmd`, because the tools need environment variables that shortcut sets up.
3. Check it: `cl` should print a version banner.
4. Build with `build_msvc.bat` instead of `build.bat` everywhere in this book.

One caveat: MSVC's assembler is `ml64.exe` and uses **MASM** syntax, while we emit **GAS/AT&T-family
Intel syntax** in Chapter 52. If you take Option B, install MinGW as well (Option A) just for
assembling, or use the bytecode backend (Chapter 54) which needs no assembler at all. Chapter 52 says
exactly what to do.

### Option C — Linux, macOS or WSL

```bash
# Debian / Ubuntu / WSL
sudo apt update && sudo apt install build-essential gdb

# Fedora
sudo dnf install gcc-c++ gdb binutils

# macOS (installs clang as `g++`)
xcode-select --install
```

Then use `make` instead of `build.bat`:

```bash
make ch04_tiny_compiler
./bin/ch04_tiny_compiler
```

On macOS note that the system assembler and linker differ from GNU's (Mach-O rather than ELF); Chapter
52 covers the three object formats and what changes. Everything up to Chapter 51 is fully portable.

---

## 2. An editor

Anything works, but **VS Code** is what this book assumes for the tooling chapters (63 builds a
language server for it).

1. Install VS Code.
2. Install the extension **C/C++** by Microsoft (IntelliSense, go-to-definition).
3. Open the `compiler_design` folder: `File → Open Folder…`.
4. Useful while reading: `Ctrl+Shift+V` opens a Markdown preview, so the book renders with its tables
   and diagrams. `Ctrl+P` then a filename jumps anywhere.

Two settings that pay for themselves in this project:

```json
// .vscode/settings.json
{
  "files.trimTrailingWhitespace": true,
  "editor.rulers": [100],
  "C_Cpp.default.cppStandard": "c++17",
  "C_Cpp.default.includePath": ["${workspaceFolder}/include"]
}
```

The last line is the important one — without it, IntelliSense will not find `#include "pebble/lexer.h"`
and will underline half the book in red.

---

## 3. Build your first chapter

From a terminal in the `compiler_design` folder:

```bat
run ch04_tiny_compiler
```

That is a two-step shortcut. It runs `build.bat ch04_tiny_compiler`, which invokes

```
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude chapters\ch04_tiny_compiler.cpp -o bin\ch04_tiny_compiler.exe
```

and then runs the produced `.exe`. Every flag has a reason:

| Flag | Meaning | Why we want it |
|------|---------|----------------|
| `-std=c++17` | use the C++17 language | we rely on `std::variant`, `std::optional`, `string_view`, structured bindings, `if constexpr` |
| `-O2` | optimise the generated code | our compiler processes a lot of small objects; `-O0` can be 5× slower. Use `-O0 -g` when debugging |
| `-Wall -Wextra` | turn on warnings | in compiler code, a forgotten `switch` case is a *silent wrong answer*. Warnings catch them |
| `-Wno-unused-parameter` | except this one | visitor methods legitimately ignore parameters |
| `-Iinclude` | where to find headers | lets us write `#include "pebble/lexer.h"` from anywhere |
| `-o bin\x.exe` | output name | keeps binaries out of the source tree |

Two more you will want later:

| Flag | When |
|------|------|
| `-g` | always, while debugging: puts debug info in the binary so `gdb` can show source |
| `-fsanitize=address,undefined` | when you get a mysterious crash. Catches buffer overruns and UB at run time. Not supported by MinGW; use WSL or Linux for this |

### Building everything

```bat
build
```

with no argument builds every `chapters\*.cpp`. Expect this to take a minute or two once the book is
complete. It is the quickest way to check that a change to a header did not break an earlier chapter —
do it before every commit.

---

## 4. Verify the whole toolchain, including the assembler

Part 6 needs more than a C++ compiler. Let us check now, so it is not a surprise in forty chapters.
Create a file `hello.s` somewhere scratch:

```asm
# hello.s — assembly, written by hand this once. Chapter 47 explains every line.
        .intel_syntax noprefix
        .globl  main
        .section .text
main:
        sub     rsp, 40
        lea     rcx, [rip + msg]
        call    puts
        xor     eax, eax
        add     rsp, 40
        ret
        .section .rdata
msg:
        .asciz  "the assembler works"
```

Assemble, link and run it with `gcc` (which recognises `.s` and calls the assembler for you):

```bat
gcc hello.s -o hello.exe
hello.exe
```

Expected output: `the assembler works`.

On Linux, replace `lea rcx, ...` with `lea rdi, ...` and `.rdata` with `.rodata` — the calling
convention and section names differ, which is exactly the portability problem Chapter 48 deals with.

If that printed, you have everything the book needs. If it did not, do not worry yet: nothing before
Chapter 47 uses it.

---

## 5. Look inside a binary (optional, five minutes, very motivating)

```bat
objdump -d -M intel hello.exe | findstr /C:"<main>" /C:"call" /C:"ret"
```

You are looking at the bytes your CPU will execute, disassembled back into text. In Chapter 52 our
compiler produces those bytes. Being able to run `objdump` on your own output — and compare it with
`gcc -O2 -S` on the equivalent C — is the single most useful debugging habit in Part 6.

A related trick you will use constantly: <https://godbolt.org> (Compiler Explorer) shows the assembly
for any snippet in any compiler, side by side. When you wonder "how does a real compiler do this?",
that is where you look.

---

## 6. Sanity check the project layout

```
compiler_design/
├── docs/                 the book
├── include/pebble/       our library (header-only)
├── chapters/             chNN_*.cpp, one program per chapter
├── examples/             .peb files
├── tests/                the test suite
└── bin/                  created by the build; ignored by git
```

Two deliberate decisions:

**Header-only library.** Every part of the compiler lives in a `.h` under `include/pebble/`, and each
chapter program is a single `.cpp` that includes what it needs. This keeps the build trivial (one
command per chapter, no link step to explain, no stale object files) and lets you read a whole
subsystem in one file. Real compilers split into `.h`/`.cpp` pairs to get faster incremental builds —
Chapter 65 discusses when it starts to matter and how to make the switch.

**One program per chapter, not one growing program.** `chapters/ch07_lexer.cpp` stays exactly as it was
when you wrote it in Chapter 7, even though `include/pebble/lexer.h` keeps growing. So you can always
go back and run an earlier stage in isolation. The final, full compiler driver appears in
`chapters/ch61_pebblec.cpp`.

---

## 7. Common problems

**`g++` is not recognised as an internal or external command.**
PATH is wrong, or you did not restart the terminal. Check with `where g++` (Windows) or `which g++`.
If `where g++` finds nothing, re-do step 3 of Option A and open a *new* terminal.

**`g++: error: unrecognized command-line option '-std=c++17'`.**
Your g++ is ancient (pre-5.0), probably an old MinGW or a Cygwin stub. Install WinLibs as above and
make sure its `bin` comes *first* in PATH.

**`fatal error: pebble/lexer.h: No such file or directory`.**
You are not in the `compiler_design` folder, or you invoked `g++` by hand without `-Iinclude`. `cd`
into the project root and use `build.bat`.

**The build says `Access is denied` when writing `bin\...exe`.**
The program is still running. Close the console window that has it open.

**Antivirus deletes `bin\*.exe`, or blocks it.**
This happens with self-written executables, and even more in Chapter 59 where we write machine code
into memory and jump to it. Add an exclusion for the project folder.

**Everything builds but the program prints nothing.**
On Windows, `bin\x.exe` run from a graphical shell opens and closes instantly. Always run from a
terminal.

**MSVC: `error C2039: 'variant': is not a member of 'std'`.**
You forgot `/std:c++17`, or you are on VS 2015. Use `build_msvc.bat`, and VS 2019 or newer.

**Linux: `cannot execute: required file not found` when running `bin/x`.**
You built for a different architecture, or the file is not executable: `chmod +x bin/x`.

More in [Appendix B](appendix-b-troubleshooting.md), which grows as the book does.

---

## 8. Optional, but worth it

**A debugger.** `gdb bin\ch04_tiny_compiler.exe`, then `break main`, `run`, `next`, `print tok`. In
VS Code, the C/C++ extension gives you F5 debugging with a `launch.json`. There is one point in this
book — the register allocator — where stepping through beats printing, and several where it merely
saves an hour.

**Git.** The project is already a git repository. Commit after every chapter:

```bat
git add -A && git commit -m "chapter 7: the lexer core"
```

When Chapter 41's optimiser breaks Chapter 39's interpreter, `git diff` is how you find out what you
changed. A compiler is the kind of program where bisecting your own history genuinely pays.

**`clang-format`.** Included with LLVM; keeps the formatting consistent so diffs stay small. A
`.clang-format` with `BasedOnStyle: LLVM`, `ColumnLimit: 100` matches this book.

---

## Check yourself

1. Why does this book use `-Wall -Wextra` rather than default warnings, given that warnings are not
   errors?
2. What does `-Iinclude` change, exactly, and why can we then write `#include "pebble/lexer.h"` from a
   file in `chapters/`?
3. We install a C++ compiler, but Chapter 52 needs an assembler. Where does it come from?
4. Why is the library header-only, and what is the cost of that choice?

<details>
<summary>Answers</summary>

1. Because the most common compiler-writing bug is *an unhandled case* — a new AST node kind added in
   Chapter 29 that a `switch` in Chapter 35 does not know about. `-Wall` includes
   `-Wswitch`, which reports exactly that for `enum class` switches. The warning is the difference
   between a compile-time note and a wrong program.
2. It adds the `include` directory to the list of places searched for `#include "..."` and
   `#include <...>`. Without it, quoted includes are resolved relative to the *including file*, so
   `chapters/x.cpp` would need `#include "../include/pebble/lexer.h"` — brittle, and different for
   every directory.
3. From the same package. GCC distributions include the GNU binutils: `as` (assembler), `ld`
   (linker), `objdump`, `nm`, `ar`. Invoking `gcc file.s` runs `as` and then `ld` for you.
4. Simplicity: one compile command per chapter, no link step, no build system to learn, and a whole
   subsystem readable in one file. The cost is compile time — every chapter recompiles every header it
   includes, so a one-line change to `ast.h` rebuilds everything that uses it. In a real project that
   becomes intolerable (Chapter 65).
</details>

[Next: Chapter 2 — C++ for compiler writers →](02-cpp-for-compiler-writers.md)
