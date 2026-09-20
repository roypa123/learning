// pebble/diag.h
// ------------------------------------------------------------
// DIAGNOSTICS: the one channel through which every stage reports problems.
//
//   Severity    error / warning / note / help
//   Label       a secondary span with its own message
//   Suggestion  "replace this span with this text" - a machine-applicable fix
//   Diagnostic  one complete report
//   Diagnostics the collector, and the renderer
//
// Errors are NOT exceptions and NOT return codes. They are appended to a list,
// so one run can report many, and so a later stage can decide whether the
// earlier ones were bad enough to stop for.
//
// Explained in docs/10-diagnostics.md and docs/line-by-line/diag.md
// ------------------------------------------------------------
#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "pebble/source.h"

namespace pebble {

enum class Severity { Error, Warning, Note, Help };

inline const char* severity_name(Severity s) {
    switch (s) {
        case Severity::Error:   return "error";
        case Severity::Warning: return "warning";
        case Severity::Note:    return "note";
        case Severity::Help:    return "help";
    }
    return "?";
}

// ANSI colour codes. Disabled by default because a redirected stream, a CI log
// and older Windows consoles all mangle them.
inline const char* severity_colour(Severity s) {
    switch (s) {
        case Severity::Error:   return "\x1b[1;31m";   // bold red
        case Severity::Warning: return "\x1b[1;33m";   // bold yellow
        case Severity::Note:    return "\x1b[1;36m";   // bold cyan
        case Severity::Help:    return "\x1b[1;32m";   // bold green
    }
    return "";
}

struct Label {
    Span        span;
    std::string message;
};

struct Suggestion {
    Span        span;          // the text to replace (may be empty = insert here)
    std::string replacement;
    std::string message;
};

struct Diagnostic {
    Severity                severity = Severity::Error;
    std::string             message;
    Span                    span;
    std::string             code;          // e.g. "E0102", for documentation links
    std::vector<Label>      labels;        // extra places worth pointing at
    std::vector<Suggestion> suggestions;
};

class Diagnostics;

// ------------------------------------------------------------
// A builder, so a call site reads like a sentence:
//
//   diag.error(span, "cannot multiply 'float' by 'int'")
//       .label(lhs_span, "this is float")
//       .label(rhs_span, "this is int")
//       .help(rhs_span, "2.0", "write the literal as a float");
//
// It stores an INDEX, not a pointer, because the vector it points into can
// reallocate while the builder is alive (see docs/02, idiom 3).
// ------------------------------------------------------------
class DiagBuilder {
public:
    DiagBuilder(Diagnostics* owner, std::size_t index) : owner_(owner), index_(index) {}

    DiagBuilder& label(Span span, std::string message);
    DiagBuilder& note(std::string message);
    DiagBuilder& help(Span span, std::string replacement, std::string message);
    DiagBuilder& code(std::string c);

private:
    Diagnostics* owner_;
    std::size_t  index_;
};

// ------------------------------------------------------------
// Diagnostics
// ------------------------------------------------------------
class Diagnostics {
public:
    explicit Diagnostics(const SourceFile* src = nullptr) : src_(src) {}

    void set_source(const SourceFile* src) { src_ = src; }
    void set_colour(bool on) { colour_ = on; }
    void set_max_errors(u32 n) { max_errors_ = n; }

    DiagBuilder error(Span span, std::string message) {
        return add(Severity::Error, span, std::move(message));
    }
    DiagBuilder warning(Span span, std::string message) {
        return add(Severity::Warning, span, std::move(message));
    }

    bool has_errors() const { return errors_ > 0; }
    u32  error_count() const { return errors_; }
    u32  warning_count() const { return warnings_; }
    bool too_many_errors() const { return errors_ >= max_errors_; }

    const std::vector<Diagnostic>& items() const { return items_; }
    std::vector<Diagnostic>&       items() { return items_; }

    // Print everything, in source order. Sorting matters: stages do not run in
    // source order (the type checker may report line 3 after line 40), and a
    // user reads top to bottom.
    void render(std::ostream& os) {
        std::stable_sort(items_.begin(), items_.end(),
                         [](const Diagnostic& a, const Diagnostic& b) {
                             return a.span.begin < b.span.begin;
                         });
        for (const Diagnostic& d : items_) render_one(d, os);
        if (errors_ || warnings_) {
            os << summary() << "\n";
        }
    }

    std::string summary() const {
        std::string s;
        if (errors_)   s += std::to_string(errors_) + (errors_ == 1 ? " error" : " errors");
        if (errors_ && warnings_) s += ", ";
        if (warnings_) s += std::to_string(warnings_) +
                            (warnings_ == 1 ? " warning" : " warnings");
        if (s.empty()) s = "no diagnostics";
        else           s += " generated";
        return s;
    }

    void clear() { items_.clear(); errors_ = warnings_ = 0; }

private:
    friend class DiagBuilder;

    const SourceFile*       src_ = nullptr;
    std::vector<Diagnostic> items_;
    u32                     errors_     = 0;
    u32                     warnings_   = 0;
    u32                     max_errors_ = 20;
    bool                    colour_     = false;

    DiagBuilder add(Severity sev, Span span, std::string message) {
        Diagnostic d;
        d.severity = sev;
        d.span     = span;
        d.message  = std::move(message);
        items_.push_back(std::move(d));
        if (sev == Severity::Error)   errors_++;
        if (sev == Severity::Warning) warnings_++;
        return DiagBuilder(this, items_.size() - 1);
    }

    const char* col(Severity s) const { return colour_ ? severity_colour(s) : ""; }
    const char* off() const { return colour_ ? "\x1b[0m" : ""; }
    const char* bold() const { return colour_ ? "\x1b[1m" : ""; }

    // Expand tabs so a caret can be lined up, whatever the reader's tab width.
    static std::string expand_tabs(std::string_view line, u32 tab_width = 4) {
        std::string out;
        for (char c : line) {
            if (c == '\t') out.append(tab_width - (out.size() % tab_width), ' ');
            else           out.push_back(c);
        }
        return out;
    }

    void render_one(const Diagnostic& d, std::ostream& os) const {
        if (!src_) {                                   // no file: message only
            os << severity_name(d.severity) << ": " << d.message << "\n";
            return;
        }
        LineCol     lc     = src_->line_col(d.span.begin);
        std::string number = std::to_string(lc.line);
        std::string gutter(number.size() + 1, ' ');

        // header:   error[E0102]: cannot multiply 'float' by 'int'
        os << col(d.severity) << severity_name(d.severity);
        if (!d.code.empty()) os << "[" << d.code << "]";
        os << off() << bold() << ": " << d.message << off() << "\n";

        // location: --> file:line:col
        os << gutter << "--> " << src_->name() << ":" << lc.line << ":" << lc.col << "\n";

        render_snippet(d.span, d.severity, lc.line, gutter, os, primary_label(d));

        for (const Label& l : d.labels) {
            if (l.span == d.span) continue;              // already drawn under the caret
            LineCol     llc  = src_->line_col(l.span.begin);
            std::string lnum = std::to_string(llc.line);
            if (llc.line != lc.line) os << gutter << "...\n";
            render_snippet(l.span, Severity::Note, llc.line,
                           std::string(lnum.size() + 1, ' '), os, l.message);
        }

        for (const Suggestion& s : d.suggestions) {
            os << col(Severity::Help) << "help" << off() << ": " << s.message << "\n";
            LineCol     slc  = src_->line_col(s.span.begin);
            std::string snum = std::to_string(slc.line);
            std::string sg(snum.size() + 1, ' ');
            std::string line = expand_tabs(src_->line_text(slc.line));
            u32         from = src_->display_col(s.span.begin) - 1;
            u32         to   = src_->display_col(s.span.end) - 1;
            std::string fixed = line.substr(0, from) + s.replacement +
                                (to <= line.size() ? line.substr(to) : std::string());
            os << sg << "|\n";
            os << snum << " | " << fixed << "\n";
            os << sg << "| " << std::string(from, ' ')
               << std::string(std::max<std::size_t>(1, s.replacement.size()), '~') << "\n";
        }
        os << "\n";
    }

    static std::string primary_label(const Diagnostic& d) {
        // The caret usually needs no repeated text; a label under the caret is
        // used only when the diagnostic supplied one for the primary span.
        for (const Label& l : d.labels)
            if (l.span == d.span) return l.message;
        return std::string();
    }

    void render_snippet(Span span, Severity sev, u32 line, const std::string& gutter,
                        std::ostream& os, const std::string& caret_message) const {
        std::string number = std::to_string(line);
        std::string shown  = expand_tabs(src_->line_text(line));
        u32         from   = src_->display_col(span.begin);
        u32         to     = src_->display_col(span.end);
        u32         width  = std::max<u32>(1, to > from ? to - from : 1);

        os << gutter << "|\n";
        os << number << " | " << shown << "\n";
        os << gutter << "| " << std::string(from - 1, ' ')
           << col(sev) << std::string(width, sev == Severity::Note ? '-' : '^');
        if (!caret_message.empty()) os << " " << caret_message;
        os << off() << "\n";
    }
};

// ---- DiagBuilder methods, now that Diagnostics is complete ----
inline DiagBuilder& DiagBuilder::label(Span span, std::string message) {
    owner_->items_[index_].labels.push_back(Label{span, std::move(message)});
    return *this;
}
inline DiagBuilder& DiagBuilder::note(std::string message) {
    owner_->items_[index_].labels.push_back(
        Label{owner_->items_[index_].span, std::move(message)});
    return *this;
}
inline DiagBuilder& DiagBuilder::help(Span span, std::string replacement,
                                      std::string message) {
    owner_->items_[index_].suggestions.push_back(
        Suggestion{span, std::move(replacement), std::move(message)});
    return *this;
}
inline DiagBuilder& DiagBuilder::code(std::string c) {
    owner_->items_[index_].code = std::move(c);
    return *this;
}

}  // namespace pebble
