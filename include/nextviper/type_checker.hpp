#pragma once

#include "nextviper/ast.hpp"
#include "nextviper/type.hpp"
#include "nextviper/diagnostic.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace nextviper {

struct TypeSymbol {
    std::string name;
    TypePtr type;
    bool is_mut;
};

class TypeEnvironment : public std::enable_shared_from_this<TypeEnvironment> {
public:
    explicit TypeEnvironment(std::shared_ptr<TypeEnvironment> parent = nullptr)
        : parent_(std::move(parent)) {}

    static std::shared_ptr<TypeEnvironment> create(std::shared_ptr<TypeEnvironment> parent = nullptr) {
        return std::make_shared<TypeEnvironment>(std::move(parent));
    }

    void define(const std::string& name, TypePtr type, bool is_mut = false) {
        symbols_[name] = {name, type, is_mut};
    }

    bool assign(const std::string& name, TypePtr type, std::string& error_msg) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            if (!it->second.is_mut && parent_ != nullptr) { // local immutable
                error_msg = "cannot reassign to immutable variable '" + name + "'";
                return false;
            }
            if (!it->second.type->is_assignable_from(type)) {
                error_msg = "cannot assign type '" + type->to_string() + "' to variable '" + name + "' of type '" + it->second.type->to_string() + "'";
                return false;
            }
            return true;
        }
        if (parent_) {
            return parent_->assign(name, type, error_msg);
        }
        // Top-level bare assignment auto-declaration
        symbols_[name] = {name, type, true};
        return true;
    }

    TypePtr lookup(const std::string& name) const {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second.type;
        }
        if (parent_) {
            return parent_->lookup(name);
        }
        return nullptr;
    }

    bool is_declared(const std::string& name) const {
        if (symbols_.find(name) != symbols_.end()) return true;
        if (parent_) return parent_->is_declared(name);
        return false;
    }

    std::shared_ptr<TypeEnvironment> parent() const { return parent_; }

private:
    std::shared_ptr<TypeEnvironment> parent_;
    std::unordered_map<std::string, TypeSymbol> symbols_;
};

class TypeChecker : public ASTVisitor {
public:
    explicit TypeChecker(DiagnosticEngine& diagnostics);

    bool check(const Program& program);
    TypePtr infer_expression(const Expr& expr);
    void check_statement(const Stmt& stmt);

    std::shared_ptr<TypeEnvironment> environment() { return current_env_; }

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

private:
    void init_builtins();

    DiagnosticEngine& diagnostics_;
    std::shared_ptr<TypeEnvironment> globals_;
    std::shared_ptr<TypeEnvironment> current_env_;
    TypePtr current_function_return_type_ = nullptr;
    TypePtr last_inferred_type_ = nullptr;
};

} // namespace nextviper
