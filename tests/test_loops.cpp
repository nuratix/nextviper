#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"

using namespace nextviper;

static bool eval_script(const std::string& src, DiagnosticEngine& diag, Interpreter& interp) {
    Lexer lexer(src, "test_loops.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return interp.execute(*program);
}

static bool run_vm(const std::string& src, DiagnosticEngine& diag, VM& vm) {
    Lexer lexer(src, "test_loops_vm.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return vm.execute(*program) == VMResult::OK;
}

NV_TEST(Loops, RangeLoopsHalfOpenAndInclusive) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut sum1 = 0\n"
                      "for i in 0..10:\n"
                      "    sum1 = sum1 + i\n"
                      "\n"
                      "let mut sum2 = 0\n"
                      "for i in 0..=10:\n"
                      "    sum2 = sum2 + i\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto s1 = interp.globals()->get("sum1");
    NV_ASSERT_TRUE(s1.has_value() && s1->is_int() && s1->as_int() == 45); // 0+1+..+9 = 45

    auto s2 = interp.globals()->get("sum2");
    NV_ASSERT_TRUE(s2.has_value() && s2->is_int() && s2->as_int() == 55); // 0+1+..+10 = 55
}

NV_TEST(Loops, WhileLoopWithCondition) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut count = 0\n"
                      "let mut val = 1\n"
                      "while val < 100:\n"
                      "    val = val * 2\n"
                      "    count = count + 1\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto v = interp.globals()->get("val");
    NV_ASSERT_TRUE(v.has_value() && v->is_int() && v->as_int() == 128);

    auto c = interp.globals()->get("count");
    NV_ASSERT_TRUE(c.has_value() && c->is_int() && c->as_int() == 7);
}

NV_TEST(Loops, BreakAndContinue) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut evens_sum = 0\n"
                      "for i in 0..20:\n"
                      "    if i >= 10:\n"
                      "        break\n"
                      "    if i % 2 != 0:\n"
                      "        continue\n"
                      "    evens_sum = evens_sum + i\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    // 0 + 2 + 4 + 6 + 8 = 20
    auto res = interp.globals()->get("evens_sum");
    NV_ASSERT_TRUE(res.has_value() && res->is_int() && res->as_int() == 20);
}

NV_TEST(Loops, NestedLoopsMatrixAndBreak) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut product_sum = 0\n"
                      "let mut inner_breaks = 0\n"
                      "for i in 1..=4:\n"
                      "    for j in 1..=4:\n"
                      "        if j > 2:\n"
                      "            inner_breaks = inner_breaks + 1\n"
                      "            break\n"
                      "        product_sum = product_sum + (i * j)\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    // for each i in 1..4:
    //   j = 1: prod += i*1
    //   j = 2: prod += i*2
    //   j = 3: inner_breaks++, break
    // Sum = (1+2)*1 + (1+2)*2 + (1+2)*3 + (1+2)*4 = 3 * 10 = 30
    auto psum = interp.globals()->get("product_sum");
    NV_ASSERT_TRUE(psum.has_value() && psum->is_int() && psum->as_int() == 30);

    auto ib = interp.globals()->get("inner_breaks");
    NV_ASSERT_TRUE(ib.has_value() && ib->is_int() && ib->as_int() == 4);
}

NV_TEST(Loops, EdgeCasesEmptyRangesAndCollections) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let mut count_empty_range = 0\n"
                      "for i in 10..10:\n"
                      "    count_empty_range = count_empty_range + 1\n"
                      "\n"
                      "let mut count_empty_list = 0\n"
                      "for x in []:\n"
                      "    count_empty_list = count_empty_list + 1\n"
                      "\n"
                      "let mut single_item_seen = 0\n"
                      "for x in [999]:\n"
                      "    single_item_seen = x\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto cer = interp.globals()->get("count_empty_range");
    NV_ASSERT_TRUE(cer.has_value() && cer->is_int() && cer->as_int() == 0);

    auto cel = interp.globals()->get("count_empty_list");
    NV_ASSERT_TRUE(cel.has_value() && cel->is_int() && cel->as_int() == 0);

    auto sis = interp.globals()->get("single_item_seen");
    NV_ASSERT_TRUE(sis.has_value() && sis->is_int() && sis->as_int() == 999);
}

NV_TEST(Loops, VMExecutionLoopsAndContinues) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    VM vm(diag);

    std::string src = "let mut sum = 0\n"
                      "for i in 0..10:\n"
                      "    if i % 2 != 0:\n"
                      "        continue\n"
                      "    sum = sum + i\n"
                      "\n"
                      "let mut total_3d = 0\n"
                      "for x in 0..3:\n"
                      "    for y in 0..3:\n"
                      "        for z in 0..3:\n"
                      "            total_3d = total_3d + 1\n";

    NV_ASSERT_TRUE(run_vm(src, diag, vm));

    // Even sum 0+2+4+6+8 = 20
    auto it_sum = vm.globals().find("sum");
    NV_ASSERT_TRUE(it_sum != vm.globals().end() && it_sum->second.is_int() && it_sum->second.as_int() == 20);

    // 3 * 3 * 3 = 27
    auto it_3d = vm.globals().find("total_3d");
    NV_ASSERT_TRUE(it_3d != vm.globals().end() && it_3d->second.is_int() && it_3d->second.as_int() == 27);
}
