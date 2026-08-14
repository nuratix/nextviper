#pragma once

#include "nextviper/ast.hpp"
#include "nextviper/value.hpp"
#include "nextviper/environment.hpp"
#include "nextviper/diagnostic.hpp"
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace nextviper {

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(std::string message, SourceSpan span)
        : std::runtime_error(message), message_(std::move(message)), span_(span) {}

    const std::string& message() const { return message_; }
    const SourceSpan& span() const { return span_; }

private:
    std::string message_;
    SourceSpan span_;
};

class Interpreter : public ASTVisitor {
public:
    explicit Interpreter(DiagnosticEngine& diagnostics);

    bool execute(const Program& program);
    Value evaluate(const Expr& expr);
    void execute_statement(const Stmt& stmt);

    std::shared_ptr<Environment> globals() { return globals_; }
    std::shared_ptr<Environment> environment() { return environment_; }

    // AST Visitor methods for Expressions
    void visit_literal(const LiteralExpr& expr) override;
    void visit_identifier(const IdentifierExpr& expr) override;
    void visit_unary(const UnaryExpr& expr) override;
    void visit_binary(const BinaryExpr& expr) override;
    void visit_call(const CallExpr& expr) override;
    void visit_index(const IndexExpr& expr) override;
    void visit_array(const ArrayExpr& expr) override;
    void visit_object(const ObjectExpr& expr) override;
    void visit_pipe(const PipeExpr& expr) override;
    void visit_assign(const AssignExpr& expr) override;
    void visit_range(const RangeExpr& expr) override;
    void visit_lambda(const LambdaExpr& expr) override;

    // AST Visitor methods for Statements
    void visit_expr_stmt(const ExprStmt& stmt) override;
    void visit_let_stmt(const LetStmt& stmt) override;
    void visit_block_stmt(const BlockStmt& stmt) override;
    void visit_if_stmt(const IfStmt& stmt) override;
    void visit_while_stmt(const WhileStmt& stmt) override;
    void visit_for_in_stmt(const ForInStmt& stmt) override;
    void visit_return_stmt(const ReturnStmt& stmt) override;
    void visit_break_stmt(const BreakStmt& stmt) override;
    void visit_continue_stmt(const ContinueStmt& stmt) override;
    void visit_fn_decl_stmt(const FnDeclStmt& stmt) override;

    void execute_block(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> env);
    Value call_function(const Value& callee, const std::vector<Value>& args, SourceSpan span);

private:
    void init_builtins();
    void runtime_error(const std::string& message, SourceSpan span);

    DiagnosticEngine& diagnostics_;
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> environment_;
    Value last_evaluated_value_;
};

} // namespace nextviper
