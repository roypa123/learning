# Chapter 1 — Setting up the toolchain

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: The x86 machine →](02-the-x86-machine.md)

---

## Goal

Three tools on your `PATH`, and a five-line assembly program booting in an emulator. By the end of
this chapter you will have watched a machine execute code you wrote, with no operating system
underneath it, and you will know that every piece of the chain works — which matters, because when
Chapter 5's boot sector produces a blank screen you want to be certain the blankness is your bug and
not your setup.

The three tools:

| Tool | What it does | Why we cannot use the obvious alternative |
|---|---|---|
| **NASM** | Assembles x86 assembly | GAS works too, but its AT&T syntax makes every Intel manual harder to follow |
| **i686-elf-gcc** | Compiles C for bare metal | Your system GCC produces code that *requires* an operating system. §3 explains. |
| **QEMU** | Emulates a PC | Real hardware takes 30 seconds per attempt and tells you nothing |

---

## 1. The shape of the problem

Here is what has to happen to turn `kernel.c` into bytes on a disk that a CPU will execute:

```
kernel.c  --[compiler]-->  kernel.o   (ELF object, i386 machine code, unresolved symbols)
boot.asm  --[assembler]->  boot.bin   (flat binary, exactly 512 bytes)
entry.asm --[assembler]->  entry.o    (ELF object)

kernel.o + entry.o + link.ld  --[linker]-->  kernel.elf  (addresses assigned)
kernel.elf --[objcopy]--> kernel.bin  (headers stripped; just the bytes)

boot.bin + stage2.bin + kernel.bin  --[mkimage]-->  spark.img
spark.img --[QEMU]--> a machine that boots it
```

Six programs, and the only unusual one is the third: a linker that will place our code at an address
*we* choose rather than an address an operating system chooses. That is what `link.ld` is for, and
Chapter 9 explains it fully.

---

## 2. Installing on Windows

This is the path the repository's `build.bat` and `run.bat` assume.

### 2.1 NASM

Download the Windows installer from [nasm.us](https://www.nasm.us/) (follow *stable* → the latest
version → `win64`), or with a package manager:

```powershell
winget install NASM.NASM
```

The installer does **not** add NASM to `PATH`. Either add `C:\Program Files\NASM` through *System
Properties → Environment Variables*, or for a single terminal session:

```bat
set PATH=%PATH%;C:\Program Files\NASM
```

Check it:

```bat
nasm -v
```

You want version 2.14 or later. Anything from the last decade is fine.

### 2.2 QEMU

```powershell
winget install SoftwareFreedomConservancy.QEMU
```

or the installer from [qemu.weilnetz.de](https://qemu.weilnetz.de/w64/). Again, `PATH` is not set for
you:

```bat
set PATH=%PATH%;C:\Program Files\qemu
qemu-system-i386 --version
```

### 2.3 The cross-compiler

This is the one with no one-line installer, and §3 explains why it is worth the trouble. Three
options, easiest first.

**Option A — a prebuilt binary (recommended).**
[github.com/lordmilko/i686-elf-tools](https://github.com/lordmilko/i686-elf-tools/releases) publishes
Windows builds of the whole GCC/binutils suite. Download `i686-elf-tools-windows.zip`, unzip it
somewhere permanent such as `C:\i686-elf`, and add `C:\i686-elf\bin` to `PATH`.

**Option B — MSYS2.**

```bash
pacman -S mingw-w64-x86_64-toolchain base-devel gmp-devel mpfr-devel mpc-devel
```

then build binutils and GCC from source following §2.5. Slower to set up, but you also get `make`,
`dd` and a usable shell, which the `Makefile` wants.

**Option C — WSL.** If you have WSL2, everything is one command away:

```bash
sudo apt install build-essential nasm qemu-system-x86 gcc-multilib xorriso
```

and then build the cross-compiler with §2.5. Work inside the WSL filesystem, not `/mnt/c`, or the
build will take four times as long.

Check it:

```bat
i686-elf-gcc --version
i686-elf-ld --version
```

### 2.4 Verify everything at once

The repository has a target for this:

```bat
cd operating_system
build check
```

```
Checking the toolchain...

  ok        NASM assembler
  ok        i686-elf cross compiler
  ok        i686-elf linker
  ok        i686-elf objcopy
  ok        QEMU PC emulator

Anything marked MISSING is explained in docs\01-setup.md
```

### 2.5 Building the cross-compiler from source

Only needed for options B and C. It takes 20–40 minutes and is entirely mechanical.

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

mkdir -p ~/src && cd ~/src
curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.gz
curl -O https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar xf binutils-2.42.tar.gz && tar xf gcc-13.2.0.tar.gz

mkdir build-binutils && cd build-binutils
../binutils-2.42/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && make install
cd ..

cd gcc-13.2.0 && ./contrib/download_prerequisites && cd ..
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

Two flags in there are the whole point and are worth understanding:

**`--target=i686-elf`** says "produce code for a 32-bit x86 machine, in ELF format, with no operating
system". The `elf` where you might expect `linux` or `windows` is the significant part: there is no
OS name because there is no OS.

**`--without-headers`** says "there is no C library; do not look for one, and do not assume any of its
functions exist". Without it, `configure` looks for `stdio.h`, fails, and stops.

**`all-target-libgcc`** builds the one library we *do* link against. More on that in §4.

---

## 3. Why a cross-compiler is not optional

This is the part people skip and then spend a day paying for. The short version: **your system
compiler produces programs, and we are not writing a program.**

Try it. On a 64-bit Linux box:

```bash
gcc -c kernel.c -o kernel.o
```

Four separate things go wrong, and each teaches something.

### 3.1 It builds 64-bit code

Your GCC targets x86-64. The machine we are booting starts in 16-bit real mode and we switch it to
32-bit protected mode; it will not be in long mode. Every instruction would be wrong. `-m32` fixes
this one, if you have the multilib packages, and it is the only one of the four that has a flag.

### 3.2 It assumes a C library exists

```c
struct point { int x, y; };
void f(struct point *a, struct point *b) { *a = *b; }
```

No function calls in that code. Compile it and disassemble:

```
f:  push  ebp
    mov   ebp, esp
    push  8            ; size
    push  DWORD [ebp+12]
    push  DWORD [ebp+8]
    call  memcpy       ; <-- where did that come from?
```

GCC is allowed to turn a struct assignment into a `memcpy` call, and it does. It is also allowed to
turn a zero-filling loop into `memset`, and a character-counting loop into `strlen`. On a normal
system the C library supplies them. We have no C library.

`-ffreestanding` tells GCC that no hosted environment exists, which stops it assuming `main` is
special and stops some transformations — but it explicitly does **not** stop these four. The standard
requires a freestanding implementation to provide `memcpy`, `memmove`, `memset` and `memcmp`, so GCC
assumes they are there. That is precisely why [`nimbus/lib/string.c`](../nimbus/lib/string.c) exists
and why those four functions are not optional even if you never call them.

### 3.3 It produces the wrong object format

A Windows GCC emits PE-COFF. A macOS one emits Mach-O. Our linker script speaks ELF, our loader
parses ELF, and QEMU's `-kernel` reads ELF. Targeting `i686-elf` gets us ELF regardless of what the
host machine runs.

### 3.4 It links a runtime you cannot have

Left to itself, the linker adds `crt1.o`, `crti.o`, `crtbegin.o` and friends, which set up a stack
frame, call into the dynamic loader, initialise the C library, run static constructors, call `main`,
and call `exit`. Every one of those steps needs an operating system.

`-nostdlib` turns it off, and then `_start` is our own code — the first instruction of the kernel,
running on a machine where nothing has been set up.

### 3.5 The flags we use, and what each one prevents

From the [Makefile](../Makefile):

```make
CFLAGS = -std=gnu11 -m32 -ffreestanding -nostdlib -fno-builtin \
         -fno-stack-protector -fno-pic -fno-omit-frame-pointer \
         -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-red-zone \
         -Wall -Wextra -Wno-unused-parameter -O2 -g \
         -Inimbus/include
```

| Flag | Without it |
|---|---|
| `-ffreestanding` | GCC assumes a hosted environment and a standard library |
| `-nostdlib` | The linker pulls in crt0 and libc and fails |
| `-fno-builtin` | `memcpy` compiles into a call to `memcpy`, i.e. to itself, infinitely |
| `-fno-stack-protector` | Link error: `undefined reference to __stack_chk_fail` — the canary lives in thread-local storage we do not have |
| `-fno-pic` | GCC emits PIC that needs a Global Offset Table, which something must relocate. Nobody will. |
| `-mno-sse` etc. | GCC vectorises a struct copy with `movaps`, which faults because `CR4.OSFXSR` is clear and there is no FXSAVE area |
| `-mno-red-zone` | GCC uses 128 bytes below `ESP` as scratch; an interrupt pushes onto exactly that space and corrupts it |
| `-fno-omit-frame-pointer` | You lose stack traces in GDB, which you will want in Chapter 47 |
| `-g` | No source-level debugging. Costs nothing at runtime — DWARF lives in sections nothing loads |

The red zone one deserves a note even though it is a 64-bit ABI feature and therefore not strictly
active for us: it is the single most instructive item on the list. The ABI says a leaf function may
use the 128 bytes below the stack pointer without adjusting `ESP`, because nothing will ever write
there. That is true — in userland. In a kernel, an interrupt arrives, and the CPU pushes its frame at
exactly that address. The function's scratch data is gone and the corruption is invisible.

---

## 4. The one library we do link: libgcc

`-nostdlib` removes everything, including `libgcc`, and then this fails to link:

```c
uint64_t divide(uint64_t a, uint64_t b) { return a / b; }
```

with `undefined reference to __udivdi3`.

The 32-bit x86 has no instruction that divides a 64-bit number by a 64-bit number. GCC emits a call
to a helper function, and those helpers live in `libgcc`. The same goes for 64-bit modulo, some
shifts, and a few floating-point conversions.

Two options. Add `-lgcc` and let the linker find it:

```make
LDFLAGS = -L$(shell $(CC) -print-libgcc-file-name | xargs dirname) -lgcc
```

Or avoid 64-bit division entirely. Nimbus takes the second route, which is why
[`timer.c`](../nimbus/kernel/timer.c) is careful about where it divides, and why `printf`'s `%llu`
path is the only place 64-bit arithmetic appears. It keeps the link line to one line. Either is fine;
know which one you chose.

---

## 5. Proof that it all works

Five lines of assembly, into an emulated PC.

Create `hello.asm`:

```nasm
[BITS 16]
[ORG 0x7C00]

    mov ah, 0x0E            ; BIOS teletype output
    mov al, 'X'
    int 0x10                ; print AL at the cursor

    jmp $                   ; jump to this instruction, forever

times 510 - ($ - $$) db 0   ; pad to 510 bytes
dw 0xAA55                   ; the boot signature
```

Build and run:

```bat
nasm -f bin hello.asm -o hello.img
qemu-system-i386 -fda hello.img -boot a
```

A QEMU window opens, SeaBIOS prints its banner, and then there is an `X`.

That `X` is worth a moment. Between the character on screen and the transistors, there is nothing —
no kernel, no library, no runtime, no loader. The BIOS copied your 512 bytes to `0x7C00`, jumped to
the first one, and the CPU has been executing your instructions ever since. `jmp $` means the machine
is still in your loop right now.

### If something went wrong

| Symptom | Cause |
|---|---|
| "No bootable device" | The last two bytes are not `0x55 0xAA`. Check the `times` line assembled without error. |
| QEMU window never opens | QEMU is not on `PATH`; see §2.2 |
| `times value is negative` | Your code is over 510 bytes. Not possible with five lines — check for a stray `%include`. |
| A black screen and nothing else | The BIOS did not find the signature *or* your `ORG` is wrong. Add `-d int` and look for `check_int_eflags`. |

### A second, better test

The first test proves the assembler works. This one proves the *compiler* works, which is the part
most likely to be misconfigured:

```bat
cd operating_system
build spark
run spark
```

You should see Spark's boot messages and then its banner. If `build spark` fails, the error will name
the missing tool. If it succeeds and `run spark` shows a blank screen, that is a genuine bug and
Chapter 47 is the chapter you want — but it will not happen from a fresh checkout.

---

## 6. QEMU flags you will use constantly

From [`run.bat`](../run.bat) and the [Makefile](../Makefile):

| Flag | What it does |
|---|---|
| `-fda file` | Attach as the first floppy. Pair with `-boot a`. |
| `-drive file=x,format=raw` | Attach as a hard disk. The `format=raw` silences a warning and stops QEMU guessing. |
| `-kernel file.elf` | QEMU's own multiboot loader: reads the ELF, jumps to the entry point, no bootloader needed |
| `-initrd file` | Hand a file to the kernel as multiboot module 0 |
| `-m 128M` | RAM. Nimbus is happy from 32 MiB up. |
| `-serial stdio` | **The most useful flag in the list.** The kernel's serial output appears in your terminal. |
| `-serial file:log.txt` | ...or in a file you can grep |
| `-no-reboot` | Stop on a triple fault instead of rebooting forever, so the last screen stays visible |
| `-d int` | Log every interrupt and exception. Enormous output, and the fastest way to find a triple fault. |
| `-D file` | Send `-d` output to a file rather than the terminal |
| `-S -s` | Stop before the first instruction and listen for GDB on `:1234` |
| `-monitor stdio` | The QEMU monitor: `info registers`, `info mem`, `info tlb`, `xp /16x 0x7c00` |

`-serial stdio` and `-monitor stdio` both want the terminal, so you cannot use both. Use
`-serial stdio` and reach the monitor with Ctrl-Alt-2 in the QEMU window.

---

## 7. An editor set up for this

Not required, and worth ten minutes.

**Syntax highlighting for `.asm`.** VS Code: the *x86 and x86_64 Assembly* extension. NASM syntax,
not GAS.

**A linker script mode.** `.ld` files highlight as C in most editors, which is wrong but close
enough to be useful.

**Tell your editor about the include path**, or every `#include <nimbus/...>` shows as an error. For
VS Code, `.vscode/c_cpp_properties.json`:

```json
{
  "configurations": [{
    "name": "Nimbus",
    "includePath": ["${workspaceFolder}/nimbus/include"],
    "defines": ["NIMBUS_KERNEL=1", "__i386__"],
    "cStandard": "gnu11",
    "intelliSenseMode": "gcc-x86"
  }],
  "version": 4
}
```

**GDB.** If your cross-toolchain came with `i686-elf-gdb`, use it. A 64-bit `gdb` mostly works on
32-bit targets but gets confused about register widths at exactly the wrong moments. Chapter 47 has a
`.gdbinit` worth copying.

---

## 8. Exercises

🟢 **1.1** Change the `X` in `hello.asm` to print your name, one character at a time. You will need
a loop, `lodsb`, and to remember that `int 0x10` clobbers nothing you care about.

🟢 **1.2** Delete the `dw 0xAA55` line and run it. Read the error the BIOS gives, and note how little
it tells you. This is the failure mode you are signing up for.

🟡 **1.3** Run `i686-elf-gcc -m32 -ffreestanding -O2 -S` on a file containing only a struct
assignment, and find the `memcpy` call in the output. Then add `-fno-builtin` and see that it is
still there — this is the point at which most people realise `-fno-builtin` does not mean what they
assumed.

🟡 **1.4** Use `qemu-system-i386 -fda hello.img -boot a -d int -D log.txt` and read the log. Find the
BIOS's `int 0x13` calls reading the boot sector, and find your `int 0x10`. Getting comfortable with
this log now will save you an hour in Chapter 7.

🔴 **1.5** Build the cross-compiler from source even if you used a prebuilt one. Nothing teaches you
what a toolchain *is* like watching it be constructed — and when a flag misbehaves in Chapter 9 you
will know where to look.

---

## What we covered

- The six-program pipeline from source to a bootable image, and where each one fits.
- Why your system compiler cannot build a kernel: wrong width, assumes a C library, wrong object
  format, links a runtime that needs an OS.
- What each flag in `CFLAGS` prevents, concretely, including the four functions GCC will call whether
  or not you wrote them.
- `libgcc`, `__udivdi3`, and why 64-bit division is not free on a 32-bit machine.
- A five-line program running on bare metal, and the QEMU flags you will be using for the rest of
  the book.

[Chapter 2](02-the-x86-machine.md) describes the machine we are about to program: registers, the two
address spaces, what "real mode" actually means, and why an address on this CPU has been computed
three completely different ways over its lifetime.

---

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: The x86 machine →](02-the-x86-machine.md)
