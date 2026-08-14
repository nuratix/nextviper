#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/type_checker.hpp"
#include "nextviper/interpreter.hpp"
#include <random>
#include <string>
#include <vector>

using namespace nextviper;

// Helper to run full pipeline on input string and assert no crash
static void run_pipeline_safely(const std::string& input) {
    SourceManager sm;
    sm.add_file("<fuzz_input>", input);
    DiagnosticEngine diag(sm, false);

    try {
        Lexer lexer(input, "<fuzz_input>", diag);
        auto tokens = lexer.tokenize();

        if (diag.has_errors()) return;

        Parser parser(tokens, diag);
        auto program = parser.parse_program();

        if (diag.has_errors() || !program) return;

        TypeChecker checker(diag);
        checker.check(*program);

        if (diag.has_errors()) return;

        Interpreter interpreter(diag);
        interpreter.execute(*program);
    } catch (const RuntimeError&) {
        // Expected runtime exceptions are caught gracefully
    } catch (const std::exception&) {
        // Standard exceptions are caught gracefully
    }
}

NV_TEST(Fuzz, LexerMalformedInputs) {
    std::vector<std::string> malformed_inputs = {
        "\"",
        "\"unterminated string without closing quote",
        "\"escaped \\\" quote without end",
        "0b10201",
        "0xGHIJK",
        "0o89",
        "123.456.789",
        "....",
        "/* unclosed comment",
        "/* nested /* comment */ without close",
        "\0\0\0",
        "\xff\xfe\xfd\x80\x81",
        "let x = @#$%^&",
        "\"\\u1234 invalid unicode \\u999999\"",
        "1e+9999999999999999999999999999999999999999"
    };

    for (const auto& input : malformed_inputs) {
        SourceManager sm;
        sm.add_file("<test>", input);
        DiagnosticEngine diag(sm, false);
        Lexer lexer(input, "<test>", diag);
        auto tokens = lexer.tokenize();
        NV_ASSERT_TRUE(!tokens.empty());
        NV_ASSERT_EQ(tokens.back().type, TokenType::EOF_TOKEN);
    }
}

NV_TEST(Fuzz, ParserDeepNestingDepthLimit) {
    // Generate 600 nested parentheses to test nesting limit guard
    std::string deep_parens;
    for (int i = 0; i < 600; ++i) deep_parens += "(";
    deep_parens += "1";
    for (int i = 0; i < 600; ++i) deep_parens += ")";

    SourceManager sm;
    sm.add_file("<test>", deep_parens);
    DiagnosticEngine diag(sm, false);

    Lexer lexer(deep_parens, "<test>", diag);
    auto tokens = lexer.tokenize();

    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    // Parser should catch depth limit and report error without crashing or segfaulting
    NV_ASSERT_TRUE(diag.has_errors());
}

NV_TEST(Fuzz, ParserMalformedStructures) {
    std::vector<std::string> malformed_programs = {
        "fn () -> :",
        "let = ;",
        "if : else : elif :",
        "while :",
        "for in :",
        "[1, 2, , 4]",
        "{ \"a\": , : 2 }",
        "let x = 1 + + + 2",
        "fn f(a, , b): return a",
        "return return return",
        "break continue",
        "import",
        "from import",
        "let x = (1 + 2",
        "let x = [1, 2, 3",
        "let x = {\"k\": 1"
    };

    for (const auto& code : malformed_programs) {
        SourceManager sm;
        sm.add_file("<test>", code);
        DiagnosticEngine diag(sm, false);
        Lexer lexer(code, "<test>", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse_program();
        // Malformed code must be handled without crash
        NV_ASSERT_TRUE(program != nullptr || diag.has_errors());
    }
}

NV_TEST(Fuzz, TypeCheckerMalformedInputs) {
    std::vector<std::string> type_error_codes = {
        "let x: int = \"hello\"",
        "let y: bool = 123",
        "let z: list[int] = \"string\"",
        "fn add(a: int, b: int) -> int: return \"wrong\"",
        "let a = missing_var + 10"
    };

    for (const auto& code : type_error_codes) {
        SourceManager sm;
        sm.add_file("<test>", code);
        DiagnosticEngine diag(sm, false);
        Lexer lexer(code, "<test>", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse_program();
        if (program && !diag.has_errors()) {
            TypeChecker checker(diag);
            checker.check(*program);
            NV_ASSERT_TRUE(diag.has_errors());
        }
    }
}

NV_TEST(Fuzz, InterpreterRecursionLimitProtection) {
    std::string infinite_recursion = 
        "fn recurse(n):\n"
        "    return recurse(n + 1)\n"
        "recurse(0)\n";

    SourceManager sm;
    sm.add_file("<test>", infinite_recursion);
    DiagnosticEngine diag(sm, false);
    Lexer lexer(infinite_recursion, "<test>", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interpreter(diag);
    bool ok = interpreter.execute(*program);
    NV_ASSERT_FALSE(ok);
    NV_ASSERT_TRUE(diag.has_errors());
}

NV_TEST(Fuzz, InterpreterResourceLimits) {
    // Test oversized range allocation protection
    std::string oversized_range = "let r = range(0, 1000000000, 1)";
    SourceManager sm;
    sm.add_file("<test>", oversized_range);
    DiagnosticEngine diag(sm, false);
    Lexer lexer(oversized_range, "<test>", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    Interpreter interpreter(diag);
    bool ok = interpreter.execute(*program);
    NV_ASSERT_FALSE(ok);
    NV_ASSERT_TRUE(diag.has_errors());
}

NV_TEST(Fuzz, RandomizedMutationFuzzing) {
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> char_dist(0, 255);
    std::uniform_int_distribution<int> len_dist(1, 200);

    const std::vector<std::string> seeds = {
        "let x = 10\nlet y = 20\nprint(x + y)",
        "fn f(a, b): return a * b\nprint(f(3, 4))",
        "for i in 0..10: print(i)",
        "if x > 0: print(\"positive\") else: print(\"non-positive\")",
        "let lst = [1, 2, 3]; lst.append(4)",
        "let m = {\"a\": 1, \"b\": 2}; print(m[\"a\"])",
        "import math\nprint(math.sqrt(16))",
        "import tensor\nlet t = tensor.ones([2, 2])"
    };

    // 1. Pure random binary/ascii fuzzing (100 iterations)
    for (int iter = 0; iter < 100; ++iter) {
        int len = len_dist(rng);
        std::string random_str;
        random_str.reserve(len);
        for (int i = 0; i < len; ++i) {
            random_str.push_back(static_cast<char>(char_dist(rng)));
        }
        run_pipeline_safely(random_str);
    }

    // 2. Mutation-based fuzzing from valid seeds (100 iterations)
    for (int iter = 0; iter < 100; ++iter) {
        std::string mutated = seeds[iter % seeds.size()];
        int mutations = (rng() % 5) + 1;
        for (int m = 0; m < mutations && !mutated.empty(); ++m) {
            int op = rng() % 3;
            size_t pos = rng() % mutated.size();
            if (op == 0) {
                // Byte substitution
                mutated[pos] = static_cast<char>(char_dist(rng));
            } else if (op == 1) {
                // Byte insertion
                mutated.insert(pos, 1, static_cast<char>(char_dist(rng)));
            } else {
                // Byte deletion
                mutated.erase(pos, 1);
            }
        }
        run_pipeline_safely(mutated);
    }

    NV_ASSERT_TRUE(true);
}
