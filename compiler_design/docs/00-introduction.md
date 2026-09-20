# Chapter 0 — Introduction: what a compiler really is

[Contents](README.md) · [Next: Setting up →](01-setup.md)

---

## Goal

By the end of this chapter you will be able to explain, to a friend with no computing background,
what a compiler does and why it is built the way it is. You will know the name of every stage we are
going to write, what each stage receives, what it hands on, and why the whole thing is divided up
like that. No code yet — this is the map before the journey.

---

## 1. The one-sentence answer

> A **compiler** is a program that translates a program written in one language into an equivalent
> program in another language.

Two words in that sentence carry all the weight.

**Translates.** Not "runs". A compiler does not execute your program. It reads it, understands it,
and writes a *new* program. What you get back is an artefact — an executable file, a `.class` file,
some JavaScript, some assembly — which someone or something else will run later. The compiler's job
is finished before your program starts.

**Equivalent.** The output must *mean the same thing* as the input. This is the whole difficulty.
Not "look the same" — the output usually looks nothing like the input:

```
input:   return a * 8;
output:  mov  rax, [rbp-8]
         shl  rax, 3
         ret
```

A multiplication became a bit-shift. Those are different operations. They are nevertheless
*equivalent for this program*, because multiplying an integer by 8 and shifting it left by 3 bits
always produce the same integer. The compiler is allowed to change *how*, never *what*.

Everything in this book is a consequence of those two words. The front end exists to find out what
your program means. The back end exists to say that meaning in another language. The optimiser exists
to say it *better* without saying something *else*.

---

## 2. Trace one line by hand

Forget code for a moment. Here is a single line of Pebble:

```pebble
let total = price * 2 + 1;
```

Let us do, slowly and by hand, what the compiler will do. This is the entire book in miniature.

### Stage 1 — Read characters, produce tokens (lexical analysis)

The file on disk is not a program. It is a sequence of bytes:

```
l e t space t o t a l space = space p r i c e space * space 2 space + space 1 ;
```

The first job is to group those bytes into the smallest meaningful units, called **tokens**. A token
is a word of the language, like a word in English:

| # | Token kind | Text | Why it is one unit |
|---|------------|------|--------------------|
| 1 | `kw_let` | `let` | a keyword, three characters that mean one thing |
| 2 | `identifier` | `total` | a name the programmer invented |
| 3 | `equal` | `=` | the assignment operator |
| 4 | `identifier` | `price` | another name |
| 5 | `star` | `*` | multiply |
| 6 | `int_literal` | `2` | a number, value 2 |
| 7 | `plus` | `+` | add |
| 8 | `int_literal` | `1` | a number, value 1 |
| 9 | `semicolon` | `;` | statement ends here |
| 10 | `eof` | | nothing left |

Notice what the lexer threw away: the spaces. Notice what it kept: the *text* of each token (we will
need `total` later) and, crucially, *where it was* — the file offset, so that any later error can
point at the right place.

Notice also what the lexer did **not** do: it has no idea that `*` binds tighter than `+`. It does
not know whether `price` exists. It does not care that `let` should be followed by a name. It is a
grouping machine, nothing more. Chapters 5–13.

### Stage 2 — Find the structure (syntax analysis / parsing)

A flat list of ten tokens is barely better than the bytes. Programs are *nested*: this statement
contains an expression, which contains a smaller expression. The parser turns the list into a tree —
the **abstract syntax tree**, or AST:

```
          LetDecl "total"
                │
                ▼
              Binary +
             ╱        ╲
        Binary *       IntLit 1
       ╱       ╲
  Name price    IntLit 2
```

Read that tree aloud: "declare `total` to be (the sum of (the product of `price` and 2) and 1)". The
tree encodes the answer to the question the token list could not answer: *does `*` happen before
`+`?* Yes — because `Binary *` is **deeper** in the tree, and a tree is evaluated bottom-up.

The parser is where a language's grammar lives. It is the stage that rejects

```pebble
let total = price * * 2;
```

with something like `expected expression, found '*'`. Chapters 14–23.

### Stage 3 — Work out the meaning (semantic analysis)

The tree is structurally fine, but is it *sensible*? Now the compiler asks questions that need
context:

* Does a variable called `price` exist at this point in the program? (**Name resolution**.) If yes,
  *which* `price` — the parameter, the global, the one in the enclosing block?
* What is its type? Say it is `float`.
* `price * 2`: can you multiply a `float` by an `int`? In Pebble the answer is *no, not implicitly*,
  so this is an error and the compiler will say so, suggesting `2.0`. In C the answer is yes, and the
  compiler silently inserts a conversion. **That is a language design decision**, and Part 8 is about
  making such decisions deliberately.
* Assume we wrote `price * 2.0 + 1.0`. Then the type of the whole expression is `float`, so `total`
  is a `float`.

The output of this stage is the same tree, **annotated**: every expression node now carries a type,
every name now points at the exact declaration it refers to. A tree with those annotations is often
called a *typed* or *resolved* AST. Chapters 24–33.

### Stage 4 — Flatten it into simple instructions (IR generation)

Trees are wonderful for checking and terrible for optimising. So we flatten the tree into a list of
dead-simple operations, each doing one thing, each naming its result. This is the
**intermediate representation** (IR):

```
  t0 = load price
  t1 = fmul t0, 2.0
  t2 = fadd t1, 1.0
  store total, t2
```

Every line has at most one operator. Nothing is nested. This form is boring on purpose: boring code
is easy for a machine to analyse. Chapters 34–39.

### Stage 5 — Make it better (optimisation)

Suppose the compiler had already worked out that, right here, `price` is always `10.0`. Then:

```
  t0 = 10.0              ; constant propagation
  t1 = fmul 10.0, 2.0
  t2 = fadd t1, 1.0
```
becomes
```
  t1 = 20.0              ; constant folding: the compiler does the multiply itself
  t2 = 21.0              ; and the add
  store total, 21.0
```
and finally, if nothing ever reads `total`:
```
  ; nothing at all       ; dead code elimination
```

Three lines of arithmetic became zero instructions. No machine cycles will be spent at run time on
work whose answer was already knowable at compile time. That is what an optimiser is: a collection of
passes, each of which rewrites the IR into a cheaper IR that means the same thing. Chapters 40–46.

### Stage 6 — Say it in the machine's language (code generation)

Finally the IR is translated into instructions the CPU actually has. The CPU has no `t1`; it has 16
general-purpose registers and a stack. Deciding which value lives in which register is **register
allocation**, and it is one of the most beautiful problems in the book (it is graph colouring —
Chapter 51).

```asm
        movsd   xmm0, [rbp-8]        ; t0 = price
        mulsd   xmm0, [rel .LC2]     ; * 2.0
        addsd   xmm0, [rel .LC3]     ; + 1.0
        movsd   [rbp-16], xmm0       ; total = ...
```

Chapters 47–57.

### The whole trip on one line

```
 characters ─▶ tokens ─▶ AST ─▶ typed AST ─▶ IR ─▶ better IR ─▶ assembly ─▶ machine code
   (file)      lexer    parser    sema      lower    optimise    codegen    assembler
```

Every single chapter of this book fits somewhere on that line. When you feel lost — and in Part 5 or
6 you will — come back to this diagram and ask "which arrow am I on?"

---

## 3. Compiler, interpreter, JIT, transpiler

These words get used loosely. The distinctions matter, because we will build three of the four.

**Compiler (ahead-of-time).** Translates the whole program before it runs. Output: a file. Examples:
GCC, Clang, Rust, Go, the Pebble compiler we are building. Advantage: all analysis and optimisation
cost is paid once, at build time; the program starts fast and runs fast. Disadvantage: a slow
edit-build-test loop, and the compiler must guess about things it cannot know (which branch is hot,
what the actual input will look like).

**Interpreter.** Reads the program and *performs* it, directly, without producing a translated
artefact. Examples: CPython's core loop, a shell, our IR interpreter in Chapter 39. Advantage:
instant start, easy to debug, trivially portable. Disadvantage: usually 10–100× slower, because the
work of understanding each instruction is repeated every time that instruction runs.

A tree-walking interpreter is the simplest kind: take the AST from stage 3 and recursively evaluate
it. A **bytecode** interpreter (Chapter 54) first compiles to a compact instruction set, then runs a
tight dispatch loop. That is faster, and it is what Python, Lua, Java and C# all do.

**JIT (just-in-time compiler).** Starts by interpreting, watches which functions run often, and
compiles *those* to machine code while the program is running. Because it compiles at run time, it
knows things an AOT compiler can only guess: the actual types flowing through a function, which
branch is taken 99% of the time, the exact CPU it is on. Examples: the JVM's HotSpot, V8,
LuaJIT, .NET. We build a small one in Chapter 59.

**Transpiler (source-to-source compiler).** A compiler whose target language is also a high-level
language. TypeScript → JavaScript, Cfront (the first C++ compiler) → C. There is nothing
second-class about this; it is a normal compiler that stops before code generation and prints text
instead. Chapter 52 shows how close our own back end gets to this.

The important realisation: **these are not different kinds of program, they are different places to
put the same stages.** Every one of them lexes, parses and analyses. They differ only in what they do
after that.

| | AOT compiler | Bytecode VM | JIT | Transpiler |
|---|---|---|---|---|
| Lexer | yes | yes | yes | yes |
| Parser | yes | yes | yes | yes |
| Semantic analysis | yes | yes | yes | yes |
| IR | yes | yes (bytecode) | yes | sometimes |
| Optimiser | yes, heavy | light | yes, at run time | light |
| Emits | machine code | bytecode | machine code, lazily | source text |

This is why learning to write a compiler teaches you *all* of them.

---

## 4. Front end, middle end, back end

The six stages are conventionally grouped into three:

```
┌───────────────────────────────┬──────────────────────┬───────────────────────────────┐
│          FRONT END            │      MIDDLE END      │           BACK END            │
│  lexer → parser → semantics   │   IR → optimisation  │  selection → regalloc → emit  │
│  "what did the human write     │  "how can this be    │  "how does this machine       │
│   and what does it mean?"      │   made cheaper?"     │   express it?"                │
│  knows the LANGUAGE            │  knows NEITHER       │  knows the MACHINE            │
└───────────────────────────────┴──────────────────────┴───────────────────────────────┘
```

The middle's ignorance is the point. Because the optimiser understands only the IR, it does not care
whether the source was C, Rust, Fortran or Pebble; and because it does not know the target, the same
pass works for x86, ARM and RISC-V. That is why:

* **N languages × M machines needs N + M pieces, not N × M.** Write a front end for your language
  that emits LLVM IR and you inherit thirty back ends and a thousand optimisation passes. This single
  economic fact explains the existence of GCC's GIMPLE and of LLVM, and it is why new languages
  (Rust, Swift, Julia, Zig, Clang itself) appear so much faster than they used to.
* **You can test the pieces separately.** Our IR interpreter (Chapter 39) lets us check that lowering
  is correct before any machine code exists, and check that every optimisation preserves behaviour.
  Without it, a bug could be anywhere in 20,000 lines.

We build all three ends ourselves, so that when you later use LLVM you will know exactly what it is
doing for you, and what it cannot do for you.

---

## 5. Why writing a compiler makes you a better programmer

This is not a sentimental claim; it is a list of specific transfers.

1. **You stop guessing about performance.** After Chapter 43 you will know why the loop you wrote was
   or was not vectorised, why `i++` in a hot loop is free, and why that innocent `std::string` copy
   costs a heap allocation. You will be reading the same IR your optimiser reads.
2. **Error messages become a design problem, not an afterthought.** Chapter 10 will make you
   permanently intolerant of `Segmentation fault` as an error report.
3. **You will use data structures for real.** A compiler is a tour of everything: hash maps (symbol
   tables), trees (AST), graphs (CFG, call graph, interference graph), worklists (data-flow),
   union-find (type inference), bit vectors (liveness), arenas (allocation). Each one solves a problem
   you can see, not an exercise.
4. **Recursion stops being a trick.** Recursive descent, tree walks, and the recursive structure of
   types make recursion feel like the natural way to touch nested data, which it is.
5. **You learn to specify.** Half of Part 8 is about writing down what a construct *means* before
   implementing it. It is the same skill as writing a good API.
6. **The machine stops being magic.** By Chapter 52 you will have written bytes that a CPU executes,
   and you will know what every one of them is for.

And one more: a compiler is a program with an unusually **honest** feedback loop. It either produces a
program that computes the right answer, or it does not. There is no "looks about right".

---

## 6. What we will build, concretely

By the last page you will have, in this repository:

* `pebblec`, a compiler that turns `.peb` files into x86-64 assembly, and a driver that assembles and
  links them into an executable you can double-click.
* A bytecode VM that runs the same programs without an assembler, on any machine.
* A REPL where you can type Pebble expressions and see answers.
* A JIT that compiles a function to memory and calls it.
* A formatter, a syntax highlighter and a small language server that gives hover types in VS Code.
* A test suite of several hundred programs, a golden-file harness and a fuzzer.
* Roughly 8,000 lines of C++ that you understand completely, because you wrote them.
* A second language, Cobble, designed by you in Part 8.

Here is the same picture as a dependency map — this is the order the book follows, and also the order
in which things start working:

```
   Ch 4   one-file compiler for arithmetic        ← you get a working compiler on day one
     │
   Ch 5-13   lexer + diagnostics                 ← "it can read my file"
     │
   Ch 14-23  parser + AST + formatter            ← "it understands my structure"
     │
   Ch 24-33  symbols + types                     ← "it catches my mistakes"
     │
   Ch 34-39  IR + interpreter                    ← "MY PROGRAMS RUN"      ★ milestone
     │
   Ch 40-46  optimiser                           ← "they run 3× faster"
     │
   Ch 47-57  x86-64 backend + runtime + GC       ← "they are real .exe files" ★ milestone
     │
   Ch 58-65  toolchain                           ← "it feels like a real language"
     │
   Ch 66-73  design your own                     ← "it is MY language"     ★ milestone
```

Note the star at Chapter 39. That is the first point at which you can write a Pebble program and see
it produce output. It arrives at about 40% of the book, which is normal: a compiler is mostly
front-loaded understanding. Chapter 4 exists specifically so that you do not have to wait that long
for your first taste.

---

## 7. A very short history, because the names will keep appearing

* **1952, Grace Hopper, A-0.** The first thing called a compiler; really a linker of subroutines.
  Hopper also gave us the word *compiler*, from "compiling" a program out of a library of pieces.
* **1957, FORTRAN, John Backus and team.** The first optimising compiler, and the moment the argument
  "machine code by hand is always faster" started to lose. It took 18 person-years. Its register
  allocator and its handling of array indices are recognisably the ancestors of ours.
* **1960, Algol 60.** Introduced block structure, recursion, and the *first formal grammar of a
  programming language* (BNF, by Backus and Naur). Chapter 14's notation is theirs. Also began the
  tradition of a language spec being a document, not a program.
* **1965–1975, the theory.** Knuth formalised LR parsing (1965); DeRemer gave us LALR (1969); the
  data-flow framework and the first serious optimisation theory were worked out at IBM. Almost every
  algorithm in Parts 2, 4 and 5 is from this period. It is remarkable how much of a modern compiler
  was invented before 1980.
* **1977–1986, the tools.** `yacc`, `lex`, and then the **Dragon Book** (Aho, Sethi, Ullman, 1986),
  which fixed the vocabulary we still use. If you continue past this book, that is one of the two
  places to go (Appendix G).
* **1987, GCC.** Free, retargetable, and the reason free software could bootstrap itself.
* **1991, SSA.** Cytron et al. publish efficient SSA construction. Everything in Part 4 and most of
  Part 5 dates from this paper; it is probably the single most important compiler idea of the last
  forty years.
* **2003, LLVM.** Lattner's thesis project turns "the middle end is a library" into an industry.
  Clang, Rust, Swift, Julia, Zig and most GPU compilers are downstream of it.
* **Now.** The frontiers are: better diagnostics and IDE integration (incremental, error-tolerant
  compilation — Chapters 19 and 63), proof (CompCert, a compiler verified correct in Coq), and
  ML-guided heuristics for the places where we currently guess (inlining, scheduling).

You are, in other words, about to re-walk a seventy-year road, but on a paved version of it.

---

## 8. Four misconceptions to drop right now

**"Compilers are too hard for me."** The individual pieces are small. A competent lexer is 300 lines.
A Pratt parser is 150. The type checker is a big `switch`. What is hard is *the number of pieces* and
keeping them consistent — which is an engineering problem, solved by doing them one at a time in
order, which is what this book is.

**"You need to be a maths person."** You need: functions, sets, a little graph vocabulary, and
induction. We will define everything else. There is no calculus, no algebra beyond "multiplying by 8
equals shifting by 3", and the two genuinely mathematical ideas (lattices in Chapter 40, unification
in Chapter 30) are taught from zero.

**"Modern compilers are so complex that a hand-written one is pointless."** GCC is 15 million lines,
yes. But the shape of those lines is the shape in this book, and several important production
compilers are small: TCC (a complete C compiler, ~30k lines), the original Go compiler, Lua's (~25k
lines including the VM and the stdlib). Hand-written recursive-descent parsers, not generated ones,
are what GCC, Clang, Rust and Go all actually use. You are not learning a toy version. You are
learning a small version.

**"The compiler is always right."** Compilers have bugs, including in the optimiser, including ones
that silently produce wrong code. Part 7 is partly about the discipline — differential testing,
fuzzing, golden files — that keeps yours honest. Learning where a compiler *can* be wrong is also how
you learn to stop blaming it for your own undefined behaviour.

---

## 9. How to actually get through this book

* **One chapter per sitting, code in front of you.** Reading a compiler chapter without typing is
  like reading about swimming.
* **Keep a `notes.md`.** When you make a design decision that differs from the book's — and you
  should — write down why. Part 8 asks you to reread those notes.
* **When something does not work, print the intermediate form.** That is the deep advantage of a
  staged design: you can always ask "what did the lexer give the parser?" Every stage in this book has
  a dumper for exactly that reason. `--dump-tokens`, `--dump-ast`, `--dump-ir`.
* **Do not skip the theory chapters.** They are short, and each one exists because at some point you
  will hit a bug that is only explicable in its terms (ambiguous grammars, non-terminating data-flow,
  ill-founded types).
* **Expect Part 6 to be slow.** Code generation is where you are learning a second language (assembly)
  at the same time as an algorithm. Chapter 47 is a full assembly tutorial for this reason.

---

## Check yourself

1. Why must the lexer record the *position* of every token, when the parser only ever asks for the
   next one?
2. `price * 2` produced an error in section 2. Name a language where it would not, and say what the
   compiler silently inserts there.
3. What is the one advantage a JIT has that no ahead-of-time compiler can ever have?
4. The optimiser "knows neither the language nor the machine". Give one optimisation that this
   ignorance makes *impossible*, and say which stage must do it instead.
5. In the trace in section 2, at which stage would the error `undefined name: prcie` be reported? At
   which stage would `expected ';'` be reported?

<details>
<summary>Answers</summary>

1. So that later stages — which may run long after the token was consumed — can point at the exact
   characters in an error message. Positions flow from the lexer into AST nodes, into IR
   instructions, and finally into debug info, so a run-time crash can name a source line. A token
   without a position is an error message without an address.
2. C, C++, Java, Python, JavaScript — all of them. The compiler inserts an implicit conversion
   (in C's terms, the *usual arithmetic conversions* promote the `int` `2` to `double` `2.0`).
   Chapter 27 discusses why Pebble does not, and what that costs and buys.
3. Knowledge of the actual run-time behaviour: real types, real branch frequencies, real input sizes,
   and the exact CPU model. An AOT compiler can be *told* some of this (profile-guided optimisation)
   but can never observe it directly.
4. Anything requiring language semantics the IR has erased — for example, devirtualising a call
   because the language guarantees a class is `final`, or removing a bounds check because the
   language's `for` loop cannot exceed the array. Those must be done in the front end, or the front
   end must record the fact in the IR (which is why real IRs carry so much metadata). Likewise
   machine-specific rewrites (using a single `lea` for `a*4+b`) belong in the back end.
5. `undefined name: prcie` — stage 3, semantic analysis, specifically name resolution; the parser is
   perfectly happy with any identifier. `expected ';'` — stage 2, the parser; the lexer produces the
   tokens without caring what order they are in.
</details>

---

## What is next

Chapter 1 gets a C++ compiler installed and builds something. Chapter 2 is the C++ we will lean on —
skim it if you are comfortable, and return to it when a `std::variant` bites you. Chapter 3 walks the
pipeline again, but with the real names, real data structures and real file layout of *our* compiler.
Chapter 4 then compiles arithmetic to x86-64 in one file of about 250 lines, so that you have built a
compiler before you have finished your first evening.

[Next: Chapter 1 — Setting up →](01-setup.md)
