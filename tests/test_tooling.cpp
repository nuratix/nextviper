#include "test_runner.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include <sstream>

using namespace nextviper;

NV_TEST(Tooling, PromptExactErrorFormat) {
    SourceManager sm;
    std::string filename = "main.nv";
    std::string source = 
        "let x = 10\n"
        "let y = 20\n"
        "print(total)\n";

    sm.add_file(filename, source);
    DiagnosticEngine diag(sm, false); // no ANSI colors for string check

    // Error on line 3, column 7: `total` (span len = 5)
    SourceSpan span(SourceLocation(3, 7), SourceLocation(3, 12), filename);
    diag.error("unknown variable `total`", span, "define `total` before using it", "NV102");

    std::string rendered = diag.render_to_string();

    // Verify format components from user prompt:
    // error[NV102]:
    //     unknown variable `total`
    //
    //     --> main.nv:3:7
    //
    //      3 | print(total)
    //        |       ^^^^^
    //
    //     help: define `total` before using it

    NV_ASSERT_TRUE(rendered.find("error[NV102]:") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("unknown variable `total`") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("--> main.nv:3:7") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("3 | print(total)") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("^^^^^") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("help: define `total` before using it") != std::string::npos);
}

NV_TEST(Tooling, MachineReadableJSONDiagnostics) {
    SourceManager sm;
    std::string filename = "app.nv";
    std::string source = "let a = 10 / 0\n";

    sm.add_file(filename, source);
    DiagnosticEngine diag(sm, false);

    SourceSpan span(SourceLocation(1, 9), SourceLocation(1, 15), filename);
    diag.error("division by zero", span, "ensure denominator is non-zero before division", "NV105");

    std::string json_output = diag.to_json();

    NV_ASSERT_TRUE(json_output.find("\"level\": \"error\"") != std::string::npos);
    NV_ASSERT_TRUE(json_output.find("\"code\": \"NV105\"") != std::string::npos);
    NV_ASSERT_TRUE(json_output.find("\"message\": \"division by zero\"") != std::string::npos);
    NV_ASSERT_TRUE(json_output.find("\"file\": \"app.nv\"") != std::string::npos);
    NV_ASSERT_TRUE(json_output.find("\"line\": 1") != std::string::npos);
}

NV_TEST(Tooling, RuntimeExecutionErrorEmission) {
    SourceManager sm;
    std::string source = "print(total)\n";
    std::string filename = "runtime_test.nv";

    sm.add_file(filename, source);
    DiagnosticEngine diag(sm, false);

    Lexer lexer(source, filename, diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interp(diag);
    bool ok = interp.execute(*program);

    NV_ASSERT_FALSE(ok);
    NV_ASSERT_TRUE(diag.has_errors());
    std::string err_str = diag.render_to_string();
    NV_ASSERT_TRUE(err_str.find("error[NV1001]:") != std::string::npos || err_str.find("error[NV102]:") != std::string::npos);
    NV_ASSERT_TRUE(err_str.find("total`") != std::string::npos);
}
