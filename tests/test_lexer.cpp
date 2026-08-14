#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include <iostream>

using namespace nextviper;

static std::vector<Token> lex_string(const std::string& src, DiagnosticEngine& diag, bool emit_newlines = false) {
    Lexer lexer(src, "test.nv", diag, emit_newlines);
    return lexer.tokenize();
}

NV_TEST(Lexer, IdentifiersAndKeywords) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "let mut fn return if else while for in loop break continue true false null nil match struct type import and or not my_var _counter name123";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(tokens.size(), 27); // 26 tokens + EOF

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
    NV_ASSERT_EQ(tokens[12].type, TokenType::KEYWORD_TRUE);
    NV_ASSERT_TRUE(tokens[12].bool_value);
    NV_ASSERT_EQ(tokens[13].type, TokenType::KEYWORD_FALSE);
    NV_ASSERT_FALSE(tokens[13].bool_value);
    NV_ASSERT_EQ(tokens[14].type, TokenType::KEYWORD_NULL);
    NV_ASSERT_EQ(tokens[15].type, TokenType::KEYWORD_NIL);
    NV_ASSERT_EQ(tokens[16].type, TokenType::KEYWORD_MATCH);
    NV_ASSERT_EQ(tokens[17].type, TokenType::KEYWORD_STRUCT);
    NV_ASSERT_EQ(tokens[18].type, TokenType::KEYWORD_TYPE);
    NV_ASSERT_EQ(tokens[19].type, TokenType::KEYWORD_IMPORT);
    NV_ASSERT_EQ(tokens[20].type, TokenType::KEYWORD_AND);
    NV_ASSERT_EQ(tokens[21].type, TokenType::KEYWORD_OR);
    NV_ASSERT_EQ(tokens[22].type, TokenType::KEYWORD_NOT);

    NV_ASSERT_EQ(tokens[23].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[23].text, "my_var");

    NV_ASSERT_EQ(tokens[24].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[24].text, "_counter");

    NV_ASSERT_EQ(tokens[25].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[25].text, "name123");

    NV_ASSERT_EQ(tokens[26].type, TokenType::EOF_TOKEN);
}

NV_TEST(Lexer, IntegersAllBasesAndUnderscores) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "0 42 1_000_000 0xFF 0x1A2B 0XDEAD_BEEF 0b1010 0B1111_0000 0o755 0O644";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(tokens.size(), 11); // 10 literals + EOF

    NV_ASSERT_EQ(tokens[0].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[0].int_value, 0);

    NV_ASSERT_EQ(tokens[1].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[1].int_value, 42);

    NV_ASSERT_EQ(tokens[2].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[2].int_value, 1000000);

    // Hex
    NV_ASSERT_EQ(tokens[3].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[3].int_value, 255);

    NV_ASSERT_EQ(tokens[4].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[4].int_value, 0x1A2B);

    NV_ASSERT_EQ(tokens[5].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[5].int_value, (int64_t)0xDEADBEEF);

    // Binary
    NV_ASSERT_EQ(tokens[6].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[6].int_value, 10);

    NV_ASSERT_EQ(tokens[7].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[7].int_value, 240);

    // Octal
    NV_ASSERT_EQ(tokens[8].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[8].int_value, 493); // 0755 octal = 493

    NV_ASSERT_EQ(tokens[9].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[9].int_value, 420); // 0644 octal = 420
}

NV_TEST(Lexer, FloatingPointNumbers) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "3.14159 0.0 123.456 1e5 2.5e-3 1E+6";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(tokens.size(), 7); // 6 floats + EOF

    NV_ASSERT_EQ(tokens[0].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT(std::abs(tokens[0].float_value - 3.14159) < 1e-6);

    NV_ASSERT_EQ(tokens[1].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT_EQ(tokens[1].float_value, 0.0);

    NV_ASSERT_EQ(tokens[3].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT_EQ(tokens[3].float_value, 100000.0);

    NV_ASSERT_EQ(tokens[4].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT(std::abs(tokens[4].float_value - 0.0025) < 1e-7);

    NV_ASSERT_EQ(tokens[5].type, TokenType::FLOAT_LITERAL);
    NV_ASSERT_EQ(tokens[5].float_value, 1000000.0);
}

NV_TEST(Lexer, StringsAndEscapes) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "\"Junaid\" 'NextViper' \"Hello\\nWorld\\t!\" \"\\x41\\x42\" \"\\u{1F600}\" r\"C:\\path\\test\" \"\"\"multi\nline\nstring\"\"\"";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(tokens.size(), 8); // 7 strings + EOF

    NV_ASSERT_EQ(tokens[0].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[0].string_value, "Junaid");

    NV_ASSERT_EQ(tokens[1].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[1].string_value, "NextViper");

    NV_ASSERT_EQ(tokens[2].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[2].string_value, "Hello\nWorld\t!");

    // Hex escape \x41\x42 -> "AB"
    NV_ASSERT_EQ(tokens[3].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[3].string_value, "AB");

    // Unicode escape \u{1F600}
    NV_ASSERT_EQ(tokens[4].type, TokenType::STRING_LITERAL);
    NV_ASSERT_FALSE(tokens[4].string_value.empty());

    // Raw string
    NV_ASSERT_EQ(tokens[5].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[5].string_value, "C:\\path\\test");

    // Multiline string
    NV_ASSERT_EQ(tokens[6].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[6].string_value, "multi\nline\nstring");
}

NV_TEST(Lexer, OperatorsAndDelimiters) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "+ - * / % ** ^ = += -= *= /= %= == != < <= > >= && || ! & | ~ |> => -> .. ..= ( ) { } [ ] , : :: ; . ? @";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());

    std::vector<TokenType> expected = {
        TokenType::PLUS, TokenType::MINUS, TokenType::STAR, TokenType::SLASH, TokenType::PERCENT, TokenType::POWER, TokenType::CARET,
        TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN, TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN, TokenType::PERCENT_ASSIGN,
        TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL,
        TokenType::AMP_AMP, TokenType::PIPE_PIPE, TokenType::BANG, TokenType::AMP, TokenType::PIPE, TokenType::TILDE,
        TokenType::PIPE_GREATER, TokenType::FAT_ARROW, TokenType::ARROW, TokenType::DOT_DOT, TokenType::DOT_DOT_EQUAL,
        TokenType::LPAREN, TokenType::RPAREN, TokenType::LBRACE, TokenType::RBRACE, TokenType::LBRACKET, TokenType::RBRACKET,
        TokenType::COMMA, TokenType::COLON, TokenType::COLON_COLON, TokenType::SEMICOLON, TokenType::DOT, TokenType::QUESTION, TokenType::AT,
        TokenType::EOF_TOKEN
    };

    NV_ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        NV_ASSERT_EQ(tokens[i].type, expected[i]);
    }
}

NV_TEST(Lexer, CommentsAndNesting) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "// Single line comment\n"
                      "let x = 10; // inline comment\n"
                      "/* Block comment */\n"
                      "let y = /* middle */ 20;\n"
                      "/* Outer /* Nested inner */ Outer end */\n"
                      "let z = 30;\n"
                      "/// Doc comment\n";
    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    // Should have: let, x, =, 10, ;, let, y, =, 20, ;, let, z, =, 30, ;, EOF
    NV_ASSERT_EQ(tokens.size(), 16);
    NV_ASSERT_EQ(tokens[3].int_value, 10);
    NV_ASSERT_EQ(tokens[8].int_value, 20);
    NV_ASSERT_EQ(tokens[13].int_value, 30);
}

NV_TEST(Lexer, UserPromptExampleTokenization) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);

    std::string src = "let name = \"Junaid\"\n"
                      "let age = 15\n"
                      "\n"
                      "print(name)\n"
                      "print(age + 1)\n";

    auto tokens = lex_string(src, diag);

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(tokens.size(), 19); // 18 tokens + EOF

    // Line 1: let name = "Junaid"
    NV_ASSERT_EQ(tokens[0].type, TokenType::KEYWORD_LET);
    NV_ASSERT_EQ(tokens[0].line(), 1);
    NV_ASSERT_EQ(tokens[0].column(), 1);

    NV_ASSERT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[1].text, "name");
    NV_ASSERT_EQ(tokens[1].line(), 1);
    NV_ASSERT_EQ(tokens[1].column(), 5);

    NV_ASSERT_EQ(tokens[2].type, TokenType::ASSIGN);
    NV_ASSERT_EQ(tokens[2].line(), 1);
    NV_ASSERT_EQ(tokens[2].column(), 10);

    NV_ASSERT_EQ(tokens[3].type, TokenType::STRING_LITERAL);
    NV_ASSERT_EQ(tokens[3].string_value, "Junaid");
    NV_ASSERT_EQ(tokens[3].line(), 1);
    NV_ASSERT_EQ(tokens[3].column(), 12);

    // Line 2: let age = 15
    NV_ASSERT_EQ(tokens[4].type, TokenType::KEYWORD_LET);
    NV_ASSERT_EQ(tokens[4].line(), 2);
    NV_ASSERT_EQ(tokens[4].column(), 1);

    NV_ASSERT_EQ(tokens[5].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[5].text, "age");
    NV_ASSERT_EQ(tokens[5].line(), 2);
    NV_ASSERT_EQ(tokens[5].column(), 5);

    NV_ASSERT_EQ(tokens[6].type, TokenType::ASSIGN);
    NV_ASSERT_EQ(tokens[6].line(), 2);
    NV_ASSERT_EQ(tokens[6].column(), 9);

    NV_ASSERT_EQ(tokens[7].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[7].int_value, 15);
    NV_ASSERT_EQ(tokens[7].line(), 2);
    NV_ASSERT_EQ(tokens[7].column(), 11);

    // Line 4: print(name)
    NV_ASSERT_EQ(tokens[8].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[8].text, "print");
    NV_ASSERT_EQ(tokens[8].line(), 4);

    NV_ASSERT_EQ(tokens[9].type, TokenType::LPAREN);
    NV_ASSERT_EQ(tokens[10].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[10].text, "name");
    NV_ASSERT_EQ(tokens[11].type, TokenType::RPAREN);

    // Line 5: print(age + 1)
    NV_ASSERT_EQ(tokens[12].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[12].text, "print");
    NV_ASSERT_EQ(tokens[13].type, TokenType::LPAREN);
    NV_ASSERT_EQ(tokens[14].type, TokenType::IDENTIFIER);
    NV_ASSERT_EQ(tokens[14].text, "age");
    NV_ASSERT_EQ(tokens[15].type, TokenType::PLUS);
    NV_ASSERT_EQ(tokens[16].type, TokenType::INT_LITERAL);
    NV_ASSERT_EQ(tokens[16].int_value, 1);
    NV_ASSERT_EQ(tokens[17].type, TokenType::RPAREN);
    NV_ASSERT_EQ(tokens[18].type, TokenType::EOF_TOKEN);
}

NV_TEST(Lexer, MalformedStringsAndInvalidChars) {
    // 1. Unterminated string at EOF
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let s = \"unterminated string", diag);
        NV_ASSERT_TRUE(diag.has_errors());
        NV_ASSERT_EQ(tokens.back().type, TokenType::EOF_TOKEN);
    }

    // 2. Unescaped newline in string literal
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let s = \"hello\nworld\"", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 3. Invalid hex escape
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let s = \"invalid \\xZZ hex\"", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 4. Invalid binary literal digit
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let b = 0b1021", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 5. Invalid octal literal digit
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let o = 0o789", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 6. Invalid hex literal digit
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let h = 0x12G4", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 7. Unterminated block comment
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("/* unclosed comment", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }

    // 8. Unexpected character
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        auto tokens = lex_string("let $bad = 42", diag);
        NV_ASSERT_TRUE(diag.has_errors());
    }
}
