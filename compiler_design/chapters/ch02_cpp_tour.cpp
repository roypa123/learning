// ch02_cpp_tour.cpp
// ------------------------------------------------------------
// Chapter 2: the C++ this book uses, in one runnable program.
//
// Ten idioms, each in its own function, each printing what it did:
//   1. enum class + exhaustive switch
//   2. std::string_view over a stable buffer
//   3. why we store INDICES, not pointers, into vectors
//   4. std::unique_ptr trees (the AST shape)
//   5. std::variant as a sum type (the other AST shape)
//   6. std::optional for "maybe found"
//   7. string interning with std::unordered_map
//   8. RAII for scope push/pop
//   9. building text with std::ostringstream
//  10. assertions as machine-checked comments
//
// Build & run:   run ch02_cpp_tour
// Explained in:  docs/02-cpp-for-compiler-writers.md
//                docs/line-by-line/ch02_cpp_tour.md
// ------------------------------------------------------------
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

// ============================================================
// 1. enum class + exhaustive switch
// ============================================================
// A compiler is full of "one of these N kinds" values. `enum class` gives us
// names that do not collide and do not silently convert to int.
enum class TokenKind { Int, Plus, Star, LParen, RParen, End };

// Turning a kind into text is needed constantly (error messages, dumps).
// Note: no `default:` label. That is deliberate — if someone adds a kind to the
// enum and forgets this function, -Wall reports it as a warning instead of
// silently returning "?" at run time.
static const char* kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::Int:    return "Int";
        case TokenKind::Plus:   return "Plus";
        case TokenKind::Star:   return "Star";
        case TokenKind::LParen: return "LParen";
        case TokenKind::RParen: return "RParen";
        case TokenKind::End:    return "End";
    }
    return "<invalid>";   // only reached if the value is not a valid enumerator
}

static void demo_enum_class() {
    std::cout << "\n-- 1. enum class --\n";
    TokenKind k = TokenKind::Star;
    std::cout << "kind_name(Star) = " << kind_name(k) << "\n";
    // int n = k;                 // ERROR: no implicit conversion. Good.
    int n = static_cast<int>(k);  // explicit is allowed, and says what we mean
    std::cout << "as int         = " << n << "\n";
}

// ============================================================
// 2. std::string_view over a stable buffer
// ============================================================
// A token's text is a slice of the source file. Copying it into a std::string
// would allocate once per token. A string_view is just {pointer, length}: no
// allocation, no copy. The rule: the buffer it points into must outlive it.
struct Token {
    TokenKind        kind;
    std::string_view text;    // points INTO the source string, does not own it
    std::size_t      offset;  // where it started, for error messages
};

static void demo_string_view(const std::string& source) {
    std::cout << "\n-- 2. string_view --\n";
    Token t{TokenKind::Int, std::string_view(source).substr(8, 2), 8};
    std::cout << "text = '" << t.text << "'  size = " << t.text.size()
              << "  offset = " << t.offset << "\n";
    std::cout << "points inside source? "
              << (t.text.data() >= source.data() &&
                  t.text.data() < source.data() + source.size())
              << "\n";
}

// ============================================================
// 3. Indices, not pointers, into vectors
// ============================================================
// A vector moves its elements when it grows. Any pointer or reference into it
// becomes dangling. Compilers build vectors incrementally all the time, so we
// store indices and look them up when needed.
using NodeId = std::uint32_t;   // a named index: self-documenting, and 4 bytes

struct FlatNode {
    TokenKind op;
    NodeId    lhs;
    NodeId    rhs;
    long long value;
};

static void demo_indices() {
    std::cout << "\n-- 3. indices, not pointers --\n";
    std::vector<FlatNode> nodes;
    nodes.push_back({TokenKind::Int, 0, 0, 2});          // id 0
    const FlatNode* danger = &nodes[0];                  // DO NOT DO THIS
    for (int i = 0; i < 100; ++i)                        // force reallocation
        nodes.push_back({TokenKind::Int, 0, 0, i});
    std::cout << "vector data() moved? "
              << (danger != &nodes[0] ? "yes - the pointer is now dangling"
                                      : "not this time (still UB to rely on it)")
              << "\n";
    NodeId safe = 0;                                     // DO THIS instead
    std::cout << "nodes[safe].value = " << nodes[safe].value << "\n";
}

// ============================================================
// 4. std::unique_ptr trees: the AST shape we use in Part 2
// ============================================================
// One owner per node, children owned by their parent, freed automatically when
// the root dies. Polymorphism through virtual functions.
struct Expr {
    virtual ~Expr() = default;                 // virtual dtor: deleting via
    virtual long long eval() const = 0;        // Expr* must run the right dtor
    virtual void      print(std::ostream& os) const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;         // an alias, because we type it a lot

struct IntLit : Expr {
    long long value;
    explicit IntLit(long long v) : value(v) {}
    long long eval() const override { return value; }
    void print(std::ostream& os) const override { os << value; }
};

struct Binary : Expr {
    TokenKind op;
    ExprPtr   lhs, rhs;                        // owns its children
    Binary(TokenKind o, ExprPtr l, ExprPtr r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}   // move: unique_ptr cannot be copied
    long long eval() const override {
        long long a = lhs->eval(), b = rhs->eval();
        return op == TokenKind::Plus ? a + b : a * b;
    }
    void print(std::ostream& os) const override {
        os << "(";  lhs->print(os);
        os << (op == TokenKind::Plus ? " + " : " * ");
        rhs->print(os);  os << ")";
    }
};

static void demo_unique_ptr_tree() {
    std::cout << "\n-- 4. unique_ptr tree --\n";
    // 2 + 3 * 4
    ExprPtr tree = std::make_unique<Binary>(
        TokenKind::Plus,
        std::make_unique<IntLit>(2),
        std::make_unique<Binary>(TokenKind::Star,
                                 std::make_unique<IntLit>(3),
                                 std::make_unique<IntLit>(4)));
    tree->print(std::cout);
    std::cout << " = " << tree->eval() << "\n";
    // No delete anywhere: when `tree` goes out of scope the whole tree is freed.
}

// ============================================================
// 5. std::variant: a sum type without inheritance
// ============================================================
// The same "one of N shapes" idea, but by value, with no virtual calls and no
// allocation for the small cases. We use this style for IR instructions.
struct VInt { long long value; };
struct VAdd { int lhs, rhs; };          // indices into a vector of VExpr
struct VMul { int lhs, rhs; };
using VExpr = std::variant<VInt, VAdd, VMul>;

// A visitor is a struct with one operator() per alternative. std::visit picks
// the right one. Forget an alternative and it will not compile.
struct EvalVisitor {
    const std::vector<VExpr>& pool;
    long long operator()(const VInt& n) const { return n.value; }
    long long operator()(const VAdd& n) const {
        return std::visit(*this, pool[n.lhs]) + std::visit(*this, pool[n.rhs]);
    }
    long long operator()(const VMul& n) const {
        return std::visit(*this, pool[n.lhs]) * std::visit(*this, pool[n.rhs]);
    }
};

static void demo_variant() {
    std::cout << "\n-- 5. variant --\n";
    std::vector<VExpr> pool;
    pool.push_back(VInt{2});             // 0
    pool.push_back(VInt{3});             // 1
    pool.push_back(VInt{4});             // 2
    pool.push_back(VMul{1, 2});          // 3:  3 * 4
    pool.push_back(VAdd{0, 3});          // 4:  2 + (3 * 4)
    EvalVisitor v{pool};
    std::cout << "2 + 3 * 4 = " << std::visit(v, pool.back()) << "\n";
    std::cout << "index of root alternative = " << pool.back().index() << "\n";
    std::cout << "is the root a VAdd? " << std::holds_alternative<VAdd>(pool.back()) << "\n";
}

// ============================================================
// 6. std::optional: "maybe there is an answer"
// ============================================================
// Better than a sentinel value (-1, nullptr, "") because the type says so and
// the compiler makes you check.
static std::optional<int> precedence_of(TokenKind k) {
    switch (k) {
        case TokenKind::Plus: return 10;
        case TokenKind::Star: return 20;
        default:              return std::nullopt;   // not a binary operator
    }
}

static void demo_optional() {
    std::cout << "\n-- 6. optional --\n";
    for (TokenKind k : {TokenKind::Plus, TokenKind::Star, TokenKind::LParen}) {
        if (auto p = precedence_of(k))            // converts to bool
            std::cout << kind_name(k) << " has precedence " << *p << "\n";
        else
            std::cout << kind_name(k) << " is not an operator\n";
    }
}

// ============================================================
// 7. String interning
// ============================================================
// Every identifier appears many times. Store each spelling once, hand out a
// small integer, and then name comparison is an integer comparison.
class Interner {
public:
    using Sym = std::uint32_t;
    Sym intern(std::string_view text) {
        std::string key(text);
        auto it = map_.find(key);
        if (it != map_.end()) return it->second;
        Sym id = static_cast<Sym>(strings_.size());
        strings_.push_back(key);
        map_.emplace(std::move(key), id);
        return id;
    }
    const std::string& text(Sym s) const { return strings_[s]; }
    std::size_t size() const { return strings_.size(); }
private:
    std::vector<std::string>                     strings_;
    std::unordered_map<std::string, Sym>         map_;
};

static void demo_interning() {
    std::cout << "\n-- 7. interning --\n";
    Interner in;
    Interner::Sym a = in.intern("price");
    Interner::Sym b = in.intern("total");
    Interner::Sym c = in.intern("price");
    std::cout << "price -> " << a << ", total -> " << b << ", price again -> " << c << "\n";
    std::cout << "a == c ? " << (a == c) << "   distinct strings stored: " << in.size() << "\n";
    std::cout << "text(b) = " << in.text(b) << "\n";
}

// ============================================================
// 8. RAII for scopes
// ============================================================
// Entering a block pushes a scope; leaving it must pop — on every path,
// including early `return`. A destructor does that for free.
class ScopeStack {
public:
    void        push() { depth_++; }
    void        pop()  { depth_--; }
    int         depth() const { return depth_; }
private:
    int depth_ = 0;
};

class ScopeGuard {
public:
    explicit ScopeGuard(ScopeStack& s) : s_(s) { s_.push(); }
    ~ScopeGuard() { s_.pop(); }
    ScopeGuard(const ScopeGuard&) = delete;             // not copyable:
    ScopeGuard& operator=(const ScopeGuard&) = delete;  // one push, one pop
private:
    ScopeStack& s_;
};

static void walk(ScopeStack& scopes, int level) {
    ScopeGuard guard(scopes);                  // push
    std::cout << std::string(level * 2, ' ') << "depth = " << scopes.depth() << "\n";
    if (level < 2) walk(scopes, level + 1);
    if (level == 1) return;                    // early return: still pops
}                                              // pop happens here, always

static void demo_raii() {
    std::cout << "\n-- 8. RAII scopes --\n";
    ScopeStack scopes;
    walk(scopes, 0);
    std::cout << "back at depth " << scopes.depth() << " (must be 0)\n";
    assert(scopes.depth() == 0);
}

// ============================================================
// 9. Building text with ostringstream
// ============================================================
// Dumps and error messages are built up piecewise. ostringstream is the
// standard way before C++20's std::format.
static std::string describe(const Token& t) {
    std::ostringstream os;
    os << kind_name(t.kind) << "('" << t.text << "') @" << t.offset;
    return os.str();
}

static void demo_ostringstream(const std::string& source) {
    std::cout << "\n-- 9. ostringstream --\n";
    Token t{TokenKind::Int, std::string_view(source).substr(8, 2), 8};
    std::cout << describe(t) << "\n";
}

// ============================================================
// 10. Assertions
// ============================================================
// An assert is a comment the compiler checks. Use it for invariants that must
// hold if your code is correct, never for input validation (asserts vanish
// under -DNDEBUG, and a user's bad input is not a bug in your compiler).
static long long safe_eval(const Expr* e) {
    assert(e != nullptr && "eval of a null node means the parser returned garbage");
    return e->eval();
}

static void demo_assert() {
    std::cout << "\n-- 10. assert --\n";
    IntLit five(5);
    std::cout << "safe_eval(5) = " << safe_eval(&five) << "\n";
    std::cout << "(an assert failure would abort the program with file and line)\n";
}

// ============================================================
int main() {
    const std::string source = "let x = 42 + 1;";
    std::cout << "source: " << source << "\n";
    demo_enum_class();
    demo_string_view(source);
    demo_indices();
    demo_unique_ptr_tree();
    demo_variant();
    demo_optional();
    demo_interning();
    demo_raii();
    demo_ostringstream(source);
    demo_assert();
    std::cout << "\nAll ten idioms demonstrated.\n";
    return 0;
}
