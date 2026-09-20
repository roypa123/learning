# Chapter 7 — A hand-written lexer, part 1: the scanner core

[← Regular languages](06-theory-regular-languages.md) · [Contents](README.md) · [Next: Numbers and strings →](08-lexer-part2.md)

> 📖 **Line by line:** [token.h](line-by-line/token.md) · [lexer.h](line-by-line/lexer.md) · [ch07_lexer](line-by-line/ch07_lexer.md)

---

## Goal

Build the real thing: `Token`, `TokenKind`, and the core of `Lexer`. By the end of this chapter the
compiler can read a Pebble file and produce representation 2 — a `std::vector<Token>` where every token
knows its kind, its text and its `Span`.

This chapter covers the skeleton: the cursor, the dispatch, identifiers, whitespace, and the error
reporting path. Chapter 8 fills in numbers, strings and comments; Chapter 9 does keywords and
multi-character operators. All three describe the same file, `include/pebble/lexer.h`, which is complete
from the start so that you can build and run after every section.

---

## 1. `Token`: what we are producing

```cpp
struct Token {
    TokenKind        kind = TokenKind::Eof;
    Span             span;
    std::string_view text;            // exactly the characters, as they appear
    u64              int_value   = 0; // IntLit, CharLit
    double           float_value = 0; // FloatLit
    u32              value_index = 0; // StringLit: index into Lexer::string_values()
    bool             erroneous   = false;
};
```

Four design decisions, each with a reason and a cost.

**`text` is a `string_view`, not a `std::string`.** Chapter 2, idiom 2: a 60,000-token file would
otherwise mean 60,000 heap allocations. The view points into the `SourceFile`'s buffer, which is why
that buffer must outlive the token vector and why `SourceFile` cannot be copied.

**The numeric payload is inline; the string payload is an index.** A decoded string literal is genuinely
new data — `"a\tb"` is five characters in the source and three in memory, so it cannot be a view into the
source. But putting a `std::string` in `Token` would make every token 64 bytes and non-trivially
copyable. So decoded strings go into a side vector owned by the `Lexer`, and the token carries a `u32`
index. This is the "give things small integer ids" idiom again, and Clang, Rust and Go all do the same.

**`span` *and* `text` look redundant** — `text` is `src.slice(span)`. It is a deliberate 16-byte
redundancy: `text` is what the parser compares and prints, `span` is what diagnostics need, and having
both means neither the parser nor the diagnostics engine needs a `SourceFile` reference in hand. If you
want the memory back, delete `text` and pass the `SourceFile` everywhere; measure before you decide.

**`erroneous` marks "a diagnostic was already reported for this token".** It exists so that later stages
can stay quiet about a token they know is already broken. One bad character in a file should produce one
error message, not one per stage. This single flag prevents the "cascade of nonsense" that older
compilers are famous for.

Total size: 32 bytes with typical padding. A million-token file (about 250,000 lines of code) costs 32 MB
of tokens — acceptable, and it means the whole stream stays in memory, which is what makes the parser
simple and backtracking possible.

### `TokenKind`, and the ranges trick

```cpp
enum class TokenKind {
    Eof, Unknown, Comment,
    Ident, IntLit, FloatLit, StringLit, CharLit,
    KwLet, KwVar, KwFn, /* ... */ KwNull,          // <- one contiguous block
    LParen, RParen, /* ... */
    Eq, PlusEq, /* ... */ ShrEq,                   // <- another contiguous block
    COUNT
};

constexpr TokenKind KEYWORD_FIRST = TokenKind::KwLet;
constexpr TokenKind KEYWORD_LAST  = TokenKind::KwNull;

inline bool is_keyword(TokenKind k) { return k >= KEYWORD_FIRST && k <= KEYWORD_LAST; }
```

Grouping related kinds contiguously turns "is this a keyword?" into two integer comparisons instead of a
twenty-case switch. The cost is a rule you must remember: **a new keyword goes inside the keyword
block**. That rule is easy to forget, so the enum has comments marking the ranges, and the exercises ask
you to add a keyword and see what happens if you put it in the wrong place.

`COUNT` at the end is the standard trick for sizing arrays indexed by the enum — the statistics code in
`ch07_lexer.cpp` uses `u32 counts[(int)TokenKind::COUNT]`.

---

## 2. The cursor

The entire scanning machinery is five one-line functions:

```cpp
char cur() const { return src_.at(pos_); }
char peek(u32 n = 1) const { return src_.at(pos_ + n); }
bool at_end() const { return pos_ >= src_.size(); }
char advance() { return src_.at(pos_++); }

bool eat(char c) {                 // consume IF it matches
    if (cur() != c) return false;
    pos_++;
    return true;
}
```

The lexer's entire mutable state is `pos_`, one `u32`. That is worth pausing on: a lexer is a
*deterministic finite automaton* (Chapter 11 makes that precise), and the only thing a DFA remembers is
which state it is in — here, "how far have I read".

**Why none of them needs a bounds check.** `SourceFile::at()` returns `'\0'` past the end. So:

```cpp
while (is_ident_cont(cur())) pos_++;        // stops at EOF, because '\0' is not an ident char
```

No `pos_ < size()` in the condition. Every scanning loop in the file is shorter and faster for it, and
the class of bug where one loop forgot the check simply does not exist. This is the **sentinel**
technique; Clang and V8 both use it (Clang appends an actual `'\0'` to its buffer). The one thing to
remember is that it works *because* `'\0'` is not part of any valid token — if Pebble allowed NUL inside
identifiers, we would need a real flag.

**`eat()` is the whole of maximal munch for operators.** Chapter 9 explains the pattern, but you can see
it already:

```cpp
case '<':
    if (eat('<')) return make(eat('=') ? TokenKind::ShlEq : TokenKind::Shl, start);
    if (eat('=')) return make(TokenKind::Le, start);
    return make(TokenKind::Lt, start);
```

Longest first, always. `<<=` is tested before `<<`, which is tested before `<`. Reverse any two of those
lines and a program that used `<<=` would silently lex as `<<` then `=`.

---

## 3. Our own character predicates (this matters more than it looks)

```cpp
inline bool is_space(char c)  { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool is_digit(char c)  { return c >= '0' && c <= '9'; }
inline bool is_alpha(char c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_ident_start(char c) { return is_alpha(c) || c == '_'; }
inline bool is_ident_cont(char c)  { return is_ident_start(c) || is_digit(c); }
```

Why not `<cctype>`'s `isalpha`, `isdigit`, `isspace`? Three reasons, and all three are real bugs found in
real compilers:

**1. Undefined behaviour on negative `char`.** `std::isdigit(int)` requires its argument to be
representable as `unsigned char` or be `EOF`. On platforms where `char` is signed (x86, ARM by default),
any byte ≥ 0x80 — that is, every UTF-8 continuation byte, every accented character, every emoji — becomes
a *negative int*. That is UB. On MSVC debug builds it trips an assertion and kills your compiler; on
glibc it indexes a lookup table out of bounds and returns whatever is there. The idiomatic fix,
`std::isdigit((unsigned char)c)`, is easy to write and easy to forget in one place out of thirty.

**2. Locale dependence.** `<cctype>` answers according to the *current locale*. Under a Turkish locale,
`toupper('i')` is `'İ'`; under some locales additional bytes are "alphabetic". So the set of valid
identifiers in your language would depend on an environment variable — meaning the same file compiles on
your machine and fails on your colleague's. A compiler must be **deterministic**, so it must not ask the
locale anything. (This is also why we never use `std::stod` or `printf("%f")` for reading and writing
floats in the IR: the decimal separator is locale-dependent. Chapter 8 returns to this.)

**3. Speed.** These inline to two comparisons and a branch. `isalpha` is a function call into a table
lookup that the compiler cannot inline across the library boundary. Lexing is the one phase that touches
every byte of input, so it is the one place where this is measurable — around 10–20% on a lexer-bound
workload.

The general principle, which recurs throughout the book: **a compiler should depend on as little of the
outside world as possible.** Same input, same output, on every machine, forever.

---

## 4. `next()`: trivia, then dispatch

```cpp
Token next() {
    for (;;) {
        u32 start = pos_;
        if (at_end()) return make(TokenKind::Eof, start);

        char c = cur();

        if (is_space(c)) { pos_++; continue; }

        if (c == '/' && peek() == '/') {
            Token t = lex_line_comment();
            if (keep_comments) return t;
            continue;
        }
        if (c == '/' && peek() == '*') {
            Token t = lex_block_comment();
            if (keep_comments) return t;
            continue;
        }

        if (is_ident_start(c)) return lex_ident();
        if (is_digit(c))       return lex_number();
        if (c == '"')          return lex_string();
        if (c == '\'')         return lex_char();

        return lex_operator();
    }
}
```

This function is the shape of every hand-written lexer ever written. Points worth making explicit:

**The dispatch is on the *first character only*.** That is possible because Pebble's token set is
designed so the first character determines the shape: a letter or `_` starts an identifier, a digit
starts a number, `"` starts a string. A language where that is *not* true needs more lookahead, and
every such case is a design smell. (C has one: `.5` is a valid float, so `.` can start a number. Pebble
requires `0.5`, which costs the programmer one character and saves the lexer a special case. Chapter 67
asks you to weigh exactly that kind of trade.)

**It is a loop, not recursion.** Skipping whitespace could be written as `return next();`. It is a loop
because a file could begin with a megabyte of comments, and a megabyte of tail calls is a stack overflow
on any compiler that does not optimise them. Never recurse once per input character.

**Trivia is skipped here, not thrown away earlier.** Whitespace and comments are *trivia*: they separate
tokens and carry no meaning for the parser. But they are not useless — a formatter needs them, a
documentation generator needs doc comments, a syntax highlighter needs comment spans. So the lexer is
given a switch:

```cpp
bool keep_comments = false;      // the compiler wants false; tools want true
```

That one flag is why Chapter 23's formatter and Chapter 63's language server can reuse this lexer
instead of having their own. Real compilers that did not plan for it (GCC, for years) ended up with
separate, subtly different lexers in their tooling — and tooling that disagrees with the compiler about
what a token is produces very confusing bugs.

**`make()` builds the span and the text in one place:**

```cpp
Token make(TokenKind kind, u32 start) const {
    Token t;
    t.kind = kind;
    t.span = Span(start, pos_);       // from where we began, to wherever we now are
    t.text = src_.slice(t.span);
    return t;
}
```

Every token is produced through `make`, so no token can ever be missing its span — the single most
valuable invariant in the front end. If you take one habit from this chapter, take that one: **make it
impossible to construct the thing without its source position.**

---

## 5. Identifiers

```cpp
Token lex_ident() {
    u32 start = pos_;
    while (is_ident_cont(cur())) pos_++;
    /* ... non-ASCII check ... */
    Token t = make(TokenKind::Ident, start);
    if (auto kw = lookup_keyword(t.text)) t.kind = *kw;
    return t;
}
```

Four lines of real work.

**The loop is maximal munch.** `is_ident_cont` includes digits, so `price2` is one token. The *start*
character was already known to be `is_ident_start` — which excludes digits — so `2fast` cannot be an
identifier; the dispatch sent `2` to `lex_number`, which then complains (Chapter 8).

**Keywords are looked up afterwards.** The lexer scans an identifier and *then* asks a table whether that
spelling is reserved. Consequences:

* Adding a keyword to Pebble costs one enumerator, one name and one table row. No lexer code changes.
* Keywords automatically obey maximal munch: `iffy` is scanned as a 4-character identifier, and `iffy` is
  not in the table, so it stays an identifier. We get the behaviour Chapter 6 said we needed, free.
* `if` is scanned as a 2-character identifier and *is* in the table, so it becomes `KwIf`.

This is why Chapter 6's "on a tie the first rule wins" disappears from the implementation: with a lookup
after the fact, there is no tie to break. The ordering discipline is only needed by *generated* lexers,
where keywords really are competing rules.

Chapter 9 measures the lookup (a linear scan over twenty short strings beats a hash map at this size) and
shows the perfect-hash version that GCC uses.

### The non-ASCII check

```cpp
if (static_cast<u8>(cur()) >= 0x80) {
    u32 bad = pos_;
    while (static_cast<u8>(cur()) >= 0x80) pos_++;
    Token t = make(TokenKind::Ident, start);
    diag_.error(Span(bad, pos_), "non-ASCII character in identifier")
        .code("E0002")
        .note("Pebble identifiers are ASCII letters, digits and '_'");
    t.erroneous = true;
    return t;
}
```

Pebble identifiers are ASCII (Chapter 5 §3 argued why). But *silence* is not the same as a *message*.
Without this check, `naïve` lexes as `na`, then an unknown character, then `ve` — three tokens, one
baffling error, and a parse failure on the next line.

The cases that actually happen, all of which this catches:

* a non-breaking space (U+00A0) pasted from a web page or a Word document;
* a "smart quote" (U+2018/U+2019) from a text editor that autocorrected `'`;
* an accented letter in a name (`café`, `naïve`, `größe`);
* a Cyrillic or Greek lookalike, sometimes maliciously.

The token is still returned as an `Ident` with `erroneous = true`, so the parser can carry on and find
more errors instead of collapsing. That policy — *always return something plausible, mark it, and keep
going* — is the heart of error recovery, and Chapter 19 makes it a rule.

---

## 6. The error path

The lexer never throws, never exits, never prints directly. It calls:

```cpp
diag_.error(span, "message").code("E0002").note("...").help(span, "fix", "did you mean...");
```

and continues. `Diagnostics` (Chapter 10) collects them; the driver decides when to stop. Three
consequences:

* **One run reports many errors.** Fix ten typos per compile instead of one.
* **The lexer is testable.** A test can lex a string and assert on the *list* of diagnostics, without
  capturing stdout or catching exceptions.
* **A tool can use the lexer without the diagnostics being printed anywhere.** A language server turns
  the same list into squiggles in your editor.

The `Unknown` token exists for the same reason:

```cpp
Token t = make(TokenKind::Unknown, start);
t.erroneous = true;
auto d = diag_.error(t.span, describe_bad_char(c)).code("E0015");
if (const char* fix = suggest_for(c)) d.help(t.span, fix, "did you mean this?");
return t;
```

A character we cannot lex still produces a token, so `pos_` always advances — a lexer that returns
"nothing" on bad input, without advancing, hangs. (Write that bug once and you will remember it. The
guard is: *every path through `next()` either advances `pos_` or returns `Eof`.*)

And `suggest_for` guesses at the three or four characters people genuinely type by accident:

```cpp
case '#': return "//";     // a Python or shell habit
case '`': return "\"";     // a markdown habit
```

Two lines of table that turn `unexpected character '#'` into `did you mean //?`. Guessing well here is
cheap and disproportionately appreciated.

---

## Run it

```bat
run ch07_lexer
run ch07_lexer examples\tour.peb
run ch07_lexer examples\tour.peb --stats
run ch07_lexer examples\tour.peb --comments
run ch07_lexer examples\lex_errors.peb
```

---

## What you should see

With no arguments, the built-in snippet:

```
=== SOURCE (<builtin>, 226 bytes, 10 lines) ===
 1 | fn main() -> int {
 2 |     let price = 19;          // an integer
 3 |     let rate  = 0.085;       // a float
 4 |     var total = price * 2;
 5 |     if total >= 30 && price != 0 {
 6 |         print_str("over budget\n");
 7 |     }
 8 |     return total;
 9 | }
10 |

=== TOKENS (44) ===
line:col   kind                 text
---------------------------------------------------------------
1:1        'fn'                 fn
1:4        identifier           main
1:8        '('                  (
1:9        ')'                  )
1:11       '->'                 ->
1:14       identifier           int
1:18       '{'                  {
2:5        'let'                let
2:9        identifier           price
2:15       '='                  =
2:17       integer literal      19   = 19
2:19       ';'                  ;
3:5        'let'                let
3:9        identifier           rate
3:15       '='                  =
3:17       float literal        0.085   = 0.085
3:22       ';'                  ;
4:5        'var'                var
4:9        identifier           total
4:15       '='                  =
4:17       identifier           price
4:23       '*'                  *
4:25       integer literal      2   = 2
4:26       ';'                  ;
5:5        'if'                 if
5:8        identifier           total
5:14       '>='                 >=
5:17       integer literal      30   = 30
5:20       '&&'                 &&
5:23       identifier           price
5:29       '!='                 !=
5:32       integer literal      0   = 0
5:34       '{'                  {
6:9        identifier           print_str
6:18       '('                  (
6:19       string literal       "over budget\n"   = "over budget
" (12 bytes)
6:34       ')'                  )
6:35       ';'                  ;
7:5        '}'                  }
8:5        'return'             return
8:12       identifier           total
8:17       ';'                  ;
9:1        '}'                  }
10:1       end of file

no diagnostics
```

Five things to notice, because each one is a decision from this chapter made visible:

**The comments are gone.** Lines 2 and 3 had `// an integer` and `// a float`; no token for either. Run
with `--comments` and they appear.

**44 tokens for 9 lines.** About 5 bytes of source per token — which is why `tokenize()` does
`out.reserve(src_.size() / 4 + 8)`: one allocation instead of eleven reallocations.

**The string literal prints twice**: once as it appears in the source (`"over budget\n"`, 14 characters
between the quotes) and once decoded (12 bytes, with a real newline — which is why the output wraps). The
lexer *recognised* it as one token and *decoded* it into a side table. Chapter 8 is about that decoding.

**Keywords print as `'fn'`, `'let'`, `'if'`** with quotes, while identifiers print as `identifier`. That
is `token_name`, and it is written for its other job: appearing inside messages like
`expected ';', found 'return'`. Designing that string once, for both uses, keeps every error message in
the compiler consistent.

**There is a line 10 and it is empty.** The file ends with a newline, so a tenth line "starts" at the
last byte. `Eof` sits at 10:1. Chapter 5's line map did that, and it is correct — but it is also why
"the file has N lines" is a surprisingly ambiguous statement.

And on the error file, a taste of Chapter 10:

```
> run ch07_lexer examples\lex_errors.peb
...
=== DIAGNOSTICS ===
error[E0003]: identifier immediately after a number
  --> examples/lex_errors.peb:4:16
   |
 4 |     let a = 123abc;                 // identifier straight after a number
   |                ^^^
help: separate them with a space
   |
 4 |     let a = 123 abc;                // identifier straight after a number
   |                ~
...
13 errors generated
```

---

## Try it yourself

1. **Two minutes.** `run ch07_lexer examples\tour.peb --stats`. Which token kind is most common? (In most
   real code it is `identifier`, followed by punctuation. That fact is why identifier lexing and keyword
   lookup are the only two places in the lexer worth optimising.)
2. **Two minutes.** Add `while` to the built-in snippet and re-run. Then look for where you had to change
   the lexer. You did not. That is §5's point.
3. **Five minutes.** Add a keyword `unless` to Pebble: one enumerator (inside the keyword block!), one
   line in `token_name`, one row in `keyword_table`. Test that `unless` lexes as a keyword and `unlesss`
   as an identifier.
4. **Five minutes.** Now add the enumerator *after* `KwNull` — outside the range — and check `is_keyword`
   on it. It returns false, silently. Fix it, and then decide whether the ranges trick was worth it. (It
   is, but you should feel the cost.)
5. **Ten minutes.** Break the sentinel: change `SourceFile::at` to assert instead of returning `'\0'`,
   and lex a file whose last character is an identifier character. Watch it fire. Then put it back and
   appreciate the invariant.
6. **Ten minutes.** Delete the non-ASCII check in `lex_ident` and lex a file containing `let café = 1;`.
   Count the diagnostics. Then restore it and count again. The difference is the whole value of a
   targeted error message.
7. **Fifteen minutes.** Implement `Lexer::peek_token()` and `Lexer::next_token()` as a *streaming*
   interface (lex on demand, keep one token of lookahead) instead of `tokenize()` returning a vector.
   Then say which one the parser in Chapter 16 would rather have, and why (hint: how much lookahead does
   error recovery need?).
8. **Half an hour.** Write `bool is_ident_start_utf8(u32 codepoint)` for the *XID_Start* property of a
   handful of scripts, and allow Unicode identifiers behind a flag. Notice how quickly you need a table,
   and then read Unicode Annex #31 to see how big the real table is. This is the exercise that makes
   Chapter 5's ASCII decision feel like a gift.

---

## Common problems

**The lexer loops forever.**
Some path through `next()` neither advanced `pos_` nor returned. Put
`assert(pos_ > start || t.kind == TokenKind::Eof)` at the end of `next()` and it will tell you which.

**Every identifier is one character long.**
`is_ident_cont` is being used where `is_ident_start` was meant, or the `while` loop body forgot `pos_++`.

**A warning: `comparison is always true due to limited range of data type`.**
You wrote `c >= 0x80` with a signed `char`. Cast to `u8` first, as the non-ASCII check does. This warning
is your friend; it marks exactly the places where signed `char` is about to bite.

**MSVC: assertion failure in `isdigit`.**
You used `<cctype>` somewhere with a raw `char`. §3. Use ours.

**Tokens have the right kinds but the wrong text.**
`make()` was called before `pos_` had finished advancing, so the span is short. The rule is: scan first,
`make` last.

**`string_view` contents are garbage by the time the parser runs.**
The `SourceFile` was moved or destroyed after lexing. In `ch07_lexer.cpp` the `std::move` happens
*before* the `Lexer` is constructed, deliberately.

---

## Check yourself

1. Why does `SourceFile::at()` return `'\0'` past the end rather than asserting, and what breaks if a
   language allows NUL bytes inside identifiers?
2. `TokenKind::Comment` exists, but the compiler never sees a `Comment` token. Why have it?
3. The lexer scans an identifier and *then* checks a keyword table. What would go wrong if it instead
   tried to match each keyword first?
4. Why is `erroneous` on the token, rather than just leaving the diagnostic in the `Diagnostics` list?
5. `next()` dispatches on one character. Which single Pebble design decision makes that possible, and
   what does C do differently?

<details>
<summary>Answers</summary>

1. It removes a bounds check from every scanning loop, which is most of the lexer's inner loops. If NUL
   were a valid identifier character, `while (is_ident_cont(cur()))` would not terminate at end of file,
   so the lexer would need a genuine `at_end()` test in every loop — or the buffer would need an
   explicit, known-invalid terminator, which is the other standard approach.
2. Because the *lexer* is shared with tools that do want comments: the formatter (Chapter 23), the
   highlighter and the language server (Chapter 63). One flag and one token kind are much cheaper than a
   second lexer, and a second lexer that disagrees with the first is a category of bug that keeps
   appearing forever.
3. Maximal munch would break. Matching `if` against the input `iffy` succeeds at two characters, so a
   keyword-first lexer would produce `if` followed by the identifier `fy`. You would then need the extra
   rule "a keyword match only counts if the next character is not an identifier character" — which is
   exactly the identifier scan, done twice. Scanning the identifier first makes the problem disappear.
4. So that later stages can be quiet about it. The parser, seeing `erroneous`, can skip the token without
   adding "expected expression, found unknown character" on top of the lexer's clearer message. Searching
   the diagnostics list for "is there an error covering this span?" would work, but it is O(n) and it
   couples the parser to the diagnostics engine.
5. That a number must start with a digit — `0.5`, never `.5`. So `.` unambiguously starts `.` or `..`, and
   the dispatch needs no lookahead. C allows `.5`, so a C lexer seeing `.` must look at the next character
   to decide between a float, `...`, `.` and (in C++) `.*`. One character of lookahead is not expensive;
   the point is that every such case is a place where the *grammar* pushed work into the lexer, and those
   accumulate.
</details>

---

## Where we are

Representation 2 exists, for the easy tokens. Chapter 8 does the hard ones: numbers that can overflow,
strings that need decoding, comments that nest — the three places where a lexer stops being a
two-comparison loop and starts having opinions.

[Next: Chapter 8 — Numbers, strings, comments →](08-lexer-part2.md)
