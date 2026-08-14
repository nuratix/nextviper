#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <ostream>

namespace nextviper {

struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
    size_t offset = 0;

    bool operator==(const SourceLocation& other) const {
        return line == other.line && column == other.column && offset == other.offset;
    }
};

struct SourceSpan {
    SourceLocation start;
    SourceLocation end;
    std::string file_path;

    SourceSpan() = default;
    SourceSpan(SourceLocation start_loc, SourceLocation end_loc, std::string path = "")
        : start(start_loc), end(end_loc), file_path(std::move(path)) {}

    static SourceSpan merge(const SourceSpan& a, const SourceSpan& b) {
        return SourceSpan(a.start, b.end, a.file_path.empty() ? b.file_path : a.file_path);
    }
};

inline std::ostream& operator<<(std::ostream& os, const SourceLocation& loc) {
    return os << loc.line << ":" << loc.column;
}

} // namespace nextviper
