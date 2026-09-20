# Compiler Design From Scratch: The Book

> *"A compiler is a program that reads a sentence nobody has ever written before,
> works out what it means, and then explains that meaning to a machine which has no
> idea what words are."*

Welcome. This book takes you from **zero** — never having written a compiler, maybe only average
C++ — to a working, native-code-generating compiler for a real language, and then on to designing a
language of your own.

Nothing is hidden. We use no `flex`, no `bison`, no ANTLR, no LLVM, no assembler library. Every stage
is code you write: the scanner, the parser, the type checker, the SSA optimiser, the register
allocator, the machine-code emitter, the garbage collector, even a JIT.

---

## How to use this book

* **Read in order.** Each chapter adds one piece to the same compiler. The early chapters are gentle;
  the later ones are demanding, but by then you will be ready.
* **Build and run every chapter.** Each chapter has a complete program in `chapters/`. Build and run
  it with `run <name>` and compare the output with the chapter text.
* **Do the "Try it yourself" exercises.** Breaking a compiler on purpose and reading its error message
  teaches more than any paragraph can.
* **Theory is never skipped, and never abstract.** When we need a theorem we state it, prove the part
  that matters, and then immediately write the loop that uses it.

Each chapter follows the same shape:

| Section | What it gives you |
|---------|-------------------|
| **Goal** | What you will be able to do by the end |
| **The idea** | The concept in plain language, with diagrams |
| **The theory** | The formal statement, only as deep as we need it |
| **The design** | Why we chose *this* representation and not another |
| **The code** | A walk-through, then the complete listing |
| **Run it** | The exact command to type |
| **What you should see** | The output, so you know it worked |
| **Try it yourself** | Experiments, from 2-minute to 2-hour |
| **Common problems** | What usually goes wrong, and why |
| **Check yourself** | Questions, with answers at the bottom |

Two kinds of page:

* **Chapters** (this folder) explain *ideas and algorithms*.
* **[Line-by-line pages](line-by-line/README.md)** explain *every line of every source file* in plain
  words. Read the chapter first, then the line-by-line page with the source open beside it.

---

## The language we build: Pebble

```pebble
struct Point { x: float, y: float }

fn dist2(a: Point, b: Point) -> float {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    return dx * dx + dy * dy;
}

fn main() -> int {
    let p = Point { x: 3.0, y: 4.0 };
    let q = Point { x: 0.0, y: 0.0 };
    print_float(dist2(p, q));      // 25
    return 0;
}
```

Statically typed. Compiled to native x86-64, and also to bytecode for a VM we write. The full
specification is [Appendix F](appendix-f-pebble-spec.md); the full grammar is
[Appendix D](appendix-d-grammar.md).

---

## Table of contents

### Part 0 — Foundations
* [Chapter 0 — Introduction: what a compiler really is](00-introduction.md)
* [Chapter 1 — Setting up: compiler, editor, building](01-setup.md)
* [Chapter 2 — C++ for compiler writers](02-cpp-for-compiler-writers.md)
* [Chapter 3 — The shape of a compiler: the whole pipeline](03-the-pipeline.md)
* [Chapter 4 — A tiny compiler in one file: arithmetic to x86-64](04-tiny-compiler.md)

### Part 1 — Lexical analysis: turning text into tokens
* [Chapter 5 — Source text: files, encodings, spans, line maps](05-source-text.md)
* [Chapter 6 — Theory: alphabets, languages, regular expressions](06-theory-regular-languages.md)
* [Chapter 7 — A hand-written lexer, part 1: the scanner core](07-lexer-part1.md)
* [Chapter 8 — A hand-written lexer, part 2: numbers, strings, comments](08-lexer-part2.md)
* [Chapter 9 — Keywords, identifiers, operators, maximal munch](09-keywords-and-operators.md)
* [Chapter 10 — Diagnostics: errors that teach](10-diagnostics.md)
* [Chapter 11 — Theory: finite automata, NFA to DFA](11-theory-automata.md)
* [Chapter 12 — Building a lexer generator from regular expressions](12-lexer-generator.md)
* [Chapter 13 — Testing and fuzzing the lexer](13-testing-the-lexer.md)

### Part 2 — Syntax: turning tokens into structure
* [Chapter 14 — Theory: context-free grammars](14-theory-grammars.md)
* [Chapter 15 — Designing the AST](15-designing-the-ast.md)
* [Chapter 16 — Recursive descent: statements and declarations](16-recursive-descent.md)
* [Chapter 17 — Pratt parsing: expressions, precedence, associativity](17-pratt-parsing.md)
* [Chapter 18 — Types in the grammar, and the full Pebble grammar](18-parsing-types.md)
* [Chapter 19 — Error recovery: how a parser survives bad input](19-error-recovery.md)
* [Chapter 20 — Theory: LL(1), FIRST, FOLLOW, predictive parsing](20-theory-ll1.md)
* [Chapter 21 — Theory: LR parsing, shift-reduce, the LALR automaton](21-theory-lr.md)
* [Chapter 22 — Building an LALR(1) table generator](22-lalr-generator.md)
* [Chapter 23 — Printing the AST: a formatter for Pebble](23-formatter.md)

### Part 3 — Semantic analysis: does it *mean* anything?
* [Chapter 24 — Scopes and symbol tables](24-scopes-and-symbols.md)
* [Chapter 25 — Name resolution](25-name-resolution.md)
* [Chapter 26 — Theory: types, judgments, inference rules](26-theory-types.md)
* [Chapter 27 — The type checker, part 1: expressions](27-typecheck-expressions.md)
* [Chapter 28 — The type checker, part 2: statements and functions](28-typecheck-statements.md)
* [Chapter 29 — Aggregates: structs, arrays, memory layout](29-aggregates-and-layout.md)
* [Chapter 30 — Type inference for locals, and a taste of Hindley–Milner](30-type-inference.md)
* [Chapter 31 — Control-flow checks: reachability, definite assignment](31-control-flow-checks.md)
* [Chapter 32 — Mutability, lvalues, addressability](32-mutability-and-lvalues.md)
* [Chapter 33 — Generics by monomorphisation](33-generics.md)

### Part 4 — Intermediate representation
* [Chapter 34 — Why an IR? Designing Pebble IR](34-why-an-ir.md)
* [Chapter 35 — Lowering: from AST to IR](35-lowering.md)
* [Chapter 36 — Basic blocks and the control-flow graph](36-cfg.md)
* [Chapter 37 — Theory: SSA form, dominance, phi nodes](37-theory-ssa.md)
* [Chapter 38 — Building SSA: dominance frontiers and renaming](38-building-ssa.md)
* [Chapter 39 — An IR interpreter, so we can trust the IR](39-ir-interpreter.md)

### Part 5 — Optimisation
* [Chapter 40 — Theory: data-flow analysis, lattices, fixpoints](40-theory-dataflow.md)
* [Chapter 41 — Constant folding, propagation, algebraic identities](41-constant-propagation.md)
* [Chapter 42 — Dead code elimination, CSE, copy propagation](42-dce-and-cse.md)
* [Chapter 43 — Loops: detection, LICM, strength reduction, unrolling](43-loop-optimisation.md)
* [Chapter 44 — The call graph: inlining and tail calls](44-inlining.md)
* [Chapter 45 — Memory: alias analysis, load/store optimisation](45-memory-optimisation.md)
* [Chapter 46 — Leaving SSA: phi elimination and critical edges](46-out-of-ssa.md)

### Part 6 — Code generation: down to the metal
* [Chapter 47 — The machine: x86-64 registers, memory, flags](47-the-machine.md)
* [Chapter 48 — Calling conventions and stack frames](48-calling-conventions.md)
* [Chapter 49 — Instruction selection](49-instruction-selection.md)
* [Chapter 50 — Register allocation, part 1: linear scan](50-regalloc-linear-scan.md)
* [Chapter 51 — Register allocation, part 2: graph colouring](51-regalloc-colouring.md)
* [Chapter 52 — Emitting assembly, assembling, linking](52-emitting-assembly.md)
* [Chapter 53 — Peephole optimisation and instruction scheduling](53-peephole.md)
* [Chapter 54 — A portable backend: bytecode and a VM](54-bytecode-vm.md)
* [Chapter 55 — The runtime: startup, printing, memory](55-the-runtime.md)
* [Chapter 56 — Garbage collection: mark–sweep and copying](56-garbage-collection.md)
* [Chapter 57 — Object files, symbols, relocations, the linker](57-object-files-and-linking.md)

### Part 7 — A real toolchain
* [Chapter 58 — Debug info, stack traces, sanitizers](58-debug-info.md)
* [Chapter 59 — A JIT: turning IR into memory you can call](59-jit.md)
* [Chapter 60 — A REPL for Pebble](60-repl.md)
* [Chapter 61 — Modules, imports, separate compilation](61-modules.md)
* [Chapter 62 — The Pebble standard library](62-standard-library.md)
* [Chapter 63 — Tooling: formatter, highlighter, language server](63-tooling-and-lsp.md)
* [Chapter 64 — Testing a compiler: golden files, differential testing, fuzzing](64-testing-a-compiler.md)
* [Chapter 65 — Making the compiler fast](65-compiler-performance.md)

### Part 8 — Create your own language
* [Chapter 66 — How to design a language: goals, non-goals, taste](66-designing-a-language.md)
* [Chapter 67 — Syntax design workshop](67-syntax-design.md)
* [Chapter 68 — Type system design: decisions and consequences](68-type-system-design.md)
* [Chapter 69 — Semantics design: memory, errors, concurrency](69-semantics-design.md)
* [Chapter 70 — Case study: building "Cobble" in a weekend](70-case-study-cobble.md)
* [Chapter 71 — Adding a feature end to end](71-feature-end-to-end.md)
* [Chapter 72 — Self-hosting: Pebble written in Pebble](72-self-hosting.md)
* [Chapter 73 — Shipping a language, and where to go next](73-shipping-and-next-steps.md)

### Appendices
* [Appendix A — The `pebble` library reference](appendix-a-library-reference.md)
* [Appendix B — Troubleshooting](appendix-b-troubleshooting.md)
* [Appendix C — Glossary](appendix-c-glossary.md)
* [Appendix D — The Pebble grammar (full EBNF)](appendix-d-grammar.md)
* [Appendix E — x86-64 quick reference](appendix-e-x86-64.md)
* [Appendix F — The Pebble language specification](appendix-f-pebble-spec.md)
* [Appendix G — Further reading, annotated](appendix-g-further-reading.md)
* [Appendix H — Full source listings](appendix-h-full-source.md)

---

## Three ways to read this book

**The complete path (recommended).** Chapters 0 to 73 in order. You finish with a native compiler and
a language of your own. Budget three to six months of evenings.

**The fast path to "it runs".** Chapters 0, 1, 3, 4, 5, 7, 8, 9, 10, 15, 16, 17, 24, 25, 27, 28, 34,
35, 39, 54. That gives you a working implementation of a real language in about two weeks, with no
machine code and no optimiser. Then come back for the rest.

**The theory path.** Chapters 6, 11, 14, 20, 21, 26, 37, 40, 46. These are the chapters a university
course would cover; each one still ends in running code, but they can be read on their own.

---

## What you need to know before starting

* C++ basics: functions, `struct`, `std::vector`, `std::string`, references, `for` loops.
* That is genuinely it. Chapter 2 teaches the rest — `std::unique_ptr`, `std::variant`, visitors,
  arenas, `enum class`, `std::string_view` — in the exact form this book uses.

You do **not** need assembly (Chapter 47 teaches it), formal language theory (Chapters 6 and 14 teach
it), or any previous compiler experience.

---

[Start with Chapter 0 →](00-introduction.md)
