#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <stdexcept>

namespace nextviper {

struct VariableBinding {
    Value value;
    bool is_mut = false;
    std::string type_annotation;
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment() : enclosing_(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> enclosing)
        : enclosing_(std::move(enclosing)) {}

    static std::shared_ptr<Environment> create(std::shared_ptr<Environment> enclosing = nullptr) {
        return std::make_shared<Environment>(std::move(enclosing));
    }

    void define(const std::string& name, Value value, bool is_mut = false, std::string type_annotation = "");
    bool assign(const std::string& name, Value value, std::string& error_msg);
    std::optional<Value> get(const std::string& name) const;
    bool contains(const std::string& name) const;
    bool is_mutable(const std::string& name) const;

    std::shared_ptr<Environment> enclosing() const { return enclosing_; }
    const std::unordered_map<std::string, VariableBinding>& bindings() const { return bindings_; }

private:
    std::shared_ptr<Environment> enclosing_;
    std::unordered_map<std::string, VariableBinding> bindings_;
};

} // namespace nextviper
