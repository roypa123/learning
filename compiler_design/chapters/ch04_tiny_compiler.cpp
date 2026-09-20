// ch04_tiny_compiler.cpp
// ------------------------------------------------------------
// Chapter 4: a COMPLETE compiler in one file.
//
// Language:  integer arithmetic.   + - * / % ( ) and unary minus.
// Target:    x86-64 assembly text, which gcc can assemble into a real .exe.
//
// It contains, in miniature, every stage of the rest of the book:
//     1. Lexer        characters      -> tokens
//     2. Parser       tokens          -> AST          (precedence climbing)
//     3. Printer      AST             -> text         (so we can see it)
//     4. Optimiser    AST             -> smaller AST   (constant folding)
//     5. Interpreter  AST             -> a number      (the reference answer)
//     6. Codegen      AST             -> x86-64 asm    (a stack machine)
//
// Build & run:
//     run ch04_tiny_compiler
//     run ch04_tiny_compiler "2 + 3 * (10 - 4)"
//
// Then make it a real program:
//     gcc out\tiny.s -o out\tiny.exe
//     out\tiny.exe
//
// Explained in:  docs/04-tiny-compiler.md
//                docs/line-by-line/ch04_tiny_compiler.md
// ------------------------------------------------------------
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// 0. Errors
// ============================================================
// One source line, so an error report is a caret under a column.
// The real diagnostics engine is Chapter 10; this is its seed.
static std::string g_source;

[[noreturn]] static void fail(std::size_t pos, const std::string& message) {
    std::cerr << "error: " << message << "\n";
    std::cerr << "  |\n";
    std::cerr << "  | " << g_source << "\n";
    std::cerr << "  | " << std::string(pos, ' ') << "^\n";
    std::exit(1);
}

// ============================================================
// 1. Lexer:  characters -> tokens
// ============================================================
enum class Kind { Int, Plus, Minus, Star, Slash, Percent, LParen, RParen, End };

static const char* kind_name(Kind k) {
    switch (k) {
        case Kind::Int:     return "Int";
        case Kind::Plus:    return "Plus";
        case Kind::Minus:   return "Minus";
        case Kind::Star:    return "Star";
        case Kind::Slash:   return "Slash";
        case Kind::Percent: return "Percent";
        case Kind::LParen:  return "LParen";
        case Kind::RParen:  return "RParen";
        case Kind::End:     return "End";
    }
    return "<invalid>";
}

struct Tok {
    Kind        kind;
    long long   value = 0;   // only meaningful for Int
    std::size_t pos   = 0;   // byte offset in the source, for error messages
};

static std::vector<Tok> tokenize(const std::string& src) {
    std::vector<Tok> out;
    std::size_t i = 0;
    while (i < src.size()) {
        char c = src[i];

        // (a) whitespace: skip it, it carries no meaning
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        // (b) a number: consume every digit. This is "maximal munch".
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t start = i;
            long long   v     = 0;
            while (i < src.size() && std::isdigit(static_cast<unsigned char>(src[i]))) {
                v = v * 10 + (src[i] - '0');
                i++;
            }
            out.push_back({Kind::Int, v, start});
            continue;
        }

        // (c) single-character operators: a lookup, one position forward
        Kind k = Kind::End;
        switch (c) {
            case '+': k = Kind::Plus;    break;
            case '-': k = Kind::Minus;   break;
            case '*': k = Kind::Star;    break;
            case '/': k = Kind::Slash;   break;
            case '%': k = Kind::Percent; break;
            case '(': k = Kind::LParen;  break;
            case ')': k = Kind::RParen;  break;
            default:
                fail(i, std::string("unexpected character '") + c + "'");
        }
        out.push_back({k, 0, i});
        i++;
    }
    // (d) the End token. Having one means the parser never has to check for
    //     "did I run off the end?" - there is always a token to look at.
    out.push_back({Kind::End, 0, src.size()});
    return out;
}

static void dump_tokens(const std::vector<Tok>& toks) {
    for (const Tok& t : toks) {
        std::cout << "  " << kind_name(t.kind);
        if (t.kind == Kind::Int) std::cout << "(" << t.value << ")";
        std::cout << " @" << t.pos << "\n";
    }
}

// ============================================================
// 2. AST:  the shape of the program
// ============================================================
struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Node {
    enum class Tag { Int, Binary, Neg } tag;
    long long   value = 0;        // Tag::Int
    Kind        op    = Kind::End; // Tag::Binary
    NodePtr     lhs, rhs;          // Binary uses both, Neg uses lhs only
    std::size_t pos   = 0;         // where it came from

    static NodePtr make_int(long long v, std::size_t pos) {
        auto n = std::make_unique<Node>();
        n->tag = Tag::Int; n->value = v; n->pos = pos;
        return n;
    }
    static NodePtr make_binary(Kind op, NodePtr l, NodePtr r, std::size_t pos) {
        auto n = std::make_unique<Node>();
        n->tag = Tag::Binary; n->op = op; n->pos = pos;
        n->lhs = std::move(l); n->rhs = std::move(r);
        return n;
    }
    static NodePtr make_neg(NodePtr v, std::size_t pos) {
        auto n = std::make_unique<Node>();
        n->tag = Tag::Neg; n->pos = pos; n->lhs = std::move(v);
        return n;
    }
};

// ============================================================
// 3. Parser:  tokens -> AST  (precedence climbing)
// ============================================================
// Binding power of each infix operator. Bigger binds tighter, so it ends up
// DEEPER in the tree, so it is evaluated first.
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

class Parser {
public:
    explicit Parser(const std::vector<Tok>& toks) : toks_(toks) {}

    NodePtr parse() {
        NodePtr e = parse_expr(0);
        expect(Kind::End, "end of input");
        return e;
    }

private:
    const std::vector<Tok>& toks_;
    std::size_t             i_ = 0;

    const Tok& peek() const { return toks_[i_]; }
    const Tok& advance()    { return toks_[i_++]; }

    void expect(Kind k, const char* what) {
        if (peek().kind != k)
            fail(peek().pos, std::string("expected ") + what + ", found " +
                             kind_name(peek().kind));
        advance();
    }

    // A "primary" is an expression with no infix operator at the top:
    // a number, a parenthesised expression, or a unary minus applied to one.
    NodePtr parse_primary() {
        const Tok& t = peek();
        switch (t.kind) {
            case Kind::Int:
                advance();
                return Node::make_int(t.value, t.pos);

            case Kind::Minus: {                 // unary minus, e.g. -3 or -(a+b)
                advance();
                NodePtr operand = parse_primary();
                return Node::make_neg(std::move(operand), t.pos);
            }

            case Kind::LParen: {
                advance();
                NodePtr inner = parse_expr(0);   // start again at lowest precedence
                expect(Kind::RParen, "')'");
                return inner;                    // note: no node for the parens
            }

            default:
                fail(t.pos, std::string("expected a number, '-' or '(', found ") +
                            kind_name(t.kind));
        }
    }

    // The heart of it. Parse a left operand, then keep absorbing
    // "operator right-operand" pairs as long as the operator binds at least as
    // tightly as min_prec.
    NodePtr parse_expr(int min_prec) {
        NodePtr lhs = parse_primary();
        for (;;) {
            Kind op   = peek().kind;
            int  prec = precedence(op);
            if (prec < min_prec || prec < 0) return lhs;   // stop: too loose, or not an operator
            std::size_t pos = advance().pos;               // consume the operator
            // Left associative: the right side may only contain operators that
            // bind MORE tightly, so we pass prec + 1.
            NodePtr rhs = parse_expr(prec + 1);
            lhs = Node::make_binary(op, std::move(lhs), std::move(rhs), pos);
        }
    }
};

// ============================================================
// 4. Printer:  AST -> text
// ============================================================
static const char* op_symbol(Kind k) {
    switch (k) {
        case Kind::Plus:    return "+";
        case Kind::Minus:   return "-";
        case Kind::Star:    return "*";
        case Kind::Slash:   return "/";
        case Kind::Percent: return "%";
        default:            return "?";
    }
}

static void print_tree(const Node* n, std::ostream& os, int indent = 0) {
    std::string pad(indent * 2, ' ');
    switch (n->tag) {
        case Node::Tag::Int:
            os << pad << "Int " << n->value << "\n";
            break;
        case Node::Tag::Neg:
            os << pad << "Neg\n";
            print_tree(n->lhs.get(), os, indent + 1);
            break;
        case Node::Tag::Binary:
            os << pad << "Binary " << op_symbol(n->op) << "\n";
            print_tree(n->lhs.get(), os, indent + 1);
            print_tree(n->rhs.get(), os, indent + 1);
            break;
    }
}

// Also print it as an expression, fully parenthesised, so you can check that
// the tree really means what you typed.
static void print_infix(const Node* n, std::ostream& os) {
    switch (n->tag) {
        case Node::Tag::Int: os << n->value; break;
        case Node::Tag::Neg: os << "-"; print_infix(n->lhs.get(), os); break;
        case Node::Tag::Binary:
            os << "(";
            print_infix(n->lhs.get(), os);
            os << " " << op_symbol(n->op) << " ";
            print_infix(n->rhs.get(), os);
            os << ")";
            break;
    }
}

// ============================================================
// 5. Interpreter:  AST -> a number
// ============================================================
static long long eval(const Node* n) {
    switch (n->tag) {
        case Node::Tag::Int: return n->value;
        case Node::Tag::Neg: return -eval(n->lhs.get());
        case Node::Tag::Binary: {
            long long a = eval(n->lhs.get());
            long long b = eval(n->rhs.get());
            switch (n->op) {
                case Kind::Plus:    return a + b;
                case Kind::Minus:   return a - b;
                case Kind::Star:    return a * b;
                case Kind::Slash:
                    if (b == 0) fail(n->pos, "division by zero");
                    return a / b;
                case Kind::Percent:
                    if (b == 0) fail(n->pos, "remainder by zero");
                    return a % b;
                default: fail(n->pos, "unknown operator");
            }
        }
    }
    return 0;
}

// ============================================================
// 6. Optimiser:  constant folding
// ============================================================
// If both children of an operator are known numbers, do the arithmetic now, at
// compile time, and replace the whole subtree with the answer. This is the
// smallest real optimisation there is, and Chapter 41 generalises it.
static int g_folds = 0;

static bool is_int(const Node* n) { return n && n->tag == Node::Tag::Int; }

static NodePtr fold(NodePtr n) {
    if (!n) return n;
    if (n->lhs) n->lhs = fold(std::move(n->lhs));      // bottom-up: children first
    if (n->rhs) n->rhs = fold(std::move(n->rhs));

    if (n->tag == Node::Tag::Neg && is_int(n->lhs.get())) {
        g_folds++;
        return Node::make_int(-n->lhs->value, n->pos);
    }
    if (n->tag == Node::Tag::Binary && is_int(n->lhs.get()) && is_int(n->rhs.get())) {
        long long a = n->lhs->value, b = n->rhs->value;
        if ((n->op == Kind::Slash || n->op == Kind::Percent) && b == 0)
            return n;                                   // leave it; eval will report it
        g_folds++;
        switch (n->op) {
            case Kind::Plus:    return Node::make_int(a + b, n->pos);
            case Kind::Minus:   return Node::make_int(a - b, n->pos);
            case Kind::Star:    return Node::make_int(a * b, n->pos);
            case Kind::Slash:   return Node::make_int(a / b, n->pos);
            case Kind::Percent: return Node::make_int(a % b, n->pos);
            default: break;
        }
        g_folds--;
    }
    return n;
}

// ============================================================
// 7. Code generator:  AST -> x86-64 assembly
// ============================================================
// Strategy: a STACK MACHINE. Every subexpression leaves its value on the
// machine stack. That is not efficient - Part 6 does it properly with
// registers - but it is only ten lines and it always works, at any nesting
// depth, because the hardware stack does the bookkeeping for us.
//
//   Int n     ->  push n
//   a + b     ->  <code for a>  <code for b>  pop r10 / pop rax / add / push
//   -a        ->  <code for a>  pop rax / neg rax / push rax

#ifdef _WIN32
static const char* kArg1     = "rcx";       // Microsoft x64: 1st integer argument
static const char* kArg2     = "rdx";       //                2nd
static const char* kRoSection = ".rdata";
#else
static const char* kArg1     = "rdi";       // System V: 1st integer argument
static const char* kArg2     = "rsi";       //           2nd
static const char* kRoSection = ".rodata";
#endif

static void gen_expr(const Node* n, std::ostream& os) {
    switch (n->tag) {
        case Node::Tag::Int:
            os << "        mov     rax, " << n->value << "\n";
            os << "        push    rax\n";
            return;

        case Node::Tag::Neg:
            gen_expr(n->lhs.get(), os);
            os << "        pop     rax\n";
            os << "        neg     rax\n";
            os << "        push    rax\n";
            return;

        case Node::Tag::Binary:
            gen_expr(n->lhs.get(), os);        // leaves lhs on the stack
            gen_expr(n->rhs.get(), os);        // leaves rhs on top of it
            os << "        pop     r10\n";     // r10 = rhs  (top of stack)
            os << "        pop     rax\n";     // rax = lhs
            switch (n->op) {
                case Kind::Plus:  os << "        add     rax, r10\n"; break;
                case Kind::Minus: os << "        sub     rax, r10\n"; break;
                case Kind::Star:  os << "        imul    rax, r10\n"; break;
                case Kind::Slash:
                    os << "        cqo\n";            // sign-extend rax into rdx:rax
                    os << "        idiv    r10\n";    // quotient in rax, remainder in rdx
                    break;
                case Kind::Percent:
                    os << "        cqo\n";
                    os << "        idiv    r10\n";
                    os << "        mov     rax, rdx\n";
                    break;
                default: break;
            }
            os << "        push    rax\n";
            return;
    }
}

static void gen_program(const Node* root, std::ostream& os, const std::string& expr) {
    os << "# ------------------------------------------------------------\n";
    os << "# Generated by ch04_tiny_compiler from:  " << expr << "\n";
    os << "# Assemble and run:   gcc tiny.s -o tiny.exe && tiny.exe\n";
    os << "# ------------------------------------------------------------\n";
    os << "        .intel_syntax noprefix\n";
    os << "        .globl  main\n";
    os << "        .text\n";
    os << "main:\n";
    os << "        push    rbp\n";              // standard prologue
    os << "        mov     rbp, rsp\n";
    os << "        sub     rsp, 32\n";          // shadow space (Windows) + alignment
    gen_expr(root, os);                          // ... value ends up on the stack
    os << "        pop     rax\n";              // rax = the answer
    os << "        lea     " << kArg1 << ", [rip + fmt]\n";
    os << "        mov     " << kArg2 << ", rax\n";
    os << "        call    printf\n";
    os << "        xor     eax, eax\n";         // return 0 from main
    os << "        mov     rsp, rbp\n";         // standard epilogue
    os << "        pop     rbp\n";
    os << "        ret\n";
    os << "        .section " << kRoSection << "\n";
    os << "fmt:\n";
    os << "        .asciz  \"%lld\\n\"\n";
}

// ============================================================
// main:  run every stage and show its output
// ============================================================
int main(int argc, char** argv) {
    // Join all arguments so both of these work:
    //   run ch04_tiny_compiler "2 + 3 * 4"
    //   run ch04_tiny_compiler 2 + 3 * 4
    std::string expr;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) expr += ' ';
        expr += argv[i];
    }
    if (expr.empty()) expr = "2 + 3 * (10 - 4)";
    g_source = expr;

    std::cout << "=== 0. SOURCE ===\n  " << expr << "\n";

    std::cout << "\n=== 1. TOKENS ===\n";
    std::vector<Tok> toks = tokenize(expr);
    dump_tokens(toks);

    std::cout << "\n=== 2. AST ===\n";
    NodePtr tree = Parser(toks).parse();
    print_tree(tree.get(), std::cout, 1);
    std::cout << "  as an expression: ";
    print_infix(tree.get(), std::cout);
    std::cout << "\n";

    std::cout << "\n=== 3. INTERPRETED ===\n";
    std::cout << "  " << eval(tree.get()) << "\n";

    std::cout << "\n=== 4. OPTIMISED (constant folding) ===\n";
    tree = fold(std::move(tree));
    print_tree(tree.get(), std::cout, 1);
    std::cout << "  " << g_folds << " subtree(s) folded at compile time\n";

    std::cout << "\n=== 5. X86-64 ASSEMBLY ===\n";
    std::ostringstream asm_text;
    gen_program(tree.get(), asm_text, expr);
    std::cout << asm_text.str();

    std::error_code ec;
    std::filesystem::create_directories("out", ec);
    std::ofstream f("out/tiny.s", std::ios::binary);
    if (!f) {
        std::cerr << "warning: could not write out/tiny.s\n";
        return 1;
    }
    f << asm_text.str();
    f.close();

    std::cout << "\nWrote out/tiny.s  ->  now run:\n";
    std::cout << "    gcc out/tiny.s -o out/tiny.exe\n";
    std::cout << "    out" << static_cast<char>(std::filesystem::path::preferred_separator)
              << "tiny.exe\n";
    return 0;
}
