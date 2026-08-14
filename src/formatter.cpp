#include "nextviper/formatter.hpp"
#include <sstream>
#include <vector>
#include <cctype>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace nextviper {

static std::string trim_right(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string trim_left(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start);
}

static std::string format_line_tokens(const std::string& line) {
    std::string result;
    bool in_string = false;
    char string_char = 0;
    bool in_comment = false;
    std::string comment_part;

    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];

        // 1. Check for single-line comments
        if (!in_string && c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            in_comment = true;
            comment_part = line.substr(i);
            break;
        }

        // 2. Handle string literals
        if (in_string) {
            result += c;
            if (c == '\\' && i + 1 < line.size()) {
                result += line[i + 1];
                i += 2;
                continue;
            }
            if (c == string_char) {
                in_string = false;
            }
            i++;
            continue;
        } else if (c == '"' || c == '\'') {
            in_string = true;
            string_char = c;
            result += c;
            i++;
            continue;
        }

        // 3. Multi-character operators
        if (i + 1 < line.size()) {
            std::string op2 = line.substr(i, 2);
            if (op2 == "==" || op2 == "!=" || op2 == "<=" || op2 == ">=" ||
                op2 == "+=" || op2 == "-=" || op2 == "*=" || op2 == "/=" ||
                op2 == "&&" || op2 == "||" || op2 == "|>" || op2 == "->") {
                // Ensure spaces around 2-char operators
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += op2;
                result += ' ';
                i += 2;
                // Skip subsequent spaces
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
                continue;
            }
            if (op2 == "..") {
                // Keep range operator compact
                if (i + 2 < line.size() && line[i + 2] == '=') {
                    // ..=
                    while (!result.empty() && result.back() == ' ') result.pop_back();
                    result += "..=";
                    i += 3;
                } else {
                    while (!result.empty() && result.back() == ' ') result.pop_back();
                    result += "..";
                    i += 2;
                }
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
                continue;
            }
        }

        // 4. Single-character operators and delimiters
        if (c == '=') {
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += '=';
            result += ' ';
            i++;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            continue;
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            // Check if unary minus/plus (e.g. at start, after '(', after '=', after ',')
            bool is_unary = false;
            if (c == '-' || c == '+') {
                std::string prev_trimmed = trim_right(result);
                if (prev_trimmed.empty() || prev_trimmed.back() == '(' || prev_trimmed.back() == '[' ||
                    prev_trimmed.back() == '=' || prev_trimmed.back() == ',' || prev_trimmed.back() == ':') {
                    is_unary = true;
                }
            }

            if (is_unary) {
                if (!result.empty() && result.back() != ' ' && result.back() != '(' && result.back() != '[') {
                    result += ' ';
                }
                result += c;
                i++;
            } else {
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += c;
                result += ' ';
                i++;
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            }
            continue;
        } else if (c == '<' || c == '>') {
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += c;
            result += ' ';
            i++;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            continue;
        } else if (c == ',') {
            while (!result.empty() && result.back() == ' ') result.pop_back();
            result += ", ";
            i++;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            continue;
        } else if (c == ':') {
            // If end of line (e.g. if x > 0:), no space after or before
            while (!result.empty() && result.back() == ' ') result.pop_back();
            result += ':';
            i++;
            if (i < line.size()) {
                // If in middle of line, add one space after
                if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\r') {
                    result += ' ';
                }
            }
            continue;
        } else if (c == ' ' || c == '\t') {
            if (!result.empty() && result.back() != ' ' && result.back() != '(' && result.back() != '[') {
                result += ' ';
            }
            i++;
            continue;
        } else {
            result += c;
            i++;
        }
    }

    result = trim_right(result);
    if (in_comment) {
        if (!result.empty()) {
            result += "  ";
        }
        result += comment_part;
    }
    return result;
}

std::string Formatter::format_source(const std::string& source) {
    std::istringstream stream(source);
    std::string raw_line;
    std::vector<std::string> lines;
    int current_indent = 0;
    bool prev_was_empty = false;

    while (std::getline(stream, raw_line)) {
        std::string trimmed = trim_left(trim_right(raw_line));
        if (trimmed.empty()) {
            if (!prev_was_empty && !lines.empty()) {
                lines.push_back("");
                prev_was_empty = true;
            }
            continue;
        }
        prev_was_empty = false;

        // Check for dedents
        if (trimmed[0] == '}' || trimmed[0] == ']' || trimmed[0] == ')') {
            current_indent = std::max(0, current_indent - 1);
        } else if (trimmed.rfind("else:", 0) == 0 || trimmed.rfind("elif ", 0) == 0) {
            int temp_indent = std::max(0, current_indent - 1);
            std::string formatted_line = format_line_tokens(trimmed);
            lines.push_back(std::string(temp_indent * 4, ' ') + formatted_line);
            continue;
        }

        // Format line content
        std::string formatted_line = format_line_tokens(trimmed);
        lines.push_back(std::string(current_indent * 4, ' ') + formatted_line);

        // Check if line opens a new block
        std::string code_no_comment = formatted_line;
        size_t c_idx = code_no_comment.find("//");
        if (c_idx != std::string::npos) {
            code_no_comment = trim_right(code_no_comment.substr(0, c_idx));
        }

        if (!code_no_comment.empty()) {
            char last_c = code_no_comment.back();
            if (last_c == ':' || last_c == '{' || last_c == '[') {
                current_indent++;
            }
        }
    }

    std::ostringstream result;
    for (const auto& l : lines) {
        result << l << "\n";
    }

    std::string res = result.str();
    while (res.size() >= 2 && res[res.size() - 1] == '\n' && res[res.size() - 2] == '\n') {
        res.pop_back();
    }
    if (res.empty() || res.back() != '\n') {
        res.push_back('\n');
    }
    return res;
}

bool Formatter::is_formatted(const std::string& source) {
    return format_source(source) == source;
}

std::string Formatter::format_diff(const std::string& original, const std::string& formatted, const std::string& filename) {
    std::istringstream orig_stream(original);
    std::istringstream fmt_stream(formatted);

    std::vector<std::string> orig_lines;
    std::vector<std::string> fmt_lines;

    std::string line;
    while (std::getline(orig_stream, line)) orig_lines.push_back(line);
    while (std::getline(fmt_stream, line)) fmt_lines.push_back(line);

    std::ostringstream diff;
    diff << "--- " << filename << " (original)\n";
    diff << "+++ " << filename << " (formatted)\n";

    size_t max_lines = std::max(orig_lines.size(), fmt_lines.size());
    for (size_t i = 0; i < max_lines; ++i) {
        std::string o = (i < orig_lines.size()) ? orig_lines[i] : "";
        std::string f = (i < fmt_lines.size()) ? fmt_lines[i] : "";

        if (o != f) {
            diff << "@@ line " << (i + 1) << " @@\n";
            if (i < orig_lines.size()) diff << "- " << o << "\n";
            if (i < fmt_lines.size()) diff << "+ " << f << "\n";
        }
    }
    return diff.str();
}

FormatResult Formatter::format_file(const std::string& file_path, bool write_in_place) {
    FormatResult res;
    res.file_path = file_path;

    std::ifstream in(file_path);
    if (!in.is_open()) {
        res.is_formatted = false;
        return res;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();

    std::string original = buffer.str();
    std::string formatted = format_source(original);

    res.formatted_content = formatted;
    res.is_formatted = (original == formatted);

    if (!res.is_formatted) {
        res.diff = format_diff(original, formatted, file_path);
        if (write_in_place) {
            std::ofstream out(file_path);
            if (out.is_open()) {
                out << formatted;
                out.close();
            }
        }
    }
    return res;
}

} // namespace nextviper
