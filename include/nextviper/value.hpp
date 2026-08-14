#pragma once

#include "nextviper/common.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <functional>
#include <ostream>

namespace nextviper {

// Forward declarations
class FnDeclStmt;
class Environment;
class CompiledFunction;
class Value;

struct FunctionObject {
    std::string name;
    std::vector<std::string> params;
    const FnDeclStmt* decl = nullptr;
    std::shared_ptr<Environment> closure;
};

using NativeFn = std::function<Value(const std::vector<Value>& args, SourceSpan span)>;

struct NativeFunctionObject {
    std::string name;
    int arity = -1; // -1 for variable arity
    NativeFn func;
};

enum class ValueType {
    NIL,
    BOOL,
    INT,
    FLOAT,
    STRING,
    ARRAY,
    OBJECT,
    FUNCTION,
    NATIVE_FUNCTION,
    COMPILED_FUNCTION
};

class Value {
public:
    using ArrayPtr = std::shared_ptr<std::vector<Value>>;
    using ObjectPtr = std::shared_ptr<std::map<std::string, Value>>;
    using FunctionPtr = std::shared_ptr<FunctionObject>;
    using NativeFnPtr = std::shared_ptr<NativeFunctionObject>;
    using CompiledFnPtr = std::shared_ptr<CompiledFunction>;

    // Constructors
    Value() : type_(ValueType::NIL) {}
    Value(std::nullptr_t) : type_(ValueType::NIL) {}
    Value(bool b) : type_(ValueType::BOOL), bool_val_(b) {}
    Value(int64_t i) : type_(ValueType::INT), int_val_(i) {}
    Value(int i) : type_(ValueType::INT), int_val_(i) {}
    Value(double f) : type_(ValueType::FLOAT), float_val_(f) {}
    Value(std::string s) : type_(ValueType::STRING), string_val_(std::move(s)) {}
    Value(const char* s) : type_(ValueType::STRING), string_val_(s) {}
    Value(ArrayPtr arr) : type_(ValueType::ARRAY), array_val_(std::move(arr)) {}
    Value(ObjectPtr obj) : type_(ValueType::OBJECT), object_val_(std::move(obj)) {}
    Value(FunctionPtr fn) : type_(ValueType::FUNCTION), func_val_(std::move(fn)) {}
    Value(NativeFnPtr nfn) : type_(ValueType::NATIVE_FUNCTION), native_fn_val_(std::move(nfn)) {}
    Value(CompiledFnPtr cfn) : type_(ValueType::COMPILED_FUNCTION), compiled_fn_val_(std::move(cfn)) {}

    // Factory methods
    static Value make_nil() { return Value(); }
    static Value make_bool(bool b) { return Value(b); }
    static Value make_int(int64_t i) { return Value(i); }
    static Value make_float(double f) { return Value(f); }
    static Value make_string(std::string s) { return Value(std::move(s)); }
    static Value make_array(std::vector<Value> elements = {});
    static Value make_object(std::map<std::string, Value> entries = {});
    static Value make_function(FunctionPtr fn) { return Value(std::move(fn)); }
    static Value make_native_fn(std::string name, int arity, NativeFn func);
    static Value make_compiled_fn(CompiledFnPtr cfn) { return Value(std::move(cfn)); }

    // Type queries
    ValueType type() const { return type_; }
    bool is_nil() const { return type_ == ValueType::NIL; }
    bool is_bool() const { return type_ == ValueType::BOOL; }
    bool is_int() const { return type_ == ValueType::INT; }
    bool is_float() const { return type_ == ValueType::FLOAT; }
    bool is_number() const { return type_ == ValueType::INT || type_ == ValueType::FLOAT; }
    bool is_string() const { return type_ == ValueType::STRING; }
    bool is_array() const { return type_ == ValueType::ARRAY; }
    bool is_object() const { return type_ == ValueType::OBJECT; }
    bool is_function() const { return type_ == ValueType::FUNCTION || type_ == ValueType::NATIVE_FUNCTION || type_ == ValueType::COMPILED_FUNCTION; }
    bool is_callable() const { return is_function(); }

    // Getters
    bool as_bool() const { return bool_val_; }
    int64_t as_int() const { return int_val_; }
    double as_float() const { return type_ == ValueType::INT ? static_cast<double>(int_val_) : float_val_; }
    const std::string& as_string() const { return string_val_; }
    ArrayPtr as_array() const { return array_val_; }
    ObjectPtr as_object() const { return object_val_; }
    FunctionPtr as_function() const { return func_val_; }
    NativeFnPtr as_native_fn() const { return native_fn_val_; }
    CompiledFnPtr as_compiled_fn() const { return compiled_fn_val_; }

    // Semantics
    bool is_truthy() const;
    std::string type_name() const;
    std::string to_string() const;
    std::string inspect() const;

    // Comparisons
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }
    bool operator<(const Value& other) const;
    bool operator<=(const Value& other) const;
    bool operator>(const Value& other) const;
    bool operator>=(const Value& other) const;

private:
    ValueType type_ = ValueType::NIL;
    bool bool_val_ = false;
    int64_t int_val_ = 0;
    double float_val_ = 0.0;
    std::string string_val_;
    ArrayPtr array_val_;
    ObjectPtr object_val_;
    FunctionPtr func_val_;
    NativeFnPtr native_fn_val_;
    CompiledFnPtr compiled_fn_val_;
};

inline std::ostream& operator<<(std::ostream& os, const Value& val) {
    return os << val.to_string();
}

inline std::ostream& operator<<(std::ostream& os, ValueType type) {
    switch (type) {
        case ValueType::NIL: return os << "Nil";
        case ValueType::BOOL: return os << "Bool";
        case ValueType::INT: return os << "Int";
        case ValueType::FLOAT: return os << "Float";
        case ValueType::STRING: return os << "String";
        case ValueType::ARRAY: return os << "Array";
        case ValueType::OBJECT: return os << "Object";
        case ValueType::FUNCTION: return os << "Function";
        case ValueType::NATIVE_FUNCTION: return os << "NativeFunction";
        case ValueType::COMPILED_FUNCTION: return os << "CompiledFunction";
    }
    return os << "Unknown";
}

} // namespace nextviper
