# Chapter 5 — Source text: files, encodings, spans, line maps

[← A tiny compiler](04-tiny-compiler.md) · [Contents](README.md) · [Next: Regular languages →](06-theory-regular-languages.md)

> 📖 **Line by line:** [source.h](line-by-line/source.md) · [ch05_source](line-by-line/ch05_source.md)

---

## Goal

Build representation 1 of the pipeline: `SourceFile`, `Span`, `LineCol`. By the end you will have:

* a file loaded into memory, with its byte-order mark stripped and its encoding checked,
* a **line map** that converts any byte offset to a line and column in O(log n),
* a `Span` type that every later stage will carry,
* and a caret-under-the-code renderer, which is the ancestor of Chapter 10's diagnostics.

This chapter looks humble. It is not. Almost every complaint people have about compilers — *"the error
points at the wrong line"*, *"it says column 14 but my editor says column 11"*, *"it choked on my file
and said nothing"* — is a bug in this layer. Getting it right once, at the bottom, means never thinking
about it again.

---

## 1. The idea: positions are byte offsets

Here is the first real design decision of the compiler, and it is one that many hand-rolled compilers
get wrong.

> **A position in the source is a single 32-bit byte offset. Not a line and column.**

The lexer walks the text with one index. Recording `(line, column)` at each token means maintaining two
extra counters, deciding what a column means when tabs are involved, and storing eight bytes instead of
four in every token. And it makes the most common operation — *"give me the text of this token"* — into a
two-dimensional lookup.

With offsets, a source position is an integer, a source *range* is two integers, and the text is
`text_.substr(begin, end - begin)`. Line and column are **computed on demand**, which happens only when
an error is actually printed — thousands of times, not millions.

This is what every serious compiler does. Clang calls it a `SourceLocation` and packs it into 32 bits;
Rust calls it a `BytePos`; GCC has `location_t`. They all resolve to line and column lazily, through a
map.

### `Span`: a half-open range

```cpp
struct Span {
    u32 begin = 0;
    u32 end   = 0;               // exclusive
    u32  length() const { return end - begin; }
    bool contains(u32 offset) const { return offset >= begin && offset < end; }
    Span merge(const Span& other) const;   // smallest span covering both
};
```

Half-open (`[begin, end)`) for the same reason every range in C++ and every slice in Python is:
`length` is `end - begin` with no `+1`, an empty range is `begin == end`, and adjacent ranges share an
endpoint instead of overlapping. Every off-by-one bug you do not have comes from this convention.

`merge` is used more than you would guess. When the parser builds `a + b`, the span of the whole
expression is `a.span.merge(b.span)` — so an error about the addition underlines everything from the
first character of `a` to the last of `b`. Build that in from the start and your error messages get
good underlines for free.

Eight bytes per span means we can afford one in every token, every AST node and every IR instruction —
which is exactly what we do, because a position that is not recorded is an error message that cannot be
written.

---

## 2. The line map

To turn offset 45 into "line 3, column 9" we need to know where each line starts. Computing that by
scanning from the beginning of the file is O(n) per query; with thousands of diagnostics and an IDE
asking constantly, that is too slow. So we build a table **once**, at load time:

```
line_starts_ = [0, 20, 37, 64, 89, 107]
                │   │   │
                │   │   └── line 3 starts at byte 37
                │   └────── line 2 starts at byte 20
                └────────── line 1 starts at byte 0
```

Then a lookup is a binary search for the last entry `<= offset`:

```cpp
LineCol line_col(u32 offset) const {
    auto it  = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    u32  idx = static_cast<u32>((it - line_starts_.begin()) - 1);
    return LineCol{idx + 1, offset - line_starts_[idx] + 1};
}
```

`upper_bound` gives the first element *strictly greater* than `offset`; step back one and you have the
line containing it. The `+ 1`s convert our 0-based arrays into the 1-based numbering that every editor
and every error message uses. Getting those `+1`s wrong is the classic "off by one line" bug, so the
chapter program round-trips every offset through `line_col` and `offset_of` and prints whether it came
back unchanged.

Cost: 4 bytes per line (a 10,000-line file costs 40 KB) and one pass at load time. Benefit: O(log n)
lookups forever.

### Line endings: three conventions, one answer

```cpp
for (u32 i = 0; i < size(); ++i) {
    char c = text_[i];
    if (c == '\n') {
        line_starts_.push_back(i + 1);
    } else if (c == '\r') {
        if (i + 1 < size() && text_[i + 1] == '\n') continue;   // CRLF: the \n will do it
        line_starts_.push_back(i + 1);                          // lone CR (classic Mac)
    }
}
```

* **LF** (`\n`) — Unix, macOS since 2001, and what git stores.
* **CRLF** (`\r\n`) — Windows, and therefore half the `.peb` files you will ever be sent.
* **CR** (`\r`) — classic Mac OS, and still emitted by a few tools.

The `continue` is the whole trick: when we see `\r` followed by `\n` we do nothing, because the `\n`
one byte later will record the line start. Miss that, and every Windows file appears to have twice as
many lines as it does, with every second one empty. This is the single most common bug in hand-written
line maps.

Note that we do **not** normalise the text (rewrite CRLF to LF). Rewriting would change every offset
after the first line ending, so a span computed by the lexer would no longer match the bytes on disk —
which breaks any tool that wants to apply a suggested fix by patching the file. We keep the bytes
exactly as they came, and we handle the awkwardness in one function.

---

## 3. Encoding: UTF-8, and only UTF-8

Source text is bytes. To read it as characters you need to know the encoding, and there is no reliable
way to detect one. So, like Go, Rust, Swift and modern C++ practice, we *declare* one:

> Pebble source files are UTF-8. Identifiers and keywords are ASCII. String and character literals,
> and comments, may contain any valid UTF-8.

Three consequences, each with code.

### The byte-order mark

Several Windows editors write three bytes — `EF BB BF` — at the start of a UTF-8 file. They mean "this
is UTF-8", which we already knew. Left in place, the lexer sees a byte that starts no token and reports
`unexpected character` at line 1, column 1, pointing at what looks like an empty position. The fix is
three lines at load time:

```cpp
void strip_bom() {
    if (text_.size() >= 3 && (u8)text_[0] == 0xEF && (u8)text_[1] == 0xBB && (u8)text_[2] == 0xBF) {
        text_.erase(0, 3);
        had_bom_ = true;
    }
}
```

We remember that it was there (`had_bom_`), because a tool that rewrites the file should put it back.

Note that we strip it *before* building the line map, so offsets are relative to the real text.

### Validation

```cpp
std::optional<u32> find_invalid_utf8() const;
```

The function decodes the length of each sequence from its lead byte (`110xxxxx` = 2 bytes,
`1110xxxx` = 3, `11110xxx` = 4), then checks that the right number of continuation bytes (`10xxxxxx`)
follow. It reports the offset of the first byte that breaks the rules.

Why bother? Because the failure mode otherwise is terrible: a truncated or mis-encoded file produces a
garbage token somewhere in the middle, and the error message contains half a character and corrupts the
terminal. A compiler should say *"this file is not valid UTF-8, at byte 86"* and stop. That is a
one-line message that saves an hour.

The checker as written is deliberately simple; a strict validator also rejects **overlong encodings**
(e.g. `C0 80` for NUL) and **surrogates** (`ED A0 80`), both of which are security-relevant because two
different byte sequences would decode to the same character. Exercise 5 adds those.

### Why identifiers are ASCII

This is a language design decision, not a technical limitation, and it is worth understanding because
Chapter 67 asks you to make it yourself.

Allowing Unicode identifiers (as C++, Java, Python 3, Rust and Swift do) means deciding:

* **Which characters are letters?** The answer is a 30,000-entry table from Unicode Annex #31, and it
  changes with each Unicode version — so your language's grammar depends on a table that was updated
  after your compiler shipped.
* **When are two identifiers the same?** `é` can be written as one code point (U+00E9) or as `e` plus a
  combining accent (U+0301). They look identical. Are they the same name? Getting this right means
  **normalising** every identifier to NFC before comparing.
* **What about lookalikes?** Cyrillic `а` (U+0430) and Latin `a` (U+0061) render identically in most
  fonts. Two functions with "the same" name that are different functions is a real, exploited
  supply-chain attack vector.
* **What about direction?** In 2021 the *Trojan Source* attack showed that bidirectional control
  characters (U+202E and friends) can make source code *display* in a different order from how it
  *compiles* — so a reviewer sees `if (isAdmin)` where the compiler sees something else. Every major
  compiler now warns about bidi characters in source.

None of that is insurmountable, and a friendly language should probably support Unicode identifiers. But
it is a chapter of work that teaches nothing about compilers, so Pebble says ASCII for identifiers, full
UTF-8 everywhere else, and — importantly — *says so in the spec* rather than leaving it to whatever the
lexer happens to do.

### Columns: bytes, code points, or screen positions?

There are three different "column 22" for the line `    // prix en euros €`:

| Measure | Value at the `€` | Who wants it |
|---------|------------------|--------------|
| Byte column | 22 | our internal arithmetic |
| Code point column | 22 | the Language Server Protocol (actually UTF-16 units) |
| Display column | 22, and **one** column wide though three bytes long | a caret in a terminal |

And with tabs there is a fourth answer, because a tab advances to the next tab stop, which depends on a
setting in the *reader's* editor.

Our `display_col` handles both tabs and UTF-8:

```cpp
u32 display_col(u32 offset, u32 tab_width = 4) const {
    u32 col = 1;
    for (u32 i = bol; i < offset; ++i) {
        if (text_[i] == '\t') col += tab_width - ((col - 1) % tab_width);
        else if ((u8(text_[i]) & 0xC0) != 0x80) col += 1;     // skip UTF-8 continuation bytes
    }
    return col;
}
```

The tab arithmetic says "advance to the next multiple of `tab_width`". The UTF-8 line says "count only
lead bytes", because a continuation byte (`10xxxxxx`) is part of a character already counted. (A fully
correct renderer would also handle double-width CJK characters and zero-width combining marks; `wcwidth`
is the rabbit hole, and Chapter 10 notes where it goes.)

`LineCol.col` stays a **byte** column, because that is what we can compute in O(1) and what our own
arithmetic needs. `display_col` is used only for drawing carets. Keeping the two apart, and naming them
differently, prevents a whole family of "the caret is under the wrong character" bugs.

---

## 4. Ownership: who owns the text

```cpp
class SourceFile {
    std::string      name_;
    std::string      text_;
    std::vector<u32> line_starts_;
    SourceFile(const SourceFile&) = delete;          // no copies
};
```

**One owner, and it is `SourceFile`.** Every `std::string_view` in the entire compiler — every token's
text, every identifier spelling, every string literal — points into `text_`. So:

* Copying is **deleted**. A copy would give you two buffers and views pointing at the wrong one.
* Moving is allowed but documented as "before any view exists". Moving a `std::string` transfers the
  heap buffer, so long strings survive; short ones (under ~15 characters, stored inline by the
  small-string optimisation) do not. Rather than reason about that, the rule is simply: construct the
  `SourceFile`, then lex it, and never move it again. `std::optional<SourceFile>` returned from `read`
  is fine because nothing has looked at it yet.
* `at(offset)` returns `'\0'` past the end instead of asserting. That single decision removes a bounds
  check from every line of the lexer in the next chapter: `while (is_digit(at(i)))` terminates at the
  end of the file on its own, because `'\0'` is not a digit.

**Why read the whole file into memory?** Because the compiler needs random access to the text long after
lexing — to print an error, to serve a hover request, to apply a fix. Streaming would mean re-reading.
At 1 byte per byte of source, a 100,000-line file costs about 3 MB, which is nothing; and every real
compiler does the same, or memory-maps the file (which is faster for large files but adds platform code
and a nasty failure mode if the file changes while you read it).

**Multiple files.** A real compiler has many, so a position must say *which*. The standard solution is a
`SourceManager` that owns all the files and hands out globally unique offsets: file 1 occupies
`[0, 20000)`, file 2 `[20000, 31000)`, and so on. Then a `Span` still fits in two `u32`s, and
`SourceManager::line_col(offset)` finds the file first by binary search, then the line. That is exactly
Clang's design, and Chapter 61 adds it when modules arrive. Until then, one file, one `SourceFile`.

---

## 5. The caret renderer

This is twenty lines, and it is the reason the rest of the book can produce good errors:

```cpp
static void show_span(const SourceFile& src, Span span, const std::string& message) {
    LineCol     lc     = src.line_col(span.begin);
    std::string number = std::to_string(lc.line);
    std::string gutter(number.size(), ' ');

    std::cout << gutter << "--> " << src.name() << ":" << lc.line << ":" << lc.col << "\n";
    std::cout << gutter << " |\n";

    std::string_view text = src.line_text(lc.line);
    std::string      shown;
    for (char c : text) {                                    // expand tabs, or the
        if (c == '\t') shown.append(4 - (shown.size() % 4), ' ');   // caret will not line up
        else           shown.push_back(c);
    }
    std::cout << number << " | " << shown << "\n";

    u32 col   = src.display_col(span.begin);
    u32 width = std::max<u32>(1, src.display_col(span.end) - col);
    std::cout << gutter << " | " << std::string(col - 1, ' ')
              << std::string(width, '^') << " " << message << "\n";
}
```

Three details that matter more than they look:

* **The gutter is as wide as the line number**, so the `|` characters line up whether you are on line 7
  or line 1207.
* **Tabs in the source line are expanded to spaces before printing.** If you print the tab and then
  count display columns for the caret, the two disagree in any terminal whose tab width is not yours.
  Expanding means *we* control the alignment. (Rust's compiler does exactly this, and had a long bug
  report about it.)
* **`width` is at least 1**, so a zero-length span (an insertion point, "expected `;` here") still gets
  a caret.

---

## Run it

```bat
run ch05_source
```

or point it at a real file:

```bat
run ch05_source examples\hello.peb
run ch05_source docs\05-source-text.md
```

---

## What you should see

```
=== 1. THE FILE ===
name       : sample.peb
bytes      : 108
lines      : 6
had bom    : no
valid utf-8: yes

=== 2. THE LINE MAP ===
line  start  bytes  text
   1      0     20  'fn main() -> int {'
   2     20     17  '	let price = 19;'
   3     37     27  '    let total = price * 2;'
   4     64     25  '    // prix en euros €'
   5     89     18  '    return total;'
   6    107      1  '}'
(line 1 is 20 bytes because CRLF is two of them, and the map
 counts CRLF as ONE line ending)

=== 3. OFFSET -> LINE:COL, AND BACK ===
offset   0 -> line 1, col  1   (display col 1)   round trip -> 0  ok
offset   5 -> line 1, col  6   (display col 6)   round trip -> 5  ok
offset  20 -> line 2, col  1   (display col 1)   round trip -> 20  ok
offset  21 -> line 2, col  2   (display col 5)   round trip -> 21  ok
offset  25 -> line 2, col  6   (display col 9)   round trip -> 25  ok
offset 107 -> line 6, col  1   (display col 1)   round trip -> 107  ok
offset 108 -> line 6, col  2   (display col 2)   round trip -> 108  ok

=== 4. SPANS ===
first  = [25,30) = 'price'
second = [53,58) = 'price'
merged = [25,58) spans 2 to 3, 33 bytes
does the merged span contain offset 27? yes

=== 5. A CARET UNDER A SPAN ===
 --> sample.peb:2:6
  |
2 |     let price = 19;
  |         ^^^^^ declared here (note: the line is TAB-indented)

 --> sample.peb:3:9
  |
3 |     let total = price * 2;
  |         ^^^^^ and here

 --> sample.peb:4:22
  |
4 |     // prix en euros €
  |                      ^ 3 bytes, but ONE column wide

=== 6. UTF-8 VALIDATION ===
the sample            : valid
with one byte smashed : invalid at byte 86 (line 4, col 23)
truncated at the end  : invalid at byte 85

=== 7. THE BOM ===
had_bom     : yes
first byte  : 'f' (would be 0xEF if we had not stripped it)
line 1      : 'fn main() -> int { return 0; }'

All source-text machinery exercised.
```

Read section 3 carefully. Offset 21 is at **byte** column 2 but **display** column 5, because a tab
precedes it. Offset 25 is byte column 6, display column 9. If a compiler prints the byte column and your
editor shows the display column, you get a bug report titled "the column is wrong" — and both of you are
right.

Section 5 is the payoff: three carets, each correctly aligned, one under a tab-indented line and one
under a three-byte character that occupies one column.

---

## Try it yourself

1. **Two minutes.** Run `run ch05_source docs\05-source-text.md`. It reports the size, the line count and
   whether this very chapter is valid UTF-8 (it contains `€`, `→` and box-drawing characters, so it is a
   real test).
2. **Five minutes.** Delete the `continue` in `build_line_map` so CRLF counts as two line endings.
   Re-run. Line 1 is unchanged, but every later line number is wrong, and the caret in section 5 lands
   on an empty line. This is what the bug looks like in the wild.
3. **Five minutes.** Change `display_col`'s default `tab_width` to 8 and re-run. The caret moves. Now you
   know why several compilers have a `-ftabstop=` flag, and why arguing about tabs is not entirely
   frivolous.
4. **Ten minutes.** Add `SourceFile::span_of_line(u32 line)` returning a `Span` that excludes the line
   terminator, and use it to print each line's text without `line_text`. (You now have two ways to do the
   same thing — decide which one you would keep, and delete the other. Keeping both is how APIs rot.)
5. **Twenty minutes.** Make `find_invalid_utf8` strict: reject overlong encodings (a 2-byte sequence
   whose value is below 0x80, etc.), surrogates (U+D800–U+DFFF), and code points above U+10FFFF. Test it
   with the byte sequences `C0 80`, `E0 80 80`, `ED A0 80`, `F5 80 80 80`. Each of those is invalid for a
   different reason, and each one has been a security advisory somewhere.
6. **Twenty minutes.** Add detection of bidirectional control characters (U+202A–U+202E, U+2066–U+2069)
   anywhere in the file, and print a warning span for each. You have just implemented the Trojan Source
   mitigation that every major compiler added in 2021.
7. **Half an hour.** Build the multi-file `SourceManager` described in §4: it owns a
   `std::vector<std::unique_ptr<SourceFile>>`, assigns each file a range of global offsets, and answers
   `file_of(offset)`, `line_col(offset)` and `slice(Span)`. Make sure a `Span` never crosses a file
   boundary, and assert that it does not.

---

## Common problems

**The caret is one column to the left or right.**
Almost always a 0-based/1-based mix-up: `col` is 1-based, so the number of spaces before the caret is
`col - 1`. The chapter program's round-trip check in section 3 exists to catch this.

**Every line number is doubled on Windows-authored files.**
CRLF counted twice. See exercise 2.

**`line_text` includes a stray `\r`.**
The trailing-terminator trim must strip *both* `\n` and `\r`, in that order, because the last two bytes
of a CRLF line are `\r\n`.

**An `assert` fires, or you read garbage, when a span reaches the end of the file.**
`slice` clamps both endpoints for exactly this reason. The "no newline at end of file" case (our sample's
line 6) is the one people forget: the last line has no terminator, so its end is `size()`, not a line
start.

**A `std::string_view` from the lexer prints rubbish later on.**
The `SourceFile` was copied or moved after lexing, or it went out of scope. Copies are deleted; look for
a move, or for a `SourceFile` created inside a function and returned after tokens were taken from it.

**Reading a file gives 0 bytes but no error.**
You opened it in text mode on Windows and it contains a `0x1A` (Ctrl-Z) byte, which historically means
end of file. `std::ios::binary` — which `read()` uses — avoids that, and also stops CRLF being silently
translated, which would shift every offset.

---

## Check yourself

1. Why store a byte offset in each token rather than a line and column?
2. `line_starts_` for a file whose last line has no newline: how many entries, and why is the last line
   still findable?
3. Why is `at(offset)` defined to return `'\0'` past the end, rather than asserting?
4. The compiler is told to underline `a + b`, where `a` is on line 3 and `b` on line 5. What should the
   renderer do, and what does ours do?
5. Why do we not normalise CRLF to LF at load time, since it would simplify everything downstream?

<details>
<summary>Answers</summary>

1. It is half the size, it is what the lexer already has (its loop index), it makes "the text of this
   token" a substring rather than a two-dimensional lookup, and it defers the only expensive part —
   deciding what a column *means* with tabs and Unicode — to the moment a human actually reads a message.
2. The number of line *starts*, which is the number of lines: 6 for our sample. The last line is findable
   because `line_span` uses `size()` as the end when there is no following start. A common bug is to push
   a final entry at `size()` when the file ends with a newline, which creates a phantom empty last line
   — try it and watch the line count go to 7.
3. Because it removes an explicit bounds check from every character test in the lexer. `is_digit('\0')`
   is false, `is_ident_start('\0')` is false, so every scanning loop stops at the end of the file
   naturally. This "sentinel" technique is ancient and still used by Clang and V8. (Note that it works
   only because `'\0'` is not part of any valid token — if it were, the lexer would need a real
   end-of-input flag.)
4. It should print both lines, with the caret starting on line 3 and a marker on line 5 — GCC and Clang
   print the range with `...` in between when the lines are far apart. Ours prints only the line
   containing `span.begin`, which is a known limitation; Chapter 10 extends it to multi-line spans and
   this is exercise 4 there.
5. Because normalising changes the byte offsets of everything after the first line ending, so a span
   computed by the compiler would no longer match the file on disk. Any tool that applies a suggested
   fix, or an editor that highlights a range the compiler reported, would be off by one byte per
   preceding line. Keeping the bytes untouched and handling the awkwardness in one function is the
   cheaper trade.
</details>

---

## Where we are

Representation 1 exists. We can load a file, know where everything in it is, and point at any part of it.

The next chapter is the first theory chapter: what a *token* is, formally, and why "the longest match
wins" is the right rule. Then Chapter 7 writes the lexer that turns this text into representation 2.

[Next: Chapter 6 — Theory: alphabets, languages, regular expressions →](06-theory-regular-languages.md)
