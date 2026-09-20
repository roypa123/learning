// ch07_lexer.cpp
// ------------------------------------------------------------
// Chapters 7-10: the hand-written lexer, driven from the command line.
//
// Usage:
//     run ch07_lexer                          lex a built-in snippet
//     run ch07_lexer examples\tour.peb        lex a file
//     run ch07_lexer examples\lex_errors.peb  see every lexical diagnostic
//     run ch07_lexer examples\tour.peb --comments
//     run ch07_lexer examples\tour.peb --stats
//
// Explained in:  docs/07-lexer-part1.md .. docs/10-diagnostics.md
//                docs/line-by-line/ch07_lexer.md
// ------------------------------------------------------------
#include <chrono>
#include <iostream>
#include <string>

#include "pebble/lexer.h"

using namespace pebble;

static const char* kBuiltin =
    "fn main() -> int {\n"
    "    let price = 19;          // an integer\n"
    "    let rate  = 0.085;       // a float\n"
    "    var total = price * 2;\n"
    "    if total >= 30 && price != 0 {\n"
    "        print_str(\"over budget\\n\");\n"
    "    }\n"
    "    return total;\n"
    "}\n";

int main(int argc, char** argv) {
    std::string path;
    bool        want_comments = false;
    bool        want_stats    = false;
    bool        want_colour   = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--comments")   want_comments = true;
        else if (a == "--stats") want_stats = true;
        else if (a == "--colour" || a == "--color") want_colour = true;
        else                     path = a;
    }

    // ---- 1. load the source -------------------------------------------------
    SourceFile src = SourceFile::from_string("<builtin>", kBuiltin);
    if (!path.empty()) {
        auto loaded = SourceFile::read(path);
        if (!loaded) {
            std::cerr << "error: cannot open " << path << "\n";
            return 1;
        }
        src = std::move(*loaded);       // safe: no string_view into it exists yet
    }

    std::cout << "=== SOURCE (" << src.name() << ", " << src.size() << " bytes, "
              << src.line_count() << " lines) ===\n";
    for (u32 line = 1; line <= src.line_count() && line <= 40; ++line)
        std::cout << (line < 10 ? " " : "") << line << " | " << src.line_text(line) << "\n";
    if (src.line_count() > 40) std::cout << "  ... (truncated)\n";

    // ---- 2. lex it ----------------------------------------------------------
    Diagnostics diag(&src);
    diag.set_colour(want_colour);

    Lexer lexer(src, diag);
    lexer.keep_comments = want_comments;

    auto               t0     = std::chrono::steady_clock::now();
    std::vector<Token> tokens = lexer.tokenize();
    auto               t1     = std::chrono::steady_clock::now();

    // ---- 3. show the tokens -------------------------------------------------
    std::cout << "\n=== TOKENS (" << tokens.size() << ") ===\n";
    dump_tokens(tokens, src, lexer.string_values(), std::cout);

    // ---- 4. show the diagnostics -------------------------------------------
    if (!diag.items().empty()) {
        std::cout << "\n=== DIAGNOSTICS ===\n";
        diag.render(std::cout);
    } else {
        std::cout << "\nno diagnostics\n";
    }

    // ---- 5. statistics ------------------------------------------------------
    if (want_stats) {
        std::cout << "\n=== STATISTICS ===\n";
        u32 counts[static_cast<int>(TokenKind::COUNT)] = {};
        for (const Token& t : tokens) counts[static_cast<int>(t.kind)]++;
        for (int k = 0; k < static_cast<int>(TokenKind::COUNT); ++k) {
            if (counts[k] == 0) continue;
            std::cout << "  " << token_name(static_cast<TokenKind>(k)) << ": "
                      << counts[k] << "\n";
        }
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        std::cout << "  bytes         : " << src.size() << "\n";
        std::cout << "  tokens        : " << tokens.size() << "\n";
        std::cout << "  bytes/token   : "
                  << (tokens.size() ? (double)src.size() / (double)tokens.size() : 0.0) << "\n";
        std::cout << "  time          : " << us << " us\n";
        if (us > 0)
            std::cout << "  throughput    : " << (src.size() / us) << " MB/s\n";
    }

    return diag.has_errors() ? 1 : 0;
}
