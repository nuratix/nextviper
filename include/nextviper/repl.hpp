#pragma once

#include "nextviper/interpreter.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>

namespace nextviper {

class REPL {
public:
    REPL();
    void run();

private:
    void print_banner() const;
    void print_help() const;
    bool handle_command(const std::string& line);

    SourceManager source_manager_;
    DiagnosticEngine diagnostics_;
    Interpreter interpreter_;
    size_t line_counter_ = 1;
};

} // namespace nextviper
