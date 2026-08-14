#pragma once

#include "nextviper/common.hpp"
#include "nextviper/token.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace nextviper {

class Lexer {
public:
    Lexer(std::string source, std::string file_path, DiagnosticEngine& diagnostics, bool emit_newlines = false);

    std::vector<Token> tokenize();
    Token next_token();

    bool emit_newlines() const { return emit_newlines_; }
    void set_emit_newlines(bool val) { emit_newlines_ = val; }

private:
    bool is_at_end() const;
    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);

    void skip_whitespace_and_comments();
    Token make_token(TokenType type);
    Token make_token(TokenType type, std::string text);

    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_string(char quote_char, bool is_raw = false, bool is_multiline = false);

    SourceLocation current_location() const;
    SourceSpan make_span(SourceLocation start_loc) const;

    std::string source_;
    std::string file_path_;
    DiagnosticEngine& diagnostics_;
    bool emit_newlines_ = false;

    size_t start_offset_ = 0;
    size_t current_offset_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    size_t start_line_ = 1;
    size_t start_column_ = 1;

    static const std::unordered_map<std::string_view, TokenType> keywords_;
};

} // namespace nextviper
