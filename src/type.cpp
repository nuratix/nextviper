#include "nextviper/type.hpp"
#include <cctype>

namespace nextviper {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

TypePtr Type::make_any() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::ANY);
    return instance;
}

TypePtr Type::make_int() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::INT);
    return instance;
}

TypePtr Type::make_float() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::FLOAT);
    return instance;
}

TypePtr Type::make_string() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::STRING);
    return instance;
}

TypePtr Type::make_bool() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::BOOL);
    return instance;
}

TypePtr Type::make_null() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::NULL_TYPE);
    return instance;
}

TypePtr Type::make_void() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::VOID);
    return instance;
}

TypePtr Type::make_unknown() {
    static TypePtr instance = std::make_shared<Type>(TypeKind::UNKNOWN);
    return instance;
}

TypePtr Type::make_list(TypePtr element_type) {
    auto t = std::make_shared<Type>(TypeKind::LIST);
    t->element_type_ = element_type ? element_type : make_any();
    return t;
}

TypePtr Type::make_map(TypePtr key_type, TypePtr value_type) {
    auto t = std::make_shared<Type>(TypeKind::MAP);
    t->key_type_ = key_type ? key_type : make_string();
    t->value_type_ = value_type ? value_type : make_any();
    return t;
}

TypePtr Type::make_function(std::vector<TypePtr> param_types, TypePtr return_type) {
    auto t = std::make_shared<Type>(TypeKind::FUNCTION);
    t->param_types_ = std::move(param_types);
    t->return_type_ = return_type ? return_type : make_void();
    return t;
}

TypePtr Type::make_tensor(TypePtr element_type, std::vector<int64_t> shape) {
    auto t = std::make_shared<Type>(TypeKind::TENSOR);
    t->element_type_ = element_type ? element_type : make_float();
    t->tensor_shape_ = std::move(shape);
    return t;
}

TypePtr Type::make_custom(std::string name) {
    return std::make_shared<Type>(TypeKind::CUSTOM, std::move(name));
}

TypePtr Type::make_type_var(std::string name) {
    return std::make_shared<Type>(TypeKind::TYPE_VAR, std::move(name));
}

TypePtr Type::parse(const std::string& input) {
    std::string s = trim(input);
    if (s.empty() || s == "any" || s == "Any") return make_any();
    if (s == "int" || s == "Int" || s == "i64" || s == "i32" || s == "int64") return make_int();
    if (s == "float" || s == "Float" || s == "f64" || s == "f32" || s == "float64" || s == "double") return make_float();
    if (s == "string" || s == "str" || s == "String") return make_string();
    if (s == "bool" || s == "Bool" || s == "boolean") return make_bool();
    if (s == "null" || s == "nil" || s == "None") return make_null();
    if (s == "void" || s == "Void") return make_void();

    // List: list[T] or array[T]
    if (s.starts_with("list[") && s.ends_with("]")) {
        std::string inner = s.substr(5, s.length() - 6);
        return make_list(parse(inner));
    }
    if (s.starts_with("array[") && s.ends_with("]")) {
        std::string inner = s.substr(6, s.length() - 7);
        return make_list(parse(inner));
    }
    if (s == "list" || s == "List" || s == "array" || s == "Array") {
        return make_list(make_any());
    }

    // Map: map[K, V] or dict[K, V]
    if (s.starts_with("map[") && s.ends_with("]")) {
        std::string inner = s.substr(4, s.length() - 5);
        auto comma_pos = inner.find(',');
        if (comma_pos != std::string::npos) {
            std::string k = inner.substr(0, comma_pos);
            std::string v = inner.substr(comma_pos + 1);
            return make_map(parse(k), parse(v));
        }
        return make_map(make_string(), parse(inner));
    }
    if (s.starts_with("dict[") && s.ends_with("]")) {
        std::string inner = s.substr(5, s.length() - 6);
        auto comma_pos = inner.find(',');
        if (comma_pos != std::string::npos) {
            std::string k = inner.substr(0, comma_pos);
            std::string v = inner.substr(comma_pos + 1);
            return make_map(parse(k), parse(v));
        }
        return make_map(make_string(), parse(inner));
    }
    if (s == "map" || s == "Map" || s == "dict" || s == "Dict") {
        return make_map(make_string(), make_any());
    }

    // Tensor: tensor[float] or tensor[float, [3, 224, 224]]
    if (s.starts_with("tensor[") && s.ends_with("]")) {
        std::string inner = s.substr(7, s.length() - 8);
        return make_tensor(parse(inner));
    }
    if (s == "tensor" || s == "Tensor") {
        return make_tensor(make_float());
    }

    // Function: fn(T1, T2) -> R or (T1, T2) -> R
    if (s.starts_with("fn(") || s.starts_with("(")) {
        size_t arrow_pos = s.find("->");
        if (arrow_pos != std::string::npos) {
            std::string ret_str = s.substr(arrow_pos + 2);
            size_t open_paren = s.find('(');
            size_t close_paren = s.find(')', open_paren);
            std::vector<TypePtr> params;
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren + 1) {
                std::string params_str = s.substr(open_paren + 1, close_paren - open_paren - 1);
                std::stringstream ss(params_str);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    params.push_back(parse(item));
                }
            }
            return make_function(std::move(params), parse(ret_str));
        }
    }

    // Custom named struct / class / generic variable
    return make_custom(s);
}

bool Type::is_assignable_from(const TypePtr& other) const {
    if (!other) return false;
    if (this->is_any() || other->is_any()) return true;
    if (this->is_unknown() || other->is_unknown()) return true;

    if (this->kind_ == other->kind_) {
        switch (kind_) {
            case TypeKind::INT:
            case TypeKind::FLOAT:
            case TypeKind::STRING:
            case TypeKind::BOOL:
            case TypeKind::NULL_TYPE:
            case TypeKind::VOID:
                return true;
            case TypeKind::LIST:
                return element_type_->is_assignable_from(other->element_type_);
            case TypeKind::MAP:
                return key_type_->is_assignable_from(other->key_type_) &&
                       value_type_->is_assignable_from(other->value_type_);
            case TypeKind::FUNCTION:
                if (param_types_.size() != other->param_types_.size()) return false;
                for (size_t i = 0; i < param_types_.size(); ++i) {
                    if (!param_types_[i]->is_assignable_from(other->param_types_[i])) return false;
                }
                return return_type_->is_assignable_from(other->return_type_);
            case TypeKind::TENSOR:
                return element_type_->is_assignable_from(other->element_type_);
            case TypeKind::CUSTOM:
            case TypeKind::TYPE_VAR:
                return custom_name_ == other->custom_name_;
            default:
                return true;
        }
    }

    // Allow widening int -> float
    if (this->is_float() && other->is_int()) return true;

    return false;
}

bool Type::equals(const TypePtr& other) const {
    if (!other) return false;
    if (this->kind_ != other->kind_) return false;
    if (this->kind_ == TypeKind::CUSTOM || this->kind_ == TypeKind::TYPE_VAR) {
        return this->custom_name_ == other->custom_name_;
    }
    if (this->kind_ == TypeKind::LIST) {
        return element_type_->equals(other->element_type_);
    }
    if (this->kind_ == TypeKind::MAP) {
        return key_type_->equals(other->key_type_) && value_type_->equals(other->value_type_);
    }
    if (this->kind_ == TypeKind::FUNCTION) {
        if (param_types_.size() != other->param_types_.size()) return false;
        for (size_t i = 0; i < param_types_.size(); ++i) {
            if (!param_types_[i]->equals(other->param_types_[i])) return false;
        }
        return return_type_->equals(other->return_type_);
    }
    return true;
}

std::string Type::to_string() const {
    switch (kind_) {
        case TypeKind::ANY: return "any";
        case TypeKind::INT: return "int";
        case TypeKind::FLOAT: return "float";
        case TypeKind::STRING: return "string";
        case TypeKind::BOOL: return "bool";
        case TypeKind::NULL_TYPE: return "null";
        case TypeKind::VOID: return "void";
        case TypeKind::LIST:
            return "list[" + (element_type_ ? element_type_->to_string() : "any") + "]";
        case TypeKind::MAP:
            return "map[" + (key_type_ ? key_type_->to_string() : "string") + ", " +
                   (value_type_ ? value_type_->to_string() : "any") + "]";
        case TypeKind::FUNCTION: {
            std::string res = "fn(";
            for (size_t i = 0; i < param_types_.size(); ++i) {
                if (i > 0) res += ", ";
                res += param_types_[i]->to_string();
            }
            res += ") -> " + (return_type_ ? return_type_->to_string() : "void");
            return res;
        }
        case TypeKind::TENSOR: {
            std::string res = "tensor[" + (element_type_ ? element_type_->to_string() : "float");
            if (!tensor_shape_.empty()) {
                res += ", [";
                for (size_t i = 0; i < tensor_shape_.size(); ++i) {
                    if (i > 0) res += (i == 0 ? "" : ", ") + std::to_string(tensor_shape_[i]);
                }
                res += "]";
            }
            res += "]";
            return res;
        }
        case TypeKind::CUSTOM: return custom_name_.empty() ? "custom" : custom_name_;
        case TypeKind::TYPE_VAR: return custom_name_.empty() ? "T" : custom_name_;
        case TypeKind::UNKNOWN: return "unknown";
    }
    return "unknown";
}

} // namespace nextviper
