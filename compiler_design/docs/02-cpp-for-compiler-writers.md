# Chapter 2 — C++ for compiler writers

[← Setting up](01-setup.md) · [Contents](README.md) · [Next: The pipeline →](03-the-pipeline.md)

> 📖 **Line by line:** [ch02_cpp_tour explained line by line](line-by-line/ch02_cpp_tour.md)

---

## Goal

Learn the ten C++ idioms this book uses, and — more importantly — *why a compiler wants exactly these
ten*. Compiler code has an unusual shape: enormous numbers of small immutable records, deep recursion
over trees, "one of N kinds" values everywhere, and a hard requirement that adding a new kind of thing
never silently breaks an old switch. C++ has features aimed at precisely that, and a few traps.

If you already know modern C++, read section 3 (indices vs pointers) and section 5 (variant vs
virtual) and skip the rest. If your C++ is from 2005, read all of it: `unique_ptr`, `variant`,
`optional` and `string_view` change how compiler code is written.

The whole chapter is one runnable program, `chapters/ch02_cpp_tour.cpp`.

---

## 0. The dialect we use

Rules, so that nothing in the rest of the book surprises you:

* **C++17.** No C++20 (`std::format`, concepts, ranges) so that older compilers work.
* **`struct` by default, `class` when there is an invariant to protect.** Most compiler data is plain
  data: a token is four fields, an AST node is a few pointers. `struct` with public members is honest
  about that. We reach for `class` and private members when a type has a rule to enforce (the interner
  must never hand out two different ids for the same string).
* **No exceptions in the compiler's own logic.** A syntax error is not exceptional — it is the most
  common input. We report errors through a diagnostics object (Chapter 10) and keep going. Exceptions
  do appear from the standard library (`std::bad_alloc`) and we let them terminate.
* **Shallow inheritance, or none.** At most one level: `Expr` → `Binary`. No inheritance for code reuse.
* **Templates only where they remove real duplication.** We are writing a compiler, not a library.
* **Everything in `namespace pebble`.** One namespace, no nesting, so that `pebble::Token` reads well.
* **Headers are self-contained.** Every `.h` includes what it uses and starts with `#pragma once`.

---

## 1. `enum class` and the exhaustive switch

A compiler is a machine for classifying. Token kinds, AST node kinds, type kinds, IR opcodes,
diagnostic severities, register classes: each is a fixed set of alternatives.

```cpp
enum class TokenKind { Int, Plus, Star, LParen, RParen, End };
```

Why `enum class` and not a plain `enum`?

* **No name leaking.** A plain `enum { Int, Plus }` puts `Int` in the surrounding scope, which collides
  the moment you also have a `TypeKind::Int`. With `enum class` you write `TokenKind::Int`.
* **No implicit conversion to `int`.** `if (tok.kind == 3)` will not compile. In a program with eight
  different small-integer-like enums, that is a real class of bug removed.
* **A specifiable underlying type.** `enum class Opcode : std::uint8_t` makes an IR instruction
  smaller, which matters when there are millions.

The crucial habit is the **switch with no `default`**:

```cpp
static const char* kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::Int:    return "Int";
        case TokenKind::Plus:   return "Plus";
        case TokenKind::Star:   return "Star";
        case TokenKind::LParen: return "LParen";
        case TokenKind::RParen: return "RParen";
        case TokenKind::End:    return "End";
    }
    return "<invalid>";
}
```

Add `Slash` to the enum, rebuild, and `-Wall` says:

```
warning: enumeration value 'Slash' not handled in switch [-Wswitch]
```

That warning is the most valuable line of output in this entire book. A compiler is grown by adding
kinds — a new token, a new node, a new opcode — and each addition needs attention in a dozen switches
scattered over thousands of lines. The compiler finds them for you, *if* you never write `default:`.
When you truly need a catch-all, list the remaining cases explicitly:

```cpp
case TokenKind::LParen:
case TokenKind::RParen:
    return nullptr;     // still fails to compile^Wwarn when a new kind appears
```

> **Rule for this book:** no `default:` in a switch over one of our own enums. Exceptions get a comment
> explaining why.

---

## 2. `std::string_view`: text without copies

A 10,000-line source file has perhaps 60,000 tokens. If each token owns a `std::string` for its text,
that is 60,000 heap allocations to lex a small file. Compilers are judged on speed, so we do not do
that.

A `std::string_view` is a pointer and a length — a *window* onto characters someone else owns:

```cpp
struct Token {
    TokenKind        kind;
    std::string_view text;    // points INTO the source buffer, does not own it
    std::size_t      offset;  // where it started, for error messages
};
```

The source file is read once into one `std::string` that lives for the whole compilation; every token's
`text` points into it. Cost per token: 24 bytes, zero allocations.

**The rule, and it is absolute: the buffer must outlive every view into it.** The classic bug:

```cpp
std::string_view name_of(int i) {
    std::string s = "tmp" + std::to_string(i);
    return s;               // DANGLING. s dies here; the view points at freed memory.
}
```

We avoid it by the design decision in Chapter 5: `SourceFile` owns its text, is created once, and is
never copied or moved. Everything else borrows.

Also useful: `string_view` has `substr`, `find`, `==`, `starts_with` (C++20) and prints with `<<`. It
does *not* guarantee a terminating `\0`, so never pass `.data()` to a C function expecting a C string.

---

## 3. Store indices, not pointers, into vectors

This is the idiom that separates people who have debugged a compiler from people who are about to.

```cpp
std::vector<FlatNode> nodes;
nodes.push_back(...);
const FlatNode* p = &nodes[0];     // fine right now
for (int i = 0; i < 100; ++i)
    nodes.push_back(...);          // vector grows: reallocates, moves elements
// p is now a dangling pointer. Reading *p is undefined behaviour.
```

A `std::vector` guarantees contiguous storage, which means that when it outgrows its capacity it
allocates a bigger block and *moves* everything. Pointers, references and iterators into it are
invalidated. Compilers build vectors incrementally *constantly* — instructions appended to a basic
block, blocks appended to a function, symbols appended to a scope — and it is very natural to hold a
pointer to "the instruction I am working on" across a `push_back` that happens three calls down the
stack. The resulting bug is intermittent, depends on the input size, and disappears under a debugger.

So: **give things small integer ids and pass the ids around.**

```cpp
using NodeId = std::uint32_t;      // a named index
NodeId lhs = 4;
nodes[lhs].value;                  // always valid, whatever the vector did
```

The benefits go beyond safety:

* **Half the size.** A 32-bit id instead of a 64-bit pointer; IR nodes are mostly ids, so the whole
  representation shrinks, and cache behaviour improves measurably.
* **Trivially serialisable.** You can write the vectors to disk or hash them without pointer fixups —
  which is how incremental compilation caches work (Chapter 61).
* **Comparable and stable.** Ids give you a canonical order for free, which several algorithms want
  (Chapter 38's dominance computation, Chapter 51's colouring).
* **Debuggable.** `%4 = add %2, %3` in a dump is readable; `0x7ffd3a00 = add 0x7ffd39c0, ...` is not.

The cost: no type safety between id kinds (a `NodeId` and a `BlockId` are both `uint32_t`), and an
extra indirection through the vector. Chapter 34 shows how to get the type safety back with a
one-line wrapper struct when the confusion starts to hurt.

We use both styles in this book, deliberately: `unique_ptr` trees for the AST (Part 2), where nodes are
created once and never move, and ids for the IR (Part 4), where instructions are appended, deleted and
re-ordered by every optimisation pass.

---

## 4. `std::unique_ptr`: ownership for trees

An AST is a tree: every node has exactly one parent, and when the parent dies the children should die.
That is precisely `std::unique_ptr`.

```cpp
struct Expr {
    virtual ~Expr() = default;              // essential: see below
    virtual long long eval() const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;

struct Binary : Expr {
    TokenKind op;
    ExprPtr   lhs, rhs;                     // owns its children
    Binary(TokenKind o, ExprPtr l, ExprPtr r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};
```

Four things to notice.

**The virtual destructor.** Deleting a `Binary` through an `Expr*` without a virtual destructor is
undefined behaviour, and in practice leaks the children. If a class has any virtual function, give it a
virtual destructor. Always.

**`std::move` in the constructor.** A `unique_ptr` cannot be copied — that is the whole point, it is
*unique*. It can be *moved*: the pointer is transferred and the source becomes null. `std::move` does
not move anything; it is a cast that says "you may treat this as expiring, so steal from it".

**No `delete`, anywhere, ever.** When the root `ExprPtr` goes out of scope, its destructor deletes the
`Binary`, whose destructor destroys its two `ExprPtr` members, recursively. The entire tree is freed
with no code from us and no leaks. This is RAII, and it is why modern C++ is pleasant for tree work.

**Why not `shared_ptr`?** Because sharing is not what is happening. A `shared_ptr` costs an atomic
refcount increment per copy, makes cycles leak, and — worst — *hides* the ownership question. If two
parts of a compiler point at the same node, you want that visible, and you want it to be a
`Expr*` borrow (non-owning) with a documented lifetime, not a silent co-ownership.

A caution for later: deep recursion on a deep tree can overflow the stack — both when building it and
when the destructor unwinds. `a+a+a+...` a hundred thousand times is a legal program and a real bug
report for real compilers. Chapter 19 sets a nesting limit for exactly this reason.

---

## 5. `std::variant`: sum types without inheritance

The other way to say "one of N shapes":

```cpp
struct VInt { long long value; };
struct VAdd { int lhs, rhs; };
struct VMul { int lhs, rhs; };
using VExpr = std::variant<VInt, VAdd, VMul>;
```

A `variant` holds exactly one of its alternatives, *by value*, in one object big enough for the
largest. No allocation, no virtual table, no pointer chasing.

You inspect it with `std::visit` and a visitor whose `operator()` is overloaded per alternative:

```cpp
struct EvalVisitor {
    const std::vector<VExpr>& pool;
    long long operator()(const VInt& n) const { return n.value; }
    long long operator()(const VAdd& n) const {
        return std::visit(*this, pool[n.lhs]) + std::visit(*this, pool[n.rhs]);
    }
    long long operator()(const VMul& n) const { /* ... */ }
};
```

Miss an alternative and it does not compile — a *stronger* guarantee than the `-Wswitch` warning,
because it is an error rather than a warning.

### Which should you use, and when?

| | `unique_ptr` + virtual | `variant` + visit |
|---|---|---|
| Adding a **new kind** | edit one new file; existing code untouched | edit the `using`, then every visitor breaks (usefully) |
| Adding a **new operation** | edit *every* class | write one new visitor |
| Size | one allocation per node, pointer-sized handles | size of the largest alternative, no allocation |
| Speed | indirect call per dispatch | jump table, inlinable |
| Recursive shapes | natural (a node holds `ExprPtr`) | needs indices or `unique_ptr` inside — a variant cannot contain itself |

This is the *expression problem*, and it has no free answer: object-oriented code makes new kinds cheap
and new operations expensive; functional/variant code the opposite.

**Our choice.** The AST gets `unique_ptr` + virtual, because the set of node kinds is large and grows
throughout Part 2 while the operations on it are few and centralised (resolve, typecheck, lower,
print). The IR gets a flat `struct Instruction` with an opcode field and id operands, because the
opcode set is *small and fixed* while the number of passes that walk it is large and grows through all
of Part 5. Chapters 15 and 34 argue each choice in full.

There is a third option worth knowing: the **tagged struct**, a single `struct` with a kind enum and a
union of payloads, hand-rolled. That is what Clang, GCC and most production compilers actually use for
their IRs, for control over layout. Our IR is that, minus the union.

---

## 6. `std::optional`: "there might not be an answer"

```cpp
std::optional<int> precedence_of(TokenKind k) {
    switch (k) {
        case TokenKind::Plus: return 10;
        case TokenKind::Star: return 20;
        default:              return std::nullopt;
    }
}

if (auto p = precedence_of(k))        // converts to bool
    use(*p);                          // dereference to get the value
```

Compare the alternatives: returning `-1` (is that a valid precedence? the reader cannot know),
returning a `bool` and writing through an out-parameter (two things to keep in sync), or returning a
pointer (and now: who owns it?). `optional<T>` says "a `T`, or nothing" in the type, and forces the
caller to consider the nothing case.

We use it for: symbol lookup that may fail, a token's numeric value, an optional `else` branch, a
function's declared return type, and constant-folding results ("could I fold this? maybe not").

`value_or(default)` is often the shortest thing to write. `*opt` when empty is undefined behaviour;
`opt.value()` throws instead. In this book we always check first.

---

## 7. String interning

Identifiers repeat: in a typical file `i`, `self`, `result` and `length` appear hundreds of times. A
symbol table keyed on `std::string` therefore hashes the same characters over and over, and name
comparison during resolution is a `memcmp`.

Interning fixes this: store every distinct spelling *once*, hand out a small integer, compare integers.

```cpp
class Interner {
public:
    using Sym = std::uint32_t;
    Sym intern(std::string_view text);          // same text -> same id, always
    const std::string& text(Sym s) const;
private:
    std::vector<std::string>             strings_;
    std::unordered_map<std::string, Sym> map_;
};
```

After interning, `a == b` for names is one integer comparison, symbol tables can be `vector`s indexed
by `Sym`, and dumps stay readable because `text(s)` gets the spelling back. Rust, Clang and GCC all do
this (Clang calls it `IdentifierInfo`, Rust calls it `Symbol`). Chapter 9 adds it to our lexer, and gets
keyword recognition for free as a side effect: intern the keywords first, so `Sym` 0..20 *are* the
keywords and the check is `s < NUM_KEYWORDS`.

Note `std::unordered_map<std::string, Sym>` and not `<std::string_view, Sym>`: the map must own the key,
because we want the interner to keep the string alive even if the source buffer goes away.

---

## 8. RAII for anything paired

Entering a block pushes a scope; leaving it must pop — on *every* exit path, including an early
`return` after an error. Doing that by hand is how symbol tables get corrupted.

```cpp
class ScopeGuard {
public:
    explicit ScopeGuard(ScopeStack& s) : s_(s) { s_.push(); }
    ~ScopeGuard() { s_.pop(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
private:
    ScopeStack& s_;
};

void check_block(const Block& b) {
    ScopeGuard guard(scopes_);        // push
    for (auto& stmt : b.stmts) {
        if (!check(stmt)) return;     // pop still happens
    }
}                                     // pop happens here
```

Deleting the copy constructor is not pedantry: a copied guard would pop twice.

We use this pattern for scopes (Chapter 24), for "currently inside a loop" flags (Chapter 31), for
indentation in the AST printer (Chapter 23), and for timing a pass (Chapter 65).

---

## 9. Building text

Dumps, error messages and emitted assembly are all text built up piece by piece. Without C++20's
`std::format`, the tool is `std::ostringstream`:

```cpp
std::string describe(const Token& t) {
    std::ostringstream os;
    os << kind_name(t.kind) << "('" << t.text << "') @" << t.offset;
    return os.str();
}
```

Two conventions in this book:

* **Anything printable takes an `std::ostream&`**, rather than returning a string. Then the same
  function can write to `std::cout`, to a file, or to an `ostringstream`, and printing a big AST does
  not build a huge string in memory.
* **One `dump()` per subsystem.** `dump_tokens`, `dump_ast`, `dump_ir`, `dump_asm`. Each is 30 lines,
  each is worth its weight in gold the first time a stage misbehaves. Write the dumper *before* the
  stage that needs debugging, not after.

`std::cout << x` on a `double` gives 6 significant digits by default — a real source of confusing
"wrong constant folding" reports. Use `os << std::setprecision(17)` when the exact value matters.

---

## 10. Assertions

```cpp
assert(e != nullptr && "eval of a null node means the parser returned garbage");
```

An `assert` is a comment the compiler checks. The `&& "message"` trick works because a non-empty string
literal is always true, so the condition is unchanged, but the failure message prints the text.

The line between assert and error is worth stating precisely, because getting it wrong in a compiler is
a crash on someone's bad input:

* **`assert`** for things that are true *if my compiler is correct*: "a `Binary` node has two children",
  "every basic block ends in a terminator", "the register allocator assigned every virtual register".
* **A diagnostic** for anything that depends on *the user's input*: missing semicolons, unknown names,
  type mismatches, a 5 GB source file, invalid UTF-8.

Asserts vanish under `-DNDEBUG`, so never put side effects inside one. And in a compiler, an assert
firing is good news: it caught an internal inconsistency close to its cause, instead of producing a
wrong executable that misbehaves for a user next year.

---

## Run it

```bat
run ch02_cpp_tour
```

## What you should see

```
source: let x = 42 + 1;

-- 1. enum class --
kind_name(Star) = Star
as int         = 2

-- 2. string_view --
text = '42'  size = 2  offset = 8
points inside source? 1

-- 3. indices, not pointers --
vector data() moved? yes - the pointer is now dangling
nodes[safe].value = 2

-- 4. unique_ptr tree --
(2 + (3 * 4)) = 14

-- 5. variant --
2 + 3 * 4 = 14
index of root alternative = 1
is the root a VAdd? 1

-- 6. optional --
Plus has precedence 10
Star has precedence 20
LParen is not an operator

-- 7. interning --
price -> 0, total -> 1, price again -> 0
a == c ? 1   distinct strings stored: 2
text(b) = total

-- 8. RAII scopes --
depth = 1
  depth = 2
    depth = 3
back at depth 0 (must be 0)

-- 9. ostringstream --
Int('42') @8

-- 10. assert --
safe_eval(5) = 5
(an assert failure would abort the program with file and line)

All ten idioms demonstrated.
```

The `1`s are `bool`s printed by `std::cout` — add `std::boolalpha` if you prefer `true`.

---

## Try it yourself

1. **Two minutes.** Add `Slash` to `TokenKind` and rebuild. Read the warning. Now fix `kind_name`.
   Then delete a `case` and add `default: return "?";` and rebuild — notice that the warning is gone
   and the bug is not. This is the single most useful thing in this chapter.
2. **Five minutes.** In `demo_indices`, print `danger->value` after the loop. It may print 2, or
   garbage, or crash. Run it a few times, then with `-O0` and with `-O2`. Undefined behaviour is not a
   crash; it is *anything*.
3. **Ten minutes.** Add a `Neg` node to the `unique_ptr` tree (unary minus) and make `print` and `eval`
   handle it. Then add the same to the `variant` version. Count how many places you had to edit in
   each. That count is the expression problem, measured.
4. **Fifteen minutes.** Make `Interner::intern` take `std::string_view` and avoid constructing a
   `std::string` when the entry already exists. (Hint: a heterogeneous lookup needs C++20, so instead
   keep a second `std::unordered_map<std::string_view, Sym>` whose keys point into `strings_` — and
   then explain to yourself why `strings_` must be a `std::deque` or must `reserve` for that to be
   safe. This is idiom 3 again, in disguise.)
5. **Half an hour.** Write a `Timer` RAII class that prints the elapsed microseconds in its destructor,
   and wrap each `demo_*` call in one.

---

## Common problems

**`error: use of deleted function 'std::unique_ptr<T>::unique_ptr(const std::unique_ptr<T>&)'`**
You copied a `unique_ptr`. Add `std::move(...)`, or take it by reference, or ask whether the function
should own it at all.

**Crash on exit, or a leak reported, with a tree.**
Missing `virtual ~Expr()`.

**`error: cannot bind non-const lvalue reference` when calling `std::visit`**
Your visitor's `operator()` is not `const`, or you passed a `const` variant to a non-const visitor. Mark
visitor methods `const`.

**A `string_view` prints garbage.**
The buffer it points into died, moved or was reallocated. Find where the string it viewed was created,
and check it outlives the view. In our compiler, the only legitimate owner of source text is
`SourceFile`.

**`std::variant` does not compile with a recursive type.**
A variant needs the sizes of its alternatives, so it cannot contain itself. Use an index (as we did) or
a `unique_ptr` to break the cycle.

---

## Check yourself

1. Why is a `switch` without `default:` *safer* than one with it, when it is the one that can fall off
   the end?
2. You store `Instruction*` pointers in an optimisation pass, and the pass appends new instructions to
   the same `std::vector`. What happens, and when?
3. When would you choose `variant` over virtual dispatch for an AST?
4. Why does the interner's map own `std::string` keys rather than `std::string_view` keys?
5. `assert(fix_up_all_registers())` — what is wrong with this line?

<details>
<summary>Answers</summary>

1. Because the danger is not "falling off the end" — we return a fallback for that — but *silently
   handling a new case wrongly*. `default:` tells the compiler "I have thought about everything else",
   which is a promise you cannot keep for cases that do not exist yet. Without it, every new enumerator
   produces a list of exactly the places needing attention.
2. On the first `push_back` that exceeds capacity, the vector reallocates and every stored pointer
   dangles. It usually *seems* to work, because small vectors have spare capacity and freed memory is
   often still readable. It then fails on a large input, in a build configuration you were not testing.
3. When the set of *kinds* is fixed and small but the set of *operations* is large and growing, when you
   want no allocation per node, or when you want exhaustiveness as a compile *error* rather than a
   warning. A small expression language in a configuration file is an excellent fit; a full language AST
   with 40 node kinds is not.
4. Because interned symbols must stay valid independently of the source buffer — the REPL (Chapter 60)
   frees each input line, and modules (Chapter 61) are loaded and unloaded. The interner is the one
   place that deliberately owns its strings.
5. The call disappears in a release build (`-DNDEBUG`), so the registers never get fixed up — and the
   bug appears only in release. Never put a side effect inside an `assert`. Write
   `bool ok = fix_up_all_registers(); assert(ok); (void)ok;`
</details>

---

[Next: Chapter 3 — The shape of a compiler →](03-the-pipeline.md)
