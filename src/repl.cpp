#include "nextviper/repl.hpp"
#include "nextviper/version.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include <iostream>
#include <sstream>

namespace nextviper {

REPL::REPL()
    : diagnostics_(source_manager_), interpreter_(diagnostics_) {}

void REPL::print_banner() const {
    std::cout << "\033[1;36m====================================================\033[0m\n";
    std::cout << "\033[1;32m  NextViper\033[0m " << VERSION_STRING << " (" << RELEASE_CODENAME << ")\n";
    std::cout << "  Interactive Shell & Development Runtime\n";
    std::cout << "  Type \033[1m:help\033[0m for commands, \033[1m:exit\033[0m to quit.\n";
    std::cout << "\033[1;36m====================================================\033[0m\n\n";
}

void REPL::print_help() const {
    std::cout << "\nNextViper REPL Commands:\n";
    std::cout << "  :help       Display this help message\n";
    std::cout << "  :version    Display version and build information\n";
    std::cout << "  :env        List defined variables and functions\n";
    std::cout << "  :clear      Clear the terminal screen\n";
    std::cout << "  :exit       Exit the interactive shell\n\n";
}

bool REPL::handle_command(const std::string& line) {
    if (line == ":exit" || line == ":quit" || line == ":q") {
        return false;
    }
    if (line == ":help" || line == ":h") {
        print_help();
        return true;
    }
    if (line == ":version" || line == ":v") {
        std::cout << get_full_version_string() << "\n";
        return true;
    }
    if (line == ":clear") {
        std::cout << "\033[2J\033[1;1H";
        return true;
    }
    if (line == ":env") {
        std::cout << "\nDefined Environment Bindings:\n";
        for (const auto& [k, binding] : interpreter_.globals()->bindings()) {
            std::cout << "  " << (binding.is_mut ? "mut " : "let ") << k << " = " << binding.value.inspect() << "\n";
        }
        std::cout << "\n";
        return true;
    }

    std::cout << "Unknown command: " << line << ". Type :help for commands.\n";
    return true;
}

static bool needs_multiline_continuation(const std::string& code) {
    if (code.empty()) return false;

    // Check if line ends with colon
    size_t last_non_ws = code.find_last_not_of(" \t\r\n");
    if (last_non_ws != std::string::npos && code[last_non_ws] == ':') {
        return true;
    }

    // Check unclosed braces/brackets/parentheses
    int paren = 0, bracket = 0, brace = 0;
    bool in_str = false;
    char str_q = 0;

    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (in_str) {
            if (c == '\\' && i + 1 < code.size()) {
                i++;
                continue;
            }
            if (c == str_q) in_str = false;
            continue;
        }

        if (c == '"' || c == '\'') {
            in_str = true;
            str_q = c;
        } else if (c == '(') paren++;
        else if (c == ')') paren = std::max(0, paren - 1);
        else if (c == '[') bracket++;
        else if (c == ']') bracket = std::max(0, bracket - 1);
        else if (c == '{') brace++;
        else if (c == '}') brace = std::max(0, brace - 1);
    }

    return (paren > 0 || bracket > 0 || brace > 0);
}

void REPL::run() {
    print_banner();

    std::string line;
    std::string accumulated_code;

    while (true) {
        if (accumulated_code.empty()) {
            std::cout << "\033[1;32mnv>\033[0m ";
        } else {
            std::cout << "\033[1;36m... \033[0m";
        }

        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        if (accumulated_code.empty() && line.empty()) continue;

        if (accumulated_code.empty() && !line.empty() && line[0] == ':') {
            if (!handle_command(line)) {
                break;
            }
            continue;
        }

        if (!accumulated_code.empty()) {
            accumulated_code += "\n" + line;
        } else {
            accumulated_code = line;
        }

        // If user is inside multiline and submits empty line, finish
        if (line.empty() || !needs_multiline_continuation(accumulated_code)) {
            std::string code = accumulated_code;
            accumulated_code.clear();

            std::string source_name = "<repl-" + std::to_string(line_counter_++) + ">";
            source_manager_.add_file(source_name, code);
            diagnostics_.clear();

            Lexer lexer(code, source_name, diagnostics_);
            auto tokens = lexer.tokenize();

            if (diagnostics_.has_errors()) {
                diagnostics_.render(std::cerr);
                continue;
            }

            Parser parser(tokens, diagnostics_);
            auto program = parser.parse_program();

            if (diagnostics_.has_errors() || !program) {
                diagnostics_.render(std::cerr);
                continue;
            }

            // Check if single expression statement to auto-print result
            if (program->statements().size() == 1) {
                if (auto* expr_stmt = dynamic_cast<ExprStmt*>(program->statements()[0].get())) {
                    try {
                        Value val = interpreter_.evaluate(expr_stmt->expr());
                        if (!val.is_nil()) {
                            std::cout << "\033[1;34m=>\033[0m " << val.inspect() << "\n";
                        }
                    } catch (const RuntimeError&) {
                        diagnostics_.render(std::cerr);
                    }
                    continue;
                }
            }

            // Execute general statements
            if (!interpreter_.execute(*program)) {
                diagnostics_.render(std::cerr);
            }
        }
    }

    std::cout << "Goodbye!\n";
}

} // namespace nextviper
