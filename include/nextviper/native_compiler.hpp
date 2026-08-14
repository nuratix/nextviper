#pragma once

#include "nextviper/common.hpp"
#include "nextviper/ast.hpp"
#include "nextviper/ir.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>
#include <vector>
#include <memory>

namespace nextviper {

struct BenchmarkResult {
    double interpreter_ms = 0.0;
    double vm_ms = 0.0;
    double native_ms = 0.0;
    double native_speedup_vs_interpreter = 1.0;
    double native_speedup_vs_vm = 1.0;
    int iterations = 1;
};

class NativeCompiler {
public:
    explicit NativeCompiler(DiagnosticEngine& diag);

    // Converts IR to portable native C/machine code source
    std::string emit_native_source(const IRModule& module) const;

    // Compiles IRModule or AST Program to native machine code binary
    bool compile_to_binary(const IRModule& module, const std::string& output_binary_path);
    bool compile_program(const Program& program, const std::string& output_binary_path, bool optimize = true);

    // Compiles and runs directly, returning stdout
    std::pair<int, std::string> compile_and_run(const Program& program, bool optimize = true);

    // Benchmarks AST Interpreter vs VM vs Native Compiled
    BenchmarkResult benchmark(const Program& program, const std::string& source_code, int iterations = 100);

private:
    DiagnosticEngine& diag_;
    std::string find_c_compiler() const;
};

} // namespace nextviper
