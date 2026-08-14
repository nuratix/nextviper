#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"

using namespace nextviper;

static std::unique_ptr<Program> parse_src(const std::string& src, DiagnosticEngine& diag) {
    Lexer lexer(src, "parser_test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    return parser.parse_program();
}

NV_TEST(Parser, ParseLetStatements) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let name = \"Junaid\"\n"
                      "let age = 15\n"
                      "let mut count = 0\n"
                      "let z: Int = 30\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 4);

    auto* s0 = dynamic_cast<LetStmt*>(program->statements()[0].get());
    NV_ASSERT(s0 != nullptr);
    NV_ASSERT_EQ(s0->name(), "name");
    NV_ASSERT_FALSE(s0->is_mut());

    auto* s1 = dynamic_cast<LetStmt*>(program->statements()[1].get());
    NV_ASSERT(s1 != nullptr);
    NV_ASSERT_EQ(s1->name(), "age");

    auto* s2 = dynamic_cast<LetStmt*>(program->statements()[2].get());
    NV_ASSERT(s2 != nullptr);
    NV_ASSERT_EQ(s2->name(), "count");
    NV_ASSERT_TRUE(s2->is_mut());

    auto* s3 = dynamic_cast<LetStmt*>(program->statements()[3].get());
    NV_ASSERT(s3 != nullptr);
    NV_ASSERT_EQ(s3->name(), "z");
    NV_ASSERT_EQ(s3->type_annotation(), "Int");
}

NV_TEST(Parser, ParseBareAssignments) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "x = 10\n"
                      "y = 20\n"
                      "result = x + y\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 3);

    auto* s0 = dynamic_cast<ExprStmt*>(program->statements()[0].get());
    NV_ASSERT(s0 != nullptr);
    auto* assign0 = dynamic_cast<const AssignExpr*>(&s0->expr());
    NV_ASSERT(assign0 != nullptr);
    NV_ASSERT_EQ(assign0->name(), "x");

    auto* s2 = dynamic_cast<ExprStmt*>(program->statements()[2].get());
    NV_ASSERT(s2 != nullptr);
    auto* assign2 = dynamic_cast<const AssignExpr*>(&s2->expr());
    NV_ASSERT(assign2 != nullptr);
    NV_ASSERT_EQ(assign2->name(), "result");

    auto* binary = dynamic_cast<const BinaryExpr*>(&assign2->value());
    NV_ASSERT(binary != nullptr);
    NV_ASSERT_EQ(binary->op(), TokenType::PLUS);
}

NV_TEST(Parser, ParseArithmeticAndPrecedence) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let a = 1 + 2 * 3\n"
                      "let b = (1 + 2) * 3\n"
                      "let c = 2 ** 3 ** 2\n"
                      "let d = -x + 5\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 4);

    // 1 + (2 * 3)
    auto* s0 = dynamic_cast<LetStmt*>(program->statements()[0].get());
    auto* b0 = dynamic_cast<const BinaryExpr*>(s0->initializer());
    NV_ASSERT(b0 != nullptr);
    NV_ASSERT_EQ(b0->op(), TokenType::PLUS);
    auto* rhs0 = dynamic_cast<const BinaryExpr*>(&b0->right());
    NV_ASSERT(rhs0 != nullptr);
    NV_ASSERT_EQ(rhs0->op(), TokenType::STAR);

    // (1 + 2) * 3
    auto* s1 = dynamic_cast<LetStmt*>(program->statements()[1].get());
    auto* b1 = dynamic_cast<const BinaryExpr*>(s1->initializer());
    NV_ASSERT(b1 != nullptr);
    NV_ASSERT_EQ(b1->op(), TokenType::STAR);
}

NV_TEST(Parser, ParseComparisonAndBoolean) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let c1 = age >= 18\n"
                      "let c2 = x > 5 and y < 10\n"
                      "let c3 = a == b or c != d\n"
                      "let c4 = not is_ready\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 4);

    auto* s0 = dynamic_cast<LetStmt*>(program->statements()[0].get());
    auto* b0 = dynamic_cast<const BinaryExpr*>(s0->initializer());
    NV_ASSERT(b0 != nullptr);
    NV_ASSERT_EQ(b0->op(), TokenType::GREATER_EQUAL);

    auto* s1 = dynamic_cast<LetStmt*>(program->statements()[1].get());
    auto* b1 = dynamic_cast<const BinaryExpr*>(s1->initializer());
    NV_ASSERT(b1 != nullptr);
    NV_ASSERT_EQ(b1->op(), TokenType::KEYWORD_AND);

    auto* s3 = dynamic_cast<LetStmt*>(program->statements()[3].get());
    auto* u3 = dynamic_cast<const UnaryExpr*>(s3->initializer());
    NV_ASSERT(u3 != nullptr);
    NV_ASSERT_EQ(u3->op(), TokenType::KEYWORD_NOT);
}

NV_TEST(Parser, ParseFunctionCalls) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "print(\"Adult\")\n"
                      "print(age + 1)\n"
                      "let val = calculate(x, y, 42)\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 3);

    auto* s0 = dynamic_cast<ExprStmt*>(program->statements()[0].get());
    auto* call0 = dynamic_cast<const CallExpr*>(&s0->expr());
    NV_ASSERT(call0 != nullptr);
    NV_ASSERT_EQ(call0->args().size(), 1);

    auto* s1 = dynamic_cast<ExprStmt*>(program->statements()[1].get());
    auto* call1 = dynamic_cast<const CallExpr*>(&s1->expr());
    NV_ASSERT(call1 != nullptr);
    auto* bin_arg = dynamic_cast<const BinaryExpr*>(call1->args()[0].get());
    NV_ASSERT(bin_arg != nullptr);
    NV_ASSERT_EQ(bin_arg->op(), TokenType::PLUS);
}

NV_TEST(Parser, ParseIfElseBothStyles) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "// 1. Brace style\n"
                      "if age >= 18 {\n"
                      "    print(\"Adult\")\n"
                      "} else {\n"
                      "    print(\"Minor\")\n"
                      "}\n"
                      "\n"
                      "// 2. Colon style\n"
                      "if age >= 18:\n"
                      "    print(\"Adult\")\n"
                      "else:\n"
                      "    print(\"Minor\")\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 2);

    auto* if0 = dynamic_cast<IfStmt*>(program->statements()[0].get());
    NV_ASSERT(if0 != nullptr);
    NV_ASSERT(if0->else_branch() != nullptr);

    auto* if1 = dynamic_cast<IfStmt*>(program->statements()[1].get());
    NV_ASSERT(if1 != nullptr);
    NV_ASSERT(if1->else_branch() != nullptr);
}

NV_TEST(Parser, ParseFullUserProgram) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let name = \"Junaid\"\n"
                      "let age = 15\n"
                      "\n"
                      "x = 10\n"
                      "y = 20\n"
                      "\n"
                      "result = x + y\n"
                      "\n"
                      "if age >= 18:\n"
                      "    print(\"Adult\")\n"
                      "else:\n"
                      "    print(\"Minor\")\n";
    auto program = parse_src(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT(program != nullptr);
    NV_ASSERT_EQ(program->statements().size(), 6);

    // Verify AST structure
    NV_ASSERT_EQ(dynamic_cast<LetStmt*>(program->statements()[0].get())->name(), "name");
    NV_ASSERT_EQ(dynamic_cast<LetStmt*>(program->statements()[1].get())->name(), "age");
    NV_ASSERT(dynamic_cast<ExprStmt*>(program->statements()[2].get()) != nullptr);
    NV_ASSERT(dynamic_cast<ExprStmt*>(program->statements()[3].get()) != nullptr);
    NV_ASSERT(dynamic_cast<ExprStmt*>(program->statements()[4].get()) != nullptr);
    NV_ASSERT(dynamic_cast<IfStmt*>(program->statements()[5].get()) != nullptr);
}

NV_TEST(Parser, InvalidSyntaxDiagnostics) {
    // 1. Missing variable name in let
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto prog = parse_src("let = 10", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 2. Unbalanced parenthesis
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto prog = parse_src("let x = (10 + 20", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 3. Missing expression after operator
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto prog = parse_src("let x = 10 +", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }
}
