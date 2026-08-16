#include "nextviper/linter.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace nextviper {

Linter::Linter(DiagnosticEngine& diag) : diag_(diag) {}

void Linter::enter_scope() {
    scopes_.emplace_back();
}

void Linter::exit_scope() {
    if (scopes_.empty()) return;
    auto& scope = scopes_.back();

    // Check unused variables
    for (const auto& [var_name, span] : scope.declared_vars) {
        if (!var_name.empty() && var_name[0] != '_' && scope.used_vars.find(var_name) == scope.used_vars.end()) {
            report_warning("NV3001", "unused variable '" + var_name + "'", span, "prefix with '_' if this is intentional");
        }
    }

    // Check unused imports
    for (const auto& [mod_name, span] : scope.declared_imports) {
        if (scope.used_imports.find(mod_name) == scope.used_imports.end()) {
            report_warning("NV3005", "unused import '" + mod_name + "'", span, "remove import or reference imported symbols");
        }
    }

    // Propagate used vars to parent scope
    if (scopes_.size() > 1) {
        auto& parent = scopes_[scopes_.size() - 2];
        for (const auto& var : scope.used_vars) {
            parent.used_vars.insert(var);
        }
        for (const auto& imp : scope.used_imports) {
            parent.used_imports.insert(imp);
        }
    }

    scopes_.pop_back();
}

void Linter::declare_var(const std::string& name, SourceSpan span) {
    if (scopes_.empty()) return;
    scopes_.back().declared_vars[name] = span;
}

void Linter::use_var(const std::string& name) {
    if (scopes_.empty()) return;
    scopes_.back().used_vars.insert(name);
    use_import(name);
}

void Linter::declare_import(const std::string& name, SourceSpan span) {
    if (scopes_.empty()) return;
    std::string base = name;
    size_t dot = base.rfind('.');
    if (dot != std::string::npos) base = base.substr(dot + 1);
    scopes_.front().declared_imports[base] = span;
}

void Linter::use_import(const std::string& name) {
    for (auto& scope : scopes_) {
        if (scope.declared_imports.count(name)) {
            scope.used_imports.insert(name);
        }
    }
}

void Linter::report_warning(const std::string& code, const std::string& msg, SourceSpan span, const std::string& suggestion) {
    issues_.push_back({LintIssue::Severity::WARNING, code, msg, span, suggestion});
    warning_count_++;
    diag_.warning(msg, span, suggestion, code);
}

bool Linter::lint_program(const Program& program, const std::string& file_path) {
    current_file_ = file_path;
    enter_scope(); // Global scope

    for (const auto& stmt : program.statements()) {
        if (stmt) stmt->accept(*this);
    }

    exit_scope(); // Exit global scope and report
    return warning_count_ == 0 && error_count_ == 0;
}

bool Linter::lint_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        diag_.error("could not open file '" + file_path + "' for linting", SourceSpan{}, "check file path", "NV2002");
        error_count_++;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    SourceManager sm;
    sm.add_file(file_path, source);
    DiagnosticEngine local_diag(sm, true);

    Lexer lexer(source, file_path, local_diag);
    auto tokens = lexer.tokenize();
    if (local_diag.has_errors()) {
        local_diag.render(std::cerr);
        error_count_++;
        return false;
    }

    Parser parser(tokens, local_diag);
    auto program = parser.parse_program();
    if (!program || local_diag.has_errors()) {
        local_diag.render(std::cerr);
        error_count_++;
        return false;
    }

    Linter linter(local_diag);
    bool clean = linter.lint_program(*program, file_path);
    if (local_diag.has_warnings() || local_diag.has_errors()) {
        local_diag.render(std::cerr);
    }
    return clean;
}

bool Linter::lint_files(const std::vector<std::string>& file_paths) {
    bool all_clean = true;
    for (const auto& path : file_paths) {
        if (!lint_file(path)) {
            all_clean = false;
        }
    }
    return all_clean;
}

// AST Visitor
void Linter::visit_literal(const LiteralExpr&) {}

void Linter::visit_identifier(const IdentifierExpr& expr) {
    use_var(expr.name());
}

void Linter::visit_unary(const UnaryExpr& expr) {
    expr.operand().accept(*this);
}

void Linter::visit_binary(const BinaryExpr& expr) {
    // Check self-comparison: x == x or x != x
    auto* left_id = dynamic_cast<const IdentifierExpr*>(&expr.left());
    auto* right_id = dynamic_cast<const IdentifierExpr*>(&expr.right());
    if (left_id && right_id && left_id->name() == right_id->name()) {
        if (expr.op() == TokenType::EQUAL_EQUAL || expr.op() == TokenType::BANG_EQUAL) {
            report_warning("NV3003", "comparing identifier '" + left_id->name() + "' to itself", expr.span(), "this condition is redundant and always evaluates to a constant");
        }
    }

    // Check redundant arithmetic with literals: x + 0, x - 0, x * 1, x / 1
    auto* right_lit = dynamic_cast<const LiteralExpr*>(&expr.right());
    if (right_lit) {
        if (expr.op() == TokenType::PLUS && right_lit->kind() == LiteralExpr::Kind::INT && right_lit->int_value() == 0) {
            report_warning("NV3004", "redundant addition with 0", expr.span(), "remove '+ 0'");
        } else if (expr.op() == TokenType::STAR && right_lit->kind() == LiteralExpr::Kind::INT && right_lit->int_value() == 1) {
            report_warning("NV3004", "redundant multiplication with 1", expr.span(), "remove '* 1'");
        }
    }

    expr.left().accept(*this);
    expr.right().accept(*this);
}

void Linter::visit_call(const CallExpr& expr) {
    expr.callee().accept(*this);
    for (const auto& arg : expr.args()) {
        if (arg) arg->accept(*this);
    }
}

void Linter::visit_index(const IndexExpr& expr) {
    expr.target().accept(*this);
    expr.index().accept(*this);
}

void Linter::visit_slice(const SliceExpr& expr) {
    expr.target().accept(*this);
    if (expr.start()) expr.start()->accept(*this);
    if (expr.end()) expr.end()->accept(*this);
    if (expr.step()) expr.step()->accept(*this);
}

void Linter::visit_array(const ArrayExpr& expr) {
    for (const auto& elem : expr.elements()) {
        if (elem) elem->accept(*this);
    }
}

void Linter::visit_object(const ObjectExpr& expr) {
    for (const auto& entry : expr.entries()) {
        if (entry.second) entry.second->accept(*this);
    }
}

void Linter::visit_pipe(const PipeExpr& expr) {
    expr.left().accept(*this);
    expr.right().accept(*this);
}

void Linter::visit_assign(const AssignExpr& expr) {
    if (expr.is_index_assign() && expr.index_target()) {
        expr.index_target()->accept(*this);
    } else {
        use_var(expr.name());
    }
    expr.value().accept(*this);
}

void Linter::visit_range(const RangeExpr& expr) {
    if (expr.start()) expr.start()->accept(*this);
    if (expr.end()) expr.end()->accept(*this);
}

void Linter::visit_lambda(const LambdaExpr& expr) {
    enter_scope();
    for (const auto& p : expr.params()) {
        declare_var(p, expr.span());
    }
    if (expr.body_expr()) expr.body_expr()->accept(*this);
    if (expr.body_block()) expr.body_block()->accept(*this);
    exit_scope();
}

void Linter::visit_expr_stmt(const ExprStmt& stmt) {
    stmt.expr().accept(*this);
}

void Linter::visit_let_stmt(const LetStmt& stmt) {
    if (stmt.initializer()) stmt.initializer()->accept(*this);
    declare_var(stmt.name(), stmt.span());
}

void Linter::visit_block_stmt(const BlockStmt& stmt) {
    enter_scope();
    bool has_terminator = false;
    for (const auto& s : stmt.statements()) {
        if (!s) continue;
        if (has_terminator) {
            report_warning("NV3002", "unreachable code detected", s->span(), "remove unreachable statements following return, break, or continue");
            break;
        }
        s->accept(*this);
        if (dynamic_cast<const ReturnStmt*>(s.get()) ||
            dynamic_cast<const BreakStmt*>(s.get()) ||
            dynamic_cast<const ContinueStmt*>(s.get())) {
            has_terminator = true;
        }
    }
    exit_scope();
}

void Linter::visit_if_stmt(const IfStmt& stmt) {
    stmt.condition().accept(*this);
    stmt.then_branch().accept(*this);
    if (stmt.else_branch()) stmt.else_branch()->accept(*this);
}

void Linter::visit_while_stmt(const WhileStmt& stmt) {
    stmt.condition().accept(*this);
    stmt.body().accept(*this);
}

void Linter::visit_for_in_stmt(const ForInStmt& stmt) {
    stmt.iterable().accept(*this);
    enter_scope();
    declare_var(stmt.variable_name(), stmt.span());
    stmt.body().accept(*this);
    exit_scope();
}

void Linter::visit_return_stmt(const ReturnStmt& stmt) {
    if (stmt.value()) stmt.value()->accept(*this);
}

void Linter::visit_break_stmt(const BreakStmt&) {}

void Linter::visit_continue_stmt(const ContinueStmt&) {}

void Linter::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    if (scopes_.size() > 1) {
        declare_var(stmt.name(), stmt.span());
    }
    enter_scope();
    for (const auto& p : stmt.params()) {
        declare_var(p.name, stmt.span());
    }
    if (stmt.body()) stmt.body()->accept(*this);
    exit_scope();
}

void Linter::visit_import_stmt(const ImportStmt& stmt) {
    declare_import(stmt.module_name(), stmt.span());
}

void Linter::visit_export_stmt(const ExportStmt& stmt) {
    stmt.inner_stmt().accept(*this);
}

} // namespace nextviper
