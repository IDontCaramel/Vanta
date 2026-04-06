#include "environment.h"

#include <stdexcept>

Environment::Environment(std::shared_ptr<Environment> parentEnv) : parent(std::move(parentEnv)) {}

void Environment::define(const std::string& name, const Value& value, const std::string& typeHint,
                         bool validateValue) {
    if (validateValue && Value::isSupportedTypeHint(typeHint) && !value.matchesTypeHint(typeHint)) {
        throw std::runtime_error("type mismatch for variable '" + name + "': expected " + typeHint +
                                 ", got " + value.runtimeTypeLabel());
    }
    values[name] = Binding{value, typeHint};
}

Value Environment::get(const std::string& name) const {
    const auto it = values.find(name);
    if (it != values.end()) {
        return it->second.value;
    }
    if (parent) {
        return parent->get(name);
    }
    throw std::runtime_error("undefined variable: " + name);
}

void Environment::assign(const std::string& name, const Value& value) {
    const auto it = values.find(name);
    if (it != values.end()) {
        if (Value::isSupportedTypeHint(it->second.typeHint) && !value.matchesTypeHint(it->second.typeHint)) {
            throw std::runtime_error("type mismatch for variable '" + name + "': expected " +
                                     it->second.typeHint + ", got " + value.runtimeTypeLabel());
        }
        it->second.value = value;
        return;
    }
    if (parent) {
        parent->assign(name, value);
        return;
    }
    throw std::runtime_error("undefined variable: " + name);
}

bool Environment::has(const std::string& name) const {
    if (values.count(name) > 0) {
        return true;
    }
    return parent ? parent->has(name) : false;
}
