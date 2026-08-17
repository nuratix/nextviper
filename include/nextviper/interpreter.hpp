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

enum class RuntimeErrorKind {
    TYPE_ERROR,
    NAME_ERROR,
    DIVISION_BY_ZERO,
    MUTABILITY_ERROR,
    INDEX_ERROR,
    ARGUMENT_ERROR,
    GENERIC_ERROR
};

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(RuntimeErrorKind kind, std::string message, SourceSpan span, std::string help = "")
        : std::runtime_error(message), kind_(kind), message_(std::move(message)), span_(span), help_(std::move(help)) {}

    // Backward compatibility constructor
    RuntimeError(std::string message, SourceSpan span, std::string help = "")
        : std::runtime_error(message), kind_(RuntimeErrorKind::GENERIC_ERROR), message_(std::move(message)), span_(span), help_(std::move(help)) {}

    RuntimeErrorKind kind() const { return kind_; }
    const std::string& message() const { return message_; }
    const SourceSpan& span() const { return span_; }
    const std::string& help() const { return help_; }

    std::string kind_name() const {
        switch (kind_) {
            case RuntimeErrorKind::TYPE_ERROR: return "TypeError";
            case RuntimeErrorKind::NAME_ERROR: return "NameError";
            case RuntimeErrorKind::DIVISION_BY_ZERO: return "DivisionByZeroError";
            case RuntimeErrorKind::MUTABILITY_ERROR: return "MutabilityError";
            case RuntimeErrorKind::INDEX_ERROR: return "IndexError";
            case RuntimeErrorKind::ARGUMENT_ERROR: return "ArgumentError";
            case RuntimeErrorKind::GENERIC_ERROR: return "RuntimeError";
        }
        return "RuntimeError";
    }

private:
    RuntimeErrorKind kind_ = RuntimeErrorKind::GENERIC_ERROR;
    std::string message_;
    SourceSpan span_;
    std::string help_;
};

class Interpreter : public ASTVisitor {
public:
    explicit Interpreter(DiagnosticEngine& diagnostics);

    bool execute(const Program& program);
    Value evaluate(const Expr& expr);
    void execute_statement(const Stmt& stmt);

    std::shared_ptr<Environment> globals() { return globals_; }
    std::shared_ptr<Environment> environment() { return environment_; }

    // Diagnostic runtime error helpers
    [[noreturn]] void runtime_error(const std::string& message, SourceSpan span, std::string help = "");
    [[noreturn]] void type_error(const std::string& message, SourceSpan span, std::string help = "");
    [[noreturn]] void name_error(const std::string& message, SourceSpan span, std::string help = "");
    [[noreturn]] void division_by_zero_error(SourceSpan span, std::string help = "check divisor before dividing");
    [[noreturn]] void mutability_error(const std::string& message, SourceSpan span, std::string help = "");
    [[noreturn]] void index_error(const std::string& message, SourceSpan span, std::string help = "");

    // AST Visitor methods for Expressions
    void visit_literal(const LiteralExpr& expr) override;
    void visit_identifier(const IdentifierExpr& expr) override;
    void visit_unary(const UnaryExpr& expr) override;
    void visit_binary(const BinaryExpr& expr) override;
    void visit_call(const CallExpr& expr) override;
    void visit_index(const IndexExpr& expr) override;
    void visit_slice(const SliceExpr& expr) override;
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
    void visit_import_stmt(const ImportStmt& stmt) override;
    void visit_export_stmt(const ExportStmt& stmt) override;

    void set_environment(std::shared_ptr<Environment> env) { environment_ = std::move(env); }
    void set_current_file(std::string file_path) { current_file_ = std::move(file_path); }
    const std::string& current_file() const { return current_file_; }
    std::shared_ptr<class ModuleManager> module_manager() const { return module_manager_; }

    static constexpr size_t MAX_CALL_STACK_DEPTH = 1000;
    static constexpr size_t MAX_STRING_BYTES = 64 * 1024 * 1024;
    static constexpr size_t MAX_ARRAY_ELEMENTS = 10'000'000;

    size_t call_stack_depth() const { return call_stack_depth_; }
    Value call_function(const Value& callee, const std::vector<Value>& args, SourceSpan span);

private:
    void init_builtins();
    void execute_block(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> env);

    size_t call_stack_depth_ = 0;
    DiagnosticEngine& diagnostics_;
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> environment_;
    Value last_evaluated_value_;
    std::string current_file_;
    std::shared_ptr<class ModuleManager> module_manager_;
};

} // namespace nextviper
