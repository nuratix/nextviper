#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/ir.hpp"
#include "nextviper/native_compiler.hpp"

using namespace nextviper;

static std::unique_ptr<Program> parse_src(const std::string& src, DiagnosticEngine& diag) {
    Lexer lexer(src, "test_native.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    return parser.parse_program();
}

NV_TEST(NativeCompiler, UserPromptExampleCompileAndRun) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = 
        "let x = 10\n"
        "let y = 20\n"
        "print(x + y)\n";

    auto program = parse_src(src, diag);
    NV_ASSERT_TRUE(program != nullptr);
    NV_ASSERT_FALSE(diag.has_errors());

    NativeCompiler compiler(diag);
    auto [exit_code, output] = compiler.compile_and_run(*program, true);

    NV_ASSERT_EQ(exit_code, 0);
    NV_ASSERT_EQ(output, "30\n");
}

NV_TEST(NativeCompiler, IRGenerationAndConstantFolding) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = 
        "let a = 15 + 25\n"
        "let b = 100 * 2\n"
        "print(a)\n";

    auto program = parse_src(src, diag);
    NV_ASSERT_TRUE(program != nullptr);

    IRGenerator gen(diag);
    auto ir_mod = gen.generate(*program);
    NV_ASSERT_TRUE(ir_mod != nullptr);

    // Optimize with constant folding & DCE
    IROptimizer opt;
    opt.optimize(*ir_mod);

    std::string ir_str = ir_mod->to_string();
    NV_ASSERT_TRUE(ir_str.find("40") != std::string::npos);

    NativeCompiler compiler(diag);
    auto [exit_code, output] = compiler.compile_and_run(*program, true);
    NV_ASSERT_EQ(exit_code, 0);
    NV_ASSERT_EQ(output, "40\n");
}

NV_TEST(NativeCompiler, FunctionsAndLoopsCompiledExecution) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = 
        "fn multiply(a, b):\n"
        "    return a * b\n"
        "\n"
        "let x = 6\n"
        "let y = 7\n"
        "let ans = multiply(x, y)\n"
        "print(ans)\n";

    auto program = parse_src(src, diag);
    NV_ASSERT_TRUE(program != nullptr);

    NativeCompiler compiler(diag);
    auto [exit_code, output] = compiler.compile_and_run(*program, true);
    NV_ASSERT_EQ(exit_code, 0);
    NV_ASSERT_EQ(output, "42\n");
}

NV_TEST(NativeCompiler, BenchmarkSuiteExecution) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = 
        "let a = 10\n"
        "let b = 20\n"
        "let c = a + b\n";

    auto program = parse_src(src, diag);
    NV_ASSERT_TRUE(program != nullptr);

    NativeCompiler compiler(diag);
    auto bench = compiler.benchmark(*program, src, 50);

    NV_ASSERT_TRUE(bench.interpreter_ms > 0.0);
    NV_ASSERT_TRUE(bench.native_ms > 0.0);
}
