#include "nextviper/formatter.hpp"
#include <sstream>
#include <vector>
#include <cctype>

namespace nextviper {

static std::string trim_right(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string trim_left(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string Formatter::format_source(const std::string& source) {
    std::istringstream stream(source);
    std::string line;
    std::ostringstream result;
    int current_indent = 0;

    while (std::getline(stream, line)) {
        std::string trimmed = trim_left(trim_right(line));
        if (trimmed.empty()) {
            result << "\n";
            continue;
        }

        // Check if line closes a block or decreases indent
        if (trimmed[0] == '}' || trimmed[0] == ']' || trimmed[0] == ')') {
            current_indent = std::max(0, current_indent - 1);
        } else if (trimmed.rfind("else:", 0) == 0 || trimmed.rfind("elif ", 0) == 0) {
            // Un-indent else/elif relative to block
            int temp_indent = std::max(0, current_indent - 1);
            result << std::string(temp_indent * 4, ' ') << trimmed << "\n";
            continue;
        }

        result << std::string(current_indent * 4, ' ') << trimmed << "\n";

        // Check if line ends in a colon or open brace
        if (!trimmed.empty() && (trimmed.back() == ':' || trimmed.back() == '{' || trimmed.back() == '[')) {
            current_indent++;
        }
    }

    std::string formatted = result.str();
    // Ensure single trailing newline
    while (!formatted.empty() && formatted.back() == '\n') formatted.pop_back();
    formatted.push_back('\n');
    return formatted;
}

} // namespace nextviper
