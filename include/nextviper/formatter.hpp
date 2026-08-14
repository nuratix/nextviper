#pragma once

#include "nextviper/common.hpp"
#include <string>

namespace nextviper {

class Formatter {
public:
    static std::string format_source(const std::string& source);
};

} // namespace nextviper
