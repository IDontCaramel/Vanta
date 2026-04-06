#include "evaluator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include "diagnostic.h"

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

std::size_t requireIndex(const Value& index, std::size_t size, const std::string& targetType, bool allowEnd = false) {
    if (!index.isNumber()) {
        throw std::runtime_error(targetType + " index must be a number, got " + index.typeName());
    }

    const double raw = index.asNumber();
    if (!std::isfinite(raw) || std::floor(raw) != raw) {
        throw std::runtime_error(targetType + " index must be an integer, got " + index.toString());
    }

    if (raw < 0 || (!allowEnd && raw >= static_cast<double>(size)) || (allowEnd && raw > static_cast<double>(size))) {
        throw std::runtime_error(targetType + " index out of bounds: " + index.toString() + " (size " +
                                 std::to_string(size) + ")");
    }

    return static_cast<std::size_t>(raw);
}

}  // namespace

Evaluator::Evaluator(std::ostream& out, std::istream& in, std::shared_ptr<const SourceDocument> sourceDocument)
    : output(&out),
      input(&in),
      source(std::move(sourceDocument)),
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
                    runtimeError(declaration->sourceRange, "array destructuring requires an array");
                }
                for (std::size_t i = 0; i < declaration->patternNames.size(); ++i) {
                    const Value element = i < array->elements.size() ? array->elements[i] : Value();
                    currentEnv->define(declaration->patternNames[i], element);
                }
            } else {
                auto object = initializer.as<ObjectValue>();
                auto instance = initializer.as<InstanceValue>();
                if (!object && !instance) {
                    runtimeError(declaration->sourceRange, "object destructuring requires an object");
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
        try {
            currentEnv->define(declaration->name, initializer, declaration->typeHint,
                               declaration->initializer != nullptr);
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error& error) {
            runtimeError(declaration->sourceRange, error.what());
        }
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
        throw ThrowSignal{evaluateExpr(throwing->value), throwing->sourceRange};
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
        auto value = std::make_shared<FunctionValue>(function->name, function->parameters, function->body, currentEnv,
                                                     function->returnType);
        if (function->name == "__init__") {
            value->isInitializer = true;
        }
        currentEnv->define(function->name, Value::fromHeap(value));
        return Value::fromHeap(value);
    }
    if (auto klass = std::dynamic_pointer_cast<ClassDeclStmt>(stmt)) {
        std::shared_ptr<ClassValue> superClass = nullptr;
        if (!klass->parentName.empty()) {
            Value parent;
            try {
                parent = currentEnv->get(klass->parentName);
            } catch (const RuntimeError&) {
                throw;
            } catch (const std::runtime_error&) {
                runtimeError(klass->sourceRange, "undefined variable '" + klass->parentName + "'");
            }
            superClass = parent.as<ClassValue>();
            if (!superClass) {
                runtimeError(klass->sourceRange, "parent class must be a class");
            }
        }

        currentEnv->define(klass->name, Value());
        std::map<std::string, std::shared_ptr<FunctionValue>> methods;
        for (const auto& method : klass->methods) {
            auto fn = std::make_shared<FunctionValue>(method->name, method->parameters, method->body, currentEnv,
                                                      method->returnType);
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
        try {
            return currentEnv->get(variable->name);
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error&) {
            runtimeError(variable->sourceRange, "undefined variable '" + variable->name + "'");
        }
    }
    if (std::dynamic_pointer_cast<ThisExpr>(expr)) {
        try {
            return currentEnv->get("this");
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error&) {
            runtimeError(expr->sourceRange, "cannot use 'this' outside of an instance method");
        }
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
        return getMember(evaluateExpr(member->object), member->property, member->sourceRange);
    }
    if (auto index = std::dynamic_pointer_cast<IndexExpr>(expr)) {
        return getIndex(evaluateExpr(index->object), evaluateExpr(index->index), index->sourceRange);
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
                return callValue(evaluateExpr(call->callee), arguments, call);
            }
            return callValue(evaluateExpr(binary->right), {left}, binary->right);
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
                try {
                    return left.divide(right);
                } catch (const std::runtime_error& error) {
                    runtimeError(binary->sourceRange, error.what());
                }
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
        return assignTarget(assignment->target, assignment->op, rhs, assignment->sourceRange);
    }
    if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        std::vector<Value> arguments;
        arguments.reserve(call->arguments.size());
        for (const auto& argument : call->arguments) {
            arguments.push_back(evaluateExpr(argument));
        }
        return callValue(evaluateExpr(call->callee), arguments, call);
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
            runtimeError(ctor->sourceRange, "cannot instantiate " + callee.typeName() + "; 'new' expects a class");
        }
        return callValue(callee, arguments, ctor);
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

Value Evaluator::callValue(const Value& callee, const std::vector<Value>& arguments, const ExprPtr& callSite) {
    if (auto native = callee.as<NativeFunctionValue>()) {
        try {
            return native->function(*this, arguments);
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error& error) {
            if (callSite) {
                runtimeError(callSite->sourceRange, error.what());
            }
            throw;
        }
    }

    if (auto function = callee.as<FunctionValue>()) {
        auto environment = std::make_shared<Environment>(function->closure);
        try {
            for (std::size_t i = 0; i < function->parameters.size(); ++i) {
                const Value argument = i < arguments.size() ? arguments[i] : Value();
                const auto& parameter = function->parameters[i];
                enforceTypeHint(argument, parameter.typeHint, "parameter '" + parameter.name + "'");
                environment->define(parameter.name, argument, parameter.typeHint);
            }
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error& error) {
            if (callSite) {
                runtimeError(callSite->sourceRange, error.what());
            }
            throw;
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
                try {
                    enforceTypeHint(result, function->returnType, "return value");
                } catch (const std::runtime_error& error) {
                    if (callSite) {
                        runtimeError(callSite->sourceRange, error.what());
                    }
                    throw;
                }
                return result;
            }
            executeBlock(function->body, environment);
        } catch (const ReturnSignal& signal) {
            Value result = signal.value;
            if (function->isInitializer && function->closure->has("this")) {
                result = function->closure->get("this");
            }
            try {
                enforceTypeHint(result, function->returnType, "return value");
            } catch (const std::runtime_error& error) {
                if (callSite) {
                    runtimeError(callSite->sourceRange, error.what());
                }
                throw;
            }
            return result;
        }

        Value result;
        if (function->isInitializer && function->closure->has("this")) {
            result = function->closure->get("this");
        }
        try {
            enforceTypeHint(result, function->returnType, "return value");
        } catch (const std::runtime_error& error) {
            if (callSite) {
                runtimeError(callSite->sourceRange, error.what());
            }
            throw;
        }
        return result;
    }

    if (auto klass = callee.as<ClassValue>()) {
        auto instance = std::make_shared<InstanceValue>(klass);
        Value instanceValue = Value::fromHeap(instance);
        if (auto initializer = klass->findMethod("__init__")) {
            callValue(Value::fromHeap(bindFunction(initializer, instanceValue)), arguments, callSite);
        }
        return instanceValue;
    }

    if (callSite) {
        runtimeError(callSite->sourceRange, "cannot call value of type " + callee.typeName());
    }
    throw std::runtime_error("value is not callable");
}

void Evaluator::setSourceDocument(std::shared_ptr<const SourceDocument> sourceDocument) {
    source = std::move(sourceDocument);
}

std::shared_ptr<Environment> Evaluator::globals() const {
    return globalEnv;
}

void Evaluator::runtimeError(const SourceRange& range, const std::string& message) const {
    if (source && range.isValid()) {
        throw RuntimeError(message, source, range);
    }
    throw std::runtime_error(message);
}

std::string Evaluator::describePropertyOwner(const Value& object) const {
    if (auto instance = object.as<InstanceValue>()) {
        return "instance of " + instance->klass->name;
    }
    return object.typeName();
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
        "keys",
        Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "keys",
            [](Evaluator&, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    throw std::runtime_error("keys expects an object");
                }

                std::vector<Value> result;
                if (auto object = args[0].as<ObjectValue>()) {
                    result.reserve(object->properties.size());
                    for (const auto& [key, value] : object->properties) {
                        (void)value;
                        result.emplace_back(key);
                    }
                    return Value::array(std::move(result));
                }
                if (auto instance = args[0].as<InstanceValue>()) {
                    result.reserve(instance->fields.size());
                    for (const auto& [key, value] : instance->fields) {
                        (void)value;
                        result.emplace_back(key);
                    }
                    return Value::array(std::move(result));
                }

                throw std::runtime_error("keys expects an object, got " + args[0].typeName());
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

Value Evaluator::assignTarget(const ExprPtr& target, TokenType op, const Value& rhs, const SourceRange& assignmentRange) {
    if (auto variable = std::dynamic_pointer_cast<VariableExpr>(target)) {
        Value value = rhs;
        if (op != TokenType::ASSIGN) {
            try {
                Value current = currentEnv->get(variable->name);
                value = op == TokenType::PLUS_ASSIGN ? current.add(rhs) : current.subtract(rhs);
            } catch (const RuntimeError&) {
                throw;
            } catch (const std::runtime_error&) {
                runtimeError(variable->sourceRange, "undefined variable '" + variable->name + "'");
            }
        }
        try {
            currentEnv->assign(variable->name, value);
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::runtime_error& error) {
            const std::string message = std::string(error.what()).rfind("undefined variable", 0) == 0
                                            ? "undefined variable '" + variable->name + "'"
                                            : error.what();
            runtimeError(variable->sourceRange, message);
        }
        return value;
    }

    if (auto member = std::dynamic_pointer_cast<MemberExpr>(target)) {
        Value object = evaluateExpr(member->object);
        Value value = rhs;
        if (op != TokenType::ASSIGN) {
            Value current = getMember(object, member->property, member->sourceRange);
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
        runtimeError(member->sourceRange,
                     "cannot assign property '" + member->property + "' on " + describePropertyOwner(object));
    }

    if (auto index = std::dynamic_pointer_cast<IndexExpr>(target)) {
        return assignIndex(evaluateExpr(index->object), evaluateExpr(index->index), op, rhs, index->sourceRange);
    }

    runtimeError(assignmentRange, "invalid assignment target; expected a variable, property access, or index access");
}

Value Evaluator::getMember(const Value& object, const std::string& property, const SourceRange& range) {
    if (auto plainObject = object.as<ObjectValue>()) {
        const auto it = plainObject->properties.find(property);
        if (it == plainObject->properties.end()) {
            runtimeError(range, "object has no property '" + property + "'");
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
        runtimeError(range, "instance of " + instance->klass->name + " has no property '" + property + "'");
    }

    if (auto array = object.as<ArrayValue>()) {
        if (property == "length") {
            return Value(static_cast<double>(array->elements.size()));
        }
        try {
            return makeArrayMethod(object, property);
        } catch (const std::runtime_error&) {
            runtimeError(range, "array has no property '" + property + "'");
        }
    }

    if (object.isString()) {
        if (property == "length") {
            return Value(static_cast<double>(object.asString().size()));
        }
        try {
            return makeStringMethod(object, property);
        } catch (const std::runtime_error&) {
            runtimeError(range, "string has no property '" + property + "'");
        }
    }

    runtimeError(range, "cannot access property '" + property + "' on " + describePropertyOwner(object));
}

Value Evaluator::getIndex(const Value& object, const Value& index, const SourceRange& range) {
    try {
        if (auto array = object.as<ArrayValue>()) {
            return array->elements[requireIndex(index, array->elements.size(), "array")];
        }

        if (object.isString()) {
            const std::string& value = object.asString();
            const std::size_t offset = requireIndex(index, value.size(), "string");
            return Value(std::string(1, value[offset]));
        }
    } catch (const std::runtime_error& error) {
        runtimeError(range, error.what());
    }

    runtimeError(range, "indexing requires an array or string, got " + object.typeName());
}

Value Evaluator::assignIndex(const Value& object, const Value& index, TokenType op, const Value& rhs,
                             const SourceRange& range) {
    if (auto array = object.as<ArrayValue>()) {
        std::size_t offset = 0;
        try {
            offset = requireIndex(index, array->elements.size(), "array");
        } catch (const std::runtime_error& error) {
            runtimeError(range, error.what());
        }
        Value value = rhs;
        if (op != TokenType::ASSIGN) {
            const Value current = array->elements[offset];
            value = op == TokenType::PLUS_ASSIGN ? current.add(rhs) : current.subtract(rhs);
        }
        array->elements[offset] = value;
        return value;
    }

    if (object.isString()) {
        runtimeError(range, "cannot assign through string indexing");
    }

    runtimeError(range, "cannot assign through index on " + object.typeName() + "; expected an array");
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
    if (property == "reduce") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "reduce", [array](Evaluator& evaluator, const std::vector<Value>& args) -> Value {
                if (args.empty()) {
                    throw std::runtime_error("reduce expects a callback");
                }
                if (array->elements.empty() && args.size() < 2) {
                    throw std::runtime_error("reduce requires an initial value for empty arrays");
                }

                std::size_t start = 0;
                Value accumulator;
                if (args.size() > 1) {
                    accumulator = args[1];
                } else {
                    accumulator = array->elements.front();
                    start = 1;
                }

                for (std::size_t i = start; i < array->elements.size(); ++i) {
                    accumulator = evaluator.callValue(args[0], {accumulator, array->elements[i], Value(static_cast<double>(i))});
                }
                return accumulator;
            }));
    }
    if (property == "slice") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "slice", [array](Evaluator&, const std::vector<Value>& args) -> Value {
                const std::size_t start = static_cast<std::size_t>(
                    clampIndex(args.empty() ? 0.0 : args[0].toNumber(), array->elements.size()));
                const std::size_t end = static_cast<std::size_t>(clampIndex(
                    args.size() > 1 ? args[1].toNumber() : static_cast<double>(array->elements.size()),
                    array->elements.size()));
                if (end < start) {
                    return Value::array({});
                }
                return Value::array(std::vector<Value>(array->elements.begin() + static_cast<std::ptrdiff_t>(start),
                                                       array->elements.begin() + static_cast<std::ptrdiff_t>(end)));
            }));
    }
    if (property == "join") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "join", [array](Evaluator&, const std::vector<Value>& args) -> Value {
                const std::string delimiter = args.empty() ? "" : args[0].toString();
                std::ostringstream joined;
                for (std::size_t i = 0; i < array->elements.size(); ++i) {
                    if (i > 0) {
                        joined << delimiter;
                    }
                    joined << array->elements[i].toString();
                }
                return Value(joined.str());
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
    if (property == "slice") {
        return Value::fromHeap(std::make_shared<NativeFunctionValue>(
            "slice", [value](Evaluator&, const std::vector<Value>& args) -> Value {
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
        bound = std::make_shared<FunctionValue>(fn->name, fn->parameters, fn->expressionBody, environment,
                                               fn->returnType);
    } else {
        bound = std::make_shared<FunctionValue>(fn->name, fn->parameters, fn->body, environment, fn->returnType);
    }
    bound->isInitializer = fn->isInitializer;
    return bound;
}

void Evaluator::enforceTypeHint(const Value& value, const std::string& typeHint,
                                const std::string& context) const {
    if (!Value::isSupportedTypeHint(typeHint)) {
        return;
    }
    if (!value.matchesTypeHint(typeHint)) {
        throw std::runtime_error("type mismatch for " + context + ": expected " + typeHint + ", got " +
                                 value.runtimeTypeLabel());
    }
}
