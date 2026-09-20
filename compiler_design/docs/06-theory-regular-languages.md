# Chapter 6 — Theory: alphabets, languages, regular expressions

[← Source text](05-source-text.md) · [Contents](README.md) · [Next: The lexer core →](07-lexer-part1.md)

> 📖 **Line by line:** [ch06_regex explained line by line](line-by-line/ch06_regex.md)

---

## Goal

Understand, precisely, what a **token** is — and therefore what a lexer can and cannot do. By the end of
this chapter you will be able to:

* write any token of any programming language as a regular expression,
* state why *"longest match wins, then first rule wins"* is the correct and sufficient rule for a lexer,
* prove that nested parentheses are **not** describable by a regular expression, and explain why that
  single fact is the reason a compiler has a separate parser,
* and see for yourself why a backtracking regex engine is exponential, which motivates the DFA in
  Chapter 11.

This is a theory chapter, so it comes with a program that makes every claim runnable: a complete regex
engine and a lexer built out of it, in 380 lines.

---

## 1. Three definitions, and then we can be precise

**An alphabet**, written Σ, is a finite set of symbols. For us Σ is the 256 byte values. (Not "the
characters", not "Unicode" — *bytes*. That decision was made in Chapter 5 and it is what our code
actually inspects.)

**A string** over Σ is a finite sequence of symbols from Σ. The empty string is written ε and has
length 0. It is a perfectly good string, and forgetting that it exists is the cause of about a third of
all lexer bugs.

**A language** over Σ is a *set of strings*. That is the whole definition — no grammar, no meaning, just
a set. Some examples:

| Language | Contents | Size |
|----------|----------|------|
| `{ }` | nothing at all | 0 |
| `{ ε }` | just the empty string | 1 |
| `{ "if", "else", "while" }` | three keywords | 3 |
| all decimal integers | `0`, `1`, ..., `42`, ... | infinite |
| all valid Pebble programs | ... | infinite |
| all C programs that halt | ... | infinite, and *undecidable* |

So "the identifiers" is a language. "The integer literals" is a language. And the token kinds of a
programming language are just a handful of languages we need to be able to recognise.

The question this chapter answers is: **which languages can we describe finitely, and recognise
cheaply?**

---

## 2. Three operations build everything

Given languages *A* and *B*:

**Concatenation** `AB` = every string made by taking one string from *A* and following it with one from
*B*.
`{"ab", "c"} · {"d", "ef"}` = `{"abd", "abef", "cd", "cef"}`.

**Union** `A | B` = every string in either.
`{"a"} | {"b"}` = `{"a", "b"}`.

**Kleene star** `A*` = every string made by concatenating **zero or more** strings from *A*.
`{"ab"}*` = `{ ε, "ab", "abab", "ababab", ... }` — infinite, and note ε is in there.

The star is the interesting one, because it is where infinity comes from. Everything else is finite
bookkeeping; `*` is what lets a finite pattern describe an unbounded set. (It is named after Stephen
Kleene, who in 1951 was describing neural nets, not compilers.)

### Regular expressions: a finite notation for these

A **regular expression** over Σ is defined *inductively* — which is to say, as a little recursive data
type, which is exactly how we will implement it:

| Regex | Denotes the language |
|-------|---------------------|
| `a` (a symbol of Σ) | `{ "a" }` |
| `ε` | `{ ε }` |
| `∅` | `{ }` |
| `RS` | concatenation of *R*'s language and *S*'s |
| `R|S` | union |
| `R*` | Kleene star of *R*'s language |

And that is all. Everything else is sugar:

| Sugar | Means | Why we want it |
|-------|-------|----------------|
| `R+` | `RR*` | "one or more" — digits of a number |
| `R?` | `R|ε` | "optional" — a sign, an `else` |
| `.` | `(a|b|c|…)` over all of Σ | "any byte" — inside a comment |
| `[a-z]` | `(a|b|…|z)` | character classes, the workhorse |
| `[^0-9]` | every symbol *not* listed | "anything but a quote" |
| `R{2,4}` | `RR(R(R)?)?` | bounded repetition — rare in lexers |

A language is called **regular** if some regular expression denotes it. That word "regular" will keep
its meaning for the whole book: regular = describable this way = recognisable by a finite automaton
(Chapter 11) = lexable.

### Precedence, since it is a language with a grammar of its own

Postfix repeaters bind tightest, then concatenation, then alternation:

```
   ab|cd     means   (ab) | (cd)       not   a(b|c)d
   ab*       means   a(b*)             not   (ab)*
   a|b*      means   a | (b*)
```

Our regex parser (`ch06_regex.cpp`, §2) is a recursive-descent parser with one function per precedence
level — `parse_alt` → `parse_concat` → `parse_repeat` → `parse_atom`. Look at it now, because it is a
complete, small, working example of the technique that Chapter 16 uses on the real language:

```
alt     := concat ('|' concat)*
concat  := repeat*
repeat  := atom ('*' | '+' | '?')*
atom    := CHAR | '.' | '[' class ']' | '(' alt ')'
```

That is four functions, one per line of grammar. One of the pleasures of compiler construction is how
often the grammar *is* the code.

---

## 3. Every token you will ever need

Here is the whole lexical structure of Pebble, as regular expressions. This table is the specification
the next three chapters implement:

| Token | Regular expression | Notes |
|-------|-------------------|-------|
| identifier | `[A-Za-z_][A-Za-z_0-9]*` | must not *start* with a digit |
| integer | `[0-9][0-9_]*` | `1_000_000` allowed; Chapter 8 |
| hex integer | `0[xX][0-9A-Fa-f][0-9A-Fa-f_]*` | |
| binary integer | `0[bB][01][01_]*` | |
| float | `[0-9][0-9_]*\.[0-9][0-9_]*([eE][+-]?[0-9]+)?` | note: a digit is required on *both* sides of the dot |
| char literal | `'([^'\\\n]|\\.)'` | |
| string literal | `"([^"\\\n]|\\.)*"` | the classic; see below |
| line comment | `//[^\n]*` | stops before the newline, not after |
| block comment | **not regular if nested** | see §6 |
| whitespace | `[ \t\r\n]+` | discarded |
| operators | `->`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `<<`, `>>`, `+`, `-`, … | order matters; Chapter 9 |

**The string literal is worth reading character by character**, because it is the pattern people get
wrong:

```
"([^"\\\n]|\\.)*"
│ └──┬──┘ └┬─┘ │└─ the closing quote
│    │     │   └─── zero or more of the above
│    │     └─────── OR a backslash followed by any single character
│    └───────────── any character that is NOT a quote, a backslash or a newline
└────────────────── the opening quote
```

Each exclusion has a reason:

* not a quote — because a bare quote ends the string;
* not a backslash — because a backslash starts an escape, handled by the other alternative;
* not a newline — because a string that runs to the end of the line is almost always a missing quote,
  and reporting `unterminated string on line 7` beats swallowing the next 200 lines and reporting a
  bizarre error on line 208.

That last one is a **language design decision expressed in the lexer**. C makes the same one; Python
makes it too, and then adds `"""` for the times you meant it.

Note what the regex does *not* do: it accepts `"\q"`, because `\\.` allows any character after the
backslash. Whether `\q` is a valid escape is a *semantic* question, checked when we decode the literal's
value (Chapter 8). Splitting "what is the token" from "is the token meaningful" this way keeps the
pattern simple and the error messages good.

---

## 4. The lexer's rule, stated properly

A lexer is given an ordered list of (token kind, regular expression) rules and an input. At each
position it must decide which rule applies and how many characters to take. Two rules settle everything:

> **1. Longest match wins** (also called *maximal munch*).
> **2. On a tie, the rule listed first wins.**

Rule 1 is why `1234abc` starts with the integer `1234` and not `1`. It is why `>=` is one token and not
two. It is why `ifx` is the identifier `ifx` and not `if` followed by `x`.

Rule 2 is why `if` is the keyword `if` and not the identifier `if`: both patterns match exactly two
characters, so the tie is broken by listing keywords before identifiers.

These two rules together are called the **PLR** (priority-longest-rule) discipline, and `lex`, `flex`,
and every hand-written lexer in the world implement them. In our program:

```cpp
for (const Rule& r : rules) {
    long n = longest_prefix(r.re.get(), rest);
    if (n > best_len) { best_len = n; best_rule = &r; }   // strictly >, so ties keep the earlier rule
}
```

The `>` rather than `>=` is rule 2, in one character.

### Where maximal munch surprises people

**`a---b`** in C lexes as `a`, `--`, `-`, `b` — because at the first `-` the longest match is `--`. That
is then a syntax error (`a-- - b` would need `a` to be assignable). C compilers have said
"expression is not assignable" here since 1978, and the answer is always "put a space in".

**`x=-1`** in very old C was `x =- 1`, i.e. `x -= 1`, because `=-` was once an operator. Real programs
broke when it was removed. Lexical decisions are permanent decisions.

**`1..10`** — if you have a range operator `..` *and* floats ending in a dot, `1..10` is ambiguous:
maximal munch takes `1.` as a float, then `.10` as another, and your range operator never fires. Rust
solves it by requiring a digit after the dot in floats (so `1.` is not a float). *Our float pattern
requires digits on both sides for exactly this reason* — look at the table again.

**`Vec<Vec<int>>`** — maximal munch makes `>>` a shift operator, so the type does not parse. C++ lived
with "put a space between the angle brackets" until C++11, when the *parser* was given permission to
split a `>>` token. Java and C# do the same hack. Rust and Go avoid it by not having `<...>` around
types in expression position, or by lexing `>` separately and letting the parser join them. This is the
canonical case of a lexer/parser interaction, and Chapter 9 implements the fix.

**Keywords are not special.** They are identifiers that happen to be in a table. Our lexer lists them
first; a faster lexer scans an identifier and then does a hash lookup — which is Chapter 9's approach,
and it is why "adding a keyword" costs nothing at lexing time.

---

## 5. What regular expressions cannot do, and why it matters

Consider the language

> **L = { `(`ⁿ `)`ⁿ : n ≥ 0 }** = { ε, `()`, `(())`, `((()))`, … }

*n* opening parentheses followed by exactly *n* closing ones. No regular expression denotes this
language. Here is the argument, which is the **pumping lemma** in its concrete form:

Suppose some finite automaton with *k* states accepts L. Feed it `(`ᵏ⁺¹ — more opening parens than it
has states. It must therefore visit some state twice: after *i* parens and again after *j* parens, with
*i* < *j*. But then the machine cannot tell those two situations apart — it is in the same state, and a
state is all the memory it has. So if it accepts `(`ⁱ `)`ⁱ it must also accept `(`ʲ `)`ⁱ, which is not
in L. Contradiction.

The intuition, worth keeping: **a finite automaton cannot count without bound.** It can count to 3, or
to 10, or to any fixed number, by having a state for each — but not to *n* for arbitrary *n*.

### The consequences are the architecture of the book

| Question | Regular? | Which stage handles it |
|----------|----------|------------------------|
| Is this an identifier? | yes | lexer |
| Is this a number? | yes | lexer |
| Are the parentheses balanced? | **no** | parser |
| Is `{` matched by `}`? | **no** | parser |
| Is this expression well-formed? | **no** | parser |
| Is `x` declared before use? | no, and not even context-free | semantic analysis |
| Do the types match? | no | semantic analysis |
| Does this loop terminate? | undecidable | nobody |

That table is the answer to "why does a compiler have so many stages?" Each stage is the *weakest, and
therefore fastest and simplest, machine that can answer its question*. Lexing is regular, so it is a
loop with a switch: O(n), no stack, no backtracking. Parsing needs a stack — hence recursive descent, or
an explicit stack in an LR parser (Chapter 21). Name resolution needs a symbol table. Type checking needs
a recursive walk with an environment.

Using a stronger machine than necessary is not just wasteful, it hides errors: if your lexer can count
parens, then "unbalanced parens" becomes a lexical error with a useless message instead of a syntactic
one with a good message.

---

## 6. Nested block comments: a tiny lesson in the whole theory

```
/* outer /* inner */ still in the comment? */
```

If block comments nest (as in Rust, Swift, D and Pascal), then matching them requires counting how deep
you are — so **nested comments are not a regular language**, and no regular expression can describe them.

Three ways out, all used in real languages:

1. **Do not nest.** C, C++, Java, Go: `/*` runs to the *first* `*/`. This *is* regular:
   `/\*([^*]|\*+[^*/])*\*+/` — ugly but finite. And it means that commenting out a block of code that
   already contains a comment silently breaks.
2. **Nest, and handle it with a counter in hand-written code.** Rust, Swift, and us (Chapter 8). A
   counter is not a finite automaton, so strictly speaking the lexer is no longer "regular" — it is a
   deliberate, documented, five-line exception. That is a perfectly respectable engineering choice, and
   it is worth noticing that the *theory is what tells you it is an exception*.
3. **Nest, with a depth limit.** If comments may nest at most 255 deep, the language *is* regular again
   (255 states), and you can generate the automaton. This is the "count to a fixed number" escape hatch.
   Nobody does it, but it is the clearest illustration of where the boundary is.

Pebble takes option 2, and Chapter 8 writes the five lines. Chapter 67 asks you to decide for your own
language — and now you know what the decision costs.

---

## 7. Why backtracking is not good enough

Our matcher, like Perl's, Python's `re`, Java's, JavaScript's and PHP's, works by **backtracking**: try
an alternative, and if what follows fails, come back and try the next one. The continuation-passing
formulation makes this beautifully short:

```cpp
static bool match(const RNode* n, std::string_view s, std::size_t pos, const Cont& k) {
    switch (n->tag) {
        case RNode::Tag::Char:
            return pos < s.size() && s[pos] == n->ch && k(pos + 1);
        case RNode::Tag::Alt:
            for (const RPtr& kid : n->kids)
                if (match(kid.get(), s, pos, k)) return true;
            return false;
        /* ... */
    }
}
```

`k` is the *continuation*: "what still has to match after this". `(a|ab)c` against `"abc"` tries `a`,
calls `k`, which needs `c` but sees `b`, fails; returns to the `Alt`, tries `ab`, calls `k`, which sees
`c`. Success. The continuation is what makes the "come back and try again" correct, and it is why the
matcher is thirty lines rather than three hundred.

It is also why it is **exponential**. Consider `(a|aa)*b` against a string of *n* `a`s and no `b`. Every
way of splitting the `a`s into groups of one and two is a distinct path the matcher must explore, and the
number of such splittings is the *n*-th Fibonacci number. Run section 6 of the program and watch the step
count roughly double per character.

This is not academic:

* The 2019 **Cloudflare outage** that took down a large part of the web for 27 minutes was one regular
  expression with this shape, in a WAF rule.
* The 2016 **Stack Overflow outage** was a regex matching trailing whitespace.
* "ReDoS" (regular-expression denial of service) is a whole CVE category.

And the fix has been known since 1968 (Ken Thompson): compile the regex to a **finite automaton** and
run it in **O(n)** — one step per input character, no backtracking, no matter what the pattern looks
like. That is Chapters 11 and 12, and it is why they exist. It is also why hand-written lexers (which
are, in effect, hand-compiled DFAs) are what production compilers use.

Two footnotes so the picture is honest:

* Backtracking buys you features that finite automata cannot express — backreferences (`(a+)\1`) and
  lookahead — and those are genuinely not regular. That is the trade Perl made.
* RE2 (Google), Rust's `regex`, and Go's `regexp` are automaton-based and guarantee linear time,
  precisely by refusing backreferences.

---

## Run it

```bat
run ch06_regex
```

Or match a single pattern against a single string:

```bat
run ch06_regex "[a-z][a-z0-9]*" hello42
run ch06_regex "(a|aa)*b" aaaaaaaaaaaaaaa
```

---

## What you should see

Sections 1 to 5, abridged (every line should say `yes`; a `NO <-- expected …` means a bug):

```
=== 1. TOKEN PATTERNS ===
  /[0-9]+/
      matches  '0' : yes
      matches  '42' : yes
      rejects  '' : yes
      rejects  '4a' : yes
      rejects  '1.5' : yes
  /[A-Za-z_][A-Za-z_0-9]*/
      matches  '_tmp' : yes
      rejects  '2fast' : yes
  /[0-9]+\.[0-9]+/
      matches  '314.159' : yes
      rejects  '1.' : yes
      rejects  '.5' : yes
      rejects  '1.2.3' : yes
  /"([^"\\
]|\\.)*"/
      matches  '"a\"b"' : yes
      rejects  '"unterminated' : yes

=== 3. PRECEDENCE ===
  /ab|cd/
      matches  'ab' : yes
      matches  'cd' : yes
      rejects  'abcd' : yes
  /ab*/
      matches  'abbb' : yes
      rejects  'abab' : yes

=== 4. LONGEST MATCH (maximal munch) ===
  ident in 'price = 19' -> longest prefix = 5 chars
  ident in 'x+1' -> longest prefix = 1 chars
  ident in '_9ab cd' -> longest prefix = 4 chars
  int in '1234abc' -> 4 chars (NOT 1, and that is the whole rule)

=== 5. A LEXER MADE OF REGEXES ===
source:
let price = 19;  // a comment
fn add(a: int) -> int { return a + 1.5; }
let s = "he said \"hi\"";
let iffy = if1;
tokens:
  kw_let   'let'  @0
  ident    'price'  @4
  eq       '='  @10
  int      '19'  @12
  op       ';'  @14
  comment  '// a comment'  @17
  kw_fn    'fn'  @30
  ident    'add'  @33
  op       '('  @36
  ident    'a'  @37
  op       ':'  @38
  ident    'int'  @40
  op       ')'  @43
  arrow    '->'  @45
  ident    'int'  @48
  op       '{'  @52
  ident    'return'  @54
  ident    'a'  @61
  op       '+'  @63
  float    '1.5'  @65
  op       ';'  @68
  op       '}'  @70
  kw_let   'let'  @72
  ident    's'  @76
  eq       '='  @78
  string   '"he said \"hi\""'  @80
  op       ';'  @96
  kw_let   'let'  @98
  ident    'iffy'  @102
  eq       '='  @107
  ident    'if1'  @109
  op       ';'  @112
```

Three things in that token dump are worth stopping on.

**`iffy` is one `ident`, and so is `if1`.** Longest match beat rule order: `kw_if` matches 2 characters,
`ident` matches 4 and 3. Rule 2 only applies to *ties*.

**`return` came out as `ident`.** We did not list it as a keyword in this toy rule set — which shows
exactly how a lexer treats keywords: they are identifiers plus a table. Add `{"kw_return", "return"}`
before the `ident` rule and it changes kind, with no other code touched.

**The string token is 16 characters long**, including both escaped quotes. The lexer recognised it as
*one* token without understanding what `\"` means; decoding is a later job.

Section 6's numbers will differ slightly between compilers (the matcher's step counter is a crude
proxy), but the shape is the point:

```
=== 6. WHY BACKTRACKING IS NOT ENOUGH ===
  pattern /(a|aa)*b/ against 'aaa...' with no 'b':
     1 a's -> no match, 11 steps
     2 a's -> no match, 24 steps
     3 a's -> no match, 45 steps
     4 a's -> no match, 78 steps
     8 a's -> no match, 800 steps
    12 a's -> no match, 5000+ steps
    16 a's -> no match, 30000+ steps
    20 a's -> no match, 200000+ steps
```

Twenty characters, hundreds of thousands of steps, and each additional character multiplies it. Now
imagine that pattern in a web server, fed a 200-character string by a stranger.

---

## Try it yourself

1. **Two minutes.** `run ch06_regex "(a|aa)*b" aaaaaaaaaaaaaaaaaaaaaaaaa` (25 a's). Time it. Add five
   more a's. Time it again.
2. **Five minutes.** Add a `kw_return` rule to `make_rules`, *after* `ident`. Re-run and watch `return`
   stay an identifier — because on a tie the *earlier* rule wins. Move it before `ident` and it becomes a
   keyword. You have just discovered why every generated lexer's documentation says "order your rules".
3. **Five minutes.** Write a pattern for a Pebble identifier that also allows a trailing `?` or `!`
   (as Ruby does: `empty?`, `save!`). Then think about what that does to `x!=y`, and decide whether you
   still want the feature. This is language design in three minutes.
4. **Ten minutes.** Write the non-nesting block comment pattern, `/\*([^*]|\*+[^*/])*\*+/`, add it as a
   rule, and test it on `/* a */`, `/**/`, `/* * */`, `/* ** */` and `/* /* */`. Then explain in one
   sentence why the last one ends where it does.
5. **Fifteen minutes.** Add `{m,n}` bounded repetition to the regex parser. Then express "a hex escape
   of exactly two digits" (`\\x[0-9A-Fa-f]{2}`) and add it to the string rule.
6. **Twenty minutes.** Make `longest_prefix` stop exploring once it has found a match that reaches the
   end of the input, and measure the speed-up on section 6. Then convince yourself that this optimisation
   does not help at all in the worst case, and say why. (Hint: what is the worst case *for*?)
7. **Half an hour, and the best exercise in this chapter.** Add a `lazy` quantifier `*?` (match as few as
   possible) by trying `k(p)` *before* the recursive step in the `Star` case. Then write a pattern for a
   non-nesting block comment using it — `/\*.*?\*/` — and notice how much clearer it is than exercise 4.
   Then find out why no lexer generator supports it (hint: what does "as few as possible" mean to a
   machine with no memory of alternatives it has not tried?).

---

## Common problems

**The program hangs on some pattern.**
You have hit a genuine exponential case. That is section 6's lesson, not a bug. `Ctrl+C`, and make the
string shorter.

**`a*` against `"b"` returns true.**
Correct. `a*` matches ε, and ε is a prefix of every string. `full_match` additionally requires reaching
the end, which is why the test says it rejects `"b"`. If you are writing your own tests, be explicit
about whether you want "matches a prefix" or "matches the whole thing" — confusing the two is the most
common regex mistake in the world, and the reason `grep` and a lexer behave so differently.

**A pattern with `[` in it misbehaves.**
Inside a character class, almost nothing is special: `[.*+]` matches a literal dot, star or plus. But `^`
is special *first*, `]` must be escaped or listed first, and `-` is a range unless it is first or last.
Our `parse_class` handles all three; read it, because every regex implementation has these same four
special cases and they are never in the documentation.

**An infinite loop in a `Star` whose body can match ε.**
`(a*)*` — the inner star matches nothing, forever. Our matcher has `if (q == p) return false;` for
exactly this. Every backtracking engine needs that guard, and the ones that forgot it have CVEs.

---

## Check yourself

1. Is `{ ε }` the same language as `{ }`? What is the practical consequence of confusing them in a
   lexer?
2. Write a regular expression for "a C identifier that is not a keyword". (Careful.)
3. Maximal munch takes `1234` from `1234abc`. What token comes next, and what error, if any, results?
4. `/* /* */` — where does this comment end under each of the three block-comment designs in §6?
5. Why can a lexer not check that parentheses are balanced? Give the one-sentence version of the proof.
6. Our lexer tries every rule at every position — 13 regexes per character. A real lexer is one pass
   with a switch. Are they doing the same thing?

<details>
<summary>Answers</summary>

1. No. `{ }` accepts nothing at all; `{ ε }` accepts exactly the empty string. In a lexer the difference
   is fatal: a rule whose pattern can match ε will match *zero characters* at every position, the
   position never advances, and the lexer loops forever producing empty tokens. That is why a lexer must
   reject rules matching ε, and why our `lex` treats `best_len <= 0` as "no rule matched".
2. You cannot, with the operations in this chapter — not conveniently. Regular languages *are* closed
   under complement and intersection, so the language exists (identifiers ∩ ¬keywords is regular), but
   regular *expressions* have no complement operator, so writing it out means enumerating the
   complement of a 40-word set by hand. This is precisely why lexers do not try: they match the
   identifier and then look the spelling up in a table. Theory says it is possible; engineering says do
   not.
3. `abc`, as an identifier. No lexical error at all — `1234abc` is two perfectly good tokens. The error
   comes from the *parser*, which sees `int ident` with nothing between them. Some languages (and our
   Chapter 8) add a special check for "a number immediately followed by an identifier character" because
   the parser's message is unhelpful and the user's intent is obvious.
4. Non-nesting (C): at the *first* `*/`, so the comment is `/* /* */` and everything after it is code.
   Nesting (Rust, ours): the depth goes to 2 at the second `/*` and down to 1 at the `*/`, so the comment
   is **unterminated** and the lexer reports an error at end of file. Depth-limited: same as nesting,
   until the limit. Note that the two real designs disagree about whether this text is a complete
   comment — which is why you cannot mechanically translate comments between the two languages.
5. A finite automaton has finitely many states and therefore cannot count without bound; feed it more
   parens than it has states and it must repeat a state, after which it can no longer tell those two
   depths apart.
6. Yes, in result; no, in cost. Ours is O(rules × pattern size) per character with exponential worst
   cases; a hand-written lexer is O(1) per character. They compute the same function — the PLR
   discipline — which is precisely what Chapters 11 and 12 prove by *generating* the fast one from the
   slow one's rules.
</details>

---

## Where we are

We now know exactly what a lexer must do, and exactly what it must not try to do. Chapter 7 writes it —
by hand, as a switch over the first character, because that is what production compilers do and it is
both the fastest and the easiest to give good error messages from.

[Next: Chapter 7 — A hand-written lexer, part 1 →](07-lexer-part1.md)
