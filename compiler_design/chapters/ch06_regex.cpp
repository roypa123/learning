// ch06_regex.cpp
// ------------------------------------------------------------
// Chapter 6: regular languages, made concrete.
//
// A complete (small) regular-expression engine, written by backtracking, plus a
// LEXER built out of it by the "longest match wins, then first rule wins" rule.
//
// Supported syntax:   a  .  [a-z]  [^0-9]  (ab|cd)  x*  x+  x?  \n \t \\ \. etc.
//
// The point of the chapter is three-fold:
//   1. a regular expression is a finite description of an infinite language;
//   2. "longest match, then first rule" is all a lexer needs;
//   3. backtracking is exponential, which is why Chapter 11 builds a DFA.
//
// Build & run:   run ch06_regex
//                run ch06_regex "[a-z][a-z0-9]*" hello42
//
// Explained in:  docs/06-theory-regular-languages.md
//                docs/line-by-line/ch06_regex.md
// ------------------------------------------------------------
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ============================================================
// 1. The regex AST
// ============================================================
struct RNode;
using RPtr = std::unique_ptr<RNode>;

struct RNode {
    enum class Tag { Char, Any, Class, Concat, Alt, Star, Plus, Opt, Empty } tag;

    char              ch = 0;          // Char
    bool              negate = false;  // Class
    std::vector<bool> set;             // Class: 256 flags
    std::vector<RPtr> kids;            // Concat, Alt: many;  Star/Plus/Opt: one

    explicit RNode(Tag t) : tag(t) {}
};

static RPtr make(RNode::Tag t) { return std::make_unique<RNode>(t); }

// ============================================================
// 2. A recursive-descent parser for regular expressions
// ============================================================
// Grammar, lowest precedence first - exactly the shape of Chapter 16's parser:
//
//   alt     := concat ('|' concat)*
//   concat  := repeat*
//   repeat  := atom ('*' | '+' | '?')*
//   atom    := CHAR | '.' | '[' class ']' | '(' alt ')'
//
// Alternation binds loosest, concatenation next, the postfix repeaters tightest.
// That is why `ab|cd` means (ab)|(cd) and `ab*` means a(b*).
class RegexParser {
public:
    explicit RegexParser(std::string_view pattern) : p_(pattern) {}

    RPtr parse() {
        RPtr r = parse_alt();
        if (i_ != p_.size()) die("unexpected ')' or trailing junk");
        return r;
    }

private:
    std::string_view p_;
    std::size_t      i_ = 0;

    [[noreturn]] void die(const std::string& msg) {
        std::cerr << "regex error at " << i_ << ": " << msg << "\n";
        std::cerr << "  " << p_ << "\n  " << std::string(i_, ' ') << "^\n";
        std::exit(1);
    }
    bool at_end() const { return i_ >= p_.size(); }
    char peek() const { return at_end() ? '\0' : p_[i_]; }
    char take() { return p_[i_++]; }

    RPtr parse_alt() {
        RPtr first = parse_concat();
        if (peek() != '|') return first;
        RPtr alt = make(RNode::Tag::Alt);
        alt->kids.push_back(std::move(first));
        while (peek() == '|') {
            take();
            alt->kids.push_back(parse_concat());
        }
        return alt;
    }

    RPtr parse_concat() {
        RPtr cat = make(RNode::Tag::Concat);
        while (!at_end() && peek() != '|' && peek() != ')')
            cat->kids.push_back(parse_repeat());
        if (cat->kids.empty()) return make(RNode::Tag::Empty);   // e.g. "a|" or "()"
        if (cat->kids.size() == 1) return std::move(cat->kids[0]);
        return cat;
    }

    RPtr parse_repeat() {
        RPtr atom = parse_atom();
        for (;;) {
            char c = peek();
            RNode::Tag t;
            if      (c == '*') t = RNode::Tag::Star;
            else if (c == '+') t = RNode::Tag::Plus;
            else if (c == '?') t = RNode::Tag::Opt;
            else return atom;
            take();
            RPtr rep = make(t);
            rep->kids.push_back(std::move(atom));
            atom = std::move(rep);
        }
    }

    char escape(char c) {
        switch (c) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            default:  return c;      // \. \\ \[ \* ... are the character itself
        }
    }

    RPtr parse_atom() {
        if (at_end()) die("expected something to match");
        char c = take();
        if (c == '(') {
            RPtr inner = parse_alt();
            if (peek() != ')') die("missing ')'");
            take();
            return inner;
        }
        if (c == '.') return make(RNode::Tag::Any);
        if (c == '[') return parse_class();
        if (c == '\\') {
            if (at_end()) die("trailing backslash");
            RPtr n = make(RNode::Tag::Char);
            n->ch  = escape(take());
            return n;
        }
        if (c == '*' || c == '+' || c == '?') die("nothing to repeat");
        RPtr n = make(RNode::Tag::Char);
        n->ch  = c;
        return n;
    }

    RPtr parse_class() {
        RPtr n = make(RNode::Tag::Class);
        n->set.assign(256, false);
        if (peek() == '^') { take(); n->negate = true; }
        bool first = true;
        while (!at_end() && (peek() != ']' || first)) {
            first  = false;
            char lo = take();
            if (lo == '\\' && !at_end()) lo = escape(take());
            if (peek() == '-' && i_ + 1 < p_.size() && p_[i_ + 1] != ']') {
                take();                                   // the '-'
                char hi = take();
                if (hi == '\\' && !at_end()) hi = escape(take());
                for (int k = (unsigned char)lo; k <= (unsigned char)hi; ++k)
                    n->set[k] = true;
            } else {
                n->set[(unsigned char)lo] = true;
            }
        }
        if (peek() != ']') die("missing ']'");
        take();
        return n;
    }
};

// ============================================================
// 3. The matcher: backtracking with continuations
// ============================================================
// match(node, pos, k) means: "try to match `node` starting at `pos`; if it
// succeeds ending at q, call k(q); if k also succeeds, we are done."
//
// The continuation `k` is what makes backtracking correct. `(a|ab)c` must be
// able to try `a`, fail on `c`, come back, try `ab`, and succeed - and only the
// continuation knows whether what FOLLOWS worked out.
static long long g_steps = 0;     // a crude cost meter, to show the blow-up

using Cont = std::function<bool(std::size_t)>;

static bool match(const RNode* n, std::string_view s, std::size_t pos, const Cont& k) {
    g_steps++;
    switch (n->tag) {
        case RNode::Tag::Empty:
            return k(pos);

        case RNode::Tag::Char:
            return pos < s.size() && s[pos] == n->ch && k(pos + 1);

        case RNode::Tag::Any:
            return pos < s.size() && k(pos + 1);

        case RNode::Tag::Class: {
            if (pos >= s.size()) return false;
            bool in = n->set[(unsigned char)s[pos]];
            return (in != n->negate) && k(pos + 1);
        }

        case RNode::Tag::Concat: {
            // Match kid 0, then (in its continuation) kid 1, then kid 2 ...
            // A recursive lambda over the child index expresses that directly.
            std::function<bool(std::size_t, std::size_t)> step =
                [&](std::size_t idx, std::size_t p) -> bool {
                    if (idx == n->kids.size()) return k(p);
                    return match(n->kids[idx].get(), s, p,
                                 [&, idx](std::size_t q) { return step(idx + 1, q); });
                };
            return step(0, pos);
        }

        case RNode::Tag::Alt:
            for (const RPtr& kid : n->kids)
                if (match(kid.get(), s, pos, k)) return true;    // first alternative wins
            return false;

        case RNode::Tag::Opt:
            if (match(n->kids[0].get(), s, pos, k)) return true;  // greedy: try one first
            return k(pos);                                        // then try zero

        case RNode::Tag::Star: {
            // Greedy: consume as many as possible, but be willing to give some back.
            Cont loop = nullptr;
            loop = [&](std::size_t p) -> bool {
                bool more = match(n->kids[0].get(), s, p,
                                  [&, p](std::size_t q) {
                                      if (q == p) return false;   // no progress: stop, or loop forever
                                      return loop(q);
                                  });
                return more || k(p);
            };
            return loop(pos);
        }

        case RNode::Tag::Plus: {
            // x+ is one x, then the same loop as x*.
            const RNode* child = n->kids[0].get();
            Cont         loop  = nullptr;
            loop = [&](std::size_t p) -> bool {
                bool more = match(child, s, p, [&, p](std::size_t q) {
                    if (q == p) return false;
                    return loop(q);
                });
                return more || k(p);
            };
            return match(child, s, pos, [&](std::size_t q) { return loop(q); });
        }
    }
    return false;
}

// Does the whole string match? (Anchored at both ends, which is what a lexer
// rule wants, not what `grep` does.)
static bool full_match(const RNode* re, std::string_view s) {
    return match(re, s, 0, [&](std::size_t end) { return end == s.size(); });
}

// The longest prefix of `s` that matches, or -1 for none. This is the primitive
// a lexer is built from.
static long longest_prefix(const RNode* re, std::string_view s) {
    long best = -1;
    match(re, s, 0, [&](std::size_t end) {
        if ((long)end > best) best = (long)end;
        return false;              // never accept: force exploration of every path
    });
    return best;
}

// ============================================================
// 4. A lexer, from rules
// ============================================================
struct Rule {
    std::string name;
    std::string pattern;
    RPtr        re;
};

static std::vector<Rule> make_rules() {
    std::vector<Rule> rules = {
        // ORDER MATTERS on ties: the earlier rule wins. So keywords come first.
        {"kw_let",   "let",                                       nullptr},
        {"kw_fn",    "fn",                                        nullptr},
        {"kw_if",    "if",                                        nullptr},
        {"float",    "[0-9]+\\.[0-9]+",                           nullptr},
        {"int",      "[0-9]+",                                    nullptr},
        {"ident",    "[A-Za-z_][A-Za-z_0-9]*",                    nullptr},
        {"arrow",    "->",                                        nullptr},
        {"eqeq",     "==",                                        nullptr},
        {"eq",       "=",                                         nullptr},
        {"op",       "[-+*/%<>{}();,:.]",                          nullptr},
        {"space",    "[ \t\r\n]+",                                nullptr},
        {"comment",  "//[^\n]*",                                  nullptr},
        {"string",   "\"([^\"\\\\\n]|\\\\.)*\"",                   nullptr},
    };
    for (Rule& r : rules) r.re = RegexParser(r.pattern).parse();
    return rules;
}

static void lex(const std::vector<Rule>& rules, std::string_view src) {
    std::size_t pos = 0;
    while (pos < src.size()) {
        std::string_view rest = src.substr(pos);
        long             best_len  = -1;
        const Rule*      best_rule = nullptr;
        for (const Rule& r : rules) {
            long n = longest_prefix(r.re.get(), rest);
            if (n > best_len) { best_len = n; best_rule = &r; }   // strictly >: first rule wins ties
        }
        if (best_len <= 0) {
            std::cout << "  ERROR at " << pos << ": no rule matches '" << rest[0] << "'\n";
            return;
        }
        std::string_view text = rest.substr(0, (std::size_t)best_len);
        if (best_rule->name != "space") {
            std::string shown;
            for (char c : text) {
                if (c == '\n') shown += "\\n";
                else           shown.push_back(c);
            }
            std::cout << "  " << std::setw(8) << std::left << best_rule->name
                      << std::right << " '" << shown << "'  @" << pos << "\n";
        }
        pos += (std::size_t)best_len;
    }
}

// ============================================================
static void test(const char* pattern, const std::vector<const char*>& yes,
                 const std::vector<const char*>& no) {
    RPtr re = RegexParser(pattern).parse();
    std::cout << "  /" << pattern << "/\n";
    for (const char* s : yes)
        std::cout << "      matches  '" << s << "' : "
                  << (full_match(re.get(), s) ? "yes" : "NO  <-- expected yes") << "\n";
    for (const char* s : no)
        std::cout << "      rejects  '" << s << "' : "
                  << (!full_match(re.get(), s) ? "yes" : "NO  <-- expected no") << "\n";
}

int main(int argc, char** argv) {
    if (argc > 2) {                       // ad-hoc mode: run ch06_regex <pattern> <text>
        RPtr re = RegexParser(argv[1]).parse();
        g_steps = 0;
        bool ok = full_match(re.get(), argv[2]);
        std::cout << "/" << argv[1] << "/ vs '" << argv[2] << "' : "
                  << (ok ? "match" : "no match") << "   (" << g_steps << " steps)\n";
        std::cout << "longest matching prefix: " << longest_prefix(re.get(), argv[2]) << "\n";
        return 0;
    }

    std::cout << "=== 1. TOKEN PATTERNS ===\n";
    test("[0-9]+", {"0", "7", "42", "1234567890"}, {"", "4a", "-1", "1.5"});
    test("[A-Za-z_][A-Za-z_0-9]*", {"x", "_tmp", "price2", "MAX_N"}, {"2fast", "", "a-b"});
    test("[0-9]+\\.[0-9]+", {"1.5", "0.0", "314.159"}, {"1.", ".5", "1", "1.2.3"});
    test("//[^\n]*", {"//", "// hello", "//x"}, {"/ x", "/* c */"});
    test("\"([^\"\\\\\n]|\\\\.)*\"", {"\"\"", "\"hi\"", "\"a\\\"b\"", "\"tab\\there\""},
                                     {"\"unterminated", "\"bad\nnewline\""});

    std::cout << "\n=== 2. THE THREE OPERATIONS ===\n";
    test("ab", {"ab"}, {"a", "b", "ba", "abc"});                    // concatenation
    test("a|b", {"a", "b"}, {"ab", "", "c"});                       // union
    test("a*", {"", "a", "aaaa"}, {"b", "ab"});                     // Kleene star
    test("(ab)*c", {"c", "abc", "ababc"}, {"ab", "abac"});          // all three

    std::cout << "\n=== 3. PRECEDENCE ===\n";
    test("ab|cd", {"ab", "cd"}, {"abcd", "acd", "abd"});   // (ab)|(cd), not a(b|c)d
    test("ab*",   {"a", "ab", "abbb"}, {"abab", ""});      // a(b*), not (ab)*

    std::cout << "\n=== 4. LONGEST MATCH (maximal munch) ===\n";
    {
        RPtr ident = RegexParser("[A-Za-z_][A-Za-z_0-9]*").parse();
        for (const char* s : {"price = 19", "x+1", "_9ab cd"})
            std::cout << "  ident in '" << s << "' -> longest prefix = "
                      << longest_prefix(ident.get(), s) << " chars\n";
        RPtr num = RegexParser("[0-9]+").parse();
        std::cout << "  int in '1234abc' -> " << longest_prefix(num.get(), "1234abc")
                  << " chars (NOT 1, and that is the whole rule)\n";
    }

    std::cout << "\n=== 5. A LEXER MADE OF REGEXES ===\n";
    {
        std::vector<Rule> rules = make_rules();
        const char* program =
            "let price = 19;  // a comment\n"
            "fn add(a: int) -> int { return a + 1.5; }\n"
            "let s = \"he said \\\"hi\\\"\";\n"
            "let iffy = if1;\n";
        std::cout << "source:\n" << program << "tokens:\n";
        lex(rules, program);
        std::cout << "\n  Note 'iffy' and 'if1' became ONE ident each, not 'if' + rest:\n"
                     "  longest match beats rule order. But bare 'if' became kw_if,\n"
                     "  because on a TIE the earlier rule wins.\n";
    }

    std::cout << "\n=== 6. WHY BACKTRACKING IS NOT ENOUGH ===\n";
    {
        RPtr bad = RegexParser("(a|aa)*b").parse();
        std::cout << "  pattern /(a|aa)*b/ against 'aaa...' with no 'b':\n";
        std::string s;
        for (int n = 1; n <= 20; ++n) {
            s += 'a';
            g_steps = 0;
            bool ok = full_match(bad.get(), s);
            if (n <= 4 || n % 4 == 0)
                std::cout << "    " << std::setw(2) << n << " a's -> "
                          << (ok ? "match" : "no match") << ", " << g_steps << " steps\n";
        }
        std::cout << "  The step count roughly DOUBLES per character. That is exponential,\n"
                     "  and it is a real denial-of-service bug in real regex libraries.\n"
                     "  Chapter 11 fixes it: a DFA answers in ONE step per character.\n";
    }

    std::cout << "\n=== 7. WHAT REGULAR EXPRESSIONS CANNOT DO ===\n";
    std::cout << "  No regular expression matches 'n opening parens then n closing parens'\n"
                 "  for every n. A finite automaton has finitely many states, so it cannot\n"
                 "  count without bound. Proof sketch in the chapter (the pumping lemma).\n"
                 "  Consequence: LEXING is regular, PARSING is not. That is exactly why the\n"
                 "  compiler has two separate stages instead of one.\n";
    return 0;
}
