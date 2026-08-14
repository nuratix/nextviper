#pragma once

#include "nextviper/common.hpp"
#include <string>
#include <string_view>

namespace nextviper {

enum class TokenType {
    // End of file / Invalid
    EOF_TOKEN,
    INVALID,

    // Literals
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    IDENTIFIER,

    // Keywords
    KEYWORD_LET,
    KEYWORD_MUT,
    KEYWORD_FN,
    KEYWORD_RETURN,
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_FOR,
    KEYWORD_IN,
    KEYWORD_LOOP,
    KEYWORD_BREAK,
    KEYWORD_CONTINUE,
    KEYWORD_TRUE,
    KEYWORD_FALSE,
    KEYWORD_NIL,
    KEYWORD_MATCH,
    KEYWORD_STRUCT,
    KEYWORD_TYPE,
    KEYWORD_IMPORT,
    KEYWORD_AND,
    KEYWORD_OR,
    KEYWORD_NOT,

    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    POWER,          // ** or ^
    
    ASSIGN,         // =
    PLUS_ASSIGN,    // +=
    MINUS_ASSIGN,   // -=
    STAR_ASSIGN,    // *=
    SLASH_ASSIGN,   // /=
    PERCENT_ASSIGN, // %=

    EQUAL_EQUAL,    // ==
    BANG_EQUAL,     // !=
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL,  // >=

    AMP_AMP,        // &&
    PIPE_PIPE,      // ||
    BANG,           // !

    PIPE_GREATER,   // |> (Pipeline Operator)
    FAT_ARROW,      // => (Arrow function / match arm)
    ARROW,          // -> (Return type / lambda)
    DOT_DOT,        // .. (Range)
    DOT_DOT_EQUAL,  // ..= (Inclusive range)

    // Punctuation
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    COMMA,          // ,
    COLON,          // :
    SEMICOLON,      // ;
    DOT,            // .
    QUESTION        // ?
};

struct Token {
    TokenType type = TokenType::INVALID;
    std::string text;
    SourceSpan span;
    int64_t int_value = 0;
    double float_value = 0.0;
    std::string string_value;

    Token() = default;
    Token(TokenType type, std::string text, SourceSpan span)
        : type(type), text(std::move(text)), span(span) {}

    std::string_view type_name() const;
    std::string to_string() const;
};

std::string_view token_type_to_string(TokenType type);

inline std::ostream& operator<<(std::ostream& os, TokenType type) {
    return os << token_type_to_string(type);
}

} // namespace nextviper
