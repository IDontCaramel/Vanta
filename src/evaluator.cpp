#include "evaluator.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

double clampIndex(double value, std::size_t upperBound) {
    if (value < 0) {
        return 0;
    }
    if (value > static_cast<double>(upperBound)) {
        return static_cast<double>(upperBound);
    }
    return value;
}

}  // namespace

Evaluator::Evaluator(std::ostream& out, std::istream& in)
    : output(&out),
      input(&in),
      globalEnv(std::make_shared<Environment>()),
      currentEnv(globalEnv) {
    registerBuiltins();
}

Value Evaluator::evaluate(const StmtPtr& stmt) {
    if (auto program = std::dynamic_pointer_cast<ProgramStmt>(stmt)) {
        Value last;
        for (const auto& statement : program->statements) {
            last = evaluate(statement);
        }
        return last;
    }
    if (auto expression = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
        return evaluateExpr(expression->expr);
    }
    if (auto declaration = std::dynamic_pointer_cast<VarDeclStmt>(stmt)) {
        const Value initializer = declaration->initializer ? evaluateExpr(declaration->initializer) : Value();
        if (declaration->isDestructuring) {
            if (declaration->isArrayPattern) {
                auto array = initializer.as<ArrayValue>();
                if (!array) {
                    throw std::runtime_error("array destructuring requires an array");
                }
                for (std::size_t i = 0; i < declaration->patternNames.size(); ++i) {
                    const Value element = i < array->elements.size() ? array->elements[i] : Value();
                    currentEnv->define(declaration->patternNames[i], element);
                }
            } else {
                auto object = initializer.as<ObjectValue>();
                auto instance = initializer.as<InstanceValue>();
                if (!object && !instance) {
                    throw std::runtime_error("object destructuring requires an object");
                }
                for (const auto& name : declaration->patternNames) {
                    if (object && object->properties.count(name) > 0) {
                        currentEnv->define(name, object->properties[name]);
                    } else if (instance && instance->fields.count(name) > 0) {
                        currentEnv->define(name, instance->fields[name]);
                    } else {
                        currentEnv->define(name, Value());
                    }
                }
            }
            return Value();
        }
        currentEnv->define(declaration->name, initializer);
        return initializer;
    }
    if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
        return executeBlock(block->statements, std::make_shared<Environment>(currentEnv));
    }
    if (auto branch = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        if (evaluateExpr(branch->condition).isTruthy()) {
            return evaluate(branch->thenBranch);
        }
        if (branch->elseBranch) {
            return evaluate(branch->elseBranch);
        }
        return Value();
    }
    if (auto loop = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        Value last;
        while (evaluateExpr(loop->condition).isTruthy()) {
            try {
                last = evaluate(loop->body);
            } catch (const ContinueSignal&) {
                continue;
            } catch (const BreakSignal&) {
                break;
            }
        }
        return last;
    }
    if (auto loop = std::dynamic_pointer_cast<ForStmt>(stmt)) {
        const auto loopEnv = std::make_shared<Environment>(currentEnv);
        const auto previous = currentEnv;
        currentEnv = loopEnv;
        Value last;
        try {
            if (loop->initializer) {
                evaluate(loop->initializer);
            }
            while (!loop->condition || evaluateExpr(loop->condition).isTruthy()) {
                try {
                    last = evaluate(loop->body);
                } catch (const ContinueSignal&) {
                } catch (const BreakSignal&) {
                    break;
                }
                if (loop->increment) {
                    evaluateExpr(loop->increment);
                }
            }
        } catch (...) {
            currentEnv = previous;
            throw;
        }
        currentEnv = previous;
        return last;
    }
    if (auto result = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
        throw ReturnSignal{result->value ? evaluateExpr(result->value) : Value()};
    }
    if (std::dynamic_pointer_cast<BreakStmt>(stmt)) {
        throw BreakSignal{};
    }
    if (std::dynamic_pointer_cast<ContinueStmt>(stmt)) {
        throw ContinueSignal{};
    }
    if (auto throwing = std::dynamic_pointer_cast<ThrowStmt>(stmt)) {
        throw ThrowSignal{evaluateExpr(throwing->value)};
    }
    if (auto trying = std::dynamic_pointer_cast<TryStmt>(stmt)) {
        try {
            Value result = executeBlock(trying->tryBlock->statements, std::make_shared<Environment>(currentEnv));
            if (trying->finallyBlock) {
                executeBlock(trying->finallyBlock->statements, std::make_shared<Environment>(currentEnv));
            }
            return result;
        } catch (const ThrowSignal& signal) {
            if (!trying->catchBlock) {
                if (trying->finallyBlock) {
                    executeBlock(trying->finallyBlock->statements, std::make_shared<Environment>(currentEnv));
                }
                throw;
            }
            auto catchEnv = std::make_shared<Environment>(currentEnv);
            catchEnv->define(trying->catchParam, signal.value);
            Value result = executeBlock(trying->catchBlock->statements, catchEnv);
            if (trying->finallyBlock) {
                executeBlock(trying->finallyBlock->statements, std::make_shared<Environment>(currentEnv));
            }
            return result;
        } catch (...) {
            if (trying->finallyBlock) {
                executeBlock(trying->finallyBlock->statements, std::make_shared<Environment>(currentEnv));
            }
            throw;
        }
    }
    if (auto function = std::dynamic_pointer_cast<FunctionDeclStmt>(stmt)) {
        auto value = std::make_shared<FunctionValue>(function->name, function->parameters, function->body, currentEnv);
        if (function->name == "__init__") {
            value->isInitializer = true;
        }
        currentEnv->define(function->name, Value::fromHeap(value));
        return Value::fromHeap(value);
    }
    if (auto klass = std::dynamic_pointer_cast<ClassDeclStmt>(stmt)) {
        std::shared_ptr<ClassValue> superClass = nullptr;
        if (!klass->parentName.empty()) {
            Value parent = currentEnv->get(klass->parentName);
            superClass = parent.as<ClassValue>();
            if (!superClass) {
                throw std::runtime_error("parent class must be a class");
            }
        }

        currentEnv->define(klass->name, Value());
        std::map<std::string, std::shared_ptr<FunctionValue>> methods;
        for (const auto& method : klass->methods) {
            auto fn = std::make_shared<FunctionValue>(method->name, method->parameters, method->body, currentEnv);
            fn->isInitializer = method->name == "__init__";
            methods[method->name] = fn;
        }
        auto classValue = std::make_shared<ClassValue>(klass->name, superClass, std::move(methods));
        currentEnv->assign(klass->name, Value::fromHeap(classValue));
        return Value::fromHeap(classValue);
    }

    throw std::runtime_error("unsupported statement");
}

Value Evaluator::evaluateExpr(const ExprPtr& expr) {
    if (auto literal = std::dynamic_pointer_cast<LiteralExpr>(expr)) {
        if (std::holds_alternative<std::monostate>(literal->value)) {
            return Value();
        }
        if (std::holds_alternative<double>(literal->value)) {
            return Value(std::get<double>(literal->value));
        }
        if (std::holds_alternative<std::string>(literal->value)) {
            return Value(std::get<std::string>(literal->value));
        }
        return Value(std::get<bool>(literal->value));
    }
    if (auto variable = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        return currentEnv->get(variable->name);
    }
    if (std::dynamic_pointer_cast<ThisExpr>(expr)) {
        return currentEnv->get("this");
    }
    if (auto array = std::dynamic_pointer_cast<ArrayExpr>(expr)) {
        std::vector<Value> elements;
        elements.reserve(array->elements.size());
        for (const auto& element : array->elements) {
            elements.push_back(evaluateExpr(element));
        }
        return Value::array(std::move(elements));
    }
    if (auto object = std::dynamic_pointer_cast<ObjectExpr>(expr)) {
        std::map<std::string, Value> properties;
        for (const auto& [key, value] : object->properties) {
            properties[key] = evaluateExpr(value);
        }
        return Value::object(std::move(properties));
    }
    if (auto member = std::dynamic_pointer_cast<MemberExpr>(expr)) {
        return getMember(evaluateExpr(member->object), member->property);
    }
    if (auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr)) {
        const Value operand = evaluateExpr(unary->operand);
        switch (unary->op) {
            case TokenType::NOT:
                return Value(!operand.isTruthy());
            case TokenType::MINUS:
                return Value(-operand.toNumber());
            default:
                break;
        }
        throw std::runtime_error("unsupported unary operator");
    }
    if (auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        if (binary->op == TokenType::AND) {
            const Value left = evaluateExpr(binary->left);
            return left.isTruthy() ? evaluateExpr(binary->right) : left;
        }
        if (binary->op == TokenType::OR) {
            const Value left = evaluateExpr(binary->left);
            return left.isTruthy() ? left : evaluateExpr(binary->right);
        }
        if (binary->op == TokenType::PIPE) {
            const Value left = evaluateExpr(binary->left);
            if (auto call = std::dynamic_pointer_cast<CallExpr>(binary->right)) {
                std::vector<Value> arguments;
                arguments.push_back(left);
                for (const auto& argument : call->arguments) {
                    arguments.push_back(evaluateExpr(argument));
                }
                return callValue(evaluateExpr(call->callee), arguments);
            }
            return callValue(evaluateExpr(binary->right), {left});
        }

        const Value left = evaluateExpr(binary->left);
        const Value right = evaluateExpr(binary->right);
        switch (binary->op) {
            case TokenType::PLUS:
                return left.add(right);
            case TokenType::MINUS:
                return left.subtract(right);
            case TokenType::STAR:
                return left.multiply(right);
            case TokenType::SLASH:
                return left.divide(right);
            case TokenType::PERCENT:
                return left.modulo(right);
            case TokenType::EQ:
                return Value(left.equals(right));
            case TokenType::NEQ:
                return Value(!left.equals(right));
            case TokenType::LT:
                return Value(left.lessThan(right));
            case TokenType::LTE:
                return Value(left.lessThanOrEqual(right));
            case TokenType::GT:
                return Value(left.greaterThan(right));
            case TokenType::GTE:
                return Value(left.greaterThanOrEqual(right));
            default:
                break;
        }
        throw std::runtime_error("unsupported binary operator");
    }
    if (auto assignment = std::dynamic_pointer_cast<AssignmentExpr>(expr)) {
        const Value rhs = evaluateExpr(assignment->value);
        return assignTarget(assignment->target, assignment->op, rhs);
    }
    if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        std::vector<Value> arguments;
        arguments.reserve(call->arguments.size());
        for (const auto& argument : call->arguments) {
            arguments.push_back(evaluateExpr(argument));
        }
        return callValue(evaluateExpr(call->callee), arguments);
    }
    if (auto function = std::dynamic_pointer_cast<FunctionExpr>(expr)) {
        if (function->implicitReturn) {
            return Value::fromHeap(
                std::make_shared<FunctionValue>("", function->parameters, function->expressionBody, currentEnv));
        }
        return Value::fromHeap(std::make_shared<FunctionValue>("", function->parameters, function->body, currentEnv));
    }
    if (auto ctor = std::dynamic_pointer_cast<NewExpr>(expr)) {
        std::vector<Value> arguments;
        for (const auto& argument : ctor->arguments) {
            arguments.push_back(evaluateExpr(argument));
        }
        Value callee = evaluateExpr(ctor->callee);
        if (!callee.as<ClassValue>()) {
            throw std::runtime_error("new expects a class");
        }
        return callValue(callee, arguments);
    }
    if (auto interpolation = std::dynamic_pointer_cast<StringInterpolationExpr>(expr)) {
        std::string value;
        for (const auto& part : interpolation->parts) {
            if (std::holds_alternative<std::string>(part)) {
                value += std::get<std::string>(part);
            } else {
                value += evaluateExpr(std::get<ExprPtr>(part)).toString();
            }
        }
        return Value(value);
    }

    throw std::runtime_error("unsupported expression");
}

Value Evaluator::callValue(const Value& callee, const std::vector<Value>& arguments) {
    if (auto native = callee.as<NativeFunctionValue>()) {
        return native->function(*this, arguments);
    }

    if (auto function = callee.as<FunctionValue>()) {
        auto environment = std::make_shared<Environment>(function->closure);
        for (std::size_t i = 0; i < function->parameters.size(); ++i) {
            const Value argument = i < arguments.size() ? arguments[i] : Value();
            environment->define(function->parameters[i].name, argument);
        }

        try {
            if (function->implicitReturn) {
                Value result;
                const auto previous = currentEnv;
                currentEnv = environment;
                try {
                    result = evaluateExpr(function->expressionBody);
                } catch (...) {
                    currentEnv = previous;
                    throw;
                }
                currentEnv = previous;
                return result;
            }
            executeBlock(function->body, environment);
        } catch (const ReturnSignal& signal) {
            if (function->isInitializer && function->closure->has("this")) {
                return function->closure->get("this");
            }
            return signal.value;
        }

        if (function->isInitializer && function->closure->has("this")) {
            return function->closure->get("this");
        }
        return Value();
    }

    if (auto klass = callee.as<ClassValue>()) {
        auto instance = std::make_shared<InstanceValue>(klass);
        Value instanceValue = Value::fromHeap(instance);
        if (auto initializer = klass->findMethod("__init__")) {
            callValue(Value::fromHeap(bindFunction(initializer, instanceValue)), arguments);
        }
        return instanceValue;
    }

    throw std::runtime_error("value is not callable");
}

std::shared_ptr<Environment> Evaluator::globals() const {
    return globalEnv;
}

void Evaluator::registerBuiltins() {
    globalEnv->define(
        "print",
        Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "print",
            [this](Evaluator&, const std::vector<Value>& args) -> Value {
                for (std::size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) {
                        *output << " ";
                    }
                    *output << args[i].toString();
                }
                *output << '\n';
                return Value();
            })));

    globalEnv->define(
        "len",
        Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "len",
            [](Evaluator&, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    return Value(0.0);
                }
                if (auto array = args[0].as<ArrayValue>()) {
                    return Value(static_cast<double>(array->elements.size()));
                }
                if (auto object = args[0].as<ObjectValue>()) {
                    return Value(static_cast<double>(object->properties.size()));
                }
                if (args[0].isString()) {
                    return Value(static_cast<double>(args[0].asString().size()));
                }
                return Value(0.0);
            })));

    globalEnv->define(
        "type",
        Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "type",
            [](Evaluator&, const std::vector<Value>& args) -> Value {
                return Value(args.empty() ? "null" : args[0].typeName());
            })));

    globalEnv->define(
        "input",
        Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "input",
            [this](Evaluator&, const std::vector<Value>& args) -> Value {
                if (!args.empty()) {
                    *output << args[0].toString();
                }
                std::string line;
                std::getline(*input, line);
                return Value(line);
            })));
}

Value Evaluator::executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> environment) {
    const auto previous = currentEnv;
    currentEnv = std::move(environment);
    Value last;
    try {
        for (const auto& statement : statements) {
            last = evaluate(statement);
        }
    } catch (...) {
        currentEnv = previous;
        throw;
    }
    currentEnv = previous;
    return last;
}

Value Evaluator::assignTarget(const ExprPtr& target, TokenType op, const Value& rhs) {
    if (auto variable = std::dynamic_pointer_cast<VariableExpr>(target)) {
        Value value = rhs;
        if (op != TokenType::ASSIGN) {
            Value current = currentEnv->get(variable->name);
            value = op == TokenType::PLUS_ASSIGN ? current.add(rhs) : current.subtract(rhs);
        }
        currentEnv->assign(variable->name, value);
        return value;
    }

    if (auto member = std::dynamic_pointer_cast<MemberExpr>(target)) {
        Value object = evaluateExpr(member->object);
        Value value = rhs;
        if (op != TokenType::ASSIGN) {
            Value current = getMember(object, member->property);
            value = op == TokenType::PLUS_ASSIGN ? current.add(rhs) : current.subtract(rhs);
        }
        if (auto plainObject = object.as<ObjectValue>()) {
            plainObject->properties[member->property] = value;
            return value;
        }
        if (auto instance = object.as<InstanceValue>()) {
            instance->fields[member->property] = value;
            return value;
        }
        throw std::runtime_error("member assignment requires an object");
    }

    throw std::runtime_error("invalid assignment target");
}

Value Evaluator::getMember(const Value& object, const std::string& property) {
    if (auto plainObject = object.as<ObjectValue>()) {
        const auto it = plainObject->properties.find(property);
        if (it == plainObject->properties.end()) {
            throw std::runtime_error("unknown property: " + property);
        }
        return it->second;
    }

    if (auto instance = object.as<InstanceValue>()) {
        const auto field = instance->fields.find(property);
        if (field != instance->fields.end()) {
            return field->second;
        }
        if (auto method = instance->klass->findMethod(property)) {
            return Value::fromHeap(bindFunction(method, object));
        }
        throw std::runtime_error("unknown property: " + property);
    }

    if (auto array = object.as<ArrayValue>()) {
        if (property == "length") {
            return Value(static_cast<double>(array->elements.size()));
        }
        return makeArrayMethod(object, property);
    }

    if (object.isString()) {
        if (property == "length") {
            return Value(static_cast<double>(object.asString().size()));
        }
        return makeStringMethod(object, property);
    }

    throw std::runtime_error("value has no properties");
}

Value Evaluator::makeArrayMethod(const Value& object, const std::string& property) {
    auto array = object.as<ArrayValue>();
    if (property == "push") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "push", [array](Evaluator&, const std::vector<Value>& args) -> Value {
                if (!args.empty()) {
                    array->elements.push_back(args[0]);
                }
                return Value(static_cast<double>(array->elements.size()));
            }));
    }
    if (property == "pop") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "pop", [array](Evaluator&, const std::vector<Value>&) -> Value {
                if (array->elements.empty()) {
                    return Value();
                }
                Value last = array->elements.back();
                array->elements.pop_back();
                return last;
            }));
    }
    if (property == "map") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "map", [array](Evaluator& evaluator, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    throw std::runtime_error("map expects a callback");
                }
                std::vector<Value> result;
                for (const auto& element : array->elements) {
                    result.push_back(evaluator.callValue(args[0], {element}));
                }
                return Value::array(std::move(result));
            }));
    }
    if (property == "filter") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "filter", [array](Evaluator& evaluator, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    throw std::runtime_error("filter expects a callback");
                }
                std::vector<Value> result;
                for (const auto& element : array->elements) {
                    if (evaluator.callValue(args[0], {element}).isTruthy()) {
                        result.push_back(element);
                    }
                }
                return Value::array(std::move(result));
            }));
    }
    if (property == "forEach") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "forEach", [array](Evaluator& evaluator, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    throw std::runtime_error("forEach expects a callback");
                }
                for (const auto& element : array->elements) {
                    evaluator.callValue(args[0], {element});
                }
                return Value();
            }));
    }
    throw std::runtime_error("unknown array method: " + property);
}

Value Evaluator::makeStringMethod(const Value& object, const std::string& property) {
    const std::string value = object.asString();
    if (property == "substring") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "substring", [value](Evaluator&, const std::vector<Value>& args) -> Value {
                const std::size_t start = static_cast<std::size_t>(
                    clampIndex(args.empty() ? 0.0 : args[0].toNumber(), value.size()));
                const std::size_t end = static_cast<std::size_t>(
                    clampIndex(args.size() > 1 ? args[1].toNumber() : static_cast<double>(value.size()), value.size()));
                return Value(value.substr(start, end >= start ? end - start : 0));
            }));
    }
    if (property == "toUpperCase") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "toUpperCase", [value](Evaluator&, const std::vector<Value>&) -> Value {
                std::string upper = value;
                std::transform(upper.begin(), upper.end(), upper.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
                return Value(upper);
            }));
    }
    if (property == "split") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "split", [value](Evaluator&, const std::vector<Value>& args) -> Value {
                const std::string delimiter = args.empty() ? "" : args[0].toString();
                std::vector<Value> pieces;
                if (delimiter.empty()) {
                    for (char ch : value) {
                        pieces.emplace_back(std::string(1, ch));
                    }
                } else {
                    std::size_t start = 0;
                    while (true) {
                        const std::size_t next = value.find(delimiter, start);
                        if (next == std::string::npos) {
                            pieces.emplace_back(value.substr(start));
                            break;
                        }
                        pieces.emplace_back(value.substr(start, next - start));
                        start = next + delimiter.size();
                    }
                }
                return Value::array(std::move(pieces));
            }));
    }
    throw std::runtime_error("unknown string method: " + property);
}

std::shared_ptr<FunctionValue> Evaluator::bindFunction(const std::shared_ptr<FunctionValue>& fn, const Value& self) {
    auto environment = std::make_shared<Environment>(fn->closure);
    environment->define("this", self);
    std::shared_ptr<FunctionValue> bound;
    if (fn->implicitReturn) {
        bound = std::make_shared<FunctionValue>(fn->name, fn->parameters, fn->expressionBody, environment);
    } else {
        bound = std::make_shared<FunctionValue>(fn->name, fn->parameters, fn->body, environment);
    }
    bound->isInitializer = fn->isInitializer;
    return bound;
}
