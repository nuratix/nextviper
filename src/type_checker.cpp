#include "nextviper/type_checker.hpp"

namespace nextviper {

TypeChecker::TypeChecker(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    globals_ = TypeEnvironment::create();
    current_env_ = globals_;
    init_builtins();
}

void TypeChecker::init_builtins() {
    globals_->define("print", Type::make_function({Type::make_any()}, Type::make_void()), false);
    globals_->define("println", Type::make_function({Type::make_any()}, Type::make_void()), false);
    globals_->define("print_raw", Type::make_function({Type::make_any()}, Type::make_void()), false);
    globals_->define("len", Type::make_function({Type::make_any()}, Type::make_int()), false);
    globals_->define("typeof", Type::make_function({Type::make_any()}, Type::make_string()), false);
    globals_->define("range", Type::make_function({Type::make_int(), Type::make_int()}, Type::make_list(Type::make_int())), false);
    globals_->define("clock", Type::make_function({}, Type::make_float()), false);
    globals_->define("assert", Type::make_function({Type::make_bool(), Type::make_string()}, Type::make_void()), false);
    globals_->define("append", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_any()), false);
    globals_->define("push", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_any()), false);
    globals_->define("pop", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("remove", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_any()), false);
    globals_->define("insert", Type::make_function({Type::make_any(), Type::make_int(), Type::make_any()}, Type::make_any()), false);
    globals_->define("keys", Type::make_function({Type::make_any()}, Type::make_list(Type::make_string())), false);
    globals_->define("values", Type::make_function({Type::make_any()}, Type::make_list(Type::make_any())), false);
    globals_->define("entries", Type::make_function({Type::make_any()}, Type::make_list(Type::make_any())), false);
    globals_->define("contains", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_bool()), false);
    globals_->define("has", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_bool()), false);
    globals_->define("join", Type::make_function({Type::make_any(), Type::make_string()}, Type::make_string()), false);
    globals_->define("reverse", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("sort", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("map", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_list(Type::make_any())), false);
    globals_->define("filter", Type::make_function({Type::make_any(), Type::make_any()}, Type::make_list(Type::make_any())), false);
    globals_->define("reduce", Type::make_function({Type::make_any(), Type::make_any(), Type::make_any()}, Type::make_any()), false);
    globals_->define("min", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("max", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("abs", Type::make_function({Type::make_any()}, Type::make_any()), false);
    globals_->define("sqrt", Type::make_function({Type::make_float()}, Type::make_float()), false);
    globals_->define("pow", Type::make_function({Type::make_float(), Type::make_float()}, Type::make_float()), false);
    globals_->define("int", Type::make_function({Type::make_any()}, Type::make_int()), false);
    globals_->define("float", Type::make_function({Type::make_any()}, Type::make_float()), false);
    globals_->define("str", Type::make_function({Type::make_any()}, Type::make_string()), false);
    globals_->define("input", Type::make_function({Type::make_string()}, Type::make_string()), false);
}

bool TypeChecker::check(const Program& program) {
    for (const auto& stmt : program.statements()) {
        check_statement(*stmt);
    }
    return !diagnostics_.has_errors();
}

TypePtr TypeChecker::infer_expression(const Expr& expr) {
    expr.accept(*this);
    return last_inferred_type_ ? last_inferred_type_ : Type::make_unknown();
}

void TypeChecker::check_statement(const Stmt& stmt) {
    stmt.accept(*this);
}

void TypeChecker::visit_literal(const LiteralExpr& expr) {
    switch (expr.kind()) {
        case LiteralExpr::Kind::INT:
            last_inferred_type_ = Type::make_int();
            break;
        case LiteralExpr::Kind::FLOAT:
            last_inferred_type_ = Type::make_float();
            break;
        case LiteralExpr::Kind::STRING:
            last_inferred_type_ = Type::make_string();
            break;
        case LiteralExpr::Kind::BOOL:
            last_inferred_type_ = Type::make_bool();
            break;
        case LiteralExpr::Kind::NIL:
            last_inferred_type_ = Type::make_any();
            break;
    }
}

void TypeChecker::visit_identifier(const IdentifierExpr& expr) {
    auto type = current_env_->lookup(expr.name());
    if (!type) {
        diagnostics_.error("unknown identifier `" + expr.name() + "`", expr.span(),
                          "declare `" + expr.name() + "` before using it", "NV1001");
        last_inferred_type_ = Type::make_unknown();
        return;
    }
    last_inferred_type_ = type;
}

void TypeChecker::visit_unary(const UnaryExpr& expr) {
    TypePtr operand_type = infer_expression(expr.operand());

    switch (expr.op()) {
        case TokenType::BANG:
        case TokenType::KEYWORD_NOT:
            last_inferred_type_ = Type::make_bool();
            return;
        case TokenType::MINUS:
            if (operand_type->is_int()) {
                last_inferred_type_ = Type::make_int();
                return;
            }
            if (operand_type->is_float()) {
                last_inferred_type_ = Type::make_float();
                return;
            }
            if (operand_type->is_any() || operand_type->is_unknown()) {
                last_inferred_type_ = Type::make_any();
                return;
            }
            diagnostics_.error("TypeError: unary '-' requires numeric type, got '" + operand_type->to_string() + "'",
                               expr.span(), "ensure operand is an integer or float", "NV1003");
            last_inferred_type_ = Type::make_unknown();
            return;
        default:
            last_inferred_type_ = Type::make_any();
            return;
    }
}

void TypeChecker::visit_binary(const BinaryExpr& expr) {
    TypePtr left = infer_expression(expr.left());
    TypePtr right = infer_expression(expr.right());

    switch (expr.op()) {
        case TokenType::PLUS:
            if (left->is_string() || right->is_string()) {
                last_inferred_type_ = Type::make_string();
                return;
            }
            if (left->is_int() && right->is_int()) {
                last_inferred_type_ = Type::make_int();
                return;
            }
            if (left->is_numeric() && right->is_numeric()) {
                last_inferred_type_ = Type::make_float();
                return;
            }
            if (left->is_list() && right->is_list()) {
                last_inferred_type_ = left;
                return;
            }
            if (left->is_any() || right->is_any()) {
                last_inferred_type_ = Type::make_any();
                return;
            }
            diagnostics_.error("TypeError: operator '+' not supported for '" + left->to_string() + "' and '" + right->to_string() + "'",
                               expr.span(), "convert operand using str() or int()", "NV1003");
            last_inferred_type_ = Type::make_unknown();
            return;

        case TokenType::MINUS:
        case TokenType::STAR:
        case TokenType::SLASH:
        case TokenType::PERCENT:
        case TokenType::POWER:
            if (expr.op() == TokenType::STAR && left->is_string() && right->is_int()) {
                last_inferred_type_ = Type::make_string();
                return;
            }
            if (left->is_int() && right->is_int()) {
                if (expr.op() == TokenType::SLASH) {
                    last_inferred_type_ = Type::make_float();
                } else {
                    last_inferred_type_ = Type::make_int();
                }
                return;
            }
            if (left->is_numeric() && right->is_numeric()) {
                last_inferred_type_ = Type::make_float();
                return;
            }
            if (left->is_any() || right->is_any()) {
                last_inferred_type_ = Type::make_any();
                return;
            }
            diagnostics_.error("TypeError: operator requires numeric types, got '" + left->to_string() + "' and '" + right->to_string() + "'",
                               expr.span(), "ensure both operands are numeric", "NV1003");
            last_inferred_type_ = Type::make_unknown();
            return;

        case TokenType::EQUAL_EQUAL:
        case TokenType::BANG_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            last_inferred_type_ = Type::make_bool();
            return;

        case TokenType::AMP_AMP:
        case TokenType::KEYWORD_AND:
        case TokenType::PIPE_PIPE:
        case TokenType::KEYWORD_OR:
            last_inferred_type_ = Type::make_bool();
            return;

        default:
            last_inferred_type_ = Type::make_any();
            return;
    }
}

void TypeChecker::visit_call(const CallExpr& expr) {
    TypePtr callee_type = infer_expression(expr.callee());

    std::vector<TypePtr> arg_types;
    for (const auto& arg : expr.args()) {
        arg_types.push_back(infer_expression(*arg));
    }

    if (callee_type->is_function()) {
        const auto& param_types = callee_type->param_types();
        if (param_types.size() != arg_types.size() && !param_types.empty() && !param_types[0]->is_any()) {
            diagnostics_.error("TypeError: function expects " + std::to_string(param_types.size()) +
                               " argument(s), got " + std::to_string(arg_types.size()), expr.span(),
                               "adjust argument count to match function signature", "NV1004");
        } else {
            for (size_t i = 0; i < std::min(param_types.size(), arg_types.size()); ++i) {
                if (!param_types[i]->is_assignable_from(arg_types[i])) {
                    diagnostics_.error("TypeError: argument type mismatch for parameter " + std::to_string(i + 1) +
                                       ": expected '" + param_types[i]->to_string() + "', got '" + arg_types[i]->to_string() + "'",
                                       expr.args()[i]->span(), "pass value matching parameter type", "NV1004");
                }
            }
        }
        last_inferred_type_ = callee_type->return_type();
        return;
    }

    last_inferred_type_ = Type::make_any();
}

void TypeChecker::visit_index(const IndexExpr& expr) {
    TypePtr target_type = infer_expression(expr.target());
    TypePtr index_type = infer_expression(expr.index());

    if (target_type->is_list()) {
        if (index_type->is_string() || index_type->is_any()) {
            last_inferred_type_ = Type::make_any();
            return;
        }
        if (!index_type->is_int()) {
            diagnostics_.error("TypeError: list index must be an integer, got '" + index_type->to_string() + "'",
                               expr.index().span(), "use an integer index", "NV1003");
        }
        last_inferred_type_ = target_type->element_type();
        return;
    }

    if (target_type->is_map()) {
        if (!target_type->key_type()->is_assignable_from(index_type)) {
            diagnostics_.error("TypeError: map key type mismatch: expected '" + target_type->key_type()->to_string() +
                               "', got '" + index_type->to_string() + "'", expr.index().span(),
                               "use a string key", "NV1003");
        }
        last_inferred_type_ = target_type->value_type();
        return;
    }

    if (target_type->is_string()) {
        last_inferred_type_ = Type::make_string();
        return;
    }

    last_inferred_type_ = Type::make_any();
}

void TypeChecker::visit_slice(const SliceExpr& expr) {
    TypePtr target_type = infer_expression(expr.target());

    if (expr.start()) {
        TypePtr start_type = infer_expression(*expr.start());
        if (!start_type->is_int() && !start_type->is_any()) {
            diagnostics_.error("TypeError: slice start must be an integer, got '" + start_type->to_string() + "'",
                               expr.start()->span(), "use an integer for slice start", "NV1003");
        }
    }
    if (expr.end()) {
        TypePtr end_type = infer_expression(*expr.end());
        if (!end_type->is_int() && !end_type->is_any()) {
            diagnostics_.error("TypeError: slice end must be an integer, got '" + end_type->to_string() + "'",
                               expr.end()->span(), "use an integer for slice end", "NV1003");
        }
    }
    if (expr.step()) {
        TypePtr step_type = infer_expression(*expr.step());
        if (!step_type->is_int() && !step_type->is_any()) {
            diagnostics_.error("TypeError: slice step must be an integer, got '" + step_type->to_string() + "'",
                               expr.step()->span(), "use an integer for slice step", "NV1003");
        }
    }

    last_inferred_type_ = target_type;
}

void TypeChecker::visit_array(const ArrayExpr& expr) {
    if (expr.elements().empty()) {
        last_inferred_type_ = Type::make_list(Type::make_any());
        return;
    }

    TypePtr elem_type = infer_expression(*expr.elements()[0]);
    for (size_t i = 1; i < expr.elements().size(); ++i) {
        TypePtr t = infer_expression(*expr.elements()[i]);
        if (!elem_type->is_assignable_from(t)) {
            elem_type = Type::make_any();
            break;
        }
    }
    last_inferred_type_ = Type::make_list(elem_type);
}

void TypeChecker::visit_object(const ObjectExpr& expr) {
    TypePtr val_type = Type::make_any();
    if (!expr.entries().empty()) {
        val_type = infer_expression(*expr.entries()[0].second);
        for (size_t i = 1; i < expr.entries().size(); ++i) {
            TypePtr next_t = infer_expression(*expr.entries()[i].second);
            if (!val_type->is_assignable_from(next_t)) {
                val_type = Type::make_any();
                break;
            }
        }
    }
    last_inferred_type_ = Type::make_map(Type::make_string(), val_type);
}

void TypeChecker::visit_pipe(const PipeExpr& expr) {
    TypePtr left = infer_expression(expr.left());
    (void)left;
    last_inferred_type_ = infer_expression(expr.right());
}

void TypeChecker::visit_assign(const AssignExpr& expr) {
    TypePtr val_type = infer_expression(expr.value());

    auto var_type = current_env_->lookup(expr.name());
    if (var_type) {
        if (!var_type->is_assignable_from(val_type)) {
            diagnostics_.error("TypeError: cannot assign value of type '" + val_type->to_string() +
                               "' to variable '" + expr.name() + "' of type '" + var_type->to_string() + "'",
                               expr.span(), "check value type or adjust variable declaration", "NV1003");
        }
    } else {
        current_env_->define(expr.name(), val_type, true);
    }

    last_inferred_type_ = val_type;
}

void TypeChecker::visit_range(const RangeExpr&) {
    last_inferred_type_ = Type::make_list(Type::make_int());
}

void TypeChecker::visit_lambda(const LambdaExpr& expr) {
    std::vector<TypePtr> param_types(expr.params().size(), Type::make_any());
    auto fn_env = TypeEnvironment::create(current_env_);
    for (const auto& p : expr.params()) {
        fn_env->define(p, Type::make_any(), true);
    }
    auto prev_env = current_env_;
    current_env_ = fn_env;
    TypePtr ret_type = Type::make_any();
    if (expr.body_expr()) {
        ret_type = infer_expression(*expr.body_expr());
    } else if (expr.body_block()) {
        check_statement(*expr.body_block());
    }
    current_env_ = prev_env;
    last_inferred_type_ = Type::make_function(std::move(param_types), ret_type);
}

void TypeChecker::visit_expr_stmt(const ExprStmt& stmt) {
    infer_expression(stmt.expr());
}

void TypeChecker::visit_let_stmt(const LetStmt& stmt) {
    TypePtr init_type = nullptr;
    if (stmt.initializer()) {
        init_type = infer_expression(*stmt.initializer());
    }

    if (!stmt.type_annotation().empty()) {
        TypePtr annotated_type = Type::parse(stmt.type_annotation());
        if (init_type && !annotated_type->is_assignable_from(init_type)) {
            diagnostics_.error("TypeError: type mismatch in variable '" + stmt.name() + "': expected '" +
                               annotated_type->to_string() + "', found '" + init_type->to_string() + "'",
                               stmt.span(), "ensure the initializer value matches the annotated type", "NV1003");
        }
        current_env_->define(stmt.name(), annotated_type, stmt.is_mut());
    } else {
        TypePtr inferred = init_type ? init_type : Type::make_any();
        current_env_->define(stmt.name(), inferred, stmt.is_mut());
    }
}

void TypeChecker::visit_block_stmt(const BlockStmt& stmt) {
    auto block_env = TypeEnvironment::create(current_env_);
    auto prev_env = current_env_;
    current_env_ = block_env;

    for (const auto& s : stmt.statements()) {
        check_statement(*s);
    }

    current_env_ = prev_env;
}

void TypeChecker::visit_if_stmt(const IfStmt& stmt) {
    infer_expression(stmt.condition());
    check_statement(stmt.then_branch());
    if (stmt.else_branch()) {
        check_statement(*stmt.else_branch());
    }
}

void TypeChecker::visit_while_stmt(const WhileStmt& stmt) {
    infer_expression(stmt.condition());
    check_statement(stmt.body());
}

void TypeChecker::visit_for_in_stmt(const ForInStmt& stmt) {
    TypePtr iter_type = infer_expression(stmt.iterable());
    TypePtr elem_type = iter_type->is_list() ? iter_type->element_type() : Type::make_any();

    auto loop_env = TypeEnvironment::create(current_env_);
    loop_env->define(stmt.variable_name(), elem_type, true);

    auto prev_env = current_env_;
    current_env_ = loop_env;
    check_statement(stmt.body());
    current_env_ = prev_env;
}

void TypeChecker::visit_return_stmt(const ReturnStmt& stmt) {
    TypePtr ret_type = Type::make_void();
    if (stmt.value()) {
        ret_type = infer_expression(*stmt.value());
    }

    if (current_function_return_type_ && !current_function_return_type_->is_void()) {
        if (!current_function_return_type_->is_assignable_from(ret_type)) {
            diagnostics_.error("TypeError: function expected return type '" + current_function_return_type_->to_string() +
                               "', returned '" + ret_type->to_string() + "'", stmt.span(),
                               "ensure returned expression matches declared function return type", "NV1003");
        }
    }
}

void TypeChecker::visit_break_stmt(const BreakStmt&) {}
void TypeChecker::visit_continue_stmt(const ContinueStmt&) {}

void TypeChecker::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    std::vector<TypePtr> param_types;
    for (const auto& p : stmt.params()) {
        if (!p.type_annotation.empty()) {
            param_types.push_back(Type::parse(p.type_annotation));
        } else {
            param_types.push_back(Type::make_any());
        }
    }

    TypePtr return_type = Type::make_any();
    if (!stmt.return_type().empty()) {
        return_type = Type::parse(stmt.return_type());
    }

    TypePtr fn_type = Type::make_function(param_types, return_type);
    current_env_->define(stmt.name(), fn_type, false);

    // Check function body in isolated scope
    auto fn_env = TypeEnvironment::create(current_env_);
    for (size_t i = 0; i < stmt.params().size(); ++i) {
        fn_env->define(stmt.params()[i].name, param_types[i], true);
    }

    auto prev_env = current_env_;
    auto prev_ret = current_function_return_type_;
    current_env_ = fn_env;
    current_function_return_type_ = return_type;

    if (stmt.is_arrow_body()) {
        TypePtr body_type = infer_expression(*stmt.expr_body());
        if (!return_type->is_void() && !return_type->is_assignable_from(body_type)) {
            diagnostics_.error("TypeError: function expected return type '" + return_type->to_string() +
                               "', returned '" + body_type->to_string() + "'", stmt.span(),
                               "ensure returned expression matches declared function return type", "NV1003");
        }
    } else if (stmt.body()) {
        for (const auto& s : stmt.body()->statements()) {
            check_statement(*s);
        }
    }

    current_env_ = prev_env;
    current_function_return_type_ = prev_ret;
}

void TypeChecker::visit_import_stmt(const ImportStmt& stmt) {
    if (stmt.is_from_import()) {
        for (const auto& item : stmt.items()) {
            if (stmt.module_name() == "math") {
                if (item.symbol_name == "pi" || item.symbol_name == "e") {
                    current_env_->define(item.alias, Type::make_float(), false);
                } else if (item.symbol_name == "pow" || item.symbol_name == "min" || item.symbol_name == "max") {
                    current_env_->define(item.alias, Type::make_function({Type::make_float(), Type::make_float()}, Type::make_float()), false);
                } else {
                    current_env_->define(item.alias, Type::make_function({Type::make_float()}, Type::make_float()), false);
                }
            } else {
                current_env_->define(item.alias, Type::make_any(), false);
            }
        }
    } else {
        std::string bind_name = stmt.alias().empty() ? stmt.module_name() : stmt.alias();
        if (stmt.alias().empty()) {
            size_t slash = bind_name.find_last_of("/\\");
            if (slash != std::string::npos) {
                bind_name = bind_name.substr(slash + 1);
            }
            if (bind_name.size() >= 3 && bind_name.substr(bind_name.size() - 3) == ".nv") {
                bind_name = bind_name.substr(0, bind_name.size() - 3);
            }
        }
        current_env_->define(bind_name, Type::make_map(Type::make_string(), Type::make_any()), false);
        if (bind_name.starts_with("std.") && bind_name.size() > 4) {
            current_env_->define(bind_name.substr(4), Type::make_map(Type::make_string(), Type::make_any()), false);
        }
    }
}

void TypeChecker::visit_export_stmt(const ExportStmt& stmt) {
    check_statement(stmt.inner_stmt());
}

} // namespace nextviper
