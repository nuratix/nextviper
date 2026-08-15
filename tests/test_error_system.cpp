#include "test_runner.hpp"
#include "nextviper/error_registry.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/type_checker.hpp"
#include "nextviper/interpreter.hpp"
#include <sstream>

using namespace nextviper;

NV_TEST(ErrorSystem, RegistryIntegrity) {
    const auto& reg = ErrorRegistry::instance();
    const auto& defs = reg.all_definitions();

    NV_ASSERT_TRUE(defs.size() >= 16);

    for (const auto& def : defs) {
        NV_ASSERT_FALSE(def.code.empty());
        NV_ASSERT_FALSE(def.slug.empty());
        NV_ASSERT_FALSE(def.title.empty());
        NV_ASSERT_FALSE(def.category.empty());

        // Verify URL format
        std::string url = reg.get_doc_url(def.code);
        NV_ASSERT_EQ(url, "https://nextviper.nuratix.com/docs/errors/" + def.slug);
    }
}

NV_TEST(ErrorSystem, UnknownIdentifierDiagnostic) {
    SourceManager sm;
    sm.add_file("test.nv", "let x = foo + 10\n");
    DiagnosticEngine diag(sm, false);

    Lexer lexer("let x = foo + 10\n", "test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto prog = parser.parse_program();
    NV_ASSERT(prog != nullptr);

    TypeChecker checker(diag);
    checker.check(*prog);

    NV_ASSERT_TRUE(diag.has_errors());
    const auto& d = diag.diagnostics()[0];
    NV_ASSERT_EQ(d.code, "NV1001");
    NV_ASSERT_TRUE(d.message.find("unknown identifier `foo`") != std::string::npos);
    NV_ASSERT_EQ(d.span.start.line, 1);
    NV_ASSERT_EQ(d.span.start.column, 9);

    std::string rendered = diag.render_to_string();
    NV_ASSERT_TRUE(rendered.find("error[NV1001]:") != std::string::npos);
    NV_ASSERT_TRUE(rendered.find("https://nextviper.nuratix.com/docs/errors/unknown-identifier") != std::string::npos);
}

NV_TEST(ErrorSystem, TypeMismatchDiagnostic) {
    SourceManager sm;
    sm.add_file("types.nv", "let a: int = \"hello\"\n");
    DiagnosticEngine diag(sm, false);

    Lexer lexer("let a: int = \"hello\"\n", "types.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto prog = parser.parse_program();
    NV_ASSERT(prog != nullptr);

    TypeChecker checker(diag);
    checker.check(*prog);

    NV_ASSERT_TRUE(diag.has_errors());
    const auto& d = diag.diagnostics()[0];
    NV_ASSERT_EQ(d.code, "NV1003");
    NV_ASSERT_TRUE(d.message.find("TypeError: type mismatch") != std::string::npos);

    std::string json = diag.to_json();
    NV_ASSERT_TRUE(json.find("\"code\": \"NV1003\"") != std::string::npos);
    NV_ASSERT_TRUE(json.find("\"documentation\": \"https://nextviper.nuratix.com/docs/errors/type-mismatch\"") != std::string::npos);
}

NV_TEST(ErrorSystem, SyntaxErrorDiagnostic) {
    SourceManager sm;
    sm.add_file("syntax.nv", "if x > 10\n    print(x)\n");
    DiagnosticEngine diag(sm, false);

    Lexer lexer("if x > 10\n    print(x)\n", "syntax.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto prog = parser.parse_program();

    NV_ASSERT_TRUE(diag.has_errors());
    std::string rendered = diag.render_to_string();
    NV_ASSERT_TRUE(rendered.find("https://nextviper.nuratix.com/docs/errors/syntax-error") != std::string::npos);
}

NV_TEST(ErrorSystem, InvalidFunctionCallDiagnostic) {
    SourceManager sm;
    sm.add_file("call.nv", "fn add(a: int, b: int) -> int:\n    return a + b\nlet res = add(1, 2, 3)\n");
    DiagnosticEngine diag(sm, false);

    Lexer lexer("fn add(a: int, b: int) -> int:\n    return a + b\nlet res = add(1, 2, 3)\n", "call.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto prog = parser.parse_program();
    NV_ASSERT(prog != nullptr);

    TypeChecker checker(diag);
    checker.check(*prog);

    NV_ASSERT_TRUE(diag.has_errors());
    const auto& d = diag.diagnostics()[0];
    NV_ASSERT_EQ(d.code, "NV1004");
    NV_ASSERT_TRUE(d.message.find("function expects 2 argument(s), got 3") != std::string::npos);
}
