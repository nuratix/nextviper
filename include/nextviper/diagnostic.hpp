#pragma once

#include "nextviper/common.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace nextviper {

enum class DiagnosticLevel {
    NOTE,
    HELP,
    WARNING,
    ERROR
};

struct Diagnostic {
    DiagnosticLevel level;
    std::string message;
    SourceSpan span;
    std::string hint;

    Diagnostic(DiagnosticLevel level, std::string message, SourceSpan span = {}, std::string hint = "")
        : level(level), message(std::move(message)), span(span), hint(std::move(hint)) {}
};

class SourceManager {
public:
    void add_file(std::string path, std::string content);
    std::string_view get_content(const std::string& path) const;
    std::string_view get_line_content(const std::string& path, size_t line_number) const;
    bool has_file(const std::string& path) const;

private:
    struct FileEntry {
        std::string path;
        std::string content;
        std::vector<size_t> line_offsets;
    };
    std::vector<FileEntry> files_;

    void compute_line_offsets(FileEntry& entry);
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(SourceManager& sm, bool use_color = true)
        : source_manager_(sm), use_color_(use_color) {}

    void report(DiagnosticLevel level, const std::string& message, SourceSpan span = {}, const std::string& hint = "");
    void error(const std::string& message, SourceSpan span = {}, const std::string& hint = "");
    void warning(const std::string& message, SourceSpan span = {}, const std::string& hint = "");
    void note(const std::string& message, SourceSpan span = {});
    void help(const std::string& message, SourceSpan span = {});

    bool has_errors() const { return error_count_ > 0; }
    size_t error_count() const { return error_count_; }
    size_t warning_count() const { return warning_count_; }

    void render(std::ostream& os = std::cerr) const;
    void clear();

    void set_color(bool enable) { use_color_ = enable; }

private:
    SourceManager& source_manager_;
    bool use_color_;
    std::vector<Diagnostic> diagnostics_;
    size_t error_count_ = 0;
    size_t warning_count_ = 0;

    void render_single(std::ostream& os, const Diagnostic& diag) const;
};

} // namespace nextviper
