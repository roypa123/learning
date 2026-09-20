# Compiler Design From Scratch

### Building a real programming language and its compiler in C++, with nothing hidden

This project is a complete, beginner-friendly course that teaches you how a **programming language**
works, from the first character of source text up to machine code that your CPU executes — and then
how to **design and ship a language of your own**.

We build our own compiler library, called `pebble`. It uses only the C++17 standard library. No
lexer generators, no parser generators, no LLVM, no assembler libraries. Every token, every parse
tree node, every optimisation, every byte of emitted machine code comes from code you will read,
understand and be able to change.

Along the way we design and implement a real language, also called **Pebble**:

```pebble
fn fib(n: int) -> int {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}

fn main() -> int {
    var i = 0;
    while i < 20 {
        print_int(fib(i));
        i = i + 1;
    }
    return 0;
}
```

By the end you will have:

* a hand-written **lexer** with real diagnostics (carets, line numbers, suggestions),
* a hand-written **parser** (recursive descent + Pratt) producing a typed AST,
* a **semantic analyser**: scopes, name resolution, a type checker, control-flow checks,
* your own **SSA-based intermediate representation** and an IR interpreter,
* an **optimiser**: constant propagation, DCE, CSE, LICM, inlining, and a data-flow framework,
* two **backends**: a portable bytecode VM and a native **x86-64** code generator with register
  allocation,
* a small **runtime** with a garbage collector,
* a **JIT** and a **REPL**,
* and a written method for designing your *own* language, applied to a second language you build
  from scratch in the final part.

---

## The layout

```
compiler_design/
├── README.md            <- you are here
├── docs/                <- THE BOOK: start with docs/README.md
│   └── line-by-line/    <- every source file explained line by line
├── include/pebble/      <- our compiler library (header-only C++17)
├── chapters/            <- one complete program per chapter
├── examples/            <- Pebble source files (.peb) used by the book
├── tests/               <- the compiler's test suite
├── build.bat            <- build with g++ (MinGW / WinLibs) on Windows
├── build_msvc.bat       <- build with Visual Studio's cl.exe
├── run.bat              <- build + run one chapter:  run ch07_lexer
└── Makefile             <- for Linux / macOS / MSYS2
```

## Quick start

1. Install a C++17 compiler — see [docs/01-setup.md](docs/01-setup.md) (about 10 minutes).
2. Open a terminal in this folder.
3. Run your first chapter:

   ```bat
   run ch04_tiny_compiler
   ```

   It compiles `2 + 3 * (10 - 4)` all the way to x86-64 assembly and prints every stage on the way.

4. Then start reading: [docs/README.md](docs/README.md).

## Read the book

The book is in [`docs/`](docs/). It is written to be read **in order**; each chapter builds the next
piece of the compiler. Two kinds of pages:

* **Chapters** (`docs/NN-*.md`) explain the *ideas*: the theory, the algorithm, the design choice,
  then the code.
* **Line-by-line pages** (`docs/line-by-line/*.md`) explain the *code*: every block, every line,
  in plain words.

If you have never written a compiler before, that is exactly who this book is for. Start at
[Chapter 0](docs/00-introduction.md).

## A note on the name

The language is called **Pebble** because it is small, hard, and you can pick it up. If you want your
language to be called something else, that is a *search and replace* away — and Part 8 of the book is
entirely about making that language yours.
