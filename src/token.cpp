#include "nextviper/token.hpp"
#include <sstream>

namespace nextviper {

std::string_view token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::INVALID: return "INVALID";

        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";

        case TokenType::KEYWORD_LET: return "let";
        case TokenType::KEYWORD_MUT: return "mut";
        case TokenType::KEYWORD_FN: return "fn";
        case TokenType::KEYWORD_RETURN: return "return";
        case TokenType::KEYWORD_IF: return "if";
        case TokenType::KEYWORD_ELSE: return "else";
        case TokenType::KEYWORD_WHILE: return "while";
        case TokenType::KEYWORD_FOR: return "for";
        case TokenType::KEYWORD_IN: return "in";
        case TokenType::KEYWORD_LOOP: return "loop";
        case TokenType::KEYWORD_BREAK: return "break";
        case TokenType::KEYWORD_CONTINUE: return "continue";
        case TokenType::KEYWORD_TRUE: return "true";
        case TokenType::KEYWORD_FALSE: return "false";
        case TokenType::KEYWORD_NIL: return "nil";
        case TokenType::KEYWORD_MATCH: return "match";
        case TokenType::KEYWORD_STRUCT: return "struct";
        case TokenType::KEYWORD_TYPE: return "type";
        case TokenType::KEYWORD_IMPORT: return "import";
        case TokenType::KEYWORD_AND: return "and";
        case TokenType::KEYWORD_OR: return "or";
        case TokenType::KEYWORD_NOT: return "not";

        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::PERCENT: return "%";
        case TokenType::POWER: return "**";

        case TokenType::ASSIGN: return "=";
        case TokenType::PLUS_ASSIGN: return "+=";
        case TokenType::MINUS_ASSIGN: return "-=";
        case TokenType::STAR_ASSIGN: return "*=";
        case TokenType::SLASH_ASSIGN: return "/=";
        case TokenType::PERCENT_ASSIGN: return "%=";

        case TokenType::EQUAL_EQUAL: return "==";
        case TokenType::BANG_EQUAL: return "!=";
        case TokenType::LESS: return "<";
        case TokenType::LESS_EQUAL: return "<=";
        case TokenType::GREATER: return ">";
        case TokenType::GREATER_EQUAL: return ">=";

        case TokenType::AMP_AMP: return "&&";
        case TokenType::PIPE_PIPE: return "||";
        case TokenType::BANG: return "!";

        case TokenType::PIPE_GREATER: return "|>";
        case TokenType::FAT_ARROW: return "=>";
        case TokenType::ARROW: return "->";
        case TokenType::DOT_DOT: return "..";
        case TokenType::DOT_DOT_EQUAL: return "..=";

        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::LBRACE: return "{";
        case TokenType::RBRACE: return "}";
        case TokenType::LBRACKET: return "[";
        case TokenType::RBRACKET: return "]";
        case TokenType::COMMA: return ",";
        case TokenType::COLON: return ":";
        case TokenType::SEMICOLON: return ";";
        case TokenType::DOT: return ".";
        case TokenType::QUESTION: return "?";
    }
    return "UNKNOWN";
}

std::string_view Token::type_name() const {
    return token_type_to_string(type);
}

std::string Token::to_string() const {
    std::ostringstream ss;
    ss << "Token(" << type_name() << ", \"" << text << "\", " << span.start << ")";
    return ss.str();
}

} // namespace nextviper
