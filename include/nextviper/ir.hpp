#pragma once

#include "nextviper/common.hpp"
#include "nextviper/ast.hpp"
#include "nextviper/type.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <variant>

namespace nextviper {

enum class IRTypeKind {
    VOID,
    INT64,
    FLOAT64,
    BOOL,
    STRING,
    PTR,
    ANY
};

inline std::string ir_type_to_string(IRTypeKind t) {
    switch (t) {
        case IRTypeKind::VOID: return "void";
        case IRTypeKind::INT64: return "i64";
        case IRTypeKind::FLOAT64: return "f64";
        case IRTypeKind::BOOL: return "bool";
        case IRTypeKind::STRING: return "str";
        case IRTypeKind::PTR: return "ptr";
        case IRTypeKind::ANY: return "any";
    }
    return "unknown";
}

enum class IROpcode {
    CONST_INT,
    CONST_FLOAT,
    CONST_BOOL,
    CONST_STRING,
    CONST_NIL,
    ALLOCA,
    LOAD,
    STORE,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    NOT,
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE,
    JMP,
    JMP_IF_TRUE,
    JMP_IF_FALSE,
    CALL,
    PRINT,
    RET
};

inline std::string ir_opcode_to_string(IROpcode op) {
    switch (op) {
        case IROpcode::CONST_INT: return "const_i64";
        case IROpcode::CONST_FLOAT: return "const_f64";
        case IROpcode::CONST_BOOL: return "const_bool";
        case IROpcode::CONST_STRING: return "const_str";
        case IROpcode::CONST_NIL: return "const_nil";
        case IROpcode::ALLOCA: return "alloca";
        case IROpcode::LOAD: return "load";
        case IROpcode::STORE: return "store";
        case IROpcode::ADD: return "add";
        case IROpcode::SUB: return "sub";
        case IROpcode::MUL: return "mul";
        case IROpcode::DIV: return "div";
        case IROpcode::MOD: return "mod";
        case IROpcode::NEG: return "neg";
        case IROpcode::NOT: return "not";
        case IROpcode::EQ: return "eq";
        case IROpcode::NE: return "ne";
        case IROpcode::LT: return "lt";
        case IROpcode::LE: return "le";
        case IROpcode::GT: return "gt";
        case IROpcode::GE: return "ge";
        case IROpcode::JMP: return "jmp";
        case IROpcode::JMP_IF_TRUE: return "jmp_if_true";
        case IROpcode::JMP_IF_FALSE: return "jmp_if_false";
        case IROpcode::CALL: return "call";
        case IROpcode::PRINT: return "print";
        case IROpcode::RET: return "ret";
    }
    return "unknown";
}

enum class OperandKind {
    REGISTER,
    CONSTANT_INT,
    CONSTANT_FLOAT,
    CONSTANT_BOOL,
    CONSTANT_STRING,
    SYMBOL,
    LABEL
};

struct IROperand {
    OperandKind kind;
    int reg_id = -1;
    int64_t int_val = 0;
    double float_val = 0.0;
    bool bool_val = false;
    std::string str_val;

    static IROperand make_reg(int id) {
        IROperand op;
        op.kind = OperandKind::REGISTER;
        op.reg_id = id;
        return op;
    }

    static IROperand make_int(int64_t v) {
        IROperand op;
        op.kind = OperandKind::CONSTANT_INT;
        op.int_val = v;
        return op;
    }

    static IROperand make_float(double v) {
        IROperand op;
        op.kind = OperandKind::CONSTANT_FLOAT;
        op.float_val = v;
        return op;
    }

    static IROperand make_bool(bool v) {
        IROperand op;
        op.kind = OperandKind::CONSTANT_BOOL;
        op.bool_val = v;
        return op;
    }

    static IROperand make_str(std::string v) {
        IROperand op;
        op.kind = OperandKind::CONSTANT_STRING;
        op.str_val = std::move(v);
        return op;
    }

    static IROperand make_symbol(std::string v) {
        IROperand op;
        op.kind = OperandKind::SYMBOL;
        op.str_val = std::move(v);
        return op;
    }

    static IROperand make_label(std::string v) {
        IROperand op;
        op.kind = OperandKind::LABEL;
        op.str_val = std::move(v);
        return op;
    }

    std::string to_string() const;
};

struct IRInstruction {
    IROpcode opcode;
    int dest_reg = -1; // -1 if instruction produces no value
    IRTypeKind type = IRTypeKind::VOID;
    std::vector<IROperand> args;
    SourceSpan span;

    std::string to_string() const;
};

struct IRBasicBlock {
    std::string label;
    std::vector<IRInstruction> instructions;

    std::string to_string() const;
};

struct IRFunction {
    std::string name;
    IRTypeKind return_type = IRTypeKind::VOID;
    std::vector<std::pair<std::string, IRTypeKind>> params;
    std::vector<std::unique_ptr<IRBasicBlock>> blocks;

    IRBasicBlock* create_block(std::string label);
    std::string to_string() const;
};

struct IRModule {
    std::string name;
    std::vector<std::unique_ptr<IRFunction>> functions;

    IRFunction* create_function(std::string name, IRTypeKind ret_type);
    std::string to_string() const;
};

// Generates Typed IR from NextViper AST
class IRGenerator : public ASTVisitor {
public:
    explicit IRGenerator(DiagnosticEngine& diag);

    std::unique_ptr<IRModule> generate(const Program& program);

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
    std::unique_ptr<IRModule> module_;
    IRFunction* current_func_ = nullptr;
    IRBasicBlock* current_block_ = nullptr;

    int next_reg_ = 0;
    int next_block_id_ = 0;
    IROperand last_operand_;
    IRTypeKind last_type_ = IRTypeKind::VOID;

    // Scope symbol table mapping variable name -> alloca register or operand
    std::map<std::string, int> var_allocas_;
    std::map<std::string, IRTypeKind> var_types_;

    int alloc_reg();
    std::string new_block_label(const std::string& prefix = "bb");
    void emit(IROpcode op, int dest, IRTypeKind type, std::vector<IROperand> args, SourceSpan span = SourceSpan{});
};

// Optimizer for NextViper Typed IR
class IROptimizer {
public:
    IROptimizer() = default;

    void optimize(IRModule& module);

private:
    bool run_constant_folding(IRFunction& func);
    bool run_dead_code_elimination(IRFunction& func);
};

} // namespace nextviper
