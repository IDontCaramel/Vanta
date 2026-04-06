#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ast.h"

class Evaluator;
class Environment;

struct HeapValue {
    virtual ~HeapValue() = default;
    virtual std::string inspect() const = 0;
};

class Value {
public:
    enum class Type {
        NULL_,
        NUMBER,
        STRING,
        BOOLEAN,
        HEAP
    };

    Value();
    explicit Value(double number);
    explicit Value(std::string text);
    explicit Value(const char* text);
    explicit Value(bool boolean);
    explicit Value(std::shared_ptr<HeapValue> heapValue);

    static Value array(std::vector<Value> values);
    static Value object(std::map<std::string, Value> values);

    template <typename T>
    static Value fromHeap(std::shared_ptr<T> ptr) {
        return Value(std::static_pointer_cast<HeapValue>(std::move(ptr)));
    }

    Type type() const;
    bool isNull() const;
    bool isNumber() const;
    bool isString() const;
    bool isBoolean() const;
    bool isHeap() const;

    double asNumber() const;
    const std::string& asString() const;
    bool asBoolean() const;
    std::shared_ptr<HeapValue> asHeap() const;

    template <typename T>
    std::shared_ptr<T> as() const {
        if (!isHeap()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<T>(std::get<std::shared_ptr<HeapValue>>(storage));
    }

    std::string toString() const;
    std::string typeName() const;
    bool isTruthy() const;
    double toNumber() const;

    Value add(const Value& other) const;
    Value subtract(const Value& other) const;
    Value multiply(const Value& other) const;
    Value divide(const Value& other) const;
    Value modulo(const Value& other) const;

    bool equals(const Value& other) const;
    bool lessThan(const Value& other) const;
    bool lessThanOrEqual(const Value& other) const;
    bool greaterThan(const Value& other) const;
    bool greaterThanOrEqual(const Value& other) const;

private:
    using Storage = std::variant<std::monostate, double, std::string, bool, std::shared_ptr<HeapValue>>;

    Storage storage;
};

struct ArrayValue final : HeapValue {
    std::vector<Value> elements;

    explicit ArrayValue(std::vector<Value> items) : elements(std::move(items)) {}
    std::string inspect() const override;
};

struct ObjectValue final : HeapValue {
    std::map<std::string, Value> properties;

    explicit ObjectValue(std::map<std::string, Value> props) : properties(std::move(props)) {}
    std::string inspect() const override;
};

struct FunctionValue final : HeapValue {
    std::string name;
    std::vector<Parameter> parameters;
    std::vector<StmtPtr> body;
    ExprPtr expressionBody;
    bool implicitReturn = false;
    bool isInitializer = false;
    std::shared_ptr<Environment> closure;

    FunctionValue(std::string fnName, std::vector<Parameter> params, std::vector<StmtPtr> stmts,
                  std::shared_ptr<Environment> env)
        : name(std::move(fnName)),
          parameters(std::move(params)),
          body(std::move(stmts)),
          closure(std::move(env)) {}

    FunctionValue(std::string fnName, std::vector<Parameter> params, ExprPtr exprBody,
                  std::shared_ptr<Environment> env)
        : name(std::move(fnName)),
          parameters(std::move(params)),
          expressionBody(std::move(exprBody)),
          implicitReturn(true),
          closure(std::move(env)) {}

    std::string inspect() const override;
};

struct NativeFunctionValue final : HeapValue {
    std::string name;
    std::function<Value(Evaluator&, const std::vector<Value>&)> function;

    NativeFunctionValue(std::string fnName, std::function<Value(Evaluator&, const std::vector<Value>&)> fn)
        : name(std::move(fnName)), function(std::move(fn)) {}

    std::string inspect() const override;
};

struct ClassValue;

struct InstanceValue final : HeapValue {
    std::shared_ptr<ClassValue> klass;
    std::map<std::string, Value> fields;

    explicit InstanceValue(std::shared_ptr<ClassValue> classRef) : klass(std::move(classRef)) {}
    std::string inspect() const override;
};

struct ClassValue final : HeapValue {
    std::string name;
    std::shared_ptr<ClassValue> superClass;
    std::map<std::string, std::shared_ptr<FunctionValue>> methods;

    ClassValue(std::string className, std::shared_ptr<ClassValue> parent,
               std::map<std::string, std::shared_ptr<FunctionValue>> classMethods)
        : name(std::move(className)),
          superClass(std::move(parent)),
          methods(std::move(classMethods)) {}

    std::shared_ptr<FunctionValue> findMethod(const std::string& methodName) const;
    std::string inspect() const override;
};
