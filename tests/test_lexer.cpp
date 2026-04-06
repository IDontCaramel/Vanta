#include <gtest/gtest.h>

#include "lexer.h"

TEST(LexerTest, TokenizesLiteralsAndKeywords) {
    Lexer lexer("var answer = 42; func greet() { return \"hi\"; }");
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 13U);
    EXPECT_EQ(tokens[0].type, TokenType::VAR);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[5].type, TokenType::FUNC);
    EXPECT_EQ(tokens[10].type, TokenType::RETURN);
    EXPECT_EQ(tokens[11].type, TokenType::STRING);
}

TEST(LexerTest, SkipsCommentsAndTracksInterpolation) {
    Lexer lexer("42 # ignore me\n$\"hello {name}\"");
    auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 3U);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[1].type, TokenType::INTERP_STRING);
    EXPECT_EQ(tokens[1].lexeme, "hello {name}");
}

TEST(LexerTest, RecognizesArrowsAndAssignments) {
    Lexer lexer("x => x + 1; func add(a: Number) -> Number { return a; }");
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 18U);
    EXPECT_EQ(tokens[1].type, TokenType::ARROW);
    EXPECT_EQ(tokens[10].type, TokenType::COLON);
    EXPECT_EQ(tokens[13].type, TokenType::TYPE_ARROW);
}
