# Chapter 3 — The shape of a compiler: the whole pipeline

[← C++ for compiler writers](02-cpp-for-compiler-writers.md) · [Contents](README.md) · [Next: A tiny compiler →](04-tiny-compiler.md)

---

## Goal

Walk the pipeline a second time — but this time with the real type names, the real header files and the
real invariants of *our* compiler. When you finish this chapter you should be able to point at any file
in `include/pebble/` and say what it consumes, what it produces, and what it is forbidden to know.

This is the chapter to come back to whenever you are lost.

---

## 1. The data, not the code

The usual way to draw a compiler is as a row of boxes labelled with verbs: *lex*, *parse*, *check*.
That is the wrong emphasis. A compiler is better understood as a sequence of **representations**, with
translations between them. The representations are the design; the translations are just loops.

So here is the pipeline drawn as data:

```
   ┌───────────────────────────────────────────────────────────────────────┐
   │ 1. TEXT            SourceFile          "fn main() -> int { ... }"    │  Ch 5
   │                    one owned std::string + a line map                 │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  Lexer                                   Ch 7-9
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 2. TOKENS          std::vector<Token>   [kw_fn][ident main][lparen]…  │
   │                    flat, no nesting, every one with a Span            │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  Parser                                  Ch 16-19
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 3. AST             ast::Module           tree of unique_ptr nodes      │
   │                    nesting recovered; names still just strings        │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  Resolver                                Ch 24-25
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 4. RESOLVED AST    same tree + Symbol* on every name                  │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  TypeChecker                             Ch 26-33
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 5. TYPED AST       same tree + Type* on every expression              │
   │                    ← the last representation a human would recognise  │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  Lowering                                Ch 35
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 6. IR              ir::Module: functions → blocks → instructions      │
   │                    three-address, flat, in SSA form (Ch 38)           │
   └───────────────────────────────┬───────────────────────────────────────┘
                                   │  Optimisation passes                     Ch 40-46
   ┌───────────────────────────────▼───────────────────────────────────────┐
   │ 7. BETTER IR       same shape, fewer and cheaper instructions          │
   └───────────────┬───────────────────────────────┬───────────────────────┘
                   │ Instruction selection          │ Bytecode emitter       Ch 49 / 54
   ┌───────────────▼───────────────┐   ┌────────────▼──────────────────────┐
   │ 8a. MACHINE IR (virtual regs) │   │ 8b. BYTECODE  vector<uint8_t>     │
   └───────────────┬───────────────┘   └────────────┬──────────────────────┘
                   │ Register allocation  Ch 50-51   │ the VM                Ch 54
   ┌───────────────▼───────────────┐   ┌────────────▼──────────────────────┐
   │ 9a. ASSEMBLY TEXT  .s file    │   │ 9b. it just runs                  │
   └───────────────┬───────────────┘   └───────────────────────────────────┘
                   │ assembler + linker (Ch 52, 57)
   ┌───────────────▼───────────────┐
   │ 10. EXECUTABLE   a.exe        │
   └───────────────────────────────┘
```

Ten representations. Every chapter of this book either *defines* one of them or *writes the translation*
between two of them. That is the whole book on one page.

---

## 2. The files

Each representation gets a header; each translation gets a header. That is the entire design of
`include/pebble/`:

| Header | Defines | Kind | Chapter |
|--------|---------|------|---------|
| `source.h` | `SourceFile`, `Span`, `LineMap` | representation 1 | 5 |
| `diag.h` | `Diagnostic`, `Diagnostics` | crosses everything | 10 |
| `token.h` | `TokenKind`, `Token` | representation 2 | 7 |
| `lexer.h` | `Lexer` | 1 → 2 | 7–9 |
| `ast.h` | `ast::Expr`, `ast::Stmt`, `ast::Decl`, `ast::Module` | representation 3 | 15 |
| `parser.h` | `Parser` | 2 → 3 | 16–19 |
| `printer.h` | `print_ast`, `format_source` | 3 → text | 23 |
| `symbols.h` | `Symbol`, `Scope`, `SymbolTable` | representation 4 | 24 |
| `types.h` | `Type`, `TypeKind`, `TypeContext` | representation 5 | 26 |
| `sema.h` | `Resolver`, `TypeChecker` | 3 → 4 → 5 | 25, 27–33 |
| `ir.h` | `ir::Value`, `Instruction`, `BasicBlock`, `Function`, `Module` | representation 6 | 34 |
| `lower.h` | `Lowerer` | 5 → 6 | 35 |
| `cfg.h` | `CFG`, `DominatorTree`, `LoopInfo` | analysis of 6 | 36, 43 |
| `ssa.h` | `to_ssa`, `from_ssa` | 6 → 6 | 38, 46 |
| `dataflow.h` | the generic fixpoint solver | analysis of 6 | 40 |
| `opt.h` | every optimisation pass | 6 → 7 | 41–45 |
| `interp.h` | `Interpreter` | runs 6 | 39 |
| `bytecode.h` | `Chunk`, `VM` | 7 → 8b → run | 54 |
| `x64.h` | `MachineFunction`, `emit_asm` | 7 → 8a → 9a | 47–49, 52 |
| `regalloc.h` | `LinearScan`, `GraphColour` | 8a → 8a | 50–51 |
| `runtime.h` | `print_int`, allocation, entry point | linked with 10 | 55–56 |
| `jit.h` | `Jit` | 7 → memory → call | 59 |
| `driver.h` | `Driver`, `Options` | orchestrates all of it | 61 |

Twenty-three headers, about 8,000 lines. Note how the table has only three kinds of row: *this is a
representation*, *this is a translation*, *this is an analysis*. If you ever find yourself writing a
header that is none of the three, you are probably putting logic in the wrong place.

---

## 3. What the driver looks like

Here is the whole compiler, with the detail removed. This is (nearly) the real `driver.h` from
Chapter 61:

```cpp
int compile(const Options& opt) {
    Diagnostics diag;                                   // one per compilation

    // 1 → 2
    SourceFile src = SourceFile::read(opt.input_path);   // may fail: file missing
    Lexer lexer(src, diag);
    std::vector<Token> tokens = lexer.tokenize();
    if (opt.dump_tokens) dump_tokens(tokens, src);

    // 2 → 3
    Parser parser(tokens, src, diag);
    ast::Module module = parser.parse_module();
    if (opt.dump_ast) print_ast(module, std::cout);
    if (diag.has_errors()) return 1;                     // stop: the tree is unreliable

    // 3 → 4 → 5
    TypeContext types;
    Resolver resolver(diag, types);
    resolver.resolve(module);
    TypeChecker checker(diag, types);
    checker.check(module);
    if (diag.has_errors()) return 1;                     // stop: meanings are unknown

    // 5 → 6
    ir::Module ir = Lowerer(types).lower(module);
    to_ssa(ir);

    // 6 → 7
    run_passes(ir, opt.opt_level);
    if (opt.dump_ir) print_ir(ir, std::cout);

    // 7 → out
    switch (opt.backend) {
        case Backend::Interpret: return Interpreter(ir).run_main();
        case Backend::Bytecode:  return VM(compile_bytecode(ir)).run();
        case Backend::Native:    from_ssa(ir);
                                 emit_asm(ir, opt.output_path);
                                 return assemble_and_link(opt);
    }
}
```

Forty lines. Everything else in the book is the *inside* of one of those calls. Read this listing again
after Part 6 and it will feel like a table of contents.

Three details worth noticing now:

**`Diagnostics diag` is created first and passed to everything.** Errors are not exceptions and not
return codes; they are *appended to a list* that anyone can write to. This is what makes it possible to
report twelve errors from one run instead of one at a time (Chapter 10).

**There are two `if (diag.has_errors()) return 1;` barriers**, and their placement is a real design
decision. After parsing, we stop, because a type checker cannot be trusted with a tree built from
guesses during error recovery. After type checking, we stop, because lowering assumes every expression
has a type. Between those barriers, we deliberately *keep going* — a program with three type errors
should report three.

**The backend is a `switch`, and the three cases share everything before them.** That is the front
end/back end split paying for itself: the interpreter, the VM and the native compiler are 90% the same
program.

---

## 4. The invariants: what each stage may not know

A staged design only works if the stages are genuinely ignorant of each other. These are the rules
this book keeps, and each one is a rule you could break to save ten lines today and lose a day next
month.

| Stage | Must not know | Because |
|-------|---------------|---------|
| Lexer | the grammar | otherwise you cannot reuse it for tooling, and `a < b` vs `Vec<T>` becomes a disaster (Chapter 9) |
| Parser | whether names exist, or types | keeps the parser context-free-ish and fast, and lets an IDE parse broken code (Chapter 19) |
| Resolver | anything about types | name lookup must work before types are known, since a type's name must itself be resolved |
| Type checker | the target machine | `int` is 64 bits because *the language says so*, not because x86 has 64-bit registers |
| Lowering | the optimiser | lowering emits the obvious, dumb IR; making it clever is a pass's job |
| Optimiser | the source language and the machine | so passes are reusable across both — the whole reason a middle end exists |
| Register allocator | what the values mean | it sees live ranges and interference, nothing else |

The one that always tempts people is the third. "While I am resolving names I *could* work out the
types, it is the same walk." You can, and single-pass compilers did, and then mutual recursion between
two functions defined in either order becomes impossible, forward declarations become mandatory, and
your language has C's header files. Chapter 25 is explicit about this.

---

## 5. Why passes at all? A short history of phase ordering

Early compilers were **single-pass**: they read the source once and emitted code as they went, because
memory was measured in kilobytes and you could not hold the program. That constraint shaped languages
for thirty years:

* C requires a declaration before use — so a one-pass compiler knows the type when it sees the call.
* C's `struct` must be complete before use — so the compiler knows the size when it allocates.
* Pascal's rigid declaration order (`label`, `const`, `type`, `var`, procedures, body) — same reason.
* FORTRAN's implicit typing by first letter — so no lookup was needed at all.

Modern compilers are **multi-pass**, and languages are designed accordingly (in Pebble, as in Rust, Go
and Java, you can call a function declared later in the file). The trade is: we hold the whole program
in memory, and we walk it several times. On today's machines that is obviously right.

But "multi-pass" raises a real question with a real cost: **in what order?** Some examples of genuine
phase-ordering problems you will meet later in this book:

* **Inline first, or optimise first?** Inlining exposes optimisation opportunities (the callee's code is
  now specialised for these arguments), so inline first. But optimising first makes functions smaller,
  so more of them fit under the inlining threshold, so *optimise* first. Both are true. Real compilers
  run the pipeline more than once, and the pass list in Chapter 44 is a pragmatic compromise.
* **Constant propagation and dead code elimination each create work for the other.** Folding `if (true)`
  makes a branch dead; deleting the dead branch makes more values constant. So you iterate until
  nothing changes (Chapter 41) — or you write one combined pass that does both at once and is stronger
  than either repeated (that is SCCP, and it is the exercise at the end of Chapter 41).
* **Register allocation and instruction scheduling.** Scheduling wants to move instructions apart to
  hide latency, which lengthens live ranges, which makes allocation spill. Allocating first constrains
  the schedule. This one has no good answer and is still researched.

The honest summary: pass ordering in production compilers is a hand-tuned heuristic, justified by
benchmarks, not by theory. It is one of the places where compiler construction is engineering rather
than mathematics — and one of the places where your own compiler can beat a big one on your own
workload.

---

## 6. Diagnostics cross every stage

Every box in the diagram can produce errors, and they are all reported the same way:

```
error: cannot multiply 'float' by 'int'
  --> examples/price.peb:7:17
   |
 7 |     let total = price * 2;
   |                 ^^^^^^^^^ 'price' is float, '2' is int
   |
help: write the literal as a float
   |
 7 |     let total = price * 2.0;
   |                         ~~~
```

To produce that, the type checker needed: the message, a `Span` (byte offsets 112–121), a note with its
own span, and a suggested edit. It did **not** need to know how to find line 7, how to read the file, or
how to colour a terminal. That is `diag.h`'s job, and it is why every representation in the pipeline
carries spans: an error found in representation 6 must still be able to point at representation 1.

```
Span lives in:  Token → ast node → ir::Instruction → debug info → run-time stack trace
```

That chain is worth remembering. Losing the span at any link means a later stage cannot report a good
error — which is exactly why so many compilers say `error in function foo` with no line number for
back-end problems.

---

## 7. Inspecting everything: the dump flags

Because each stage has a printable representation, our compiler can show its work:

```bat
pebblec --dump-tokens examples\hello.peb
pebblec --dump-ast    examples\hello.peb
pebblec --dump-ir     examples\hello.peb
pebblec --dump-ir-after=inline examples\hello.peb
pebblec -S            examples\hello.peb     REM stop after assembly
```

This is not a debugging luxury; it is the primary development method for the rest of the book. When
Chapter 43's loop optimiser produces a wrong answer, the question "which stage first contains the
mistake?" is answered by dumping representations 6 and 7 and diffing them. Real compilers have exactly
these switches (`gcc -fdump-tree-all`, `clang -emit-llvm -S`, `rustc --emit=mir`), and for exactly this
reason.

**A habit to adopt now:** write the dumper for a representation *before* the translation that produces
it. It takes twenty minutes and it will save you hours, every time.

---

## 8. What we are deliberately not building

So that you know these exist and why they are absent:

* **A preprocessor.** C's `#include`/`#define` runs as a separate text-level pass before lexing, which
  is why C error messages can point at code you never wrote. Pebble has a module system instead
  (Chapter 61). If you want macros in *your* language, Chapter 69 discusses where they belong
  (hint: after parsing, on the AST, like Rust and Lisp — not before it, like C).
* **A separate assembler.** We emit assembly *text* and let `gcc` assemble it (Chapter 52), then
  Chapter 57 shows what the assembler and linker actually do, and we write a minimal object-file
  writer so the mystery is gone.
* **Incremental and parallel compilation.** Chapter 65 explains the design (query-based compilation, as
  in `rustc` and Roslyn) without building it; it is a book of its own.
* **Full Unicode.** We handle UTF-8 in strings and comments, and restrict identifiers to ASCII
  (Chapter 5 says why, and what the alternative costs).

---

## Check yourself

1. Representation 5 (typed AST) and representation 6 (IR) both describe the same program. Why keep both,
   instead of type-checking the IR directly?
2. The driver stops after parsing if there were errors, but *not* between the resolver and the type
   checker. Why the difference?
3. Which stage should reject `let x: int = "hello";` and which should reject `let x = ;`?
4. The optimiser may not know the target machine. But surely whether to unroll a loop depends on cache
   size. How do real compilers reconcile this?
5. You add a `for` loop to Pebble. Which of the twenty-three headers change?

<details>
<summary>Answers</summary>

1. Because they are good at different things. The AST mirrors the source, so it is what you need for
   error messages, formatters and IDEs, and it preserves distinctions the language cares about
   (`a[i]` vs `*(a+i)`). The IR erases those distinctions on purpose so that analysis is uniform:
   one kind of jump, no nesting, every value named. Type-checking the IR would mean the errors could no
   longer point at what the programmer wrote.
2. Because the type checker cannot run on a tree where names did not resolve — every lookup would
   cascade into a second, meaningless error ("unknown name `prcie`" followed by "cannot add `error` to
   `int`"). In practice the resolver marks unresolved names and the type checker skips expressions
   involving them; that is the middle path, and Chapter 27 implements it. But the barrier after parsing
   is absolute, because error recovery invents tree nodes that were never written.
3. `let x: int = "hello";` is the type checker (stage 5) — it is perfectly grammatical. `let x = ;` is
   the parser (stage 3) — no expression can start with `;`.
4. By *parameterising* the optimiser rather than by letting it query the machine directly. LLVM passes
   take a `TargetTransformInfo` — a small interface answering "how expensive is this instruction?",
   "how many registers are there?", "what is the vector width?" — implemented per target. The pass
   still contains no x86 knowledge; it asks a question through a narrow door. Chapter 43 gives ours
   three numbers and no more.
5. `token.h` (a `kw_for` token), `lexer.h` (recognise it — actually free, if keywords are interned),
   `ast.h` (a `ForStmt` node), `parser.h` (parse it), `printer.h` (print it), `sema.h` (check the
   loop variable, mark "inside a loop" for `break`), `lower.h` (desugar to the same IR a `while`
   produces). Seven files — and *nothing* in the optimiser or back end, because by the time IR exists
   a `for` loop is indistinguishable from a `while`. That is the payoff of the layering, and the
   exercise in Chapter 71 is exactly this, done for real.
</details>

---

[Next: Chapter 4 — A tiny compiler in one file →](04-tiny-compiler.md)
