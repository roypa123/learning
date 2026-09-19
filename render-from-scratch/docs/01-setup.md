# Chapter 1 — Setting up: compiler, editor, building

[← Introduction](00-introduction.md) · [Contents](README.md) · [Next: C++ crash course →](02-cpp-crash-course.md)

---

## Goal

* Install a C++ compiler on Windows (or check you already have one).
* Understand what "compiling" means.
* Build and run the chapter programs with one command.
* Know where the output images go and how to look at them.

---

## 1. What is a compiler?

You write C++ as **text** (`.cpp` and `.h` files). The processor in your computer can't read text;
it runs **machine code**, which is binary instructions. A **compiler** translates one into the
other:

```
  ch03_first_image.cpp  ──▶  [ g++ compiler ]  ──▶  ch03_first_image.exe  ──▶  run it  ──▶  images\ch03_gradient.bmp
       (text you write)                               (program)                            (the result)
```

C++ is a *compiled* language. That makes it one of the fastest languages there is, which matters a
lot for rendering: a movie frame needs **billions** of calculations.

There are three popular C++ compilers:

| Compiler | Where it comes from | Command |
|----------|--------------------|---------|
| **GCC** (`g++`) | Free, open source. On Windows it comes in "MinGW" packages. | `g++` |
| **MSVC** | Microsoft, part of Visual Studio | `cl` |
| **Clang** | Free, open source (LLVM) | `clang++` |

This book works with all three. The instructions below use **GCC via WinLibs** because it is a
simple download with nothing else to configure.

---

## 2. Installing GCC on Windows (recommended)

### Option A: with `winget` (one command)

Open **PowerShell** or **Command Prompt** and type:

```bat
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

Then **close and reopen** the terminal (so it picks up the new PATH) and check:

```bat
g++ --version
```

You should see something like:

```
g++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders) 14.2.0
```

Any version **9 or newer** works (we need C++17).

### Option B: manual download

1. Go to **https://winlibs.com**.
2. Download the latest **UCRT runtime, Win64, zip** release (for example
   `winlibs-x86_64-posix-seh-gcc-14.x.x-...-ucrt-....zip`).
3. Unzip it to a simple path, for example `C:\mingw64`.
4. Add `C:\mingw64\bin` to your **PATH**:
   * Press the Windows key, type **"environment variables"**, open *Edit the system environment
     variables* → *Environment Variables...*
   * Under *User variables*, select `Path` → *Edit* → *New* → type `C:\mingw64\bin` → OK.
5. Open a **new** terminal and run `g++ --version`.

> **Why "posix"?** We use `std::thread` for multithreading (chapter 21). The *posix* thread model
> of MinGW supports it out of the box.

### Option C: Visual Studio (MSVC)

If you already use Visual Studio:

1. Open the **Visual Studio Installer** → *Modify* → tick **"Desktop development with C++"** → install.
2. From the Start menu, open **"x64 Native Tools Command Prompt for VS"**.
3. `cd` into the `render-from-scratch` folder and use `build_msvc` instead of `build`.

> Having Visual Studio installed is **not enough** by itself: the C++ workload must be ticked.
> If `cl` says *"not recognized"*, you are not in the Native Tools prompt or the workload is missing.

### Linux and macOS

* Linux: `sudo apt install g++ make` (Debian/Ubuntu) or your distribution's equivalent.
* macOS: `xcode-select --install` gives you `clang++`. Use `make` (the Makefile uses `g++`; run
  `make CXX=clang++`).

---

## 3. An editor

Any text editor works. **Visual Studio Code** is a good choice:

1. Install from https://code.visualstudio.com.
2. Install the **C/C++** extension (by Microsoft).
3. *File → Open Folder...* → choose `render-from-scratch`.
4. Open a terminal inside VS Code with **Ctrl + `** (backtick).

VS Code can also show Markdown files (like this book) with images: open a `.md` file and press
**Ctrl+Shift+V**.

---

## 4. The project layout

```
render-from-scratch/
├── include/pixel/     our library: only .h files ("header-only")
├── chapters/          chXX_name.cpp - one program per chapter
├── docs/              this book
├── images/            output images (created automatically)
├── models/            3D model files (created in chapter 25)
├── bin/               compiled programs (created by build.bat)
├── build.bat          compile with g++
├── build_msvc.bat     compile with Visual Studio
├── run.bat            compile ONE chapter and run it
└── Makefile           for make (Linux/macOS/MSYS2)
```

### Why "header-only"?

A C++ program is often split into `.h` (declarations) and `.cpp` (definitions) files that are
compiled separately and then *linked*. That's powerful for huge projects, but it means more build
setup. Our library puts **everything in headers**, marked `inline` where needed, so each chapter
program is a single `.cpp` file that you compile with one command. Simple!

---

## 5. Building and running

Open a terminal **in the `render-from-scratch` folder** (important: the programs write to
`images\` relative to where you run them).

### Build and run one chapter

```bat
run ch03_first_image
```

`run.bat` does two things:

```bat
call build.bat ch03_first_image        :: compile chapters\ch03_first_image.cpp -> bin\ch03_first_image.exe
bin\ch03_first_image.exe               :: run it
```

### Build every chapter

```bat
build
```

### What the compile command means

Inside `build.bat` the real work is this line:

```bat
g++ -std=c++17 -O2 -Wall -Iinclude -pthread chapters\ch03_first_image.cpp -o bin\ch03_first_image.exe
```

| Part | Meaning |
|------|---------|
| `g++` | the compiler |
| `-std=c++17` | use the C++17 language standard (we need `std::filesystem`, among others) |
| `-O2` | **optimize**. Makes renders 5–20 times faster. Never benchmark without it! |
| `-Wall` | show all common warnings (warnings help you find bugs) |
| `-Iinclude` | "look for `#include` files in the `include` folder", so `#include "pixel/pixel.h"` works |
| `-pthread` | enable threads (`std::thread`) |
| `chapters\ch03...cpp` | the input file |
| `-o bin\...exe` | the output program |

### Passing options to a chapter

Some chapters have a quick default setting and a slow, high-quality one:

```bat
run ch21_first_masterpiece          :: quick preview
run ch21_first_masterpiece final    :: big and clean, takes much longer
```

---

## 6. Looking at the images

| Format | How to open it on Windows |
|--------|---------------------------|
| `.bmp` | Double-click: Photos, Paint, anything |
| `.png` | Double-click: Photos, any browser, VS Code |
| `.gif` | Any web browser (animated) |
| `.ppm` | Not supported by Windows Photos. Use GIMP, IrfanView, or VS Code with an image-preview extension. (From chapter 5 we also save BMP/PNG versions.) |

**Tip:** keep the `images` folder open in a file explorer window with the view set to *Large icons*.
Every time you run a chapter, the new images appear there.

---

## 7. Your first build: a smoke test

Let's make sure everything works:

```bat
cd path\to\render-from-scratch
run ch03_first_image
```

Expected output:

```
Building ch03_first_image ...
OK: bin\ch03_first_image.exe

Running ch03_first_image ...
Wrote images/ch03_gradient.ppm
Wrote images/ch03_gradient.bmp  (double-click it!)
```

Now open `images\ch03_gradient.bmp`.

> **Image description:** A 256 × 256 square. The top-left corner is dark blue, the top-right
> is purple-red, the bottom-left is green-teal, and the bottom-right is yellow-orange. The colors
> blend smoothly across the square with no visible bands.

If you see that, you're ready. 🎉

---

## Common problems

| Problem | Cause | Fix |
|---------|-------|-----|
| `'g++' is not recognized...` | PATH not set, or terminal was open before installing | Reopen the terminal; check PATH (section 2) |
| `fatal error: pixel/pixel.h: No such file or directory` | Wrong folder, or `-Iinclude` missing | Run `build`/`run` from inside `render-from-scratch` |
| `error: 'filesystem' is not a member of 'std'` | Very old GCC (< 8) | Install a current WinLibs |
| Image not created | Program ran from a different folder | Run from `render-from-scratch`; look for `images\` there |
| Program is *very* slow | Built without `-O2` (for example in a debugger) | Use `build.bat`, which has `-O2` |
| Antivirus deletes the `.exe` | Some antivirus tools distrust fresh unsigned programs | Add the `bin` folder as an exception |
| `undefined reference to pthread...` | Missing `-pthread` | Use the provided build scripts |

---

## Summary

* A compiler turns C++ text into a program. We use `g++` (or MSVC).
* `run chXX_name` compiles and runs a chapter; results go to `images\`.
* Always build with optimizations (`-O2`), because rendering is heavy work.

Next: [a crash course in the C++ we'll use →](02-cpp-crash-course.md)
