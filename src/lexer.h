#pragma once

#include <string>
#include <vector>

#include "token.h"

class Lexer {
public:
    explicit Lexer(std::string sourceCode);

    std::vector<Token> tokenize();

private:
    std::string source;
    std::size_t pos = 0;
    int line = 1;
    int column = 1;

    char peek(int offset = 0) const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;
    void skipWhitespaceAndComments();
    Token makeToken(TokenType type, const std::string& lexeme, int tokenLine, int tokenColumn) const;
    Token scanNumber();
    Token scanString(char quote, bool interpolation);
    Token scanIdentifierOrKeyword();
};
