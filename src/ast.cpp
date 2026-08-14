#include "nextviper/ast.hpp"
#include <sstream>

namespace nextviper {

void ASTPrinter::indent() {
    for (int i = 0; i < indent_level_; ++i) {
        result_ += "  ";
    }
}

std::string ASTPrinter::print(const Program& program) {
    result_.clear();
    indent_level_ = 0;
    result_ += "Program:\n";
    indent_level_++;
    for (const auto& stmt : program.statements()) {
        stmt->accept(*this);
    }
    indent_level_--;
    return result_;
}

std::string ASTPrinter::print(const Stmt& stmt) {
    result_.clear();
    indent_level_ = 0;
    stmt.accept(*this);
    return result_;
}

std::string ASTPrinter::print(const Expr& expr) {
    result_.clear();
    indent_level_ = 0;
    expr.accept(*this);
    return result_;
}

void ASTPrinter::visit_literal(const LiteralExpr& expr) {
    indent();
    result_ += "Literal(";
    switch (expr.kind()) {
        case LiteralExpr::Kind::INT:
            result_ += "Int: " + std::to_string(expr.int_value());
            break;
        case LiteralExpr::Kind::FLOAT:
            result_ += "Float: " + std::to_string(expr.float_value());
            break;
        case LiteralExpr::Kind::STRING:
            result_ += "String: \"" + expr.string_value() + "\"";
            break;
        case LiteralExpr::Kind::BOOL:
            result_ += "Bool: " + std::string(expr.bool_value() ? "true" : "false");
            break;
        case LiteralExpr::Kind::NIL:
            result_ += "Nil";
            break;
    }
    result_ += ")\n";
}

void ASTPrinter::visit_identifier(const IdentifierExpr& expr) {
    indent();
    result_ += "Identifier(" + expr.name() + ")\n";
}

void ASTPrinter::visit_unary(const UnaryExpr& expr) {
    indent();
    result_ += "UnaryExpr(op: " + std::string(token_type_to_string(expr.op())) + ")\n";
    indent_level_++;
    expr.operand().accept(*this);
    indent_level_--;
}

void ASTPrinter::visit_binary(const BinaryExpr& expr) {
    indent();
    result_ += "BinaryExpr(op: " + std::string(token_type_to_string(expr.op())) + ")\n";
    indent_level_++;
    expr.left().accept(*this);
    expr.right().accept(*this);
    indent_level_--;
}

void ASTPrinter::visit_call(const CallExpr& expr) {
    indent();
    result_ += "CallExpr:\n";
    indent_level_++;
    indent();
    result_ += "Callee:\n";
    indent_level_++;
    expr.callee().accept(*this);
    indent_level_--;

    indent();
    result_ += "Arguments (" + std::to_string(expr.args().size()) + "):\n";
    indent_level_++;
    for (const auto& arg : expr.args()) {
        arg->accept(*this);
    }
    indent_level_--;
    indent_level_--;
}

void ASTPrinter::visit_index(const IndexExpr& expr) {
    indent();
    result_ += "IndexExpr:\n";
    indent_level_++;
    indent();
    result_ += "Target:\n";
    indent_level_++;
    expr.target().accept(*this);
    indent_level_--;
    indent();
    result_ += "Index:\n";
    indent_level_++;
    expr.index().accept(*this);
    indent_level_--;
    indent_level_--;
}

void ASTPrinter::visit_slice(const SliceExpr& expr) {
    indent();
    result_ += "SliceExpr:\n";
    indent_level_++;
    indent();
    result_ += "Target:\n";
    indent_level_++;
    expr.target().accept(*this);
    indent_level_--;
    if (expr.start()) {
        indent();
        result_ += "Start:\n";
        indent_level_++;
        expr.start()->accept(*this);
        indent_level_--;
    }
    if (expr.end()) {
        indent();
        result_ += "End:\n";
        indent_level_++;
        expr.end()->accept(*this);
        indent_level_--;
    }
    if (expr.step()) {
        indent();
        result_ += "Step:\n";
        indent_level_++;
        expr.step()->accept(*this);
        indent_level_--;
    }
    indent_level_--;
}

void ASTPrinter::visit_array(const ArrayExpr& expr) {
    indent();
    result_ += "ArrayExpr (" + std::to_string(expr.elements().size()) + " elements):\n";
    indent_level_++;
    for (const auto& el : expr.elements()) {
        el->accept(*this);
    }
    indent_level_--;
}

void ASTPrinter::visit_object(const ObjectExpr& expr) {
    indent();
    result_ += "ObjectExpr:\n";
    indent_level_++;
    for (const auto& [key, val] : expr.entries()) {
        indent();
        result_ += "Key: \"" + key + "\"\n";
        indent_level_++;
        val->accept(*this);
        indent_level_--;
    }
    indent_level_--;
}

void ASTPrinter::visit_pipe(const PipeExpr& expr) {
    indent();
    result_ += "PipeExpr (|>):\n";
    indent_level_++;
    indent();
    result_ += "Input:\n";
    indent_level_++;
    expr.left().accept(*this);
    indent_level_--;
    indent();
    result_ += "Transform:\n";
    indent_level_++;
    expr.right().accept(*this);
    indent_level_--;
    indent_level_--;
}

void ASTPrinter::visit_assign(const AssignExpr& expr) {
    indent();
    if (expr.is_index_assign()) {
        result_ += "AssignIndexExpr(op: " + std::string(token_type_to_string(expr.op())) + "):\n";
        indent_level_++;
        expr.index_target()->accept(*this);
    } else {
        result_ += "AssignExpr(var: " + expr.name() + ", op: " + std::string(token_type_to_string(expr.op())) + "):\n";
        indent_level_++;
    }
    expr.value().accept(*this);
    indent_level_--;
}

void ASTPrinter::visit_range(const RangeExpr& expr) {
    indent();
    result_ += "RangeExpr(inclusive: " + std::string(expr.inclusive() ? "true" : "false") + "):\n";
    indent_level_++;
    if (expr.start()) expr.start()->accept(*this);
    if (expr.end()) expr.end()->accept(*this);
    indent_level_--;
}

void ASTPrinter::visit_lambda(const LambdaExpr& expr) {
    indent();
    result_ += "LambdaExpr (params: ";
    for (size_t i = 0; i < expr.params().size(); ++i) {
        if (i > 0) result_ += ", ";
        result_ += expr.params()[i];
    }
    result_ += "):\n";
    indent_level_++;
    if (expr.body_expr()) {
        expr.body_expr()->accept(*this);
    } else if (expr.body_block()) {
        expr.body_block()->accept(*this);
    }
    indent_level_--;
}

void ASTPrinter::visit_expr_stmt(const ExprStmt& stmt) {
    indent();
    result_ += "ExprStmt:\n";
    indent_level_++;
    stmt.expr().accept(*this);
    indent_level_--;
}

void ASTPrinter::visit_let_stmt(const LetStmt& stmt) {
    indent();
    result_ += "LetStmt (name: " + stmt.name();
    if (stmt.is_mut()) result_ += ", mut";
    if (!stmt.type_annotation().empty()) result_ += ", type: " + stmt.type_annotation();
    result_ += "):\n";
    if (stmt.initializer()) {
        indent_level_++;
        stmt.initializer()->accept(*this);
        indent_level_--;
    }
}

void ASTPrinter::visit_block_stmt(const BlockStmt& stmt) {
    indent();
    result_ += "BlockStmt (" + std::to_string(stmt.statements().size()) + " statements):\n";
    indent_level_++;
    for (const auto& s : stmt.statements()) {
        s->accept(*this);
    }
    indent_level_--;
}

void ASTPrinter::visit_if_stmt(const IfStmt& stmt) {
    indent();
    result_ += "IfStmt:\n";
    indent_level_++;
    indent();
    result_ += "Condition:\n";
    indent_level_++;
    stmt.condition().accept(*this);
    indent_level_--;
    indent();
    result_ += "Then:\n";
    indent_level_++;
    stmt.then_branch().accept(*this);
    indent_level_--;
    if (stmt.else_branch()) {
        indent();
        result_ += "Else:\n";
        indent_level_++;
        stmt.else_branch()->accept(*this);
        indent_level_--;
    }
    indent_level_--;
}

void ASTPrinter::visit_while_stmt(const WhileStmt& stmt) {
    indent();
    result_ += "WhileStmt:\n";
    indent_level_++;
    indent();
    result_ += "Condition:\n";
    indent_level_++;
    stmt.condition().accept(*this);
    indent_level_--;
    indent();
    result_ += "Body:\n";
    indent_level_++;
    stmt.body().accept(*this);
    indent_level_--;
    indent_level_--;
}

void ASTPrinter::visit_for_in_stmt(const ForInStmt& stmt) {
    indent();
    result_ += "ForInStmt (var: " + stmt.variable_name() + "):\n";
    indent_level_++;
    indent();
    result_ += "Iterable:\n";
    indent_level_++;
    stmt.iterable().accept(*this);
    indent_level_--;
    indent();
    result_ += "Body:\n";
    indent_level_++;
    stmt.body().accept(*this);
    indent_level_--;
    indent_level_--;
}

void ASTPrinter::visit_return_stmt(const ReturnStmt& stmt) {
    indent();
    result_ += "ReturnStmt:\n";
    if (stmt.value()) {
        indent_level_++;
        stmt.value()->accept(*this);
        indent_level_--;
    }
}

void ASTPrinter::visit_break_stmt(const BreakStmt&) {
    indent();
    result_ += "BreakStmt\n";
}

void ASTPrinter::visit_continue_stmt(const ContinueStmt&) {
    indent();
    result_ += "ContinueStmt\n";
}

void ASTPrinter::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    indent();
    result_ += "FnDeclStmt (name: " + stmt.name() + "(";
    for (size_t i = 0; i < stmt.params().size(); ++i) {
        if (i > 0) result_ += ", ";
        result_ += stmt.params()[i].name;
        if (!stmt.params()[i].type_annotation.empty()) {
            result_ += ": " + stmt.params()[i].type_annotation;
        }
    }
    result_ += ")";
    if (!stmt.return_type().empty()) {
        result_ += " -> " + stmt.return_type();
    }
    result_ += "):\n";
    indent_level_++;
    if (stmt.body()) {
        stmt.body()->accept(*this);
    } else if (stmt.expr_body()) {
        stmt.expr_body()->accept(*this);
    }
    indent_level_--;
}

} // namespace nextviper
