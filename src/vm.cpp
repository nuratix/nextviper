#include "nextviper/vm.hpp"
#include "nextviper/compiler.hpp"
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
        runtime_error("stack overflow");
        return;
    }
    *stack_top_++ = std::move(value);
}

Value VM::pop() {
    if (stack_top_ == stack_.data()) {
        runtime_error("stack underflow");
        return Value::make_nil();
    }
    return *(--stack_top_);
}

Value VM::peek(int distance) const {
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
        int64_t start = 0, end = 0, step = 1;
        if (args.size() == 1) {
            end = args[0].as_int();
        } else if (args.size() == 2) {
            start = args[0].as_int();
            end = args[1].as_int();
        } else if (args.size() >= 3) {
            start = args[0].as_int();
            end = args[1].as_int();
            step = args[2].as_int();
            if (step == 0) step = 1;
        }
        std::vector<Value> res;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) res.push_back(Value::make_int(i));
        } else {
            for (int64_t i = start; i > end; i += step) res.push_back(Value::make_int(i));
        }
        return Value::make_array(std::move(res));
    });

    define_native("push", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_array()) {
            args[0].as_array()->push_back(args[1]);
        }
        return args[0];
    });

    define_native("pop", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_array() && !args[0].as_array()->empty()) {
            Value last = args[0].as_array()->back();
            args[0].as_array()->pop_back();
            return last;
        }
        return Value::make_nil();
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
        if (args[0].is_int()) return Value::make_int(std::abs(args[0].as_int()));
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
        if (args[0].is_int() && args[1].is_int() && args[1].as_int() >= 0) {
            int64_t base = args[0].as_int(), exp = args[1].as_int(), res = 1;
            for (int64_t i = 0; i < exp; ++i) res *= base;
            return Value::make_int(res);
        }
        return Value::make_float(std::pow(args[0].as_float(), args[1].as_float()));
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
        if (v.is_float()) return Value::make_int(static_cast<int64_t>(v.as_float()));
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

    return run_interpreter();
}

VMResult VM::run_interpreter() {
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
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a == b));
                break;
            }
            case OpCode::OP_NOT_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a != b));
                break;
            }
            case OpCode::OP_GREATER: {
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a > b));
                break;
            }
            case OpCode::OP_GREATER_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a >= b));
                break;
            }
            case OpCode::OP_LESS: {
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a < b));
                break;
            }
            case OpCode::OP_LESS_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value::make_bool(a <= b));
                break;
            }

            case OpCode::OP_ADD: {
                Value b = pop();
                Value a = pop();
                if (a.is_int() && b.is_int()) {
                    push(Value::make_int(a.as_int() + b.as_int()));
                } else if (a.is_number() && b.is_number()) {
                    push(Value::make_float(a.as_float() + b.as_float()));
                } else if (a.is_string() || b.is_string()) {
                    push(Value::make_string(a.to_string() + b.to_string()));
                } else if (a.is_array() && b.is_array()) {
                    auto new_arr = *a.as_array();
                    const auto& r = *b.as_array();
                    new_arr.insert(new_arr.end(), r.begin(), r.end());
                    push(Value::make_array(std::move(new_arr)));
                } else {
                    runtime_error("operands must be numbers, strings, or arrays");
                    return VMResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::OP_SUBTRACT: {
                Value b = pop();
                Value a = pop();
                if (a.is_int() && b.is_int()) {
                    push(Value::make_int(a.as_int() - b.as_int()));
                } else if (a.is_number() && b.is_number()) {
                    push(Value::make_float(a.as_float() - b.as_float()));
                } else {
                    runtime_error("operands must be numbers for '-'");
                    return VMResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::OP_MULTIPLY: {
                Value b = pop();
                Value a = pop();
                if (a.is_int() && b.is_int()) {
                    push(Value::make_int(a.as_int() * b.as_int()));
                } else if (a.is_number() && b.is_number()) {
                    push(Value::make_float(a.as_float() * b.as_float()));
                } else {
                    runtime_error("operands must be numbers for '*'");
                    return VMResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::OP_DIVIDE: {
                Value b = pop();
                Value a = pop();
                if (b.as_float() == 0.0) {
                    runtime_error("division by zero");
                    return VMResult::RUNTIME_ERROR;
                }
                if (a.is_int() && b.is_int() && a.as_int() % b.as_int() == 0) {
                    push(Value::make_int(a.as_int() / b.as_int()));
                } else {
                    push(Value::make_float(a.as_float() / b.as_float()));
                }
                break;
            }
            case OpCode::OP_MODULO: {
                Value b = pop();
                Value a = pop();
                if (b.as_int() == 0) {
                    runtime_error("modulo by zero");
                    return VMResult::RUNTIME_ERROR;
                }
                push(Value::make_int(a.as_int() % b.as_int()));
                break;
            }
            case OpCode::OP_POWER: {
                Value b = pop();
                Value a = pop();
                if (a.is_int() && b.is_int() && b.as_int() >= 0) {
                    int64_t base = a.as_int(), exp = b.as_int(), res = 1;
                    for (int64_t i = 0; i < exp; ++i) res *= base;
                    push(Value::make_int(res));
                } else {
                    push(Value::make_float(std::pow(a.as_float(), b.as_float())));
                }
                break;
            }
            case OpCode::OP_NEGATE: {
                Value a = pop();
                if (a.is_int()) push(Value::make_int(-a.as_int()));
                else if (a.is_float()) push(Value::make_float(-a.as_float()));
                else {
                    runtime_error("operand must be a number for '-'");
                    return VMResult::RUNTIME_ERROR;
                }
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
                if (frame_count_ == 0) {
                    pop(); // pop main script function
                    return VMResult::OK;
                }

                stack_top_ = frame->slots;
                push(result);
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

            case OpCode::OP_INDEX_GET: {
                Value index = pop();
                Value target = pop();

                if (target.is_array()) {
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
                    auto it = obj->find(index.as_string());
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
                    (*target.as_object())[index.as_string()] = val;
                    push(val);
                    break;
                }

                runtime_error("cannot set index on " + target.type_name());
                return VMResult::RUNTIME_ERROR;
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
