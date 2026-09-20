// pebble/source.h
// ------------------------------------------------------------
// Representation 1 of the pipeline: SOURCE TEXT.
//
//   Span        a half-open range of byte offsets: [begin, end)
//   LineCol     a 1-based line and column, for humans
//   SourceFile  one file: its name, its text, and a line map
//
// Everything else in the compiler refers to source positions with a Span, and
// only SourceFile knows how to turn a Span into "line 7, column 17".
//
// Explained in docs/05-source-text.md and docs/line-by-line/source.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pebble {

using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

// ============================================================
// Span: where something is, in bytes
// ============================================================
// Half-open, like every range in C++: begin is included, end is not.
// 4 + 4 = 8 bytes, so we can afford one in every token and every AST node.
struct Span {
    u32 begin = 0;
    u32 end   = 0;

    Span() = default;
    Span(u32 b, u32 e) : begin(b), end(e) {}

    u32  length() const { return end - begin; }
    bool empty() const { return begin == end; }
    bool contains(u32 offset) const { return offset >= begin && offset < end; }

    // The smallest span covering both. Used constantly when building AST nodes:
    // a binary expression spans from its left operand to its right one.
    Span merge(const Span& other) const {
        return Span(std::min(begin, other.begin), std::max(end, other.end));
    }

    // A span of zero length at `begin`, for "insert something here" suggestions.
    Span start() const { return Span(begin, begin); }

    bool operator==(const Span& o) const { return begin == o.begin && end == o.end; }
    bool operator!=(const Span& o) const { return !(*this == o); }
};

// ============================================================
// LineCol: where something is, for a human
// ============================================================
struct LineCol {
    u32 line = 1;    // 1-based, because every editor and every error message is
    u32 col  = 1;    // 1-based, counted in BYTES (see display_col for the other kind)
};

// ============================================================
// SourceFile
// ============================================================
// Owns the text. Every std::string_view in the compiler points into this
// buffer, so it must outlive them all, and must not be moved once lexing has
// started. Copying is deleted to make that harder to get wrong by accident.
class SourceFile {
public:
    SourceFile() = default;

    SourceFile(std::string name, std::string text)
        : name_(std::move(name)), text_(std::move(text)) {
        strip_bom();
        build_line_map();
    }

    SourceFile(const SourceFile&)            = delete;
    SourceFile& operator=(const SourceFile&) = delete;
    SourceFile(SourceFile&&)                 = default;   // safe BEFORE any view exists
    SourceFile& operator=(SourceFile&&)      = default;

    // Read a whole file into memory. Returns nothing if it cannot be opened.
    // Reading the whole file at once is deliberate: a compiler needs random
    // access to the text for error messages long after lexing.
    static std::optional<SourceFile> read(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        return SourceFile(path, std::move(text));
    }

    // Convenience for tests and the REPL: a "file" that was never on disk.
    static SourceFile from_string(std::string name, std::string text) {
        return SourceFile(std::move(name), std::move(text));
    }

    const std::string& name() const { return name_; }
    const std::string& text() const { return text_; }
    u32                size() const { return static_cast<u32>(text_.size()); }

    // ---- reading the text ----------------------------------

    // The byte at `offset`, or '\0' at (and past) the end. Returning a sentinel
    // instead of asserting means the lexer never needs a bounds check.
    char at(u32 offset) const {
        return offset < text_.size() ? text_[offset] : '\0';
    }

    std::string_view slice(const Span& s) const {
        u32 b = std::min(s.begin, size());
        u32 e = std::min(s.end, size());
        if (e < b) e = b;
        return std::string_view(text_.data() + b, e - b);
    }

    // ---- the line map --------------------------------------

    u32 line_count() const { return static_cast<u32>(line_starts_.size()); }

    // Offset -> (line, col). O(log n) by binary search over the line starts.
    // This is why we precompute the map: error messages ask for it thousands of
    // times, and scanning the text from the beginning each time would be O(n).
    LineCol line_col(u32 offset) const {
        if (line_starts_.empty()) return LineCol{1, 1};
        offset = std::min(offset, size());
        // The last line start that is <= offset.
        auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
        u32  idx = static_cast<u32>((it - line_starts_.begin()) - 1);
        return LineCol{idx + 1, offset - line_starts_[idx] + 1};
    }

    // The text of a 1-based line, without its line terminator.
    std::string_view line_text(u32 line) const {
        if (line == 0 || line > line_count()) return {};
        u32 begin = line_starts_[line - 1];
        u32 end   = (line < line_count()) ? line_starts_[line] : size();
        while (end > begin && (text_[end - 1] == '\n' || text_[end - 1] == '\r')) end--;
        return std::string_view(text_.data() + begin, end - begin);
    }

    Span line_span(u32 line) const {
        if (line == 0 || line > line_count()) return Span();
        u32 begin = line_starts_[line - 1];
        u32 end   = (line < line_count()) ? line_starts_[line] : size();
        return Span(begin, end);
    }

    // The inverse of line_col, for tools that speak in line/column (a language
    // server receives cursor positions that way - Chapter 63).
    u32 offset_of(u32 line, u32 col) const {
        if (line == 0 || line > line_count()) return size();
        u32 begin = line_starts_[line - 1];
        u32 limit = line_span(line).end;
        u32 off   = begin + (col - 1);
        return std::min(off, limit);
    }

    // Column as a human sees it on screen: tabs advance to the next multiple of
    // `tab_width`. Needed to line a caret up under the right character.
    u32 display_col(u32 offset, u32 tab_width = 4) const {
        LineCol lc  = line_col(offset);
        u32     bol = line_starts_[lc.line - 1];
        u32     col = 1;
        for (u32 i = bol; i < offset && i < size(); ++i) {
            if (text_[i] == '\t') col += tab_width - ((col - 1) % tab_width);
            else if ((static_cast<u8>(text_[i]) & 0xC0) != 0x80) col += 1;  // skip UTF-8 tails
        }
        return col;
    }

    // ---- encoding ------------------------------------------

    // We accept UTF-8 and nothing else. Returns the offset of the first
    // malformed byte, or nothing if the whole file is valid UTF-8.
    std::optional<u32> find_invalid_utf8() const {
        u32 i = 0, n = size();
        while (i < n) {
            u8 c = static_cast<u8>(text_[i]);
            u32 extra;
            if (c < 0x80)              { i++; continue; }
            else if ((c & 0xE0) == 0xC0) extra = 1;
            else if ((c & 0xF0) == 0xE0) extra = 2;
            else if ((c & 0xF8) == 0xF0) extra = 3;
            else return i;                                   // 0x80-0xBF or 0xF8+
            if (i + extra >= n) return i;                    // truncated sequence
            for (u32 k = 1; k <= extra; ++k)
                if ((static_cast<u8>(text_[i + k]) & 0xC0) != 0x80) return i + k;
            i += extra + 1;
        }
        return std::nullopt;
    }

    bool had_bom() const { return had_bom_; }

private:
    std::string      name_;
    std::string      text_;
    std::vector<u32> line_starts_;      // byte offset of the first char of each line
    bool             had_bom_ = false;

    // A UTF-8 byte-order mark is three bytes of noise at the start of files
    // written by several Windows editors. Left in place it becomes a mysterious
    // "unexpected character" on line 1, column 1.
    void strip_bom() {
        if (text_.size() >= 3 && static_cast<u8>(text_[0]) == 0xEF &&
            static_cast<u8>(text_[1]) == 0xBB && static_cast<u8>(text_[2]) == 0xBF) {
            text_.erase(0, 3);
            had_bom_ = true;
        }
    }

    // One pass over the file. Handles LF, CRLF and lone CR, so a file written on
    // any of the three platform conventions gives the same line numbers.
    void build_line_map() {
        line_starts_.clear();
        line_starts_.push_back(0);                  // line 1 starts at offset 0
        for (u32 i = 0; i < size(); ++i) {
            char c = text_[i];
            if (c == '\n') {
                line_starts_.push_back(i + 1);
            } else if (c == '\r') {
                if (i + 1 < size() && text_[i + 1] == '\n') continue;   // CRLF: wait for the \n
                line_starts_.push_back(i + 1);                          // lone CR (old Mac)
            }
        }
    }
};

}  // namespace pebble
