#include "nextviper/value.hpp"
#include "nextviper/compiled_function.hpp"
#include <sstream>
#include <iomanip>

namespace nextviper {

Value Value::make_array(std::vector<Value> elements) {
    return Value(std::make_shared<std::vector<Value>>(std::move(elements)));
}

Value Value::make_object(std::map<std::string, Value> entries) {
    return Value(std::make_shared<std::map<std::string, Value>>(std::move(entries)));
}

Value Value::make_native_fn(std::string name, int arity, NativeFn func) {
    auto nfn = std::make_shared<NativeFunctionObject>();
    nfn->name = std::move(name);
    nfn->arity = arity;
    nfn->func = std::move(func);
    return Value(std::move(nfn));
}

bool Value::is_truthy() const {
    switch (type_) {
        case ValueType::NIL: return false;
        case ValueType::BOOL: return bool_val_;
        case ValueType::INT: return int_val_ != 0;
        case ValueType::FLOAT: return float_val_ != 0.0;
        case ValueType::STRING: return !string_val_.empty();
        case ValueType::ARRAY: return array_val_ && !array_val_->empty();
        case ValueType::OBJECT: return object_val_ && !object_val_->empty();
        case ValueType::FUNCTION: return true;
        case ValueType::NATIVE_FUNCTION: return true;
        case ValueType::COMPILED_FUNCTION: return true;
    }
    return false;
}

std::string Value::type_name() const {
    switch (type_) {
        case ValueType::NIL: return "Nil";
        case ValueType::BOOL: return "Bool";
        case ValueType::INT: return "Int";
        case ValueType::FLOAT: return "Float";
        case ValueType::STRING: return "String";
        case ValueType::ARRAY: return "Array";
        case ValueType::OBJECT: return "Object";
        case ValueType::FUNCTION: return "Function";
        case ValueType::NATIVE_FUNCTION: return "NativeFunction";
        case ValueType::COMPILED_FUNCTION: return "CompiledFunction";
    }
    return "Unknown";
}

std::string Value::to_string() const {
    switch (type_) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return bool_val_ ? "true" : "false";
        case ValueType::INT: return std::to_string(int_val_);
        case ValueType::FLOAT: {
            std::ostringstream ss;
            ss << float_val_;
            std::string s = ss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
                s += ".0";
            }
            return s;
        }
        case ValueType::STRING: return string_val_;
        case ValueType::ARRAY: {
            if (!array_val_) return "[]";
            std::string out = "[";
            for (size_t i = 0; i < array_val_->size(); ++i) {
                if (i > 0) out += ", ";
                out += (*array_val_)[i].inspect();
            }
            out += "]";
            return out;
        }
        case ValueType::OBJECT: {
            if (!object_val_) return "{}";
            std::string out = "{";
            size_t i = 0;
            for (const auto& [k, v] : *object_val_) {
                if (i > 0) out += ", ";
                out += "\"" + k + "\": " + v.inspect();
                i++;
            }
            out += "}";
            return out;
        }
        case ValueType::FUNCTION:
            return "<fn " + (func_val_ ? func_val_->name : "anonymous") + ">";
        case ValueType::NATIVE_FUNCTION:
            return "<native fn " + (native_fn_val_ ? native_fn_val_->name : "anonymous") + ">";
        case ValueType::COMPILED_FUNCTION:
            return "<compiled fn " + (compiled_fn_val_ ? compiled_fn_val_->name() : "anonymous") + ">";
    }
    return "nil";
}

std::string Value::inspect() const {
    if (type_ == ValueType::STRING) {
        std::string out = "\"";
        for (char c : string_val_) {
            if (c == '"') out += "\\\"";
            else if (c == '\n') out += "\\n";
            else if (c == '\t') out += "\\t";
            else if (c == '\r') out += "\\r";
            else if (c == '\\') out += "\\\\";
            else out += c;
        }
        out += "\"";
        return out;
    }
    return to_string();
}

bool Value::operator==(const Value& other) const {
    if (type_ != other.type_) {
        if (is_number() && other.is_number()) {
            return as_float() == other.as_float();
        }
        return false;
    }

    switch (type_) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return bool_val_ == other.bool_val_;
        case ValueType::INT: return int_val_ == other.int_val_;
        case ValueType::FLOAT: return float_val_ == other.float_val_;
        case ValueType::STRING: return string_val_ == other.string_val_;
        case ValueType::ARRAY: {
            if (array_val_ == other.array_val_) return true;
            if (!array_val_ || !other.array_val_) return false;
            if (array_val_->size() != other.array_val_->size()) return false;
            for (size_t i = 0; i < array_val_->size(); ++i) {
                if ((*array_val_)[i] != (*other.array_val_)[i]) return false;
            }
            return true;
        }
        case ValueType::OBJECT: {
            if (object_val_ == other.object_val_) return true;
            if (!object_val_ || !other.object_val_) return false;
            if (object_val_->size() != other.object_val_->size()) return false;
            for (const auto& [k, v] : *object_val_) {
                auto it = other.object_val_->find(k);
                if (it == other.object_val_->end() || it->second != v) return false;
            }
            return true;
        }
        case ValueType::FUNCTION: return func_val_ == other.func_val_;
        case ValueType::NATIVE_FUNCTION: return native_fn_val_ == other.native_fn_val_;
        case ValueType::COMPILED_FUNCTION: return compiled_fn_val_ == other.compiled_fn_val_;
    }
    return false;
}

bool Value::operator<(const Value& other) const {
    if (is_number() && other.is_number()) {
        return as_float() < other.as_float();
    }
    if (type_ == ValueType::STRING && other.type_ == ValueType::STRING) {
        return string_val_ < other.string_val_;
    }
    return false;
}

bool Value::operator<=(const Value& other) const {
    return *this < other || *this == other;
}

bool Value::operator>(const Value& other) const {
    return !(*this <= other);
}

bool Value::operator>=(const Value& other) const {
    return !(*this < other);
}

} // namespace nextviper
