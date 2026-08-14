#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>

namespace nextviper {

enum class TypeKind {
    ANY,
    INT,
    FLOAT,
    STRING,
    BOOL,
    NULL_TYPE,
    VOID,
    LIST,
    MAP,
    FUNCTION,
    TENSOR,
    CUSTOM,
    TYPE_VAR,
    UNKNOWN
};

class Type;
using TypePtr = std::shared_ptr<Type>;

class Type : public std::enable_shared_from_this<Type> {
public:
    explicit Type(TypeKind kind, std::string custom_name = "")
        : kind_(kind), custom_name_(std::move(custom_name)) {}

    TypeKind kind() const { return kind_; }
    const std::string& custom_name() const { return custom_name_; }

    // Factory methods
    static TypePtr make_any();
    static TypePtr make_int();
    static TypePtr make_float();
    static TypePtr make_string();
    static TypePtr make_bool();
    static TypePtr make_null();
    static TypePtr make_void();
    static TypePtr make_unknown();
    static TypePtr make_list(TypePtr element_type);
    static TypePtr make_map(TypePtr key_type, TypePtr value_type);
    static TypePtr make_function(std::vector<TypePtr> param_types, TypePtr return_type);
    static TypePtr make_tensor(TypePtr element_type, std::vector<int64_t> shape = {});
    static TypePtr make_custom(std::string name);
    static TypePtr make_type_var(std::string name);

    // Parse type string (e.g., "int", "float", "string", "bool", "list[int]", "map[string, int]", "fn(int, int) -> int")
    static TypePtr parse(const std::string& str);

    // Queries
    bool is_any() const { return kind_ == TypeKind::ANY; }
    bool is_int() const { return kind_ == TypeKind::INT; }
    bool is_float() const { return kind_ == TypeKind::FLOAT; }
    bool is_numeric() const { return is_int() || is_float(); }
    bool is_string() const { return kind_ == TypeKind::STRING; }
    bool is_bool() const { return kind_ == TypeKind::BOOL; }
    bool is_null() const { return kind_ == TypeKind::NULL_TYPE; }
    bool is_void() const { return kind_ == TypeKind::VOID; }
    bool is_list() const { return kind_ == TypeKind::LIST; }
    bool is_map() const { return kind_ == TypeKind::MAP; }
    bool is_function() const { return kind_ == TypeKind::FUNCTION; }
    bool is_tensor() const { return kind_ == TypeKind::TENSOR; }
    bool is_custom() const { return kind_ == TypeKind::CUSTOM; }
    bool is_unknown() const { return kind_ == TypeKind::UNKNOWN; }

    // Sub-type getters
    TypePtr element_type() const { return element_type_; }
    TypePtr key_type() const { return key_type_; }
    TypePtr value_type() const { return value_type_; }
    const std::vector<TypePtr>& param_types() const { return param_types_; }
    TypePtr return_type() const { return return_type_; }
    const std::vector<int64_t>& tensor_shape() const { return tensor_shape_; }

    // Type compatibility & subtyping
    bool is_assignable_from(const TypePtr& other) const;
    bool equals(const TypePtr& other) const;

    // String representation
    std::string to_string() const;

private:
    TypeKind kind_;
    std::string custom_name_;

    // For LIST and TENSOR
    TypePtr element_type_ = nullptr;

    // For MAP
    TypePtr key_type_ = nullptr;
    TypePtr value_type_ = nullptr;

    // For FUNCTION
    std::vector<TypePtr> param_types_;
    TypePtr return_type_ = nullptr;

    // For TENSOR
    std::vector<int64_t> tensor_shape_;
};

} // namespace nextviper
