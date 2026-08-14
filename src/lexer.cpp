#include "nextviper/lexer.hpp"
#include <cctype>
#include <cstdlib>

namespace nextviper {

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
    {"nil", TokenType::KEYWORD_NIL},
    {"match", TokenType::KEYWORD_MATCH},
    {"struct", TokenType::KEYWORD_STRUCT},
    {"type", TokenType::KEYWORD_TYPE},
    {"import", TokenType::KEYWORD_IMPORT},
    {"and", TokenType::KEYWORD_AND},
    {"or", TokenType::KEYWORD_OR},
    {"not", TokenType::KEYWORD_NOT},
};

Lexer::Lexer(std::string source, std::string file_path, DiagnosticEngine& diagnostics)
    : source_(std::move(source)), file_path_(std::move(file_path)), diagnostics_(diagnostics) {}

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
            case '\n':
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // Single line comment
                    while (!is_at_end() && peek() != '\n') {
                        advance();
                    }
                } else if (peek_next() == '*') {
                    // Multi-line comment
                    SourceLocation start_loc = current_location();
                    advance(); // skip '/'
                    advance(); // skip '*'
                    bool closed = false;
                    while (!is_at_end()) {
                        if (peek() == '*' && peek_next() == '/') {
                            advance(); // skip '*'
                            advance(); // skip '/'
                            closed = true;
                            break;
                        }
                        advance();
                    }
                    if (!closed) {
                        diagnostics_.error("unterminated block comment", make_span(start_loc));
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
    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        Token token = make_token(it->second, text);
        if (it->second == TokenType::KEYWORD_TRUE) {
            token.int_value = 1;
        } else if (it->second == TokenType::KEYWORD_FALSE) {
            token.int_value = 0;
        }
        return token;
    }

    return make_token(TokenType::IDENTIFIER, text);
}

Token Lexer::scan_number() {
    bool is_float = false;

    // Check for hex: 0x... or 0X...
    if (source_[start_offset_] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); // consume 'x'
        while (std::isxdigit(peek()) || peek() == '_') {
            advance();
        }
        std::string raw = source_.substr(start_offset_ + 2, current_offset_ - (start_offset_ + 2));
        std::string clean;
        for (char c : raw) { if (c != '_') clean.push_back(c); }
        int64_t val = 0;
        try {
            val = std::stoll(clean, nullptr, 16);
        } catch (...) {
            diagnostics_.error("invalid hexadecimal number literal", make_span(SourceLocation{start_line_, start_column_, start_offset_}));
        }
        Token tok = make_token(TokenType::INT_LITERAL);
        tok.int_value = val;
        return tok;
    }

    // Check for binary: 0b... or 0B...
    if (source_[start_offset_] == '0' && (peek() == 'b' || peek() == 'B')) {
        advance(); // consume 'b'
        while (peek() == '0' || peek() == '1' || peek() == '_') {
            advance();
        }
        std::string raw = source_.substr(start_offset_ + 2, current_offset_ - (start_offset_ + 2));
        std::string clean;
        for (char c : raw) { if (c != '_') clean.push_back(c); }
        int64_t val = 0;
        try {
            val = std::stoll(clean, nullptr, 2);
        } catch (...) {
            diagnostics_.error("invalid binary number literal", make_span(SourceLocation{start_line_, start_column_, start_offset_}));
        }
        Token tok = make_token(TokenType::INT_LITERAL);
        tok.int_value = val;
        return tok;
    }

    while (std::isdigit(peek()) || peek() == '_') {
        advance();
    }

    // Fractional part: .123 (ensure it's not a range like 1..10)
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
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
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
            diagnostics_.error("invalid floating point literal", tok.span);
        }
        return tok;
    } else {
        Token tok = make_token(TokenType::INT_LITERAL, text);
        try {
            tok.int_value = std::stoll(clean);
        } catch (...) {
            diagnostics_.error("integer literal overflow", tok.span);
        }
        return tok;
    }
}

Token Lexer::scan_string() {
    std::string value;
    SourceLocation start_loc{start_line_, start_column_, start_offset_};

    while (!is_at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\') {
            if (is_at_end()) break;
            char esc = advance();
            switch (esc) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '\\': value.push_back('\\'); break;
                case '"': value.push_back('"'); break;
                case '0': value.push_back('\0'); break;
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

    if (is_at_end()) {
        diagnostics_.error("unterminated string literal", make_span(start_loc), "did you forget a closing '\"'?");
        return make_token(TokenType::INVALID);
    }

    advance(); // Consume closing quote '"'
    Token tok = make_token(TokenType::STRING_LITERAL);
    tok.string_value = std::move(value);
    return tok;
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

    // Identifiers & keywords
    if (std::isalpha(c) || c == '_') {
        return scan_identifier_or_keyword();
    }

    // Number literals
    if (std::isdigit(c)) {
        return scan_number();
    }

    switch (c) {
        // Single / Double character operators
        case '(': return make_token(TokenType::LPAREN);
        case ')': return make_token(TokenType::RPAREN);
        case '{': return make_token(TokenType::LBRACE);
        case '}': return make_token(TokenType::RBRACE);
        case '[': return make_token(TokenType::LBRACKET);
        case ']': return make_token(TokenType::RBRACKET);
        case ',': return make_token(TokenType::COMMA);
        case ':': return make_token(TokenType::COLON);
        case ';': return make_token(TokenType::SEMICOLON);
        case '?': return make_token(TokenType::QUESTION);

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
            return make_token(TokenType::POWER);

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
            diagnostics_.error("unexpected character '&', did you mean '&&' or 'and'?", make_span(SourceLocation{start_line_, start_column_, start_offset_}));
            return make_token(TokenType::INVALID);

        case '|':
            if (match('>')) return make_token(TokenType::PIPE_GREATER);
            if (match('|')) return make_token(TokenType::PIPE_PIPE);
            diagnostics_.error("unexpected character '|', did you mean '||' or '|>'?", make_span(SourceLocation{start_line_, start_column_, start_offset_}));
            return make_token(TokenType::INVALID);

        case '.':
            if (match('.')) {
                if (match('=')) return make_token(TokenType::DOT_DOT_EQUAL);
                return make_token(TokenType::DOT_DOT);
            }
            return make_token(TokenType::DOT);

        case '"':
            return scan_string();

        default:
            diagnostics_.error(std::string("unexpected character '") + c + "'", make_span(SourceLocation{start_line_, start_column_, start_offset_}));
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
