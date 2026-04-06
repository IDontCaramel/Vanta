#include "value.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace {

std::string trimNumber(double value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

}  // namespace

Value::Value() : storage(std::monostate{}) {}

Value::Value(double number) : storage(number) {}

Value::Value(std::string text) : storage(std::move(text)) {}

Value::Value(const char* text) : storage(std::string(text)) {}

Value::Value(bool boolean) : storage(boolean) {}

Value::Value(std::shared_ptr<HeapValue> heapValue) : storage(std::move(heapValue)) {}

Value Value::array(std::vector<Value> values) {
    return fromHeap(std::make_shared<ArrayValue>(std::move(values)));
}

Value Value::object(std::map<std::string, Value> values) {
    return fromHeap(std::make_shared<ObjectValue>(std::move(values)));
}

Value::Type Value::type() const {
    if (std::holds_alternative<std::monostate>(storage)) {
        return Type::NULL_;
    }
    if (std::holds_alternative<double>(storage)) {
        return Type::NUMBER;
    }
    if (std::holds_alternative<std::string>(storage)) {
        return Type::STRING;
    }
    if (std::holds_alternative<bool>(storage)) {
        return Type::BOOLEAN;
    }
    return Type::HEAP;
}

bool Value::isNull() const {
    return type() == Type::NULL_;
}

bool Value::isNumber() const {
    return type() == Type::NUMBER;
}

bool Value::isString() const {
    return type() == Type::STRING;
}

bool Value::isBoolean() const {
    return type() == Type::BOOLEAN;
}

bool Value::isHeap() const {
    return type() == Type::HEAP;
}

double Value::asNumber() const {
    return std::get<double>(storage);
}

const std::string& Value::asString() const {
    return std::get<std::string>(storage);
}

bool Value::asBoolean() const {
    return std::get<bool>(storage);
}

std::shared_ptr<HeapValue> Value::asHeap() const {
    return std::get<std::shared_ptr<HeapValue>>(storage);
}

std::string Value::toString() const {
    switch (type()) {
        case Type::NULL_:
            return "null";
        case Type::NUMBER:
            return trimNumber(asNumber());
        case Type::STRING:
            return asString();
        case Type::BOOLEAN:
            return asBoolean() ? "true" : "false";
        case Type::HEAP:
            return asHeap()->inspect();
    }
    return "";
}

std::string Value::typeName() const {
    switch (type()) {
        case Type::NULL_:
            return "null";
        case Type::NUMBER:
            return "number";
        case Type::STRING:
            return "string";
        case Type::BOOLEAN:
            return "boolean";
        case Type::HEAP:
            if (as<ArrayValue>()) {
                return "array";
            }
            if (as<ObjectValue>()) {
                return "object";
            }
            if (as<FunctionValue>()) {
                return "function";
            }
            if (as<NativeFunctionValue>()) {
                return "native_function";
            }
            if (as<ClassValue>()) {
                return "class";
            }
            if (as<InstanceValue>()) {
                return "instance";
            }
            return "heap";
    }
    return "unknown";
}

bool Value::isTruthy() const {
    switch (type()) {
        case Type::NULL_:
            return false;
        case Type::NUMBER:
            return asNumber() != 0.0;
        case Type::STRING:
            return !asString().empty();
        case Type::BOOLEAN:
            return asBoolean();
        case Type::HEAP:
            return true;
    }
    return false;
}

double Value::toNumber() const {
    switch (type()) {
        case Type::NUMBER:
            return asNumber();
        case Type::STRING:
            try {
                return std::stod(asString());
            } catch (...) {
                return 0.0;
            }
        case Type::BOOLEAN:
            return asBoolean() ? 1.0 : 0.0;
        case Type::NULL_:
        case Type::HEAP:
            return 0.0;
    }
    return 0.0;
}

Value Value::add(const Value& other) const {
    if (isString() || other.isString()) {
        return Value(toString() + other.toString());
    }
    return Value(toNumber() + other.toNumber());
}

Value Value::subtract(const Value& other) const {
    return Value(toNumber() - other.toNumber());
}

Value Value::multiply(const Value& other) const {
    return Value(toNumber() * other.toNumber());
}

Value Value::divide(const Value& other) const {
    const double divisor = other.toNumber();
    if (divisor == 0.0) {
        throw std::runtime_error("division by zero");
    }
    return Value(toNumber() / divisor);
}

Value Value::modulo(const Value& other) const {
    return Value(std::fmod(toNumber(), other.toNumber()));
}

bool Value::equals(const Value& other) const {
    if (type() != other.type()) {
        return false;
    }

    switch (type()) {
        case Type::NULL_:
            return true;
        case Type::NUMBER:
            return asNumber() == other.asNumber();
        case Type::STRING:
            return asString() == other.asString();
        case Type::BOOLEAN:
            return asBoolean() == other.asBoolean();
        case Type::HEAP:
            return asHeap() == other.asHeap();
    }
    return false;
}

bool Value::lessThan(const Value& other) const {
    if (isString() && other.isString()) {
        return asString() < other.asString();
    }
    return toNumber() < other.toNumber();
}

bool Value::lessThanOrEqual(const Value& other) const {
    return lessThan(other) || equals(other);
}

bool Value::greaterThan(const Value& other) const {
    if (isString() && other.isString()) {
        return asString() > other.asString();
    }
    return toNumber() > other.toNumber();
}

bool Value::greaterThanOrEqual(const Value& other) const {
    return greaterThan(other) || equals(other);
}

std::string ArrayValue::inspect() const {
    std::ostringstream stream;
    stream << "[";
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) {
            stream << ", ";
        }
        stream << elements[i].toString();
    }
    stream << "]";
    return stream.str();
}

std::string ObjectValue::inspect() const {
    std::ostringstream stream;
    stream << "{";
    std::size_t index = 0;
    for (const auto& [key, value] : properties) {
        if (index++ > 0) {
            stream << ", ";
        }
        stream << key << ": " << value.toString();
    }
    stream << "}";
    return stream.str();
}

std::string FunctionValue::inspect() const {
    return name.empty() ? "[function]" : "[function " + name + "]";
}

std::string NativeFunctionValue::inspect() const {
    return "[native function " + name + "]";
}

std::string InstanceValue::inspect() const {
    return "[instance " + klass->name + "]";
}

std::shared_ptr<FunctionValue> ClassValue::findMethod(const std::string& methodName) const {
    const auto it = methods.find(methodName);
    if (it != methods.end()) {
        return it->second;
    }
    if (superClass) {
        return superClass->findMethod(methodName);
    }
    return nullptr;
}

std::string ClassValue::inspect() const {
    return "[class " + name + "]";
}
