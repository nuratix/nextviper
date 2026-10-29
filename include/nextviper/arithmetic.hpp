#pragma once

#include "nextviper/token.hpp"
#include "nextviper/value.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nextviper {

inline int64_t checked_add(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_add_overflow(a, b, &result)) throw std::overflow_error("integer overflow");
    return result;
}

inline int64_t checked_subtract(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_sub_overflow(a, b, &result)) throw std::overflow_error("integer overflow");
    return result;
}

inline int64_t checked_multiply(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_mul_overflow(a, b, &result)) throw std::overflow_error("integer overflow");
    return result;
}

inline int64_t checked_power(int64_t base, int64_t exponent) {
    int64_t result = 1;
    // Exponentiation by squaring also bounds the work for very large exponents.
    while (exponent > 0) {
        if (exponent & 1) result = checked_multiply(result, base);
        exponent >>= 1;
        if (exponent) base = checked_multiply(base, base);
    }
    return result;
}

inline int64_t checked_float_to_int(double number) {
    // INT64_MAX rounds to 2^63 as a double, so the upper bound is exclusive.
    constexpr double upper = 9223372036854775808.0;
    if (!std::isfinite(number) || number < -upper || number >= upper)
        throw std::overflow_error("number is outside the Int range");
    return static_cast<int64_t>(number);
}

inline TokenType compound_operator(TokenType op) {
    switch (op) {
        case TokenType::PLUS_ASSIGN: return TokenType::PLUS;
        case TokenType::MINUS_ASSIGN: return TokenType::MINUS;
        case TokenType::STAR_ASSIGN: return TokenType::STAR;
        case TokenType::SLASH_ASSIGN: return TokenType::SLASH;
        case TokenType::PERCENT_ASSIGN: return TokenType::PERCENT;
        default: throw std::invalid_argument("unsupported compound assignment");
    }
}

inline Value make_range_values(int64_t start, int64_t end, int64_t step = 1, bool inclusive = false) {
    if (step == 0) throw std::invalid_argument("range step cannot be zero");
    if ((step > 0 && (start > end || (!inclusive && start == end))) ||
        (step < 0 && (start < end || (!inclusive && start == end)))) return Value::make_array();
    const uint64_t distance = step > 0 ? uint64_t(end) - uint64_t(start) : uint64_t(start) - uint64_t(end);
    const uint64_t magnitude = step > 0 ? uint64_t(step) : uint64_t(0) - uint64_t(step);
    const uint64_t quotient = inclusive ? distance / magnitude : (distance - 1) / magnitude;
    if (quotient >= 10'000'000) throw std::length_error("range element count exceeds maximum allowed limit (10,000,000)");
    const uint64_t count = quotient + 1;
    std::vector<Value> values;
    values.reserve(static_cast<size_t>(count));
    int64_t value = start;
    for (uint64_t i = 0; i < count; ++i) {
        values.push_back(Value::make_int(value));
        if (i + 1 < count) value = checked_add(value, step);
    }
    return Value::make_array(std::move(values));
}

// Shared by the interpreter and VM. Compound assignments have exactly the
// arithmetic semantics of their corresponding binary operator.
inline Value apply_arithmetic(TokenType op, const Value& a, const Value& b) {
    constexpr size_t max_string_bytes = 64 * 1024 * 1024;
    constexpr size_t max_array_elements = 10'000'000;
    if (op == TokenType::PLUS && (a.is_string() || b.is_string())) {
        auto left = a.to_string(), right = b.to_string();
        if (left.size() > max_string_bytes || right.size() > max_string_bytes - left.size())
            throw std::length_error("string size limit exceeded");
        return Value::make_string(left + right);
    }
    if (op == TokenType::PLUS && a.is_array() && b.is_array()) {
        const auto& left = *a.as_array();
        const auto& right = *b.as_array();
        if (left.size() > max_array_elements || right.size() > max_array_elements - left.size())
            throw std::length_error("array size limit exceeded");
        auto result = left;
        result.insert(result.end(), right.begin(), right.end());
        return Value::make_array(std::move(result));
    }
    if (op == TokenType::STAR && a.is_string() && b.is_int()) {
        const auto& text = a.as_string();
        const auto count = b.as_int();
        if (count <= 0 || text.empty()) return Value::make_string("");
        if (static_cast<uint64_t>(count) > max_string_bytes / text.size())
            throw std::length_error("string size limit exceeded");
        std::string result;
        result.reserve(text.size() * static_cast<size_t>(count));
        for (int64_t i = 0; i < count; ++i) result += text;
        return Value::make_string(std::move(result));
    }
    if (!a.is_number() || !b.is_number())
        throw std::invalid_argument("arithmetic requires numbers, got " + a.type_name() + " and " + b.type_name());
    if (op == TokenType::PERCENT) {
        if (!a.is_int() || !b.is_int()) throw std::invalid_argument("operator '%' requires integers");
        if (b.as_int() == 0) throw std::domain_error("division by zero");
        return Value::make_int(a.as_int() == INT64_MIN && b.as_int() == -1 ? 0 : a.as_int() % b.as_int());
    }
    if (op == TokenType::SLASH) {
        if (b.as_float() == 0) throw std::domain_error("division by zero");
        if (a.is_int() && b.is_int()) {
            if (a.as_int() == INT64_MIN && b.as_int() == -1) throw std::overflow_error("integer overflow");
            if (a.as_int() % b.as_int() == 0) return Value::make_int(a.as_int() / b.as_int());
        }
        return Value::make_float(a.as_float() / b.as_float());
    }
    if (a.is_int() && b.is_int()) {
        switch (op) {
            case TokenType::PLUS: return Value::make_int(checked_add(a.as_int(), b.as_int()));
            case TokenType::MINUS: return Value::make_int(checked_subtract(a.as_int(), b.as_int()));
            case TokenType::STAR: return Value::make_int(checked_multiply(a.as_int(), b.as_int()));
            case TokenType::POWER:
                if (b.as_int() >= 0) return Value::make_int(checked_power(a.as_int(), b.as_int()));
                break;
            default: break;
        }
    }
    switch (op) {
        case TokenType::PLUS: return Value::make_float(a.as_float() + b.as_float());
        case TokenType::MINUS: return Value::make_float(a.as_float() - b.as_float());
        case TokenType::STAR: return Value::make_float(a.as_float() * b.as_float());
        case TokenType::POWER: return Value::make_float(std::pow(a.as_float(), b.as_float()));
        default: throw std::invalid_argument("unsupported arithmetic operator");
    }
}

} // namespace nextviper
