#pragma once

#include "nextviper/compiled_function.hpp"
#include "nextviper/value.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/ast.hpp"
#include <array>
#include <unordered_map>
#include <memory>
#include <string>

namespace nextviper {

enum class VMResult {
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR
};

struct CallFrame {
    std::shared_ptr<CompiledFunction> function;
    const uint8_t* ip = nullptr;
    Value* slots = nullptr;
};

class VM {
public:
    static constexpr size_t STACK_MAX = 4096;
    static constexpr size_t FRAMES_MAX = 256;

    explicit VM(DiagnosticEngine& diagnostics);

    VMResult run(std::shared_ptr<CompiledFunction> function);
    VMResult execute(const Program& program);

    void define_global(const std::string& name, Value value);
    void define_native(const std::string& name, int arity, NativeFn func);

    void reset_stack();
    void push(Value value);
    Value pop();
    Value peek(int distance = 0) const;

    const std::unordered_map<std::string, Value>& globals() const { return globals_; }

private:
    VMResult run_interpreter();
    void init_builtins();
    void runtime_error(const std::string& message);

    DiagnosticEngine& diagnostics_;
    std::array<CallFrame, FRAMES_MAX> frames_;
    size_t frame_count_ = 0;

    std::array<Value, STACK_MAX> stack_;
    Value* stack_top_ = nullptr;

    std::unordered_map<std::string, Value> globals_;
};

} // namespace nextviper
