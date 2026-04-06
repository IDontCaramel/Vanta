#pragma once

#include <iostream>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "ast.h"
#include "environment.h"
#include "value.h"

struct ReturnSignal {
    Value value;
};

struct BreakSignal {};
struct ContinueSignal {};

struct ThrowSignal {
    Value value;
};

class Evaluator {
public:
    explicit Evaluator(std::ostream& out = std::cout, std::istream& in = std::cin);

    Value evaluate(const StmtPtr& stmt);
    Value evaluateExpr(const ExprPtr& expr);
    Value callValue(const Value& callee, const std::vector<Value>& arguments);

    std::shared_ptr<Environment> globals() const;

private:
    std::ostream* output;
    std::istream* input;
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;

    void registerBuiltins();
    Value executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> environment);
    Value assignTarget(const ExprPtr& target, TokenType op, const Value& rhs);
    Value getMember(const Value& object, const std::string& property);
    Value makeArrayMethod(const Value& object, const std::string& property);
    Value makeStringMethod(const Value& object, const std::string& property);
    std::shared_ptr<FunctionValue> bindFunction(const std::shared_ptr<FunctionValue>& fn, const Value& self);
};
