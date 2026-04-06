#pragma once

#include <string>

enum class TokenType {
    NUMBER,
    STRING,
    INTERP_STRING,
    IDENTIFIER,

    VAR,
    FUNC,
    CLASS,
    RETURN,
    IF,
    ELSE,
    WHILE,
    FOR,
    BREAK,
    CONTINUE,
    NEW,
    THIS,
    TRY,
    CATCH,
    FINALLY,
    THROW,
    EXTENDS,
    TRUE_,
    FALSE_,
    NULL_,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,
    AND,
    OR,
    NOT,
    ASSIGN,
    PLUS_ASSIGN,
    MINUS_ASSIGN,

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    DOT,
    COMMA,
    SEMICOLON,
    COLON,
    ARROW,
    TYPE_ARROW,
    PIPE,

    EOF_,
    ERROR_
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    Token(TokenType tokenType, std::string tokenLexeme, int tokenLine, int tokenColumn)
        : type(tokenType), lexeme(std::move(tokenLexeme)), line(tokenLine), column(tokenColumn) {}
};
