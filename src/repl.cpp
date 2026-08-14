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

void REPL::run() {
    print_banner();

    std::string line;
    while (true) {
        std::cout << "\033[1;32mnv>\033[0m ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        if (line.empty()) continue;

        if (line[0] == ':') {
            if (!handle_command(line)) {
                break;
            }
            continue;
        }

        std::string source_name = "<repl-" + std::to_string(line_counter_++) + ">";
        source_manager_.add_file(source_name, line);
        diagnostics_.clear();

        Lexer lexer(line, source_name, diagnostics_);
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

    std::cout << "Goodbye!\n";
}

} // namespace nextviper
