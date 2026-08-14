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
    current_func_ = module_->create_function("main", IRTypeKind::INT64);
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
    stmt.body().accept(*this);
}

void IRGenerator::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    auto saved_func = current_func_;
    auto saved_block = current_block_;
    auto saved_allocas = var_allocas_;

    current_func_ = module_->create_function(stmt.name(), IRTypeKind::INT64);
    current_block_ = current_func_->create_block("entry");

    for (const auto& p : stmt.params()) {
        current_func_->params.push_back({p.name, IRTypeKind::INT64});
        int reg = alloc_reg();
        emit(IROpcode::ALLOCA, reg, IRTypeKind::INT64, {IROperand::make_symbol(p.name)}, stmt.span());
        emit(IROpcode::STORE, -1, IRTypeKind::INT64, {IROperand::make_reg(reg), IROperand::make_symbol(p.name)}, stmt.span());
        var_allocas_[p.name] = reg;
    }

    if (stmt.body()) stmt.body()->accept(*this);

    if (current_block_ && (current_block_->instructions.empty() || current_block_->instructions.back().opcode != IROpcode::RET)) {
        int r = alloc_reg();
        emit(IROpcode::CONST_INT, r, IRTypeKind::INT64, {IROperand::make_int(0)});
        emit(IROpcode::RET, -1, IRTypeKind::VOID, {IROperand::make_reg(r)});
    }

    current_func_ = saved_func;
    current_block_ = saved_block;
    var_allocas_ = saved_allocas;
}

void IRGenerator::visit_return_stmt(const ReturnStmt& stmt) {
    if (stmt.value()) {
        stmt.value()->accept(*this);
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
        last_operand_ = IROperand::make_symbol(expr.name());
        last_type_ = IRTypeKind::ANY;
    }
}

void IRGenerator::visit_assign(const AssignExpr& expr) {
    expr.value().accept(*this);
    auto it = var_allocas_.find(expr.name());
    if (it != var_allocas_.end()) {
        emit(IROpcode::STORE, -1, var_types_[expr.name()], {IROperand::make_reg(it->second), last_operand_}, expr.span());
    } else {
        int alloca_reg = alloc_reg();
        emit(IROpcode::ALLOCA, alloca_reg, last_type_, {IROperand::make_symbol(expr.name())}, expr.span());
        var_allocas_[expr.name()] = alloca_reg;
        var_types_[expr.name()] = last_type_;
        emit(IROpcode::STORE, -1, last_type_, {IROperand::make_reg(alloca_reg), last_operand_}, expr.span());
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
        call_args.push_back(IROperand::make_symbol(id->name()));
    } else {
        expr.callee().accept(*this);
        call_args.push_back(last_operand_);
    }

    for (const auto& a : expr.args()) {
        a->accept(*this);
        call_args.push_back(last_operand_);
    }

    int r = alloc_reg();
    emit(IROpcode::CALL, r, IRTypeKind::INT64, std::move(call_args), expr.span());
    last_operand_ = IROperand::make_reg(r);
    last_type_ = IRTypeKind::INT64;
}

void IRGenerator::visit_index(const IndexExpr&) {}
void IRGenerator::visit_slice(const SliceExpr&) {}
void IRGenerator::visit_array(const ArrayExpr&) {}
void IRGenerator::visit_object(const ObjectExpr&) {}
void IRGenerator::visit_pipe(const PipeExpr&) {}
void IRGenerator::visit_range(const RangeExpr&) {}
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
