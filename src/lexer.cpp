#include "nextviper/lexer.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace nextviper {

static void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

const std::unordered_map<std::string_view, TokenType> Lexer::keywords_ = {
    {"let", TokenType::KEYWORD_LET},
    {"mut", TokenType::KEYWORD_MUT},
    {"fn", TokenType::KEYWORD_FN},
    {"return", TokenType::KEYWORD_RETURN},
    {"if", TokenType::KEYWORD_IF},
    {"else", TokenType::KEYWORD_ELSE},
    {"while", TokenType::KEYWORD_WHILE},
    {"for", TokenType::KEYWORD_FOR},
    {"in", TokenType::KEYWORD_IN},
    {"loop", TokenType::KEYWORD_LOOP},
    {"break", TokenType::KEYWORD_BREAK},
    {"continue", TokenType::KEYWORD_CONTINUE},
    {"true", TokenType::KEYWORD_TRUE},
    {"false", TokenType::KEYWORD_FALSE},
    {"null", TokenType::KEYWORD_NULL},
    {"nil", TokenType::KEYWORD_NIL},
    {"match", TokenType::KEYWORD_MATCH},
    {"struct", TokenType::KEYWORD_STRUCT},
    {"type", TokenType::KEYWORD_TYPE},
    {"import", TokenType::KEYWORD_IMPORT},
    {"from", TokenType::KEYWORD_FROM},
    {"as", TokenType::KEYWORD_AS},
    {"export", TokenType::KEYWORD_EXPORT},
    {"and", TokenType::KEYWORD_AND},
    {"or", TokenType::KEYWORD_OR},
    {"not", TokenType::KEYWORD_NOT},
};

Lexer::Lexer(std::string source, std::string file_path, DiagnosticEngine& diagnostics, bool emit_newlines)
    : source_(std::move(source)), file_path_(std::move(file_path)), diagnostics_(diagnostics), emit_newlines_(emit_newlines) {}

bool Lexer::is_at_end() const {
    return current_offset_ >= source_.size();
}

char Lexer::advance() {
    if (is_at_end()) return '\0';
    char c = source_[current_offset_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_offset_];
}

char Lexer::peek_next() const {
    if (current_offset_ + 1 >= source_.size()) return '\0';
    return source_[current_offset_ + 1];
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_offset_] != expected) {
        return false;
    }
    advance();
    return true;
}

SourceLocation Lexer::current_location() const {
    return SourceLocation{line_, column_, current_offset_};
}

SourceSpan Lexer::make_span(SourceLocation start_loc) const {
    return SourceSpan(start_loc, current_location(), file_path_);
}

Token Lexer::make_token(TokenType type) {
    std::string text = source_.substr(start_offset_, current_offset_ - start_offset_);
    return Token(type, text, make_span(SourceLocation{start_line_, start_column_, start_offset_}));
}

Token Lexer::make_token(TokenType type, std::string text) {
    return Token(type, std::move(text), make_span(SourceLocation{start_line_, start_column_, start_offset_}));
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                if (emit_newlines_) return;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // Single line comment (// or ///)
                    while (!is_at_end() && peek() != '\n') {
                        advance();
                    }
                } else if (peek_next() == '*') {
                    // Multi-line nested comment
                    SourceLocation start_loc = current_location();
                    advance(); // skip '/'
                    advance(); // skip '*'
                    int depth = 1;
                    while (!is_at_end() && depth > 0) {
                        if (peek() == '/' && peek_next() == '*') {
                            advance();
                            advance();
                            depth++;
                        } else if (peek() == '*' && peek_next() == '/') {
                            advance();
                            advance();
                            depth--;
                        } else {
                            advance();
                        }
                    }
                    if (depth > 0) {
                        diagnostics_.error("unterminated block comment", make_span(start_loc), "did you forget '*/'?");
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

Token Lexer::scan_identifier_or_keyword() {
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string text = source_.substr(start_offset_, current_offset_ - start_offset_);

    // Check for raw string prefix: r"..." or r'...'
    if (text == "r" && (peek() == '"' || peek() == '\'')) {
        char quote = advance();
        return scan_string(quote, /*is_raw=*/true, /*is_multiline=*/false);
    }

    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        Token token = make_token(it->second, text);
        if (it->second == TokenType::KEYWORD_TRUE) {
            token.bool_value = true;
            token.int_value = 1;
        } else if (it->second == TokenType::KEYWORD_FALSE) {
            token.bool_value = false;
            token.int_value = 0;
        }
        return token;
    }

    return make_token(TokenType::IDENTIFIER, text);
}

Token Lexer::scan_number() {
    bool is_float = false;
    SourceLocation start_loc{start_line_, start_column_, start_offset_};

    // 1. Hexadecimal: 0x... or 0X...
    if (source_[start_offset_] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); // consume 'x' / 'X'
        size_t digit_count = 0;
        while (std::isxdigit(peek()) || peek() == '_') {
            if (peek() != '_') digit_count++;
            advance();
        }

        if (digit_count == 0) {
            diagnostics_.error("expected at least one hexadecimal digit after '0x'", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }

        // Check for invalid letters following hex
        if (std::isalpha(peek())) {
            diagnostics_.error(std::string("invalid digit '") + peek() + "' in hexadecimal literal", make_span(current_location()));
            while (std::isalnum(peek()) || peek() == '_') advance();
            return make_token(TokenType::INVALID);
        }

        std::string raw = source_.substr(start_offset_ + 2, current_offset_ - (start_offset_ + 2));
        std::string clean;
        for (char c : raw) { if (c != '_') clean.push_back(c); }
        int64_t val = 0;
        try {
            val = std::stoll(clean, nullptr, 16);
        } catch (...) {
            diagnostics_.error("hexadecimal literal overflow", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }
        Token tok = make_token(TokenType::INT_LITERAL);
        tok.int_value = val;
        return tok;
    }

    // 2. Binary: 0b... or 0B...
    if (source_[start_offset_] == '0' && (peek() == 'b' || peek() == 'B')) {
        advance(); // consume 'b' / 'B'
        size_t digit_count = 0;
        while (peek() == '0' || peek() == '1' || peek() == '_') {
            if (peek() != '_') digit_count++;
            advance();
        }

        if (digit_count == 0) {
            diagnostics_.error("expected at least one binary digit (0 or 1) after '0b'", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }

        if (std::isdigit(peek()) || std::isalpha(peek())) {
            diagnostics_.error(std::string("invalid digit '") + peek() + "' in binary literal", make_span(current_location()));
            while (std::isalnum(peek()) || peek() == '_') advance();
            return make_token(TokenType::INVALID);
        }

        std::string raw = source_.substr(start_offset_ + 2, current_offset_ - (start_offset_ + 2));
        std::string clean;
        for (char c : raw) { if (c != '_') clean.push_back(c); }
        int64_t val = 0;
        try {
            val = std::stoll(clean, nullptr, 2);
        } catch (...) {
            diagnostics_.error("binary literal overflow", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }
        Token tok = make_token(TokenType::INT_LITERAL);
        tok.int_value = val;
        return tok;
    }

    // 3. Octal: 0o... or 0O...
    if (source_[start_offset_] == '0' && (peek() == 'o' || peek() == 'O')) {
        advance(); // consume 'o' / 'O'
        size_t digit_count = 0;
        while ((peek() >= '0' && peek() <= '7') || peek() == '_') {
            if (peek() != '_') digit_count++;
            advance();
        }

        if (digit_count == 0) {
            diagnostics_.error("expected at least one octal digit (0-7) after '0o'", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }

        if (std::isdigit(peek()) || std::isalpha(peek())) {
            diagnostics_.error(std::string("invalid digit '") + peek() + "' in octal literal", make_span(current_location()));
            while (std::isalnum(peek()) || peek() == '_') advance();
            return make_token(TokenType::INVALID);
        }

        std::string raw = source_.substr(start_offset_ + 2, current_offset_ - (start_offset_ + 2));
        std::string clean;
        for (char c : raw) { if (c != '_') clean.push_back(c); }
        int64_t val = 0;
        try {
            val = std::stoll(clean, nullptr, 8);
        } catch (...) {
            diagnostics_.error("octal literal overflow", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }
        Token tok = make_token(TokenType::INT_LITERAL);
        tok.int_value = val;
        return tok;
    }

    // 4. Standard Decimal digits
    while (std::isdigit(peek()) || peek() == '_') {
        advance();
    }

    // Fractional part: .123 (ensuring not range operator `..`)
    if (peek() == '.' && std::isdigit(peek_next())) {
        is_float = true;
        advance(); // consume '.'
        while (std::isdigit(peek()) || peek() == '_') {
            advance();
        }
    }

    // Scientific notation: 1e5, 1.2e-3
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance(); // consume 'e' / 'E'
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        if (!std::isdigit(peek())) {
            diagnostics_.error("expected exponent digits in scientific notation", make_span(start_loc));
            return make_token(TokenType::INVALID);
        }
        while (std::isdigit(peek()) || peek() == '_') {
            advance();
        }
    }

    std::string text = source_.substr(start_offset_, current_offset_ - start_offset_);
    std::string clean;
    for (char c : text) { if (c != '_') clean.push_back(c); }

    if (is_float) {
        Token tok = make_token(TokenType::FLOAT_LITERAL, text);
        try {
            tok.float_value = std::stod(clean);
        } catch (...) {
            diagnostics_.error("floating point literal overflow", tok.span);
            return make_token(TokenType::INVALID);
        }
        return tok;
    } else {
        Token tok = make_token(TokenType::INT_LITERAL, text);
        try {
            tok.int_value = std::stoll(clean);
        } catch (...) {
            diagnostics_.error("integer literal overflow", tok.span);
            return make_token(TokenType::INVALID);
        }
        return tok;
    }
}

Token Lexer::scan_string(char quote_char, bool is_raw, bool is_multiline) {
    std::string value;
    SourceLocation start_loc{start_line_, start_column_, start_offset_};

    while (!is_at_end()) {
        if (is_multiline) {
            if (peek() == quote_char && peek_next() == quote_char &&
                current_offset_ + 2 < source_.size() && source_[current_offset_ + 2] == quote_char) {
                advance(); advance(); advance(); // Consume triple quotes
                Token tok = make_token(TokenType::STRING_LITERAL);
                tok.string_value = std::move(value);
                return tok;
            }
        } else {
            if (peek() == quote_char) {
                advance(); // Consume closing quote
                Token tok = make_token(TokenType::STRING_LITERAL);
                tok.string_value = std::move(value);
                return tok;
            }
            if (peek() == '\n') {
                diagnostics_.error("unescaped newline in string literal", make_span(current_location()),
                                   "use '\\n' or triple quotes (\"\"\"...\"\"\") for multiline strings");
                return make_token(TokenType::INVALID);
            }
        }

        char c = advance();

        if (c == '\\' && !is_raw) {
            if (is_at_end()) break;
            char esc = advance();
            switch (esc) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '\\': value.push_back('\\'); break;
                case '"': value.push_back('"'); break;
                case '\'': value.push_back('\''); break;
                case '0': value.push_back('\0'); break;
                case 'a': value.push_back('\a'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'v': value.push_back('\v'); break;

                // Hex escape: \xHH
                case 'x': {
                    char h1 = peek();
                    char h2 = peek_next();
                    if (std::isxdigit(h1) && std::isxdigit(h2)) {
                        advance(); advance();
                        std::string hex_str{h1, h2};
                        int val = std::stoi(hex_str, nullptr, 16);
                        value.push_back(static_cast<char>(val));
                    } else {
                        diagnostics_.error("invalid hex escape sequence, expected 2 hex digits (e.g. \\x41)",
                                           make_span(current_location()));
                        return make_token(TokenType::INVALID);
                    }
                    break;
                }

                // Unicode escape: \u{HHHH} or \uHHHH
                case 'u': {
                    if (peek() == '{') {
                        advance(); // consume '{'
                        std::string hex_str;
                        while (!is_at_end() && peek() != '}') {
                            if (!std::isxdigit(peek())) {
                                diagnostics_.error("invalid hex digit in unicode escape", make_span(current_location()));
                                return make_token(TokenType::INVALID);
                            }
                            hex_str.push_back(advance());
                        }
                        if (is_at_end() || peek() != '}') {
                            diagnostics_.error("unterminated unicode escape sequence, expected '}'", make_span(current_location()));
                            return make_token(TokenType::INVALID);
                        }
                        advance(); // consume '}'
                        if (hex_str.empty() || hex_str.size() > 6) {
                            diagnostics_.error("unicode escape must contain 1 to 6 hex digits", make_span(current_location()));
                            return make_token(TokenType::INVALID);
                        }
                        uint32_t cp = static_cast<uint32_t>(std::stoul(hex_str, nullptr, 16));
                        append_utf8(value, cp);
                    } else {
                        // \uHHHH (4 hex digits)
                        std::string hex_str;
                        for (int i = 0; i < 4; ++i) {
                            if (!std::isxdigit(peek())) {
                                diagnostics_.error("expected 4 hex digits in \\u escape (or use \\u{...})",
                                                   make_span(current_location()));
                                return make_token(TokenType::INVALID);
                            }
                            hex_str.push_back(advance());
                        }
                        uint32_t cp = static_cast<uint32_t>(std::stoul(hex_str, nullptr, 16));
                        append_utf8(value, cp);
                    }
                    break;
                }

                default:
                    diagnostics_.warning(std::string("unknown escape sequence '\\") + esc + "'",
                                        make_span(SourceLocation{line_, column_ - 2, current_offset_ - 2}));
                    value.push_back(esc);
                    break;
            }
        } else {
            value.push_back(c);
        }
    }

    diagnostics_.error("unterminated string literal", make_span(start_loc),
                       is_multiline ? "did you forget closing '\"\"\"'?" : "did you forget closing quote?");
    return make_token(TokenType::INVALID);
}

Token Lexer::next_token() {
    skip_whitespace_and_comments();

    start_offset_ = current_offset_;
    start_line_ = line_;
    start_column_ = column_;

    if (is_at_end()) {
        return Token(TokenType::EOF_TOKEN, "", make_span(current_location()));
    }

    char c = advance();

    if (c == '\n' && emit_newlines_) {
        return make_token(TokenType::NEWLINE);
    }

    // Identifiers & keywords
    if (std::isalpha(c) || c == '_') {
        return scan_identifier_or_keyword();
    }

    // Number literals
    if (std::isdigit(c)) {
        return scan_number();
    }

    switch (c) {
        // Single / Double character operators & Delimiters
        case '(': return make_token(TokenType::LPAREN);
        case ')': return make_token(TokenType::RPAREN);
        case '{': return make_token(TokenType::LBRACE);
        case '}': return make_token(TokenType::RBRACE);
        case '[': return make_token(TokenType::LBRACKET);
        case ']': return make_token(TokenType::RBRACKET);
        case ',': return make_token(TokenType::COMMA);
        case ';': return make_token(TokenType::SEMICOLON);
        case '?': return make_token(TokenType::QUESTION);
        case '@': return make_token(TokenType::AT);
        case '~': return make_token(TokenType::TILDE);

        case ':':
            if (match(':')) return make_token(TokenType::COLON_COLON);
            return make_token(TokenType::COLON);

        case '+':
            return make_token(match('=') ? TokenType::PLUS_ASSIGN : TokenType::PLUS);

        case '-':
            if (match('>')) return make_token(TokenType::ARROW);
            if (match('=')) return make_token(TokenType::MINUS_ASSIGN);
            return make_token(TokenType::MINUS);

        case '*':
            if (match('*')) return make_token(TokenType::POWER);
            if (match('=')) return make_token(TokenType::STAR_ASSIGN);
            return make_token(TokenType::STAR);

        case '/':
            return make_token(match('=') ? TokenType::SLASH_ASSIGN : TokenType::SLASH);

        case '%':
            return make_token(match('=') ? TokenType::PERCENT_ASSIGN : TokenType::PERCENT);

        case '^':
            return make_token(TokenType::CARET);

        case '=':
            if (match('=')) return make_token(TokenType::EQUAL_EQUAL);
            if (match('>')) return make_token(TokenType::FAT_ARROW);
            return make_token(TokenType::ASSIGN);

        case '!':
            return make_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);

        case '<':
            return make_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);

        case '>':
            return make_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);

        case '&':
            if (match('&')) return make_token(TokenType::AMP_AMP);
            return make_token(TokenType::AMP);

        case '|':
            if (match('>')) return make_token(TokenType::PIPE_GREATER);
            if (match('|')) return make_token(TokenType::PIPE_PIPE);
            return make_token(TokenType::PIPE);

        case '.':
            if (match('.')) {
                if (match('=')) return make_token(TokenType::DOT_DOT_EQUAL);
                return make_token(TokenType::DOT_DOT);
            }
            // Check leading dot for float: .5
            if (std::isdigit(peek())) {
                current_offset_ = start_offset_;
                line_ = start_line_;
                column_ = start_column_;
                return scan_number();
            }
            return make_token(TokenType::DOT);

        // Strings
        case '"':
            // Check triple quotes: """..."""
            if (peek() == '"' && peek_next() == '"') {
                advance(); advance(); // consume extra two quotes
                return scan_string('"', /*is_raw=*/false, /*is_multiline=*/true);
            }
            return scan_string('"', /*is_raw=*/false, /*is_multiline=*/false);

        case '\'':
            // Check triple quotes: '''...'''
            if (peek() == '\'' && peek_next() == '\'') {
                advance(); advance();
                return scan_string('\'', /*is_raw=*/false, /*is_multiline=*/true);
            }
            return scan_string('\'', /*is_raw=*/false, /*is_multiline=*/false);

        default:
            diagnostics_.error(std::string("unexpected character '") + c + "'",
                               make_span(SourceLocation{start_line_, start_column_, start_offset_}));
            return make_token(TokenType::INVALID);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token token = next_token();
        tokens.push_back(token);
        if (token.type == TokenType::EOF_TOKEN) {
            break;
        }
    }
    return tokens;
}

} // namespace nextviper
