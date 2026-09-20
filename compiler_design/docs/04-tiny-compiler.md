# Chapter 4 — A tiny compiler in one file: arithmetic to x86-64

[← The pipeline](03-the-pipeline.md) · [Contents](README.md) · [Next: Source text →](05-source-text.md)

> 📖 **Line by line:** [ch04_tiny_compiler explained line by line](line-by-line/ch04_tiny_compiler.md)

---

## Goal

Write a **complete, real compiler** today, in 380 lines of one file. It will:

* lex, parse, print, optimise, interpret and generate x86-64 assembly,
* produce a `.s` file that `gcc` assembles into an `.exe` you can run,
* and report errors with a caret under the offending character.

Its language is only integer arithmetic — `+ - * / %`, parentheses, unary minus — but every stage is a
genuine, scaled-down version of what Parts 1 to 6 build properly. When you finish this chapter you will
have written a compiler, and the rest of the book becomes "make each of these six pieces serious".

---

## 1. The idea: six functions in a row

```
  "2 + 3 * (10 - 4)"
         │
         │  tokenize()          §3
         ▼
  [Int 2][Plus][Int 3][Star][LParen][Int 10][Minus][Int 4][RParen][End]
         │
         │  Parser::parse()     §4   ← the only clever part
         ▼
        (+)
       ╱   ╲
     2     (*)
          ╱   ╲
        3     (-)
             ╱   ╲
          10      4
         │
         ├── print_tree()       §5   — so we can see what we built
         ├── eval()             §6   — an interpreter, our reference answer: 20
         ├── fold()             §7   — an optimiser: the whole tree becomes `20`
         ▼
  gen_program()                 §8
         │
         ▼
  main: mov rax, 20 / push rax / pop rax / printf / ret
```

Notice that `eval` and `gen_program` are *alternative* consumers of the same tree. That is the shape of
every compiler that can also interpret — including ours in Chapter 39, and including Python, Java and
C#.

---

## 2. Design decisions, made explicitly

Small programs have design decisions too, and these five recur throughout the book.

**A single global `g_source` and an exiting `fail()`.** Real diagnostics need a `Diagnostics` object,
multiple errors, notes and suggestions (Chapter 10). Today, one error and `exit(1)`. The *shape* is
already right, though: `fail(position, message)` takes a byte offset, not a line number, because byte
offsets are what the lexer naturally has and line numbers are computed later.

**An `End` token.** Rather than every function checking "have I run off the end of the vector?", the
token list always ends with `Kind::End`. Then `peek()` is unconditional, and "unexpected end of input"
becomes an ordinary "unexpected token" message. This is a small trick with a large effect on code size,
and every serious lexer does it.

**One `Node` struct with a `Tag`, not a class hierarchy.** For three node kinds, a tagged struct is
clearly right; Chapter 15 switches to a hierarchy when there are thirty. Notice that the struct is
*wasteful* — an `Int` node carries two unused `NodePtr`s. For a real AST that waste matters, and
Chapter 15 measures it.

**The parser owns no tokens.** `Parser` holds a `const std::vector<Tok>&`. The tokens live in `main`.
Nothing is copied.

**The code generator emits text, not bytes.** We write assembly and let `gcc` assemble it. That is what
GCC itself does by default, and what Clang does with `-S`. Emitting machine code directly is Chapter 57
(and the JIT in Chapter 59 must, because there is no file to assemble).

---

## 3. The lexer

```cpp
static std::vector<Tok> tokenize(const std::string& src) {
    std::vector<Tok> out;
    std::size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }      // (a)
        if (std::isdigit(static_cast<unsigned char>(c))) {                        // (b)
            std::size_t start = i;
            long long   v     = 0;
            while (i < src.size() && std::isdigit(static_cast<unsigned char>(src[i]))) {
                v = v * 10 + (src[i] - '0');
                i++;
            }
            out.push_back({Kind::Int, v, start});
            continue;
        }
        Kind k = Kind::End;                                                       // (c)
        switch (c) {
            case '+': k = Kind::Plus;  break;
            /* ... */
            default: fail(i, std::string("unexpected character '") + c + "'");
        }
        out.push_back({k, 0, i});
        i++;
    }
    out.push_back({Kind::End, 0, src.size()});                                    // (d)
    return out;
}
```

**(a) Whitespace is discarded.** It separates tokens and then it is gone. That is why
`2+3`, `2 + 3` and `2   +3` all produce identical token lists — and why a language whose meaning
depends on layout (Python, Haskell) needs a lexer that *does not* throw it away, but emits `INDENT`
and `DEDENT` tokens. Chapter 9 discusses that.

**(b) Numbers use maximal munch.** Having seen a digit, consume *as many digits as possible*. If we
stopped after one, `10` would become `Int 1` then `Int 0` — which is exactly what happens if you delete
the inner `while`. The general principle ("always take the longest token that matches") is the
fundamental rule of lexing, and Chapter 9 shows the cases where it is surprising (`a---b`,
`Vec<Vec<int>>`).

**Why `static_cast<unsigned char>`?** `std::isdigit` takes an `int` that must be a valid `unsigned char`
value or `EOF`. Passing a `char` that is negative (any byte above 127, i.e. any UTF-8 continuation byte)
is undefined behaviour, and on MSVC debug builds it *asserts and crashes*. This is a real bug in an
enormous amount of real code. Chapter 7 avoids it permanently with our own `is_digit` helpers.

**(c) Operators are a lookup.** One character, one token. Chapter 9 extends this to multi-character
operators (`==`, `->`, `<<=`), where the rule "try the longest first" reappears.

**(d) The `End` sentinel**, as designed above. Note its position is `src.size()` — one past the end —
which is where "unexpected end of input" carets point.

What the lexer does *not* know: that `(` must be matched, that `+` needs two operands, that `%` and
`/` behave alike. It is a grouping machine. Keeping it that ignorant is what makes it reusable by the
syntax highlighter in Chapter 63.

---

## 4. The parser: precedence climbing

This is the one genuinely clever function in the file, and it is fourteen lines.

The problem: from a *flat* list, recover the *nesting* implied by operator precedence, so that
`2 + 3 * 4` becomes `2 + (3 * 4)` and not `(2 + 3) * 4`.

Give every infix operator a number, its **binding power**:

```cpp
static int precedence(Kind k) {
    switch (k) {
        case Kind::Plus:
        case Kind::Minus:   return 10;
        case Kind::Star:
        case Kind::Slash:
        case Kind::Percent: return 20;
        default:            return -1;   // not an infix operator
    }
}
```

Now:

```cpp
NodePtr parse_expr(int min_prec) {
    NodePtr lhs = parse_primary();                 // 1. one operand, always
    for (;;) {
        Kind op   = peek().kind;
        int  prec = precedence(op);
        if (prec < min_prec || prec < 0) return lhs;   // 2. stop
        std::size_t pos = advance().pos;               // 3. eat the operator
        NodePtr rhs = parse_expr(prec + 1);            // 4. parse the right side
        lhs = Node::make_binary(op, std::move(lhs), std::move(rhs), pos);   // 5. grow left
    }
}
```

Read it as: *"I have a left-hand side. As long as the next operator binds at least as tightly as I was
told to accept, take it, parse a right-hand side that binds more tightly than this operator, and fold
the three of them into a new left-hand side."*

### Trace it: `2 + 3 * 4`

| Call | `min_prec` | State | Action |
|------|-----------|-------|--------|
| `parse_expr(0)` | 0 | lhs = `2` | sees `+` (10). 10 ≥ 0, so take it |
| → `parse_expr(11)` | 11 | lhs = `3` | sees `*` (20). 20 ≥ 11, so take it |
| → → `parse_expr(21)` | 21 | lhs = `4` | sees `End` (−1). Stop, return `4` |
| ← `parse_expr(11)` | 11 | lhs = `(3 * 4)` | sees `End`. Stop, return `(3*4)` |
| ← `parse_expr(0)` | 0 | lhs = `(2 + (3*4))` | sees `End`. Stop. Done |

The `*` got pulled into the *right* subtree of `+`, which puts it deeper, which makes it evaluate
first. Precedence became depth. That is the whole trick.

### Trace it: `2 * 3 + 4`

| Call | `min_prec` | State | Action |
|------|-----------|-------|--------|
| `parse_expr(0)` | 0 | lhs = `2` | sees `*` (20) ≥ 0, take it |
| → `parse_expr(21)` | 21 | lhs = `3` | sees `+` (10). **10 < 21 — stop**, return `3` |
| ← `parse_expr(0)` | 0 | lhs = `(2 * 3)` | loop again: sees `+` (10) ≥ 0, take it |
| → `parse_expr(11)` | 11 | lhs = `4` | sees `End`. Stop |
| ← `parse_expr(0)` | 0 | lhs = `((2*3) + 4)` | done |

The inner call *refused* the `+` because it binds too loosely, handed control back, and the outer loop
picked it up. That refusal is the entire mechanism of precedence.

### Why `prec + 1`?

It makes operators **left-associative**. With `prec + 1`, in `1 - 2 - 3` the inner call is
`parse_expr(11)` which refuses the second `-` (10 < 11), so the outer loop takes it and builds
`((1-2)-3) = -4`. Correct.

Pass `prec` instead of `prec + 1` and the inner call *accepts* the second `-`, producing
`(1-(2-3)) = 2`. Wrong for subtraction — but exactly what you want for **right**-associative operators
like `**` or `=`. So:

> Left-associative: recurse with `prec + 1`. Right-associative: recurse with `prec`.

One `+1` is the difference. Chapter 17 generalises this to a table with a left and a right binding
power per operator, which is Pratt's formulation and handles prefix, postfix and mixfix operators too.

### `parse_primary`

```cpp
case Kind::LParen:
    advance();
    NodePtr inner = parse_expr(0);      // start over at the lowest precedence
    expect(Kind::RParen, "')'");
    return inner;                       // note: NO node for the parentheses
```

Two things worth their own sentence.

**Parentheses reset the precedence to 0** — that is *what parentheses mean*. They are not an operator;
they are an instruction to the parser to start a fresh expression. Recursion makes that one line.

**No AST node is created for them.** `(2+3)` and `2+3` produce identical trees. The tree already
encodes the grouping, so the parentheses have done their job and can vanish. This is why an AST is
called *abstract*: it keeps the structure, not the punctuation. (A *concrete* syntax tree, or CST, keeps
everything — which is what a formatter or refactoring tool needs. Chapter 23 discusses the trade.)

Unary minus is `case Kind::Minus:` in `parse_primary`, and calling `parse_primary` again (not
`parse_expr`) gives it very high precedence: `-2 * 3` parses as `(-2) * 3`, and `-2 + 3` as `(-2) + 3`.
Both correct. Chapter 17 shows why the general rule is "a prefix operator has its own binding power".

---

## 5. Printing the tree

Two printers, because they answer different questions.

`print_tree` shows the structure, indented — this is the one you debug with:

```
Binary +
  Int 2
  Binary *
    Int 3
    Binary -
      Int 10
      Int 4
```

`print_infix` shows it as a fully parenthesised expression — this is the one that instantly reveals a
precedence bug, because you can compare it with what you typed:

```
(2 + (3 * (10 - 4)))
```

Write both. Every real compiler has both (`clang -ast-dump` and `clang-format` respectively).

---

## 6. The interpreter

```cpp
static long long eval(const Node* n) {
    switch (n->tag) {
        case Node::Tag::Int: return n->value;
        case Node::Tag::Neg: return -eval(n->lhs.get());
        case Node::Tag::Binary: {
            long long a = eval(n->lhs.get());
            long long b = eval(n->rhs.get());
            switch (n->op) { /* ... */ }
        }
    }
}
```

Nine lines, and it is a complete tree-walking interpreter. The structure of the code mirrors the
structure of the data, which is what makes recursion the natural tool for trees.

Its real purpose in this book is as an **oracle**. For any expression, `eval` gives the right answer;
so when the code generator produces something different, we know the bug is in the code generator, not
in our understanding of arithmetic. Differential testing against a simple reference implementation is
how compilers are actually tested (Chapter 64), and it starts here.

Note where division by zero is caught: at *evaluation*, not at parse time. `10 / 0` is grammatically
perfect. It is a run-time error in the interpreter, and — as `fold` shows — something a compiler must be
careful *not* to do at compile time, since folding `10/0` would crash the compiler instead of the
program.

---

## 7. The optimiser: constant folding

```cpp
static NodePtr fold(NodePtr n) {
    if (n->lhs) n->lhs = fold(std::move(n->lhs));       // children first
    if (n->rhs) n->rhs = fold(std::move(n->rhs));
    if (n->tag == Node::Tag::Binary && is_int(n->lhs.get()) && is_int(n->rhs.get())) {
        long long a = n->lhs->value, b = n->rhs->value;
        if ((n->op == Kind::Slash || n->op == Kind::Percent) && b == 0)
            return n;                                    // do NOT fold; leave the error
        switch (n->op) {
            case Kind::Plus: return Node::make_int(a + b, n->pos);
            /* ... */
        }
    }
    return n;
}
```

**Bottom-up matters.** We fold the children *before* looking at the node, so `3 * (10 - 4)` becomes
`3 * 6` becomes `18` in a single pass. Top-down would need repeated passes to reach the same fixpoint —
and "run passes until nothing changes" is exactly the pattern Chapter 41 formalises.

**The division guard is the whole ethics of optimisation in three lines.** An optimiser may change
*how*, never *what*. Folding `10/0` would replace a program that *fails at run time* with a compiler
that *crashes at compile time*. Real compilers are full of such guards: you may not fold
`INT_MIN / -1` (it overflows), you may not reassociate floating-point additions (they are not
associative), you may not remove a division you thought was dead if it could trap. Every one of those
is a "do not fold" case somebody learned the hard way.

**Notice what folding does to our language.** With only literals and operators, *every* expression folds
to a single number, so the generated code is always `mov rax, <answer>`. Our compiler has, accidentally,
become perfect. That is a sign the language is too small: the interesting work in a compiler begins with
*variables*, values not known until run time. Which is precisely where Part 1 takes us.

---

## 8. Code generation: a stack machine on real hardware

The x86-64 CPU has sixteen general-purpose registers and no way to say "evaluate this tree". We need a
strategy for turning nested expressions into a flat instruction sequence. The simplest one that always
works is to use the hardware stack:

| Tree | Emitted code | Stack after |
|------|--------------|-------------|
| `Int n` | `mov rax, n` / `push rax` | `… n` |
| `a op b` | code for `a`; code for `b`; `pop r10`; `pop rax`; *op*; `push rax` | `… (a op b)` |
| `-a` | code for `a`; `pop rax`; `neg rax`; `push rax` | `… -a` |

Every subexpression leaves exactly one value on the stack; every operator consumes two and leaves one.
By induction, the whole expression leaves exactly one value, which we `pop` into `rax` at the end. It
works at any nesting depth, with no register bookkeeping at all, because the hardware stack *is* the
bookkeeping.

```cpp
case Node::Tag::Binary:
    gen_expr(n->lhs.get(), os);        // lhs on the stack
    gen_expr(n->rhs.get(), os);        // rhs on top of it
    os << "        pop     r10\n";     // r10 = rhs  (top!)
    os << "        pop     rax\n";     // rax = lhs
    switch (n->op) {
        case Kind::Plus:  os << "        add     rax, r10\n"; break;
        case Kind::Minus: os << "        sub     rax, r10\n"; break;
        case Kind::Star:  os << "        imul    rax, r10\n"; break;
        case Kind::Slash: os << "        cqo\n        idiv    r10\n"; break;
        case Kind::Percent:
            os << "        cqo\n        idiv    r10\n        mov     rax, rdx\n"; break;
        default: break;
    }
    os << "        push    rax\n";
```

**Pop order is a real bug source.** The right operand was pushed last, so it comes off first. Swap the
two `pop`s and `10 - 4` computes `-6`. (Try it — exercise 3.)

**`cqo` before `idiv`.** x86's signed division divides the 128-bit value in `rdx:rax` by its operand,
putting the quotient in `rax` and the remainder in `rdx`. So before dividing, `rdx` must hold the sign
extension of `rax`, which is what `cqo` does. Forget it and `7 / 2` gives garbage or a hardware
exception. This is the flavour of Part 6: the machine has opinions, and you must know them.

**Why `r10`?** Because it is a caller-saved scratch register that is not used for arguments in either
calling convention, so it can never collide with the `printf` call. Choosing registers to avoid
collisions, by hand, for a whole function, is exactly the job that Chapters 50 and 51 automate.

### The wrapper: prologue, printf, epilogue

```asm
        .intel_syntax noprefix
        .globl  main
        .text
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 32          # shadow space (Windows) + keeps rsp 16-byte aligned
        ...                      # expression code
        pop     rax              # the answer
        lea     rcx, [rip + fmt] # 1st argument: the format string   (rdi on Linux)
        mov     rdx, rax         # 2nd argument: the value           (rsi on Linux)
        call    printf
        xor     eax, eax         # return 0
        mov     rsp, rbp
        pop     rbp
        ret
        .section .rdata          # .rodata on Linux
fmt:
        .asciz  "%lld\n"
```

Four things here that Part 6 spends chapters on:

* **`.intel_syntax noprefix`** asks the GNU assembler for Intel syntax (`mov dst, src`) instead of its
  default AT&T syntax (`movq %src, %dst`). Intel order matches the manuals and most documentation.
* **The prologue/epilogue** establish a frame pointer so that locals could be addressed as `[rbp-8]`.
  We have no locals yet; Chapter 48 does.
* **`sub rsp, 32`** is the Microsoft x64 *shadow space*: a caller must leave 32 bytes for the callee to
  spill its first four register arguments. Omit it on Windows and `printf` corrupts your stack.
  Chapter 48 explains why the two conventions differ, and why it is 32.
* **The argument registers differ by platform** — `rcx, rdx` on Windows, `rdi, rsi` on Linux/macOS —
  so the generator picks them with `#ifdef _WIN32`. That one `#ifdef` is the entire ABI portability
  story of this chapter; Chapter 48 turns it into a proper target description.

**Stack alignment, since it bites everyone once:** at the moment of a `call`, `rsp` must be a multiple
of 16. On entry to `main` the return address has made it ≡ 8 (mod 16); `push rbp` brings it to 0;
`sub rsp, 32` keeps it at 0; and our pushes and pops are perfectly balanced, so at `call printf` it is
still 0. Correct. An unbalanced stack here causes a crash inside `printf` with a stack trace pointing
at code you did not write — a classic, and Chapter 48's most common bug.

---

## Run it

```bat
run ch04_tiny_compiler
```

or with your own expression (quote it, so the shell does not eat the `*`):

```bat
run ch04_tiny_compiler "7 * (1 + 2) - 100 / 5 % 3"
```

Then turn the output into a real program:

```bat
gcc out\tiny.s -o out\tiny.exe
out\tiny.exe
```

---

## What you should see

```
=== 0. SOURCE ===
  2 + 3 * (10 - 4)

=== 1. TOKENS ===
  Int(2) @0
  Plus @2
  Int(3) @4
  Star @6
  LParen @8
  Int(10) @9
  Minus @12
  Int(4) @14
  RParen @15
  End @16

=== 2. AST ===
  Binary +
    Int 2
    Binary *
      Int 3
      Binary -
        Int 10
        Int 4
  as an expression: (2 + (3 * (10 - 4)))

=== 3. INTERPRETED ===
  20

=== 4. OPTIMISED (constant folding) ===
  Int 20
  3 subtree(s) folded at compile time

=== 5. X86-64 ASSEMBLY ===
# ------------------------------------------------------------
# Generated by ch04_tiny_compiler from:  2 + 3 * (10 - 4)
# Assemble and run:   gcc tiny.s -o tiny.exe && tiny.exe
# ------------------------------------------------------------
        .intel_syntax noprefix
        .globl  main
        .text
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 32
        mov     rax, 20
        push    rax
        pop     rax
        lea     rcx, [rip + fmt]
        mov     rdx, rax
        call    printf
        xor     eax, eax
        mov     rsp, rbp
        pop     rbp
        ret
        .section .rdata
fmt:
        .asciz  "%lld\n"

Wrote out/tiny.s  ->  now run:
    gcc out/tiny.s -o out/tiny.exe
    out\tiny.exe
```

And then:

```
> gcc out\tiny.s -o out\tiny.exe
> out\tiny.exe
20
```

**That `20` is the point of the chapter.** A number you typed as text was turned by *your* program into
instructions that a CPU executed. Everything after this is refinement.

Notice also the `push rax` / `pop rax` pair sitting there doing nothing. That is a real
inefficiency, produced by a naive code generator, and removing exactly such pairs is **peephole
optimisation** — Chapter 53, thirty lines, and it will delete this instantly.

---

## Try it yourself

1. **One minute.** Run it on `2 + 3 * 4` and on `(2 + 3) * 4`. Compare the `as an expression:` lines.
2. **Two minutes.** Run `1 - 2 - 3`. You should get −4. Now change `parse_expr(prec + 1)` to
   `parse_expr(prec)`, rebuild, and run it again. You get 2. You have just made subtraction
   right-associative. Change it back.
3. **Two minutes.** Swap the two `pop`s in the `Binary` case of `gen_expr` and run `10 - 4` through
   `gcc`. You get −6. Operand order is not something you can guess at.
4. **Five minutes.** Comment out the `tree = fold(std::move(tree));` line and run again. Now the
   assembly contains the whole stack machine — eighteen instructions instead of two. Read it, and follow
   the stack by hand. This is the code the rest of Part 6 learns to improve.
5. **Ten minutes.** Add the `**` (power) operator, right-associative, binding tighter than `*`. You need:
   a `Kind::StarStar`, two-character lexing (peek at `src[i+1]`), a precedence of 30, and — because it is
   right-associative — a recursive call with `prec`, not `prec + 1`. Check that `2 ** 3 ** 2` is 512 and
   not 64.
6. **Ten minutes.** Add a `--no-fold` command-line flag, and a `--asm-only` flag that prints nothing but
   the assembly. You now have a real compiler's command line, and it will grow all book.
7. **Twenty minutes.** Make the error message better. `2 + * 3` currently says
   `expected a number, '-' or '(', found Star`. Make it point at the `+` too, with a note saying
   *"this operator needs a right-hand operand"*. Compare your result with the design in Chapter 10.
8. **Half an hour.** Add variables: `x = 3; x * 2`. You will need a `std::map<std::string, long long>`
   for the interpreter, and for the code generator you will need to *allocate stack slots*. You will
   quickly feel why Part 3 (symbol tables) and Chapter 48 (stack frames) exist. Do not aim to finish —
   aim to hit the wall, so the later chapters land.

---

## Common problems

**`gcc out\tiny.s` says `Error: junk at end of line, first unrecognized character is '['`.**
Your assembler is reading AT&T syntax. Check that the first emitted line is
`.intel_syntax noprefix`, and that you are calling `gcc` (which recognises `.s`), not `as` directly.

**The program crashes inside `printf` (or prints nothing).**
Stack alignment or shadow space. Confirm `sub rsp, 32` is present, and that every `push` in the
expression code has a matching `pop`.

**On Linux: `undefined reference to printf` or a segfault.**
Check the `#else` branch is being taken: arguments must be `rdi`/`rsi`, and the section `.rodata`. If
you copied a Windows-generated `.s` to Linux, regenerate it there.

**On macOS: `symbol "main" is undefined` or a link error about `_main`.**
Mach-O prefixes symbols with an underscore. You need `_main` and `_printf`, and `.section
__TEXT,__text`. Chapter 52 covers the three object formats; for now, use the bytecode path or Linux.

**Very long expression, then a crash.**
`parse_expr` recurses once per operator, and `eval`/`gen_expr` once per node. A few hundred thousand
nested operators will overflow the stack. Real compilers impose a nesting limit and say so; ours does
in Chapter 19.

**`10 / 0` prints an error instead of folding.**
That is correct. See §7.

**Huge numbers give nonsense.**
`v = v * 10 + digit` silently overflows `long long` past 9,223,372,036,854,775,807. Detecting that is
Chapter 8, and it is a surprisingly deep little problem.

---

## Check yourself

1. Why does `parse_expr` take a `min_prec` argument rather than consulting a global "current
   precedence"?
2. `(2+3)` and `2+3` produce the same AST. Name a tool that would be *harmed* by that, and say what it
   would need instead.
3. In the generated code, why must `pop r10` come before `pop rax`?
4. Constant folding turned the whole program into `mov rax, 20`. What language feature would make
   folding *stop* being able to do that?
5. Why is `fold` careful about `/ 0` when `eval` reports it as an error anyway?

<details>
<summary>Answers</summary>

1. Because the answer is different at every depth, simultaneously — the outer call is willing to accept
   `+` while the inner one is not. That is the definition of a *parameter* rather than a global. A
   global would have to be saved and restored around every recursive call, which is a parameter with
   extra steps and a bug waiting to happen.
2. A formatter or a refactoring tool: `clang-format` must not delete your parentheses, and an IDE
   "extract variable" must reproduce them. They need a *concrete* syntax tree that keeps every token,
   including punctuation and comments, or an AST that records the original source span of each node so
   the original text can be recovered. Chapter 23 builds the second of those.
3. Because the right-hand operand was pushed last, so it is on top of the stack, and `pop` takes the
   top. Getting it backwards makes non-commutative operators (`-`, `/`, `%`) silently wrong, which is
   worse than a crash.
4. Anything whose value is not known until run time: a variable, a function parameter, reading input, a
   call to an unknown function. That is why folding is only *one* pass among many — for real programs
   most of the work is reasoning about values you do *not* know. Part 5 is about exactly that.
5. Because `fold` runs at *compile* time. `eval` running `10/0` is a compile-time diagnostic in our tiny
   program, but in a real compiler the folded expression might be inside a branch that never executes at
   run time — `if (n != 0) { return 10 / n; }` with `n` known to be 0 on the *other* path. Folding a
   trapping operation moves a possible run-time fault into a definite compile-time one, changing the
   program's meaning. The general rule: an optimiser must never introduce a fault that the original
   program would not have had, and never remove one it would.
</details>

---

## Where we are

You have written a compiler. It has all six stages, it produces real machine code, and it reports errors
with a caret. It is also about 1% of the way to a usable language, and the missing 99% is exactly what
makes languages interesting: names, types, control flow, functions, memory.

Part 1 starts at the beginning and does it properly. The first thing to get right is the thing this
chapter faked with a global variable: knowing *where in the file* we are.

[Next: Chapter 5 — Source text →](05-source-text.md)
