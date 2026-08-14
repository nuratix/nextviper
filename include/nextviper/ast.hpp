#pragma once

#include "nextviper/common.hpp"
#include "nextviper/token.hpp"
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace nextviper {

// Forward declarations
class Expr;
class Stmt;

// Expression forward declarations
class LiteralExpr;
class IdentifierExpr;
class UnaryExpr;
class BinaryExpr;
class CallExpr;
class IndexExpr;
class ArrayExpr;
class ObjectExpr;
class PipeExpr;
class AssignExpr;
class RangeExpr;
class LambdaExpr;

// Statement forward declarations
class ExprStmt;
class LetStmt;
class BlockStmt;
class IfStmt;
class WhileStmt;
class ForInStmt;
class ReturnStmt;
class BreakStmt;
class ContinueStmt;
class FnDeclStmt;

// Visitor Interface
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Expressions
    virtual void visit_literal(const LiteralExpr& expr) = 0;
    virtual void visit_identifier(const IdentifierExpr& expr) = 0;
    virtual void visit_unary(const UnaryExpr& expr) = 0;
    virtual void visit_binary(const BinaryExpr& expr) = 0;
    virtual void visit_call(const CallExpr& expr) = 0;
    virtual void visit_index(const IndexExpr& expr) = 0;
    virtual void visit_array(const ArrayExpr& expr) = 0;
    virtual void visit_object(const ObjectExpr& expr) = 0;
    virtual void visit_pipe(const PipeExpr& expr) = 0;
    virtual void visit_assign(const AssignExpr& expr) = 0;
    virtual void visit_range(const RangeExpr& expr) = 0;
    virtual void visit_lambda(const LambdaExpr& expr) = 0;

    // Statements
    virtual void visit_expr_stmt(const ExprStmt& stmt) = 0;
    virtual void visit_let_stmt(const LetStmt& stmt) = 0;
    virtual void visit_block_stmt(const BlockStmt& stmt) = 0;
    virtual void visit_if_stmt(const IfStmt& stmt) = 0;
    virtual void visit_while_stmt(const WhileStmt& stmt) = 0;
    virtual void visit_for_in_stmt(const ForInStmt& stmt) = 0;
    virtual void visit_return_stmt(const ReturnStmt& stmt) = 0;
    virtual void visit_break_stmt(const BreakStmt& stmt) = 0;
    virtual void visit_continue_stmt(const ContinueStmt& stmt) = 0;
    virtual void visit_fn_decl_stmt(const FnDeclStmt& stmt) = 0;
};

// Base AST Node
class ASTNode {
public:
    explicit ASTNode(SourceSpan span) : span_(span) {}
    virtual ~ASTNode() = default;

    const SourceSpan& span() const { return span_; }
    void set_span(SourceSpan s) { span_ = s; }

private:
    SourceSpan span_;
};

// Base Expression
class Expr : public ASTNode {
public:
    using ASTNode::ASTNode;
    virtual void accept(ASTVisitor& visitor) const = 0;
};

// Base Statement
class Stmt : public ASTNode {
public:
    using ASTNode::ASTNode;
    virtual void accept(ASTVisitor& visitor) const = 0;
};

// Literal Expression
class LiteralExpr : public Expr {
public:
    enum class Kind { INT, FLOAT, STRING, BOOL, NIL };

    LiteralExpr(int64_t val, SourceSpan span)
        : Expr(span), kind_(Kind::INT), int_val_(val) {}
    LiteralExpr(double val, SourceSpan span)
        : Expr(span), kind_(Kind::FLOAT), float_val_(val) {}
    LiteralExpr(std::string val, SourceSpan span)
        : Expr(span), kind_(Kind::STRING), string_val_(std::move(val)) {}
    LiteralExpr(bool val, SourceSpan span)
        : Expr(span), kind_(Kind::BOOL), bool_val_(val) {}
    explicit LiteralExpr(SourceSpan span)
        : Expr(span), kind_(Kind::NIL) {}

    Kind kind() const { return kind_; }
    int64_t int_value() const { return int_val_; }
    double float_value() const { return float_val_; }
    const std::string& string_value() const { return string_val_; }
    bool bool_value() const { return bool_val_; }

    void accept(ASTVisitor& visitor) const override { visitor.visit_literal(*this); }

private:
    Kind kind_;
    int64_t int_val_ = 0;
    double float_val_ = 0.0;
    std::string string_val_;
    bool bool_val_ = false;
};

// Identifier Expression
class IdentifierExpr : public Expr {
public:
    IdentifierExpr(std::string name, SourceSpan span)
        : Expr(span), name_(std::move(name)) {}

    const std::string& name() const { return name_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_identifier(*this); }

private:
    std::string name_;
};

// Unary Expression
class UnaryExpr : public Expr {
public:
    UnaryExpr(TokenType op, std::unique_ptr<Expr> operand, SourceSpan span)
        : Expr(span), op_(op), operand_(std::move(operand)) {}

    TokenType op() const { return op_; }
    const Expr& operand() const { return *operand_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_unary(*this); }

private:
    TokenType op_;
    std::unique_ptr<Expr> operand_;
};

// Binary Expression
class BinaryExpr : public Expr {
public:
    BinaryExpr(std::unique_ptr<Expr> left, TokenType op, std::unique_ptr<Expr> right, SourceSpan span)
        : Expr(span), left_(std::move(left)), op_(op), right_(std::move(right)) {}

    const Expr& left() const { return *left_; }
    TokenType op() const { return op_; }
    const Expr& right() const { return *right_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_binary(*this); }

private:
    std::unique_ptr<Expr> left_;
    TokenType op_;
    std::unique_ptr<Expr> right_;
};

// Call Expression
class CallExpr : public Expr {
public:
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args, SourceSpan span)
        : Expr(span), callee_(std::move(callee)), args_(std::move(args)) {}

    const Expr& callee() const { return *callee_; }
    const std::vector<std::unique_ptr<Expr>>& args() const { return args_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_call(*this); }

private:
    std::unique_ptr<Expr> callee_;
    std::vector<std::unique_ptr<Expr>> args_;
};

// Index Expression
class IndexExpr : public Expr {
public:
    IndexExpr(std::unique_ptr<Expr> target, std::unique_ptr<Expr> index, SourceSpan span)
        : Expr(span), target_(std::move(target)), index_(std::move(index)) {}

    const Expr& target() const { return *target_; }
    const Expr& index() const { return *index_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_index(*this); }

private:
    std::unique_ptr<Expr> target_;
    std::unique_ptr<Expr> index_;
};

// Array Expression
class ArrayExpr : public Expr {
public:
    ArrayExpr(std::vector<std::unique_ptr<Expr>> elements, SourceSpan span)
        : Expr(span), elements_(std::move(elements)) {}

    const std::vector<std::unique_ptr<Expr>>& elements() const { return elements_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_array(*this); }

private:
    std::vector<std::unique_ptr<Expr>> elements_;
};

// Object/Map Expression
class ObjectExpr : public Expr {
public:
    using Entry = std::pair<std::string, std::unique_ptr<Expr>>;

    ObjectExpr(std::vector<Entry> entries, SourceSpan span)
        : Expr(span), entries_(std::move(entries)) {}

    const std::vector<Entry>& entries() const { return entries_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_object(*this); }

private:
    std::vector<Entry> entries_;
};

// Pipeline Expression: expr |> func()
class PipeExpr : public Expr {
public:
    PipeExpr(std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, SourceSpan span)
        : Expr(span), left_(std::move(left)), right_(std::move(right)) {}

    const Expr& left() const { return *left_; }
    const Expr& right() const { return *right_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_pipe(*this); }

private:
    std::unique_ptr<Expr> left_;
    std::unique_ptr<Expr> right_;
};

// Assignment Expression: x = 10 or arr[0] = 5
class AssignExpr : public Expr {
public:
    AssignExpr(std::string name, TokenType op, std::unique_ptr<Expr> value, SourceSpan span)
        : Expr(span), name_(std::move(name)), op_(op), value_(std::move(value)) {}

    AssignExpr(std::unique_ptr<IndexExpr> index_target, TokenType op, std::unique_ptr<Expr> value, SourceSpan span)
        : Expr(span), index_target_(std::move(index_target)), op_(op), value_(std::move(value)) {}

    bool is_index_assign() const { return index_target_ != nullptr; }
    const std::string& name() const { return name_; }
    const IndexExpr* index_target() const { return index_target_.get(); }
    TokenType op() const { return op_; }
    const Expr& value() const { return *value_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_assign(*this); }

private:
    std::string name_;
    std::unique_ptr<IndexExpr> index_target_;
    TokenType op_;
    std::unique_ptr<Expr> value_;
};

// Range Expression: 0..10 or 1..=10
class RangeExpr : public Expr {
public:
    RangeExpr(std::unique_ptr<Expr> start, std::unique_ptr<Expr> end, bool inclusive, SourceSpan span)
        : Expr(span), start_(std::move(start)), end_(std::move(end)), inclusive_(inclusive) {}

    const Expr* start() const { return start_.get(); }
    const Expr* end() const { return end_.get(); }
    bool inclusive() const { return inclusive_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_range(*this); }

private:
    std::unique_ptr<Expr> start_;
    std::unique_ptr<Expr> end_;
    bool inclusive_;
};

// Lambda Expression: fn(x, y) => x + y
class LambdaExpr : public Expr {
public:
    LambdaExpr(std::vector<std::string> params, std::unique_ptr<Expr> body_expr, std::unique_ptr<BlockStmt> body_block, SourceSpan span)
        : Expr(span), params_(std::move(params)), body_expr_(std::move(body_expr)), body_block_(std::move(body_block)) {}

    const std::vector<std::string>& params() const { return params_; }
    const Expr* body_expr() const { return body_expr_.get(); }
    const BlockStmt* body_block() const { return body_block_.get(); }
    void accept(ASTVisitor& visitor) const override { visitor.visit_lambda(*this); }

private:
    std::vector<std::string> params_;
    std::unique_ptr<Expr> body_expr_;
    std::unique_ptr<BlockStmt> body_block_;
};

// Statement Classes

class ExprStmt : public Stmt {
public:
    ExprStmt(std::unique_ptr<Expr> expr, SourceSpan span)
        : Stmt(span), expr_(std::move(expr)) {}

    const Expr& expr() const { return *expr_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_expr_stmt(*this); }

private:
    std::unique_ptr<Expr> expr_;
};

class LetStmt : public Stmt {
public:
    LetStmt(std::string name, std::string type_annotation, std::unique_ptr<Expr> initializer, bool is_mut, SourceSpan span)
        : Stmt(span), name_(std::move(name)), type_annotation_(std::move(type_annotation)),
          initializer_(std::move(initializer)), is_mut_(is_mut) {}

    const std::string& name() const { return name_; }
    const std::string& type_annotation() const { return type_annotation_; }
    const Expr* initializer() const { return initializer_.get(); }
    bool is_mut() const { return is_mut_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_let_stmt(*this); }

private:
    std::string name_;
    std::string type_annotation_;
    std::unique_ptr<Expr> initializer_;
    bool is_mut_;
};

class BlockStmt : public Stmt {
public:
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements, SourceSpan span)
        : Stmt(span), statements_(std::move(statements)) {}

    const std::vector<std::unique_ptr<Stmt>>& statements() const { return statements_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_block_stmt(*this); }

private:
    std::vector<std::unique_ptr<Stmt>> statements_;
};

class IfStmt : public Stmt {
public:
    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch, SourceSpan span)
        : Stmt(span), condition_(std::move(condition)), then_branch_(std::move(then_branch)), else_branch_(std::move(else_branch)) {}

    const Expr& condition() const { return *condition_; }
    const Stmt& then_branch() const { return *then_branch_; }
    const Stmt* else_branch() const { return else_branch_.get(); }
    void accept(ASTVisitor& visitor) const override { visitor.visit_if_stmt(*this); }

private:
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> then_branch_;
    std::unique_ptr<Stmt> else_branch_;
};

class WhileStmt : public Stmt {
public:
    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body, SourceSpan span)
        : Stmt(span), condition_(std::move(condition)), body_(std::move(body)) {}

    const Expr& condition() const { return *condition_; }
    const Stmt& body() const { return *body_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_while_stmt(*this); }

private:
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> body_;
};

class ForInStmt : public Stmt {
public:
    ForInStmt(std::string variable_name, std::unique_ptr<Expr> iterable, std::unique_ptr<Stmt> body, SourceSpan span)
        : Stmt(span), variable_name_(std::move(variable_name)), iterable_(std::move(iterable)), body_(std::move(body)) {}

    const std::string& variable_name() const { return variable_name_; }
    const Expr& iterable() const { return *iterable_; }
    const Stmt& body() const { return *body_; }
    void accept(ASTVisitor& visitor) const override { visitor.visit_for_in_stmt(*this); }

private:
    std::string variable_name_;
    std::unique_ptr<Expr> iterable_;
    std::unique_ptr<Stmt> body_;
};

class ReturnStmt : public Stmt {
public:
    ReturnStmt(std::unique_ptr<Expr> value, SourceSpan span)
        : Stmt(span), value_(std::move(value)) {}

    const Expr* value() const { return value_.get(); }
    void accept(ASTVisitor& visitor) const override { visitor.visit_return_stmt(*this); }

private:
    std::unique_ptr<Expr> value_;
};

class BreakStmt : public Stmt {
public:
    using Stmt::Stmt;
    void accept(ASTVisitor& visitor) const override { visitor.visit_break_stmt(*this); }
};

class ContinueStmt : public Stmt {
public:
    using Stmt::Stmt;
    void accept(ASTVisitor& visitor) const override { visitor.visit_continue_stmt(*this); }
};

class FnDeclStmt : public Stmt {
public:
    struct Parameter {
        std::string name;
        std::string type_annotation;
    };

    FnDeclStmt(std::string name, std::vector<Parameter> params, std::string return_type,
               std::unique_ptr<BlockStmt> body, std::unique_ptr<Expr> expr_body, SourceSpan span)
        : Stmt(span), name_(std::move(name)), params_(std::move(params)),
          return_type_(std::move(return_type)), body_(std::move(body)), expr_body_(std::move(expr_body)) {}

    const std::string& name() const { return name_; }
    const std::vector<Parameter>& params() const { return params_; }
    const std::string& return_type() const { return return_type_; }
    const BlockStmt* body() const { return body_.get(); }
    const Expr* expr_body() const { return expr_body_.get(); }
    bool is_arrow_body() const { return expr_body_ != nullptr; }

    void accept(ASTVisitor& visitor) const override { visitor.visit_fn_decl_stmt(*this); }

private:
    std::string name_;
    std::vector<Parameter> params_;
    std::string return_type_;
    std::unique_ptr<BlockStmt> body_;
    std::unique_ptr<Expr> expr_body_;
};

// Program representation (list of statements)
class Program {
public:
    explicit Program(std::vector<std::unique_ptr<Stmt>> statements)
        : statements_(std::move(statements)) {}

    const std::vector<std::unique_ptr<Stmt>>& statements() const { return statements_; }
    void add_statement(std::unique_ptr<Stmt> stmt) { statements_.push_back(std::move(stmt)); }

private:
    std::vector<std::unique_ptr<Stmt>> statements_;
};

class ASTPrinter : public ASTVisitor {
public:
    std::string print(const Program& program);
    std::string print(const Stmt& stmt);
    std::string print(const Expr& expr);

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

    void visit_expr_stmt(const ExprStmt& stmt) override;
    void visit_let_stmt(const LetStmt& stmt) override;
    void visit_block_stmt(const BlockStmt& stmt) override;
    void visit_if_stmt(const IfStmt& stmt) override;
    void visit_while_stmt(const WhileStmt& stmt) override;
    void visit_for_in_stmt(const ForInStmt& stmt) override;
    virtual void visit_return_stmt(const ReturnStmt& stmt) override;
    virtual void visit_break_stmt(const BreakStmt& stmt) override;
    virtual void visit_continue_stmt(const ContinueStmt& stmt) override;
    virtual void visit_fn_decl_stmt(const FnDeclStmt& stmt) override;

private:
    std::string result_;
    int indent_level_ = 0;

    void indent();
};

} // namespace nextviper
