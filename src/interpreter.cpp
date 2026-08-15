#include "nextviper/interpreter.hpp"
#include "nextviper/module.hpp"
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
    : call_stack_depth_(0), diagnostics_(diagnostics), globals_(Environment::create()),
      environment_(globals_), last_evaluated_value_(), current_file_(),
      module_manager_(std::make_shared<ModuleManager>(diagnostics)) {
    init_builtins();
}

void Interpreter::runtime_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error(message, span, help, "NV4005");
    throw RuntimeError(RuntimeErrorKind::GENERIC_ERROR, message, span, help);
}

void Interpreter::type_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error(message, span, help.empty() ? "ensure operands have compatible types" : help, "NV1003");
    throw RuntimeError(RuntimeErrorKind::TYPE_ERROR, message, span, help);
}

void Interpreter::name_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error(message, span, help.empty() ? "make sure the identifier is declared before use" : help, "NV1001");
    throw RuntimeError(RuntimeErrorKind::NAME_ERROR, message, span, help);
}

void Interpreter::division_by_zero_error(SourceSpan span, std::string help) {
    diagnostics_.error("division by zero", span, help.empty() ? "ensure denominator is non-zero before division" : help, "NV4001");
    throw RuntimeError(RuntimeErrorKind::DIVISION_BY_ZERO, "cannot divide by zero", span, help);
}

void Interpreter::mutability_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error(message, span, help.empty() ? "use 'let mut' to make the variable reassignable" : help, "NV1003");
    throw RuntimeError(RuntimeErrorKind::MUTABILITY_ERROR, message, span, help);
}

void Interpreter::index_error(const std::string& message, SourceSpan span, std::string help) {
    diagnostics_.error(message, span, help.empty() ? "index is out of range for collection" : help, "NV4002");
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

        int64_t count = 0;
        if (step > 0 && end > start) {
            count = (end - start + step - 1) / step;
        } else if (step < 0 && start > end) {
            count = (start - end - step - 1) / (-step);
        }
        if (count > static_cast<int64_t>(MAX_ARRAY_ELEMENTS)) {
            throw RuntimeError("range() element count (" + std::to_string(count) + ") exceeds maximum allowed limit (10,000,000)", span);
        }

        std::vector<Value> result;
        result.reserve(static_cast<size_t>(count));
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
        auto arr = args[0].as_array();
        if (arr->size() >= MAX_ARRAY_ELEMENTS) {
            throw RuntimeError("array size exceeds maximum allowed limit (10,000,000 elements)", span);
        }
        arr->push_back(args[1]);
        return args[0];
    }));

    globals_->define("append", Value::make_native_fn("append", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) {
            throw RuntimeError("append() requires Array as first argument", span);
        }
        auto arr = args[0].as_array();
        if (arr->size() >= MAX_ARRAY_ELEMENTS) {
            throw RuntimeError("array size exceeds maximum allowed limit (10,000,000 elements)", span);
        }
        arr->push_back(args[1]);
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
    } catch (const RuntimeError& e) {
        diagnostics_.error(e.message(), e.span(), e.help(), "NV100");
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
        name_error("unknown variable `" + expr.name() + "`", expr.span(), "define `" + expr.name() + "` before using it");
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

    if (call_stack_depth_ >= MAX_CALL_STACK_DEPTH) {
        throw RuntimeError("maximum call stack depth exceeded (limit: 1000 frames)", span, "check for infinite recursion");
    }

    struct CallDepthGuard {
        size_t& depth;
        CallDepthGuard(size_t& d) : depth(d) { ++depth; }
        ~CallDepthGuard() { if (depth > 0) --depth; }
    };
    CallDepthGuard depth_guard(call_stack_depth_);

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
            } else if (fn->lambda_decl) {
                if (fn->lambda_decl->body_expr()) {
                    return_val = evaluate(*fn->lambda_decl->body_expr());
                } else if (fn->lambda_decl->body_block()) {
                    for (const auto& s : fn->lambda_decl->body_block()->statements()) {
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
        if (index.is_string()) {
            std::string method = index.as_string();
            if (method == "len") {
                last_evaluated_value_ = Value::make_native_fn("len", 0, [arr](const std::vector<Value>&, SourceSpan) -> Value {
                    return Value::make_int(static_cast<int64_t>(arr->size()));
                });
                return;
            }
            if (method == "append" || method == "push") {
                last_evaluated_value_ = Value::make_native_fn("append", 1, [arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    if (arr->size() >= MAX_ARRAY_ELEMENTS) {
                        throw RuntimeError("array size exceeds maximum allowed limit (10,000,000 items)", span, "prevent heap exhaustion");
                    }
                    arr->push_back(args[0]);
                    return Value::make_nil();
                });
                return;
            }
            if (method == "insert") {
                last_evaluated_value_ = Value::make_native_fn("insert", 2, [arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    if (!args[0].is_int()) throw RuntimeError("insert index must be an integer", span);
                    int64_t idx = args[0].as_int();
                    if (idx < 0) idx = 0;
                    if (idx > static_cast<int64_t>(arr->size())) idx = static_cast<int64_t>(arr->size());
                    arr->insert(arr->begin() + idx, args[1]);
                    return Value::make_nil();
                });
                return;
            }
            if (method == "remove") {
                last_evaluated_value_ = Value::make_native_fn("remove", 1, [arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    if (!args[0].is_int()) throw RuntimeError("remove index must be an integer", span);
                    int64_t idx = args[0].as_int();
                    if (idx < 0 || idx >= static_cast<int64_t>(arr->size())) {
                        throw RuntimeError("remove index out of bounds", span);
                    }
                    Value val = (*arr)[idx];
                    arr->erase(arr->begin() + idx);
                    return val;
                });
                return;
            }
            if (method == "slice") {
                last_evaluated_value_ = Value::make_native_fn("slice", 2, [arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    if (!args[0].is_int() || !args[1].is_int()) throw RuntimeError("slice indices must be integers", span);
                    int64_t start = std::max<int64_t>(0, args[0].as_int());
                    int64_t end = std::min<int64_t>(static_cast<int64_t>(arr->size()), args[1].as_int());
                    std::vector<Value> sub;
                    for (int64_t i = start; i < end; ++i) sub.push_back((*arr)[i]);
                    return Value::make_array(std::move(sub));
                });
                return;
            }
            if (method == "map") {
                last_evaluated_value_ = Value::make_native_fn("map", 1, [this, arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    std::vector<Value> res;
                    for (const auto& item : *arr) {
                        res.push_back(this->call_function(args[0], {item}, span));
                    }
                    return Value::make_array(std::move(res));
                });
                return;
            }
            if (method == "filter") {
                last_evaluated_value_ = Value::make_native_fn("filter", 1, [this, arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    std::vector<Value> res;
                    for (const auto& item : *arr) {
                        Value pred = this->call_function(args[0], {item}, span);
                        if (pred.as_bool()) res.push_back(item);
                    }
                    return Value::make_array(std::move(res));
                });
                return;
            }
            if (method == "reduce") {
                last_evaluated_value_ = Value::make_native_fn("reduce", 2, [this, arr](const std::vector<Value>& args, SourceSpan span) -> Value {
                    Value acc = args[0];
                    for (const auto& item : *arr) {
                        acc = this->call_function(args[1], {acc, item}, span);
                    }
                    return acc;
                });
                return;
            }
            runtime_error("unknown list method '" + method + "'", expr.span());
        }

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
        if (index.is_string()) {
            std::string method = index.as_string();
            if (method == "len" || method == "size" || method == "length") {
                last_evaluated_value_ = Value::make_native_fn(method, 0, [str](const std::vector<Value>&, SourceSpan) -> Value {
                    return Value::make_int(static_cast<int64_t>(str.size()));
                });
                return;
            }
            if (method == "to_upper") {
                last_evaluated_value_ = Value::make_native_fn(method, 0, [str](const std::vector<Value>&, SourceSpan) -> Value {
                    std::string s = str;
                    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                    return Value::make_string(s);
                });
                return;
            }
            if (method == "to_lower") {
                last_evaluated_value_ = Value::make_native_fn(method, 0, [str](const std::vector<Value>&, SourceSpan) -> Value {
                    std::string s = str;
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    return Value::make_string(s);
                });
                return;
            }
            if (method == "trim") {
                last_evaluated_value_ = Value::make_native_fn(method, 0, [str](const std::vector<Value>&, SourceSpan) -> Value {
                    size_t first = str.find_first_not_of(" \t\n\r");
                    if (first == std::string::npos) return Value::make_string("");
                    size_t last = str.find_last_not_of(" \t\n\r");
                    return Value::make_string(str.substr(first, last - first + 1));
                });
                return;
            }
            if (method == "split") {
                last_evaluated_value_ = Value::make_native_fn(method, 1, [str](const std::vector<Value>& args, SourceSpan) -> Value {
                    std::string delim = args[0].as_string();
                    std::vector<Value> res;
                    if (delim.empty()) {
                        for (char c : str) res.push_back(Value::make_string(std::string(1, c)));
                        return Value::make_array(std::move(res));
                    }
                    size_t start = 0, end = 0;
                    while ((end = str.find(delim, start)) != std::string::npos) {
                        res.push_back(Value::make_string(str.substr(start, end - start)));
                        start = end + delim.length();
                    }
                    res.push_back(Value::make_string(str.substr(start)));
                    return Value::make_array(std::move(res));
                });
                return;
            }
            if (method == "contains") {
                last_evaluated_value_ = Value::make_native_fn(method, 1, [str](const std::vector<Value>& args, SourceSpan) -> Value {
                    return Value::make_bool(str.find(args[0].as_string()) != std::string::npos);
                });
                return;
            }
            if (method == "starts_with") {
                last_evaluated_value_ = Value::make_native_fn(method, 1, [str](const std::vector<Value>& args, SourceSpan) -> Value {
                    return Value::make_bool(str.rfind(args[0].as_string(), 0) == 0);
                });
                return;
            }
            if (method == "ends_with") {
                last_evaluated_value_ = Value::make_native_fn(method, 1, [str](const std::vector<Value>& args, SourceSpan) -> Value {
                    std::string suf = args[0].as_string();
                    if (suf.size() > str.size()) return Value::make_bool(false);
                    return Value::make_bool(str.compare(str.size() - suf.size(), suf.size(), suf) == 0);
                });
                return;
            }
        }
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
        std::string key = index.as_string();
        auto it = obj->find(key);
        if (it != obj->end()) {
            last_evaluated_value_ = it->second;
            return;
        }

        // Built-in map helper methods
        if (key == "has") {
            last_evaluated_value_ = Value::make_native_fn("has", 1, [obj](const std::vector<Value>& args, SourceSpan span) -> Value {
                if (!args[0].is_string()) throw RuntimeError("map.has() requires string key", span);
                return Value::make_bool(obj->find(args[0].as_string()) != obj->end());
            });
            return;
        }
        if (key == "keys") {
            last_evaluated_value_ = Value::make_native_fn("keys", 0, [obj](const std::vector<Value>&, SourceSpan) -> Value {
                std::vector<Value> ks;
                for (const auto& [k, _] : *obj) ks.push_back(Value::make_string(k));
                return Value::make_array(std::move(ks));
            });
            return;
        }
        if (key == "values") {
            last_evaluated_value_ = Value::make_native_fn("values", 0, [obj](const std::vector<Value>&, SourceSpan) -> Value {
                std::vector<Value> vs;
                for (const auto& [_, v] : *obj) vs.push_back(v);
                return Value::make_array(std::move(vs));
            });
            return;
        }
        if (key == "len") {
            last_evaluated_value_ = Value::make_native_fn("len", 0, [obj](const std::vector<Value>&, SourceSpan) -> Value {
                return Value::make_int(static_cast<int64_t>(obj->size()));
            });
            return;
        }
        if (key == "remove") {
            last_evaluated_value_ = Value::make_native_fn("remove", 1, [obj](const std::vector<Value>& args, SourceSpan span) -> Value {
                if (!args[0].is_string()) throw RuntimeError("map.remove() requires string key", span);
                auto oit = obj->find(args[0].as_string());
                if (oit == obj->end()) return Value::make_nil();
                Value res = oit->second;
                obj->erase(oit);
                return res;
            });
            return;
        }

        last_evaluated_value_ = Value::make_nil();
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
    fn_obj->lambda_decl = &expr;

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
    if (const auto* range_expr = dynamic_cast<const RangeExpr*>(&stmt.iterable())) {
        int64_t start = 0;
        int64_t end = 0;
        if (range_expr->start()) {
            Value sv = evaluate(*range_expr->start());
            if (!sv.is_int()) runtime_error("range start must be an integer", range_expr->start()->span());
            start = sv.as_int();
        }
        if (range_expr->end()) {
            Value ev = evaluate(*range_expr->end());
            if (!ev.is_int()) runtime_error("range end must be an integer", range_expr->end()->span());
            end = ev.as_int();
        }
        bool inclusive = range_expr->inclusive();
        for (int64_t i = start; inclusive ? (i <= end) : (i < end); ++i) {
            auto loop_env = Environment::create(environment_);
            loop_env->define(stmt.variable_name(), Value::make_int(i), true);
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

void Interpreter::visit_import_stmt(const ImportStmt& stmt) {
    if (!module_manager_) {
        module_manager_ = std::make_shared<ModuleManager>(diagnostics_);
    }

    auto mod_opt = module_manager_->load_module(stmt.module_name(), current_file_, *this);
    if (!mod_opt) {
        runtime_error("failed to import module '" + stmt.module_name() + "'", stmt.span());
    }

    Value mod_obj = *mod_opt;
    if (!mod_obj.is_object()) {
        runtime_error("invalid module export for '" + stmt.module_name() + "'", stmt.span());
    }

    auto exports = mod_obj.as_object();

    if (stmt.is_from_import()) {
        for (const auto& item : stmt.items()) {
            auto it = exports->find(item.symbol_name);
            if (it == exports->end()) {
                runtime_error("module '" + stmt.module_name() + "' does not export symbol '" + item.symbol_name + "'", stmt.span());
            }
            environment_->define(item.alias, it->second, false);
        }
    } else {
        std::string bind_name = stmt.alias().empty() ? stmt.module_name() : stmt.alias();
        if (stmt.alias().empty()) {
            if (bind_name.rfind("std.", 0) == 0) {
                bind_name = bind_name.substr(4);
            }
            size_t slash = bind_name.find_last_of("/\\");
            if (slash != std::string::npos) {
                bind_name = bind_name.substr(slash + 1);
            }
            if (bind_name.size() >= 3 && bind_name.substr(bind_name.size() - 3) == ".nv") {
                bind_name = bind_name.substr(0, bind_name.size() - 3);
            }
        }
        environment_->define(bind_name, mod_obj, false);
        if (stmt.alias().empty() && stmt.module_name() != bind_name) {
            environment_->define(stmt.module_name(), mod_obj, false);
        }
    }
}

void Interpreter::visit_export_stmt(const ExportStmt& stmt) {
    execute_statement(stmt.inner_stmt());
}

} // namespace nextviper
