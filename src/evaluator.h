#pragma once

#include <iostream>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "ast.h"
#include "source_file.h"
#include "environment.h"
#include "value.h"

struct ReturnSignal {
    Value value;
};

struct BreakSignal {};
struct ContinueSignal {};

struct ThrowSignal {
    Value value;
    SourceRange range;
};

class Evaluator {
public:
    explicit Evaluator(std::ostream& out = std::cout, std::istream& in = std::cin,
                       std::shared_ptr<const SourceDocument> source = nullptr);

    Value evaluate(const StmtPtr& stmt);
    Value evaluateExpr(const ExprPtr& expr);
    Value callValue(const Value& callee, const std::vector<Value>& arguments, const ExprPtr& callSite = nullptr);
    void setSourceDocument(std::shared_ptr<const SourceDocument> sourceDocument);

    std::shared_ptr<Environment> globals() const;

private:
    std::ostream* output;
    std::istream* input;
    std::shared_ptr<const SourceDocument> source;
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;

    void registerBuiltins();
    Value executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> environment);
    Value assignTarget(const ExprPtr& target, TokenType op, const Value& rhs, const SourceRange& assignmentRange);
    Value getMember(const Value& object, const std::string& property, const SourceRange& range);
    Value getIndex(const Value& object, const Value& index, const SourceRange& range);
    Value assignIndex(const Value& object, const Value& index, TokenType op, const Value& rhs, const SourceRange& range);
    Value makeArrayMethod(const Value& object, const std::string& property);
    Value makeStringMethod(const Value& object, const std::string& property);
    std::shared_ptr<FunctionValue> bindFunction(const std::shared_ptr<FunctionValue>& fn, const Value& self);
    [[noreturn]] void runtimeError(const SourceRange& range, const std::string& message) const;
    std::string describePropertyOwner(const Value& object) const;
    void enforceTypeHint(const Value& value, const std::string& typeHint, const std::string& context) const;
};
