#include "nextviper/ir.hpp"
#include <sstream>
#include <iomanip>
#include <set>
#include <unordered_set>

namespace nextviper {

std::string IROperand::to_string() const {
    switch (kind) {
        case OperandKind::REGISTER:
            return "%r" + std::to_string(reg_id);
        case OperandKind::CONSTANT_INT:
            return std::to_string(int_val);
        case OperandKind::CONSTANT_FLOAT: {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(4) << float_val;
            return ss.str();
        }
        case OperandKind::CONSTANT_BOOL:
            return bool_val ? "true" : "false";
        case OperandKind::CONSTANT_STRING:
            return "\"" + str_val + "\"";
        case OperandKind::SYMBOL:
            return "@" + str_val;
        case OperandKind::LABEL:
            return "$" + str_val;
    }
    return "?";
}

std::string IRInstruction::to_string() const {
    std::ostringstream ss;
    if (dest_reg >= 0) {
        ss << "  %r" << dest_reg << " : " << ir_type_to_string(type) << " = ";
    } else {
        ss << "  ";
    }
    ss << ir_opcode_to_string(opcode);
    for (size_t i = 0; i < args.size(); ++i) {
        ss << (i == 0 ? " " : ", ") << args[i].to_string();
    }
    return ss.str();
}

std::string IRBasicBlock::to_string() const {
    std::ostringstream ss;
    ss << label << ":\n";
    for (const auto& inst : instructions) {
        ss << inst.to_string() << "\n";
    }
    return ss.str();
}

IRBasicBlock* IRFunction::create_block(std::string label) {
    auto bb = std::make_unique<IRBasicBlock>();
    bb->label = std::move(label);
    auto ptr = bb.get();
    blocks.push_back(std::move(bb));
    return ptr;
}

std::string IRFunction::to_string() const {
    std::ostringstream ss;
    ss << "fn @" << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << params[i].first << ": " << ir_type_to_string(params[i].second);
    }
    ss << ") -> " << ir_type_to_string(return_type) << " {\n";
    for (const auto& b : blocks) {
        ss << b->to_string();
    }
    ss << "}\n";
    return ss.str();
}

IRFunction* IRModule::create_function(std::string name, IRTypeKind ret_type) {
    auto fn = std::make_unique<IRFunction>();
    fn->name = std::move(name);
    fn->return_type = ret_type;
    auto ptr = fn.get();
    functions.push_back(std::move(fn));
    return ptr;
}

std::string IRModule::to_string() const {
    std::ostringstream ss;
    ss << "; ModuleID = '" << name << "'\n\n";
    for (const auto& fn : functions) {
        ss << fn->to_string() << "\n";
    }
    return ss.str();
}

// --- IRGenerator Implementation ---

IRGenerator::IRGenerator(DiagnosticEngine& diag) : diag_(diag) {}

int IRGenerator::alloc_reg() {
    return next_reg_++;
}

std::string IRGenerator::new_block_label(const std::string& prefix) {
    return prefix + "_" + std::to_string(next_block_id_++);
}

void IRGenerator::emit(IROpcode op, int dest, IRTypeKind type, std::vector<IROperand> args, SourceSpan span) {
    if (!current_block_) return;
    IRInstruction inst;
    inst.opcode = op;
    inst.dest_reg = dest;
    inst.type = type;
    inst.args = std::move(args);
    inst.span = span;
    current_block_->instructions.push_back(std::move(inst));
}

std::unique_ptr<IRModule> IRGenerator::generate(const Program& program) {
    module_ = std::make_unique<IRModule>();
    module_->name = "main_module";

    // Create main entry function
    current_func_ = module_->create_function("__nv_entry_main", IRTypeKind::INT64);
    current_block_ = current_func_->create_block("entry");

    for (const auto& stmt : program.statements()) {
        if (stmt) stmt->accept(*this);
    }

    // Emit default return 0 if not terminated
    if (current_block_ && (current_block_->instructions.empty() || current_block_->instructions.back().opcode != IROpcode::RET)) {
        int r = alloc_reg();
        emit(IROpcode::CONST_INT, r, IRTypeKind::INT64, {IROperand::make_int(0)});
        emit(IROpcode::RET, -1, IRTypeKind::VOID, {IROperand::make_reg(r)});
    }

    return std::move(module_);
}

void IRGenerator::visit_let_stmt(const LetStmt& stmt) {
    int alloca_reg = alloc_reg();
    IRTypeKind var_type = IRTypeKind::INT64;

    if (stmt.initializer()) {
        stmt.initializer()->accept(*this);
        var_type = last_type_;
    }

    emit(IROpcode::ALLOCA, alloca_reg, var_type, {IROperand::make_symbol(stmt.name())}, stmt.span());
    var_allocas_[stmt.name()] = alloca_reg;
    var_types_[stmt.name()] = var_type;

    if (stmt.initializer()) {
        emit(IROpcode::STORE, -1, var_type, {IROperand::make_reg(alloca_reg), last_operand_}, stmt.span());
    }
}

void IRGenerator::visit_expr_stmt(const ExprStmt& stmt) {
    stmt.expr().accept(*this);
}

void IRGenerator::visit_block_stmt(const BlockStmt& stmt) {
    for (const auto& s : stmt.statements()) {
        if (s) s->accept(*this);
    }
}

void IRGenerator::visit_if_stmt(const IfStmt& stmt) {
    std::string then_lbl = new_block_label("if_then");
    std::string else_lbl = new_block_label("if_else");
    std::string merge_lbl = new_block_label("if_merge");

    stmt.condition().accept(*this);
    emit(IROpcode::JMP_IF_TRUE, -1, IRTypeKind::VOID, {last_operand_, IROperand::make_label(then_lbl)}, stmt.span());
    emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(stmt.else_branch() ? else_lbl : merge_lbl)}, stmt.span());

    // Then block
    current_block_ = current_func_->create_block(then_lbl);
    stmt.then_branch().accept(*this);
    emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(merge_lbl)});

    // Else block
    if (stmt.else_branch()) {
        current_block_ = current_func_->create_block(else_lbl);
        stmt.else_branch()->accept(*this);
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(merge_lbl)});
    }

    current_block_ = current_func_->create_block(merge_lbl);
}

void IRGenerator::visit_while_stmt(const WhileStmt& stmt) {
    std::string cond_lbl = new_block_label("while_cond");
    std::string body_lbl = new_block_label("while_body");
    std::string exit_lbl = new_block_label("while_exit");

    emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

    current_block_ = current_func_->create_block(cond_lbl);
    stmt.condition().accept(*this);
    emit(IROpcode::JMP_IF_TRUE, -1, IRTypeKind::VOID, {last_operand_, IROperand::make_label(body_lbl)}, stmt.span());
    emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(exit_lbl)});

    current_block_ = current_func_->create_block(body_lbl);
    stmt.body().accept(*this);
    emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

    current_block_ = current_func_->create_block(exit_lbl);
}

void IRGenerator::visit_for_in_stmt(const ForInStmt& stmt) {
    int var_reg = alloc_reg();
    var_allocas_[stmt.variable_name()] = var_reg;
    var_types_[stmt.variable_name()] = IRTypeKind::INT64;
    emit(IROpcode::ALLOCA, var_reg, IRTypeKind::INT64, {IROperand::make_symbol(stmt.variable_name())}, stmt.span());

    const RangeExpr* range_expr = dynamic_cast<const RangeExpr*>(&stmt.iterable());
    const CallExpr* call_expr = dynamic_cast<const CallExpr*>(&stmt.iterable());
    bool is_range_call = false;
    if (call_expr) {
        if (auto id = dynamic_cast<const IdentifierExpr*>(&call_expr->callee())) {
            if (id->name() == "range") is_range_call = true;
        }
    }

    if (range_expr || is_range_call) {
        IROperand start_op = IROperand::make_int(0);
        IROperand end_op = IROperand::make_int(0);
        bool inclusive = false;

        if (range_expr) {
            if (range_expr->start()) {
                range_expr->start()->accept(*this);
                start_op = last_operand_;
            }
            if (range_expr->end()) {
                range_expr->end()->accept(*this);
                end_op = last_operand_;
            }
            inclusive = range_expr->inclusive();
        } else if (call_expr) {
            if (call_expr->args().size() == 1) {
                call_expr->args()[0]->accept(*this);
                end_op = last_operand_;
            } else if (call_expr->args().size() >= 2) {
                call_expr->args()[0]->accept(*this);
                start_op = last_operand_;
                call_expr->args()[1]->accept(*this);
                end_op = last_operand_;
            }
        }

        // Store start into var
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(var_reg), start_op}, stmt.span());

        std::string cond_lbl = new_block_label("for_cond");
        std::string body_lbl = new_block_label("for_body");
        std::string step_lbl = new_block_label("for_step");
        std::string exit_lbl = new_block_label("for_exit");

        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

        // Cond block
        current_block_ = current_func_->create_block(cond_lbl);
        int cur_val_reg = alloc_reg();
        emit(IROpcode::LOAD, cur_val_reg, IRTypeKind::INT64, {IROperand::make_reg(var_reg)}, stmt.span());
        int cmp_reg = alloc_reg();
        emit(inclusive ? IROpcode::LE : IROpcode::LT, cmp_reg, IRTypeKind::BOOL, {IROperand::make_reg(cur_val_reg), end_op}, stmt.span());
        emit(IROpcode::JMP_IF_TRUE, -1, IRTypeKind::VOID, {IROperand::make_reg(cmp_reg), IROperand::make_label(body_lbl)}, stmt.span());
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(exit_lbl)});

        // Body block
        current_block_ = current_func_->create_block(body_lbl);
        stmt.body().accept(*this);
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(step_lbl)});

        // Step block: var = var + 1
        current_block_ = current_func_->create_block(step_lbl);
        int step_cur_reg = alloc_reg();
        emit(IROpcode::LOAD, step_cur_reg, IRTypeKind::INT64, {IROperand::make_reg(var_reg)}, stmt.span());
        int inc_reg = alloc_reg();
        emit(IROpcode::ADD, inc_reg, IRTypeKind::INT64, {IROperand::make_reg(step_cur_reg), IROperand::make_int(1)}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(var_reg), IROperand::make_reg(inc_reg)}, stmt.span());
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

        current_block_ = current_func_->create_block(exit_lbl);
    } else {
        stmt.iterable().accept(*this);
        IROperand arr_op = last_operand_;

        int idx_reg = alloc_reg();
        emit(IROpcode::ALLOCA, idx_reg, IRTypeKind::INT64, {IROperand::make_symbol("_idx")}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(idx_reg), IROperand::make_int(0)}, stmt.span());

        std::string cond_lbl = new_block_label("for_arr_cond");
        std::string body_lbl = new_block_label("for_arr_body");
        std::string step_lbl = new_block_label("for_arr_step");
        std::string exit_lbl = new_block_label("for_arr_exit");

        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

        current_block_ = current_func_->create_block(cond_lbl);
        int cur_idx_reg = alloc_reg();
        emit(IROpcode::LOAD, cur_idx_reg, IRTypeKind::INT64, {IROperand::make_reg(idx_reg)}, stmt.span());
        int len_reg = alloc_reg();
        emit(IROpcode::CALL, len_reg, IRTypeKind::INT64, {IROperand::make_symbol("len"), arr_op}, stmt.span());
        int cmp_reg = alloc_reg();
        emit(IROpcode::LT, cmp_reg, IRTypeKind::BOOL, {IROperand::make_reg(cur_idx_reg), IROperand::make_reg(len_reg)}, stmt.span());
        emit(IROpcode::JMP_IF_TRUE, -1, IRTypeKind::VOID, {IROperand::make_reg(cmp_reg), IROperand::make_label(body_lbl)}, stmt.span());
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(exit_lbl)});

        current_block_ = current_func_->create_block(body_lbl);
        int item_reg = alloc_reg();
        emit(IROpcode::CALL, item_reg, IRTypeKind::INT64, {IROperand::make_symbol("get"), arr_op, IROperand::make_reg(cur_idx_reg)}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(var_reg), IROperand::make_reg(item_reg)}, stmt.span());
        stmt.body().accept(*this);
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(step_lbl)});

        current_block_ = current_func_->create_block(step_lbl);
        int step_cur_idx = alloc_reg();
        emit(IROpcode::LOAD, step_cur_idx, IRTypeKind::INT64, {IROperand::make_reg(idx_reg)}, stmt.span());
        int next_idx = alloc_reg();
        emit(IROpcode::ADD, next_idx, IRTypeKind::INT64, {IROperand::make_reg(step_cur_idx), IROperand::make_int(1)}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(idx_reg), IROperand::make_reg(next_idx)}, stmt.span());
        emit(IROpcode::JMP, -1, IRTypeKind::VOID, {IROperand::make_label(cond_lbl)});

        current_block_ = current_func_->create_block(exit_lbl);
    }
}

void IRGenerator::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    auto saved_func = current_func_;
    auto saved_block = current_block_;
    auto saved_allocas = var_allocas_;
    auto saved_var_types = var_types_;

    current_func_ = module_->create_function(stmt.name(), IRTypeKind::INT64);
    current_block_ = current_func_->create_block("entry");

    for (const auto& p : stmt.params()) {
        current_func_->params.push_back({p.name, IRTypeKind::INT64});
        int reg = alloc_reg();
        emit(IROpcode::ALLOCA, reg, IRTypeKind::INT64, {IROperand::make_symbol(p.name)}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(reg), IROperand::make_symbol(p.name)}, stmt.span());
        var_allocas_[p.name] = reg;
        var_types_[p.name] = IRTypeKind::INT64;
    }

    if (stmt.is_arrow_body() && stmt.expr_body()) {
        stmt.expr_body()->accept(*this);
        emit(IROpcode::RET, -1, last_type_, {last_operand_}, stmt.span());
    } else if (stmt.body()) {
        stmt.body()->accept(*this);
    }

    if (current_block_ && (current_block_->instructions.empty() || current_block_->instructions.back().opcode != IROpcode::RET)) {
        int r = alloc_reg();
        emit(IROpcode::CONST_INT, r, IRTypeKind::INT64, {IROperand::make_int(0)});
        emit(IROpcode::RET, -1, IRTypeKind::VOID, {IROperand::make_reg(r)});
    }

    current_func_ = saved_func;
    current_block_ = saved_block;
    var_allocas_ = saved_allocas;
    var_types_ = saved_var_types;
}

void IRGenerator::visit_return_stmt(const ReturnStmt& stmt) {
    if (stmt.value()) {
        stmt.value()->accept(*this);
        if (current_func_) {
            func_return_types_[current_func_->name] = last_type_;
        }
        emit(IROpcode::RET, -1, last_type_, {last_operand_}, stmt.span());
    } else {
        int r = alloc_reg();
        emit(IROpcode::CONST_INT, r, IRTypeKind::INT64, {IROperand::make_int(0)});
        emit(IROpcode::RET, -1, IRTypeKind::VOID, {IROperand::make_reg(r)}, stmt.span());
    }
}

void IRGenerator::visit_break_stmt(const BreakStmt&) {}
void IRGenerator::visit_continue_stmt(const ContinueStmt&) {}
void IRGenerator::visit_import_stmt(const ImportStmt&) {}
void IRGenerator::visit_export_stmt(const ExportStmt& stmt) {
    stmt.inner_stmt().accept(*this);
}

void IRGenerator::visit_literal(const LiteralExpr& expr) {
    int r = alloc_reg();
    switch (expr.kind()) {
        case LiteralExpr::Kind::INT:
            emit(IROpcode::CONST_INT, r, IRTypeKind::INT64, {IROperand::make_int(expr.int_value())}, expr.span());
            last_operand_ = IROperand::make_reg(r);
            last_type_ = IRTypeKind::INT64;
            break;
        case LiteralExpr::Kind::FLOAT:
            emit(IROpcode::CONST_FLOAT, r, IRTypeKind::FLOAT64, {IROperand::make_float(expr.float_value())}, expr.span());
            last_operand_ = IROperand::make_reg(r);
            last_type_ = IRTypeKind::FLOAT64;
            break;
        case LiteralExpr::Kind::BOOL:
            emit(IROpcode::CONST_BOOL, r, IRTypeKind::BOOL, {IROperand::make_bool(expr.bool_value())}, expr.span());
            last_operand_ = IROperand::make_reg(r);
            last_type_ = IRTypeKind::BOOL;
            break;
        case LiteralExpr::Kind::STRING:
            emit(IROpcode::CONST_STRING, r, IRTypeKind::STRING, {IROperand::make_str(expr.string_value())}, expr.span());
            last_operand_ = IROperand::make_reg(r);
            last_type_ = IRTypeKind::STRING;
            break;
        case LiteralExpr::Kind::NIL:
            emit(IROpcode::CONST_NIL, r, IRTypeKind::PTR, {}, expr.span());
            last_operand_ = IROperand::make_reg(r);
            last_type_ = IRTypeKind::PTR;
            break;
    }
}

void IRGenerator::visit_identifier(const IdentifierExpr& expr) {
    auto it = var_allocas_.find(expr.name());
    if (it != var_allocas_.end()) {
        int r = alloc_reg();
        IRTypeKind t = var_types_[expr.name()];
        emit(IROpcode::LOAD, r, t, {IROperand::make_reg(it->second)}, expr.span());
        last_operand_ = IROperand::make_reg(r);
        last_type_ = t;
    } else {
        last_operand_ = IROperand::make_symbol("(int64_t)(uintptr_t)nv_fn_" + expr.name());
        last_type_ = IRTypeKind::PTR;
    }
}

void IRGenerator::visit_assign(const AssignExpr& expr) {
    if (expr.is_index_assign() && expr.index_target()) {
        expr.index_target()->target().accept(*this);
        IROperand target_op = last_operand_;
        expr.index_target()->index().accept(*this);
        IROperand index_op = last_operand_;
        expr.value().accept(*this);
        IROperand val_op = last_operand_;
        int r = alloc_reg();
        emit(IROpcode::CALL, r, IRTypeKind::INT64, {IROperand::make_symbol("set"), target_op, index_op, val_op}, expr.span());
        last_operand_ = IROperand::make_reg(r);
        return;
    }

    expr.value().accept(*this);
    IROperand rhs_val = last_operand_;
    IRTypeKind rhs_type = last_type_;

    auto it = var_allocas_.find(expr.name());
    if (it != var_allocas_.end()) {
        if (expr.op() == TokenType::PLUS_ASSIGN || expr.op() == TokenType::MINUS_ASSIGN ||
            expr.op() == TokenType::STAR_ASSIGN || expr.op() == TokenType::SLASH_ASSIGN ||
            expr.op() == TokenType::PERCENT_ASSIGN) {
            int cur_r = alloc_reg();
            emit(IROpcode::LOAD, cur_r, var_types_[expr.name()], {IROperand::make_reg(it->second)}, expr.span());
            int res_r = alloc_reg();
            IROpcode bin_op = IROpcode::ADD;
            if (expr.op() == TokenType::MINUS_ASSIGN) bin_op = IROpcode::SUB;
            else if (expr.op() == TokenType::STAR_ASSIGN) bin_op = IROpcode::MUL;
            else if (expr.op() == TokenType::SLASH_ASSIGN) bin_op = IROpcode::DIV;
            else if (expr.op() == TokenType::PERCENT_ASSIGN) bin_op = IROpcode::MOD;
            emit(bin_op, res_r, var_types_[expr.name()], {IROperand::make_reg(cur_r), rhs_val}, expr.span());
            rhs_val = IROperand::make_reg(res_r);
        }
        emit(IROpcode::STORE, -1, var_types_[expr.name()], {IROperand::make_reg(it->second), rhs_val}, expr.span());
    } else {
        int alloca_reg = alloc_reg();
        emit(IROpcode::ALLOCA, alloca_reg, rhs_type, {IROperand::make_symbol(expr.name())}, expr.span());
        var_allocas_[expr.name()] = alloca_reg;
        var_types_[expr.name()] = rhs_type;
        emit(IROpcode::STORE, -1, rhs_type, {IROperand::make_reg(alloca_reg), rhs_val}, expr.span());
    }
}

void IRGenerator::visit_binary(const BinaryExpr& expr) {
    expr.left().accept(*this);
    IROperand left_op = last_operand_;
    IRTypeKind left_t = last_type_;

    expr.right().accept(*this);
    IROperand right_op = last_operand_;

    int r = alloc_reg();
    IROpcode op = IROpcode::ADD;

    switch (expr.op()) {
        case TokenType::PLUS: op = IROpcode::ADD; break;
        case TokenType::MINUS: op = IROpcode::SUB; break;
        case TokenType::STAR: op = IROpcode::MUL; break;
        case TokenType::SLASH: op = IROpcode::DIV; break;
        case TokenType::PERCENT: op = IROpcode::MOD; break;
        case TokenType::EQUAL_EQUAL: op = IROpcode::EQ; break;
        case TokenType::BANG_EQUAL: op = IROpcode::NE; break;
        case TokenType::LESS: op = IROpcode::LT; break;
        case TokenType::LESS_EQUAL: op = IROpcode::LE; break;
        case TokenType::GREATER: op = IROpcode::GT; break;
        case TokenType::GREATER_EQUAL: op = IROpcode::GE; break;
        default: op = IROpcode::ADD; break;
    }

    emit(op, r, left_t, {left_op, right_op}, expr.span());
    last_operand_ = IROperand::make_reg(r);
    last_type_ = left_t;
}

void IRGenerator::visit_unary(const UnaryExpr& expr) {
    expr.operand().accept(*this);
    int r = alloc_reg();
    IROpcode op = (expr.op() == TokenType::MINUS) ? IROpcode::NEG : IROpcode::NOT;
    emit(op, r, last_type_, {last_operand_}, expr.span());
    last_operand_ = IROperand::make_reg(r);
}

void IRGenerator::visit_call(const CallExpr& expr) {
    if (auto id = dynamic_cast<const IdentifierExpr*>(&expr.callee())) {
        if (id->name() == "print") {
            std::vector<IROperand> print_args;
            for (const auto& a : expr.args()) {
                a->accept(*this);
                print_args.push_back(last_operand_);
            }
            emit(IROpcode::PRINT, -1, IRTypeKind::VOID, std::move(print_args), expr.span());
            last_operand_ = IROperand::make_int(0);
            last_type_ = IRTypeKind::VOID;
            return;
        }
    }

    std::vector<IROperand> call_args;
    if (auto id = dynamic_cast<const IdentifierExpr*>(&expr.callee())) {
        auto it = var_allocas_.find(id->name());
        if (it != var_allocas_.end()) {
            int r = alloc_reg();
            emit(IROpcode::LOAD, r, var_types_[id->name()], {IROperand::make_reg(it->second)}, expr.span());
            call_args.push_back(IROperand::make_reg(r));
        } else {
            call_args.push_back(IROperand::make_symbol(id->name()));
        }
    } else if (auto idx_expr = dynamic_cast<const IndexExpr*>(&expr.callee())) {
        if (auto mem_lit = dynamic_cast<const LiteralExpr*>(&idx_expr->index())) {
            std::string method_name = mem_lit->string_value();
            call_args.push_back(IROperand::make_symbol(method_name));
            idx_expr->target().accept(*this);
            call_args.push_back(last_operand_);
        } else {
            expr.callee().accept(*this);
            call_args.push_back(last_operand_);
        }
    } else {
        expr.callee().accept(*this);
        call_args.push_back(last_operand_);
    }

    for (const auto& a : expr.args()) {
        a->accept(*this);
        call_args.push_back(last_operand_);
    }

    IRTypeKind ret_type = IRTypeKind::INT64;
    if (auto id = dynamic_cast<const IdentifierExpr*>(&expr.callee())) {
        if (id->name() == "clock") ret_type = IRTypeKind::FLOAT64;
        else if (id->name() == "array_new" || id->name() == "object_new" || id->name() == "range" || id->name() == "push" || id->name() == "append" || id->name() == "insert") {
            ret_type = IRTypeKind::PTR;
        } else if (func_return_types_.count(id->name())) {
            ret_type = func_return_types_[id->name()];
        }
    } else if (auto idx_expr = dynamic_cast<const IndexExpr*>(&expr.callee())) {
        if (auto mem_lit = dynamic_cast<const LiteralExpr*>(&idx_expr->index())) {
            std::string m = mem_lit->string_value();
            if (m == "push" || m == "append" || m == "insert" || m == "slice" || m == "keys" || m == "values") {
                ret_type = IRTypeKind::PTR;
            } else if (m == "len") {
                ret_type = IRTypeKind::INT64;
            }
        }
    }

    int r = alloc_reg();
    emit(IROpcode::CALL, r, ret_type, std::move(call_args), expr.span());
    last_operand_ = IROperand::make_reg(r);
    last_type_ = ret_type;
}

void IRGenerator::visit_index(const IndexExpr& expr) {
    expr.target().accept(*this);
    IROperand target_op = last_operand_;
    expr.index().accept(*this);
    IROperand index_op = last_operand_;
    IRTypeKind res_type = (index_op.kind == OperandKind::CONSTANT_STRING || last_type_ == IRTypeKind::STRING) ? IRTypeKind::PTR : IRTypeKind::INT64;
    int r = alloc_reg();
    emit(IROpcode::CALL, r, res_type, {IROperand::make_symbol("get"), target_op, index_op}, expr.span());
    last_operand_ = IROperand::make_reg(r);
    last_type_ = res_type;
}

void IRGenerator::visit_slice(const SliceExpr&) {}

void IRGenerator::visit_array(const ArrayExpr& expr) {
    int arr_reg = alloc_reg();
    emit(IROpcode::CALL, arr_reg, IRTypeKind::PTR, {IROperand::make_symbol("array_new")}, expr.span());
    for (const auto& elem : expr.elements()) {
        elem->accept(*this);
        int push_res = alloc_reg();
        emit(IROpcode::CALL, push_res, IRTypeKind::INT64, {IROperand::make_symbol("push"), IROperand::make_reg(arr_reg), last_operand_}, expr.span());
    }
    last_operand_ = IROperand::make_reg(arr_reg);
    last_type_ = IRTypeKind::PTR;
}

void IRGenerator::visit_object(const ObjectExpr& expr) {
    int obj_reg = alloc_reg();
    emit(IROpcode::CALL, obj_reg, IRTypeKind::PTR, {IROperand::make_symbol("object_new")}, expr.span());
    for (const auto& entry : expr.entries()) {
        entry.second->accept(*this);
        int set_res = alloc_reg();
        emit(IROpcode::CALL, set_res, IRTypeKind::INT64, {
            IROperand::make_symbol("set_prop"),
            IROperand::make_reg(obj_reg),
            IROperand::make_str(entry.first),
            last_operand_
        }, expr.span());
    }
    last_operand_ = IROperand::make_reg(obj_reg);
    last_type_ = IRTypeKind::PTR;
}
void IRGenerator::visit_pipe(const PipeExpr&) {}

void IRGenerator::visit_range(const RangeExpr& expr) {
    IROperand start_op = IROperand::make_int(0);
    IROperand end_op = IROperand::make_int(0);
    if (expr.start()) {
        expr.start()->accept(*this);
        start_op = last_operand_;
    }
    if (expr.end()) {
        expr.end()->accept(*this);
        end_op = last_operand_;
    }
    int r = alloc_reg();
    emit(IROpcode::CALL, r, IRTypeKind::PTR, {IROperand::make_symbol("range"), start_op, end_op}, expr.span());
    last_operand_ = IROperand::make_reg(r);
    last_type_ = IRTypeKind::PTR;
}

void IRGenerator::visit_lambda(const LambdaExpr&) {}

// --- IROptimizer Implementation ---

void IROptimizer::optimize(IRModule& module) {
    for (auto& fn : module.functions) {
        bool changed = true;
        int max_passes = 10;
        while (changed && max_passes-- > 0) {
            changed = false;
            changed |= run_constant_folding(*fn);
            changed |= run_dead_code_elimination(*fn);
        }
    }
}

bool IROptimizer::run_constant_folding(IRFunction& func) {
    bool changed = false;
    std::map<int, int64_t> const_ints;
    std::map<int, double> const_floats;

    for (auto& bb : func.blocks) {
        for (auto& inst : bb->instructions) {
            if (inst.opcode == IROpcode::CONST_INT && inst.args.size() == 1 && inst.args[0].kind == OperandKind::CONSTANT_INT) {
                const_ints[inst.dest_reg] = inst.args[0].int_val;
            } else if (inst.opcode == IROpcode::CONST_FLOAT && inst.args.size() == 1 && inst.args[0].kind == OperandKind::CONSTANT_FLOAT) {
                const_floats[inst.dest_reg] = inst.args[0].float_val;
            }

            if (inst.args.size() == 2 && inst.args[0].kind == OperandKind::REGISTER && inst.args[1].kind == OperandKind::REGISTER) {
                int r1 = inst.args[0].reg_id;
                int r2 = inst.args[1].reg_id;

                if (const_ints.count(r1) && const_ints.count(r2)) {
                    int64_t v1 = const_ints[r1];
                    int64_t v2 = const_ints[r2];
                    int64_t res = 0;
                    bool fold = true;

                    switch (inst.opcode) {
                        case IROpcode::ADD: res = v1 + v2; break;
                        case IROpcode::SUB: res = v1 - v2; break;
                        case IROpcode::MUL: res = v1 * v2; break;
                        case IROpcode::DIV: res = (v2 != 0) ? v1 / v2 : 0; break;
                        case IROpcode::EQ: res = (v1 == v2); break;
                        case IROpcode::NE: res = (v1 != v2); break;
                        case IROpcode::LT: res = (v1 < v2); break;
                        case IROpcode::LE: res = (v1 <= v2); break;
                        case IROpcode::GT: res = (v1 > v2); break;
                        case IROpcode::GE: res = (v1 >= v2); break;
                        default: fold = false; break;
                    }

                    if (fold) {
                        inst.opcode = IROpcode::CONST_INT;
                        inst.args = {IROperand::make_int(res)};
                        const_ints[inst.dest_reg] = res;
                        changed = true;
                    }
                }
            }
        }
    }
    return changed;
}

bool IROptimizer::run_dead_code_elimination(IRFunction& func) {
    bool changed = false;
    std::unordered_set<int> used_regs;

    for (const auto& bb : func.blocks) {
        for (const auto& inst : bb->instructions) {
            for (const auto& arg : inst.args) {
                if (arg.kind == OperandKind::REGISTER) {
                    used_regs.insert(arg.reg_id);
                }
            }
        }
    }

    for (auto& bb : func.blocks) {
        auto it = bb->instructions.begin();
        while (it != bb->instructions.end()) {
            if (it->dest_reg >= 0 && used_regs.find(it->dest_reg) == used_regs.end()) {
                if (it->opcode == IROpcode::CONST_INT || it->opcode == IROpcode::CONST_FLOAT ||
                    it->opcode == IROpcode::ADD || it->opcode == IROpcode::SUB ||
                    it->opcode == IROpcode::MUL || it->opcode == IROpcode::LOAD) {
                    it = bb->instructions.erase(it);
                    changed = true;
                    continue;
                }
            }
            ++it;
        }
    }

    return changed;
}

} // namespace nextviper
