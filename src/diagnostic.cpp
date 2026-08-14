#include "nextviper/diagnostic.hpp"
#include <iomanip>
#include <sstream>

namespace nextviper {

// ANSI escape codes
namespace color {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* BOLD_RED = "\033[1;31m";
    constexpr const char* BOLD_YELLOW = "\033[1;33m";
    constexpr const char* BOLD_BLUE = "\033[1;34m";
    constexpr const char* BOLD_CYAN = "\033[1;36m";
    constexpr const char* BOLD_GREEN = "\033[1;32m";
}

void SourceManager::add_file(std::string path, std::string content) {
    for (auto& f : files_) {
        if (f.path == path) {
            f.content = std::move(content);
            compute_line_offsets(f);
            return;
        }
    }
    FileEntry entry;
    entry.path = std::move(path);
    entry.content = std::move(content);
    compute_line_offsets(entry);
    files_.push_back(std::move(entry));
}

void SourceManager::compute_line_offsets(FileEntry& entry) {
    entry.line_offsets.clear();
    entry.line_offsets.push_back(0); // line 1 starts at index 0

    for (size_t i = 0; i < entry.content.size(); ++i) {
        if (entry.content[i] == '\n') {
            entry.line_offsets.push_back(i + 1);
        }
    }
}

bool SourceManager::has_file(const std::string& path) const {
    for (const auto& f : files_) {
        if (f.path == path) return true;
    }
    return false;
}

std::string_view SourceManager::get_content(const std::string& path) const {
    for (const auto& f : files_) {
        if (f.path == path) return f.content;
    }
    return "";
}

std::string_view SourceManager::get_line_content(const std::string& path, size_t line_number) const {
    for (const auto& f : files_) {
        if (f.path == path) {
            if (line_number == 0 || line_number > f.line_offsets.size()) {
                return "";
            }
            size_t start = f.line_offsets[line_number - 1];
            size_t end = (line_number < f.line_offsets.size()) ? f.line_offsets[line_number] - 1 : f.content.size();
            
            // Trim carriage return if present
            if (end > start && f.content[end - 1] == '\r') {
                end--;
            }
            return std::string_view(f.content.data() + start, end - start);
        }
    }
    return "";
}

void SourceManager::clear() {
    files_.clear();
}

static std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1f) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

std::string Diagnostic::to_json() const {
    std::ostringstream ss;
    std::string level_str;
    switch (level) {
        case DiagnosticLevel::ERROR: level_str = "error"; break;
        case DiagnosticLevel::WARNING: level_str = "warning"; break;
        case DiagnosticLevel::NOTE: level_str = "note"; break;
        case DiagnosticLevel::HELP: level_str = "help"; break;
    }

    ss << "{\n"
       << "  \"level\": \"" << level_str << "\",\n"
       << "  \"code\": \"" << escape_json(code) << "\",\n"
       << "  \"message\": \"" << escape_json(message) << "\",\n"
       << "  \"file\": \"" << escape_json(span.file_path) << "\",\n"
       << "  \"line\": " << span.start.line << ",\n"
       << "  \"column\": " << span.start.column << ",\n"
       << "  \"end_line\": " << span.end.line << ",\n"
       << "  \"end_column\": " << span.end.column << ",\n"
       << "  \"hint\": \"" << escape_json(hint) << "\"\n"
       << "}";
    return ss.str();
}

void DiagnosticEngine::report(DiagnosticLevel level, const std::string& message, SourceSpan span, const std::string& hint, const std::string& code) {
    if (level == DiagnosticLevel::ERROR) {
        error_count_++;
    } else if (level == DiagnosticLevel::WARNING) {
        warning_count_++;
    }
    diagnostics_.emplace_back(level, message, span, hint, code);
}

void DiagnosticEngine::error(const std::string& message, SourceSpan span, const std::string& hint, const std::string& code) {
    report(DiagnosticLevel::ERROR, message, span, hint, code);
}

void DiagnosticEngine::warning(const std::string& message, SourceSpan span, const std::string& hint, const std::string& code) {
    report(DiagnosticLevel::WARNING, message, span, hint, code);
}

void DiagnosticEngine::note(const std::string& message, SourceSpan span) {
    report(DiagnosticLevel::NOTE, message, span, "", "");
}

void DiagnosticEngine::help(const std::string& message, SourceSpan span) {
    report(DiagnosticLevel::HELP, message, span, "", "");
}

void DiagnosticEngine::clear() {
    diagnostics_.clear();
    error_count_ = 0;
    warning_count_ = 0;
}

void DiagnosticEngine::render(std::ostream& os) const {
    for (const auto& diag : diagnostics_) {
        render_single(os, diag);
    }
}

std::string DiagnosticEngine::render_to_string() const {
    std::ostringstream ss;
    for (const auto& diag : diagnostics_) {
        render_single(ss, diag);
    }
    return ss.str();
}

std::string DiagnosticEngine::to_json() const {
    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < diagnostics_.size(); ++i) {
        ss << diagnostics_[i].to_json();
        if (i + 1 < diagnostics_.size()) ss << ",";
        ss << "\n";
    }
    ss << "]\n";
    return ss.str();
}

void DiagnosticEngine::render_single(std::ostream& os, const Diagnostic& diag) const {
    const char* level_str = "error";
    const char* level_color = color::BOLD_RED;

    switch (diag.level) {
        case DiagnosticLevel::ERROR:
            level_str = "error";
            level_color = use_color_ ? color::BOLD_RED : "";
            break;
        case DiagnosticLevel::WARNING:
            level_str = "warning";
            level_color = use_color_ ? color::BOLD_YELLOW : "";
            break;
        case DiagnosticLevel::NOTE:
            level_str = "note";
            level_color = use_color_ ? color::BOLD_BLUE : "";
            break;
        case DiagnosticLevel::HELP:
            level_str = "help";
            level_color = use_color_ ? color::BOLD_GREEN : "";
            break;
    }

    const char* reset = use_color_ ? color::RESET : "";
    const char* bold = use_color_ ? color::BOLD : "";
    const char* cyan = use_color_ ? color::BOLD_CYAN : "";
    const char* green = use_color_ ? color::BOLD_GREEN : "";

    // 1. Header: error[NV102]:
    os << level_color << level_str;
    if (!diag.code.empty()) {
        os << "[" << diag.code << "]";
    }
    os << ":" << reset << "\n";

    // 2. Indented message:
    os << "    " << bold << diag.message << reset << "\n\n";

    // 3. Location line: --> main.nv:12:5
    if (!diag.span.file_path.empty() || diag.span.start.line > 0) {
        std::string path = diag.span.file_path.empty() ? "<input>" : diag.span.file_path;
        os << cyan << "    --> " << reset << path << ":" << diag.span.start.line << ":" << diag.span.start.column << "\n\n";

        std::string_view line_str = source_manager_.get_line_content(diag.span.file_path, diag.span.start.line);
        if (!line_str.empty() || diag.span.start.line > 0) {
            std::string line_num_str = std::to_string(diag.span.start.line);
            size_t gutter_width = std::max<size_t>(2, line_num_str.size());

            // Source line: 12 | print(total)
            os << cyan << "    " << std::setw(gutter_width) << line_num_str << " | " << reset << line_str << "\n";

            // Caret underline:    |       ^^^^^
            os << cyan << "    " << std::string(gutter_width, ' ') << " | " << reset;
            size_t col = (diag.span.start.column > 0) ? diag.span.start.column - 1 : 0;
            for (size_t i = 0; i < col; ++i) {
                if (i < line_str.size() && line_str[i] == '\t') {
                    os << "\t";
                } else {
                    os << " ";
                }
            }

            size_t underline_len = 1;
            if (diag.span.end.line == diag.span.start.line && diag.span.end.column > diag.span.start.column) {
                underline_len = diag.span.end.column - diag.span.start.column;
            }
            os << level_color;
            for (size_t i = 0; i < underline_len; ++i) {
                os << "^";
            }
            os << reset << "\n";
        }
    }

    // 4. Help section: help: define `total` before using it
    if (!diag.hint.empty()) {
        os << "\n    " << green << "help: " << reset << bold << diag.hint << reset << "\n";
    }

    os << "\n";
}

} // namespace nextviper
