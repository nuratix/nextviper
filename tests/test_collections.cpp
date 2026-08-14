#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"
#include <sstream>

using namespace nextviper;

static bool eval_script(const std::string& src, DiagnosticEngine& diag, Interpreter& interp) {
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return interp.execute(*program);
}

NV_TEST(Collections, ListIndexingAndSlicing) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let numbers = [10, 20, 30, 40, 50]\n"
                      "let first = numbers[0]\n"
                      "let last = numbers[-1]\n"
                      "let mid_slice = numbers[1:4]\n"
                      "let prefix_slice = numbers[:3]\n"
                      "let suffix_slice = numbers[2:]\n"
                      "let step_slice = numbers[::2]\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto first = interp.globals()->get("first");
    NV_ASSERT_TRUE(first.has_value() && first->is_int() && first->as_int() == 10);

    auto last = interp.globals()->get("last");
    NV_ASSERT_TRUE(last.has_value() && last->is_int() && last->as_int() == 50);

    auto mid = interp.globals()->get("mid_slice");
    NV_ASSERT_TRUE(mid.has_value() && mid->is_array() && mid->as_array()->size() == 3);
    NV_ASSERT_EQ((*mid->as_array())[0].as_int(), 20);
    NV_ASSERT_EQ((*mid->as_array())[2].as_int(), 40);

    auto pref = interp.globals()->get("prefix_slice");
    NV_ASSERT_TRUE(pref.has_value() && pref->is_array() && pref->as_array()->size() == 3);

    auto suff = interp.globals()->get("suffix_slice");
    NV_ASSERT_TRUE(suff.has_value() && suff->is_array() && suff->as_array()->size() == 3);

    auto step = interp.globals()->get("step_slice");
    NV_ASSERT_TRUE(step.has_value() && step->is_array() && step->as_array()->size() == 3);
    NV_ASSERT_EQ((*step->as_array())[0].as_int(), 10);
    NV_ASSERT_EQ((*step->as_array())[1].as_int(), 30);
    NV_ASSERT_EQ((*step->as_array())[2].as_int(), 50);
}

NV_TEST(Collections, MapCreationAndOperations) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let user = {\n"
                      "    \"name\": \"Junaid\",\n"
                      "    \"age\": 15,\n"
                      "    \"city\": \"Tokyo\"\n"
                      "}\n"
                      "let u_name = user[\"name\"]\n"
                      "let u_age = user[\"age\"]\n"
                      "let u_len = len(user)\n"
                      "let u_has_name = has(user, \"name\")\n"
                      "let u_has_score = has(user, \"score\")\n"
                      "let u_keys = keys(user)\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto name = interp.globals()->get("u_name");
    NV_ASSERT_TRUE(name.has_value() && name->is_string() && name->as_string() == "Junaid");

    auto age = interp.globals()->get("u_age");
    NV_ASSERT_TRUE(age.has_value() && age->is_int() && age->as_int() == 15);

    auto l = interp.globals()->get("u_len");
    NV_ASSERT_TRUE(l.has_value() && l->is_int() && l->as_int() == 3);

    auto has_name = interp.globals()->get("u_has_name");
    NV_ASSERT_TRUE(has_name.has_value() && has_name->is_bool() && has_name->as_bool() == true);

    auto has_score = interp.globals()->get("u_has_score");
    NV_ASSERT_TRUE(has_score.has_value() && has_score->is_bool() && has_score->as_bool() == false);
}

NV_TEST(Collections, AppendRemoveInsert) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let items = [1, 2, 3]\n"
                      "append(items, 4)\n"
                      "insert(items, 0, 0)\n"
                      "let popped_last = pop(items)\n"
                      "let popped_first = pop(items, 0)\n"
                      "let final_len = len(items)\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto items = interp.globals()->get("items");
    NV_ASSERT_TRUE(items.has_value() && items->is_array());
    NV_ASSERT_EQ(items->as_array()->size(), 3);
    NV_ASSERT_EQ((*items->as_array())[0].as_int(), 1);
    NV_ASSERT_EQ((*items->as_array())[1].as_int(), 2);
    NV_ASSERT_EQ((*items->as_array())[2].as_int(), 3);

    auto p_last = interp.globals()->get("popped_last");
    NV_ASSERT_TRUE(p_last.has_value() && p_last->is_int() && p_last->as_int() == 4);

    auto p_first = interp.globals()->get("popped_first");
    NV_ASSERT_TRUE(p_first.has_value() && p_first->is_int() && p_first->as_int() == 0);
}

NV_TEST(Collections, MapFilterReduce) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let numbers = [1, 2, 3, 4, 5]\n"
                      "fn square(x): return x * x\n"
                      "fn is_even(x): return x % 2 == 0\n"
                      "fn add_acc(acc, x): return acc + x\n"
                      "\n"
                      "let squared = map(numbers, square)\n"
                      "let evens = filter(numbers, is_even)\n"
                      "let total_sum = reduce(numbers, add_acc, 0)\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto sq = interp.globals()->get("squared");
    NV_ASSERT_TRUE(sq.has_value() && sq->is_array() && sq->as_array()->size() == 5);
    NV_ASSERT_EQ((*sq->as_array())[0].as_int(), 1);
    NV_ASSERT_EQ((*sq->as_array())[1].as_int(), 4);
    NV_ASSERT_EQ((*sq->as_array())[2].as_int(), 9);
    NV_ASSERT_EQ((*sq->as_array())[3].as_int(), 16);
    NV_ASSERT_EQ((*sq->as_array())[4].as_int(), 25);

    auto ev = interp.globals()->get("evens");
    NV_ASSERT_TRUE(ev.has_value() && ev->is_array() && ev->as_array()->size() == 2);
    NV_ASSERT_EQ((*ev->as_array())[0].as_int(), 2);
    NV_ASSERT_EQ((*ev->as_array())[1].as_int(), 4);

    auto sum = interp.globals()->get("total_sum");
    NV_ASSERT_TRUE(sum.has_value() && sum->is_int() && sum->as_int() == 15);
}

NV_TEST(Collections, ForInIteration) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "let list_items = [10, 20, 30]\n"
                      "let mut total = 0\n"
                      "for x in list_items:\n"
                      "    total = total + x\n"
                      "\n"
                      "let user_data = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
                      "let mut key_count = 0\n"
                      "for k in user_data:\n"
                      "    key_count = key_count + 1\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto total = interp.globals()->get("total");
    NV_ASSERT_TRUE(total.has_value() && total->is_int() && total->as_int() == 60);

    auto kcount = interp.globals()->get("key_count");
    NV_ASSERT_TRUE(kcount.has_value() && kcount->is_int() && kcount->as_int() == 3);
}

NV_TEST(Collections, VMExecutionAndSlicing) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "let nums = [1, 2, 3, 4, 5]\n"
                      "let sliced = nums[1:4]\n"
                      "let total = len(sliced)\n";

    Lexer lexer(src, "test_vm_coll.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    VM vm(diag);
    VMResult res = vm.execute(*program);
    NV_ASSERT_TRUE(res == VMResult::OK);

    auto it = vm.globals().find("total");
    NV_ASSERT_TRUE(it != vm.globals().end() && it->second.is_int() && it->second.as_int() == 3);
}
