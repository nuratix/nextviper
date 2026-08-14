#pragma once

#include "nextviper/common.hpp"
#include "nextviper/token.hpp"
#include "nextviper/ast.hpp"
#include "nextviper/diagnostic.hpp"
#include <vector>
#include <memory>
#include <string>

namespace nextviper {

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticEngine& diagnostics);

    std::unique_ptr<Program> parse_program();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Expr> parse_expression();

private:
    // Statement parsing
    std::unique_ptr<Stmt> parse_let_statement();
    std::unique_ptr<Stmt> parse_fn_declaration();
    std::unique_ptr<Stmt> parse_if_statement();
    std::unique_ptr<Stmt> parse_while_statement();
    std::unique_ptr<Stmt> parse_for_in_statement();
    std::unique_ptr<Stmt> parse_return_statement();
    std::unique_ptr<Stmt> parse_break_statement();
    std::unique_ptr<Stmt> parse_continue_statement();
    std::unique_ptr<BlockStmt> parse_block_statement();
    std::unique_ptr<Stmt> parse_body_statement(size_t parent_col = 0);
    std::unique_ptr<Stmt> parse_expression_statement();
    std::string parse_type_annotation();

    // Expression parsing with precedence
    std::unique_ptr<Expr> parse_assignment();
    std::unique_ptr<Expr> parse_pipe();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_range();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_power();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_postfix();
    std::unique_ptr<Expr> parse_primary();

    std::unique_ptr<Expr> finish_call(std::unique_ptr<Expr> callee);
    std::unique_ptr<Expr> parse_array_literal();
    std::unique_ptr<Expr> parse_object_literal();
    std::unique_ptr<Expr> parse_lambda();

    // Helper utilities
    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message, const std::string& hint = "");
    void synchronize();

    std::vector<Token> tokens_;
    DiagnosticEngine& diagnostics_;
    size_t current_ = 0;
};

} // namespace nextviper
