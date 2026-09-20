// ch05_source.cpp
// ------------------------------------------------------------
// Chapter 5: source text, spans and the line map.
//
// Exercises include/pebble/source.h:
//   1. loading text, stripping a BOM, counting lines (LF, CRLF, lone CR)
//   2. Span: slice, merge, contains
//   3. offset -> (line, col) and back again
//   4. drawing a caret under a span, with tabs handled
//   5. UTF-8 validation
//
// Build & run:   run ch05_source
//                run ch05_source examples\hello.peb
//
// Explained in:  docs/05-source-text.md
//                docs/line-by-line/ch05_source.md
// ------------------------------------------------------------
#include <iomanip>
#include <iostream>
#include <string>

#include "pebble/source.h"

using namespace pebble;

// ------------------------------------------------------------
// Render one line of source with a caret under a span. This is the seed of the
// real diagnostic renderer in Chapter 10 - about 20 lines, and already the
// single most useful thing a compiler can print.
// ------------------------------------------------------------
static void show_span(const SourceFile& src, Span span, const std::string& message) {
    LineCol lc = src.line_col(span.begin);

    // The gutter is as wide as the biggest line number we might print.
    std::string number = std::to_string(lc.line);
    std::string gutter(number.size(), ' ');

    std::cout << gutter << "--> " << src.name() << ":" << lc.line << ":" << lc.col << "\n";
    std::cout << gutter << " |\n";

    // Print the line, but expand tabs to spaces so the caret can line up.
    std::string_view text = src.line_text(lc.line);
    std::string      shown;
    for (char c : text) {
        if (c == '\t') shown.append(4 - (shown.size() % 4), ' ');
        else           shown.push_back(c);
    }
    std::cout << number << " | " << shown << "\n";

    // The caret: display_col accounts for tabs and for multi-byte UTF-8.
    u32 col   = src.display_col(span.begin);
    u32 width = std::max<u32>(1, src.display_col(span.end) - col);
    std::cout << gutter << " | " << std::string(col - 1, ' ')
              << std::string(width, '^') << " " << message << "\n";
}

static void section(const char* title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main(int argc, char** argv) {
    // ------------------------------------------------------------
    // If given a path, just report on that file and stop. Otherwise use a
    // built-in sample that deliberately contains every awkward case.
    // ------------------------------------------------------------
    if (argc > 1) {
        auto loaded = SourceFile::read(argv[1]);
        if (!loaded) {
            std::cerr << "error: cannot open " << argv[1] << "\n";
            return 1;
        }
        const SourceFile& f = *loaded;
        std::cout << "file   : " << f.name() << "\n";
        std::cout << "bytes  : " << f.size() << "\n";
        std::cout << "lines  : " << f.line_count() << "\n";
        std::cout << "bom    : " << (f.had_bom() ? "yes (stripped)" : "no") << "\n";
        if (auto bad = f.find_invalid_utf8())
            std::cout << "utf-8  : INVALID at byte " << *bad << "\n";
        else
            std::cout << "utf-8  : valid\n";
        for (u32 line = 1; line <= f.line_count() && line <= 10; ++line)
            std::cout << std::setw(4) << line << " | " << f.line_text(line) << "\n";
        if (f.line_count() > 10) std::cout << "  ... (" << f.line_count() - 10 << " more)\n";
        return 0;
    }

    // A sample with: a CRLF line ending, a tab-indented line, a UTF-8 comment,
    // and no trailing newline on the last line.
    std::string text =
        "fn main() -> int {\r\n"          // CRLF, as Windows editors write
        "\tlet price = 19;\n"             // a TAB, not spaces
        "    let total = price * 2;\n"
        "    // prix en euros \xE2\x82\xAC\n"   // UTF-8: the euro sign, 3 bytes
        "    return total;\n"
        "}";                              // no newline at the end of the file

    SourceFile src = SourceFile::from_string("sample.peb", text);

    section("1. THE FILE");
    std::cout << "name       : " << src.name() << "\n";
    std::cout << "bytes      : " << src.size() << "\n";
    std::cout << "lines      : " << src.line_count() << "\n";
    std::cout << "had bom    : " << (src.had_bom() ? "yes" : "no") << "\n";
    std::cout << "valid utf-8: " << (src.find_invalid_utf8() ? "no" : "yes") << "\n";

    section("2. THE LINE MAP");
    std::cout << "line  start  bytes  text\n";
    for (u32 line = 1; line <= src.line_count(); ++line) {
        Span s = src.line_span(line);
        std::cout << std::setw(4) << line << std::setw(7) << s.begin
                  << std::setw(7) << s.length() << "  '" << src.line_text(line) << "'\n";
    }
    std::cout << "(line 1 is 20 bytes because CRLF is two of them, and the map\n"
                 " counts CRLF as ONE line ending)\n";

    section("3. OFFSET -> LINE:COL, AND BACK");
    for (u32 offset : {0u, 5u, 20u, 21u, 25u, src.size() - 1, src.size()}) {
        LineCol lc = src.line_col(offset);
        u32     rt = src.offset_of(lc.line, lc.col);
        std::cout << "offset " << std::setw(3) << offset
                  << " -> line " << lc.line << ", col " << std::setw(2) << lc.col
                  << "   (display col " << src.display_col(offset) << ")"
                  << "   round trip -> " << rt << (rt == offset ? "  ok" : "  CLAMPED")
                  << "\n";
    }

    section("4. SPANS");
    // Find "price" on line 2 and "price" again on line 3, by hand for now.
    u32  p1     = static_cast<u32>(text.find("price"));
    u32  p2     = static_cast<u32>(text.find("price", p1 + 1));
    Span first  = Span(p1, p1 + 5);
    Span second = Span(p2, p2 + 5);
    std::cout << "first  = [" << first.begin << "," << first.end << ") = '"
              << src.slice(first) << "'\n";
    std::cout << "second = [" << second.begin << "," << second.end << ") = '"
              << src.slice(second) << "'\n";
    Span both = first.merge(second);
    std::cout << "merged = [" << both.begin << "," << both.end << ") spans "
              << src.line_col(both.begin).line << " to "
              << src.line_col(both.end).line << ", " << both.length() << " bytes\n";
    std::cout << "does the merged span contain offset " << p1 + 2 << "? "
              << (both.contains(p1 + 2) ? "yes" : "no") << "\n";

    section("5. A CARET UNDER A SPAN");
    show_span(src, first, "declared here (note: the line is TAB-indented)");
    std::cout << "\n";
    u32 total_pos = static_cast<u32>(text.find("total"));
    show_span(src, Span(total_pos, total_pos + 5), "and here");
    std::cout << "\n";
    u32 euro = static_cast<u32>(text.find("\xE2\x82\xAC"));
    show_span(src, Span(euro, euro + 3), "3 bytes, but ONE column wide");

    section("6. UTF-8 VALIDATION");
    std::cout << "the sample            : "
              << (src.find_invalid_utf8() ? "invalid" : "valid") << "\n";
    std::string broken = text;
    broken[euro + 1] = 'X';                      // smash a continuation byte
    SourceFile bad = SourceFile::from_string("broken.peb", broken);
    if (auto where = bad.find_invalid_utf8()) {
        LineCol lc = bad.line_col(*where);
        std::cout << "with one byte smashed : invalid at byte " << *where
                  << " (line " << lc.line << ", col " << lc.col << ")\n";
    }
    std::string truncated = text.substr(0, euro + 2);   // cut a sequence in half
    SourceFile cut = SourceFile::from_string("cut.peb", truncated);
    if (auto where = cut.find_invalid_utf8())
        std::cout << "truncated at the end  : invalid at byte " << *where << "\n";

    section("7. THE BOM");
    std::string with_bom = "\xEF\xBB\xBFfn main() -> int { return 0; }";
    SourceFile  bom = SourceFile::from_string("bom.peb", with_bom);
    std::cout << "had_bom     : " << (bom.had_bom() ? "yes" : "no") << "\n";
    std::cout << "first byte  : '" << bom.at(0) << "' (would be 0xEF if we had not stripped it)\n";
    std::cout << "line 1      : '" << bom.line_text(1) << "'\n";

    std::cout << "\nAll source-text machinery exercised.\n";
    return 0;
}
