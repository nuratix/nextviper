#include "test_runner.hpp"
#include "nextviper/linter.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"

using namespace nextviper;

static std::pair<size_t, size_t> lint_code(const std::string& code) {
    SourceManager sm;
    sm.add_file("<test>", code);
    DiagnosticEngine diag(sm, false);

    Lexer lexer(code, "<test>", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    if (!program) return {0, 1};

    Linter linter(diag);
    linter.lint_program(*program, "<test>");
    return {linter.warning_count(), linter.error_count()};
}

NV_TEST(Linter, CleanCodePassesWithZeroWarnings) {
    std::string clean = 
        "import std.io\n"
        "fn calculate(a: int, b: int) -> int:\n"
        "    return a + b\n"
        "let result = calculate(10, 20)\n"
        "io.print(result)\n";

    auto [warns, errs] = lint_code(clean);
    NV_ASSERT_EQ(warns, 0);
    NV_ASSERT_EQ(errs, 0);
}

NV_TEST(Linter, DetectsUnusedVariables) {
    std::string code = 
        "fn test():\n"
        "    let unused_var = 42\n"
        "    let _intentional = 100\n"
        "    return _intentional\n"
        "test()\n";

    auto [warns, errs] = lint_code(code);
    NV_ASSERT_EQ(warns, 1); // unused_var should be flagged, _intentional should not
}

NV_TEST(Linter, DetectsUnreachableCode) {
    std::string code = 
        "fn test():\n"
        "    return 42\n"
        "    let x = 100\n"
        "test()\n";

    auto [warns, errs] = lint_code(code);
    NV_ASSERT_EQ(warns, 1); // unreachable code
}

NV_TEST(Linter, DetectsRedundantSelfComparison) {
    std::string code = 
        "fn test(x: int) -> bool:\n"
        "    return x == x\n"
        "test(5)\n";

    auto [warns, errs] = lint_code(code);
    NV_ASSERT_EQ(warns, 1); // x == x redundant
}
