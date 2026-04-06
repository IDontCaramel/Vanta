#include "parser.h"

#include <sstream>
#include <stdexcept>

#include "diagnostic.h"
#include "lexer.h"

namespace {

SourceRange mergeRanges(const SourceRange& start, const SourceRange& end) {
    if (!start.isValid()) {
        return end;
    }
    if (!end.isValid()) {
        return start;
    }
    return SourceRange::fromBounds(start.start.line, start.start.column, end.end.line, end.end.column);
}

template <typename NodePtr>
NodePtr withRange(NodePtr node, const SourceRange& range) {
    node->sourceRange = range;
    return node;
}

template <typename NodePtr>
NodePtr withRange(NodePtr node, const Token& token) {
    node->sourceRange = token.range();
    return node;
}

std::string describeToken(const Token& token) {
    if (token.type == TokenType::EOF_) {
        return "end of input";
    }
    if (token.lexeme.empty()) {
        return "token";
    }
    return "'" + token.lexeme + "'";
}

}  // namespace

Parser::Parser(std::vector<Token> tokenStream, std::shared_ptr<const SourceDocument> sourceDocument)
    : tokens(std::move(tokenStream)), source(std::move(sourceDocument)) {}

std::shared_ptr<ProgramStmt> Parser::parse() {
    std::vector<StmtPtr> statements;
    while (!isAtEnd()) {
        statements.push_back(declaration());
    }
    auto program = std::make_shared<ProgramStmt>(std::move(statements));
    if (!program->statements.empty()) {
        program->sourceRange = mergeRanges(program->statements.front()->sourceRange,
                                           program->statements.back()->sourceRange);
    } else if (!tokens.empty()) {
        program->sourceRange = tokens.front().range();
    }
    return program;
}

ExprPtr Parser::parseExpressionOnly() {
    ExprPtr expr = expression();
    consume(TokenType::EOF_, "expected end of embedded expression");
    return expr;
}

StmtPtr Parser::declaration() {
    if (match(TokenType::VAR)) {
        return varDeclaration();
    }
    if (match(TokenType::FUNC)) {
        return functionDeclaration("function");
    }
    if (match(TokenType::CLASS)) {
        return classDeclaration();
    }
    return statement();
}

StmtPtr Parser::varDeclaration(bool expectSemicolon) {
    const Token keyword = previous();
    if (match(TokenType::LBRACKET)) {
        std::vector<std::string> names;
        if (!check(TokenType::RBRACKET)) {
            do {
                names.push_back(consume(TokenType::IDENTIFIER, "expected array destructuring name").lexeme);
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACKET, "expected closing ] in destructuring declaration");
        consume(TokenType::ASSIGN, "expected = after destructuring pattern");
        ExprPtr initializer = expression();
        if (expectSemicolon) {
            consume(TokenType::SEMICOLON, "expected ; after variable declaration");
        }
        const SourceRange initializerRange = initializer->sourceRange;
        return withRange(std::make_shared<VarDeclStmt>(std::move(names), true, std::move(initializer)),
                         mergeRanges(keyword.range(), initializerRange));
    }

    if (match(TokenType::LBRACE)) {
        std::vector<std::string> names;
        if (!check(TokenType::RBRACE)) {
            do {
                names.push_back(consume(TokenType::IDENTIFIER, "expected object destructuring name").lexeme);
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACE, "expected closing } in destructuring declaration");
        consume(TokenType::ASSIGN, "expected = after destructuring pattern");
        ExprPtr initializer = expression();
        if (expectSemicolon) {
            consume(TokenType::SEMICOLON, "expected ; after variable declaration");
        }
        const SourceRange initializerRange = initializer->sourceRange;
        return withRange(std::make_shared<VarDeclStmt>(std::move(names), false, std::move(initializer)),
                         mergeRanges(keyword.range(), initializerRange));
    }

    const std::string name = consume(TokenType::IDENTIFIER, "expected variable name").lexeme;
    const std::string typeHint = parseOptionalTypeHint();
    ExprPtr initializer = nullptr;
    if (match(TokenType::ASSIGN)) {
        initializer = expression();
    }
    if (expectSemicolon) {
        consume(TokenType::SEMICOLON, "expected ; after variable declaration");
    }
    const SourceRange endRange = initializer ? initializer->sourceRange : previous().range();
    return withRange(std::make_shared<VarDeclStmt>(name, initializer, typeHint),
                     mergeRanges(keyword.range(), endRange));
}

std::shared_ptr<FunctionDeclStmt> Parser::functionDeclaration(const std::string& kind, bool allowAnonymousName) {
    const Token keyword = previous();
    std::string name;
    if (!allowAnonymousName || check(TokenType::IDENTIFIER)) {
        name = consume(TokenType::IDENTIFIER, "expected " + kind + " name").lexeme;
    }

    consume(TokenType::LPAREN, "expected ( after " + kind + " name");
    std::vector<Parameter> parameters = parseParameterList(TokenType::RPAREN);
    consume(TokenType::RPAREN, "expected ) after parameters");
    const std::string returnType = match(TokenType::TYPE_ARROW)
                                       ? consume(TokenType::IDENTIFIER, "expected return type").lexeme
                                       : std::string();
    std::shared_ptr<BlockStmt> body = blockStatement();
    return withRange(std::make_shared<FunctionDeclStmt>(name, std::move(parameters), std::move(body->statements),
                                                        returnType),
                     mergeRanges(keyword.range(), body->sourceRange));
}

StmtPtr Parser::classDeclaration() {
    const Token keyword = previous();
    const std::string name = consume(TokenType::IDENTIFIER, "expected class name").lexeme;
    std::string parentName;
    if (match(TokenType::EXTENDS)) {
        parentName = consume(TokenType::IDENTIFIER, "expected parent class name").lexeme;
    }

    consume(TokenType::LBRACE, "expected { after class declaration");
    std::vector<std::shared_ptr<FunctionDeclStmt>> methods;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        consume(TokenType::FUNC, "expected func in class body");
        methods.push_back(functionDeclaration("method"));
    }
    const Token closingBrace = consume(TokenType::RBRACE, "expected } after class body");
    return withRange(std::make_shared<ClassDeclStmt>(name, parentName, std::move(methods)),
                     mergeRanges(keyword.range(), closingBrace.range()));
}

StmtPtr Parser::statement() {
    if (match(TokenType::IF)) {
        return ifStatement();
    }
    if (match(TokenType::WHILE)) {
        return whileStatement();
    }
    if (match(TokenType::FOR)) {
        return forStatement();
    }
    if (match(TokenType::RETURN)) {
        return returnStatement();
    }
    if (match(TokenType::BREAK)) {
        const Token keyword = previous();
        const Token semicolon = consume(TokenType::SEMICOLON, "expected ; after break");
        return withRange(std::make_shared<BreakStmt>(), mergeRanges(keyword.range(), semicolon.range()));
    }
    if (match(TokenType::CONTINUE)) {
        const Token keyword = previous();
        const Token semicolon = consume(TokenType::SEMICOLON, "expected ; after continue");
        return withRange(std::make_shared<ContinueStmt>(), mergeRanges(keyword.range(), semicolon.range()));
    }
    if (match(TokenType::TRY)) {
        return tryStatement();
    }
    if (match(TokenType::THROW)) {
        return throwStatement();
    }
    if (check(TokenType::LBRACE)) {
        return blockStatement();
    }
    return expressionStatement();
}

std::shared_ptr<BlockStmt> Parser::blockStatement() {
    const Token openingBrace = consume(TokenType::LBRACE, "expected { to start block");
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(declaration());
    }
    const Token closingBrace = consume(TokenType::RBRACE, "expected } after block");
    return withRange(std::make_shared<BlockStmt>(std::move(statements)),
                     mergeRanges(openingBrace.range(), closingBrace.range()));
}

StmtPtr Parser::ifStatement() {
    const Token keyword = previous();
    consume(TokenType::LPAREN, "expected ( after if");
    ExprPtr condition = expression();
    consume(TokenType::RPAREN, "expected ) after if condition");
    StmtPtr thenBranch = statement();
    StmtPtr elseBranch = nullptr;
    if (match(TokenType::ELSE)) {
        elseBranch = statement();
    }
    const SourceRange endRange = elseBranch ? elseBranch->sourceRange : thenBranch->sourceRange;
    return withRange(std::make_shared<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch)),
                     mergeRanges(keyword.range(), endRange));
}

StmtPtr Parser::whileStatement() {
    const Token keyword = previous();
    consume(TokenType::LPAREN, "expected ( after while");
    ExprPtr condition = expression();
    consume(TokenType::RPAREN, "expected ) after while condition");
    StmtPtr body = statement();
    return withRange(std::make_shared<WhileStmt>(std::move(condition), body),
                     mergeRanges(keyword.range(), body->sourceRange));
}

StmtPtr Parser::forStatement() {
    const Token keyword = previous();
    consume(TokenType::LPAREN, "expected ( after for");
    StmtPtr initializer = nullptr;
    if (match(TokenType::SEMICOLON)) {
        initializer = nullptr;
    } else if (match(TokenType::VAR)) {
        initializer = varDeclaration(false);
        consume(TokenType::SEMICOLON, "expected ; after for initializer");
    } else {
        initializer = std::make_shared<ExprStmt>(expression());
        consume(TokenType::SEMICOLON, "expected ; after for initializer");
    }

    ExprPtr condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "expected ; after for condition");

    ExprPtr increment = nullptr;
    if (!check(TokenType::RPAREN)) {
        increment = expression();
    }
    consume(TokenType::RPAREN, "expected ) after for clauses");
    StmtPtr body = statement();
    return withRange(std::make_shared<ForStmt>(std::move(initializer), std::move(condition), std::move(increment),
                                               body),
                     mergeRanges(keyword.range(), body->sourceRange));
}

StmtPtr Parser::returnStatement() {
    const Token keyword = previous();
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }
    const Token semicolon = consume(TokenType::SEMICOLON, "expected ; after return");
    const SourceRange endRange = value ? value->sourceRange : semicolon.range();
    return withRange(std::make_shared<ReturnStmt>(std::move(value)),
                     mergeRanges(keyword.range(), endRange));
}

StmtPtr Parser::throwStatement() {
    const Token keyword = previous();
    ExprPtr value = expression();
    consume(TokenType::SEMICOLON, "expected ; after throw");
    const SourceRange valueRange = value->sourceRange;
    return withRange(std::make_shared<ThrowStmt>(std::move(value)),
                     mergeRanges(keyword.range(), valueRange));
}

StmtPtr Parser::tryStatement() {
    const Token keyword = previous();
    auto tryBlock = blockStatement();
    std::string catchParam;
    std::shared_ptr<BlockStmt> catchBlock = nullptr;
    std::shared_ptr<BlockStmt> finallyBlock = nullptr;

    if (match(TokenType::CATCH)) {
        consume(TokenType::LPAREN, "expected ( after catch");
        catchParam = consume(TokenType::IDENTIFIER, "expected catch variable").lexeme;
        consume(TokenType::RPAREN, "expected ) after catch variable");
        catchBlock = blockStatement();
    }

    if (match(TokenType::FINALLY)) {
        finallyBlock = blockStatement();
    }

    if (!catchBlock && !finallyBlock) {
        error(previous(), "expected catch or finally after try block");
    }

    const SourceRange endRange = finallyBlock
                                     ? finallyBlock->sourceRange
                                     : (catchBlock ? catchBlock->sourceRange : tryBlock->sourceRange);
    return withRange(std::make_shared<TryStmt>(std::move(tryBlock), catchParam, std::move(catchBlock),
                                               std::move(finallyBlock)),
                     mergeRanges(keyword.range(), endRange));
}

StmtPtr Parser::expressionStatement() {
    ExprPtr expr = expression();
    consume(TokenType::SEMICOLON, "expected ; after expression");
    const SourceRange exprRange = expr->sourceRange;
    return withRange(std::make_shared<ExprStmt>(std::move(expr)), exprRange);
}

ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::assignment() {
    ExprPtr expr = pipe();

    if (matchAny({TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN})) {
        const TokenType op = previous().type;
        ExprPtr value = assignment();
        SourceRange range = mergeRanges(expr->sourceRange, value->sourceRange);
        return withRange(std::make_shared<AssignmentExpr>(std::move(expr), op, std::move(value)), range);
    }

    return expr;
}

ExprPtr Parser::pipe() {
    ExprPtr expr = logicalOr();
    while (match(TokenType::PIPE)) {
        ExprPtr right = logicalOr();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), TokenType::PIPE, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    while (match(TokenType::OR)) {
        ExprPtr right = logicalAnd();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), TokenType::OR, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = equality();
    while (match(TokenType::AND)) {
        ExprPtr right = equality();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), TokenType::AND, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    while (matchAny({TokenType::EQ, TokenType::NEQ})) {
        const TokenType op = previous().type;
        ExprPtr right = comparison();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = term();
    while (matchAny({TokenType::LT, TokenType::LTE, TokenType::GT, TokenType::GTE})) {
        const TokenType op = previous().type;
        ExprPtr right = term();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        const TokenType op = previous().type;
        ExprPtr right = factor();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        const TokenType op = previous().type;
        ExprPtr right = unary();
        SourceRange range = mergeRanges(expr->sourceRange, right->sourceRange);
        expr = withRange(std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right)), range);
    }
    return expr;
}

ExprPtr Parser::unary() {
    if (matchAny({TokenType::NOT, TokenType::MINUS})) {
        const Token op = previous();
        ExprPtr operand = unary();
        const SourceRange operandRange = operand->sourceRange;
        return withRange(std::make_shared<UnaryExpr>(op.type, std::move(operand)),
                         mergeRanges(op.range(), operandRange));
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            --current;
            expr = finishCall(std::move(expr));
        } else if (match(TokenType::LBRACKET)) {
            --current;
            expr = finishIndex(std::move(expr));
        } else if (match(TokenType::DOT)) {
            const Token property = consume(TokenType::IDENTIFIER, "expected property name after .");
            SourceRange range = mergeRanges(expr->sourceRange, property.range());
            expr = withRange(std::make_shared<MemberExpr>(std::move(expr), property.lexeme), range);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::primary() {
    if (check(TokenType::IDENTIFIER) && current + 1 < tokens.size() &&
        tokens[current + 1].type == TokenType::ARROW) {
        return parseArrowFunctionWithIdentifier();
    }

    if (check(TokenType::LPAREN) && isArrowFunctionAhead()) {
        return parseArrowFunctionWithParens();
    }

    if (match(TokenType::NUMBER)) {
        return withRange(std::make_shared<LiteralExpr>(std::stod(previous().lexeme)), previous());
    }
    if (match(TokenType::STRING)) {
        return withRange(std::make_shared<LiteralExpr>(previous().lexeme), previous());
    }
    if (match(TokenType::INTERP_STRING)) {
        return withRange(std::make_shared<StringInterpolationExpr>(parseInterpolationParts(previous())), previous());
    }
    if (match(TokenType::TRUE_)) {
        return withRange(std::make_shared<LiteralExpr>(true), previous());
    }
    if (match(TokenType::FALSE_)) {
        return withRange(std::make_shared<LiteralExpr>(false), previous());
    }
    if (match(TokenType::NULL_)) {
        return withRange(std::make_shared<LiteralExpr>(std::monostate{}), previous());
    }
    if (match(TokenType::THIS)) {
        return withRange(std::make_shared<ThisExpr>(), previous());
    }
    if (match(TokenType::IDENTIFIER)) {
        return withRange(std::make_shared<VariableExpr>(previous().lexeme), previous());
    }
    if (match(TokenType::FUNC)) {
        return parseFunctionExpression();
    }
    if (match(TokenType::NEW)) {
        const Token keyword = previous();
        const Token className = consume(TokenType::IDENTIFIER, "expected class name after new");
        ExprPtr callee = withRange(std::make_shared<VariableExpr>(className.lexeme), className);
        consume(TokenType::LPAREN, "expected ( after class name");
        std::vector<ExprPtr> arguments;
        if (!check(TokenType::RPAREN)) {
            do {
                arguments.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        const Token closingParen = consume(TokenType::RPAREN, "expected ) after constructor arguments");
        return withRange(std::make_shared<NewExpr>(std::move(callee), std::move(arguments)),
                         mergeRanges(keyword.range(), closingParen.range()));
    }
    if (match(TokenType::LBRACKET)) {
        const Token openingBracket = previous();
        std::vector<ExprPtr> elements;
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        const Token closingBracket = consume(TokenType::RBRACKET, "expected ] after array literal");
        return withRange(std::make_shared<ArrayExpr>(std::move(elements)),
                         mergeRanges(openingBracket.range(), closingBracket.range()));
    }
    if (match(TokenType::LBRACE)) {
        const Token openingBrace = previous();
        std::vector<std::pair<std::string, ExprPtr>> properties;
        if (!check(TokenType::RBRACE)) {
            do {
                std::string key;
                if (match(TokenType::STRING)) {
                    key = previous().lexeme;
                } else {
                    key = consume(TokenType::IDENTIFIER, "expected object property name").lexeme;
                }
                consume(TokenType::COLON, "expected : after object property");
                properties.emplace_back(key, expression());
            } while (match(TokenType::COMMA));
        }
        const Token closingBrace = consume(TokenType::RBRACE, "expected } after object literal");
        return withRange(std::make_shared<ObjectExpr>(std::move(properties)),
                         mergeRanges(openingBrace.range(), closingBrace.range()));
    }
    if (match(TokenType::LPAREN)) {
        const Token openingParen = previous();
        ExprPtr expr = expression();
        const Token closingParen = consume(TokenType::RPAREN, "expected ) after grouped expression");
        expr->sourceRange = mergeRanges(openingParen.range(), closingParen.range());
        return expr;
    }

    error(peek(), "unexpected token " + describeToken(peek()));
}

ExprPtr Parser::finishCall(ExprPtr callee) {
    consume(TokenType::LPAREN, "expected ( before argument list");
    std::vector<ExprPtr> arguments;
    if (!check(TokenType::RPAREN)) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::COMMA));
    }
    const Token closingParen = consume(TokenType::RPAREN, "expected ) after arguments");
    const SourceRange calleeRange = callee->sourceRange;
    return withRange(std::make_shared<CallExpr>(std::move(callee), std::move(arguments)),
                     mergeRanges(calleeRange, closingParen.range()));
}

ExprPtr Parser::finishIndex(ExprPtr object) {
    consume(TokenType::LBRACKET, "expected [ before index expression");
    ExprPtr index = expression();
    const Token closingBracket = consume(TokenType::RBRACKET, "expected ] after index expression");
    const SourceRange objectRange = object->sourceRange;
    return withRange(std::make_shared<IndexExpr>(std::move(object), std::move(index)),
                     mergeRanges(objectRange, closingBracket.range()));
}

ExprPtr Parser::parseFunctionExpression() {
    const Token keyword = previous();
    consume(TokenType::LPAREN, "expected ( after func");
    std::vector<Parameter> parameters = parseParameterList(TokenType::RPAREN);
    consume(TokenType::RPAREN, "expected ) after parameters");
    std::shared_ptr<BlockStmt> body = blockStatement();
    return withRange(std::make_shared<FunctionExpr>(std::move(parameters), std::move(body->statements)),
                     mergeRanges(keyword.range(), body->sourceRange));
}

ExprPtr Parser::parseArrowFunctionWithIdentifier() {
    const Token parameterToken = advance();
    Parameter parameter{parameterToken.lexeme, ""};
    consume(TokenType::ARROW, "expected => after arrow parameter");
    if (check(TokenType::LBRACE)) {
        std::shared_ptr<BlockStmt> body = blockStatement();
        return withRange(std::make_shared<FunctionExpr>(std::vector<Parameter>{parameter}, std::move(body->statements)),
                         mergeRanges(parameterToken.range(), body->sourceRange));
    }
    ExprPtr body = expression();
    const SourceRange bodyRange = body->sourceRange;
    return withRange(std::make_shared<FunctionExpr>(std::vector<Parameter>{parameter}, std::move(body)),
                     mergeRanges(parameterToken.range(), bodyRange));
}

ExprPtr Parser::parseArrowFunctionWithParens() {
    const Token openingParen = consume(TokenType::LPAREN, "expected ( before arrow parameters");
    std::vector<Parameter> parameters = parseParameterList(TokenType::RPAREN);
    consume(TokenType::RPAREN, "expected ) after arrow parameters");
    consume(TokenType::ARROW, "expected => after arrow parameter list");
    if (check(TokenType::LBRACE)) {
        std::shared_ptr<BlockStmt> body = blockStatement();
        return withRange(std::make_shared<FunctionExpr>(std::move(parameters), std::move(body->statements)),
                         mergeRanges(openingParen.range(), body->sourceRange));
    }
    ExprPtr body = expression();
    const SourceRange bodyRange = body->sourceRange;
    return withRange(std::make_shared<FunctionExpr>(std::move(parameters), std::move(body)),
                     mergeRanges(openingParen.range(), bodyRange));
}

std::vector<Parameter> Parser::parseParameterList(TokenType closingToken) {
    std::vector<Parameter> parameters;
    if (!check(closingToken)) {
        do {
            const std::string name = consume(TokenType::IDENTIFIER, "expected parameter name").lexeme;
            parameters.push_back(Parameter{name, parseOptionalTypeHint()});
        } while (match(TokenType::COMMA));
    }
    return parameters;
}

std::string Parser::parseOptionalTypeHint() {
    if (!match(TokenType::COLON)) {
        return {};
    }
    return consume(TokenType::IDENTIFIER, "expected type name").lexeme;
}

std::vector<std::variant<std::string, ExprPtr>> Parser::parseInterpolationParts(const Token& token) {
    const std::string& raw = token.lexeme;
    std::vector<std::variant<std::string, ExprPtr>> parts;
    std::string currentText;
    std::size_t index = 0;

    while (index < raw.size()) {
        if (raw[index] == '{') {
            if (!currentText.empty()) {
                parts.emplace_back(currentText);
                currentText.clear();
            }
            int depth = 1;
            std::size_t start = ++index;
            while (index < raw.size() && depth > 0) {
                if (raw[index] == '{') {
                    ++depth;
                } else if (raw[index] == '}') {
                    --depth;
                }
                if (depth > 0) {
                    ++index;
                }
            }
            if (depth != 0) {
                throw SyntaxError("unterminated interpolation expression", source, token.range());
            }
            const std::string inner = raw.substr(start, index - start);
            Lexer lexer(inner);
            try {
                Parser parser(lexer.tokenize());
                parts.emplace_back(parser.parseExpressionOnly());
            } catch (const SyntaxError& error) {
                throw SyntaxError("invalid interpolation expression: " + error.message(), source, token.range());
            }
            ++index;
            continue;
        }
        currentText += raw[index++];
    }

    if (!currentText.empty()) {
        parts.emplace_back(currentText);
    }

    return parts;
}

bool Parser::isArrowFunctionAhead() const {
    if (!check(TokenType::LPAREN)) {
        return false;
    }

    std::size_t index = current + 1;
    if (index >= tokens.size()) {
        return false;
    }

    if (tokens[index].type == TokenType::RPAREN) {
        return index + 1 < tokens.size() && tokens[index + 1].type == TokenType::ARROW;
    }

    while (index < tokens.size()) {
        if (tokens[index].type != TokenType::IDENTIFIER) {
            return false;
        }
        ++index;
        if (index < tokens.size() && tokens[index].type == TokenType::COLON) {
            ++index;
            if (index >= tokens.size() || tokens[index].type != TokenType::IDENTIFIER) {
                return false;
            }
            ++index;
        }
        if (index < tokens.size() && tokens[index].type == TokenType::COMMA) {
            ++index;
            continue;
        }
        if (index < tokens.size() && tokens[index].type == TokenType::RPAREN) {
            return index + 1 < tokens.size() && tokens[index + 1].type == TokenType::ARROW;
        }
        return false;
    }

    return false;
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::EOF_;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) {
        return type == TokenType::EOF_;
    }
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::matchAny(const std::vector<TokenType>& types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        ++current;
    }
    return previous();
}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(peek(), message);
}

[[noreturn]] void Parser::error(const Token& token, const std::string& message) const {
    throw SyntaxError(message, source, token.range());
}
