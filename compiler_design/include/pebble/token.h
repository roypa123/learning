// pebble/token.h
// ------------------------------------------------------------
// Representation 2 of the pipeline: TOKENS.
//
//   TokenKind   one enumerator per kind of token in Pebble
//   Token       a kind + a Span + the payload of a literal
//
// This file is pure data and lookup tables. It knows nothing about how tokens
// are produced (lexer.h) or what they mean (parser.h).
//
// Explained in docs/07-lexer-part1.md and docs/line-by-line/token.md
// ------------------------------------------------------------
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

#include "pebble/source.h"

namespace pebble {

// ============================================================
// TokenKind
// ============================================================
// The order of the groups is deliberate: the ranges are used by the
// is_xxx() predicates below, so a new keyword must be added INSIDE the keyword
// range and a new operator INSIDE the operator range.
enum class TokenKind {
    // --- end and error ---
    Eof,          // one of these always terminates the stream
    Unknown,      // a character we could not lex; the lexer reported it already

    // --- literals and names ---
    Ident,
    IntLit,
    FloatLit,
    StringLit,
    CharLit,

    // --- keywords (KEYWORD_FIRST .. KEYWORD_LAST) ---
    KwLet, KwVar, KwFn, KwReturn,
    KwIf, KwElse, KwWhile, KwLoop, KwFor, KwIn,
    KwBreak, KwContinue,
    KwStruct, KwEnum, KwType, KwImport,
    KwTrue, KwFalse, KwAs, KwNull,

    // --- grouping ---
    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,

    // --- punctuation ---
    Comma, Semi, Colon, Dot, DotDot, Arrow, FatArrow, Question,

    // --- operators (OPERATOR_FIRST .. OPERATOR_LAST) ---
    Plus, Minus, Star, Slash, Percent,
    Amp, Pipe, Caret, Tilde, Shl, Shr,
    AndAnd, OrOr, Bang,
    Lt, Gt, Le, Ge, EqEq, BangEq,

    // --- assignment ---
    Eq, PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
    AmpEq, PipeEq, CaretEq, ShlEq, ShrEq,

    // marker, never produced
    COUNT
};

// The two ranges the predicates rely on.
constexpr TokenKind KEYWORD_FIRST  = TokenKind::KwLet;
constexpr TokenKind KEYWORD_LAST   = TokenKind::KwNull;
constexpr TokenKind ASSIGN_FIRST   = TokenKind::Eq;
constexpr TokenKind ASSIGN_LAST    = TokenKind::ShrEq;

inline bool is_keyword(TokenKind k) {
    return k >= KEYWORD_FIRST && k <= KEYWORD_LAST;
}
inline bool is_assignment(TokenKind k) {
    return k >= ASSIGN_FIRST && k <= ASSIGN_LAST;
}
inline bool is_literal(TokenKind k) {
    switch (k) {
        case TokenKind::IntLit:
        case TokenKind::FloatLit:
        case TokenKind::StringLit:
        case TokenKind::CharLit:
        case TokenKind::KwTrue:
        case TokenKind::KwFalse:
        case TokenKind::KwNull:
            return true;
        default:
            return false;
    }
}

// ============================================================
// Names and spellings
// ============================================================
// token_name  - for dumps and for "expected X, found Y" messages
// spelling    - the exact characters, for operators and keywords; empty for
//               things whose text varies (identifiers, literals)
//
// Deliberately no `default:` label: adding a TokenKind without adding it here
// is a -Wswitch warning, which is how we find every place that needs updating.
inline const char* token_name(TokenKind k) {
    switch (k) {
        case TokenKind::Eof:        return "end of file";
        case TokenKind::Unknown:    return "unknown character";
        case TokenKind::Ident:      return "identifier";
        case TokenKind::IntLit:     return "integer literal";
        case TokenKind::FloatLit:   return "float literal";
        case TokenKind::StringLit:  return "string literal";
        case TokenKind::CharLit:    return "character literal";

        case TokenKind::KwLet:      return "'let'";
        case TokenKind::KwVar:      return "'var'";
        case TokenKind::KwFn:       return "'fn'";
        case TokenKind::KwReturn:   return "'return'";
        case TokenKind::KwIf:       return "'if'";
        case TokenKind::KwElse:     return "'else'";
        case TokenKind::KwWhile:    return "'while'";
        case TokenKind::KwLoop:     return "'loop'";
        case TokenKind::KwFor:      return "'for'";
        case TokenKind::KwIn:       return "'in'";
        case TokenKind::KwBreak:    return "'break'";
        case TokenKind::KwContinue: return "'continue'";
        case TokenKind::KwStruct:   return "'struct'";
        case TokenKind::KwEnum:     return "'enum'";
        case TokenKind::KwType:     return "'type'";
        case TokenKind::KwImport:   return "'import'";
        case TokenKind::KwTrue:     return "'true'";
        case TokenKind::KwFalse:    return "'false'";
        case TokenKind::KwAs:       return "'as'";
        case TokenKind::KwNull:     return "'null'";

        case TokenKind::LParen:     return "'('";
        case TokenKind::RParen:     return "')'";
        case TokenKind::LBrace:     return "'{'";
        case TokenKind::RBrace:     return "'}'";
        case TokenKind::LBracket:   return "'['";
        case TokenKind::RBracket:   return "']'";

        case TokenKind::Comma:      return "','";
        case TokenKind::Semi:       return "';'";
        case TokenKind::Colon:      return "':'";
        case TokenKind::Dot:        return "'.'";
        case TokenKind::DotDot:     return "'..'";
        case TokenKind::Arrow:      return "'->'";
        case TokenKind::FatArrow:   return "'=>'";
        case TokenKind::Question:   return "'?'";

        case TokenKind::Plus:       return "'+'";
        case TokenKind::Minus:      return "'-'";
        case TokenKind::Star:       return "'*'";
        case TokenKind::Slash:      return "'/'";
        case TokenKind::Percent:    return "'%'";
        case TokenKind::Amp:        return "'&'";
        case TokenKind::Pipe:       return "'|'";
        case TokenKind::Caret:      return "'^'";
        case TokenKind::Tilde:      return "'~'";
        case TokenKind::Shl:        return "'<<'";
        case TokenKind::Shr:        return "'>>'";
        case TokenKind::AndAnd:     return "'&&'";
        case TokenKind::OrOr:       return "'||'";
        case TokenKind::Bang:       return "'!'";
        case TokenKind::Lt:         return "'<'";
        case TokenKind::Gt:         return "'>'";
        case TokenKind::Le:         return "'<='";
        case TokenKind::Ge:         return "'>='";
        case TokenKind::EqEq:       return "'=='";
        case TokenKind::BangEq:     return "'!='";

        case TokenKind::Eq:         return "'='";
        case TokenKind::PlusEq:     return "'+='";
        case TokenKind::MinusEq:    return "'-='";
        case TokenKind::StarEq:     return "'*='";
        case TokenKind::SlashEq:    return "'/='";
        case TokenKind::PercentEq:  return "'%='";
        case TokenKind::AmpEq:      return "'&='";
        case TokenKind::PipeEq:     return "'|='";
        case TokenKind::CaretEq:    return "'^='";
        case TokenKind::ShlEq:      return "'<<='";
        case TokenKind::ShrEq:      return "'>>='";

        case TokenKind::COUNT:      return "<count>";
    }
    return "<invalid token kind>";
}

// ============================================================
// The keyword table
// ============================================================
// One place, twenty lines. Adding a keyword to Pebble means: one enumerator
// above, one name above, one row here. Nothing in the lexer changes - which is
// the payoff of "a keyword is an identifier plus a lookup".
struct KeywordEntry {
    std::string_view text;
    TokenKind        kind;
};

inline const KeywordEntry* keyword_table(std::size_t& count) {
    static const KeywordEntry table[] = {
        {"let", TokenKind::KwLet},           {"var", TokenKind::KwVar},
        {"fn", TokenKind::KwFn},             {"return", TokenKind::KwReturn},
        {"if", TokenKind::KwIf},             {"else", TokenKind::KwElse},
        {"while", TokenKind::KwWhile},       {"loop", TokenKind::KwLoop},
        {"for", TokenKind::KwFor},           {"in", TokenKind::KwIn},
        {"break", TokenKind::KwBreak},       {"continue", TokenKind::KwContinue},
        {"struct", TokenKind::KwStruct},     {"enum", TokenKind::KwEnum},
        {"type", TokenKind::KwType},         {"import", TokenKind::KwImport},
        {"true", TokenKind::KwTrue},         {"false", TokenKind::KwFalse},
        {"as", TokenKind::KwAs},             {"null", TokenKind::KwNull},
    };
    count = sizeof(table) / sizeof(table[0]);
    return table;
}

// A linear scan over twenty short strings, with a length check first, is faster
// than a hash map at this size and needs no allocation. Chapter 9 measures it
// and shows the perfect-hash alternative.
inline std::optional<TokenKind> lookup_keyword(std::string_view text) {
    std::size_t         n     = 0;
    const KeywordEntry* table = keyword_table(n);
    for (std::size_t i = 0; i < n; ++i)
        if (table[i].text.size() == text.size() && table[i].text == text)
            return table[i].kind;
    return std::nullopt;
}

// ============================================================
// Token
// ============================================================
// 32 bytes. Small enough that a vector of them for a 100k-line file is a few
// megabytes, which is why we can afford to keep the whole stream in memory
// instead of lexing on demand.
//
// The payload of a literal is stored inline for numbers, and as an INDEX into a
// side table for strings - so that Token stays trivially copyable and does not
// carry a std::string. See docs/08-lexer-part2.md for why.
struct Token {
    TokenKind        kind = TokenKind::Eof;
    Span             span;
    std::string_view text;            // exactly the characters, as they appear
    u64              int_value   = 0; // IntLit, CharLit
    double           float_value = 0; // FloatLit
    u32              value_index = 0; // StringLit: index into Lexer::string_values()
    bool             erroneous   = false;  // a diagnostic was already reported for it

    bool is(TokenKind k) const { return kind == k; }
    bool is_one_of(TokenKind a, TokenKind b) const { return kind == a || kind == b; }
};

}  // namespace pebble
