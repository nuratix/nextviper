#pragma once

#include "nextviper/common.hpp"
#include <string>
#include <vector>

namespace nextviper {

struct FormatResult {
    bool is_formatted = true;
    std::string formatted_content;
    std::string diff;
    std::string file_path;
};

class Formatter {
public:
    static std::string format_source(const std::string& source);
    static bool is_formatted(const std::string& source);
    static std::string format_diff(const std::string& original, const std::string& formatted, const std::string& filename = "file.nv");
    static FormatResult format_file(const std::string& file_path, bool write_in_place = true);
};

} // namespace nextviper
