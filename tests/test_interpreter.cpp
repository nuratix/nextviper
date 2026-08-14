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

NV_TEST(Interpreter, UserPromptExample) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let x = 10\n"
                      "let y = 20\n"
                      "\n"
                      "if x < y:\n"
                      "    print(\"x is smaller\")\n"
                      "\n"
                      "let sum = x + y\n"
                      "print(sum)\n";

    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto vx = interp.globals()->get("x");
    NV_ASSERT(vx.has_value());
    NV_ASSERT_EQ(vx->as_int(), 10);

    auto vy = interp.globals()->get("y");
    NV_ASSERT(vy.has_value());
    NV_ASSERT_EQ(vy->as_int(), 20);

    auto vsum = interp.globals()->get("sum");
    NV_ASSERT(vsum.has_value());
    NV_ASSERT_EQ(vsum->as_int(), 30);
}

NV_TEST(Interpreter, VariablesAndScopes) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let global_var = 100\n"
                      "let mut outer_mut = 50\n"
                      "{\n"
                      "    let inner_var = 200\n"
                      "    outer_mut = outer_mut + inner_var\n"
                      "}\n"
                      "let final_outer = outer_mut\n";

    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    auto v_global = interp.globals()->get("global_var");
    NV_ASSERT(v_global.has_value());
    NV_ASSERT_EQ(v_global->as_int(), 100);

    auto v_outer = interp.globals()->get("final_outer");
    NV_ASSERT(v_outer.has_value());
    NV_ASSERT_EQ(v_outer->as_int(), 250);

    // inner_var should not exist in global scope
    auto v_inner = interp.globals()->get("inner_var");
    NV_ASSERT_FALSE(v_inner.has_value());
}

NV_TEST(Interpreter, ArithmeticAndComparisons) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let a = 15 + 25\n"
                      "let b = 100 - 30\n"
                      "let c = 6 * 7\n"
                      "let d = 80 / 4\n"
                      "let e = 17 % 5\n"
                      "let f = 2 ** 8\n"
                      "let cmp1 = 10 < 20\n"
                      "let cmp2 = 20 <= 20\n"
                      "let cmp3 = 50 > 30\n"
                      "let cmp4 = 40 >= 40\n"
                      "let cmp5 = 42 == 42\n"
                      "let cmp6 = 42 != 99\n";

    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    NV_ASSERT_EQ(interp.globals()->get("a")->as_int(), 40);
    NV_ASSERT_EQ(interp.globals()->get("b")->as_int(), 70);
    NV_ASSERT_EQ(interp.globals()->get("c")->as_int(), 42);
    NV_ASSERT_EQ(interp.globals()->get("d")->as_int(), 20);
    NV_ASSERT_EQ(interp.globals()->get("e")->as_int(), 2);
    NV_ASSERT_EQ(interp.globals()->get("f")->as_int(), 256);

    NV_ASSERT_TRUE(interp.globals()->get("cmp1")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("cmp2")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("cmp3")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("cmp4")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("cmp5")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("cmp6")->as_bool());
}

NV_TEST(Interpreter, LogicalOperatorsAndNull) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let l1 = true and false\n"
                      "let l2 = false or true\n"
                      "let l3 = not false\n"
                      "let l4 = !true\n"
                      "let n1 = null\n"
                      "let n2 = nil\n"
                      "let is_n1_nil = n1 == nil\n";

    NV_ASSERT_TRUE(eval_script(src, interp, diag));

    NV_ASSERT_FALSE(interp.globals()->get("l1")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("l2")->as_bool());
    NV_ASSERT_TRUE(interp.globals()->get("l3")->as_bool());
    NV_ASSERT_FALSE(interp.globals()->get("l4")->as_bool());

    NV_ASSERT_TRUE(interp.globals()->get("n1")->is_nil());
    NV_ASSERT_TRUE(interp.globals()->get("n2")->is_nil());
    NV_ASSERT_TRUE(interp.globals()->get("is_n1_nil")->as_bool());
}

NV_TEST(Interpreter, RuntimeErrorsDetection) {
    // 1. NameError on undefined variable
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        Interpreter interp(diag);
        NV_ASSERT_FALSE(eval_script("let x = undefined_variable_name + 10", interp, diag));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 2. DivisionByZeroError
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        Interpreter interp(diag);
        NV_ASSERT_FALSE(eval_script("let x = 10 / 0", interp, diag));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 3. MutabilityError on reassigning immutable variable
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        Interpreter interp(diag);
        NV_ASSERT_FALSE(eval_script("let constant = 42; constant = 99;", interp, diag));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 4. TypeError on invalid operands
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        Interpreter interp(diag);
        NV_ASSERT_FALSE(eval_script("let bad = \"text\" - 5", interp, diag));
        NV_ASSERT_TRUE(diag.has_errors());
    }
}
