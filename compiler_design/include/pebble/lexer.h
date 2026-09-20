// pebble/lexer.h
// ------------------------------------------------------------
// Translation 1 -> 2:  SOURCE TEXT  ->  TOKENS.
//
// A hand-written scanner: one switch on the first character, then one small
// function per token shape. This is what GCC, Clang, Rust and Go all do, for
// three reasons: it is the fastest, it gives the best error messages, and it can
// break the rules in the two or three places where a real language needs it to.
//
// Chapter 7  - the core: positions, the switch, identifiers, trivia
// Chapter 8  - numbers, strings, characters, nested comments
// Chapter 9  - keywords and multi-character operators (maximal munch)
// Chapter 10 - the diagnostics it produces
//
// Explained in docs/07-lexer-part1.md, 08-lexer-part2.md, 09-keywords-and-operators.md
//          and docs/line-by-line/lexer.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "pebble/diag.h"
#include "pebble/source.h"
#include "pebble/token.h"

namespace pebble {

// ============================================================
// Character predicates
// ============================================================
// Our own, not <cctype>'s, for three reasons:
//   1. std::isalpha takes an int that must be a valid unsigned char; passing a
//      negative char (any UTF-8 continuation byte) is undefined behaviour;
//   2. <cctype> depends on the current locale, so "is this a letter" could
//      change with an environment variable. A compiler must not do that;
//   3. these inline to two comparisons.
inline bool is_space(char c)  { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool is_digit(char c)  { return c >= '0' && c <= '9'; }
inline bool is_bin_digit(char c) { return c == '0' || c == '1'; }
inline bool is_oct_digit(char c) { return c >= '0' && c <= '7'; }
inline bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
inline bool is_ident_start(char c) { return is_alpha(c) || c == '_'; }
inline bool is_ident_cont(char c)  { return is_ident_start(c) || is_digit(c); }

inline int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Encode one Unicode code point as UTF-8. Needed by the \u{...} escape.
inline void append_utf8(std::string& out, u32 cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// ============================================================
// Lexer
// ============================================================
class Lexer {
public:
    Lexer(const SourceFile& src, Diagnostics& diag) : src_(src), diag_(diag) {
        if (auto bad = src_.find_invalid_utf8()) {
            diag_.error(Span(*bad, *bad + 1), "file is not valid UTF-8")
                .code("E0001")
                .note("Pebble source files must be UTF-8 encoded");
        }
    }

    // If true, comments are returned as tokens instead of being skipped. The
    // compiler wants false; a formatter or syntax highlighter wants true.
    bool keep_comments = false;

    // The whole file, in one pass. Always ends with exactly one Eof token.
    std::vector<Token> tokenize() {
        std::vector<Token> out;
        out.reserve(src_.size() / 4 + 8);      // ~4 bytes per token, measured
        for (;;) {
            Token t = next();
            out.push_back(t);
            if (t.kind == TokenKind::Eof) break;
        }
        return out;
    }

    // Decoded string literals live here; Token::value_index indexes this.
    const std::vector<std::string>& string_values() const { return strings_; }

private:
    const SourceFile&        src_;
    Diagnostics&             diag_;
    u32                      pos_ = 0;
    std::vector<std::string> strings_;

    // ---- the cursor -----------------------------------------
    // at() returns '\0' past the end, so none of these needs a bounds check.
    char cur() const { return src_.at(pos_); }
    char peek(u32 n = 1) const { return src_.at(pos_ + n); }
    bool at_end() const { return pos_ >= src_.size(); }
    char advance() { return src_.at(pos_++); }

    // Consume one character if it is the one we want. The workhorse of
    // multi-character operator lexing.
    bool eat(char c) {
        if (cur() != c) return false;
        pos_++;
        return true;
    }

    Token make(TokenKind kind, u32 start) const {
        Token t;
        t.kind = kind;
        t.span = Span(start, pos_);
        t.text = src_.slice(t.span);
        return t;
    }

    // ============================================================
    // next(): skip trivia, then dispatch on the first character
    // ============================================================
    Token next() {
        // Trivia can produce a token (a comment, when keep_comments is on),
        // so the loop is written to allow that.
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

    // ============================================================
    // Identifiers and keywords                        (Chapters 7, 9)
    // ============================================================
    Token lex_ident() {
        u32 start = pos_;
        while (is_ident_cont(cur())) pos_++;

        // A non-ASCII byte glued to an identifier is almost always an accident
        // (a smart quote, a non-breaking space, a Cyrillic lookalike). Say so
        // clearly rather than emitting a mysterious Unknown token.
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

        Token t = make(TokenKind::Ident, start);
        if (auto kw = lookup_keyword(t.text)) t.kind = *kw;   // Chapter 9
        return t;
    }

    // ============================================================
    // Numbers                                              (Chapter 8)
    // ============================================================
    Token lex_number() {
        u32 start = pos_;

        // 0x..., 0b..., 0o...
        if (cur() == '0' && (peek() == 'x' || peek() == 'X')) return lex_radix(start, 16, "hex");
        if (cur() == '0' && (peek() == 'b' || peek() == 'B')) return lex_radix(start, 2, "binary");
        if (cur() == '0' && (peek() == 'o' || peek() == 'O')) return lex_radix(start, 8, "octal");

        while (is_digit(cur()) || cur() == '_') pos_++;

        bool is_float = false;

        // A '.' makes it a float only if a DIGIT follows. That is what keeps
        // `1..10` (a range) and `x.field` lexable. See docs/06 section 4.
        if (cur() == '.' && is_digit(peek())) {
            is_float = true;
            pos_++;
            while (is_digit(cur()) || cur() == '_') pos_++;
        }
        if (cur() == 'e' || cur() == 'E') {
            u32  save = pos_;
            u32  p    = pos_ + 1;
            if (src_.at(p) == '+' || src_.at(p) == '-') p++;
            if (is_digit(src_.at(p))) {
                is_float = true;
                pos_     = p;
                while (is_digit(cur()) || cur() == '_') pos_++;
            } else {
                pos_ = save;                  // `1e` is the integer 1 then ident `e`
            }
        }

        Token t = make(is_float ? TokenKind::FloatLit : TokenKind::IntLit, start);

        // `123abc` is never what anyone meant, and the parser's message for it
        // ("expected ';', found identifier") is unhelpful. Catch it here.
        if (is_ident_start(cur())) {
            u32 bad = pos_;
            while (is_ident_cont(cur())) pos_++;
            diag_.error(Span(bad, pos_), "identifier immediately after a number")
                .code("E0003")
                .help(Span(bad, bad), " ", "separate them with a space");
            t.span      = Span(start, pos_);
            t.text      = src_.slice(t.span);
            t.erroneous = true;
            return t;
        }

        if (is_float) t.float_value = parse_float(t.text);
        else          t.int_value   = parse_decimal(t.text, t.span, t.erroneous);
        return t;
    }

    Token lex_radix(u32 start, int base, const char* what) {
        pos_ += 2;                                   // the 0x / 0b / 0o
        u32  digits_begin = pos_;
        u64  value        = 0;
        bool overflow     = false;
        bool any          = false;
        while (true) {
            char c = cur();
            if (c == '_') { pos_++; continue; }
            int  d = hex_value(c);
            if (d < 0 || d >= base) break;
            pos_++;
            any = true;
            if (value > (UINT64_MAX - static_cast<u64>(d)) / static_cast<u64>(base))
                overflow = true;
            value = value * static_cast<u64>(base) + static_cast<u64>(d);
        }
        Token t = make(TokenKind::IntLit, start);
        if (!any) {
            diag_.error(t.span, std::string("expected at least one ") + what + " digit")
                .code("E0004");
            t.erroneous = true;
        } else if (overflow || value > static_cast<u64>(INT64_MAX)) {
            diag_.error(t.span, "integer literal is too large for 'int'")
                .code("E0005")
                .note("'int' is a signed 64-bit integer; the maximum is 9223372036854775807");
            t.erroneous = true;
        }
        // A digit of the wrong base is a better error than "junk after number".
        if (is_ident_cont(cur()) || hex_value(cur()) >= 0) {
            u32 bad = pos_;
            while (is_ident_cont(cur())) pos_++;
            diag_.error(Span(bad, pos_),
                        std::string("invalid digit for a ") + what + " literal")
                .code("E0006");
            t.span      = Span(start, pos_);
            t.text      = src_.slice(t.span);
            t.erroneous = true;
        }
        (void)digits_begin;
        t.int_value = value;
        return t;
    }

    u64 parse_decimal(std::string_view text, Span span, bool& erroneous) {
        u64  value    = 0;
        bool overflow = false;
        for (char c : text) {
            if (c == '_') continue;
            u64 d = static_cast<u64>(c - '0');
            if (value > (UINT64_MAX - d) / 10) overflow = true;
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

    double parse_float(std::string_view text) {
        std::string clean;
        clean.reserve(text.size());
        for (char c : text)
            if (c != '_') clean.push_back(c);
        return std::strtod(clean.c_str(), nullptr);
    }

    // ============================================================
    // Strings and characters                               (Chapter 8)
    // ============================================================
    Token lex_string() {
        u32         start = pos_;
        pos_++;                                       // the opening quote
        std::string value;
        bool        bad = false;

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

        Token t       = make(TokenKind::StringLit, start);
        t.value_index = static_cast<u32>(strings_.size());
        t.erroneous   = bad;
        strings_.push_back(std::move(value));
        return t;
    }

    Token lex_char() {
        u32         start = pos_;
        pos_++;                                       // the opening quote
        std::string value;
        bool        bad = false;

        if (cur() == '\'') {
            pos_++;
            diag_.error(Span(start, pos_), "empty character literal").code("E0008");
            Token t = make(TokenKind::CharLit, start);
            t.erroneous = true;
            return t;
        }
        if (at_end() || cur() == '\n') {
            diag_.error(Span(start, pos_), "unterminated character literal").code("E0009");
            Token t = make(TokenKind::CharLit, start);
            t.erroneous = true;
            return t;
        }
        if (cur() == '\\') read_escape(value, bad);
        else               value.push_back(advance());

        if (!eat('\'')) {
            u32 extra = pos_;
            while (!at_end() && cur() != '\'' && cur() != '\n') pos_++;
            bool closed = eat('\'');
            diag_.error(Span(start, pos_),
                        closed ? "character literal must contain exactly one character"
                               : "unterminated character literal")
                .code("E0009")
                .label(Span(extra, pos_), "more than one character here")
                .help(Span(start, start + 1), "\"", "use double quotes for a string");
            bad = true;
        }

        Token t     = make(TokenKind::CharLit, start);
        t.erroneous = bad;
        // The value is the first code point of the decoded text.
        t.int_value = value.empty() ? 0 : static_cast<u8>(value[0]);
        return t;
    }

    // Read one backslash escape, appending the decoded bytes to `out`.
    void read_escape(std::string& out, bool& bad) {
        u32 esc_start = pos_;
        pos_++;                                       // the backslash
        char c = advance();
        switch (c) {
            case 'n':  out.push_back('\n'); return;
            case 't':  out.push_back('\t'); return;
            case 'r':  out.push_back('\r'); return;
            case '0':  out.push_back('\0'); return;
            case '\\': out.push_back('\\'); return;
            case '"':  out.push_back('"');  return;
            case '\'': out.push_back('\''); return;

            case 'x': {                               // \xNN, exactly two digits
                int hi = hex_value(cur()), lo = hex_value(peek());
                if (hi < 0 || lo < 0) {
                    diag_.error(Span(esc_start, pos_), "'\\x' needs exactly two hex digits")
                        .code("E0010");
                    bad = true;
                    return;
                }
                pos_ += 2;
                out.push_back(static_cast<char>(hi * 16 + lo));
                return;
            }

            case 'u': {                               // \u{1F600}
                if (!eat('{')) {
                    diag_.error(Span(esc_start, pos_), "'\\u' must be followed by '{'")
                        .code("E0011");
                    bad = true;
                    return;
                }
                u32  cp    = 0;
                int  count = 0;
                while (hex_value(cur()) >= 0 && count < 6) {
                    cp = cp * 16 + static_cast<u32>(hex_value(advance()));
                    count++;
                }
                if (count == 0 || !eat('}')) {
                    diag_.error(Span(esc_start, pos_),
                                "expected 1 to 6 hex digits, then '}'")
                        .code("E0011");
                    bad = true;
                    return;
                }
                if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                    diag_.error(Span(esc_start, pos_), "not a valid Unicode code point")
                        .code("E0012")
                        .note("valid range is 0 to 10FFFF, excluding D800-DFFF");
                    bad = true;
                    return;
                }
                append_utf8(out, cp);
                return;
            }

            default: {
                diag_.error(Span(esc_start, pos_), "unknown escape sequence")
                    .code("E0013")
                    .note("valid escapes are \\n \\t \\r \\0 \\\\ \\\" \\' \\xNN \\u{...}")
                    .help(Span(esc_start, esc_start + 1), "\\\\",
                          "write a literal backslash as '\\\\'");
                bad = true;
                out.push_back(c);
                return;
            }
        }
    }

    // ============================================================
    // Comments                                             (Chapter 8)
    // ============================================================
    Token lex_line_comment() {
        u32 start = pos_;
        pos_ += 2;
        while (!at_end() && cur() != '\n') pos_++;
        return make(TokenKind::Unknown, start);      // only used when keep_comments
    }

    // Nested, like Rust's. A counter, which is exactly the thing a regular
    // expression cannot do - see docs/06 section 6.
    Token lex_block_comment() {
        u32 start = pos_;
        pos_ += 2;
        int depth = 1;
        while (depth > 0) {
            if (at_end()) {
                diag_.error(Span(start, start + 2), "unterminated block comment")
                    .code("E0014")
                    .note(depth == 1 ? "expected '*/' before end of file"
                                     : "block comments nest, and " +
                                           std::to_string(depth) + " are still open");
                break;
            }
            if (cur() == '/' && peek() == '*') { pos_ += 2; depth++; continue; }
            if (cur() == '*' && peek() == '/') { pos_ += 2; depth--; continue; }
            pos_++;
        }
        return make(TokenKind::Comment, start);
    }

    // ============================================================
    // Operators and punctuation                            (Chapter 9)
    // ============================================================
    // Maximal munch, written as nested eat() calls: always try the LONGEST
    // spelling first. `<<=` before `<<` before `<`.
    Token lex_operator() {
        u32  start = pos_;
        char c     = advance();
        switch (c) {
            case '(': return make(TokenKind::LParen, start);
            case ')': return make(TokenKind::RParen, start);
            case '{': return make(TokenKind::LBrace, start);
            case '}': return make(TokenKind::RBrace, start);
            case '[': return make(TokenKind::LBracket, start);
            case ']': return make(TokenKind::RBracket, start);
            case ',': return make(TokenKind::Comma, start);
            case ';': return make(TokenKind::Semi, start);
            case ':': return make(TokenKind::Colon, start);
            case '?': return make(TokenKind::Question, start);
            case '~': return make(TokenKind::Tilde, start);

            case '.': return make(eat('.') ? TokenKind::DotDot : TokenKind::Dot, start);

            case '+': return make(eat('=') ? TokenKind::PlusEq : TokenKind::Plus, start);
            case '*': return make(eat('=') ? TokenKind::StarEq : TokenKind::Star, start);
            case '/': return make(eat('=') ? TokenKind::SlashEq : TokenKind::Slash, start);
            case '%': return make(eat('=') ? TokenKind::PercentEq : TokenKind::Percent, start);
            case '^': return make(eat('=') ? TokenKind::CaretEq : TokenKind::Caret, start);

            case '-':
                if (eat('=')) return make(TokenKind::MinusEq, start);
                if (eat('>')) return make(TokenKind::Arrow, start);
                return make(TokenKind::Minus, start);

            case '=':
                if (eat('=')) return make(TokenKind::EqEq, start);
                if (eat('>')) return make(TokenKind::FatArrow, start);
                return make(TokenKind::Eq, start);

            case '!': return make(eat('=') ? TokenKind::BangEq : TokenKind::Bang, start);

            case '&':
                if (eat('&')) return make(TokenKind::AndAnd, start);
                if (eat('=')) return make(TokenKind::AmpEq, start);
                return make(TokenKind::Amp, start);

            case '|':
                if (eat('|')) return make(TokenKind::OrOr, start);
                if (eat('=')) return make(TokenKind::PipeEq, start);
                return make(TokenKind::Pipe, start);

            case '<':
                if (eat('<')) return make(eat('=') ? TokenKind::ShlEq : TokenKind::Shl, start);
                if (eat('=')) return make(TokenKind::Le, start);
                return make(TokenKind::Lt, start);

            case '>':
                if (eat('>')) return make(eat('=') ? TokenKind::ShrEq : TokenKind::Shr, start);
                if (eat('=')) return make(TokenKind::Ge, start);
                return make(TokenKind::Gt, start);

            default: break;
        }

        // Nothing matched. Report it once, with a guess where we can, and emit
        // an Unknown token so the parser can keep going.
        Token t     = make(TokenKind::Unknown, start);
        t.erroneous = true;
        auto d      = diag_.error(t.span, describe_bad_char(c)).code("E0015");
        if (const char* fix = suggest_for(c))
            d.help(t.span, fix, "did you mean this?");
        return t;
    }

    std::string describe_bad_char(char c) const {
        u8 b = static_cast<u8>(c);
        if (b >= 0x80) return "unexpected non-ASCII character";
        if (b < 0x20)  return "unexpected control character in source";
        return std::string("unexpected character '") + c + "'";
    }

    // The characters people actually type by accident. A compiler that guesses
    // well here saves a great deal of confusion.
    static const char* suggest_for(char c) {
        switch (c) {
            case '#': return "//";     // a Python or shell habit
            case '$': return nullptr;
            case '@': return nullptr;
            case '`': return "\"";
            default:  return nullptr;
        }
    }
};

// ============================================================
// Convenience and dumping
// ============================================================
inline std::vector<Token> lex_all(const SourceFile& src, Diagnostics& diag) {
    return Lexer(src, diag).tokenize();
}

inline void dump_tokens(const std::vector<Token>& tokens, const SourceFile& src,
                        const std::vector<std::string>& strings, std::ostream& os) {
    os << "line:col   kind                 text\n";
    os << "---------------------------------------------------------------\n";
    for (const Token& t : tokens) {
        LineCol     lc  = src.line_col(t.span.begin);
        std::string loc = std::to_string(lc.line) + ":" + std::to_string(lc.col);
        std::string shown;
        for (char c : t.text) {
            if (c == '\n')      shown += "\\n";
            else if (c == '\t') shown += "\\t";
            else if (c == '\r') shown += "\\r";
            else                shown.push_back(c);
        }
        os << loc;
        for (std::size_t i = loc.size(); i < 11; ++i) os << ' ';
        std::string name = token_name(t.kind);
        os << name;
        for (std::size_t i = name.size(); i < 21; ++i) os << ' ';
        os << shown;
        switch (t.kind) {
            case TokenKind::IntLit:   os << "   = " << t.int_value; break;
            case TokenKind::FloatLit: os << "   = " << t.float_value; break;
            case TokenKind::CharLit:  os << "   = " << t.int_value; break;
            case TokenKind::StringLit:
                if (t.value_index < strings.size())
                    os << "   = \"" << strings[t.value_index] << "\" ("
                       << strings[t.value_index].size() << " bytes)";
                break;
            default: break;
        }
        if (t.erroneous) os << "   [erroneous]";
        os << "\n";
    }
}

}  // namespace pebble
