#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"

using namespace nextviper;

static bool eval_script(const std::string& src, Interpreter& interp, DiagnosticEngine& diag) {
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return interp.execute(*program);
}

NV_TEST(Interpreter, ArithmeticAndPrecedence) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let x = 2 + 3 * 4;\n"
                      "let y = (2 + 3) * 4;\n"
                      "let z = 2 ** 3;\n"
                      "let f = 10.5 / 2.0;\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vx = interp.globals()->get("x");
    NV_ASSERT(vx.has_value());
    NV_ASSERT_EQ(vx->as_int(), 14);

    auto vy = interp.globals()->get("y");
    NV_ASSERT(vy.has_value());
    NV_ASSERT_EQ(vy->as_int(), 20);

    auto vz = interp.globals()->get("z");
    NV_ASSERT(vz.has_value());
    NV_ASSERT_EQ(vz->as_int(), 8);

    auto vf = interp.globals()->get("f");
    NV_ASSERT(vf.has_value());
    NV_ASSERT_EQ(vf->as_float(), 5.25);
}

NV_TEST(Interpreter, ImmutabilityEnforcement) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    // Reassigning immutable variable should fail
    std::string src1 = "let a = 10; a = 20;";
    NV_ASSERT_FALSE(eval_script(src1, interp, diag));

    // Reassigning mutable variable should succeed
    diag.clear();
    std::string src2 = "let mut b = 10; b = 20; b += 5;";
    NV_ASSERT_TRUE(eval_script(src2, interp, diag));
    auto vb = interp.globals()->get("b");
    NV_ASSERT(vb.has_value());
    NV_ASSERT_EQ(vb->as_int(), 25);
}

NV_TEST(Interpreter, FunctionsAndRecursion) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "fn factorial(n) {\n"
                      "  if n <= 1 { return 1; }\n"
                      "  return n * factorial(n - 1);\n"
                      "}\n"
                      "let res = factorial(6);\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vres = interp.globals()->get("res");
    NV_ASSERT(vres.has_value());
    NV_ASSERT_EQ(vres->as_int(), 720);
}

NV_TEST(Interpreter, ArrowFunctionsAndClosures) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "fn make_adder(x) {\n"
                      "  fn add(y) => x + y;\n"
                      "  return add;\n"
                      "}\n"
                      "let add10 = make_adder(10);\n"
                      "let r = add10(32);\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vr = interp.globals()->get("r");
    NV_ASSERT(vr.has_value());
    NV_ASSERT_EQ(vr->as_int(), 42);
}

NV_TEST(Interpreter, LoopsAndBreakContinue) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut sum = 0;\n"
                      "for x in [1, 2, 3, 4, 5] {\n"
                      "  if x == 4 { continue; }\n"
                      "  if x == 5 { break; }\n"
                      "  sum += x;\n"
                      "}\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vsum = interp.globals()->get("sum");
    NV_ASSERT(vsum.has_value());
    NV_ASSERT_EQ(vsum->as_int(), 6); // 1 + 2 + 3 = 6 (skipped 4, stopped at 5)
}

NV_TEST(Interpreter, PipelineOperator) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "fn double(x) => x * 2;\n"
                      "fn add5(x) => x + 5;\n"
                      "let val = 10 |> double() |> add5();\n"
                      "let length = [10, 20, 30] |> len();\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vval = interp.globals()->get("val");
    NV_ASSERT(vval.has_value());
    NV_ASSERT_EQ(vval->as_int(), 25); // (10 * 2) + 5 = 25

    auto vlen = interp.globals()->get("length");
    NV_ASSERT(vlen.has_value());
    NV_ASSERT_EQ(vlen->as_int(), 3);
}

NV_TEST(Interpreter, BuiltinFunctions) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let t_str = typeof(\"hello\");\n"
                      "let t_num = typeof(42);\n"
                      "let r = range(0, 5);\n"
                      "let l = len(r);\n"
                      "let m = max(10, 50);\n"
                      "let p = pow(2, 4);\n"
                      "let s = sqrt(16.0);\n"
                      "assert(s == 4.0, \"sqrt failed\");\n";
    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    NV_ASSERT_EQ(interp.globals()->get("t_str")->as_string(), "String");
    NV_ASSERT_EQ(interp.globals()->get("t_num")->as_string(), "Int");
    NV_ASSERT_EQ(interp.globals()->get("l")->as_int(), 5);
    NV_ASSERT_EQ(interp.globals()->get("m")->as_int(), 50);
    NV_ASSERT_EQ(interp.globals()->get("p")->as_int(), 16);
    NV_ASSERT_EQ(interp.globals()->get("s")->as_float(), 4.0);
}
