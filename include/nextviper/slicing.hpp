#pragma once

#include "nextviper/value.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace nextviper {

// Bounds are normalized after evaluating every index expression, because an
// expression may resize the target. Omitted reverse bounds differ from -1.
inline Value slice_value(const Value& target, std::optional<int64_t> start,
                         std::optional<int64_t> end, int64_t step) {
    if (!target.is_array() && !target.is_string())
        throw std::invalid_argument("slicing requires list or string, got " + target.type_name());
    if (step == 0) throw std::invalid_argument("slice step cannot be zero");
    const int64_t length = target.is_array() ? static_cast<int64_t>(target.as_array()->size())
                                             : static_cast<int64_t>(target.as_string().size());
    const bool forward = step > 0;
    auto normalize = [=](int64_t index) {
        if (index < 0) index += length;
        return std::clamp(index, forward ? int64_t{0} : int64_t{-1},
                          forward ? length : length - 1);
    };
    const int64_t first = start ? normalize(*start) : (forward ? 0 : length - 1);
    const int64_t stop = end ? normalize(*end) : (forward ? length : -1);
    std::vector<Value> values;
    std::string text;
    for (int64_t i = first; forward ? i < stop : i > stop;) {
        if (target.is_array()) values.push_back((*target.as_array())[static_cast<size_t>(i)]);
        else text.push_back(target.as_string()[static_cast<size_t>(i)]);
        // Only advance if the next index remains inside the slice. This avoids
        // signed overflow even for INT64_MIN / INT64_MAX strides.
        if (forward ? step >= stop - i : step <= stop - i) break;
        i += step;
    }
    return target.is_array() ? Value::make_array(std::move(values)) : Value::make_string(std::move(text));
}

} // namespace nextviper
