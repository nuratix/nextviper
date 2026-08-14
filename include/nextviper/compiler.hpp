#pragma once

#include "nextviper/ast.hpp"
#include "nextviper/compiled_function.hpp"
#include "nextviper/diagnostic.hpp"
#include <memory>
#include <vector>
#include <string>

namespace nextviper {

struct Local {
    std::string name;
    int depth = 0;
    bool is_mut = false;
};

class BytecodeCompiler : public ASTVisitor {
public:
    explicit BytecodeCompiler(DiagnosticEngine& diagnostics);

    std::shared_ptr<CompiledFunction> compile(const Program& program);

    // Expressions
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

    // Statements
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
    struct CompilerContext {
        std::shared_ptr<CompiledFunction> function;
        std::vector<Local> locals;
        int scope_depth = 0;
        CompilerContext* enclosing = nullptr;
        std::vector<size_t> loop_starts;
        std::vector<std::vector<size_t>> break_jumps;
        std::vector<std::vector<size_t>> continue_jumps;
    };

    Chunk& current_chunk();
    size_t current_line() const;

    void emit_byte(uint8_t byte);
    void emit_opcode(OpCode op);
    void emit_bytes(uint8_t byte1, uint8_t byte2);
    void emit_constant(Value value);
    size_t emit_jump(OpCode op);
    void patch_jump(size_t offset);
    void emit_loop(size_t loop_start);

    void begin_scope();
    void end_scope();
    void add_local(std::string name, bool is_mut);
    int resolve_local(CompilerContext* ctx, const std::string& name);

    DiagnosticEngine& diagnostics_;
    CompilerContext* current_ctx_ = nullptr;
    size_t line_ = 1;
};

} // namespace nextviper
