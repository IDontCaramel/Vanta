#include "parser.h"

#include <sstream>
#include <stdexcept>

#include "lexer.h"

namespace {

std::string tokenDescription(const Token& token) {
    std::ostringstream stream;
    stream << "line " << token.line << ", column " << token.column;
    return stream.str();
}

}  // namespace

Parser::Parser(std::vector<Token> tokenStream) : tokens(std::move(tokenStream)) {}

std::shared_ptr<ProgramStmt> Parser::parse() {
    std::vector<StmtPtr> statements;
    while (!isAtEnd()) {
        statements.push_back(declaration());
    }
    return std::make_shared<ProgramStmt>(std::move(statements));
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
        return std::make_shared<VarDeclStmt>(std::move(names), true, std::move(initializer));
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
        return std::make_shared<VarDeclStmt>(std::move(names), false, std::move(initializer));
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
    return std::make_shared<VarDeclStmt>(name, initializer, typeHint);
}

std::shared_ptr<FunctionDeclStmt> Parser::functionDeclaration(const std::string& kind, bool allowAnonymousName) {
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
    return std::make_shared<FunctionDeclStmt>(name, std::move(parameters), std::move(body->statements),
                                              returnType);
}

StmtPtr Parser::classDeclaration() {
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
    consume(TokenType::RBRACE, "expected } after class body");
    return std::make_shared<ClassDeclStmt>(name, parentName, std::move(methods));
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
        consume(TokenType::SEMICOLON, "expected ; after break");
        return std::make_shared<BreakStmt>();
    }
    if (match(TokenType::CONTINUE)) {
        consume(TokenType::SEMICOLON, "expected ; after continue");
        return std::make_shared<ContinueStmt>();
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
    consume(TokenType::LBRACE, "expected { to start block");
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(declaration());
    }
    consume(TokenType::RBRACE, "expected } after block");
    return std::make_shared<BlockStmt>(std::move(statements));
}

StmtPtr Parser::ifStatement() {
    consume(TokenType::LPAREN, "expected ( after if");
    ExprPtr condition = expression();
    consume(TokenType::RPAREN, "expected ) after if condition");
    StmtPtr thenBranch = statement();
    StmtPtr elseBranch = nullptr;
    if (match(TokenType::ELSE)) {
        elseBranch = statement();
    }
    return std::make_shared<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::whileStatement() {
    consume(TokenType::LPAREN, "expected ( after while");
    ExprPtr condition = expression();
    consume(TokenType::RPAREN, "expected ) after while condition");
    return std::make_shared<WhileStmt>(std::move(condition), statement());
}

StmtPtr Parser::forStatement() {
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
    return std::make_shared<ForStmt>(std::move(initializer), std::move(condition), std::move(increment),
                                     statement());
}

StmtPtr Parser::returnStatement() {
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }
    consume(TokenType::SEMICOLON, "expected ; after return");
    return std::make_shared<ReturnStmt>(std::move(value));
}

StmtPtr Parser::throwStatement() {
    ExprPtr value = expression();
    consume(TokenType::SEMICOLON, "expected ; after throw");
    return std::make_shared<ThrowStmt>(std::move(value));
}

StmtPtr Parser::tryStatement() {
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

    return std::make_shared<TryStmt>(std::move(tryBlock), catchParam, std::move(catchBlock),
                                     std::move(finallyBlock));
}

StmtPtr Parser::expressionStatement() {
    ExprPtr expr = expression();
    consume(TokenType::SEMICOLON, "expected ; after expression");
    return std::make_shared<ExprStmt>(std::move(expr));
}

ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::assignment() {
    ExprPtr expr = pipe();

    if (matchAny({TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN})) {
        const TokenType op = previous().type;
        ExprPtr value = assignment();
        return std::make_shared<AssignmentExpr>(std::move(expr), op, std::move(value));
    }

    return expr;
}

ExprPtr Parser::pipe() {
    ExprPtr expr = logicalOr();
    while (match(TokenType::PIPE)) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), TokenType::PIPE, logicalOr());
    }
    return expr;
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    while (match(TokenType::OR)) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), TokenType::OR, logicalAnd());
    }
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = equality();
    while (match(TokenType::AND)) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), TokenType::AND, equality());
    }
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    while (matchAny({TokenType::EQ, TokenType::NEQ})) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), previous().type, comparison());
    }
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = term();
    while (matchAny({TokenType::LT, TokenType::LTE, TokenType::GT, TokenType::GTE})) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), previous().type, term());
    }
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), previous().type, factor());
    }
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        expr = std::make_shared<BinaryExpr>(std::move(expr), previous().type, unary());
    }
    return expr;
}

ExprPtr Parser::unary() {
    if (matchAny({TokenType::NOT, TokenType::MINUS})) {
        return std::make_shared<UnaryExpr>(previous().type, unary());
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            --current;
            expr = finishCall(std::move(expr));
        } else if (match(TokenType::DOT)) {
            const std::string property =
                consume(TokenType::IDENTIFIER, "expected property name after .").lexeme;
            expr = std::make_shared<MemberExpr>(std::move(expr), property);
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
        return std::make_shared<LiteralExpr>(std::stod(previous().lexeme));
    }
    if (match(TokenType::STRING)) {
        return std::make_shared<LiteralExpr>(previous().lexeme);
    }
    if (match(TokenType::INTERP_STRING)) {
        return std::make_shared<StringInterpolationExpr>(parseInterpolationParts(previous().lexeme));
    }
    if (match(TokenType::TRUE_)) {
        return std::make_shared<LiteralExpr>(true);
    }
    if (match(TokenType::FALSE_)) {
        return std::make_shared<LiteralExpr>(false);
    }
    if (match(TokenType::NULL_)) {
        return std::make_shared<LiteralExpr>(std::monostate{});
    }
    if (match(TokenType::THIS)) {
        return std::make_shared<ThisExpr>();
    }
    if (match(TokenType::IDENTIFIER)) {
        return std::make_shared<VariableExpr>(previous().lexeme);
    }
    if (match(TokenType::FUNC)) {
        return parseFunctionExpression();
    }
    if (match(TokenType::NEW)) {
        ExprPtr callee = std::make_shared<VariableExpr>(
            consume(TokenType::IDENTIFIER, "expected class name after new").lexeme);
        consume(TokenType::LPAREN, "expected ( after class name");
        std::vector<ExprPtr> arguments;
        if (!check(TokenType::RPAREN)) {
            do {
                arguments.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "expected ) after constructor arguments");
        return std::make_shared<NewExpr>(std::move(callee), std::move(arguments));
    }
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprPtr> elements;
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACKET, "expected ] after array literal");
        return std::make_shared<ArrayExpr>(std::move(elements));
    }
    if (match(TokenType::LBRACE)) {
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
        consume(TokenType::RBRACE, "expected } after object literal");
        return std::make_shared<ObjectExpr>(std::move(properties));
    }
    if (match(TokenType::LPAREN)) {
        ExprPtr expr = expression();
        consume(TokenType::RPAREN, "expected ) after grouped expression");
        return expr;
    }

    error(peek(), "unexpected token");
}

ExprPtr Parser::finishCall(ExprPtr callee) {
    consume(TokenType::LPAREN, "expected ( before argument list");
    std::vector<ExprPtr> arguments;
    if (!check(TokenType::RPAREN)) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "expected ) after arguments");
    return std::make_shared<CallExpr>(std::move(callee), std::move(arguments));
}

ExprPtr Parser::parseFunctionExpression() {
    consume(TokenType::LPAREN, "expected ( after func");
    std::vector<Parameter> parameters = parseParameterList(TokenType::RPAREN);
    consume(TokenType::RPAREN, "expected ) after parameters");
    std::shared_ptr<BlockStmt> body = blockStatement();
    return std::make_shared<FunctionExpr>(std::move(parameters), std::move(body->statements));
}

ExprPtr Parser::parseArrowFunctionWithIdentifier() {
    Parameter parameter{advance().lexeme, ""};
    consume(TokenType::ARROW, "expected => after arrow parameter");
    if (check(TokenType::LBRACE)) {
        std::shared_ptr<BlockStmt> body = blockStatement();
        return std::make_shared<FunctionExpr>(std::vector<Parameter>{parameter}, std::move(body->statements));
    }
    return std::make_shared<FunctionExpr>(std::vector<Parameter>{parameter}, expression());
}

ExprPtr Parser::parseArrowFunctionWithParens() {
    consume(TokenType::LPAREN, "expected ( before arrow parameters");
    std::vector<Parameter> parameters = parseParameterList(TokenType::RPAREN);
    consume(TokenType::RPAREN, "expected ) after arrow parameters");
    consume(TokenType::ARROW, "expected => after arrow parameter list");
    if (check(TokenType::LBRACE)) {
        std::shared_ptr<BlockStmt> body = blockStatement();
        return std::make_shared<FunctionExpr>(std::move(parameters), std::move(body->statements));
    }
    return std::make_shared<FunctionExpr>(std::move(parameters), expression());
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

std::vector<std::variant<std::string, ExprPtr>> Parser::parseInterpolationParts(const std::string& raw) {
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
                throw std::runtime_error("unterminated interpolation expression");
            }
            const std::string inner = raw.substr(start, index - start);
            Lexer lexer(inner);
            Parser parser(lexer.tokenize());
            parts.emplace_back(parser.parseExpressionOnly());
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
    throw std::runtime_error("Parser error at " + tokenDescription(token) + ": " + message);
}
