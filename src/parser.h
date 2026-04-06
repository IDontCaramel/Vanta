#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast.h"
#include "source_file.h"
#include "token.h"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::shared_ptr<const SourceDocument> source = nullptr);

    std::shared_ptr<ProgramStmt> parse();
    ExprPtr parseExpressionOnly();

private:
    std::vector<Token> tokens;
    std::shared_ptr<const SourceDocument> source;
    std::size_t current = 0;

    StmtPtr declaration();
    StmtPtr varDeclaration(bool expectSemicolon = true);
    std::shared_ptr<FunctionDeclStmt> functionDeclaration(const std::string& kind, bool allowAnonymousName = false);
    StmtPtr classDeclaration();
    StmtPtr statement();
    std::shared_ptr<BlockStmt> blockStatement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    StmtPtr returnStatement();
    StmtPtr throwStatement();
    StmtPtr tryStatement();
    StmtPtr expressionStatement();

    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr pipe();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr primary();

    ExprPtr finishCall(ExprPtr callee);
    ExprPtr finishIndex(ExprPtr object);
    ExprPtr parseFunctionExpression();
    ExprPtr parseArrowFunctionWithIdentifier();
    ExprPtr parseArrowFunctionWithParens();
    std::vector<Parameter> parseParameterList(TokenType closingToken);
    std::string parseOptionalTypeHint();
    std::vector<std::variant<std::string, ExprPtr>> parseInterpolationParts(const Token& token);

    bool isArrowFunctionAhead() const;
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchAny(const std::vector<TokenType>& types);
    const Token& advance();
    const Token& peek() const;
    const Token& previous() const;
    const Token& consume(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& token, const std::string& message) const;
};
