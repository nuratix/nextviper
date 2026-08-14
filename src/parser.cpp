#include "nextviper/parser.hpp"

namespace nextviper {

Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& diagnostics)
    : recursion_depth_(0), tokens_(std::move(tokens)), diagnostics_(diagnostics), current_(0) {}

Parser::RecursionGuard::RecursionGuard(Parser& p) : parser(p) {
    if (++parser.recursion_depth_ > MAX_RECURSION_DEPTH) {
        ok = false;
        parser.diagnostics_.error("maximum parsing nesting depth exceeded (limit: 500)", parser.peek().span,
                                  "simplify deeply nested expressions or blocks", "NV110");
    }
}

Parser::RecursionGuard::~RecursionGuard() {
    if (parser.recursion_depth_ > 0) --parser.recursion_depth_;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::EOF_TOKEN;
}

const Token& Parser::peek() const {
    if (current_ >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[current_];
}

const Token& Parser::previous() const {
    if (current_ == 0) return tokens_[0];
    return tokens_[current_ - 1];
}

Token Parser::advance() {
    if (!is_at_end()) current_++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message, const std::string& hint) {
    if (check(type)) return advance();
    diagnostics_.error(message, peek().span, hint);
    return peek();
}

void Parser::synchronize() {
    advance();

    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;

        switch (peek().type) {
            case TokenType::KEYWORD_FN:
            case TokenType::KEYWORD_LET:
            case TokenType::KEYWORD_IF:
            case TokenType::KEYWORD_WHILE:
            case TokenType::KEYWORD_FOR:
            case TokenType::KEYWORD_RETURN:
            case TokenType::KEYWORD_STRUCT:
                return;
            default:
                break;
        }
        advance();
    }
}

std::unique_ptr<Program> Parser::parse_program() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!is_at_end() && diagnostics_.error_count() < 100) {
        auto stmt = parse_statement();
        if (stmt) {
            statements.push_back(std::move(stmt));
        } else {
            synchronize();
        }
    }
    return std::make_unique<Program>(std::move(statements));
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    RecursionGuard guard(*this);
    if (!guard.ok) return nullptr;

    if (match(TokenType::KEYWORD_LET)) {
        return parse_let_statement();
    }
    if (match(TokenType::KEYWORD_FN)) {
        return parse_fn_declaration();
    }
    if (match(TokenType::KEYWORD_IF)) {
        return parse_if_statement();
    }
    if (match(TokenType::KEYWORD_WHILE)) {
        return parse_while_statement();
    }
    if (match(TokenType::KEYWORD_FOR)) {
        return parse_for_in_statement();
    }
    if (match(TokenType::KEYWORD_RETURN)) {
        return parse_return_statement();
    }
    if (match(TokenType::KEYWORD_BREAK)) {
        return parse_break_statement();
    }
    if (match(TokenType::KEYWORD_CONTINUE)) {
        return parse_continue_statement();
    }
    if (match(TokenType::KEYWORD_IMPORT)) {
        return parse_import_statement();
    }
    if (match(TokenType::KEYWORD_FROM)) {
        return parse_from_import_statement();
    }
    if (match(TokenType::KEYWORD_EXPORT)) {
        return parse_export_statement();
    }
    if (check(TokenType::LBRACE)) {
        return parse_block_statement();
    }
    return parse_expression_statement();
}
std::string Parser::parse_type_annotation() {
    Token base_type = consume(TokenType::IDENTIFIER, "expected type name");
    std::string result = base_type.text;

    // Generic type arguments, e.g. list[int], map[string, float], tensor[float]
    if (match(TokenType::LBRACKET)) {
        result += "[";
        result += parse_type_annotation();
        while (match(TokenType::COMMA)) {
            result += ", ";
            result += parse_type_annotation();
        }
        consume(TokenType::RBRACKET, "expected ']' after type arguments");
        result += "]";
    }
    return result;
}

std::unique_ptr<Stmt> Parser::parse_let_statement() {
    SourceLocation start_loc = previous().span.start;
    bool is_mut = false;

    if (match(TokenType::KEYWORD_MUT)) {
        is_mut = true;
    }

    Token name_tok = consume(TokenType::IDENTIFIER, "expected variable name after 'let'");
    std::string name = name_tok.text;

    std::string type_annotation;
    if (match(TokenType::COLON)) {
        type_annotation = parse_type_annotation();
    }

    std::unique_ptr<Expr> initializer = nullptr;
    if (match(TokenType::ASSIGN)) {
        initializer = parse_expression();
    }

    // Optional semicolon
    match(TokenType::SEMICOLON);

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<LetStmt>(std::move(name), std::move(type_annotation), std::move(initializer), is_mut, span);
}

std::unique_ptr<Stmt> Parser::parse_fn_declaration() {
    SourceLocation start_loc = previous().span.start;
    Token name_tok = consume(TokenType::IDENTIFIER, "expected function name after 'fn'");
    std::string name = name_tok.text;

    consume(TokenType::LPAREN, "expected '(' after function name");

    std::vector<FnDeclStmt::Parameter> params;
    if (!check(TokenType::RPAREN)) {
        do {
            Token param_name = consume(TokenType::IDENTIFIER, "expected parameter name");
            std::string param_type;
            if (match(TokenType::COLON)) {
                param_type = parse_type_annotation();
            }
            params.push_back({param_name.text, param_type});
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "expected ')' after parameter list");

    std::string return_type;
    if (match(TokenType::ARROW)) {
        return_type = parse_type_annotation();
    }

    std::unique_ptr<BlockStmt> body = nullptr;
    std::unique_ptr<Expr> expr_body = nullptr;

    if (match(TokenType::FAT_ARROW)) {
        expr_body = parse_expression();
        match(TokenType::SEMICOLON);
    } else if (check(TokenType::LBRACE)) {
        body = parse_block_statement();
    } else if (match(TokenType::COLON)) {
        if (check(TokenType::LBRACE)) {
            body = parse_block_statement();
        } else {
            size_t fn_col = 1;
            std::vector<std::unique_ptr<Stmt>> stmts;

            if (!is_at_end() && peek().span.start.line == previous().span.start.line) {
                auto s = parse_statement();
                if (s) stmts.push_back(std::move(s));
            } else {
                size_t min_body_col = 0;
                while (!is_at_end() && !check(TokenType::RBRACE)) {
                    if (min_body_col == 0) {
                        min_body_col = peek().span.start.column;
                        if (min_body_col <= fn_col) {
                            break;
                        }
                    }
                    if (peek().span.start.column < min_body_col) {
                        break;
                    }
                    auto s = parse_statement();
                    if (s) {
                        stmts.push_back(std::move(s));
                    } else {
                        break;
                    }
                }
            }

            SourceSpan b_span(start_loc, previous().span.end, previous().span.file_path);
            body = std::make_unique<BlockStmt>(std::move(stmts), b_span);
        }
    } else {
        body = parse_block_statement();
    }

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<FnDeclStmt>(std::move(name), std::move(params), std::move(return_type),
                                        std::move(body), std::move(expr_body), span);
}

std::unique_ptr<Stmt> Parser::parse_body_statement(size_t parent_col) {
    if (check(TokenType::LBRACE)) {
        return parse_block_statement();
    }

    if (match(TokenType::COLON)) {
        if (check(TokenType::LBRACE)) {
            return parse_block_statement();
        }

        SourceLocation start_loc = previous().span.start;
        if (parent_col == 0) {
            parent_col = 1;
        }

        // If statement is on the same line as ':'
        if (!is_at_end() && peek().span.start.line == previous().span.start.line) {
            return parse_statement();
        }

        std::vector<std::unique_ptr<Stmt>> statements;
        size_t min_body_col = 0;
        while (!is_at_end() && !check(TokenType::RBRACE) && !check(TokenType::KEYWORD_ELSE) && !check(TokenType::KEYWORD_ELIF)) {
            if (min_body_col == 0) {
                min_body_col = peek().span.start.column;
                if (min_body_col <= parent_col) {
                    break;
                }
            }
            if (peek().span.start.column < min_body_col) {
                break;
            }
            auto s = parse_statement();
            if (s) {
                statements.push_back(std::move(s));
            } else {
                break;
            }
        }

        SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
        if (statements.size() == 1) {
            return std::move(statements[0]);
        }
        return std::make_unique<BlockStmt>(std::move(statements), span);
    }

    // Default: expect '{'
    consume(TokenType::LBRACE, "expected '{' or ':' before block");
    return parse_block_statement();
}

std::unique_ptr<Stmt> Parser::parse_if_statement() {
    SourceLocation start_loc = previous().span.start;
    auto condition = parse_expression();

    auto then_branch = parse_body_statement(start_loc.column);
    std::unique_ptr<Stmt> else_branch = nullptr;

    if (match(TokenType::KEYWORD_ELIF)) {
        else_branch = parse_if_statement();
    } else if (match(TokenType::KEYWORD_ELSE)) {
        if (match(TokenType::KEYWORD_IF)) {
            else_branch = parse_if_statement();
        } else {
            else_branch = parse_body_statement(start_loc.column);
        }
    }

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<IfStmt>(std::move(condition), std::move(then_branch), std::move(else_branch), span);
}

std::unique_ptr<Stmt> Parser::parse_while_statement() {
    SourceLocation start_loc = previous().span.start;
    auto condition = parse_expression();
    auto body = parse_body_statement(start_loc.column);

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), span);
}

std::unique_ptr<Stmt> Parser::parse_for_in_statement() {
    SourceLocation start_loc = previous().span.start;
    Token var_tok = consume(TokenType::IDENTIFIER, "expected loop variable name after 'for'");
    consume(TokenType::KEYWORD_IN, "expected 'in' after loop variable");
    auto iterable = parse_expression();
    auto body = parse_body_statement(start_loc.column);

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<ForInStmt>(var_tok.text, std::move(iterable), std::move(body), span);
}

std::unique_ptr<Stmt> Parser::parse_return_statement() {
    SourceLocation start_loc = previous().span.start;
    std::unique_ptr<Expr> value = nullptr;

    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !is_at_end()) {
        value = parse_expression();
    }

    match(TokenType::SEMICOLON);
    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<ReturnStmt>(std::move(value), span);
}

std::unique_ptr<Stmt> Parser::parse_break_statement() {
    SourceSpan span = previous().span;
    match(TokenType::SEMICOLON);
    return std::make_unique<BreakStmt>(span);
}

std::unique_ptr<Stmt> Parser::parse_continue_statement() {
    SourceSpan span = previous().span;
    match(TokenType::SEMICOLON);
    return std::make_unique<ContinueStmt>(span);
}

std::unique_ptr<Stmt> Parser::parse_import_statement() {
    SourceLocation start_loc = previous().span.start;
    std::string mod_name;
    if (check(TokenType::STRING_LITERAL)) {
        Token tok = advance();
        mod_name = tok.string_value.empty() ? tok.text : tok.string_value;
    } else {
        Token tok = consume(TokenType::IDENTIFIER, "expected module name after 'import'");
        mod_name = tok.text;
    }

    std::string alias = "";
    if (match(TokenType::KEYWORD_AS)) {
        Token alias_tok = consume(TokenType::IDENTIFIER, "expected alias name after 'as'");
        alias = alias_tok.text;
    }

    match(TokenType::SEMICOLON);
    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<ImportStmt>(mod_name, alias, std::vector<ImportStmt::ImportItem>{}, false, span);
}

std::unique_ptr<Stmt> Parser::parse_from_import_statement() {
    SourceLocation start_loc = previous().span.start;
    std::string mod_name;
    if (check(TokenType::STRING_LITERAL)) {
        Token tok = advance();
        mod_name = tok.string_value.empty() ? tok.text : tok.string_value;
    } else {
        Token tok = consume(TokenType::IDENTIFIER, "expected module name after 'from'");
        mod_name = tok.text;
    }

    consume(TokenType::KEYWORD_IMPORT, "expected 'import' after module name");

    std::vector<ImportStmt::ImportItem> items;
    do {
        Token sym_tok = consume(TokenType::IDENTIFIER, "expected symbol name to import");
        std::string alias = sym_tok.text;
        if (match(TokenType::KEYWORD_AS)) {
            Token alias_tok = consume(TokenType::IDENTIFIER, "expected alias name after 'as'");
            alias = alias_tok.text;
        }
        items.push_back({sym_tok.text, alias});
    } while (match(TokenType::COMMA));

    match(TokenType::SEMICOLON);
    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<ImportStmt>(mod_name, "", std::move(items), true, span);
}

std::unique_ptr<Stmt> Parser::parse_export_statement() {
    SourceLocation start_loc = previous().span.start;
    auto inner = parse_statement();
    if (!inner) {
        diagnostics_.error("expected declaration after 'export'", SourceSpan(start_loc, previous().span.end, previous().span.file_path));
        return nullptr;
    }
    SourceSpan span(start_loc, inner->span().end, previous().span.file_path);
    return std::make_unique<ExportStmt>(std::move(inner), span);
}

std::unique_ptr<BlockStmt> Parser::parse_block_statement() {
    SourceLocation start_loc = peek().span.start;
    consume(TokenType::LBRACE, "expected '{' before block");

    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        auto s = parse_statement();
        if (s) {
            statements.push_back(std::move(s));
        } else {
            synchronize();
        }
    }

    consume(TokenType::RBRACE, "expected '}' after block");
    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<BlockStmt>(std::move(statements), span);
}

std::unique_ptr<Stmt> Parser::parse_expression_statement() {
    SourceLocation start_loc = peek().span.start;
    auto expr = parse_expression();
    if (!expr) return nullptr;
    match(TokenType::SEMICOLON);
    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<ExprStmt>(std::move(expr), span);
}

std::unique_ptr<Expr> Parser::parse_expression() {
    RecursionGuard guard(*this);
    if (!guard.ok) return nullptr;
    return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment() {
    auto expr = parse_pipe();

    if (match({TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN,
               TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN, TokenType::PERCENT_ASSIGN})) {
        TokenType op = previous().type;
        auto value = parse_assignment();

        if (!expr || !value) {
            return nullptr;
        }

        if (auto* id_expr = dynamic_cast<IdentifierExpr*>(expr.get())) {
            SourceSpan span = SourceSpan::merge(id_expr->span(), value->span());
            return std::make_unique<AssignExpr>(id_expr->name(), op, std::move(value), span);
        } else if (auto* idx_expr = dynamic_cast<IndexExpr*>(expr.get())) {
            SourceSpan span = SourceSpan::merge(idx_expr->span(), value->span());
            // Transfer ownership
            auto target = std::unique_ptr<IndexExpr>(static_cast<IndexExpr*>(expr.release()));
            return std::make_unique<AssignExpr>(std::move(target), op, std::move(value), span);
        }

        diagnostics_.error("invalid target for assignment", expr ? expr->span() : peek().span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_pipe() {
    auto expr = parse_logical_or();

    while (match(TokenType::PIPE_GREATER)) {
        Token pipe_tok = previous();
        auto transform = parse_logical_or();
        if (!expr || !transform) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), transform->span());
        expr = std::make_unique<PipeExpr>(std::move(expr), std::move(transform), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
    auto expr = parse_logical_and();

    while (match({TokenType::PIPE_PIPE, TokenType::KEYWORD_OR})) {
        TokenType op = previous().type;
        auto right = parse_logical_and();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
    auto expr = parse_equality();

    while (match({TokenType::AMP_AMP, TokenType::KEYWORD_AND})) {
        TokenType op = previous().type;
        auto right = parse_equality();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_equality() {
    auto expr = parse_comparison();

    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
        TokenType op = previous().type;
        auto right = parse_comparison();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
    auto expr = parse_range();

    while (match({TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL})) {
        TokenType op = previous().type;
        auto right = parse_range();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_range() {
    auto expr = parse_term();

    if (match({TokenType::DOT_DOT, TokenType::DOT_DOT_EQUAL})) {
        bool inclusive = (previous().type == TokenType::DOT_DOT_EQUAL);
        std::unique_ptr<Expr> right = nullptr;
        if (!check(TokenType::COMMA) && !check(TokenType::RPAREN) && !check(TokenType::RBRACKET) &&
            !check(TokenType::RBRACE) && !check(TokenType::SEMICOLON) && !is_at_end()) {
            right = parse_term();
        }
        if (!expr) return nullptr;
        SourceSpan span = right ? SourceSpan::merge(expr->span(), right->span()) : expr->span();
        return std::make_unique<RangeExpr>(std::move(expr), std::move(right), inclusive, span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_term() {
    auto expr = parse_factor();

    while (match({TokenType::PLUS, TokenType::MINUS})) {
        TokenType op = previous().type;
        auto right = parse_factor();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_factor() {
    auto expr = parse_power();

    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        TokenType op = previous().type;
        auto right = parse_power();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_power() {
    auto expr = parse_unary();

    while (match(TokenType::POWER)) {
        TokenType op = previous().type;
        auto right = parse_unary();
        if (!expr || !right) return expr ? std::move(expr) : nullptr;
        SourceSpan span = SourceSpan::merge(expr->span(), right->span());
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), span);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (match({TokenType::BANG, TokenType::KEYWORD_NOT, TokenType::MINUS})) {
        Token op_tok = previous();
        auto operand = parse_unary();
        if (!operand) return nullptr;
        SourceSpan span = SourceSpan::merge(op_tok.span, operand->span());
        return std::make_unique<UnaryExpr>(op_tok.type, std::move(operand), span);
    }

    return parse_postfix();
}

std::unique_ptr<Expr> Parser::parse_postfix() {
    auto expr = parse_primary();
    if (!expr) return nullptr;

    while (true) {
        if (match(TokenType::LPAREN)) {
            expr = finish_call(std::move(expr));
            if (!expr) return nullptr;
        } else if (match(TokenType::LBRACKET)) {
            if (match(TokenType::COLON_COLON)) {
                std::unique_ptr<Expr> step = nullptr;
                if (!check(TokenType::RBRACKET)) {
                    step = parse_expression();
                }
                consume(TokenType::RBRACKET, "expected ']' after slice");
                SourceSpan span = SourceSpan::merge(expr->span(), previous().span);
                expr = std::make_unique<SliceExpr>(std::move(expr), nullptr, nullptr, std::move(step), span);
            } else if (match(TokenType::COLON)) {
                std::unique_ptr<Expr> end = nullptr;
                if (!check(TokenType::RBRACKET) && !check(TokenType::COLON) && !check(TokenType::COLON_COLON)) {
                    end = parse_expression();
                }
                std::unique_ptr<Expr> step = nullptr;
                if (match(TokenType::COLON)) {
                    if (!check(TokenType::RBRACKET)) {
                        step = parse_expression();
                    }
                }
                consume(TokenType::RBRACKET, "expected ']' after slice");
                SourceSpan span = SourceSpan::merge(expr->span(), previous().span);
                expr = std::make_unique<SliceExpr>(std::move(expr), nullptr, std::move(end), std::move(step), span);
            } else {
                auto first = parse_expression();
                if (match(TokenType::COLON_COLON)) {
                    std::unique_ptr<Expr> step = nullptr;
                    if (!check(TokenType::RBRACKET)) {
                        step = parse_expression();
                    }
                    consume(TokenType::RBRACKET, "expected ']' after slice");
                    SourceSpan span = SourceSpan::merge(expr->span(), previous().span);
                    expr = std::make_unique<SliceExpr>(std::move(expr), std::move(first), nullptr, std::move(step), span);
                } else if (match(TokenType::COLON)) {
                    std::unique_ptr<Expr> end = nullptr;
                    if (!check(TokenType::RBRACKET) && !check(TokenType::COLON) && !check(TokenType::COLON_COLON)) {
                        end = parse_expression();
                    }
                    std::unique_ptr<Expr> step = nullptr;
                    if (match(TokenType::COLON)) {
                        if (!check(TokenType::RBRACKET)) {
                            step = parse_expression();
                        }
                    }
                    consume(TokenType::RBRACKET, "expected ']' after slice");
                    SourceSpan span = SourceSpan::merge(expr->span(), previous().span);
                    expr = std::make_unique<SliceExpr>(std::move(expr), std::move(first), std::move(end), std::move(step), span);
                } else {
                    consume(TokenType::RBRACKET, "expected ']' after index expression");
                    SourceSpan span = SourceSpan::merge(expr->span(), previous().span);
                    expr = std::make_unique<IndexExpr>(std::move(expr), std::move(first), span);
                }
            }
        } else if (match(TokenType::DOT)) {
            Token name_tok = consume(TokenType::IDENTIFIER, "expected member name after '.'");
            auto member_name = std::make_unique<LiteralExpr>(name_tok.text, name_tok.span);
            SourceSpan span = SourceSpan::merge(expr->span(), name_tok.span);
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(member_name), span);
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expr> Parser::finish_call(std::unique_ptr<Expr> callee) {
    if (!callee) return nullptr;
    std::vector<std::unique_ptr<Expr>> args;
    if (!check(TokenType::RPAREN)) {
        do {
            auto arg = parse_expression();
            if (arg) args.push_back(std::move(arg));
        } while (match(TokenType::COMMA));
    }

    Token rparen = consume(TokenType::RPAREN, "expected ')' after arguments");
    SourceSpan span = SourceSpan::merge(callee->span(), rparen.span);
    return std::make_unique<CallExpr>(std::move(callee), std::move(args), span);
}

std::unique_ptr<Expr> Parser::parse_array_literal() {
    SourceLocation start_loc = previous().span.start;
    std::vector<std::unique_ptr<Expr>> elements;

    if (!check(TokenType::RBRACKET)) {
        do {
            if (check(TokenType::RBRACKET)) break; // trailing comma support
            auto elem = parse_expression();
            if (elem) elements.push_back(std::move(elem));
        } while (match(TokenType::COMMA));
    }

    Token rbracket = consume(TokenType::RBRACKET, "expected ']' after array elements");
    SourceSpan span(start_loc, rbracket.span.end, rbracket.span.file_path);
    return std::make_unique<ArrayExpr>(std::move(elements), span);
}

std::unique_ptr<Expr> Parser::parse_object_literal() {
    SourceLocation start_loc = previous().span.start;
    std::vector<ObjectExpr::Entry> entries;

    if (!check(TokenType::RBRACE)) {
        do {
            if (check(TokenType::RBRACE)) break; // trailing comma
            std::string key;
            if (match(TokenType::STRING_LITERAL)) {
                key = previous().string_value;
            } else if (match(TokenType::IDENTIFIER)) {
                key = previous().text;
            } else {
                diagnostics_.error("expected string or identifier key in object literal", peek().span);
                return nullptr;
            }

            consume(TokenType::COLON, "expected ':' after object key");
            auto val = parse_expression();
            if (val) entries.emplace_back(std::move(key), std::move(val));
        } while (match(TokenType::COMMA));
    }

    Token rbrace = consume(TokenType::RBRACE, "expected '}' after object literal");
    SourceSpan span(start_loc, rbrace.span.end, rbrace.span.file_path);
    return std::make_unique<ObjectExpr>(std::move(entries), span);
}

std::unique_ptr<Expr> Parser::parse_lambda() {
    SourceLocation start_loc = previous().span.start;
    consume(TokenType::LPAREN, "expected '(' after 'fn' for lambda");

    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do {
            Token p = consume(TokenType::IDENTIFIER, "expected parameter name");
            params.push_back(p.text);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "expected ')' after lambda parameter list");

    std::unique_ptr<Expr> body_expr = nullptr;
    std::unique_ptr<BlockStmt> body_block = nullptr;

    if (match(TokenType::FAT_ARROW)) {
        body_expr = parse_expression();
    } else if (match(TokenType::COLON)) {
        if (check(TokenType::LBRACE)) {
            body_block = parse_block_statement();
        } else {
            body_expr = parse_expression();
        }
    } else {
        body_block = parse_block_statement();
    }

    SourceSpan span(start_loc, previous().span.end, previous().span.file_path);
    return std::make_unique<LambdaExpr>(std::move(params), std::move(body_expr), std::move(body_block), span);
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (match({TokenType::KEYWORD_NIL, TokenType::KEYWORD_NULL})) {
        return std::make_unique<LiteralExpr>(previous().span);
    }
    if (match(TokenType::KEYWORD_TRUE)) {
        return std::make_unique<LiteralExpr>(true, previous().span);
    }
    if (match(TokenType::KEYWORD_FALSE)) {
        return std::make_unique<LiteralExpr>(false, previous().span);
    }
    if (match(TokenType::INT_LITERAL)) {
        return std::make_unique<LiteralExpr>(previous().int_value, previous().span);
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        return std::make_unique<LiteralExpr>(previous().float_value, previous().span);
    }
    if (match(TokenType::STRING_LITERAL)) {
        return std::make_unique<LiteralExpr>(previous().string_value, previous().span);
    }
    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<IdentifierExpr>(previous().text, previous().span);
    }
    if (match(TokenType::LBRACKET)) {
        return parse_array_literal();
    }
    if (match(TokenType::LBRACE)) {
        return parse_object_literal();
    }
    if (match(TokenType::KEYWORD_FN)) {
        return parse_lambda();
    }
    if (match(TokenType::LPAREN)) {
        SourceLocation start_loc = previous().span.start;
        auto expr = parse_expression();
        Token rparen = consume(TokenType::RPAREN, "expected ')' after expression");
        if (expr) {
            expr->set_span(SourceSpan(start_loc, rparen.span.end, rparen.span.file_path));
        }
        return expr;
    }

    // Diagnostics for unexpected tokens
    diagnostics_.error("expected expression, found '" + peek().text + "'", peek().span);
    advance();
    return std::make_unique<LiteralExpr>(previous().span);
}

} // namespace nextviper
