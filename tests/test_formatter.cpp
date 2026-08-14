#include "test_runner.hpp"
#include "nextviper/formatter.hpp"

using namespace nextviper;

NV_TEST(Formatter, DeterministicAndIdempotent) {
    std::string unformatted = 
        "let x=10+20\n"
        "let y =  x * 2\n"
        "if y>30:\n"
        "print(\"Large\")\n"
        "else:\n"
        "print(\"Small\")\n";

    std::string formatted_1 = Formatter::format_source(unformatted);
    std::string formatted_2 = Formatter::format_source(formatted_1);

    // Idempotency: formatting twice produces the exact same result
    NV_ASSERT_EQ(formatted_1, formatted_2);

    // Verify indentation
    NV_ASSERT_TRUE(formatted_1.find("    print(\"Large\")") != std::string::npos);
    NV_ASSERT_TRUE(formatted_1.find("    print(\"Small\")") != std::string::npos);
}

NV_TEST(Formatter, BinaryOperatorsAndDelimiters) {
    std::string unformatted = "let total=x+y*z-w/2\nlet flag=a==b&&c!=d||e>=f\n";
    std::string formatted = Formatter::format_source(unformatted);

    NV_ASSERT_TRUE(formatted.find("let total = x + y * z - w / 2") != std::string::npos);
    NV_ASSERT_TRUE(formatted.find("let flag = a == b && c != d || e >= f") != std::string::npos);
}

NV_TEST(Formatter, FunctionsAndBlocks) {
    std::string code = 
        "fn calculate(a:int,b:int)->int:\n"
        "let sum=a+b\n"
        "return sum\n";

    std::string formatted = Formatter::format_source(code);
    NV_ASSERT_TRUE(formatted.find("fn calculate(a: int, b: int) -> int:") != std::string::npos);
    NV_ASSERT_TRUE(formatted.find("    let sum = a + b") != std::string::npos);
    NV_ASSERT_TRUE(formatted.find("    return sum") != std::string::npos);
}

NV_TEST(Formatter, CommentsAndBlankLines) {
    std::string code = 
        "// Global header\n"
        "\n\n\n"
        "let x = 10 // trailing comment\n"
        "\n\n"
        "let y = 20\n";

    std::string formatted = Formatter::format_source(code);
    NV_ASSERT_TRUE(formatted.find("// Global header") != std::string::npos);
    NV_ASSERT_TRUE(formatted.find("let x = 10  // trailing comment") != std::string::npos);
    NV_ASSERT_TRUE(Formatter::is_formatted(formatted));
}

NV_TEST(Formatter, DiffGeneration) {
    std::string orig = "let x=10\n";
    std::string fmt = "let x = 10\n";

    std::string diff = Formatter::format_diff(orig, fmt, "test.nv");
    NV_ASSERT_TRUE(diff.find("--- test.nv (original)") != std::string::npos);
    NV_ASSERT_TRUE(diff.find("+++ test.nv (formatted)") != std::string::npos);
    NV_ASSERT_TRUE(diff.find("- let x=10") != std::string::npos);
    NV_ASSERT_TRUE(diff.find("+ let x = 10") != std::string::npos);
}
