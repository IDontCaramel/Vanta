#include "debug_output.h"

#include <sstream>
#include <variant>

namespace {

std::string escapeLexeme(const std::string& lexeme) {
    std::ostringstream stream;
    stream << '"';
    for (char ch : lexeme) {
        switch (ch) {
            case '\\':
                stream << "\\\\";
                break;
            case '"':
                stream << "\\\"";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                stream << ch;
                break;
        }
    }
    stream << '"';
    return stream.str();
}

std::string tokenOperatorName(TokenType type) {
    switch (type) {
        case TokenType::PLUS:
            return "+";
        case TokenType::MINUS:
            return "-";
        case TokenType::STAR:
            return "*";
        case TokenType::SLASH:
            return "/";
        case TokenType::PERCENT:
            return "%";
        case TokenType::EQ:
            return "==";
        case TokenType::NEQ:
            return "!=";
        case TokenType::LT:
            return "<";
        case TokenType::LTE:
            return "<=";
        case TokenType::GT:
            return ">";
        case TokenType::GTE:
            return ">=";
        case TokenType::AND:
            return "&&";
        case TokenType::OR:
            return "||";
        case TokenType::NOT:
            return "!";
        case TokenType::ASSIGN:
            return "=";
        case TokenType::PLUS_ASSIGN:
            return "+=";
        case TokenType::MINUS_ASSIGN:
            return "-=";
        case TokenType::PIPE:
            return "|>";
        default:
            return tokenTypeName(type);
    }
}

class AstFormatter {
public:
    std::string formatProgram(const std::shared_ptr<ProgramStmt>& program) {
        line(0, "Program");
        for (const auto& statement : program->statements) {
            formatStmt(statement, 1);
        }
        return stream.str();
    }

    std::string formatExpression(const ExprPtr& expression) {
        formatExpr(expression, 0);
        return stream.str();
    }

private:
    std::ostringstream stream;

    void line(int indent, const std::string& text) {
        stream << std::string(static_cast<std::size_t>(indent) * 2, ' ') << text << '\n';
    }

    void formatParameters(const std::vector<Parameter>& parameters, int indent) {
        line(indent, "Parameters");
        for (const auto& parameter : parameters) {
            if (parameter.typeHint.empty()) {
                line(indent + 1, parameter.name);
            } else {
                line(indent + 1, parameter.name + ": " + parameter.typeHint);
            }
        }
    }

    void formatBody(const std::vector<StmtPtr>& body, int indent) {
        line(indent, "Body");
        for (const auto& statement : body) {
            formatStmt(statement, indent + 1);
        }
    }

    void formatStmt(const StmtPtr& statement, int indent) {
        if (auto expr = std::dynamic_pointer_cast<ExprStmt>(statement)) {
            line(indent, "ExprStmt");
            formatExpr(expr->expr, indent + 1);
            return;
        }
        if (auto decl = std::dynamic_pointer_cast<VarDeclStmt>(statement)) {
            if (decl->isDestructuring) {
                line(indent, decl->isArrayPattern ? "VarDecl[array destructuring]" : "VarDecl[object destructuring]");
                line(indent + 1, "Names");
                for (const auto& name : decl->patternNames) {
                    line(indent + 2, name);
                }
            } else {
                line(indent, decl->typeHint.empty() ? "VarDecl(" + decl->name + ")"
                                                    : "VarDecl(" + decl->name + ": " + decl->typeHint + ")");
            }
            if (decl->initializer) {
                line(indent + 1, "Initializer");
                formatExpr(decl->initializer, indent + 2);
            }
            return;
        }
        if (auto block = std::dynamic_pointer_cast<BlockStmt>(statement)) {
            line(indent, "Block");
            for (const auto& child : block->statements) {
                formatStmt(child, indent + 1);
            }
            return;
        }
        if (auto branch = std::dynamic_pointer_cast<IfStmt>(statement)) {
            line(indent, "IfStmt");
            line(indent + 1, "Condition");
            formatExpr(branch->condition, indent + 2);
            line(indent + 1, "Then");
            formatStmt(branch->thenBranch, indent + 2);
            if (branch->elseBranch) {
                line(indent + 1, "Else");
                formatStmt(branch->elseBranch, indent + 2);
            }
            return;
        }
        if (auto loop = std::dynamic_pointer_cast<WhileStmt>(statement)) {
            line(indent, "WhileStmt");
            line(indent + 1, "Condition");
            formatExpr(loop->condition, indent + 2);
            line(indent + 1, "Body");
            formatStmt(loop->body, indent + 2);
            return;
        }
        if (auto loop = std::dynamic_pointer_cast<ForStmt>(statement)) {
            line(indent, "ForStmt");
            if (loop->initializer) {
                line(indent + 1, "Initializer");
                formatStmt(loop->initializer, indent + 2);
            }
            if (loop->condition) {
                line(indent + 1, "Condition");
                formatExpr(loop->condition, indent + 2);
            }
            if (loop->increment) {
                line(indent + 1, "Increment");
                formatExpr(loop->increment, indent + 2);
            }
            line(indent + 1, "Body");
            formatStmt(loop->body, indent + 2);
            return;
        }
        if (auto result = std::dynamic_pointer_cast<ReturnStmt>(statement)) {
            line(indent, "ReturnStmt");
            if (result->value) {
                formatExpr(result->value, indent + 1);
            }
            return;
        }
        if (std::dynamic_pointer_cast<BreakStmt>(statement)) {
            line(indent, "BreakStmt");
            return;
        }
        if (std::dynamic_pointer_cast<ContinueStmt>(statement)) {
            line(indent, "ContinueStmt");
            return;
        }
        if (auto throwing = std::dynamic_pointer_cast<ThrowStmt>(statement)) {
            line(indent, "ThrowStmt");
            formatExpr(throwing->value, indent + 1);
            return;
        }
        if (auto trying = std::dynamic_pointer_cast<TryStmt>(statement)) {
            line(indent, "TryStmt");
            line(indent + 1, "Try");
            formatStmt(trying->tryBlock, indent + 2);
            if (trying->catchBlock) {
                line(indent + 1, "Catch(" + trying->catchParam + ")");
                formatStmt(trying->catchBlock, indent + 2);
            }
            if (trying->finallyBlock) {
                line(indent + 1, "Finally");
                formatStmt(trying->finallyBlock, indent + 2);
            }
            return;
        }
        if (auto function = std::dynamic_pointer_cast<FunctionDeclStmt>(statement)) {
            line(indent, function->returnType.empty() ? "FunctionDecl(" + function->name + ")"
                                                      : "FunctionDecl(" + function->name + " -> " + function->returnType + ")");
            formatParameters(function->parameters, indent + 1);
            formatBody(function->body, indent + 1);
            return;
        }
        if (auto klass = std::dynamic_pointer_cast<ClassDeclStmt>(statement)) {
            line(indent, klass->parentName.empty() ? "ClassDecl(" + klass->name + ")"
                                                   : "ClassDecl(" + klass->name + " extends " + klass->parentName + ")");
            for (const auto& method : klass->methods) {
                formatStmt(method, indent + 1);
            }
            return;
        }
        if (auto program = std::dynamic_pointer_cast<ProgramStmt>(statement)) {
            line(indent, "Program");
            for (const auto& child : program->statements) {
                formatStmt(child, indent + 1);
            }
            return;
        }

        line(indent, "UnknownStmt");
    }

    void formatExpr(const ExprPtr& expression, int indent) {
        if (auto literal = std::dynamic_pointer_cast<LiteralExpr>(expression)) {
            if (std::holds_alternative<std::monostate>(literal->value)) {
                line(indent, "Literal(null)");
            } else if (std::holds_alternative<double>(literal->value)) {
                std::ostringstream value;
                value << std::get<double>(literal->value);
                line(indent, "Literal(" + value.str() + ")");
            } else if (std::holds_alternative<std::string>(literal->value)) {
                line(indent, "Literal(" + escapeLexeme(std::get<std::string>(literal->value)) + ")");
            } else {
                line(indent, std::get<bool>(literal->value) ? "Literal(true)" : "Literal(false)");
            }
            return;
        }
        if (auto variable = std::dynamic_pointer_cast<VariableExpr>(expression)) {
            line(indent, "Variable(" + variable->name + ")");
            return;
        }
        if (std::dynamic_pointer_cast<ThisExpr>(expression)) {
            line(indent, "This");
            return;
        }
        if (auto array = std::dynamic_pointer_cast<ArrayExpr>(expression)) {
            line(indent, "ArrayExpr");
            for (const auto& element : array->elements) {
                formatExpr(element, indent + 1);
            }
            return;
        }
        if (auto object = std::dynamic_pointer_cast<ObjectExpr>(expression)) {
            line(indent, "ObjectExpr");
            for (const auto& [key, value] : object->properties) {
                line(indent + 1, "Property(" + key + ")");
                formatExpr(value, indent + 2);
            }
            return;
        }
        if (auto member = std::dynamic_pointer_cast<MemberExpr>(expression)) {
            line(indent, "Member(" + member->property + ")");
            line(indent + 1, "Object");
            formatExpr(member->object, indent + 2);
            return;
        }
        if (auto index = std::dynamic_pointer_cast<IndexExpr>(expression)) {
            line(indent, "IndexExpr");
            line(indent + 1, "Object");
            formatExpr(index->object, indent + 2);
            line(indent + 1, "Index");
            formatExpr(index->index, indent + 2);
            return;
        }
        if (auto unary = std::dynamic_pointer_cast<UnaryExpr>(expression)) {
            line(indent, "Unary(" + tokenOperatorName(unary->op) + ")");
            formatExpr(unary->operand, indent + 1);
            return;
        }
        if (auto binary = std::dynamic_pointer_cast<BinaryExpr>(expression)) {
            line(indent, "Binary(" + tokenOperatorName(binary->op) + ")");
            formatExpr(binary->left, indent + 1);
            formatExpr(binary->right, indent + 1);
            return;
        }
        if (auto assignment = std::dynamic_pointer_cast<AssignmentExpr>(expression)) {
            line(indent, "Assignment(" + tokenOperatorName(assignment->op) + ")");
            line(indent + 1, "Target");
            formatExpr(assignment->target, indent + 2);
            line(indent + 1, "Value");
            formatExpr(assignment->value, indent + 2);
            return;
        }
        if (auto call = std::dynamic_pointer_cast<CallExpr>(expression)) {
            line(indent, "CallExpr");
            line(indent + 1, "Callee");
            formatExpr(call->callee, indent + 2);
            line(indent + 1, "Arguments");
            for (const auto& argument : call->arguments) {
                formatExpr(argument, indent + 2);
            }
            return;
        }
        if (auto function = std::dynamic_pointer_cast<FunctionExpr>(expression)) {
            line(indent, function->implicitReturn ? "FunctionExpr[arrow]" : "FunctionExpr");
            formatParameters(function->parameters, indent + 1);
            if (function->implicitReturn && function->expressionBody) {
                line(indent + 1, "ExpressionBody");
                formatExpr(function->expressionBody, indent + 2);
            } else {
                formatBody(function->body, indent + 1);
            }
            return;
        }
        if (auto construct = std::dynamic_pointer_cast<NewExpr>(expression)) {
            line(indent, "NewExpr");
            line(indent + 1, "Callee");
            formatExpr(construct->callee, indent + 2);
            line(indent + 1, "Arguments");
            for (const auto& argument : construct->arguments) {
                formatExpr(argument, indent + 2);
            }
            return;
        }
        if (auto interpolation = std::dynamic_pointer_cast<StringInterpolationExpr>(expression)) {
            line(indent, "StringInterpolationExpr");
            for (const auto& part : interpolation->parts) {
                if (std::holds_alternative<std::string>(part)) {
                    line(indent + 1, "Text(" + escapeLexeme(std::get<std::string>(part)) + ")");
                } else {
                    line(indent + 1, "Expr");
                    formatExpr(std::get<ExprPtr>(part), indent + 2);
                }
            }
            return;
        }

        line(indent, "UnknownExpr");
    }
};

}  // namespace

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::NUMBER:
            return "NUMBER";
        case TokenType::STRING:
            return "STRING";
        case TokenType::INTERP_STRING:
            return "INTERP_STRING";
        case TokenType::IDENTIFIER:
            return "IDENTIFIER";
        case TokenType::VAR:
            return "VAR";
        case TokenType::FUNC:
            return "FUNC";
        case TokenType::CLASS:
            return "CLASS";
        case TokenType::RETURN:
            return "RETURN";
        case TokenType::IF:
            return "IF";
        case TokenType::ELSE:
            return "ELSE";
        case TokenType::WHILE:
            return "WHILE";
        case TokenType::FOR:
            return "FOR";
        case TokenType::BREAK:
            return "BREAK";
        case TokenType::CONTINUE:
            return "CONTINUE";
        case TokenType::NEW:
            return "NEW";
        case TokenType::THIS:
            return "THIS";
        case TokenType::TRY:
            return "TRY";
        case TokenType::CATCH:
            return "CATCH";
        case TokenType::FINALLY:
            return "FINALLY";
        case TokenType::THROW:
            return "THROW";
        case TokenType::EXTENDS:
            return "EXTENDS";
        case TokenType::TRUE_:
            return "TRUE";
        case TokenType::FALSE_:
            return "FALSE";
        case TokenType::NULL_:
            return "NULL";
        case TokenType::PLUS:
            return "PLUS";
        case TokenType::MINUS:
            return "MINUS";
        case TokenType::STAR:
            return "STAR";
        case TokenType::SLASH:
            return "SLASH";
        case TokenType::PERCENT:
            return "PERCENT";
        case TokenType::EQ:
            return "EQ";
        case TokenType::NEQ:
            return "NEQ";
        case TokenType::LT:
            return "LT";
        case TokenType::LTE:
            return "LTE";
        case TokenType::GT:
            return "GT";
        case TokenType::GTE:
            return "GTE";
        case TokenType::AND:
            return "AND";
        case TokenType::OR:
            return "OR";
        case TokenType::NOT:
            return "NOT";
        case TokenType::ASSIGN:
            return "ASSIGN";
        case TokenType::PLUS_ASSIGN:
            return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN:
            return "MINUS_ASSIGN";
        case TokenType::LPAREN:
            return "LPAREN";
        case TokenType::RPAREN:
            return "RPAREN";
        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::LBRACKET:
            return "LBRACKET";
        case TokenType::RBRACKET:
            return "RBRACKET";
        case TokenType::DOT:
            return "DOT";
        case TokenType::COMMA:
            return "COMMA";
        case TokenType::SEMICOLON:
            return "SEMICOLON";
        case TokenType::COLON:
            return "COLON";
        case TokenType::ARROW:
            return "ARROW";
        case TokenType::TYPE_ARROW:
            return "TYPE_ARROW";
        case TokenType::PIPE:
            return "PIPE";
        case TokenType::EOF_:
            return "EOF";
        case TokenType::ERROR_:
            return "ERROR";
    }

    return "UNKNOWN";
}

std::string formatTokens(const std::vector<Token>& tokens) {
    std::ostringstream stream;
    for (const auto& token : tokens) {
        stream << token.line << ':' << token.column << ' ' << tokenTypeName(token.type) << ' '
               << escapeLexeme(token.lexeme) << '\n';
    }
    return stream.str();
}

std::string formatAst(const std::shared_ptr<ProgramStmt>& program) {
    AstFormatter formatter;
    return formatter.formatProgram(program);
}

std::string formatAst(const ExprPtr& expression) {
    AstFormatter formatter;
    return formatter.formatExpression(expression);
}
