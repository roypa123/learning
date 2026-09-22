# Chapter 5 — Setting Up Your C++ Toolchain

> The first practical chapter. By the end you will have compiled and run a C++ program on your
> own machine. This is the chapter most likely to cause frustration, so it is written in
> exhaustive detail, with every error message you are likely to see and what it means.

---

## 5.1 What a compiler actually does

Before installing one, understand what you are installing. Beginners often treat the compiler
as a magic box, and then cannot interpret its complaints.

Your C++ source file is text. Your CPU executes machine code — binary instructions. Something
must translate. That translation happens in four distinct stages, and knowing their names lets
you locate an error instantly.

```
   hello.cpp        (text you wrote)
       |
       |  1. PREPROCESSOR
       |     Handles every line starting with #.
       |     #include <iostream>  -> literally pastes in the entire iostream file.
       |     #define PI 3.14159   -> textually replaces PI everywhere.
       v
   hello.ii         (one enormous text file, often 50,000+ lines)
       |
       |  2. COMPILER
       |     Parses C++, checks types, optimises, emits assembly.
       |     This is where syntax errors and type errors are reported.
       v
   hello.s          (assembly: human-readable machine instructions)
       |
       |  3. ASSEMBLER
       |     Turns assembly text into binary machine code.
       v
   hello.o          (object file: machine code with unresolved references)
       |
       |  4. LINKER
       |     Joins your object files with library code, resolves every
       |     function name to an actual address, produces a runnable file.
       v
   hello.exe        (a program you can run)
```

**Why this matters for reading errors.** Errors from stage 2 look like:

```
   hello.cpp:7:15: error: 'std::cot' has not been declared
```

They tell you a **file, a line, and a column**. These are usually easy: you typed something
wrong on line 7.

Errors from stage 4 look completely different:

```
   undefined reference to `writeWav(std::string, std::vector<float>)'
```

No line number, because linking happens after all files are compiled. This means: "you promised
this function exists, I believe you, but nobody ever wrote it" — typically a missing source file
in your compile command, a typo in a definition, or a missing library.

> **Diagnostic habit.** When an error appears, first ask: *is this a compile error or a link
> error?* Compile errors have file:line. Link errors say "undefined reference" or "unresolved
> external symbol". They have completely different causes and looking for the wrong one wastes
> hours. This single distinction will save you more time in the next year than any other piece
> of advice in this chapter.

---

## 5.2 Choosing a compiler on Windows

Three realistic options. I will recommend one and explain the others so you know what people
are talking about.

| Option | Command | Pros | Cons |
|---|---|---|---|
| **MSYS2 / MinGW-w64 GCC** ⭐ | `g++` | Small, fast to install, same commands as Linux/macOS, excellent standards support, great error messages | Separate package manager to learn |
| Visual Studio Build Tools | `cl` | Microsoft's own, best Windows debugging, required for some SDKs (ASIO, VST) | ~5 GB download, different command-line syntax |
| LLVM / Clang | `clang++` | Best error messages of all, sanitizers | On Windows it usually needs one of the above for its standard library |

**This book recommends MSYS2 with GCC.** Reasons: the download is a few hundred megabytes rather
than several gigabytes; every command in this book then works identically on Windows, macOS and
Linux; and the tooling (`gdb`, `make`, `cmake`, `pkg-config`) all comes from one place.

We will install Visual Studio Build Tools later, in Chapter 65, *if* you want to build VST3
plugins, which effectively require MSVC on Windows. Nothing before then needs it.

---

## 5.3 Installing MSYS2 and GCC — step by step

### Step 1 — Download

Go to **https://www.msys2.org** and download the installer, named something like
`msys2-x86_64-<date>.exe`.

### Step 2 — Install

Run it. Accept the default install location, `C:\msys64`. Do not put it in a path with spaces
in it — several build tools still handle spaces badly, and there is no benefit to fighting that.

When the installer finishes it offers to launch MSYS2. Let it.

### Step 3 — Update the package database

A terminal window opens. Type:

```bash
pacman -Syu
```

`pacman` is the package manager (the same one Arch Linux uses). `-Syu` means "sync the package
database and upgrade everything".

It may say it needs to close the terminal to update core packages. Let it close, then reopen
**MSYS2 UCRT64** from the Start menu and run the same command again until it reports nothing
left to do.

### Step 4 — Install the compiler toolchain

In the **MSYS2 UCRT64** terminal (the name matters — see the box below):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
```

Press Enter to accept the defaults when it lists packages. This installs:

- `gcc` — the compiler (which includes `g++` for C++)
- `gdb` — the debugger, which you will need in Chapter 7
- `cmake` — the build system generator, used from Chapter 17
- `make` — the build runner

> **Which MSYS2 terminal?** MSYS2 installs several shortcuts: MSYS, MINGW64, UCRT64, CLANG64.
> They are different *environments* producing different kinds of binaries.
>
> **Always use UCRT64.** It builds native Windows programs against the modern Universal C
> Runtime, which is what current Windows expects. The plain "MSYS" environment builds
> POSIX-emulation binaries and is not what you want for this book.

### Step 5 — Add the compiler to your Windows PATH

Right now `g++` only works inside the MSYS2 terminal. You want it available in VS Code and in
any Windows terminal.

1. Press <kbd>Win</kbd>, type `environment`, choose **Edit the system environment variables**.
2. Click **Environment Variables...**
3. Under **User variables**, select **Path**, click **Edit**.
4. Click **New**, and add:
   ```
   C:\msys64\ucrt64\bin
   ```
5. OK your way out of all three dialogs.
6. **Close every open terminal and VS Code window.** PATH is read when a process starts;
   existing ones will not see the change. This step is skipped constantly and causes the
   single most common "it says g++ is not recognised" problem.

### Step 6 — Verify

Open a **new** PowerShell or Git Bash window and run:

```bash
g++ --version
```

You should see something like:

```
g++.exe (Rev3, Built by MSYS2 project) 14.2.0
Copyright (C) 2024 Free Software Foundation, Inc.
```

The exact version does not matter as long as it is **11 or higher** — this book uses C++17
features, which GCC has supported fully since version 9.

Also check:

```bash
gdb --version
cmake --version
```

> **If `g++` is not recognised:**
> - Did you open a *new* terminal after editing PATH?
> - Does `C:\msys64\ucrt64\bin\g++.exe` actually exist? Check in Explorer. If not, step 4
>   installed into a different environment — reopen **UCRT64** specifically and repeat it.
> - Did you add the path under *User* variables, and click OK on every dialog rather than
>   cancelling out of the outer one?

---

## 5.4 Your first program

Create a folder for the book's code. If you are following this book's layout, that is
`C:\roy\learning\audio\code`. Inside it, create `ch05` and a file called `hello.cpp`.

**Code — `code/ch05/hello.cpp`**

```cpp
#include <iostream>

int main()
{
    std::cout << "The toolchain works.\n";
    return 0;
}
```

**Walkthrough — every character explained**

**Line 1: `#include <iostream>`**

The `#` marks a *preprocessor directive* — handled in stage 1 above, before the compiler proper
sees anything. `#include` means "paste the contents of this file here". `iostream` is a standard
library header containing the declarations for input/output streams. The angle brackets `<>`
mean "look in the system include directories"; quotes `"..."` would mean "look in my own project
folder first", which we will use for our own headers from Chapter 17.

After preprocessing, this one line has expanded into tens of thousands of lines. That is why
C++ compiles more slowly than you might expect, and why Chapter 17 cares about which headers go
where.

**Line 3: `int main()`**

`main` is special: it is where every C++ program begins. The operating system loads your
program and calls `main`. There is exactly one `main` in a program.

`int` is the *return type* — `main` hands an integer back to the operating system when it
finishes. `()` is the (empty) parameter list. We will add parameters in Chapter 14 when we want
command-line arguments.

**Line 4 and 7: `{` and `}`**

Braces delimit a *block* — a group of statements. Everything between them is the body of
`main`. C++ does not care about indentation (unlike Python); the braces define structure. We
indent for human readers only, and we do it consistently because inconsistent indentation hides
bugs.

**Line 5: `std::cout << "The toolchain works.\n";`**

Four things here.

- `std::` is a *namespace qualifier*. The standard library lives in a namespace called `std`, so
  its names do not collide with yours. `std::cout` means "the thing called `cout` inside `std`".
- `cout` is the standard output stream — "character out". Writing to it puts text in your
  terminal.
- `<<` is the *stream insertion operator*. Read it as an arrow: send the thing on the right into
  the stream on the left. (It is the same symbol as bit-shift, reused; C++ lets operators be
  redefined for new types, and the stream library does exactly that.)
- `"The toolchain works.\n"` is a *string literal*. The `\n` is an *escape sequence* meaning
  newline. Without it, your next output would continue on the same line.
- The `;` ends the statement. C++ requires it. A missing semicolon produces an error on the
  *following* line, which confuses everyone the first few times — if line 12 makes no sense,
  check line 11.

**Line 6: `return 0;`**

Hands 0 back to the operating system. By universal convention, **0 means success** and any
nonzero value means failure. (This is backwards from most people's intuition, and it is because
there is one way to succeed and many distinct ways to fail.) In `main` specifically, C++ lets
you omit this line and it is implied, but writing it is clearer.

### Compiling and running

Open a terminal in that folder. In VS Code: right-click the folder in the Explorer pane and
choose **Open in Integrated Terminal**.

```bash
g++ -std=c++17 -Wall -Wextra -O2 hello.cpp -o hello.exe
```

Then run it:

```bash
./hello.exe
```

You should see:

```
The toolchain works.
```

**That is the moment the rest of the book becomes possible.**

### The compiler flags, explained

You will type these hundreds of times, so understand them rather than copying them.

| Flag | Meaning |
|---|---|
| `-std=c++17` | Use the C++17 standard. Without this, GCC uses a default that may be older. This book uses C++17 features throughout. |
| `-Wall` | "Warn all" — enable common warnings. Despite the name it is not all of them. |
| `-Wextra` | More warnings. Together with `-Wall`, this is the sensible baseline. |
| `-O2` | Optimisation level 2. **This matters enormously for audio.** Unoptimised DSP code can be 5–20× slower — the difference between real-time and not. |
| `hello.cpp` | The input file. Multiple source files can be listed. |
| `-o hello.exe` | The output filename. Without it you get `a.exe`, which is useless once you have more than one program. |

Some others you will meet in this book:

| Flag | Meaning | Used in |
|---|---|---|
| `-g` | Include debug information so `gdb` can show you source lines | Chapter 7 |
| `-O0` | No optimisation — pair with `-g` for debugging, since `-O2` reorders code confusingly | Chapter 7 |
| `-lm` | Link the maths library (needed on Linux; harmless on Windows) | Chapter 10 |
| `-ffast-math` | Relax floating-point rules for speed. **Be careful** — it changes results, breaks NaN handling, and can alter filter behaviour | Chapter 63 |
| `-march=native` | Optimise for this exact CPU, enabling SIMD instructions | Chapter 63 |
| `-pthread` | Enable threading support | Chapter 60 |

> **Treat warnings as errors from day one.** Add `-Wall -Wextra` to every build, and when a
> warning appears, fix it rather than ignoring it. In audio, warnings about uninitialised
> variables, signed/unsigned comparison, and implicit conversions correspond directly to audible
> bugs: garbage samples, loops that never terminate, and silently truncated values. There is a
> stronger flag, `-Werror`, which refuses to compile if there is any warning at all. It is
> strict but it is a good habit; feel free to add it once you are comfortable.

---

## 5.5 Setting up VS Code

You are already in VS Code, so this is quick.

### Extensions

Install these from the Extensions pane (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>X</kbd>):

1. **C/C++** (by Microsoft) — syntax highlighting, IntelliSense completion, debugger
   integration.
2. **CMake Tools** (by Microsoft) — needed from Chapter 17 onward. Install it now.

### One-key building

Create a file at `code/.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "build current file",
      "type": "shell",
      "command": "g++",
      "args": [
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-O2",
        "-g",
        "${file}",
        "-o",
        "${fileDirname}/${fileBasenameNoExtension}.exe"
      ],
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": ["$gcc"],
      "detail": "Compile the file currently open in the editor"
    },
    {
      "label": "build and run current file",
      "type": "shell",
      "command": "${fileDirname}/${fileBasenameNoExtension}.exe",
      "dependsOn": "build current file",
      "group": { "kind": "test", "isDefault": true },
      "problemMatcher": []
    }
  ]
}
```

**What the variables mean:**

- `${file}` — the full path of the file currently open in the editor.
- `${fileDirname}` — the folder that file is in.
- `${fileBasenameNoExtension}` — the filename without `.cpp`, so `sine.cpp` becomes `sine.exe`.

**`"problemMatcher": ["$gcc"]`** is the useful part: it teaches VS Code to parse GCC's error
output, so errors appear underlined in your source with the message on hover, and
<kbd>F8</kbd> jumps between them.

Now <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> builds whatever file you have open. Open
`hello.cpp` and try it.

### Debugging

Create `code/.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug current file",
      "type": "cppdbg",
      "request": "launch",
      "program": "${fileDirname}/${fileBasenameNoExtension}.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${fileDirname}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "miDebuggerPath": "C:/msys64/ucrt64/bin/gdb.exe",
      "setupCommands": [
        {
          "description": "Enable pretty-printing for gdb",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        }
      ],
      "preLaunchTask": "build current file"
    }
  ]
}
```

Now <kbd>F5</kbd> builds and runs under the debugger. Click in the margin to the left of a line
number to set a breakpoint; execution stops there and you can inspect every variable.

We will use this seriously in Chapter 7. For now, just confirm it launches: set a breakpoint on
the `std::cout` line in `hello.cpp` and press <kbd>F5</kbd>. Execution should pause there.

> **Pretty-printing matters more than it sounds.** Without `-enable-pretty-printing`, hovering
> over a `std::vector<float>` in the debugger shows you internal pointers. With it, you see the
> actual values. When you are debugging a buffer full of samples, that is the difference between
> useful and useless.

---

## 5.6 A second program: proving the maths library works

Audio is arithmetic, so let us confirm floating-point maths and formatting behave.

**Code — `code/ch05/mathcheck.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <cmath>

int main()
{
    const double sampleRate = 44100.0;
    const double frequency  = 440.0;

    const double period        = 1.0 / frequency;
    const double samplesPerCycle = sampleRate / frequency;
    const double nyquist       = sampleRate / 2.0;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Sample rate      : " << sampleRate       << " Hz\n";
    std::cout << "Frequency        : " << frequency        << " Hz\n";
    std::cout << "Period           : " << period           << " s\n";
    std::cout << "Samples per cycle: " << samplesPerCycle  << "\n";
    std::cout << "Nyquist          : " << nyquist          << " Hz\n";

    const double pi = 3.14159265358979323846;
    std::cout << "sin(pi/2)        : " << std::sin(pi / 2.0) << "\n";
    std::cout << "sqrt(2)          : " << std::sqrt(2.0)     << "\n";
    std::cout << "1/sqrt(2) in dB  : " << 20.0 * std::log10(1.0 / std::sqrt(2.0)) << "\n";

    return 0;
}
```

**Expected output**

```
Sample rate      : 44100.000000 Hz
Frequency        : 440.000000 Hz
Period           : 0.002273 s
Samples per cycle: 100.227273
Nyquist          : 22050.000000 Hz
sin(pi/2)        : 1.000000
sqrt(2)          : 1.414214
1/sqrt(2) in dB  : -3.010300
```

**Walkthrough**

- `#include <cmath>` brings in `std::sin`, `std::cos`, `std::sqrt`, `std::log10`, `std::pow`,
  `std::fabs` and the rest of the C maths library, in the `std` namespace. Every remaining
  chapter uses it.
- `#include <iomanip>` provides *stream manipulators* — `std::fixed` and `std::setprecision`.
- `const` means "this value never changes after initialisation". Use it by default; it documents
  intent and lets the optimiser work harder. Chapter 6 covers it properly.
- `double` is a 64-bit floating-point number, about 15–16 significant decimal digits. Chapter 4
  explained why we use `double` for rates, frequencies and phase, and `float` for sample data.
- `std::fixed` forces decimal rather than scientific notation; `std::setprecision(6)` sets six
  digits after the point. Both are *sticky* — they affect all subsequent output on that stream,
  not just the next item. This surprises people.

**Notice `samplesPerCycle` is 100.227273, not a whole number.** This is the most important
observation in the program. A 440 Hz wave at 44.1 kHz does not fit a whole number of samples per
cycle, so cycles do not align with sample boundaries. Any oscillator design that assumes whole
cycles is broken, and Chapter 30 is essentially about handling this correctly with a
floating-point phase accumulator.

**And `1/sqrt(2)` is −3.01 dB.** That is the −3 dB point you have seen on filter diagrams: the
half-power point. Chapter 11 derives it.

---

## 5.7 Common errors and what they mean

A reference section. Come back when something goes wrong.

**`'g++' is not recognized as an internal or external command`**
PATH problem. See §5.3 step 5–6. Most often: you did not restart the terminal.

**`fatal error: iostream: No such file or directory`**
The compiler cannot find the standard library headers. Usually means you are running the MSYS2
`g++` from the wrong environment, or a stray `g++` from another installation (Strawberry Perl
and some Git distributions bundle one). Check with `where g++` in PowerShell — if more than one
appears, the first wins, and you may need to reorder your PATH.

**`error: 'cout' was not declared in this scope`**
Missing `std::`, or missing `#include <iostream>`.

**`error: expected ';' before '}' token`**
A missing semicolon, on the line *before* the one reported.

**`warning: unused variable 'x' [-Wunused-variable]`**
Harmless here, but often means you computed something and forgot to use it. In DSP that is
frequently a real bug — a filter state you never fed back, for example.

**`undefined reference to 'main'`**
You compiled a file with no `main`, or misspelled it (`Main`, `mian`). C++ is case-sensitive.

**`undefined reference to 'someFunction(float)'`**
Link error. You declared the function but never defined it, or you defined it in another `.cpp`
file that you forgot to list on the compile line. From Chapter 17 the CMake build handles this
for you.

**`error: no matching function for call to ...`** followed by fifty lines of template noise
A type mismatch. Read only the **first** line and the "candidate" lines; ignore the rest. C++
template errors are notoriously verbose, and learning to read only the top of them is a real
skill.

**Program compiles but prints nothing / crashes immediately**
Run it from a terminal rather than double-clicking, or the window closes before you can read
anything. If it crashes, that is Chapter 7's material — usually an out-of-bounds array access.

**Program runs but the `.wav` it writes is silent (from Chapter 9 onward)**
Nine times out of ten: you forgot to close the file, wrote the header with the wrong sizes, or
your samples are all zero because of an integer-division bug. Chapter 9 has a dedicated
checklist.

---

## 5.8 Optional: installing Visual Studio Build Tools

You do not need this until Chapter 65. Skip it for now; I am documenting it here so the
toolchain information stays in one place.

1. Download **Build Tools for Visual Studio** from Microsoft's site (the free standalone tools,
   not the full IDE).
2. In the installer, select **Desktop development with C++**.
3. Install. It is large — 5–7 GB.
4. Use it from the **Developer Command Prompt for VS**, where the `cl` compiler is on PATH.

The equivalent compile command is:

```
cl /std:c++17 /W4 /O2 /EHsc hello.cpp
```

| GCC | MSVC | Meaning |
|---|---|---|
| `-std=c++17` | `/std:c++17` | Language standard |
| `-Wall -Wextra` | `/W4` | Warning level |
| `-O2` | `/O2` | Optimise |
| `-o name.exe` | `/Fe:name.exe` | Output name |
| (implicit) | `/EHsc` | Standard C++ exception handling |

---

## 5.9 A note on cross-platform work

Everything in this book except this chapter is portable. If you later move to macOS or Linux:

**macOS:** install Xcode Command Line Tools with `xcode-select --install`, which gives you
`clang++`. Every `g++` command in this book works if you substitute `clang++`, and on macOS
`g++` is usually an alias for `clang++` anyway.

**Linux:** `sudo apt install build-essential gdb cmake` on Debian/Ubuntu. Commands are
identical. Drop the `.exe` from output names, and remember to add `-lm` for the maths library,
which GCC on Linux does not link automatically.

The only genuinely platform-specific chapters are 57 and 58 (audio device APIs) and 65
(plugins), and both flag their platform dependencies clearly.

---

## 5.10 Exercises

**5.1** Compile `hello.cpp` *without* `-o`. What is the output file called? Now compile
`mathcheck.cpp` without `-o` as well. What happened to the first program?

**5.2** Deliberately delete a semicolon from `hello.cpp` and compile. Write down the exact error
and which line it reports versus which line is actually wrong.

**5.3** Deliberately misspell `std::cout` as `std::cuot`. Is this a compile error or a link
error? How can you tell from the message alone?

**5.4** Write a program that declares a function `double midiToFreq(int note);`, calls it from
`main`, but never defines it. Compile. Which stage fails, and what does the message look like?
Now define it as `return 440.0 * std::pow(2.0, (note - 69) / 12.0);` and verify that MIDI note
69 gives 440 Hz. (Careful — there is an integer-division trap in that expression. Chapter 6
explains it; find it yourself first.)

**5.5** Modify `mathcheck.cpp` to print the samples-per-cycle for 20 Hz, 100 Hz, 440 Hz, 1 kHz,
and 20 kHz at 44,100 Hz. Which of these produce a whole number of samples per cycle? What does
that tell you about which test tones will loop seamlessly?

**5.6** Compile `mathcheck.cpp` with `-O0` and with `-O2`, and compare the file sizes of the two
executables. Then time a program that runs a million `std::sin` calls in a loop under each. (You
will need `<chrono>`; look it up, or wait for Chapter 63.)

---

### Chapter summary

- Compilation has four stages: preprocess, compile, assemble, link. **Compile errors have a
  file and line; link errors say "undefined reference".** Knowing which you are looking at is
  the single most useful debugging skill.
- Install MSYS2, use the **UCRT64** environment, install `gcc gdb cmake make`, add
  `C:\msys64\ucrt64\bin` to PATH, and open a **new** terminal.
- The standard build line for this book:
  `g++ -std=c++17 -Wall -Wextra -O2 file.cpp -o file.exe`
- `-O2` is not optional for audio: unoptimised DSP can be an order of magnitude too slow.
- Set up `tasks.json` and `launch.json` so <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> builds
  and <kbd>F5</kbd> debugs.
- Fix warnings rather than ignoring them; in audio they usually correspond to audible bugs.
- A 440 Hz tone at 44,100 Hz takes 100.227 samples per cycle — not a whole number. That single
  fact shapes every oscillator you will write.

**Next:** [Chapter 6 — C++ Crash Course, Part 1: The Language](06-cpp-crash-course-1.md)
