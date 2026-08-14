#include "test_runner.hpp"
#include "nextviper/lexer.hpp"

using namespace nextviper;

NV_TEST(Lexer, ScanLiterals) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer("42 3.14 0xFF 0b1010 \"hello world\" true false nil", "test.nv", diag);
    auto tokens = lexer.tokenize();

    NV_ASSERT_EQ(tokens.size(), 9); // 8 tokens + EOF
    NV_ASSERT_EQ(tokens[0].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[0].int_value, 42);

    NV_ASSERT_EQ(tokens[1].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT(tokens[1].float_value > 3.13 && tokens[1].float_value < 3.15);

    NV_ASSERT_EQ(tokens[2].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[2].int_value, 255);

    NV_ASSERT_EQ(tokens[3].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[3].int_value, 10);

    NV_ASSERT_EQ(tokens[4].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[4].string_value, "hello world");

    NV_ASSERT_EQ(tokens[5].type, TokenType::KEYWORD_TRUE);
    NV_ASSERT_EQ(tokens[6].type, TokenType::KEYWORD_FALSE);
    NV_ASSERT_EQ(tokens[7].type, TokenType::KEYWORD_NIL);
    NV_ASSERT_EQ(tokens[8].type, TokenType::EOF_TOKEN);
}

NV_TEST(Lexer, ScanKeywords) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer("let mut fn return if else while for in loop break continue match struct type import and or not", "test.nv", diag);
    auto tokens = lexer.tokenize();

    NV_ASSERT_EQ(tokens[0].type, TokenType::KEYWORD_LET);
    NV_ASSERT_EQ(tokens[1].type, TokenType::KEYWORD_MUT);
    NV_ASSERT_EQ(tokens[2].type, TokenType::KEYWORD_FN);
    NV_ASSERT_EQ(tokens[3].type, TokenType::KEYWORD_RETURN);
    NV_ASSERT_EQ(tokens[4].type, TokenType::KEYWORD_IF);
    NV_ASSERT_EQ(tokens[5].type, TokenType::KEYWORD_ELSE);
    NV_ASSERT_EQ(tokens[6].type, TokenType::KEYWORD_WHILE);
    NV_ASSERT_EQ(tokens[7].type, TokenType::KEYWORD_FOR);
    NV_ASSERT_EQ(tokens[8].type, TokenType::KEYWORD_IN);
    NV_ASSERT_EQ(tokens[9].type, TokenType::KEYWORD_LOOP);
    NV_ASSERT_EQ(tokens[10].type, TokenType::KEYWORD_BREAK);
    NV_ASSERT_EQ(tokens[11].type, TokenType::KEYWORD_CONTINUE);
    NV_ASSERT_EQ(tokens[12].type, TokenType::KEYWORD_MATCH);
    NV_ASSERT_EQ(tokens[13].type, TokenType::KEYWORD_STRUCT);
    NV_ASSERT_EQ(tokens[14].type, TokenType::KEYWORD_TYPE);
    NV_ASSERT_EQ(tokens[15].type, TokenType::KEYWORD_IMPORT);
    NV_ASSERT_EQ(tokens[16].type, TokenType::KEYWORD_AND);
    NV_ASSERT_EQ(tokens[17].type, TokenType::KEYWORD_OR);
    NV_ASSERT_EQ(tokens[18].type, TokenType::KEYWORD_NOT);
}

NV_TEST(Lexer, ScanOperatorsAndPunctuation) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer("+ - * / % ** = += -= *= /= %= == != < <= > >= && || ! |> => -> .. ..= ( ) { } [ ] , : ; . ?", "test.nv", diag);
    auto tokens = lexer.tokenize();

    size_t i = 0;
    NV_ASSERT_EQ(tokens[i++].type, TokenType::PLUS);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::MINUS);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::STAR);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::SLASH);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::PERCENT);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::POWER);

    NV_ASSERT_EQ(tokens[i++].type, TokenType::ASSIGN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::PLUS_ASSIGN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::MINUS_ASSIGN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::STAR_ASSIGN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::SLASH_ASSIGN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::PERCENT_ASSIGN);

    NV_ASSERT_EQ(tokens[i++].type, TokenType::EQUAL_EQUAL);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::BANG_EQUAL);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::LESS);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::LESS_EQUAL);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::GREATER);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::GREATER_EQUAL);

    NV_ASSERT_EQ(tokens[i++].type, TokenType::AMP_AMP);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::PIPE_PIPE);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::BANG);

    NV_ASSERT_EQ(tokens[i++].type, TokenType::PIPE_GREATER); // |>
    NV_ASSERT_EQ(tokens[i++].type, TokenType::FAT_ARROW);    // =>
    NV_ASSERT_EQ(tokens[i++].type, TokenType::ARROW);        // ->
    NV_ASSERT_EQ(tokens[i++].type, TokenType::DOT_DOT);      // ..
    NV_ASSERT_EQ(tokens[i++].type, TokenType::DOT_DOT_EQUAL);// ..=

    NV_ASSERT_EQ(tokens[i++].type, TokenType::LPAREN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::RPAREN);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::LBRACE);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::RBRACE);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::LBRACKET);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::RBRACKET);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::COMMA);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::COLON);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::SEMICOLON);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::DOT);
    NV_ASSERT_EQ(tokens[i++].type, TokenType::QUESTION);
}

NV_TEST(Lexer, SkipComments) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "// single line comment\n"
                      "let x = 10; /* multi\nline\ncomment */ let y = 20;";
    Lexer lexer(src, "comments.nv", diag);
    auto tokens = lexer.tokenize();

    NV_ASSERT_EQ(tokens[0].type, TokenType::KEYWORD_LET);
    NV_ASSERT_EQ(tokens[1].text, "x");
    NV_ASSERT_EQ(tokens[2].type, TokenType::ASSIGN);
    NV_ASSERT_EQ(tokens[3].int_value, 10);
    NV_ASSERT_EQ(tokens[4].type, TokenType::SEMICOLON);
    NV_ASSERT_EQ(tokens[5].type, TokenType::KEYWORD_LET);
    NV_ASSERT_EQ(tokens[6].text, "y");
    NV_ASSERT_EQ(tokens[7].type, TokenType::ASSIGN);
    NV_ASSERT_EQ(tokens[8].int_value, 20);
}

NV_TEST(Lexer, LocationTracking) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    std::string src = "let a = 1\nlet b = 2";
    Lexer lexer(src, "test.nv", diag);
    auto tokens = lexer.tokenize();

    NV_ASSERT_EQ(tokens[0].span.start.line, 1);
    NV_ASSERT_EQ(tokens[0].span.start.column, 1);

    NV_ASSERT_EQ(tokens[4].span.start.line, 2);
    NV_ASSERT_EQ(tokens[4].span.start.column, 1);
}
