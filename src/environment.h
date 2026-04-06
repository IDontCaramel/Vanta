#pragma once

#include <map>
#include <memory>
#include <string>

#include "value.h"

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parentEnv = nullptr);

    void define(const std::string& name, const Value& value, const std::string& typeHint = {},
                bool validateValue = true);
    Value get(const std::string& name) const;
    void assign(const std::string& name, const Value& value);
    bool has(const std::string& name) const;

private:
    struct Binding {
        Value value;
        std::string typeHint;
    };

    std::map<std::string, Binding> values;
    std::shared_ptr<Environment> parent;
};
