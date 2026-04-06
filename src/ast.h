#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "token.h"

struct Expr;
struct Stmt;

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

struct Parameter {
    std::string name;
    std::string typeHint;
};

struct Expr {
    virtual ~Expr() = default;
};

struct LiteralExpr final : Expr {
    std::variant<std::monostate, double, std::string, bool> value;

    explicit LiteralExpr(std::variant<std::monostate, double, std::string, bool> literal)
        : value(std::move(literal)) {}
};

struct VariableExpr final : Expr {
    std::string name;

    explicit VariableExpr(std::string variableName) : name(std::move(variableName)) {}
};

struct ThisExpr final : Expr {};

struct ArrayExpr final : Expr {
    std::vector<ExprPtr> elements;

    explicit ArrayExpr(std::vector<ExprPtr> exprs) : elements(std::move(exprs)) {}
};

struct ObjectExpr final : Expr {
    std::vector<std::pair<std::string, ExprPtr>> properties;

    explicit ObjectExpr(std::vector<std::pair<std::string, ExprPtr>> props)
        : properties(std::move(props)) {}
};

struct MemberExpr final : Expr {
    ExprPtr object;
    std::string property;

    MemberExpr(ExprPtr owner, std::string member)
        : object(std::move(owner)), property(std::move(member)) {}
};

struct UnaryExpr final : Expr {
    TokenType op;
    ExprPtr operand;

    UnaryExpr(TokenType tokenType, ExprPtr expr) : op(tokenType), operand(std::move(expr)) {}
};

struct BinaryExpr final : Expr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;

    BinaryExpr(ExprPtr lhs, TokenType tokenType, ExprPtr rhs)
        : left(std::move(lhs)), op(tokenType), right(std::move(rhs)) {}
};

struct AssignmentExpr final : Expr {
    ExprPtr target;
    TokenType op;
    ExprPtr value;

    AssignmentExpr(ExprPtr lhs, TokenType tokenType, ExprPtr rhs)
        : target(std::move(lhs)), op(tokenType), value(std::move(rhs)) {}
};

struct CallExpr final : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr fn, std::vector<ExprPtr> args)
        : callee(std::move(fn)), arguments(std::move(args)) {}
};

struct FunctionExpr final : Expr {
    std::vector<Parameter> parameters;
    std::vector<StmtPtr> body;
    bool implicitReturn = false;
    ExprPtr expressionBody;

    FunctionExpr(std::vector<Parameter> params, std::vector<StmtPtr> stmts)
        : parameters(std::move(params)), body(std::move(stmts)) {}

    FunctionExpr(std::vector<Parameter> params, ExprPtr exprBody)
        : parameters(std::move(params)), implicitReturn(true), expressionBody(std::move(exprBody)) {}
};

struct NewExpr final : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    NewExpr(ExprPtr ctor, std::vector<ExprPtr> args)
        : callee(std::move(ctor)), arguments(std::move(args)) {}
};

struct StringInterpolationExpr final : Expr {
    std::vector<std::variant<std::string, ExprPtr>> parts;

    explicit StringInterpolationExpr(std::vector<std::variant<std::string, ExprPtr>> interpolationParts)
        : parts(std::move(interpolationParts)) {}
};

struct Stmt {
    virtual ~Stmt() = default;
};

struct ExprStmt final : Stmt {
    ExprPtr expr;

    explicit ExprStmt(ExprPtr value) : expr(std::move(value)) {}
};

struct VarDeclStmt final : Stmt {
    std::string name;
    std::string typeHint;
    ExprPtr initializer;
    bool isDestructuring = false;
    bool isArrayPattern = false;
    std::vector<std::string> patternNames;

    VarDeclStmt(std::string variableName, ExprPtr init, std::string hint = {})
        : name(std::move(variableName)), typeHint(std::move(hint)), initializer(std::move(init)) {}

    VarDeclStmt(std::vector<std::string> names, bool arrayPattern, ExprPtr init)
        : initializer(std::move(init)),
          isDestructuring(true),
          isArrayPattern(arrayPattern),
          patternNames(std::move(names)) {}
};

struct BlockStmt final : Stmt {
    std::vector<StmtPtr> statements;

    explicit BlockStmt(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};

struct IfStmt final : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    IfStmt(ExprPtr cond, StmtPtr thenStmt, StmtPtr elseStmt = nullptr)
        : condition(std::move(cond)),
          thenBranch(std::move(thenStmt)),
          elseBranch(std::move(elseStmt)) {}
};

struct WhileStmt final : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(ExprPtr cond, StmtPtr stmt) : condition(std::move(cond)), body(std::move(stmt)) {}
};

struct ForStmt final : Stmt {
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;

    ForStmt(StmtPtr init, ExprPtr cond, ExprPtr inc, StmtPtr stmt)
        : initializer(std::move(init)),
          condition(std::move(cond)),
          increment(std::move(inc)),
          body(std::move(stmt)) {}
};

struct ReturnStmt final : Stmt {
    ExprPtr value;

    explicit ReturnStmt(ExprPtr expr = nullptr) : value(std::move(expr)) {}
};

struct BreakStmt final : Stmt {};
struct ContinueStmt final : Stmt {};

struct ThrowStmt final : Stmt {
    ExprPtr value;

    explicit ThrowStmt(ExprPtr expr) : value(std::move(expr)) {}
};

struct TryStmt final : Stmt {
    std::shared_ptr<BlockStmt> tryBlock;
    std::string catchParam;
    std::shared_ptr<BlockStmt> catchBlock;
    std::shared_ptr<BlockStmt> finallyBlock;

    TryStmt(std::shared_ptr<BlockStmt> tryStmt, std::string catchName,
            std::shared_ptr<BlockStmt> catchStmt, std::shared_ptr<BlockStmt> finallyStmt)
        : tryBlock(std::move(tryStmt)),
          catchParam(std::move(catchName)),
          catchBlock(std::move(catchStmt)),
          finallyBlock(std::move(finallyStmt)) {}
};

struct FunctionDeclStmt final : Stmt {
    std::string name;
    std::vector<Parameter> parameters;
    std::vector<StmtPtr> body;
    std::string returnType;

    FunctionDeclStmt(std::string functionName, std::vector<Parameter> params,
                     std::vector<StmtPtr> stmts, std::string type = {})
        : name(std::move(functionName)),
          parameters(std::move(params)),
          body(std::move(stmts)),
          returnType(std::move(type)) {}
};

struct ClassDeclStmt final : Stmt {
    std::string name;
    std::string parentName;
    std::vector<std::shared_ptr<FunctionDeclStmt>> methods;

    ClassDeclStmt(std::string className, std::string parent,
                  std::vector<std::shared_ptr<FunctionDeclStmt>> classMethods)
        : name(std::move(className)),
          parentName(std::move(parent)),
          methods(std::move(classMethods)) {}
};

struct ProgramStmt final : Stmt {
    std::vector<StmtPtr> statements;

    explicit ProgramStmt(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};
