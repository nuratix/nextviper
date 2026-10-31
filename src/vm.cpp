#include "nextviper/slicing.hpp"
#include "nextviper/vm.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/arithmetic.hpp"
#include "nextviper/type_checker.hpp"
#include <iostream>
#include <cmath>
#include <chrono>

namespace nextviper {

VM::VM(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    reset_stack();
    init_builtins();
}

void VM::reset_stack() {
    stack_top_ = stack_.data();
    frame_count_ = 0;
}

void VM::push(Value value) {
    if (stack_top_ >= stack_.data() + STACK_MAX) {
        throw std::runtime_error("VM stack overflow");
    }
    *stack_top_++ = std::move(value);
}

Value VM::pop() {
    if (stack_top_ == stack_.data()) {
        throw std::runtime_error("VM stack underflow");
    }
    return *(--stack_top_);
}

Value VM::peek(int distance) const {
    if (distance < 0 || static_cast<size_t>(stack_top_ - stack_.data()) <= static_cast<size_t>(distance))
        throw std::runtime_error("VM stack underflow");
    return *(stack_top_ - 1 - distance);
}

void VM::define_global(const std::string& name, Value value) {
    globals_[name] = std::move(value);
}

void VM::define_native(const std::string& name, int arity, NativeFn func) {
    define_global(name, Value::make_native_fn(name, arity, std::move(func)));
}

void VM::init_builtins() {
    define_native("print", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << "\n";
        return Value::make_nil();
    });

    define_native("println", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << "\n";
        return Value::make_nil();
    });

    define_native("print_raw", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << std::flush;
        return Value::make_nil();
    });

    define_native("len", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        const auto& val = args[0];
        if (val.is_string()) return Value::make_int(static_cast<int64_t>(val.as_string().size()));
        if (val.is_array()) return Value::make_int(static_cast<int64_t>(val.as_array()->size()));
        if (val.is_object()) return Value::make_int(static_cast<int64_t>(val.as_object()->size()));
        return Value::make_int(0);
    });

    define_native("typeof", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(args[0].type_name());
    });

    define_native("range", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.empty() || args.size() > 3) throw std::invalid_argument("range requires 1 to 3 arguments");
        for (const auto& arg : args) if (!arg.is_int()) throw std::invalid_argument("range arguments must be integers");
        return make_range_values(args.size() == 1 ? 0 : args[0].as_int(),
                                 args[args.size() == 1 ? 0 : 1].as_int(),
                                 args.size() == 3 ? args[2].as_int() : 1);
    });
    define_native("$range_inclusive", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_int() || !args[1].is_int()) throw std::invalid_argument("range bounds must be integers");
        return make_range_values(args[0].as_int(), args[1].as_int(), 1, true);
    });
    define_native("$iter_snapshot", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_array()) return Value::make_array(*args[0].as_array());
        if (args[0].is_string()) return args[0];
        if (args[0].is_object()) {
            std::vector<Value> keys;
            for (const auto& [key, value] : *args[0].as_object()) { (void)value; keys.push_back(Value::make_string(key)); }
            return Value::make_array(std::move(keys));
        }
        throw std::invalid_argument("for..in requires an iterable");
    });

    define_native("push", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_array()) {
            args[0].as_array()->push_back(args[1]);
        }
        return args[0];
    });

    define_native("append", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_array()) {
            args[0].as_array()->push_back(args[1]);
        }
        return args[0];
    });

    define_native("insert", 3, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || !args[1].is_int()) return args[0];
        auto arr = args[0].as_array();
        int64_t idx = args[1].as_int();
        if (idx < 0) idx += arr->size();
        if (idx >= 0 && static_cast<size_t>(idx) <= arr->size()) {
            arr->insert(arr->begin() + idx, args[2]);
        }
        return args[0];
    });

    define_native("pop", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.empty() || !args[0].is_array()) return Value::make_nil();
        auto arr = args[0].as_array();
        if (arr->empty()) return Value::make_nil();
        int64_t idx = -1;
        if (args.size() > 1 && args[1].is_int()) idx = args[1].as_int();
        if (idx < 0) idx += arr->size();
        if (idx >= 0 && static_cast<size_t>(idx) < arr->size()) {
            Value last = (*arr)[static_cast<size_t>(idx)];
            arr->erase(arr->begin() + idx);
            return last;
        }
        return Value::make_nil();
    });

    define_native("remove", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
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
        return Value::make_nil();
    });

    define_native("keys", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_object()) return Value::make_array({});
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) res.push_back(Value::make_string(k));
        return Value::make_array(std::move(res));
    });

    define_native("values", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_object()) return Value::make_array({});
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) res.push_back(v);
        return Value::make_array(std::move(res));
    });

    define_native("entries", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_object()) return Value::make_array({});
        const auto& obj = *args[0].as_object();
        std::vector<Value> res;
        for (const auto& [k, v] : obj) res.push_back(Value::make_array({Value::make_string(k), v}));
        return Value::make_array(std::move(res));
    });

    define_native("contains", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
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
        return Value::make_bool(false);
    });

    define_native("has", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_object()) {
            const auto& obj = *args[0].as_object();
            return Value::make_bool(obj.find(args[1].to_string()) != obj.end());
        }
        if (args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            return Value::make_bool(std::find(arr.begin(), arr.end(), args[1]) != arr.end());
        }
        return Value::make_bool(false);
    });

    define_native("join", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.empty() || !args[0].is_array()) return Value::make_string("");
        std::string sep = (args.size() > 1) ? args[1].to_string() : "";
        const auto& arr = *args[0].as_array();
        std::string res;
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) res += sep;
            res += arr[i].to_string();
        }
        return Value::make_string(res);
    });

    define_native("reverse", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
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
        return args[0];
    });

    define_native("sort", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return args[0];
        auto arr = *args[0].as_array();
        std::sort(arr.begin(), arr.end());
        return Value::make_array(std::move(arr));
    });

    define_native("map", 2, [this](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        const auto& arr = *args[0].as_array();
        std::vector<Value> res;
        res.reserve(arr.size());
        for (const auto& item : arr) {
            res.push_back(this->call_value(args[1], {item}));
        }
        return Value::make_array(std::move(res));
    });

    define_native("filter", 2, [this](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        const auto& arr = *args[0].as_array();
        std::vector<Value> res;
        for (const auto& item : arr) {
            Value keep = this->call_value(args[1], {item});
            if (keep.is_truthy()) res.push_back(item);
        }
        return Value::make_array(std::move(res));
    });

    define_native("reduce", -1, [this](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.size() < 2 || !args[0].is_array()) return Value::make_nil();
        const auto& arr = *args[0].as_array();
        if (arr.empty() && args.size() < 3) return Value::make_nil();
        size_t start_idx = 0;
        Value acc;
        if (args.size() >= 3) {
            acc = args[2];
        } else {
            acc = arr[0];
            start_idx = 1;
        }
        for (size_t i = start_idx; i < arr.size(); ++i) {
            acc = this->call_value(args[1], {acc, arr[i]});
        }
        return acc;
    });

    define_native("clock", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        auto now = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(now.time_since_epoch()).count();
        return Value::make_float(sec);
    });

    define_native("assert", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.empty() || !args[0].is_truthy()) {
            std::string msg = args.size() > 1 ? args[1].to_string() : "assertion failed";
            throw std::runtime_error(msg);
        }
        return Value::make_bool(true);
    });

    define_native("abs", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int()) return Value::make_int(args[0].as_int() < 0 ? checked_subtract(0, args[0].as_int()) : args[0].as_int());
        if (args[0].is_float()) return Value::make_float(std::abs(args[0].as_float()));
        return args[0];
    });

    define_native("min", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return (args[0] < args[1]) ? args[0] : args[1];
    });

    define_native("max", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return (args[0] > args[1]) ? args[0] : args[1];
    });

    define_native("pow", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return apply_arithmetic(TokenType::POWER, args[0], args[1]);
    });

    define_native("sqrt", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::sqrt(args[0].as_float()));
    });

    define_native("str", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(args[0].to_string());
    });

    define_native("int", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        const auto& v = args[0];
        if (v.is_int()) return v;
        if (v.is_float()) return Value::make_int(checked_float_to_int(v.as_float()));
        if (v.is_string()) {
            try { return Value::make_int(std::stoll(v.as_string())); } catch (...) {}
        }
        return Value::make_int(0);
    });

    define_native("float", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        const auto& v = args[0];
        if (v.is_float()) return v;
        if (v.is_int()) return Value::make_float(static_cast<double>(v.as_int()));
        if (v.is_string()) {
            try { return Value::make_float(std::stod(v.as_string())); } catch (...) {}
        }
        return Value::make_float(0.0);
    });
}

void VM::runtime_error(const std::string& message) {
    diagnostics_.error(message, SourceSpan{});
}

VMResult VM::execute(const Program& program) {
    TypeChecker checker(diagnostics_);
    if (!checker.check(program)) return VMResult::COMPILE_ERROR;
    BytecodeCompiler compiler(diagnostics_);
    auto fn = compiler.compile(program);
    if (diagnostics_.has_errors() || !fn) {
        return VMResult::COMPILE_ERROR;
    }
    return run(fn);
}

VMResult VM::run(std::shared_ptr<CompiledFunction> function) {
    reset_stack();
    push(Value::make_compiled_fn(function));

    CallFrame* frame = &frames_[frame_count_++];
    frame->function = function;
    frame->ip = function->chunk().code().data();
    frame->slots = stack_.data();

    try {
        return run_interpreter();
    } catch (const std::exception& error) {
        runtime_error(error.what());
        reset_stack();
        return VMResult::RUNTIME_ERROR;
    }
}

Value VM::call_value(Value callee, const std::vector<Value>& args) {
    if (callee.type() == ValueType::NATIVE_FUNCTION) {
        auto nfn = callee.as_native_fn();
        if (nfn->arity >= 0 && args.size() != static_cast<size_t>(nfn->arity))
            throw std::invalid_argument("native function argument count mismatch");
        return nfn->func(args, SourceSpan{});
    }
    if (callee.type() == ValueType::COMPILED_FUNCTION) {
        auto fn = callee.as_compiled_fn();
        if (args.size() != fn->arity()) throw std::invalid_argument("function argument count mismatch");
        if (frame_count_ >= FRAMES_MAX) throw std::runtime_error("VM call stack overflow");
        size_t base_frame = frame_count_;
        push(callee);
        for (const auto& a : args) push(a);

        CallFrame* new_frame = &frames_[frame_count_++];
        new_frame->function = fn;
        new_frame->ip = fn->chunk().code().data();
        new_frame->slots = stack_top_ - args.size() - 1;

        VMResult res = run_interpreter(base_frame);
        if (res == VMResult::OK) {
            return pop();
        }
        throw std::runtime_error("VM callback execution failed");
    }
    throw std::invalid_argument("cannot call a non-function");
}

VMResult VM::run_interpreter(size_t target_frame_count) {
    CallFrame* frame = &frames_[frame_count_ - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, static_cast<uint16_t>((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk().constants()[READ_BYTE()])
#define READ_STRING() (READ_CONSTANT().as_string())

    while (true) {
        uint8_t instruction = READ_BYTE();
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            case OpCode::OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OpCode::OP_NIL: push(Value::make_nil()); break;
            case OpCode::OP_TRUE: push(Value::make_bool(true)); break;
            case OpCode::OP_FALSE: push(Value::make_bool(false)); break;
            case OpCode::OP_POP: pop(); break;
            case OpCode::OP_DUP: push(peek(0)); break;
            case OpCode::OP_DUP_TWO: {
                Value target = peek(1), index = peek(0);
                push(target);
                push(index);
                break;
            }

            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                std::string name = READ_STRING();
                auto it = globals_.find(name);
                if (it == globals_.end()) {
                    runtime_error("undefined variable '" + name + "'");
                    return VMResult::RUNTIME_ERROR;
                }
                push(it->second);
                break;
            }
            case OpCode::OP_SET_GLOBAL: {
                std::string name = READ_STRING();
                globals_[name] = peek(0);
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL: {
                std::string name = READ_STRING();
                globals_[name] = pop();
                break;
            }

            case OpCode::OP_EQUAL: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                a = Value::make_bool(a == b);
                stack_top_--;
                break;
            }
            case OpCode::OP_NOT_EQUAL: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                a = Value::make_bool(a != b);
                stack_top_--;
                break;
            }
            case OpCode::OP_GREATER: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                if (a.is_int() && b.is_int()) {
                    a = Value::make_bool(a.as_int() > b.as_int());
                } else {
                    a = Value::make_bool(a > b);
                }
                stack_top_--;
                break;
            }
            case OpCode::OP_GREATER_EQUAL: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                if (a.is_int() && b.is_int()) {
                    a = Value::make_bool(a.as_int() >= b.as_int());
                } else {
                    a = Value::make_bool(a >= b);
                }
                stack_top_--;
                break;
            }
            case OpCode::OP_LESS: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                if (a.is_int() && b.is_int()) {
                    a = Value::make_bool(a.as_int() < b.as_int());
                } else {
                    a = Value::make_bool(a < b);
                }
                stack_top_--;
                break;
            }
            case OpCode::OP_LESS_EQUAL: {
                Value& a = *(stack_top_ - 2);
                Value& b = *(stack_top_ - 1);
                if (a.is_int() && b.is_int()) {
                    a = Value::make_bool(a.as_int() <= b.as_int());
                } else {
                    a = Value::make_bool(a <= b);
                }
                stack_top_--;
                break;
            }

            case OpCode::OP_ADD:
            case OpCode::OP_SUBTRACT:
            case OpCode::OP_MULTIPLY:
            case OpCode::OP_DIVIDE:
            case OpCode::OP_MODULO:
            case OpCode::OP_POWER: {
                Value right = pop(), left = pop();
                TokenType token = op == OpCode::OP_ADD ? TokenType::PLUS :
                                  op == OpCode::OP_SUBTRACT ? TokenType::MINUS :
                                  op == OpCode::OP_MULTIPLY ? TokenType::STAR :
                                  op == OpCode::OP_DIVIDE ? TokenType::SLASH :
                                  op == OpCode::OP_MODULO ? TokenType::PERCENT : TokenType::POWER;
                push(apply_arithmetic(token, left, right));
                break;
            }
            case OpCode::OP_NEGATE: {
                Value value = pop();
                if (value.is_int()) push(Value::make_int(checked_subtract(0, value.as_int())));
                else if (value.is_float()) push(Value::make_float(-value.as_float()));
                else throw std::invalid_argument("unary '-' requires a number");
                break;
            }
            case OpCode::OP_NOT: {
                push(Value::make_bool(!pop().is_truthy()));
                break;
            }

            case OpCode::OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!peek(0).is_truthy()) {
                    frame->ip += offset;
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OpCode::OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                Value callee = peek(arg_count);

                if (callee.type() == ValueType::COMPILED_FUNCTION) {
                    auto fn = callee.as_compiled_fn();
                    if (arg_count != fn->arity()) {
                        runtime_error("expected " + std::to_string(fn->arity()) + " arguments but got " + std::to_string(arg_count));
                        return VMResult::RUNTIME_ERROR;
                    }

                    if (frame_count_ >= FRAMES_MAX) {
                        runtime_error("call stack overflow");
                        return VMResult::RUNTIME_ERROR;
                    }

                    CallFrame* new_frame = &frames_[frame_count_++];
                    new_frame->function = fn;
                    new_frame->ip = fn->chunk().code().data();
                    new_frame->slots = stack_top_ - arg_count - 1;
                    frame = new_frame;
                    break;
                }

                if (callee.type() == ValueType::NATIVE_FUNCTION) {
                    auto nfn = callee.as_native_fn();
                    if (nfn->arity != -1 && arg_count != static_cast<uint8_t>(nfn->arity)) {
                        runtime_error("native function '" + nfn->name + "' expected " + std::to_string(nfn->arity) + " arguments");
                        return VMResult::RUNTIME_ERROR;
                    }

                    std::vector<Value> args;
                    args.reserve(arg_count);
                    for (int i = arg_count - 1; i >= 0; --i) {
                        args.push_back(peek(i));
                    }

                    stack_top_ -= (arg_count + 1); // pop args and callee
                    try {
                        Value res = nfn->func(args, SourceSpan{});
                        push(res);
                    } catch (const std::exception& e) {
                        runtime_error(e.what());
                        return VMResult::RUNTIME_ERROR;
                    }
                    break;
                }

                runtime_error("can only call functions, got " + callee.type_name());
                return VMResult::RUNTIME_ERROR;
            }

            case OpCode::OP_RETURN: {
                Value result = pop();
                frame_count_--;
                stack_top_ = frame->slots;
                push(result);
                if (frame_count_ == target_frame_count) {
                    if (target_frame_count == 0) {
                        pop(); // pop main script function
                    }
                    return VMResult::OK;
                }

                frame = &frames_[frame_count_ - 1];
                break;
            }

            case OpCode::OP_BUILD_ARRAY: {
                uint8_t count = READ_BYTE();
                std::vector<Value> elements;
                elements.reserve(count);
                for (int i = count - 1; i >= 0; --i) {
                    elements.push_back(peek(i));
                }
                stack_top_ -= count;
                push(Value::make_array(std::move(elements)));
                break;
            }

            case OpCode::OP_BUILD_OBJECT: {
                uint8_t count = READ_BYTE();
                std::map<std::string, Value> entries;
                for (size_t i = 0; i < count; ++i) {
                    Value val = pop();
                    Value key = pop();
                    entries[key.as_string()] = std::move(val);
                }
                push(Value::make_object(std::move(entries)));
                break;
            }

            case OpCode::OP_INDEX_GET:
            case OpCode::OP_INDEX_GET_REQUIRED: {
                Value index = pop();
                Value target = pop();

                if (target.is_array()) {
                    if (!index.is_int()) throw std::invalid_argument("array index must be an integer");
                    auto arr = target.as_array();
                    int64_t idx = index.as_int();
                    int64_t sz = static_cast<int64_t>(arr->size());
                    if (idx < 0) idx += sz;
                    if (idx < 0 || idx >= sz) {
                        runtime_error("array index out of bounds: " + std::to_string(index.as_int()));
                        return VMResult::RUNTIME_ERROR;
                    }
                    push((*arr)[static_cast<size_t>(idx)]);
                    break;
                }

                if (target.is_string()) {
                    if (!index.is_int()) throw std::invalid_argument("string index must be an integer");
                    const auto& str = target.as_string();
                    int64_t idx = index.as_int();
                    int64_t sz = static_cast<int64_t>(str.size());
                    if (idx < 0) idx += sz;
                    if (idx < 0 || idx >= sz) {
                        runtime_error("string index out of bounds");
                        return VMResult::RUNTIME_ERROR;
                    }
                    push(Value::make_string(std::string(1, str[static_cast<size_t>(idx)])));
                    break;
                }

                if (target.is_object()) {
                    auto obj = target.as_object();
                    if (!index.is_string()) throw std::invalid_argument("object key must be a string");
                    auto it = obj->find(index.as_string());
                    if (it == obj->end() && op == OpCode::OP_INDEX_GET_REQUIRED)
                        throw std::out_of_range("unknown object key in compound assignment");
                    push(it != obj->end() ? it->second : Value::make_nil());
                    break;
                }

                runtime_error("cannot index into " + target.type_name());
                return VMResult::RUNTIME_ERROR;
            }

            case OpCode::OP_INDEX_SET: {
                Value val = pop();
                Value index = pop();
                Value target = pop();

                if (target.is_array()) {
                    if (!index.is_int()) throw std::invalid_argument("array index must be an integer");
                    auto arr = target.as_array();
                    int64_t idx = index.as_int();
                    if (idx < 0) idx += static_cast<int64_t>(arr->size());
                    if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
                        runtime_error("array index out of bounds on assign");
                        return VMResult::RUNTIME_ERROR;
                    }
                    (*arr)[static_cast<size_t>(idx)] = val;
                    push(val);
                    break;
                }

                if (target.is_object()) {
                    if (!index.is_string()) throw std::invalid_argument("object key must be a string");
                    (*target.as_object())[index.as_string()] = val;
                    push(val);
                    break;
                }

                runtime_error("cannot set index on " + target.type_name());
                return VMResult::RUNTIME_ERROR;
            }

            case OpCode::OP_SLICE: {
                Value step_val = pop();
                Value end_val = pop();
                Value start_val = pop();
                Value target = pop();

                if (!target.is_array() && !target.is_string()) {
                    runtime_error("slicing requires list or string, got " + target.type_name());
                    return VMResult::RUNTIME_ERROR;
                }

                auto bound = [](const Value& value, const char* name) -> std::optional<int64_t> {
                    if (value.is_nil()) return std::nullopt;
                    if (!value.is_int())
                        throw std::invalid_argument(std::string("slice ") + name + " must be an integer");
                    return value.as_int();
                };
                const auto start = bound(start_val, "start");
                const auto end = bound(end_val, "end");
                const auto step = bound(step_val, "step");
                push(slice_value(target, start, end, step.value_or(1)));
                break;
            }

            case OpCode::OP_PRINT: {
                uint8_t count = READ_BYTE();
                std::vector<Value> args;
                for (int i = count - 1; i >= 0; --i) {
                    args.push_back(peek(i));
                }
                stack_top_ -= count;
                for (int i = static_cast<int>(args.size()) - 1; i >= 0; --i) {
                    std::cout << args[i].to_string() << (i > 0 ? " " : "\n");
                }
                push(Value::make_nil());
                break;
            }

            default:
                runtime_error("unknown bytecode opcode: " + std::to_string(instruction));
                return VMResult::RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

} // namespace nextviper
