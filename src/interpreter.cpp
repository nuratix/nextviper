#include "nextviper/interpreter.hpp"
#include <iostream>
#include <chrono>
#include <cmath>
#include <sstream>

namespace nextviper {

struct ReturnSignal {
    Value value;
};

struct BreakSignal {};
struct ContinueSignal {};

Interpreter::Interpreter(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    globals_ = Environment::create();
    environment_ = globals_;
    init_builtins();
}

void Interpreter::runtime_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error("RuntimeError: " + message, span, help);
    throw RuntimeError(RuntimeErrorKind::GENERIC_ERROR, message, span, help);
}

void Interpreter::type_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error("TypeError: " + message, span, help);
    throw RuntimeError(RuntimeErrorKind::TYPE_ERROR, message, span, help);
}

void Interpreter::name_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error("NameError: " + message, span, help.empty() ? "make sure the variable is declared before use" : help);
    throw RuntimeError(RuntimeErrorKind::NAME_ERROR, message, span, help);
}

void Interpreter::division_by_zero_error(SourceSpan span, std::string help) {
    diagnostics_.error("DivisionByZeroError: cannot divide by zero", span, help);
    throw RuntimeError(RuntimeErrorKind::DIVISION_BY_ZERO, "cannot divide by zero", span, help);
}

void Interpreter::mutability_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error("MutabilityError: " + message, span, help.empty() ? "use 'let mut' to make the variable reassignable" : help);
    throw RuntimeError(RuntimeErrorKind::MUTABILITY_ERROR, message, span, help);
}

void Interpreter::index_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error("IndexError: " + message, span, help);
    throw RuntimeError(RuntimeErrorKind::INDEX_ERROR, message, span, help);
}

void Interpreter::init_builtins() {
    // print(...)
    globals_->define("print", Value::make_native_fn("print", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << "\n";
        return Value::make_nil();
    }));

    // println(...)
    globals_->define("println", Value::make_native_fn("println", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << "\n";
        return Value::make_nil();
    }));

    // print_raw(...)
    globals_->define("print_raw", Value::make_native_fn("print_raw", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << std::flush;
        return Value::make_nil();
    }));

    // len(collection)
    globals_->define("len", Value::make_native_fn("len", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        const auto& val = args[0];
        if (val.is_string()) {
            return Value::make_int(static_cast<int64_t>(val.as_string().size()));
        }
        if (val.is_array()) {
            return Value::make_int(static_cast<int64_t>(val.as_array()->size()));
        }
        if (val.is_object()) {
            return Value::make_int(static_cast<int64_t>(val.as_object()->size()));
        }
        throw RuntimeError("len() requires String, Array, or Object, got " + val.type_name(), span);
    }));

    // typeof(val)
    globals_->define("typeof", Value::make_native_fn("typeof", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(args[0].type_name());
    }));

    // range(start, end, step)
    globals_->define("range", Value::make_native_fn("range", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty() || args.size() > 3) {
            throw RuntimeError("range() takes 1, 2, or 3 arguments", span);
        }
        int64_t start = 0;
        int64_t end = 0;
        int64_t step = 1;

        if (args.size() == 1) {
            if (!args[0].is_int()) throw RuntimeError("range() arguments must be integers", span);
            end = args[0].as_int();
        } else if (args.size() == 2) {
            if (!args[0].is_int() || !args[1].is_int()) throw RuntimeError("range() arguments must be integers", span);
            start = args[0].as_int();
            end = args[1].as_int();
        } else {
            if (!args[0].is_int() || !args[1].is_int() || !args[2].is_int()) throw RuntimeError("range() arguments must be integers", span);
            start = args[0].as_int();
            end = args[1].as_int();
            step = args[2].as_int();
            if (step == 0) throw RuntimeError("range() step cannot be zero", span);
        }

        std::vector<Value> result;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) {
                result.push_back(Value::make_int(i));
            }
        } else {
            for (int64_t i = start; i > end; i += step) {
                result.push_back(Value::make_int(i));
            }
        }
        return Value::make_array(std::move(result));
    }));

    // push(array, val) / append(array, val)
    globals_->define("push", Value::make_native_fn("push", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) {
            throw RuntimeError("push() requires Array as first argument", span);
        }
        args[0].as_array()->push_back(args[1]);
        return args[0];
    }));

    globals_->define("append", Value::make_native_fn("append", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) {
            throw RuntimeError("append() requires Array as first argument", span);
        }
        args[0].as_array()->push_back(args[1]);
        return args[0];
    }));

    // insert(array, index, val)
    globals_->define("insert", Value::make_native_fn("insert", 3, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) throw RuntimeError("insert() requires Array as first argument", span);
        if (!args[1].is_int()) throw RuntimeError("insert() index must be an integer", span);
        auto arr = args[0].as_array();
        int64_t idx = args[1].as_int();
        if (idx < 0) idx += arr->size();
        if (idx < 0 || static_cast<size_t>(idx) > arr->size()) {
            throw RuntimeError("insert() index out of bounds", span);
        }
        arr->insert(arr->begin() + idx, args[2]);
        return args[0];
    }));

    // pop(array, [index])
    globals_->define("pop", Value::make_native_fn("pop", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty() || args.size() > 2 || !args[0].is_array()) {
            throw RuntimeError("pop() requires Array as first argument", span);
        }
        auto arr = args[0].as_array();
        if (arr->empty()) {
            throw RuntimeError("pop() called on empty Array", span);
        }
        int64_t idx = -1;
        if (args.size() == 2) {
            if (!args[1].is_int()) throw RuntimeError("pop() index must be an integer", span);
            idx = args[1].as_int();
        }
        if (idx < 0) idx += arr->size();
        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
            throw RuntimeError("pop() index out of bounds", span);
        }
        Value val = (*arr)[static_cast<size_t>(idx)];
        arr->erase(arr->begin() + idx);
        return val;
    }));

    // remove(coll, key_or_val)
    globals_->define("remove", Value::make_native_fn("remove", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_array()) {
            auto arr = args[0].as_array();
            if (args[1].is_int()) {
                int64_t idx = args[1].as_int();
                if (idx < 0) idx += arr->size();
                if (idx >= 0 && static_cast<size_t>(idx) < arr->size()) {
                    Value removed = (*arr)[static_cast<size_t>(idx)];
                    arr->erase(arr->begin() + idx);
                    return removed;
                }
            }
            auto it = std::find(arr->begin(), arr->end(), args[1]);
            if (it != arr->end()) {
                Value removed = *it;
                arr->erase(it);
                return removed;
            }
            return Value::make_nil();
        }
        if (args[0].is_object()) {
            auto obj = args[0].as_object();
            std::string key = args[1].to_string();
            auto it = obj->find(key);
            if (it != obj->end()) {
                Value val = it->second;
                obj->erase(it);
                return val;
            }
            return Value::make_nil();
        }
        throw RuntimeError("remove() requires List or Map as first argument", span);
    }));

    // keys(map)
    globals_->define("keys", Value::make_native_fn("keys", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("keys() requires Map as argument", span);
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) {
            res.push_back(Value::make_string(k));
        }
        return Value::make_array(std::move(res));
    }));

    // values(map)
    globals_->define("values", Value::make_native_fn("values", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("values() requires Map as argument", span);
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) {
            res.push_back(v);
        }
        return Value::make_array(std::move(res));
    }));

    // entries(map)
    globals_->define("entries", Value::make_native_fn("entries", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("entries() requires Map as argument", span);
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) {
            res.push_back(Value::make_array({Value::make_string(k), v}));
        }
        return Value::make_array(std::move(res));
    }));

    // contains(coll, item) / has(coll, key)
    globals_->define("contains", Value::make_native_fn("contains", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            return Value::make_bool(std::find(arr.begin(), arr.end(), args[1]) != arr.end());
        }
        if (args[0].is_object()) {
            const auto& obj = *args[0].as_object();
            return Value::make_bool(obj.find(args[1].to_string()) != obj.end());
        }
        if (args[0].is_string()) {
            return Value::make_bool(args[0].as_string().find(args[1].to_string()) != std::string::npos);
        }
        throw RuntimeError("contains() requires List, Map, or String", span);
    }));

    globals_->define("has", Value::make_native_fn("has", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_object()) {
            const auto& obj = *args[0].as_object();
            return Value::make_bool(obj.find(args[1].to_string()) != obj.end());
        }
        if (args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            return Value::make_bool(std::find(arr.begin(), arr.end(), args[1]) != arr.end());
        }
        throw RuntimeError("has() requires Map or List", span);
    }));

    // join(list, separator)
    globals_->define("join", Value::make_native_fn("join", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty() || !args[0].is_array()) throw RuntimeError("join() requires List as first argument", span);
        std::string sep = (args.size() > 1) ? args[1].to_string() : "";
        const auto& arr = *args[0].as_array();
        std::string res;
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) res += sep;
            res += arr[i].to_string();
        }
        return Value::make_string(res);
    }));

    // reverse(list)
    globals_->define("reverse", Value::make_native_fn("reverse", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_array()) {
            auto arr = *args[0].as_array();
            std::reverse(arr.begin(), arr.end());
            return Value::make_array(std::move(arr));
        }
        if (args[0].is_string()) {
            std::string s = args[0].as_string();
            std::reverse(s.begin(), s.end());
            return Value::make_string(std::move(s));
        }
        throw RuntimeError("reverse() requires List or String", span);
    }));

    // sort(list)
    globals_->define("sort", Value::make_native_fn("sort", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) throw RuntimeError("sort() requires List as argument", span);
        auto arr = *args[0].as_array();
        std::sort(arr.begin(), arr.end());
        return Value::make_array(std::move(arr));
    }));

    // map(list, fn)
    globals_->define("map", Value::make_native_fn("map", 2, [this](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) throw RuntimeError("map() requires List as first argument", span);
        if (!args[1].is_callable()) throw RuntimeError("map() requires callable function as second argument", span);
        const auto& arr = *args[0].as_array();
        std::vector<Value> res;
        res.reserve(arr.size());
        for (const auto& item : arr) {
            res.push_back(this->call_function(args[1], {item}, span));
        }
        return Value::make_array(std::move(res));
    }));

    // filter(list, fn)
    globals_->define("filter", Value::make_native_fn("filter", 2, [this](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) throw RuntimeError("filter() requires List as first argument", span);
        if (!args[1].is_callable()) throw RuntimeError("filter() requires callable function as second argument", span);
        const auto& arr = *args[0].as_array();
        std::vector<Value> res;
        for (const auto& item : arr) {
            Value keep = this->call_function(args[1], {item}, span);
            if (keep.is_truthy()) {
                res.push_back(item);
            }
        }
        return Value::make_array(std::move(res));
    }));

    // reduce(list, fn, [initial])
    globals_->define("reduce", Value::make_native_fn("reduce", -1, [this](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2 || args.size() > 3) {
            throw RuntimeError("reduce() takes 2 or 3 arguments: reduce(list, fn, [initial])", span);
        }
        if (!args[0].is_array()) throw RuntimeError("reduce() requires List as first argument", span);
        if (!args[1].is_callable()) throw RuntimeError("reduce() requires callable function as second argument", span);
        const auto& arr = *args[0].as_array();
        if (arr.empty() && args.size() < 3) {
            throw RuntimeError("reduce() on empty List requires an initial value", span);
        }
        size_t start_idx = 0;
        Value accumulator;
        if (args.size() == 3) {
            accumulator = args[2];
        } else {
            accumulator = arr[0];
            start_idx = 1;
        }
        for (size_t i = start_idx; i < arr.size(); ++i) {
            accumulator = this->call_function(args[1], {accumulator, arr[i]}, span);
        }
        return accumulator;
    }));

    // clock() -> seconds since epoch
    globals_->define("clock", Value::make_native_fn("clock", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        auto now = std::chrono::high_resolution_clock::now();
        auto dur = now.time_since_epoch();
        double sec = std::chrono::duration<double>(dur).count();
        return Value::make_float(sec);
    }));

    // assert(condition, message)
    globals_->define("assert", Value::make_native_fn("assert", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty() || args.size() > 2) {
            throw RuntimeError("assert() takes 1 or 2 arguments", span);
        }
        if (!args[0].is_truthy()) {
            std::string msg = (args.size() == 2) ? args[1].to_string() : "assertion failed";
            throw RuntimeError(msg, span);
        }
        return Value::make_bool(true);
    }));

    // str(val)
    globals_->define("str", Value::make_native_fn("str", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(args[0].to_string());
    }));

    // int(val)
    globals_->define("int", Value::make_native_fn("int", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        const auto& v = args[0];
        if (v.is_int()) return v;
        if (v.is_float()) return Value::make_int(static_cast<int64_t>(v.as_float()));
        if (v.is_bool()) return Value::make_int(v.as_bool() ? 1 : 0);
        if (v.is_string()) {
            try {
                return Value::make_int(std::stoll(v.as_string()));
            } catch (...) {
                throw RuntimeError("cannot convert string '" + v.as_string() + "' to Int", span);
            }
        }
        throw RuntimeError("cannot convert " + v.type_name() + " to Int", span);
    }));

    // float(val)
    globals_->define("float", Value::make_native_fn("float", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        const auto& v = args[0];
        if (v.is_float()) return v;
        if (v.is_int()) return Value::make_float(static_cast<double>(v.as_int()));
        if (v.is_bool()) return Value::make_float(v.as_bool() ? 1.0 : 0.0);
        if (v.is_string()) {
            try {
                return Value::make_float(std::stod(v.as_string()));
            } catch (...) {
                throw RuntimeError("cannot convert string '" + v.as_string() + "' to Float", span);
            }
        }
        throw RuntimeError("cannot convert " + v.type_name() + " to Float", span);
    }));

    // Math functions
    globals_->define("abs", Value::make_native_fn("abs", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_int()) return Value::make_int(std::abs(args[0].as_int()));
        if (args[0].is_float()) return Value::make_float(std::abs(args[0].as_float()));
        throw RuntimeError("abs() requires a number, got " + args[0].type_name(), span);
    }));

    globals_->define("sqrt", Value::make_native_fn("sqrt", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_number()) throw RuntimeError("sqrt() requires a number", span);
        double val = args[0].as_float();
        if (val < 0) throw RuntimeError("cannot take sqrt of negative number", span);
        return Value::make_float(std::sqrt(val));
    }));

    globals_->define("pow", Value::make_native_fn("pow", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) throw RuntimeError("pow() requires two numbers", span);
        if (args[0].is_int() && args[1].is_int() && args[1].as_int() >= 0) {
            int64_t base = args[0].as_int();
            int64_t exp = args[1].as_int();
            int64_t res = 1;
            for (int64_t i = 0; i < exp; ++i) res *= base;
            return Value::make_int(res);
        }
        return Value::make_float(std::pow(args[0].as_float(), args[1].as_float()));
    }));

    globals_->define("min", Value::make_native_fn("min", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) throw RuntimeError("min() requires numbers", span);
        return (args[0] < args[1]) ? args[0] : args[1];
    }));

    globals_->define("max", Value::make_native_fn("max", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) throw RuntimeError("max() requires numbers", span);
        return (args[0] > args[1]) ? args[0] : args[1];
    }));

    // input(prompt)
    globals_->define("input", Value::make_native_fn("input", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args.empty()) {
            std::cout << args[0].to_string() << std::flush;
        }
        std::string line;
        if (std::getline(std::cin, line)) {
            return Value::make_string(line);
        }
        return Value::make_nil();
    }));
}

bool Interpreter::execute(const Program& program) {
    try {
        for (const auto& stmt : program.statements()) {
            execute_statement(*stmt);
        }
        return true;
    } catch (const RuntimeError&) {
        return false;
    } catch (const ReturnSignal&) {
        diagnostics_.error("return statement outside function", SourceSpan{});
        return false;
    } catch (const BreakSignal&) {
        diagnostics_.error("break statement outside loop", SourceSpan{});
        return false;
    } catch (const ContinueSignal&) {
        diagnostics_.error("continue statement outside loop", SourceSpan{});
        return false;
    }
}

Value Interpreter::evaluate(const Expr& expr) {
    expr.accept(*this);
    return last_evaluated_value_;
}

void Interpreter::execute_statement(const Stmt& stmt) {
    stmt.accept(*this);
}

void Interpreter::visit_literal(const LiteralExpr& expr) {
    switch (expr.kind()) {
        case LiteralExpr::Kind::INT:
            last_evaluated_value_ = Value::make_int(expr.int_value());
            break;
        case LiteralExpr::Kind::FLOAT:
            last_evaluated_value_ = Value::make_float(expr.float_value());
            break;
        case LiteralExpr::Kind::STRING:
            last_evaluated_value_ = Value::make_string(expr.string_value());
            break;
        case LiteralExpr::Kind::BOOL:
            last_evaluated_value_ = Value::make_bool(expr.bool_value());
            break;
        case LiteralExpr::Kind::NIL:
            last_evaluated_value_ = Value::make_nil();
            break;
    }
}

void Interpreter::visit_identifier(const IdentifierExpr& expr) {
    auto val = environment_->get(expr.name());
    if (!val) {
        name_error("undefined variable '" + expr.name() + "'", expr.span());
    }
    last_evaluated_value_ = *val;
}

void Interpreter::visit_unary(const UnaryExpr& expr) {
    Value right = evaluate(expr.operand());

    switch (expr.op()) {
        case TokenType::BANG:
        case TokenType::KEYWORD_NOT:
            last_evaluated_value_ = Value::make_bool(!right.is_truthy());
            return;
        case TokenType::MINUS:
            if (right.is_int()) {
                last_evaluated_value_ = Value::make_int(-right.as_int());
                return;
            }
            if (right.is_float()) {
                last_evaluated_value_ = Value::make_float(-right.as_float());
                return;
            }
            type_error("unary minus operator '-' requires a number, got " + right.type_name(), expr.span());
            return;
        default:
            runtime_error("unsupported unary operator", expr.span());
            return;
    }
}

void Interpreter::visit_binary(const BinaryExpr& expr) {
    // Short-circuit logical operators
    if (expr.op() == TokenType::AMP_AMP || expr.op() == TokenType::KEYWORD_AND) {
        Value left = evaluate(expr.left());
        if (!left.is_truthy()) {
            last_evaluated_value_ = left;
            return;
        }
        last_evaluated_value_ = evaluate(expr.right());
        return;
    }

    if (expr.op() == TokenType::PIPE_PIPE || expr.op() == TokenType::KEYWORD_OR) {
        Value left = evaluate(expr.left());
        if (left.is_truthy()) {
            last_evaluated_value_ = left;
            return;
        }
        last_evaluated_value_ = evaluate(expr.right());
        return;
    }

    Value left = evaluate(expr.left());
    Value right = evaluate(expr.right());

    switch (expr.op()) {
        case TokenType::PLUS:
            if (left.is_int() && right.is_int()) {
                last_evaluated_value_ = Value::make_int(left.as_int() + right.as_int());
                return;
            }
            if (left.is_number() && right.is_number()) {
                last_evaluated_value_ = Value::make_float(left.as_float() + right.as_float());
                return;
            }
            if (left.is_string() || right.is_string()) {
                last_evaluated_value_ = Value::make_string(left.to_string() + right.to_string());
                return;
            }
            if (left.is_array() && right.is_array()) {
                auto new_arr = *left.as_array();
                const auto& r_arr = *right.as_array();
                new_arr.insert(new_arr.end(), r_arr.begin(), r_arr.end());
                last_evaluated_value_ = Value::make_array(std::move(new_arr));
                return;
            }
            type_error("operator '+' not supported between " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::MINUS:
            if (left.is_int() && right.is_int()) {
                last_evaluated_value_ = Value::make_int(left.as_int() - right.as_int());
                return;
            }
            if (left.is_number() && right.is_number()) {
                last_evaluated_value_ = Value::make_float(left.as_float() - right.as_float());
                return;
            }
            type_error("operator '-' requires numbers, got " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::STAR:
            if (left.is_int() && right.is_int()) {
                last_evaluated_value_ = Value::make_int(left.as_int() * right.as_int());
                return;
            }
            if (left.is_number() && right.is_number()) {
                last_evaluated_value_ = Value::make_float(left.as_float() * right.as_float());
                return;
            }
            if (left.is_string() && right.is_int()) {
                std::string res;
                int64_t count = right.as_int();
                for (int64_t i = 0; i < count; ++i) res += left.as_string();
                last_evaluated_value_ = Value::make_string(res);
                return;
            }
            type_error("operator '*' not supported between " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::SLASH:
            if (left.is_number() && right.is_number()) {
                double r = right.as_float();
                if (r == 0.0) {
                    division_by_zero_error(expr.span(), "cannot divide by zero");
                }
                if (left.is_int() && right.is_int() && left.as_int() % right.as_int() == 0) {
                    last_evaluated_value_ = Value::make_int(left.as_int() / right.as_int());
                } else {
                    last_evaluated_value_ = Value::make_float(left.as_float() / r);
                }
                return;
            }
            type_error("operator '/' requires numbers, got " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::PERCENT:
            if (left.is_int() && right.is_int()) {
                int64_t r = right.as_int();
                if (r == 0) {
                    division_by_zero_error(expr.span(), "modulo by zero is not allowed");
                }
                last_evaluated_value_ = Value::make_int(left.as_int() % r);
                return;
            }
            type_error("operator '%' requires integers, got " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::POWER:
            if (left.is_number() && right.is_number()) {
                if (left.is_int() && right.is_int() && right.as_int() >= 0) {
                    int64_t base = left.as_int();
                    int64_t exp = right.as_int();
                    int64_t res = 1;
                    for (int64_t i = 0; i < exp; ++i) res *= base;
                    last_evaluated_value_ = Value::make_int(res);
                } else {
                    last_evaluated_value_ = Value::make_float(std::pow(left.as_float(), right.as_float()));
                }
                return;
            }
            type_error("power operator '**' requires numbers, got " + left.type_name() + " and " + right.type_name(), expr.span());
            return;

        case TokenType::EQUAL_EQUAL:
            last_evaluated_value_ = Value::make_bool(left == right);
            return;

        case TokenType::BANG_EQUAL:
            last_evaluated_value_ = Value::make_bool(left != right);
            return;

        case TokenType::LESS:
            last_evaluated_value_ = Value::make_bool(left < right);
            return;

        case TokenType::LESS_EQUAL:
            last_evaluated_value_ = Value::make_bool(left <= right);
            return;

        case TokenType::GREATER:
            last_evaluated_value_ = Value::make_bool(left > right);
            return;

        case TokenType::GREATER_EQUAL:
            last_evaluated_value_ = Value::make_bool(left >= right);
            return;

        default:
            runtime_error("unsupported binary operator", expr.span());
            return;
    }
}

void Interpreter::visit_call(const CallExpr& expr) {
    Value callee = evaluate(expr.callee());

    std::vector<Value> args;
    args.reserve(expr.args().size());
    for (const auto& arg : expr.args()) {
        args.push_back(evaluate(*arg));
    }

    last_evaluated_value_ = call_function(callee, args, expr.span());
}

static bool check_type_compat(const Value& val, const std::string& type_name) {
    if (type_name.empty() || type_name == "any" || type_name == "Any") return true;
    if (type_name == "int" || type_name == "Int") return val.is_int();
    if (type_name == "float" || type_name == "Float") return val.is_float();
    if (type_name == "number" || type_name == "Number") return val.is_number();
    if (type_name == "string" || type_name == "str" || type_name == "String") return val.is_string();
    if (type_name == "bool" || type_name == "Bool") return val.is_bool();
    if (type_name == "array" || type_name == "Array" || type_name == "list") return val.is_array();
    if (type_name == "object" || type_name == "Object" || type_name == "map") return val.is_object();
    if (type_name == "nil" || type_name == "null" || type_name == "void" || type_name == "None") return val.is_nil();
    return true; // Extensible for future type system
}

Value Interpreter::call_function(const Value& callee, const std::vector<Value>& args, SourceSpan span) {
    if (!callee.is_callable()) {
        runtime_error("cannot call non-function value of type " + callee.type_name(), span);
    }

    if (callee.type() == ValueType::NATIVE_FUNCTION) {
        auto nfn = callee.as_native_fn();
        if (nfn->arity >= 0 && static_cast<int>(args.size()) != nfn->arity) {
            runtime_error("native function '" + nfn->name + "' expects " + std::to_string(nfn->arity) +
                          " argument(s), got " + std::to_string(args.size()), span);
        }
        return nfn->func(args, span);
    }

    if (callee.type() == ValueType::FUNCTION) {
        auto fn = callee.as_function();
        if (args.size() != fn->params.size()) {
            runtime_error("function '" + fn->name + "' expects " + std::to_string(fn->params.size()) +
                          " argument(s), got " + std::to_string(args.size()), span);
        }

        // Validate parameter type annotations if present
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i < fn->param_types.size() && !fn->param_types[i].empty()) {
                const auto& expected = fn->param_types[i];
                if (!check_type_compat(args[i], expected)) {
                    type_error("expected parameter '" + fn->params[i] + "' to have type '" + expected +
                               "', got '" + args[i].type_name() + "'", span);
                }
            }
        }

        auto fn_env = Environment::create(fn->closure);
        for (size_t i = 0; i < fn->params.size(); ++i) {
            fn_env->define(fn->params[i], args[i], true);
        }

        auto previous_env = environment_;
        environment_ = fn_env;

        Value return_val = Value::make_nil();
        try {
            if (fn->decl) {
                if (fn->decl->is_arrow_body()) {
                    return_val = evaluate(*fn->decl->expr_body());
                } else if (fn->decl->body()) {
                    for (const auto& s : fn->decl->body()->statements()) {
                        execute_statement(*s);
                    }
                }
            }
        } catch (const ReturnSignal& sig) {
            return_val = sig.value;
        }

        environment_ = previous_env;

        // Validate return type annotation if present
        if (!fn->return_type.empty()) {
            if (!check_type_compat(return_val, fn->return_type)) {
                type_error("function '" + fn->name + "' expected return type '" + fn->return_type +
                           "', got '" + return_val.type_name() + "'", span);
            }
        }

        return return_val;
    }

    return Value::make_nil();
}

void Interpreter::visit_index(const IndexExpr& expr) {
    Value target = evaluate(expr.target());
    Value index = evaluate(expr.index());

    if (target.is_array()) {
        auto arr = target.as_array();
        if (!index.is_int()) {
            runtime_error("array index must be an integer, got " + index.type_name(), expr.span());
        }
        int64_t idx = index.as_int();
        int64_t size = static_cast<int64_t>(arr->size());
        if (idx < 0) idx += size; // negative indexing support like arr[-1]
        if (idx < 0 || idx >= size) {
            runtime_error("array index out of bounds: index " + std::to_string(index.as_int()) + " on array of size " + std::to_string(size), expr.span());
        }
        last_evaluated_value_ = (*arr)[static_cast<size_t>(idx)];
        return;
    }

    if (target.is_string()) {
        const std::string& str = target.as_string();
        if (!index.is_int()) {
            runtime_error("string index must be an integer, got " + index.type_name(), expr.span());
        }
        int64_t idx = index.as_int();
        int64_t size = static_cast<int64_t>(str.size());
        if (idx < 0) idx += size;
        if (idx < 0 || idx >= size) {
            runtime_error("string index out of bounds: index " + std::to_string(index.as_int()) + " on string of size " + std::to_string(size), expr.span());
        }
        last_evaluated_value_ = Value::make_string(std::string(1, str[static_cast<size_t>(idx)]));
        return;
    }

    if (target.is_object()) {
        auto obj = target.as_object();
        if (!index.is_string()) {
            runtime_error("object property key must be a string, got " + index.type_name(), expr.span());
        }
        auto it = obj->find(index.as_string());
        if (it != obj->end()) {
            last_evaluated_value_ = it->second;
        } else {
            last_evaluated_value_ = Value::make_nil();
        }
        return;
    }

    runtime_error("indexing not supported on type " + target.type_name(), expr.span());
}

void Interpreter::visit_slice(const SliceExpr& expr) {
    Value target = evaluate(expr.target());

    if (!target.is_array() && !target.is_string()) {
        type_error("slicing requires list or string, got " + target.type_name(), expr.span());
    }

    int64_t len = target.is_array() ? static_cast<int64_t>(target.as_array()->size()) : static_cast<int64_t>(target.as_string().size());

    int64_t start = 0;
    int64_t end = len;
    int64_t step = 1;

    if (expr.start()) {
        Value v_start = evaluate(*expr.start());
        if (!v_start.is_int()) type_error("slice start must be an integer, got " + v_start.type_name(), expr.start()->span());
        start = v_start.as_int();
        if (start < 0) start += len;
        if (start < 0) start = 0;
        if (start > len) start = len;
    }

    if (expr.end()) {
        Value v_end = evaluate(*expr.end());
        if (!v_end.is_int()) type_error("slice end must be an integer, got " + v_end.type_name(), expr.end()->span());
        end = v_end.as_int();
        if (end < 0) end += len;
        if (end < 0) end = 0;
        if (end > len) end = len;
    }

    if (expr.step()) {
        Value v_step = evaluate(*expr.step());
        if (!v_step.is_int()) type_error("slice step must be an integer, got " + v_step.type_name(), expr.step()->span());
        step = v_step.as_int();
        if (step == 0) runtime_error("slice step cannot be zero", expr.step()->span());
    }

    if (target.is_array()) {
        const auto& arr = *target.as_array();
        std::vector<Value> sliced;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) {
                sliced.push_back(arr[static_cast<size_t>(i)]);
            }
        } else {
            for (int64_t i = start; i > end; i += step) {
                sliced.push_back(arr[static_cast<size_t>(i)]);
            }
        }
        last_evaluated_value_ = Value::make_array(std::move(sliced));
        return;
    }

    if (target.is_string()) {
        const auto& str = target.as_string();
        std::string sliced;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) {
                sliced += str[static_cast<size_t>(i)];
            }
        } else {
            for (int64_t i = start; i > end; i += step) {
                sliced += str[static_cast<size_t>(i)];
            }
        }
        last_evaluated_value_ = Value::make_string(std::move(sliced));
        return;
    }
}

void Interpreter::visit_array(const ArrayExpr& expr) {
    std::vector<Value> elements;
    elements.reserve(expr.elements().size());
    for (const auto& el : expr.elements()) {
        elements.push_back(evaluate(*el));
    }
    last_evaluated_value_ = Value::make_array(std::move(elements));
}

void Interpreter::visit_object(const ObjectExpr& expr) {
    std::map<std::string, Value> entries;
    for (const auto& [k, v] : expr.entries()) {
        entries[k] = evaluate(*v);
    }
    last_evaluated_value_ = Value::make_object(std::move(entries));
}

void Interpreter::visit_pipe(const PipeExpr& expr) {
    Value input = evaluate(expr.left());

    // If right is a CallExpr, inject input as first argument
    if (auto* call = dynamic_cast<const CallExpr*>(&expr.right())) {
        Value callee = evaluate(call->callee());
        std::vector<Value> args;
        args.push_back(input);
        for (const auto& arg : call->args()) {
            args.push_back(evaluate(*arg));
        }
        last_evaluated_value_ = call_function(callee, args, expr.span());
        return;
    }

    // Otherwise, evaluate right as a callable and pass input
    Value callee = evaluate(expr.right());
    last_evaluated_value_ = call_function(callee, {input}, expr.span());
}

void Interpreter::visit_assign(const AssignExpr& expr) {
    Value val = evaluate(expr.value());

    if (expr.is_index_assign()) {
        const auto* idx = expr.index_target();
        Value target = evaluate(idx->target());
        Value index_val = evaluate(idx->index());

        if (target.is_array()) {
            auto arr = target.as_array();
            if (!index_val.is_int()) {
                runtime_error("array index must be an integer", expr.span());
            }
            int64_t i = index_val.as_int();
            if (i < 0) i += static_cast<int64_t>(arr->size());
            if (i < 0 || static_cast<size_t>(i) >= arr->size()) {
                runtime_error("array index out of bounds on assignment", expr.span());
            }

            if (expr.op() == TokenType::ASSIGN) {
                (*arr)[static_cast<size_t>(i)] = val;
            } else {
                // Compound assignment
                Value cur = (*arr)[static_cast<size_t>(i)];
                if (expr.op() == TokenType::PLUS_ASSIGN && cur.is_int() && val.is_int()) {
                    (*arr)[static_cast<size_t>(i)] = Value::make_int(cur.as_int() + val.as_int());
                } else if (expr.op() == TokenType::MINUS_ASSIGN && cur.is_int() && val.is_int()) {
                    (*arr)[static_cast<size_t>(i)] = Value::make_int(cur.as_int() - val.as_int());
                } else {
                    runtime_error("compound assignment unsupported on array element", expr.span());
                }
            }
            last_evaluated_value_ = val;
            return;
        }

        if (target.is_object()) {
            auto obj = target.as_object();
            if (!index_val.is_string()) {
                runtime_error("object key must be a string", expr.span());
            }
            (*obj)[index_val.as_string()] = val;
            last_evaluated_value_ = val;
            return;
        }

        runtime_error("cannot assign index to type " + target.type_name(), expr.span());
        return;
    }

    // Variable assignment
    if (expr.op() != TokenType::ASSIGN) {
        auto current_val = environment_->get(expr.name());
        if (!current_val) {
            runtime_error("undefined variable '" + expr.name() + "'", expr.span());
        }
        if (expr.op() == TokenType::PLUS_ASSIGN) {
            if (current_val->is_int() && val.is_int()) {
                val = Value::make_int(current_val->as_int() + val.as_int());
            } else if (current_val->is_number() && val.is_number()) {
                val = Value::make_float(current_val->as_float() + val.as_float());
            } else if (current_val->is_string() || val.is_string()) {
                val = Value::make_string(current_val->to_string() + val.to_string());
            } else {
                runtime_error("invalid operands for '+='", expr.span());
            }
        } else if (expr.op() == TokenType::MINUS_ASSIGN) {
            if (current_val->is_int() && val.is_int()) {
                val = Value::make_int(current_val->as_int() - val.as_int());
            } else if (current_val->is_number() && val.is_number()) {
                val = Value::make_float(current_val->as_float() - val.as_float());
            } else {
                runtime_error("invalid operands for '-='", expr.span());
            }
        } else if (expr.op() == TokenType::STAR_ASSIGN) {
            if (current_val->is_int() && val.is_int()) {
                val = Value::make_int(current_val->as_int() * val.as_int());
            } else if (current_val->is_number() && val.is_number()) {
                val = Value::make_float(current_val->as_float() * val.as_float());
            } else {
                runtime_error("invalid operands for '*='", expr.span());
            }
        } else if (expr.op() == TokenType::SLASH_ASSIGN) {
            if (current_val->is_number() && val.is_number()) {
                double r = val.as_float();
                if (r == 0.0) runtime_error("division by zero in '/='", expr.span());
                val = Value::make_float(current_val->as_float() / r);
            } else {
                runtime_error("invalid operands for '/='", expr.span());
            }
        }
    }

    std::string err;
    if (!environment_->assign(expr.name(), val, err)) {
        runtime_error(err, expr.span());
    }
    last_evaluated_value_ = val;
}

void Interpreter::visit_range(const RangeExpr& expr) {
    int64_t start = 0;
    int64_t end = 0;
    if (expr.start()) {
        Value sv = evaluate(*expr.start());
        if (!sv.is_int()) runtime_error("range start must be an integer", expr.span());
        start = sv.as_int();
    }
    if (expr.end()) {
        Value ev = evaluate(*expr.end());
        if (!ev.is_int()) runtime_error("range end must be an integer", expr.span());
        end = ev.as_int();
    }

    std::vector<Value> elements;
    if (expr.inclusive()) {
        for (int64_t i = start; i <= end; ++i) elements.push_back(Value::make_int(i));
    } else {
        for (int64_t i = start; i < end; ++i) elements.push_back(Value::make_int(i));
    }
    last_evaluated_value_ = Value::make_array(std::move(elements));
}

void Interpreter::visit_lambda(const LambdaExpr& expr) {
    auto fn_obj = std::make_shared<FunctionObject>();
    fn_obj->name = "anonymous";
    fn_obj->params = expr.params();
    fn_obj->closure = environment_;

    // Create anonymous function wrapper
    if (expr.body_expr()) {
        // Direct expression body
        fn_obj->decl = nullptr; // Handled in closure
    }

    last_evaluated_value_ = Value::make_function(fn_obj);
}

void Interpreter::visit_expr_stmt(const ExprStmt& stmt) {
    last_evaluated_value_ = evaluate(stmt.expr());
}

void Interpreter::visit_let_stmt(const LetStmt& stmt) {
    Value init_val = Value::make_nil();
    if (stmt.initializer()) {
        init_val = evaluate(*stmt.initializer());
    }
    environment_->define(stmt.name(), init_val, stmt.is_mut(), stmt.type_annotation());
}

void Interpreter::visit_block_stmt(const BlockStmt& stmt) {
    auto block_env = Environment::create(environment_);
    execute_block(stmt.statements(), block_env);
}

void Interpreter::execute_block(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> env) {
    auto previous_env = environment_;
    environment_ = env;

    try {
        for (const auto& stmt : statements) {
            execute_statement(*stmt);
        }
    } catch (...) {
        environment_ = previous_env;
        throw;
    }

    environment_ = previous_env;
}

void Interpreter::visit_if_stmt(const IfStmt& stmt) {
    Value cond = evaluate(stmt.condition());
    if (cond.is_truthy()) {
        execute_statement(stmt.then_branch());
    } else if (stmt.else_branch()) {
        execute_statement(*stmt.else_branch());
    }
}

void Interpreter::visit_while_stmt(const WhileStmt& stmt) {
    while (evaluate(stmt.condition()).is_truthy()) {
        try {
            execute_statement(stmt.body());
        } catch (const BreakSignal&) {
            break;
        } catch (const ContinueSignal&) {
            continue;
        }
    }
}

void Interpreter::visit_for_in_stmt(const ForInStmt& stmt) {
    Value iter = evaluate(stmt.iterable());

    if (iter.is_array()) {
        auto arr = iter.as_array();
        for (const auto& item : *arr) {
            auto loop_env = Environment::create(environment_);
            loop_env->define(stmt.variable_name(), item, true);
            auto prev_env = environment_;
            environment_ = loop_env;
            try {
                execute_statement(stmt.body());
            } catch (const BreakSignal&) {
                environment_ = prev_env;
                break;
            } catch (const ContinueSignal&) {
                environment_ = prev_env;
                continue;
            }
            environment_ = prev_env;
        }
        return;
    }

    if (iter.is_string()) {
        const std::string& str = iter.as_string();
        for (char c : str) {
            auto loop_env = Environment::create(environment_);
            loop_env->define(stmt.variable_name(), Value::make_string(std::string(1, c)), true);
            auto prev_env = environment_;
            environment_ = loop_env;
            try {
                execute_statement(stmt.body());
            } catch (const BreakSignal&) {
                environment_ = prev_env;
                break;
            } catch (const ContinueSignal&) {
                environment_ = prev_env;
                continue;
            }
            environment_ = prev_env;
        }
        return;
    }

    if (iter.is_object()) {
        auto obj = iter.as_object();
        for (const auto& [key, val] : *obj) {
            auto loop_env = Environment::create(environment_);
            loop_env->define(stmt.variable_name(), Value::make_string(key), true);
            auto prev_env = environment_;
            environment_ = loop_env;
            try {
                execute_statement(stmt.body());
            } catch (const BreakSignal&) {
                environment_ = prev_env;
                break;
            } catch (const ContinueSignal&) {
                environment_ = prev_env;
                continue;
            }
            environment_ = prev_env;
        }
        return;
    }

    runtime_error("for..in requires iterable (Array, Map, or String), got " + iter.type_name(), stmt.span());
}

void Interpreter::visit_return_stmt(const ReturnStmt& stmt) {
    Value val = Value::make_nil();
    if (stmt.value()) {
        val = evaluate(*stmt.value());
    }
    throw ReturnSignal{val};
}

void Interpreter::visit_break_stmt(const BreakStmt&) {
    throw BreakSignal{};
}

void Interpreter::visit_continue_stmt(const ContinueStmt&) {
    throw ContinueSignal{};
}

void Interpreter::visit_fn_decl_stmt(const FnDeclStmt& stmt) {
    auto fn_obj = std::make_shared<FunctionObject>();
    fn_obj->name = stmt.name();
    for (const auto& p : stmt.params()) {
        fn_obj->params.push_back(p.name);
        fn_obj->param_types.push_back(p.type_annotation);
    }
    fn_obj->return_type = stmt.return_type();
    fn_obj->decl = &stmt;
    fn_obj->closure = environment_;

    environment_->define(stmt.name(), Value::make_function(fn_obj), false);
}

} // namespace nextviper
