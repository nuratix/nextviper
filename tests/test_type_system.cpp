#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/type.hpp"
#include "nextviper/type_checker.hpp"

using namespace nextviper;

static bool type_check_script(const std::string& src, DiagnosticEngine& diag, TypeChecker& tc) {
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return tc.check(*program);
}

NV_TEST(TypeSystem, PromptPrimitivesAndInference) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    TypeChecker tc(diag);

    std::string src = "let name: string = \"Junaid\"\n"
                      "let age: int = 15\n"
                      "let score: float = 95.5\n"
                      "let active: bool = true\n"
                      "\n"
                      "let inferred_age = 15\n"
                      "let inferred_score = 95.5\n"
                      "let inferred_name = \"Junaid\"\n"
                      "let inferred_active = true\n"
                      "let inferred_sum = inferred_age + 10\n";

    NV_ASSERT_TRUE(type_check_script(src, diag, tc));

    auto t_name = tc.environment()->lookup("name");
    NV_ASSERT(t_name != nullptr);
    NV_ASSERT_TRUE(t_name->is_string());

    auto t_age = tc.environment()->lookup("age");
    NV_ASSERT(t_age != nullptr);
    NV_ASSERT_TRUE(t_age->is_int());

    auto t_score = tc.environment()->lookup("score");
    NV_ASSERT(t_score != nullptr);
    NV_ASSERT_TRUE(t_score->is_float());

    auto t_active = tc.environment()->lookup("active");
    NV_ASSERT(t_active != nullptr);
    NV_ASSERT_TRUE(t_active->is_bool());

    // Check inferred types
    auto t_inf_age = tc.environment()->lookup("inferred_age");
    NV_ASSERT(t_inf_age != nullptr);
    NV_ASSERT_TRUE(t_inf_age->is_int());

    auto t_inf_sum = tc.environment()->lookup("inferred_sum");
    NV_ASSERT(t_inf_sum != nullptr);
    NV_ASSERT_TRUE(t_inf_sum->is_int());
}

NV_TEST(TypeSystem, GenericCollections) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    TypeChecker tc(diag);

    std::string src = "let numbers: list[int] = [10, 20, 30]\n"
                      "let fruits: list[string] = [\"apple\", \"banana\"]\n"
                      "let user_scores: map[string, int] = {\"math\": 98, \"physics\": 95}\n"
                      "let inferred_list = [1, 2, 3]\n";

    NV_ASSERT_TRUE(type_check_script(src, diag, tc));

    auto t_numbers = tc.environment()->lookup("numbers");
    NV_ASSERT(t_numbers != nullptr);
    NV_ASSERT_TRUE(t_numbers->is_list());
    NV_ASSERT_TRUE(t_numbers->element_type()->is_int());

    auto t_scores = tc.environment()->lookup("user_scores");
    NV_ASSERT(t_scores != nullptr);
    NV_ASSERT_TRUE(t_scores->is_map());
    NV_ASSERT_TRUE(t_scores->key_type()->is_string());
    NV_ASSERT_TRUE(t_scores->value_type()->is_int());
}

NV_TEST(TypeSystem, FunctionSignatures) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    TypeChecker tc(diag);

    std::string src = "fn add(a: int, b: int) -> int:\n"
                      "    return a + b\n"
                      "\n"
                      "fn greet(name: string) -> string:\n"
                      "    return \"Hello, \" + name\n"
                      "\n"
                      "let res: int = add(10, 20)\n"
                      "let msg: string = greet(\"Junaid\")\n";

    NV_ASSERT_TRUE(type_check_script(src, diag, tc));

    auto t_fn = tc.environment()->lookup("add");
    NV_ASSERT(t_fn != nullptr);
    NV_ASSERT_TRUE(t_fn->is_function());
    NV_ASSERT_EQ(t_fn->param_types().size(), 2);
    NV_ASSERT_TRUE(t_fn->param_types()[0]->is_int());
    NV_ASSERT_TRUE(t_fn->param_types()[1]->is_int());
    NV_ASSERT_TRUE(t_fn->return_type()->is_int());
}

NV_TEST(TypeSystem, TensorAndFutureTypes) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    TypeChecker tc(diag);

    auto t_tensor = Type::parse("tensor[float]");
    NV_ASSERT(t_tensor != nullptr);
    NV_ASSERT_TRUE(t_tensor->is_tensor());
    NV_ASSERT_TRUE(t_tensor->element_type()->is_float());
    NV_ASSERT_EQ(t_tensor->to_string(), "tensor[float]");

    auto t_map = Type::parse("map[string, float]");
    NV_ASSERT(t_map != nullptr);
    NV_ASSERT_TRUE(t_map->is_map());
    NV_ASSERT_TRUE(t_map->key_type()->is_string());
    NV_ASSERT_TRUE(t_map->value_type()->is_float());
}

NV_TEST(TypeSystem, TypeMismatchDiagnostics) {
    // 1. Variable initialization mismatch
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        TypeChecker tc(diag);
        std::string src = "let age: int = \"not_an_int\"\n";
        NV_ASSERT_FALSE(type_check_script(src, diag, tc));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 2. Variable reassignment mismatch
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        TypeChecker tc(diag);
        std::string src = "let mut age: int = 15\n"
                          "age = \"fifteen\"\n";
        NV_ASSERT_FALSE(type_check_script(src, diag, tc));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 3. Function argument mismatch
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        TypeChecker tc(diag);
        std::string src = "fn add(a: int, b: int) -> int:\n"
                          "    return a + b\n"
                          "let bad = add(\"bad\", 20)\n";
        NV_ASSERT_FALSE(type_check_script(src, diag, tc));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 4. Function return type mismatch
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        TypeChecker tc(diag);
        std::string src = "fn compute() -> int:\n"
                          "    return \"string_instead_of_int\"\n";
        NV_ASSERT_FALSE(type_check_script(src, diag, tc));
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 5. Unsupported binary operands
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        TypeChecker tc(diag);
        std::string src = "let bad = \"hello\" - 5\n";
        NV_ASSERT_FALSE(type_check_script(src, diag, tc));
        NV_ASSERT_TRUE(diag.has_errors());
    }
}
