# Chapter 8 — A hand-written lexer, part 2: numbers, strings, comments

[← The lexer core](07-lexer-part1.md) · [Contents](README.md) · [Next: Keywords and operators →](09-keywords-and-operators.md)

> 📖 **Line by line:** [lexer.h](line-by-line/lexer.md)

---

## Goal

Finish the lexer's hard half. These three token kinds are where a scanner stops being a two-comparison
loop and starts making decisions with consequences:

* **numbers** — which can overflow, can be written in four bases, and must be converted to a value
  *exactly*;
* **strings and characters** — which must be *decoded*, not just recognised, and which are the most common
  place for an unterminated token;
* **comments** — which, if they nest, are not a regular language at all (Chapter 6 §6).

Each section ends with the diagnostics it produces, because "what does the compiler say when this is
wrong?" is half the design.

---

## 1. Numbers: recognising

```cpp
Token lex_number() {
    u32 start = pos_;

    if (cur() == '0' && (peek() == 'x' || peek() == 'X')) return lex_radix(start, 16, "hex");
    if (cur() == '0' && (peek() == 'b' || peek() == 'B')) return lex_radix(start, 2, "binary");
    if (cur() == '0' && (peek() == 'o' || peek() == 'O')) return lex_radix(start, 8, "octal");

    while (is_digit(cur()) || cur() == '_') pos_++;

    bool is_float = false;
    if (cur() == '.' && is_digit(peek())) {          // (a)
        is_float = true;
        pos_++;
        while (is_digit(cur()) || cur() == '_') pos_++;
    }
    if (cur() == 'e' || cur() == 'E') {              // (b)
        u32 save = pos_, p = pos_ + 1;
        if (src_.at(p) == '+' || src_.at(p) == '-') p++;
        if (is_digit(src_.at(p))) {
            is_float = true;
            pos_ = p;
            while (is_digit(cur()) || cur() == '_') pos_++;
        } else {
            pos_ = save;                             // `1e` is the integer 1, then ident `e`
        }
    }
    /* ... */
}
```

### (a) The dot rule, which is a whole language design decision

`cur() == '.' && is_digit(peek())` — a dot continues the number **only if a digit follows**. Three
programs explain why:

| Source | Our tokens | If we dropped `is_digit(peek())` |
|--------|-----------|----------------------------------|
| `1.5` | `float 1.5` | `float 1.5` |
| `1..10` | `int 1`, `..`, `int 10` | `float 1.`, `float .10` — range operator dead |
| `x.field` on `1.field` | `int 1`, `.`, `ident field` | `float 1.` then a confused parser |

Maximal munch is a *rule*, not a *goal*: here we deliberately restrict what can be munched so that two
features (float literals and the `..` range operator) can coexist. Rust made the same choice. C, which has
no `..`, allows `1.` — and pays for it with the `.5` lookahead case from Chapter 7.

### (b) The exponent backtrack

`1e` is not a valid float, but it is a perfectly good `1` followed by the identifier `e`. So we *save the
position*, look ahead past an optional sign for a digit, and **restore the position if there is none**.

This is the only backtracking in our entire lexer, and it is worth noticing how little is needed: two
characters of lookahead, one saved index. A generated DFA lexer (Chapter 12) handles this without
backtracking at all, because a DFA can be in a state that means "I have seen digits and an `e`, and if the
next character is not a digit I will report the longest earlier accepting state". That is *exactly* what
"restore the position" is doing by hand. When you meet the DFA's `last_accept` variable in Chapter 12,
remember this paragraph.

### Digit separators

`while (is_digit(cur()) || cur() == '_') pos_++;` allows `1_000_000`, `0xFF_00`, `3.14159_26535`. Cheap,
and every modern language has it (Java 7, C++14, Rust, Go, C#, Python 3.6).

Note that this accepts `1_`, `1__2` and `_1` is an identifier, not a number. Rust rejects a trailing
underscore; we allow it. That is a two-line check (exercise 3), and the interesting part is *deciding*:
strictness costs a rule in the spec, laxness costs a surprise in code review. Chapter 67 calls this class
of decision "small syntax, large arguments".

---

## 2. Numbers: converting, and overflow

Recognising `9223372036854775808` is easy. Turning it into a value is where correctness lives.

```cpp
u64 parse_decimal(std::string_view text, Span span, bool& erroneous) {
    u64  value    = 0;
    bool overflow = false;
    for (char c : text) {
        if (c == '_') continue;
        u64 d = static_cast<u64>(c - '0');
        if (value > (UINT64_MAX - d) / 10) overflow = true;      // <-- the check
        value = value * 10 + d;
    }
    if (overflow || value > static_cast<u64>(INT64_MAX)) {
        diag_.error(span, "integer literal is too large for 'int'")
            .code("E0005")
            .note("'int' is a signed 64-bit integer; the maximum is 9223372036854775807");
        erroneous = true;
        return 0;
    }
    return value;
}
```

**Read the overflow check carefully, because the obvious version is wrong.**

```cpp
u64 next = value * 10 + d;
if (next < value) overflow = true;      // WRONG: wraps for some inputs and not others
```

Unsigned overflow wraps (it is defined behaviour, unlike signed overflow), but `value * 10` can wrap all
the way past `value` and land *above* it, so the comparison misses it. The correct form asks the question
*before* doing the arithmetic: "is `value` already bigger than what would fit after multiplying by 10 and
adding `d`?" — i.e. `value > (UINT64_MAX - d) / 10`. That formulation never overflows, because both
operations on the right shrink.

If you ever write an overflow check, write it in that shape: **rearrange the inequality so no overflow can
happen inside the test.**

**Why accumulate in `u64` when Pebble's `int` is signed 64-bit?** Because of exactly one literal:
`-9223372036854775808`, the most negative `int`. The lexer never sees the minus sign — that is a unary
operator, parsed later — so it must be able to hold `9223372036854775808`, which does not fit in `i64`.
Every language has this wart:

* C solves it by making `INT_MIN` a macro spelled `(-2147483647 - 1)`, because `-2147483648` is not a
  valid `int` constant expression.
* Rust makes the lexer produce a `u128` and lets the type checker apply the sign.
* We accept the literal up to `INT64_MAX` and report the one edge case as an error; exercise 5 is to fix
  it properly by moving the range check to the point where unary minus is folded (Chapter 27).

That is a genuine, documented limitation, and noticing it is the point. Overflow at the *boundary between
two stages* is where real compilers have real bugs.

### Floats: never use the locale

```cpp
double parse_float(std::string_view text) {
    std::string clean;
    for (char c : text) if (c != '_') clean.push_back(c);
    return std::strtod(clean.c_str(), nullptr);
}
```

Three notes, two of them warnings.

**`strtod` is locale-dependent.** In a German locale the decimal separator is `,`, so `strtod("3.14")`
returns `3.0`. Our compiler never calls `setlocale`, so we are in the "C" locale and safe — but that is a
*fragile* guarantee, because any library we link might call it. The robust answer in C++17 is
`std::from_chars`, which is locale-independent by specification and does correct round-tripping:

```cpp
double v;
std::from_chars(clean.data(), clean.data() + clean.size(), v);   // needs a recent libstdc++
```

`from_chars` for floating point arrived late in GCC (11) and Clang (20+ with libc++), which is why the
book uses `strtod` and mentions this. Exercise 6 switches it over with a feature test.

**Correct rounding matters.** `0.1` is not representable in binary; the nearest `double` is
`0.1000000000000000055511151231257827`. A *correct* decimal-to-double conversion returns the nearest
double, and doing that with `value * pow(10, exp)` accumulates error — so hand-rolled float parsing is
almost always subtly wrong. `strtod` and `from_chars` are correct; use them. (The algorithms are
Clinger's and, recently, Lemire's `fast_float`. If your language cares about exact literals, that is the
reading.)

**The compiler and the program must agree.** If the compiler folds `0.1 + 0.2` at compile time using the
host's `double`, and the target uses a different precision (x87 80-bit intermediates! ARM fused
multiply-add!), the constant-folded answer differs from the run-time answer. That is not hypothetical:
x87's excess precision caused years of "the optimiser changed my results" bug reports. Chapter 41 returns
to this with the rule: **fold floating point only with the exact semantics the target will use, or not at
all.**

### `123abc`

```cpp
if (is_ident_start(cur())) {
    u32 bad = pos_;
    while (is_ident_cont(cur())) pos_++;
    diag_.error(Span(bad, pos_), "identifier immediately after a number")
        .code("E0003")
        .help(Span(bad, bad), " ", "separate them with a space");
    /* ... */
}
```

Strictly, `123abc` is two valid tokens and the parser should complain. But the parser's message would be
`expected ';', found identifier` pointing at `abc`, which explains nothing. Here we know exactly what
happened, so we say it, and we offer the fix.

This is a recurring pattern worth naming: **the lexer sometimes reports an error for something that is
not a lexical error, because it has better context than the parser will.** Rust does this for `1.0.0`,
Clang for `0x1p` and for `1'000` in C. It is not architectural impurity; it is where the information is.

---

## 3. Radix prefixes

```cpp
Token lex_radix(u32 start, int base, const char* what) {
    pos_ += 2;                                  // the 0x / 0b / 0o
    u64 value = 0; bool overflow = false, any = false;
    while (true) {
        char c = cur();
        if (c == '_') { pos_++; continue; }
        int d = hex_value(c);
        if (d < 0 || d >= base) break;          // wrong base, or not a digit at all
        pos_++; any = true;
        if (value > (UINT64_MAX - (u64)d) / (u64)base) overflow = true;
        value = value * (u64)base + (u64)d;
    }
    /* ... diagnostics ... */
}
```

One function for all three bases, parameterised by `base` and by a word for the message. The nice part is
`hex_value(c)` combined with `d >= base`: for binary, `hex_value('5')` is 5, which is `>= 2`, so the loop
stops and the *next* check reports `invalid digit for a binary literal` with the caret on the `5`. That
error is much better than `expected ';'`, and it costs nothing extra.

Three diagnostics come out of this function, and each corresponds to a real mistake:

| Input | Message |
|-------|---------|
| `0x` | `expected at least one hex digit` |
| `0b1012` | `invalid digit for a binary literal` (caret on `2`) |
| `0xFFFFFFFFFFFFFFFFF` | `integer literal is too large for 'int'` |

**Why no `0777` octal?** Because C's leading-zero octal is one of the worst syntax decisions in
programming language history: `chmod(path, 0644)` is intended, `chmod(path, 644)` compiles and does
something else, and `int x = 010;` is 8. Pebble requires `0o644`, as Python 3, Rust and Go do. A leading
zero in Pebble is just a decimal zero. Chapter 67 uses this as the canonical example of "a syntax you can
only fix by breaking compatibility".

---

## 4. Strings: recognising and decoding at once

```cpp
Token lex_string() {
    u32 start = pos_;
    pos_++;                                       // the opening quote
    std::string value;
    bool bad = false;

    for (;;) {
        char c = cur();
        if (c == '"') { pos_++; break; }
        if (at_end() || c == '\n') {
            diag_.error(Span(start, pos_), "unterminated string literal")
                .code("E0007")
                .note("a string may not span a line; end it with '\"'")
                .help(Span(pos_, pos_), "\"", "add the closing quote");
            bad = true;
            break;
        }
        if (c == '\\') { read_escape(value, bad); continue; }
        value.push_back(advance());
    }

    Token t = make(TokenKind::StringLit, start);
    t.value_index = static_cast<u32>(strings_.size());
    t.erroneous = bad;
    strings_.push_back(std::move(value));
    return t;
}
```

### Decoding belongs here, and only here

`"a\tb"` is six characters in the file and three bytes in memory. Somebody must do that conversion, and
the lexer is the right somebody: it is already walking the characters, it knows where each escape starts
(so its error messages can point at the escape, not at the string), and doing it once means no later stage
ever has to think about escapes again.

The decoded bytes go into `strings_`, a `std::vector<std::string>` owned by the `Lexer`, and the token
holds an index. Why not a `std::string` in the token? Chapter 7 §1: it would double the size of every
token, for a payload that fewer than 2% of tokens have. (A `std::variant` would be worse — the variant is
as big as its largest member.) A side table with an index is the standard answer, and it has a bonus: two
identical string literals could be de-duplicated by hashing, which is exercise 8 and is what real
compilers do for the read-only data section.

### The newline rule

```cpp
if (at_end() || c == '\n') { /* unterminated */ }
```

A string may not contain a raw newline. The alternative — letting it run — turns one missing quote into a
catastrophe: the lexer swallows the rest of the file as a string, and the error appears hundreds of lines
later at the *next* quote, or at end of file. Every language that allows multi-line strings gives them a
*different* delimiter for exactly this reason (Python's `"""`, C#'s `@""`, Rust's `r#""#`, shell's
heredocs, and Pebble's answer in Chapter 67's exercises).

Note the `help` suggestion inserts a quote at the *current* position — the end of the line — because that
is where the closing quote belongs. A suggestion with an empty span is an *insertion*, which is why
`Span::start()` and zero-length spans exist.

### The escapes

```cpp
case 'n':  out.push_back('\n'); return;
case 't':  out.push_back('\t'); return;
case 'r':  out.push_back('\r'); return;
case '0':  out.push_back('\0'); return;
case '\\': out.push_back('\\'); return;
case '"':  out.push_back('"');  return;
case '\'': out.push_back('\''); return;
case 'x':  /* \xNN, exactly two hex digits */
case 'u':  /* \u{1F600}, one to six hex digits */
default:   /* unknown escape: report it, keep the character */
```

The list is short on purpose. Notice what is **not** there:

* **`\a` `\b` `\f` `\v`** — C's terminal-control escapes from 1972. Nobody has used a vertical tab
  deliberately in decades, and `\b` in a string is a bug more often than a backspace.
* **`\123` octal escapes** — because `"\1234"` is ambiguous to a human (is that three digits or four?) and
  because they encourage writing bytes where characters are meant.
* **Line continuation `\` at end of line** — because it interacts with the "no raw newline" rule and
  because trailing whitespace after the backslash silently breaks it.

Every omission is a thing you never have to explain, document or debug. A short escape table is a feature.

**The `default` case is where the design shows:**

```cpp
diag_.error(Span(esc_start, pos_), "unknown escape sequence")
    .code("E0013")
    .note("valid escapes are \\n \\t \\r \\0 \\\\ \\\" \\' \\xNN \\u{...}")
    .help(Span(esc_start, esc_start + 1), "\\\\", "write a literal backslash as '\\\\'");
out.push_back(c);      // recover: assume they meant the character itself
```

An error, with the full list of valid escapes (because the user does not know it), plus a guess at the
most likely intent (a Windows path: `"C:\temp"`), plus *recovery* — we keep the character and carry on, so
the string is still usable and the parser still sees one `StringLit`. Compare with a compiler that says
`error: invalid escape` and nothing else.

### `\u{...}` and UTF-8 encoding

```cpp
u32 cp = 0; int count = 0;
while (hex_value(cur()) >= 0 && count < 6) { cp = cp * 16 + hex_value(advance()); count++; }
if (count == 0 || !eat('}')) { /* error */ }
if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { /* error */ }
append_utf8(out, cp);
```

Two validity checks, both mandatory:

* **Above U+10FFFF** does not exist. Unicode is capped there because UTF-16 cannot address more.
* **U+D800 to U+DFFF** are *surrogates*: halves of a UTF-16 pair. They are not characters, and encoding
  one in UTF-8 produces a byte sequence (`ED A0 80`) that strict decoders must reject. Allowing them means
  your compiler can emit invalid UTF-8, which becomes someone else's security bug. This is called
  WTF-8 when done deliberately, and you do not want it.

`append_utf8` is the encoder, and it is worth reading once because the pattern is the whole of UTF-8:

| Code point | Bytes | Layout |
|-----------|-------|--------|
| U+0000–U+007F | 1 | `0xxxxxxx` |
| U+0080–U+07FF | 2 | `110xxxxx 10xxxxxx` |
| U+0800–U+FFFF | 3 | `1110xxxx 10xxxxxx 10xxxxxx` |
| U+10000–U+10FFFF | 4 | `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` |

The lead byte's high bits say how many bytes follow; every continuation byte starts `10`. That is why
Chapter 5's validator and `display_col` could both work by testing `(b & 0xC0) != 0x80`, and it is why
UTF-8 is self-synchronising: from any byte you can find the start of its character by walking back while
you see `10xxxxxx`.

### Characters

```cpp
Token lex_char();         // '' -> error; 'ab' -> error with a suggestion; '\n' -> fine
```

A character literal holds exactly one character, so three things can go wrong, and each gets its own
message: empty (`''`), unterminated (`'a` then end of line), and too long (`'too many'`). The third one
carries a suggestion:

```cpp
.help(Span(start, start + 1), "\"", "use double quotes for a string")
```

because someone writing `'too many'` came from Python or shell, where quotes are interchangeable. Guessing
the *cause* of the mistake, not just naming it, is what makes error messages feel helpful.

---

## 5. Comments, and the counter that leaves the theory behind

```cpp
Token lex_block_comment() {
    u32 start = pos_;
    pos_ += 2;
    int depth = 1;                                   // <- the whole story
    while (depth > 0) {
        if (at_end()) { /* unterminated: report, break */ }
        if (cur() == '/' && peek() == '*') { pos_ += 2; depth++; continue; }
        if (cur() == '*' && peek() == '/') { pos_ += 2; depth--; continue; }
        pos_++;
    }
    return make(TokenKind::Comment, start);
}
```

`int depth` is unbounded counting, which Chapter 6 proved a finite automaton cannot do. So our lexer is
*not* a finite automaton — deliberately, in exactly one place, for five lines. Three observations:

1. **That is fine, and it is normal.** Real lexers all have two or three such exceptions: nested comments,
   Python's `INDENT`/`DEDENT` (which needs a stack), C++'s raw string delimiters (`R"delim(...)delim"`,
   which needs to remember the delimiter), and here-documents. The theory's job is not to forbid them but
   to tell you *which* parts of your lexer are the special ones — because those are the parts a lexer
   generator cannot produce and the parts most likely to have bugs.
2. **Nesting is the right choice for a modern language.** The reason is a single workflow: commenting out
   a block of code that already contains a comment. With C's rules that silently breaks; with nesting it
   works. Rust, Swift, D, Scala and Kotlin all nest. C, C++, Java, Go and JavaScript do not (and cannot
   now, without breaking existing code).
3. **The unterminated case must name the depth.** `expected '*/' before end of file` is unhelpful when you
   have three unclosed comments; `block comments nest, and 3 are still open` tells you what to look for.

**Doc comments.** Note that `lex_line_comment` returns a `Comment` token whose text includes the `//`. A
documentation generator wants `///` and `/** */` to be *kept* and attached to the following declaration.
The hook is already there — `keep_comments` — and exercise 9 adds a `DocComment` kind. The design question
(is a doc comment trivia, or part of the AST?) is a real one: Rust makes them attributes in the AST, Java
keeps them as trivia and re-lexes in javadoc. Chapter 15 says which we do and why.

---

## Run it

```bat
run ch07_lexer examples\tour.peb
run ch07_lexer examples\lex_errors.peb
```

`tour.peb` contains one of nearly every token; `lex_errors.peb` contains one of every lexical error.

---

## What you should see

From `tour.peb`, the interesting rows of the token dump (abridged):

```
line:col   kind                 text
---------------------------------------------------------------
...
20:16      integer literal      0xFF_00   = 65280
21:16      integer literal      0b1010_0101   = 165
22:16      integer literal      0o755   = 493
...
17:15      float literal        3.14159_26535   = 3.14159
...
32:17      string literal       "pebble \u{1F4A1}"   = "pebble 💡" (11 bytes)
33:17      string literal       "a\tb\\c\"d"   = "a	b\c"d" (7 bytes)
34:17      character literal    'x'   = 120
35:17      character literal    '\n'   = 10
36:17      integer literal      9223372036854775807   = 9223372036854775807
...
```

Check these against the source and make sure you believe each one:

* `0xFF_00` is 65280 — the underscore contributed nothing, as intended.
* `0b1010_0101` is 165. `0o755` is 493, the familiar Unix permission bits.
* `3.14159_26535` prints as `3.14159` because `std::cout` shows six significant digits by default. The
  *stored* value has full precision — add `<< std::setprecision(17)` to `dump_tokens` and you will see
  `3.1415926535000001`. That is not a bug; it is `double`.
* The emoji string is 11 bytes: `pebble ` is 7, and U+1F4A1 encodes as 4. The source spelled it in 10
  characters (`\u{1F4A1}`) — proof that decoding happened.
* `"a\tb\\c\"d"` decodes to 7 bytes: `a`, TAB, `b`, `\`, `c`, `"`, `d`. The dump prints a real tab, so the
  output looks misaligned. That is the decoded value, printed raw.
* `'\n'` has value 10.

And from `lex_errors.peb`, the whole point of the chapter — twelve distinct, specific messages instead of
one confused one:

```
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

error[E0005]: integer literal is too large for 'int'
  --> examples/lex_errors.peb:5:13
   |
 5 |     let b = 99999999999999999999;   // too large for int
   |             ^^^^^^^^^^^^^^^^^^^^ 'int' is a signed 64-bit integer; the maximum is 9223372036854775807

error[E0004]: expected at least one hex digit
  --> examples/lex_errors.peb:6:13
   |
 6 |     let c = 0x;                     // no hex digits
   |             ^^

error[E0006]: invalid digit for a binary literal
  --> examples/lex_errors.peb:7:18
   |
 7 |     let d = 0b1012;                 // 2 is not a binary digit
   |                  ^

error[E0007]: unterminated string literal
  --> examples/lex_errors.peb:8:13
   |
 8 |     let e = "unterminated
   |             ^^^^^^^^^^^^^ a string may not span a line; end it with '"'
help: add the closing quote
   |
 8 |     let e = "unterminated"
   |                          ~

error[E0013]: unknown escape sequence
  --> examples/lex_errors.peb:9:25
   |
 9 |     let f = "bad escape \q";
   |                         ^^ valid escapes are \n \t \r \0 \\ \" \' \xNN \u{...}
...
error[E0012]: not a valid Unicode code point
error[E0010]: '\x' needs exactly two hex digits
error[E0008]: empty character literal
error[E0009]: character literal must contain exactly one character
error[E0015]: unexpected character '#'
error[E0002]: non-ASCII character in identifier
error[E0014]: unterminated block comment

13 errors generated
```

Thirteen errors from one run. That is the payoff of §6 of Chapter 7: errors are collected, not thrown.

---

## Try it yourself

1. **Two minutes.** Add `let x = 1e10;` and `let y = 1e;` to a file and lex it. The first is a float; the
   second is `1` followed by the identifier `e`. Find the code that made that happen (§1b).
2. **Five minutes.** Add `let z = 1..10;` and confirm three tokens. Then delete `&& is_digit(peek())` from
   the dot test, rebuild, and look at what `1..10` becomes. Restore it.
3. **Five minutes.** Reject a trailing digit separator: `1_` should be an error, `1_0` should not. Two
   lines. Then decide whether you would also reject `1__0`, and write down your reason — you are now doing
   language design.
4. **Ten minutes.** Replace the overflow check with the naive `if (next < value)` version and find an
   input where it is wrong. (Try 20 digits of 9s, then 21.) This is the most valuable ten minutes in the
   chapter.
5. **Fifteen minutes.** Make `-9223372036854775808` work. The clean fix is *not* in the lexer: let the
   lexer accept any literal up to `UINT64_MAX` without complaint, store it in `u64`, and have the type
   checker (Chapter 27) report the range error after unary minus has been folded. Write the lexer half now
   and leave a `TODO` naming the chapter.
6. **Fifteen minutes.** Switch `parse_float` to `std::from_chars` behind a
   `#if defined(__cpp_lib_to_chars)` test, falling back to `strtod`. Then test `0.1`, `1e308`, `1e309`
   (infinity) and `1e-400` (zero, with underflow).
7. **Twenty minutes.** Add raw strings: `r"C:\temp\new"` with no escape processing, and `r#"he said "hi""#`
   with a variable number of `#`s. You will need to remember the delimiter length while scanning — another
   place where the lexer stops being a finite automaton. Now you know why C++11's raw strings took so long.
8. **Twenty minutes.** De-duplicate identical string literals: keep a `std::unordered_map<std::string, u32>`
   next to `strings_` and reuse the index. Measure how many duplicates a large file has. (This is what the
   back end's read-only data section does, and it is free here.)
9. **Half an hour.** Add `DocComment` as a token kind for `///` and `/** */`, keep them even when
   `keep_comments` is false, and have `dump_tokens` show them. Then read Chapter 15 and decide where they
   should be attached in the AST.

---

## Common problems

**Floats are all slightly wrong, or `3.14` becomes `3`.**
A locale set `,` as the decimal separator, or you hand-rolled the conversion. §2.

**`"\u{1F600}"` produces four garbage characters in the terminal.**
The bytes are probably right; your console is not in UTF-8 mode. On Windows try `chcp 65001`, or check
the output by printing the byte values instead.

**An unterminated string eats the rest of the file.**
The `c == '\n'` case is missing or is after `at_end()` in a way that never fires. Ours checks both in one
condition.

**Nested comment counting goes wrong on `/*/`.**
`/*/` is an opening `/*` followed by `/`, not an opening and a closing. Our loop tests
`cur() == '/' && peek() == '*'` *first* and consumes two characters, so after `/*` the position is at `/`,
which matches neither pattern and is skipped. Trace it by hand; then trace `/**/` and `/***/`.

**`0x10` lexes as `0`, then `x10`.**
The radix test ran after the decimal digit loop consumed the `0`. Order matters: the prefix check must come
first, and it must look at `cur()` and `peek()` before anything has been consumed.

---

## Check yourself

1. Why does the lexer decode escape sequences instead of leaving the raw text for a later stage?
2. `if (value > (UINT64_MAX - d) / 10)` — why is this the right shape for an overflow check?
3. A string literal's decoded value lives in a side vector, not in the `Token`. Give two reasons.
4. Why is `\u{D800}` an error, given that it is a perfectly good 16-bit number?
5. Nested block comments make the lexer "not regular". Why is that acceptable here but not for, say,
   expression nesting?
6. `1.` is a float in C and two tokens in Pebble. What did Pebble buy with that restriction?

<details>
<summary>Answers</summary>

1. Because the lexer is already walking the characters (so it is free), it knows the exact span of each
   escape (so errors point at the escape, not the string), and doing it once means no later stage — parser,
   type checker, constant folder, code generator, debug-info writer — ever has to know that escapes exist.
   Decoding late means decoding in several places, and they will disagree.
2. Because it asks the question *before* performing the arithmetic that could overflow. Both operations on
   the right-hand side (subtract, divide) make the value smaller, so the test itself can never overflow.
   The naive `next < value` test performs the overflowing multiply first and then hopes the wrapped result
   is detectable, which it is not in general.
3. (a) Size: a `std::string` member would roughly double `Token` and make it non-trivially copyable,
   for a payload under 2% of tokens have. (b) Ownership and lifetime: the decoded bytes are new data that
   must outlive the token vector and be shareable, and a side vector gives one clear owner. A bonus third
   reason: identical literals can be de-duplicated by index.
4. Because it is not a character — it is half of a UTF-16 surrogate pair, and Unicode explicitly says it
   never appears in UTF-8. Encoding it produces a byte sequence that strict UTF-8 decoders must reject, so
   allowing it means the compiler can emit output that its own validator (Chapter 5) would reject.
5. Because it is *bounded in scope*: five lines, one counter, one documented exception, and nothing else in
   the lexer depends on it. Expression nesting is unbounded in *structure* — it needs a stack whose depth
   is the nesting depth, and the thing that consumes it (precedence, associativity, error recovery) is a
   whole subsystem. The right tool for a whole subsystem is a parser, not an exception in a lexer.
6. The `..` range operator, and `1.field` — that is, it kept the dot available for two other jobs. It also
   removed one lookahead case from the dispatch (`.` can never start a number). The cost is that you must
   write `1.0`, which is arguably clearer anyway.
</details>

---

## Where we are

The lexer handles every token shape in the language. Chapter 9 finishes it: how keywords are recognised
efficiently, why multi-character operators are written as nested `eat()` calls, and the two places where a
real language forces the lexer and the parser to talk to each other.

[Next: Chapter 9 — Keywords, identifiers, operators, maximal munch →](09-keywords-and-operators.md)
