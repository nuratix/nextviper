#include "nextviper/compiler.hpp"

namespace nextviper {

BytecodeCompiler::BytecodeCompiler(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {}

Chunk& BytecodeCompiler::current_chunk() {
    return current_ctx_->function->chunk();
}

size_t BytecodeCompiler::current_line() const {
    return line_;
}

void BytecodeCompiler::emit_byte(uint8_t byte) {
    current_chunk().write(byte, current_line());
}

void BytecodeCompiler::emit_opcode(OpCode op) {
    current_chunk().write_opcode(op, current_line());
}

void BytecodeCompiler::emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

void BytecodeCompiler::emit_constant(Value value) {
    size_t idx = current_chunk().add_constant(std::move(value));
    if (idx > 255) {
        diagnostics_.error("too many constants in one chunk", SourceSpan{});
        return;
    }
    emit_opcode(OpCode::OP_CONSTANT);
    emit_byte(static_cast<uint8_t>(idx));
}

size_t BytecodeCompiler::emit_jump(OpCode op) {
    emit_opcode(op);
    emit_byte(0xFF);
    emit_byte(0xFF);
    return current_chunk().count() - 2;
}

void BytecodeCompiler::patch_jump(size_t offset) {
    size_t jump = current_chunk().count() - offset - 2;
    if (jump > 65535) {
        diagnostics_.error("jump offset too large", SourceSpan{});
        return;
    }
    current_chunk().byte_at(offset) = static_cast<uint8_t>((jump >> 8) & 0xFF);
    current_chunk().byte_at(offset + 1) = static_cast<uint8_t>(jump & 0xFF);
}

void BytecodeCompiler::emit_loop(size_t loop_start) {
    emit_opcode(OpCode::OP_LOOP);
    size_t jump = current_chunk().count() - loop_start + 2;
    if (jump > 65535) {
        diagnostics_.error("loop body too large", SourceSpan{});
        return;
    }
    emit_byte(static_cast<uint8_t>((jump >> 8) & 0xFF));
    emit_byte(static_cast<uint8_t>(jump & 0xFF));
}

void BytecodeCompiler::begin_scope() {
    current_ctx_->scope_depth++;
}

void BytecodeCompiler::end_scope() {
    current_ctx_->scope_depth--;
    while (!current_ctx_->locals.empty() && current_ctx_->locals.back().depth > current_ctx_->scope_depth) {
        emit_opcode(OpCode::OP_POP);
        current_ctx_->locals.pop_back();
    }
}

void BytecodeCompiler::add_local(std::string name, bool is_mut) {
    if (current_ctx_->locals.size() >= 256) {
        diagnostics_.error("too many local variables in scope", SourceSpan{});
        return;
    }
    current_ctx_->locals.push_back({std::move(name), current_ctx_->scope_depth, is_mut});
}

int BytecodeCompiler::resolve_local(CompilerContext* ctx, const std::string& name) {
    for (int i = static_cast<int>(ctx->locals.size()) - 1; i >= 0; --i) {
        if (ctx->locals[i].name == name) {
            return i;
        }
    }
    return -1;
}

std::shared_ptr<CompiledFunction> BytecodeCompiler::compile(const Program& program) {
    CompilerContext main_ctx;
    main_ctx.function = std::make_shared<CompiledFunction>("<script>", 0);
    // Stack slot 0 is reserved for VM caller/script
    main_ctx.locals.push_back({"", 0, false});

    current_ctx_ = &main_ctx;

    for (const auto& stmt : program.statements()) {
        stmt->accept(*this);
    }

    emit_opcode(OpCode::OP_NIL);
    emit_opcode(OpCode::OP_RETURN);

    current_ctx_ = nullptr;
    return main_ctx.function;
}

void BytecodeCompiler::visit_literal(const LiteralExpr& expr) {
    line_ = expr.span().start.line;
    switch (expr.kind()) {
        case LiteralExpr::Kind::INT:
            emit_constant(Value::make_int(expr.int_value()));
            break;
        case LiteralExpr::Kind::FLOAT:
            emit_constant(Value::make_float(expr.float_value()));
            break;
        case LiteralExpr::Kind::STRING:
            emit_constant(Value::make_string(expr.string_value()));
            break;
        case LiteralExpr::Kind::BOOL:
            emit_opcode(expr.bool_value() ? OpCode::OP_TRUE : OpCode::OP_FALSE);
            break;
        case LiteralExpr::Kind::NIL:
            emit_opcode(OpCode::OP_NIL);
            break;
    }
}

void BytecodeCompiler::visit_identifier(const IdentifierExpr& expr) {
    line_ = expr.span().start.line;
    int local = resolve_local(current_ctx_, expr.name());
    if (local != -1) {
        emit_opcode(OpCode::OP_GET_LOCAL);
        emit_byte(static_cast<uint8_t>(local));
    } else {
        size_t name_idx = current_chunk().add_constant(Value::make_string(expr.name()));
        emit_opcode(OpCode::OP_GET_GLOBAL);
        emit_byte(static_cast<uint8_t>(name_idx));
    }
}

void BytecodeCompiler::visit_unary(const UnaryExpr& expr) {
    line_ = expr.span().start.line;
    expr.operand().accept(*this);

    switch (expr.op()) {
        case TokenType::BANG:
        case TokenType::KEYWORD_NOT:
            emit_opcode(OpCode::OP_NOT);
            break;
        case TokenType::MINUS:
            emit_opcode(OpCode::OP_NEGATE);
            break;
        default:
            break;
    }
}

void BytecodeCompiler::visit_binary(const BinaryExpr& expr) {
    line_ = expr.span().start.line;

    // Short-circuit logical AND
    if (expr.op() == TokenType::AMP_AMP || expr.op() == TokenType::KEYWORD_AND) {
        expr.left().accept(*this);
        size_t end_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
        emit_opcode(OpCode::OP_POP);
        expr.right().accept(*this);
        patch_jump(end_jump);
        return;
    }

    // Short-circuit logical OR
    if (expr.op() == TokenType::PIPE_PIPE || expr.op() == TokenType::KEYWORD_OR) {
        expr.left().accept(*this);
        size_t else_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
        size_t end_jump = emit_jump(OpCode::OP_JUMP);
        patch_jump(else_jump);
        emit_opcode(OpCode::OP_POP);
        expr.right().accept(*this);
        patch_jump(end_jump);
        return;
    }

    expr.left().accept(*this);
    expr.right().accept(*this);

    switch (expr.op()) {
        case TokenType::PLUS: emit_opcode(OpCode::OP_ADD); break;
        case TokenType::MINUS: emit_opcode(OpCode::OP_SUBTRACT); break;
        case TokenType::STAR: emit_opcode(OpCode::OP_MULTIPLY); break;
        case TokenType::SLASH: emit_opcode(OpCode::OP_DIVIDE); break;
        case TokenType::PERCENT: emit_opcode(OpCode::OP_MODULO); break;
        case TokenType::POWER: emit_opcode(OpCode::OP_POWER); break;
        case TokenType::EQUAL_EQUAL: emit_opcode(OpCode::OP_EQUAL); break;
        case TokenType::BANG_EQUAL: emit_opcode(OpCode::OP_NOT_EQUAL); break;
        case TokenType::GREATER: emit_opcode(OpCode::OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emit_opcode(OpCode::OP_GREATER_EQUAL); break;
        case TokenType::LESS: emit_opcode(OpCode::OP_LESS); break;
        case TokenType::LESS_EQUAL: emit_opcode(OpCode::OP_LESS_EQUAL); break;
        default: break;
    }
}

void BytecodeCompiler::visit_call(const CallExpr& expr) {
    line_ = expr.span().start.line;
    expr.callee().accept(*this);

    for (const auto& arg : expr.args()) {
        arg->accept(*this);
    }

    emit_opcode(OpCode::OP_CALL);
    emit_byte(static_cast<uint8_t>(expr.args().size()));
}

void BytecodeCompiler::visit_index(const IndexExpr& expr) {
    line_ = expr.span().start.line;
    expr.target().accept(*this);
    expr.index().accept(*this);
    emit_opcode(OpCode::OP_INDEX_GET);
}

void BytecodeCompiler::visit_array(const ArrayExpr& expr) {
    line_ = expr.span().start.line;
    for (const auto& el : expr.elements()) {
        el->accept(*this);
    }
    emit_opcode(OpCode::OP_BUILD_ARRAY);
    emit_byte(static_cast<uint8_t>(expr.elements().size()));
}

void BytecodeCompiler::visit_object(const ObjectExpr& expr) {
    line_ = expr.span().start.line;
    for (const auto& [k, v] : expr.entries()) {
        emit_constant(Value::make_string(k));
        v->accept(*this);
    }
    emit_opcode(OpCode::OP_BUILD_OBJECT);
    emit_byte(static_cast<uint8_t>(expr.entries().size()));
}

void BytecodeCompiler::visit_pipe(const PipeExpr& expr) {
    line_ = expr.span().start.line;

    if (auto* call = dynamic_cast<const CallExpr*>(&expr.right())) {
        call->callee().accept(*this);
        expr.left().accept(*this); // First argument
        for (const auto& arg : call->args()) {
            arg->accept(*this);
        }
        emit_opcode(OpCode::OP_CALL);
        emit_byte(static_cast<uint8_t>(call->args().size() + 1));
        return;
    }

    expr.right().accept(*this);
    expr.left().accept(*this);
    emit_opcode(OpCode::OP_CALL);
    emit_byte(1);
}

void BytecodeCompiler::visit_assign(const AssignExpr& expr) {
    line_ = expr.span().start.line;

    if (expr.is_index_assign()) {
        const auto* idx = expr.index_target();
        idx->target().accept(*this);
        idx->index().accept(*this);
        expr.value().accept(*this);
        emit_opcode(OpCode::OP_INDEX_SET);
        return;
    }

    int local = resolve_local(current_ctx_, expr.name());

    if (expr.op() != TokenType::ASSIGN) {
        // Load current value first
        if (local != -1) {
            emit_opcode(OpCode::OP_GET_LOCAL);
            emit_byte(static_cast<uint8_t>(local));
        } else {
            size_t name_idx = current_chunk().add_constant(Value::make_string(expr.name()));
            emit_opcode(OpCode::OP_GET_GLOBAL);
            emit_byte(static_cast<uint8_t>(name_idx));
        }

        expr.value().accept(*this);

        switch (expr.op()) {
            case TokenType::PLUS_ASSIGN: emit_opcode(OpCode::OP_ADD); break;
            case TokenType::MINUS_ASSIGN: emit_opcode(OpCode::OP_SUBTRACT); break;
            case TokenType::STAR_ASSIGN: emit_opcode(OpCode::OP_MULTIPLY); break;
            case TokenType::SLASH_ASSIGN: emit_opcode(OpCode::OP_DIVIDE); break;
            case TokenType::PERCENT_ASSIGN: emit_opcode(OpCode::OP_MODULO); break;
            default: break;
        }
    } else {
        expr.value().accept(*this);
    }

    if (local != -1) {
        if (!current_ctx_->locals[local].is_mut) {
            diagnostics_.error("cannot reassign to immutable variable '" + expr.name() + "'", expr.span());
        }
        emit_opcode(OpCode::OP_SET_LOCAL);
        emit_byte(static_cast<uint8_t>(local));
    } else {
        size_t name_idx = current_chunk().add_constant(Value::make_string(expr.name()));
        emit_opcode(OpCode::OP_SET_GLOBAL);
        emit_byte(static_cast<uint8_t>(name_idx));
    }
}

void BytecodeCompiler::visit_range(const RangeExpr& expr) {
    line_ = expr.span().start.line;
    // Call builtin range()
    size_t name_idx = current_chunk().add_constant(Value::make_string("range"));
    emit_opcode(OpCode::OP_GET_GLOBAL);
    emit_byte(static_cast<uint8_t>(name_idx));

    if (expr.start()) expr.start()->accept(*this);
    else emit_constant(Value::make_int(0));

    if (expr.end()) expr.end()->accept(*this);
    else emit_constant(Value::make_int(0));

    emit_opcode(OpCode::OP_CALL);
    emit_byte(2);
}

void BytecodeCompiler::visit_lambda(const LambdaExpr& expr) {
    line_ = expr.span().start.line;

    CompilerContext lambda_ctx;
    lambda_ctx.function = std::make_shared<CompiledFunction>("anonymous", expr.params().size());
    lambda_ctx.enclosing = current_ctx_;
    lambda_ctx.locals.push_back({"", 0, false});

    for (const auto& p : expr.params()) {
        lambda_ctx.locals.push_back({p, 0, true});
    }

    auto* prev = current_ctx_;
    current_ctx_ = &lambda_ctx;

    if (expr.body_expr()) {
        expr.body_expr()->accept(*this);
        emit_opcode(OpCode::OP_RETURN);
    } else if (expr.body_block()) {
        for (const auto& s : expr.body_block()->statements()) {
            s->accept(*this);
        }
        emit_opcode(OpCode::OP_NIL);
        emit_opcode(OpCode::OP_RETURN);
    }

    current_ctx_ = prev;
    emit_constant(Value::make_compiled_fn(lambda_ctx.function));
}

void BytecodeCompiler::visit_expr_stmt(const ExprStmt& stmt) {
    line_ = stmt.span().start.line;
    stmt.expr().accept(*this);
    emit_opcode(OpCode::OP_POP);
}

void BytecodeCompiler::visit_let_stmt(const LetStmt& stmt) {
    line_ = stmt.span().start.line;
    if (stmt.initializer()) {
        stmt.initializer()->accept(*this);
    } else {
        emit_opcode(OpCode::OP_NIL);
    }

    if (current_ctx_->scope_depth > 0) {
        add_local(stmt.name(), stmt.is_mut());
    } else {
        size_t name_idx = current_chunk().add_constant(Value::make_string(stmt.name()));
        emit_opcode(OpCode::OP_DEFINE_GLOBAL);
        emit_byte(static_cast<uint8_t>(name_idx));
    }
}

void BytecodeCompiler::visit_block_stmt(const BlockStmt& stmt) {
    line_ = stmt.span().start.line;
    begin_scope();
    for (const auto& s : stmt.statements()) {
        s->accept(*this);
    }
    end_scope();
}

void BytecodeCompiler::visit_if_stmt(const IfStmt& stmt) {
    line_ = stmt.span().start.line;
    stmt.condition().accept(*this);

    size_t then_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
    emit_opcode(OpCode::OP_POP); // Pop condition
    stmt.then_branch().accept(*this);

    size_t else_jump = emit_jump(OpCode::OP_JUMP);
    patch_jump(then_jump);
    emit_opcode(OpCode::OP_POP); // Pop condition

    if (stmt.else_branch()) {
        stmt.else_branch()->accept(*this);
    }

    patch_jump(else_jump);
}

void BytecodeCompiler::visit_while_stmt(const WhileStmt& stmt) {
    line_ = stmt.span().start.line;
    size_t loop_start = current_chunk().count();
    current_ctx_->loop_starts.push_back(loop_start);
    current_ctx_->break_jumps.push_back({});

    stmt.condition().accept(*this);
    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
    emit_opcode(OpCode::OP_POP);

    stmt.body().accept(*this);
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_opcode(OpCode::OP_POP);

    for (size_t bj : current_ctx_->break_jumps.back()) {
        patch_jump(bj);
    }

    current_ctx_->loop_starts.pop_back();
    current_ctx_->break_jumps.pop_back();
}

void BytecodeCompiler::visit_for_in_stmt(const ForInStmt& stmt) {
    line_ = stmt.span().start.line;
    // Scope for iterable and loop index
    begin_scope();

    // 1. Evaluate iterable
    stmt.iterable().accept(*this);
    add_local("$iter", false);

    // 2. Local index = 0
    emit_constant(Value::make_int(0));
    add_local("$idx", true);

    size_t loop_start = current_chunk().count();
    current_ctx_->loop_starts.push_back(loop_start);
    current_ctx_->break_jumps.push_back({});

    // Condition: $idx < len($iter)
    size_t len_fn_idx = current_chunk().add_constant(Value::make_string("len"));
    emit_opcode(OpCode::OP_GET_GLOBAL);
    emit_byte(static_cast<uint8_t>(len_fn_idx));

    int iter_local = resolve_local(current_ctx_, "$iter");
    emit_opcode(OpCode::OP_GET_LOCAL);
    emit_byte(static_cast<uint8_t>(iter_local));
    emit_opcode(OpCode::OP_CALL);
    emit_byte(1);

    int idx_local = resolve_local(current_ctx_, "$idx");
    emit_opcode(OpCode::OP_GET_LOCAL);
    emit_byte(static_cast<uint8_t>(idx_local));

    emit_opcode(OpCode::OP_GREATER); // len > idx

    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
    emit_opcode(OpCode::OP_POP);

    // Fetch item: $iter[$idx] -> bind to variable_name
    begin_scope();
    emit_opcode(OpCode::OP_GET_LOCAL);
    emit_byte(static_cast<uint8_t>(iter_local));
    emit_opcode(OpCode::OP_GET_LOCAL);
    emit_byte(static_cast<uint8_t>(idx_local));
    emit_opcode(OpCode::OP_INDEX_GET);
    add_local(stmt.variable_name(), true);

    stmt.body().accept(*this);
    end_scope();

    // Increment index: $idx += 1
    emit_opcode(OpCode::OP_GET_LOCAL);
    emit_byte(static_cast<uint8_t>(idx_local));
    emit_constant(Value::make_int(1));
    emit_opcode(OpCode::OP_ADD);
    emit_opcode(OpCode::OP_SET_LOCAL);
    emit_byte(static_cast<uint8_t>(idx_local));
    emit_opcode(OpCode::OP_POP);

    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_opcode(OpCode::OP_POP);

    for (size_t bj : current_ctx_->break_jumps.back()) {
        patch_jump(bj);
    }

    current_ctx_->loop_starts.pop_back();
    current_ctx_->break_jumps.pop_back();
    end_scope();
}

void BytecodeCompiler::visit_return_stmt(const ReturnStmt& stmt) {
    line_ = stmt.span().start.line;
    if (stmt.value()) {
        stmt.value()->accept(*this);
    } else {
        emit_opcode(OpCode::OP_NIL);
    }
    emit_opcode(OpCode::OP_RETURN);
}

void BytecodeCompiler::visit_break_stmt(const BreakStmt&) {
    if (current_ctx_->break_jumps.empty()) {
        diagnostics_.error("break statement outside loop", SourceSpan{});
        return;
    }
    size_t jump = emit_jump(OpCode::OP_JUMP);
    current_ctx_->break_jumps.back().push_back(jump);
}

void BytecodeCompiler::visit_continue_stmt(const ContinueStmt&) {
    if (current_ctx_->loop_starts.empty()) {
        diagnostics_.error("continue statement outside loop", SourceSpan{});
        return;
    }
    emit_loop(current_ctx_->loop_starts.back());
}

void BytecodeCompiler::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    line_ = stmt.span().start.line;

    CompilerContext fn_ctx;
    fn_ctx.function = std::make_shared<CompiledFunction>(stmt.name(), stmt.params().size());
    fn_ctx.enclosing = current_ctx_;
    fn_ctx.locals.push_back({stmt.name(), 0, false});

    for (const auto& p : stmt.params()) {
        fn_ctx.locals.push_back({p.name, 0, true});
    }

    auto* prev = current_ctx_;
    current_ctx_ = &fn_ctx;

    if (stmt.is_arrow_body()) {
        stmt.expr_body()->accept(*this);
        emit_opcode(OpCode::OP_RETURN);
    } else if (stmt.body()) {
        for (const auto& s : stmt.body()->statements()) {
            s->accept(*this);
        }
        emit_opcode(OpCode::OP_NIL);
        emit_opcode(OpCode::OP_RETURN);
    }

    current_ctx_ = prev;

    emit_constant(Value::make_compiled_fn(fn_ctx.function));
    if (current_ctx_->scope_depth > 0) {
        add_local(stmt.name(), false);
    } else {
        size_t name_idx = current_chunk().add_constant(Value::make_string(stmt.name()));
        emit_opcode(OpCode::OP_DEFINE_GLOBAL);
        emit_byte(static_cast<uint8_t>(name_idx));
    }
}

} // namespace nextviper
