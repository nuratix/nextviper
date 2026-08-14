#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"

using namespace nextviper;

static bool run_vm_script(const std::string& src, VM& vm, DiagnosticEngine& diag) {
    Lexer lexer(src, "vm_test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return vm.execute(*program) == VMResult::OK;
}

NV_TEST(VM, BasicArithmeticAndStack) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    VM vm(diag);

    std::string src = "let a = (10 + 20) * 2 - 5;\n"
                      "let b = 2 ** 8;\n"
                      "let c = 50 % 7;\n"
                      "let mut d = 100;\n"
                      "d = d + a;\n";
    NV_ASSERT_TRUE(run_vm_script(src, vm, diag));

    auto it_a = vm.globals().find("a");
    NV_ASSERT(it_a != vm.globals().end());
    NV_ASSERT_EQ(it_a->second.as_int(), 55);

    auto it_b = vm.globals().find("b");
    NV_ASSERT(it_b != vm.globals().end());
    NV_ASSERT_EQ(it_b->second.as_int(), 256);

    auto it_d = vm.globals().find("d");
    NV_ASSERT(it_d != vm.globals().end());
    NV_ASSERT_EQ(it_d->second.as_int(), 155);
}

NV_TEST(VM, FunctionCallsAndRecursion) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    VM vm(diag);

    std::string src = "fn fib(n) {\n"
                      "  if n <= 1 { return n; }\n"
                      "  return fib(n - 1) + fib(n - 2);\n"
                      "}\n"
                      "let res = fib(10);\n";
    NV_ASSERT_TRUE(run_vm_script(src, vm, diag));

    auto it_res = vm.globals().find("res");
    NV_ASSERT(it_res != vm.globals().end());
    NV_ASSERT_EQ(it_res->second.as_int(), 55);
}

NV_TEST(VM, LoopsAndArrays) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    VM vm(diag);

    std::string src = "let numbers = [10, 20, 30, 40, 50];\n"
                      "let mut total = 0;\n"
                      "for n in numbers {\n"
                      "  total += n;\n"
                      "}\n";
    NV_ASSERT_TRUE(run_vm_script(src, vm, diag));

    auto it_total = vm.globals().find("total");
    NV_ASSERT(it_total != vm.globals().end());
    NV_ASSERT_EQ(it_total->second.as_int(), 150);
}

NV_TEST(VM, PipelineTransformation) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    VM vm(diag);

    std::string src = "fn add_three(x) => x + 3;\n"
                      "fn square_it(x) => x * x;\n"
                      "let answer = 5 |> add_three() |> square_it();\n";
    NV_ASSERT_TRUE(run_vm_script(src, vm, diag));

    auto it_ans = vm.globals().find("answer");
    NV_ASSERT(it_ans != vm.globals().end());
    NV_ASSERT_EQ(it_ans->second.as_int(), 64); // (5 + 3)^2 = 64
}
