#include "nextviper/environment.hpp"

namespace nextviper {

void Environment::define(const std::string& name, Value value, bool is_mut, std::string type_annotation) {
    bindings_[name] = VariableBinding{std::move(value), is_mut, std::move(type_annotation)};
}

bool Environment::assign(const std::string& name, Value value, std::string& error_msg) {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        if (!it->second.is_mut) {
            error_msg = "cannot reassign to immutable variable '" + name + "' (use 'let mut " + name + "' to make it mutable)";
            return false;
        }
        it->second.value = std::move(value);
        return true;
    }

    if (enclosing_) {
        return enclosing_->assign(name, std::move(value), error_msg);
    }

    // Auto-declare global variable on first direct assignment
    bindings_[name] = VariableBinding{std::move(value), true, ""};
    return true;
}

std::optional<Value> Environment::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second.value;
    }

    if (enclosing_) {
        return enclosing_->get(name);
    }

    return std::nullopt;
}

bool Environment::contains(const std::string& name) const {
    if (bindings_.find(name) != bindings_.end()) {
        return true;
    }
    if (enclosing_) {
        return enclosing_->contains(name);
    }
    return false;
}

bool Environment::is_mutable(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second.is_mut;
    }
    if (enclosing_) {
        return enclosing_->is_mutable(name);
    }
    return false;
}

} // namespace nextviper
