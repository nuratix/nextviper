#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"

using namespace nextviper;

NV_TEST(Parser, ParseLetStatements) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let x = 10; let mut y = 20; let z: Int = 30;";
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();

    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 3);

    auto* s0 = dynamic_cast<LetStmt*>(program->statements()[0].get());
    NV_ASSERT(s0 != nullptr);
    NV_ASSERT_EQ(s0->name(), "x");
    NV_ASSERT_FALSE(s0->is_mut());

    auto* s1 = dynamic_cast<LetStmt*>(program->statements()[1].get());
    NV_ASSERT(s1 != nullptr);
    NV_ASSERT_EQ(s1->name(), "y");
    NV_ASSERT_TRUE(s1->is_mut());

    auto* s2 = dynamic_cast<LetStmt*>(program->statements()[2].get());
    NV_ASSERT(s2 != nullptr);
    NV_ASSERT_EQ(s2->name(), "z");
    NV_ASSERT_EQ(s2->type_annotation(), "Int");
}

NV_TEST(Parser, ParseFunctionDeclarations) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
                      "fn square(x) => x * x;";
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();

    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(program->statements().size(), 2);

    auto* fn0 = dynamic_cast<FnDeclStmt*>(program->statements()[0].get());
    NV_ASSERT(fn0 != nullptr);
    NV_ASSERT_EQ(fn0->name(), "add");
    NV_ASSERT_EQ(fn0->params().size(), 2);
    NV_ASSERT_EQ(fn0->return_type(), "Int");
    NV_ASSERT_FALSE(fn0->is_arrow_body());

    auto* fn1 = dynamic_cast<FnDeclStmt*>(program->statements()[1].get());
    NV_ASSERT(fn1 != nullptr);
    NV_ASSERT_EQ(fn1->name(), "square");
    NV_ASSERT_EQ(fn1->params().size(), 1);
    NV_ASSERT_TRUE(fn1->is_arrow_body());
}

NV_TEST(Parser, ParseControlFlow) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "if x > 0 { print(x); } else { print(-x); }\n"
                      "while i < 10 { i += 1; }\n"
                      "for item in list { print(item); }";
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();

    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(program->statements().size(), 3);

    auto* if_stmt = dynamic_cast<IfStmt*>(program->statements()[0].get());
    NV_ASSERT(if_stmt != nullptr);
    NV_ASSERT(if_stmt->else_branch() != nullptr);

    auto* while_stmt = dynamic_cast<WhileStmt*>(program->statements()[1].get());
    NV_ASSERT(while_stmt != nullptr);

    auto* for_stmt = dynamic_cast<ForInStmt*>(program->statements()[2].get());
    NV_ASSERT(for_stmt != nullptr);
    NV_ASSERT_EQ(for_stmt->variable_name(), "item");
}

NV_TEST(Parser, ParsePipelineOperator) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let res = data |> filter(is_valid) |> transform();";
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();

    Parser parser(tokens, diag);
    auto program = parser.parse_program();

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(program->statements().size(), 1);

    auto* let_stmt = dynamic_cast<LetStmt*>(program->statements()[0].get());
    NV_ASSERT(let_stmt != nullptr);
    auto* pipe = dynamic_cast<const PipeExpr*>(let_stmt->initializer());
    NV_ASSERT(pipe != nullptr);
}
