#pragma once

#include "nextviper/common.hpp"
#include "nextviper/ast.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>

namespace nextviper {

struct LintIssue {
    enum class Severity { WARNING, ERROR, HINT } severity;
    std::string code;
    std::string message;
    SourceSpan span;
    std::string suggestion;
};

class Linter : public ASTVisitor {
public:
    explicit Linter(DiagnosticEngine& diag);

    bool lint_program(const Program& program, const std::string& file_path);
    bool lint_file(const std::string& file_path);
    bool lint_files(const std::vector<std::string>& file_paths);

    const std::vector<LintIssue>& issues() const { return issues_; }
    size_t warning_count() const { return warning_count_; }
    size_t error_count() const { return error_count_; }

    // AST Visitor implementation
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

private:
    DiagnosticEngine& diag_;
    std::vector<LintIssue> issues_;
    size_t warning_count_ = 0;
    size_t error_count_ = 0;
    std::string current_file_;

    struct Scope {
        std::map<std::string, SourceSpan> declared_vars;
        std::set<std::string> used_vars;
        std::map<std::string, SourceSpan> declared_imports;
        std::set<std::string> used_imports;
    };

    std::vector<Scope> scopes_;

    void enter_scope();
    void exit_scope();
    void declare_var(const std::string& name, SourceSpan span);
    void use_var(const std::string& name);
    void declare_import(const std::string& name, SourceSpan span);
    void use_import(const std::string& name);

    void report_warning(const std::string& code, const std::string& msg, SourceSpan span, const std::string& suggestion);
};

} // namespace nextviper
